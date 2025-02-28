/* Copyright (c) 2008-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "simgrid/simix.hpp"
#include "src/kernel/actor/ActorImpl.hpp"
#include "src/mc/mc_config.hpp"
#include "src/mc/mc_private.hpp"
#include "src/mc/mc_record.hpp"
#include "src/mc/mc_replay.hpp"
#include "src/mc/remote/AppSide.hpp"
#include "xbt/asserts.h"
#include "xbt/random.hpp"

/* Implementation of the user API from the App to the Checker (see modelchecker.h)  */

int MC_random(int min, int max)
{
#if SIMGRID_HAVE_MC
  xbt_assert(mc_model_checker == nullptr);
#endif
  if (not MC_is_active() && not MC_record_replay_is_active()) { // no need to do a simcall in this case
    static simgrid::xbt::random::XbtRandom prng;
    return prng.uniform_int(min, max);
  }
  simgrid::kernel::actor::RandomSimcall observer{simgrid::kernel::actor::ActorImpl::self(), min, max};
  return simgrid::kernel::actor::simcall_answered([&observer] { return observer.get_value(); }, &observer);
}

void MC_assert(int prop)
{
  // Cannot used xbt_assert here, or it would be an infinite recursion.
#if SIMGRID_HAVE_MC
  if (mc_model_checker != nullptr)
    xbt_die("This should be called from the client side");
  if (not prop) {
    if (MC_is_active())
      simgrid::mc::AppSide::get()->report_assertion_failure();
    if (MC_record_replay_is_active())
      xbt_die("MC assertion failed");
  }
#else
  if (not prop)
    xbt_die("Safety property violation detected without the model-checker");
#endif
}

int MC_is_active()
{
  return simgrid::mc::cfg_do_model_check;
}

void MC_automaton_new_propositional_symbol_pointer(const char *name, int* value)
{
#if SIMGRID_HAVE_MC
  xbt_assert(mc_model_checker == nullptr);
  simgrid::mc::AppSide::get()->declare_symbol(name, value);
#endif
}

void MC_ignore(void* addr, size_t size)
{
#if SIMGRID_HAVE_MC
  xbt_assert(mc_model_checker == nullptr);
  simgrid::mc::AppSide::get()->ignore_memory(addr, size);
#endif
}

void MC_ignore_heap(void *address, size_t size)
{
#if SIMGRID_HAVE_MC
  xbt_assert(mc_model_checker == nullptr);
  simgrid::mc::AppSide::get()->ignore_heap(address, size);
#endif
}

void MC_unignore_heap(void* address, size_t size)
{
#if SIMGRID_HAVE_MC
  xbt_assert(mc_model_checker == nullptr);
  simgrid::mc::AppSide::get()->unignore_heap(address, size);
#endif
}
