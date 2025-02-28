/* s4u::Engine Simulation Engine and global functions. */

/* Copyright (c) 2006-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/kernel/routing/NetPoint.hpp>
#include <simgrid/modelchecker.h>
#include <simgrid/s4u/Engine.hpp>

#include "mc/mc.h"
#include "src/instr/instr_private.hpp"
#include "src/kernel/EngineImpl.hpp"
#include "src/kernel/resource/NetworkModel.hpp"
#include "src/kernel/resource/SplitDuplexLinkImpl.hpp"
#include "src/kernel/resource/StandardLinkImpl.hpp"
#include "src/mc/mc_replay.hpp"
#include "src/surf/HostImpl.hpp"
#include "xbt/config.hpp"

#include <algorithm>
#include <string>

XBT_LOG_NEW_CATEGORY(s4u, "Log channels of the S4U (Simgrid for you) interface");
XBT_LOG_NEW_DEFAULT_SUBCATEGORY(s4u_engine, s4u, "Logging specific to S4U (engine)");

static simgrid::kernel::actor::ActorCode maestro_code;

namespace simgrid::s4u {
xbt::signal<void()> Engine::on_platform_creation;
xbt::signal<void()> Engine::on_platform_created;
xbt::signal<void()> Engine::on_simulation_start;
xbt::signal<void()> Engine::on_simulation_end;
xbt::signal<void(double)> Engine::on_time_advance;
xbt::signal<void(void)> Engine::on_deadlock;

Engine* Engine::instance_ = nullptr; /* This singleton is awful, but I don't see no other solution right now. */

void Engine::initialize(int* argc, char** argv)
{
  xbt_assert(Engine::instance_ == nullptr, "It is currently forbidden to create more than one instance of s4u::Engine");
  Engine::instance_ = this;
  instr::init();
  pimpl->initialize(argc, argv);
  // Either create a new context with maestro or create
  // a context object with the current context maestro):
  kernel::actor::create_maestro(maestro_code);
}

Engine::Engine(std::string name) : pimpl(new kernel::EngineImpl())
{
  int argc   = 1;
  char* argv = &name[0];
  initialize(&argc, &argv);
}

Engine::Engine(int* argc, char** argv) : pimpl(new kernel::EngineImpl())
{
  initialize(argc, argv);
}

Engine::~Engine()
{
  kernel::EngineImpl::shutdown();
  Engine::instance_ = nullptr;
}

/** @brief Retrieve the engine singleton */
Engine* Engine::get_instance()
{
  int argc   = 0;
  char* argv = nullptr;
  return get_instance(&argc, &argv);
}
Engine* Engine::get_instance(int* argc, char** argv)
{
  if (Engine::instance_ == nullptr) {
    auto e = new Engine(argc, argv);
    xbt_assert(Engine::instance_ == e);
  }
  return Engine::instance_;
}

void Engine::shutdown() // XBT_ATTRIB_DEPRECATED_v335
{
  delete Engine::instance_;
}

double Engine::get_clock()
{
  if (MC_is_active() || MC_record_replay_is_active()) {
    return MC_process_clock_get(kernel::actor::ActorImpl::self());
  } else {
    return kernel::EngineImpl::get_clock();
  }
}

void Engine::add_model(std::shared_ptr<kernel::resource::Model> model,
                       const std::vector<kernel::resource::Model*>& dependencies)
{
  kernel::actor::simcall_answered([this, &model, &dependencies] { pimpl->add_model(std::move(model), dependencies); });
}

const std::vector<simgrid::kernel::resource::Model*>& Engine::get_all_models() const
{
  return pimpl->get_all_models();
}

/**
 * Creates a new platform, including hosts, links, and the routing table.
 *
 * @beginrst
 * See also: :ref:`platform`.
 * @endrst
 */
void Engine::load_platform(const std::string& platf) const
{
  pimpl->load_platform(platf);
}

/**
 * @brief Seals the platform, finishing the creation of its resources.
 *
 * This method is optional. The seal() is done automatically when you call Engine::run.
 */
