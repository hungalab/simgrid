/* Copyright (c) 2006-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "simgrid/simix.hpp"
#include <simgrid/Exception.hpp>
#include <simgrid/exec.h>
#include <simgrid/s4u/Exec.hpp>
#include <simgrid/s4u/Host.hpp>

#include "src/kernel/activity/ExecImpl.hpp"
#include "src/kernel/actor/ActorImpl.hpp"
#include "src/kernel/actor/SimcallObserver.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(s4u_exec, s4u_activity, "S4U asynchronous executions");

namespace simgrid::s4u {
xbt::signal<void(Exec const&)> Exec::on_start;

Exec::Exec(kernel::activity::ExecImplPtr pimpl)
{
  pimpl_ = pimpl;
}

void Exec::reset() const
{
  boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->reset();
}

ExecPtr Exec::init()
{
  auto pimpl = kernel::activity::ExecImplPtr(new kernel::activity::ExecImpl());
  unsigned int cb_id = Host::on_state_change.connect([pimpl](s4u::Host const& h) {
    if (not h.is_on() && pimpl->get_state() == kernel::activity::State::RUNNING &&
        std::find(pimpl->get_hosts().begin(), pimpl->get_hosts().end(), &h) != pimpl->get_hosts().end()) {
      pimpl->set_state(kernel::activity::State::FAILED);
      pimpl->post();
    }
  });
  pimpl->set_cb_id(cb_id);
  return ExecPtr(static_cast<Exec*>(pimpl->get_iface()));
}

Exec* Exec::start()
{
  kernel::actor::simcall_answered([this] {
    (*boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_))
        .set_name(get_name())
        .set_tracing_category(get_tracing_category())
        .start();
  });

  if (suspended_)
    pimpl_->suspend();

  state_      = State::STARTED;
  on_start(*this);
  return this;
}

ssize_t Exec::wait_any_for(const std::vector<ExecPtr>& execs, double timeout)
{
  std::vector<ActivityPtr> activities;
  for (const auto& exec : execs)
    activities.push_back(boost::dynamic_pointer_cast<Activity>(exec));
  return Activity::wait_any_for(activities, timeout);
}

/** @brief change the execution bound
 * This means changing the maximal amount of flops per second that it may consume, regardless of what the host may
 * deliver. Currently, this cannot be changed once the exec started.
 */
ExecPtr Exec::set_bound(double bound)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
             "Cannot change the bound of an exec after its start");
  kernel::actor::simcall_object_access(pimpl_.get(), [this, bound] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_bound(bound);
  });
  return this;
}

/** @brief  Change the execution priority, don't you think?
 *
 * An execution with twice the priority will get twice the amount of flops when the resource is shared.
 * The default priority is 1.
 *
 * Currently, this cannot be changed once the exec started. */
ExecPtr Exec::set_priority(double priority)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
             "Cannot change the priority of an exec after its start");
  kernel::actor::simcall_object_access(pimpl_.get(), [this, priority] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_sharing_penalty(1. / priority);
  });
  return this;
}

ExecPtr Exec::update_priority(double priority)
{
  kernel::actor::simcall_answered([this, priority] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->update_sharing_penalty(1. / priority);
  });
  return this;
}

ExecPtr Exec::set_flops_amount(double flops_amount)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
      "Cannot change the flop_amount of an exec after its start");
  kernel::actor::simcall_object_access(pimpl_.get(), [this, flops_amount] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_flops_amount(flops_amount);
  });
  set_remaining(flops_amount);
  return this;
}

ExecPtr Exec::set_flops_amounts(const std::vector<double>& flops_amounts)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
      "Cannot change the flops_amounts of an exec after its start");
  kernel::actor::simcall_object_access(pimpl_.get(), [this, flops_amounts] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_flops_amounts(flops_amounts);
  });
  parallel_      = true;
  return this;
}

ExecPtr Exec::set_bytes_amounts(const std::vector<double>& bytes_amounts)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
      "Cannot change the bytes_amounts of an exec after its start");
  kernel::actor::simcall_object_access(pimpl_.get(), [this, bytes_amounts] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_bytes_amounts(bytes_amounts);
  });
  parallel_      = true;
  return this;
}

ExecPtr Exec::set_thread_count(int thread_count)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
             "Cannot change the bytes_amounts of an exec after its start");
  kernel::actor::simcall_object_access(pimpl_.get(), [this, thread_count] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_thread_count(thread_count);
  });
  return this;
}

/** @brief Retrieve the host on which this activity takes place.
 *  If it runs on more than one host, only the first host is returned.
 */
Host* Exec::get_host() const
{
  return static_cast<kernel::activity::ExecImpl*>(pimpl_.get())->get_host();
}
unsigned int Exec::get_host_number() const
{
  return static_cast<kernel::activity::ExecImpl*>(pimpl_.get())->get_host_number();
}

int Exec::get_thread_count() const
{
  return static_cast<kernel::activity::ExecImpl*>(pimpl_.get())->get_thread_count();
}

/** @brief Change the host on which this activity takes place.
 *
 * The activity cannot be terminated already (but it may be started). */
