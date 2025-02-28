/* Copyright (c) 2014-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "src/mc/sosp/Snapshot.hpp"
#include "src/mc/mc_config.hpp"

#include <cstddef> /* std::size_t */

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(mc_snapshot, mc, "Taking and restoring snapshots");
namespace simgrid::mc {
/************************************* Take Snapshot ************************************/
/****************************************************************************************/

void Snapshot::snapshot_regions(RemoteProcess* process)
{
  snapshot_regions_.clear();

  for (auto const& object_info : process->object_infos)
    add_region(RegionType::Data, object_info.get(), object_info->start_rw, object_info->end_rw - object_info->start_rw);

  const s_xbt_mheap_t* heap = process->get_heap();
  void* start_heap = heap->base;
  void* end_heap   = heap->breakval;

  add_region(RegionType::Heap, nullptr, start_heap, (char*)end_heap - (char*)start_heap);
  heap_bytes_used_ = mmalloc_get_bytes_used_remote(heap->heaplimit, process->get_malloc_info());
}

/** @brief Checks whether the variable is in scope for a given IP.
 *
 *  A variable may be defined only from a given value of IP.
 *
 *  @param var   Variable description
 *  @param scope Scope description
 *  @param ip    Instruction pointer
 *  @return      true if the variable is valid
 * */
static bool valid_variable(const simgrid::mc::Variable* var, simgrid::mc::Frame* scope, const void* ip)
{
  // The variable is not yet valid:
  if (scope->range.begin() + var->start_scope > (std::uint64_t)ip)
    return false;
  else
    return true;
}

static void fill_local_variables_values(mc_stack_frame_t stack_frame, Frame* scope,
                                        std::vector<s_local_variable_t>& result)
{
  if (not scope || not scope->range.contain(stack_frame->ip))
    return;

  for (const Variable& current_variable : scope->variables) {
    if (not valid_variable(&current_variable, scope, (void*)stack_frame->ip))
      continue;

    if (not current_variable.type) {
      XBT_VERB("Ignore local variable without type: '%s' [%s]", current_variable.name.c_str(),
               stack_frame->frame->name.c_str());
      continue;
    }

    s_local_variable_t new_var;
    new_var.subprogram = stack_frame->frame;
    new_var.ip         = stack_frame->ip;
    new_var.name       = current_variable.name;
    new_var.type       = current_variable.type;
    new_var.address    = nullptr;

    if (current_variable.address != nullptr)
      new_var.address = current_variable.address;
    else if (not current_variable.location_list.empty()) {
      dwarf::Location location = simgrid::dwarf::resolve(current_variable.location_list, current_variable.object_info,
                                                         &(stack_frame->unw_cursor), (void*)stack_frame->frame_base,
                                                         &mc_model_checker->get_remote_process());

      xbt_assert(location.in_memory(), "Cannot handle non-address variable");
      new_var.address = location.address();
    } else
      xbt_die("No address");

    result.push_back(std::move(new_var));
  }

  // Recursive processing of nested scopes:
  for (Frame& nested_scope : scope->scopes)
    fill_local_variables_values(stack_frame, &nested_scope, result);
}

static std::vector<s_local_variable_t> get_local_variables_values(std::vector<s_mc_stack_frame_t>& stack_frames)
{
  std::vector<s_local_variable_t> variables;
  for (s_mc_stack_frame_t& stack_frame : stack_frames)
    fill_local_variables_values(&stack_frame, stack_frame.frame, variables);
  return variables;
}

static std::vector<s_mc_stack_frame_t> unwind_stack_frames(UnwindContext* stack_context)
{
  const RemoteProcess* process = &mc_model_checker->get_remote_process();
  std::vector<s_mc_stack_frame_t> result;

  unw_cursor_t c = stack_context->cursor();

  // TODO, check condition check (unw_init_local==0 means end of frame)

  while (true) {
    s_mc_stack_frame_t stack_frame;

    stack_frame.unw_cursor = c;

    unw_word_t ip;
    unw_word_t sp;

    unw_get_reg(&c, UNW_REG_IP, &ip);
    unw_get_reg(&c, UNW_REG_SP, &sp);

    stack_frame.ip = ip;
    stack_frame.sp = sp;

    // TODO, use real addresses in frame_t instead of fixing it here

    Frame* frame              = process->find_function(remote(ip));
    stack_frame.frame         = frame;

    if (frame) {
      stack_frame.frame_name = frame->name;
      stack_frame.frame_base = (unw_word_t)frame->frame_base(c);
    } else {
      stack_frame.frame_base = 0;
      stack_frame.frame_name = "";
    }

    result.push_back(std::move(stack_frame));

    /* Stop before context switch with maestro */
    if (frame != nullptr && frame->name == "smx_ctx_wrapper")
      break;

    int ret = unw_step(&c);
    xbt_assert(ret >= 0, "Error while unwinding stack");
    xbt_assert(ret != 0, "Unexpected end of stack.");
  }

  xbt_assert(not result.empty(), "unw_init_local failed");

  return result;
}

void Snapshot::snapshot_stacks(RemoteProcess* process)
{
  for (auto const& stack : process->stack_areas()) {
    s_mc_snapshot_stack_t st;

    // Read the context from remote process:
    unw_context_t context;
    process->read_bytes(&context, sizeof(context), remote(stack.context));

    st.context.initialize(process, &context);

    st.stack_frames    = unwind_stack_frames(&st.context);
    st.local_variables = get_local_variables_values(st.stack_frames);

    unw_word_t sp = st.stack_frames[0].sp;

    stacks_.push_back(std::move(st));

    size_t stack_size = (char*)stack.address + stack.size - (char*)sp;
    stack_sizes_.push_back(stack_size);
  }
}

void Snapshot::handle_ignore()
{
  xbt_assert(get_remote_process());

  // Copy the memory:
  for (auto const& region : get_remote_process()->ignored_regions()) {
    s_mc_snapshot_ignored_data_t ignored_data;
    ignored_data.start = (void*)region.addr;
    ignored_data.data.resize(region.size);
    // TODO, we should do this once per privatization segment:
    get_remote_process()->read_bytes(ignored_data.data.data(), region.size, remote(region.addr));
    ignored_data_.push_back(std::move(ignored_data));
  }

  // Zero the memory:
  for (auto const& region : get_remote_process()->ignored_regions())
    get_remote_process()->clear_bytes(remote(region.addr), region.size);
}

void Snapshot::ignore_restore() const
{
  for (auto const& ignored_data : ignored_data_)
    get_remote_process()->write_bytes(ignored_data.data.data(), ignored_data.data.size(), remote(ignored_data.start));
}

Snapshot::Snapshot(long num_state, RemoteProcess* process) : AddressSpace(process), num_state_(num_state)
{
  XBT_DEBUG("Taking snapshot %ld", num_state);

  handle_ignore();

  /* Save the std heap and the writable mapped pages of libsimgrid and binary */
  snapshot_regions(process);

  to_ignore_ = process->ignored_heap();

  if (_sg_mc_max_visited_states > 0 || not _sg_mc_property_file.get().empty()) {
    snapshot_stacks(process);
    hash_ = this->do_hash();
  }

  ignore_restore();
}

void Snapshot::add_region(RegionType type, ObjectInformation* object_info, void* start_addr, std::size_t size)
{
  if (type == RegionType::Data)
    xbt_assert(object_info, "Missing object info for object.");
  else if (type == RegionType::Heap)
    xbt_assert(not object_info, "Unexpected object info for heap region.");

  auto* region = new Region(type, start_addr, size);
  region->object_info(object_info);
  snapshot_regions_.push_back(std::unique_ptr<Region>(region));
}

void* Snapshot::read_bytes(void* buffer, std::size_t size, RemotePtr<void> address, ReadOptions options) const
{
  const Region* region = this->get_region((void*)address.address());
  if (region) {
    void* res = region->read(buffer, (void*)address.address(), size);
    if (buffer == res || options & ReadOptions::lazy())
      return res;
    else {
      memcpy(buffer, res, size);
      return buffer;
    }
  } else
    return this->get_remote_process()->read_bytes(buffer, size, address, options);
}
/** @brief Find the snapshotted region from a pointer
 *
 *  @param addr     Pointer
 * */
Region* Snapshot::get_region(const void* addr) const
{
  size_t n = snapshot_regions_.size();
  for (size_t i = 0; i != n; ++i) {
    Region* region = snapshot_regions_[i].get();
    if (not(region && region->contain(simgrid::mc::remote(addr))))
      continue;

    return region;
  }

  return nullptr;
}

/** @brief Find the snapshotted region from a pointer, with a hinted_region */
Region* Snapshot::get_region(const void* addr, Region* hinted_region) const
{
  if (hinted_region->contain(simgrid::mc::remote(addr)))
    return hinted_region;
  else
    return get_region(addr);
}

void Snapshot::restore(RemoteProcess* process) const
{
  XBT_DEBUG("Restore snapshot %ld", num_state_);

  // Restore regions
  for (std::unique_ptr<Region> const& region : snapshot_regions_) {
    if (region) // privatized variables are not snapshotted
      region.get()->restore();
  }

  ignore_restore();
  process->clear_cache();
}

/* ----------- Hashing logic -------------- */
class djb_hash {
  hash_type state_ = 5381LL;

public:
  template <class T> void update(T& x) { state_ = (state_ << 5) + state_ + x; }
  hash_type value() const { return state_; }
};
hash_type Snapshot::do_hash() const
{
  XBT_DEBUG("START hash %ld", num_state_);
  djb_hash hash;
  // TODO:
  // * nb_processes
  // * heap_bytes_used
  // * root variables
  // * basic stack frame information
  // * stack frame local variables
  XBT_DEBUG("END hash %ld", num_state_);
  return hash.value();
}

} // namespace simgrid::mc