void Engine::seal_platform() const
{
  pimpl->seal_platform();
}

/** Registers the main function of an actor that will be launched from the deployment file */
void Engine::register_function(const std::string& name, const std::function<void(int, char**)>& code)
{
  kernel::actor::ActorCodeFactory code_factory = [code](std::vector<std::string> args) {
    return xbt::wrap_main(code, std::move(args));
  };
  register_function(name, code_factory);
}

/** Registers the main function of an actor that will be launched from the deployment file */
void Engine::register_function(const std::string& name, const std::function<void(std::vector<std::string>)>& code)
{
  kernel::actor::ActorCodeFactory code_factory = [code{code}](std::vector<std::string> args) mutable {
    return std::bind(std::move(code), std::move(args));
  };
  register_function(name, code_factory);
}
/** Registers a function as the default main function of actors
 *
 * It will be used as fallback when the function requested from the deployment file was not registered.
 * It is used for trace-based simulations (see examples/cpp/replay-comms and similar).
 */
void Engine::register_default(const std::function<void(int, char**)>& code)
{
  register_default([code](std::vector<std::string> args) { return xbt::wrap_main(code, std::move(args)); });
}
void Engine::register_default(const kernel::actor::ActorCodeFactory& code)
{
  simgrid::kernel::actor::simcall_answered([this, &code]() { pimpl->register_default(code); });
}

void Engine::register_function(const std::string& name, const kernel::actor::ActorCodeFactory& code)
{
  simgrid::kernel::actor::simcall_answered([this, name, &code]() { pimpl->register_function(name, code); });
}

/** Load a deployment file and launch the actors that it contains
 *
 * @beginrst
 * See also: :ref:`deploy`.
 * @endrst
 */
void Engine::load_deployment(const std::string& deploy) const
{
  pimpl->load_deployment(deploy);
}

/** Returns the amount of hosts in the platform */
size_t Engine::get_host_count() const
{
  return get_all_hosts().size();
}

std::vector<Host*> Engine::get_all_hosts() const
{
  return get_filtered_hosts([](const Host*) { return true; });
}

std::vector<Host*> Engine::get_filtered_hosts(const std::function<bool(Host*)>& filter) const
{
  std::vector<Host*> hosts;
  if (pimpl->netzone_root_) {
    hosts = pimpl->netzone_root_->get_filtered_hosts(filter);
  }
  /* Sort hosts in lexicographical order: keep same behavior when the hosts were saved on Engine
   * Some tests do a get_all_hosts() and selects hosts in this order */
  std::sort(hosts.begin(), hosts.end(), [](const auto* h1, const auto* h2) { return h1->get_name() < h2->get_name(); });

  return hosts;
}

/** @brief Find a host from its name.
 *
 *  @throw std::invalid_argument if the searched host does not exist.
 */
Host* Engine::host_by_name(const std::string& name) const
{
  auto* host = host_by_name_or_null(name);
  if (not host)
    throw std::invalid_argument("Host not found: '" + name + "'");
  return host;
}

/** @brief Find a host from its name (or nullptr if that host does not exist) */
Host* Engine::host_by_name_or_null(const std::string& name) const
{
  Host* host = nullptr;
  if (pimpl->netzone_root_) {
    auto* host_impl = pimpl->netzone_root_->get_host_by_name_or_null(name);
    if (host_impl)
      host = host_impl->get_iface();
  }
  return host;
}

/** @brief Find a link from its name.
 *
 *  @throw std::invalid_argument if the searched link does not exist.
 */
Link* Engine::link_by_name(const std::string& name) const
{
  auto* link = link_by_name_or_null(name);
  if (not link)
    throw std::invalid_argument("Link not found: " + name);
  return link;
}

SplitDuplexLink* Engine::split_duplex_link_by_name(const std::string& name) const
{
  auto* link_impl = pimpl->netzone_root_ ? pimpl->netzone_root_->get_split_duplex_link_by_name_or_null(name) : nullptr;
  if (not link_impl)
    throw std::invalid_argument("Link not found: " + name);
  return link_impl->get_iface();
}

