/* Copyright (c) 2013-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "src/surf/HostImpl.hpp"

#ifndef HOST_CLM03_HPP_
#define HOST_CLM03_HPP_

/***********
 * Classes *
 ***********/

namespace simgrid::kernel::resource {

class XBT_PRIVATE HostCLM03Model : public HostModel {
public:
  using HostModel::HostModel;
  double next_occurring_event(double now) override;
  void update_actions_state(double now, double delta) override;
  Action* execute_thread(const s4u::Host* host, double flops_amount, int thread_count) override;
  Action* execute_parallel(const std::vector<s4u::Host*>& host_list, const double* flops_amount,
                           const double* bytes_amount, double rate) override;
  Action* io_stream(s4u::Host* src_host, DiskImpl* src_disk, s4u::Host* dst_host, DiskImpl* dst_disk, double size) override;
};
} // namespace simgrid::kernel::resource

#endif /* HOST_CLM03_HPP_ */