ExecPtr Exec::set_host(Host* host)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING || state_ == State::STARTED,
             "Cannot change the host of an exec once it's done (state: %s)", to_c_str(state_));

  if (state_ == State::STARTED)
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->migrate(host);

  kernel::actor::simcall_object_access(
      pimpl_.get(), [this, host] { boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_host(host); });

  if (state_ == State::STARTING)
  // Setting the host may allow to start the activity, let's try
    vetoable_start();

  return this;
}

ExecPtr Exec::set_hosts(const std::vector<Host*>& hosts)
{
  xbt_assert(state_ == State::INITED || state_ == State::STARTING,
             "Cannot change the hosts of an exec once it's done (state: %s)", to_c_str(state_));

  kernel::actor::simcall_object_access(pimpl_.get(), [this, hosts] {
    boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->set_hosts(hosts);
  });
  parallel_ = true;

  // Setting the host may allow to start the activity, let's try
  if (state_ == State::STARTING)
     vetoable_start();

  return this;
}

ExecPtr Exec::unset_host()
{
  if (not is_assigned())
    throw std::invalid_argument(
        xbt::string_printf("Exec %s: the activity is not assigned to any host(s)", get_cname()));
  else {
    reset();

    if (state_ == State::STARTED)
      cancel();
    vetoable_start();

    return this;
  }
}

double Exec::get_cost() const
{
  return (pimpl_->surf_action_ == nullptr) ? -1 : pimpl_->surf_action_->get_cost();
}

double Exec::get_remaining() const
{
  if (is_parallel()) {
    XBT_WARN("Calling get_remaining() on a parallel execution is not allowed. Call get_remaining_ratio() instead.");
    return get_remaining_ratio();
  } else
    return kernel::actor::simcall_answered(
        [this]() { return boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->get_remaining(); });
}

/** @brief Returns the ratio of elements that are still to do
 *
 * The returned value is between 0 (completely done) and 1 (nothing done yet).
 */
double Exec::get_remaining_ratio() const
{
  if (is_parallel())
    return kernel::actor::simcall_answered(
        [this]() { return boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->get_par_remaining_ratio(); });
  else
    return kernel::actor::simcall_answered(
        [this]() { return boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->get_seq_remaining_ratio(); });
}

bool Exec::is_assigned() const
{
  return not boost::static_pointer_cast<kernel::activity::ExecImpl>(pimpl_)->get_hosts().empty();
}
} // namespace simgrid::s4u

/* **************************** Public C interface *************************** */
void sg_exec_set_bound(sg_exec_t exec, double bound)
{
  exec->set_bound(bound);
}

const char* sg_exec_get_name(const_sg_exec_t exec)
{
  return exec->get_cname();
}

void sg_exec_set_name(sg_exec_t exec, const char* name)
{
  exec->set_name(name);
}

void sg_exec_set_host(sg_exec_t exec, sg_host_t new_host)
{
  exec->set_host(new_host);
}

double sg_exec_get_remaining(const_sg_exec_t exec)
{
  return exec->get_remaining();
}

double sg_exec_get_remaining_ratio(const_sg_exec_t exec)
{
  return exec->get_remaining_ratio();
}

void sg_exec_start(sg_exec_t exec)
{
  exec->vetoable_start();
}

void sg_exec_cancel(sg_exec_t exec)
{
  exec->cancel();
  exec->unref();
}

int sg_exec_test(sg_exec_t exec)
{
  bool finished = exec->test();
  if (finished)
    exec->unref();
  return finished;
}

sg_error_t sg_exec_wait(sg_exec_t exec)
{
  return sg_exec_wait_for(exec, -1.0);
}

sg_error_t sg_exec_wait_for(sg_exec_t exec, double timeout)
{
  sg_error_t status = SG_OK;

  simgrid::s4u::ExecPtr s4u_exec(exec, false);
  try {
    s4u_exec->wait_for(timeout);
  } catch (const simgrid::TimeoutException&) {
    s4u_exec->add_ref(); // the wait_for timeouted, keep the exec alive
    status = SG_ERROR_TIMEOUT;
  } catch (const simgrid::CancelException&) {
    status = SG_ERROR_CANCELED;
  } catch (const simgrid::HostFailureException&) {
    status = SG_ERROR_HOST;
  }
  return status;
}

ssize_t sg_exec_wait_any(sg_exec_t* execs, size_t count)
{
  return sg_exec_wait_any_for(execs, count, -1.0);
}

ssize_t sg_exec_wait_any_for(sg_exec_t* execs, size_t count, double timeout)
{
  std::vector<simgrid::s4u::ExecPtr> s4u_execs;
  for (size_t i = 0; i < count; i++)
    s4u_execs.emplace_back(execs[i], false);

  ssize_t pos = simgrid::s4u::Exec::wait_any_for(s4u_execs, timeout);
  for (size_t i = 0; i < count; i++) {
    if (pos != -1 && static_cast<size_t>(pos) != i)
      s4u_execs[i]->add_ref();
  }
  return pos;
}