/** @brief Find a link from its name (or nullptr if that link does not exist) */
Link* Engine::link_by_name_or_null(const std::string& name) const
{
  Link* link = nullptr;
  if (pimpl->netzone_root_) {
    /* keep behavior where internal __loopback__ link from network model is given to user */
    if (name == "__loopback__")
      return pimpl->netzone_root_->get_network_model()->loopback_->get_iface();
    auto* link_impl = pimpl->netzone_root_->get_link_by_name_or_null(name);
    if (link_impl)
      link = link_impl->get_iface();
  }
  return link;
}

/** @brief Find a mailbox from its name or create one if it does not exist) */
Mailbox* Engine::mailbox_by_name_or_create(const std::string& name) const
{
  /* two actors may have pushed the same mbox_create simcall at the same time */
  kernel::activity::MailboxImpl* mbox = kernel::actor::simcall_answered([&name, this] {
    auto [m, inserted] = pimpl->mailboxes_.try_emplace(name, nullptr);
    if (inserted) {
      m->second = new kernel::activity::MailboxImpl(name);
      XBT_DEBUG("Creating a mailbox at %p with name %s", m->second, name.c_str());
    }
    return m->second;
  });
  return mbox->get_iface();
}

/** @brief Returns the amount of links in the platform */
size_t Engine::get_link_count() const
{
  int count = 0;
  if (pimpl->netzone_root_) {
    count += pimpl->netzone_root_->get_link_count();
    /* keep behavior where internal __loopback__ link from network model is given to user */
    count += pimpl->netzone_root_->get_network_model()->loopback_ ? 1 : 0;
  }
  return count;
}

/** @brief Returns the list of all links found in the platform */
std::vector<Link*> Engine::get_all_links() const
{
  return get_filtered_links([](const Link*) { return true; });
}

std::vector<Link*> Engine::get_filtered_links(const std::function<bool(Link*)>& filter) const
{
  std::vector<Link*> res;
  if (pimpl->netzone_root_) {
    res = pimpl->netzone_root_->get_filtered_links(filter);
    /* keep behavior where internal __loopback__ link from network model is given to user */
    if (pimpl->netzone_root_->get_network_model()->loopback_ &&
        filter(pimpl->netzone_root_->get_network_model()->loopback_->get_iface()))
      res.push_back(pimpl->netzone_root_->get_network_model()->loopback_->get_iface());
  }
  return res;
}

size_t Engine::get_actor_count() const
{
  return pimpl->get_actor_count();
}

std::vector<ActorPtr> Engine::get_all_actors() const
{
  std::vector<ActorPtr> actor_list;
  for (auto const& [_, actor] : pimpl->get_actor_list()) {
    actor_list.push_back(actor->get_iface());
  }
  return actor_list;
}

std::vector<ActorPtr> Engine::get_filtered_actors(const std::function<bool(ActorPtr)>& filter) const
{
  std::vector<ActorPtr> actor_list;
  for (auto const& [_, actor] : pimpl->get_actor_list()) {
    if (filter(actor->get_iface()))
      actor_list.push_back(actor->get_iface());
  }
  return actor_list;
}

void Engine::run() const
{
  run_until(-1);
}
void Engine::run_until(double max_date) const
{
  if (static bool callback_called = false; not callback_called) {
    on_simulation_start();
    callback_called = true;
  }
  /* Clean IO before the run */
  fflush(stdout);
  fflush(stderr);

  pimpl->run(max_date);
}

void Engine::track_vetoed_activities(std::set<Activity*>* vetoed_activities) const
{
  Activity::set_vetoed_activities(vetoed_activities);
}

/** @brief Retrieve the root netzone, containing all others */
s4u::NetZone* Engine::get_netzone_root() const
{
  if (pimpl->netzone_root_)
    return pimpl->netzone_root_->get_iface();
  return nullptr;
}
/** @brief Set the root netzone, containing all others. Once set, it cannot be changed. */
void Engine::set_netzone_root(const s4u::NetZone* netzone)
{
  xbt_assert(pimpl->netzone_root_ == nullptr, "The root NetZone cannot be changed once set");
  pimpl->netzone_root_ = netzone->get_impl();
}

