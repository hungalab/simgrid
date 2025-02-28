/* Copyright (c) 2019-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/Exception.hpp>
#include <simgrid/s4u/Host.hpp>

#include "src/kernel/activity/SemaphoreImpl.hpp"
#include "src/kernel/activity/Synchro.hpp"
#include "src/kernel/actor/SynchroObserver.hpp"
#include "src/kernel/resource/CpuImpl.hpp"

#include <cmath> // std::isfinite

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(ker_semaphore, ker_synchro, "Semaphore kernel-space implementation");

namespace simgrid::kernel::activity {

/* -------- Acquisition -------- */

void SemAcquisitionImpl::wait_for(actor::ActorImpl* issuer, double timeout)
{
  xbt_assert(std::isfinite(timeout), "timeout is not finite!");
  xbt_assert(issuer == issuer_, "Cannot wait on acquisitions created by another actor (id %ld)", issuer_->get_pid());

  XBT_DEBUG("Wait semaphore %p (timeout:%f)", this, timeout);

  this->register_simcall(&issuer_->simcall_); // Block on that acquisition

  if (granted_) {
    post();
  } else if (timeout > 0) {
    surf_action_ = get_issuer()->get_host()->get_cpu()->sleep(timeout);
    surf_action_->set_activity(this);

  } else {
    // Already in the queue
  }
}
void SemAcquisitionImpl::post()
{
  finish();
}
void SemAcquisitionImpl::finish()
{
  xbt_assert(simcalls_.size() == 1, "Unexpected number of simcalls waiting: %zu", simcalls_.size());
  actor::Simcall* simcall = simcalls_.front();
  simcalls_.pop_front();

  if (surf_action_ != nullptr) { // A timeout was declared
    if (surf_action_->get_state() == resource::Action::State::FINISHED) { // The timeout elapsed
      if (granted_) { // but we got the semaphore, just in time!
        set_state(State::DONE);

      } else { // we have to report that timeout
        cancel(); // Unregister the acquisition from the semaphore

        /* Return to the englobing simcall that the wait_for timeouted */
        auto* observer = dynamic_cast<kernel::actor::SemaphoreAcquisitionObserver*>(get_issuer()->simcall_.observer_);
        xbt_assert(observer != nullptr);
        observer->set_result(true);
      }
    }
    surf_action_->unref();
    surf_action_ = nullptr;
  }

  simcall->issuer_->waiting_synchro_ = nullptr;
  simcall->issuer_->simcall_answer();
}
void SemAcquisitionImpl::cancel()
{
  /* Remove myself from the list of interested parties */
  auto issuer = get_issuer();
  auto it     = std::find_if(semaphore_->ongoing_acquisitions_.begin(), semaphore_->ongoing_acquisitions_.end(),
                             [issuer](SemAcquisitionImplPtr acqui) { return acqui->get_issuer() == issuer; });
  xbt_assert(it != semaphore_->ongoing_acquisitions_.end(),
             "Cannot find myself in the waiting queue that I have to leave");
  semaphore_->ongoing_acquisitions_.erase(it);
}

/* -------- Semaphore -------- */
unsigned SemaphoreImpl::next_id_ = 0;

SemAcquisitionImplPtr SemaphoreImpl::acquire_async(actor::ActorImpl* issuer)
{
  auto res = SemAcquisitionImplPtr(new kernel::activity::SemAcquisitionImpl(issuer, this), true);

  if (value_ > 0) {
    value_--;
    res->granted_ = true;
  } else {
    /* No free token in the semaphore; register the acquisition */
    ongoing_acquisitions_.push_back(res);
  }
  return res;
}
void SemaphoreImpl::release()
{
  XBT_DEBUG("Sem release semaphore %p", this);

  if (not ongoing_acquisitions_.empty()) {
    /* Release the first waiting actor */

    auto acqui = ongoing_acquisitions_.front();
    ongoing_acquisitions_.pop_front();

    acqui->granted_ = true;
    if (acqui == acqui->get_issuer()->waiting_synchro_)
      acqui->post();
    // else, the issuer is not blocked on this acquisition so no need to release it

  } else {
    // nobody's waiting here
    value_++;
  }
}

} // namespace simgrid::kernel::activity