static NetZone* netzone_by_name_recursive(NetZone* current, const std::string& name)
{
  if (current->get_name() == name)
    return current;

  for (auto const& elem : current->get_children()) {
    NetZone* tmp = netzone_by_name_recursive(elem, name);
    if (tmp != nullptr) {
      return tmp;
    }
  }
  return nullptr;
}

/** @brief Retrieve the NetZone of the given name (or nullptr if not found) */
NetZone* Engine::netzone_by_name_or_null(const std::string& name) const
{
  return netzone_by_name_recursive(get_netzone_root(), name);
}

/** @brief Retrieve the netpoint of the given name (or nullptr if not found) */
kernel::routing::NetPoint* Engine::netpoint_by_name_or_null(const std::string& name) const
{
  auto netp = pimpl->netpoints_.find(name);
  return netp == pimpl->netpoints_.end() ? nullptr : netp->second;
}

kernel::routing::NetPoint* Engine::netpoint_by_name(const std::string& name) const
{
  auto netp = netpoint_by_name_or_null(name);
  if (netp == nullptr) {
    throw std::invalid_argument("Netpoint not found: " + name);
  }
  return netp;
}

std::vector<kernel::routing::NetPoint*> Engine::get_all_netpoints() const
{
  std::vector<kernel::routing::NetPoint*> res;
  for (auto const& [_, netpoint] : pimpl->netpoints_)
    res.push_back(netpoint);
  return res;
}

/** @brief Register a new netpoint to the system */
void Engine::netpoint_register(kernel::routing::NetPoint* point)
{
  simgrid::kernel::actor::simcall_answered([this, point] { pimpl->netpoints_[point->get_name()] = point; });
}

/** @brief Unregister a given netpoint */
void Engine::netpoint_unregister(kernel::routing::NetPoint* point)
{
  kernel::actor::simcall_answered([this, point] {
    pimpl->netpoints_.erase(point->get_name());
    delete point;
  });
}

bool Engine::is_initialized()
{
  return Engine::instance_ != nullptr;
}
void Engine::set_config(const std::string& str)
{
  config::set_parse(str);
}
void Engine::set_config(const std::string& name, int value)
{
  config::set_value(name.c_str(), value);
}
void Engine::set_config(const std::string& name, double value)
{
  config::set_value(name.c_str(), value);
}
void Engine::set_config(const std::string& name, bool value)
{
  config::set_value(name.c_str(), value);
}
void Engine::set_config(const std::string& name, const std::string& value)
{
  config::set_value(name.c_str(), value);
}

Engine* Engine::set_default_comm_data_copy_callback(
    const std::function<void(kernel::activity::CommImpl*, void*, size_t)>& callback)
{
  kernel::activity::CommImpl::set_copy_data_callback(callback);
  return this;
}

} // namespace simgrid::s4u

/* **************************** Public C interface *************************** */
void simgrid_init(int* argc, char** argv)
{
  static simgrid::s4u::Engine e(argc, argv);
}
void simgrid_load_platform(const char* file)
{
  simgrid::s4u::Engine::get_instance()->load_platform(file);
}

void simgrid_load_deployment(const char* file)
{
  simgrid::s4u::Engine::get_instance()->load_deployment(file);
}
void simgrid_run()
{
  simgrid::s4u::Engine::get_instance()->run();
}
void simgrid_run_until(double max_date)
{
  simgrid::s4u::Engine::get_instance()->run_until(max_date);
}
void simgrid_register_function(const char* name, void (*code)(int, char**))
{
  simgrid::s4u::Engine::get_instance()->register_function(name, code);
}
void simgrid_register_default(void (*code)(int, char**))
{
  simgrid::s4u::Engine::get_instance()->register_default(code);
}
double simgrid_get_clock()
{
  return simgrid::s4u::Engine::get_clock();
}

void simgrid_set_maestro(void (*code)(void*), void* data)
{
  maestro_code = std::bind(code, data);
}
