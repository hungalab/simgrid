/* Copyright (c) 2004-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <simgrid/s4u/Engine.hpp>
#include <xbt/module.h>

#include "mc/mc.h"
#include "simgrid/sg_config.hpp"
#include "src/kernel/resource/profile/FutureEvtSet.hpp"
#include "src/kernel/resource/profile/Profile.hpp"
#include "src/surf/HostImpl.hpp"
#include "src/surf/surf_interface.hpp"

#include <fstream>
#include <string>

XBT_LOG_NEW_CATEGORY(surf, "All SURF categories");
XBT_LOG_NEW_DEFAULT_SUBCATEGORY(surf_kernel, surf, "Logging specific to SURF (kernel)");

/*********
 * Utils *
 *********/

simgrid::kernel::profile::FutureEvtSet future_evt_set;
std::vector<std::string> surf_path;

const std::vector<surf_model_description_t> surf_network_model_description = {
    {"LV08",
     "Realistic network analytic model (slow-start modeled by multiplying latency by 13.01, bandwidth by .97; "
     "bottleneck sharing uses a payload of S=20537 for evaluating RTT). ",
     &surf_network_model_init_LegrandVelho},
    {"Constant",
     "Simplistic network model where all communication take a constant time (one second). This model "
     "provides the lowest realism, but is (marginally) faster.",
     &surf_network_model_init_Constant},
    {"SMPI",
     "Realistic network model specifically tailored for HPC settings (accurate modeling of slow start with "
     "correction factors on three intervals: < 1KiB, < 64 KiB, >= 64 KiB)",
     &surf_network_model_init_SMPI},
    {"IB", "Realistic network model specifically tailored for HPC settings, with Infiniband contention model",
     &surf_network_model_init_IB},
    {"CM02",
     "Legacy network analytic model (Very similar to LV08, but without corrective factors. The timings of "
     "small messages are thus poorly modeled).",
     &surf_network_model_init_CM02},
    {"ns-3", "Network pseudo-model using the ns-3 tcp model instead of an analytic model",
     &surf_network_model_init_NS3},
};

#if !HAVE_SMPI
void surf_network_model_init_IB()
{
  xbt_die("Please activate SMPI support in cmake to use the IB network model.");
}
#endif
#if !SIMGRID_HAVE_NS3
void surf_network_model_init_NS3()
{
  xbt_die("Please activate ns-3 support in cmake and install the dependencies to use the NS3 network model.");
}
#endif

const std::vector<surf_model_description_t> surf_cpu_model_description = {
    {"Cas01", "Simplistic CPU model (time=size/speed).", &surf_cpu_model_init_Cas01},
};

const std::vector<surf_model_description_t> surf_disk_model_description = {
    {"S19", "Simplistic disk model.", &surf_disk_model_init_S19},
};

const std::vector<surf_model_description_t> surf_host_model_description = {
    {"default", "Default host model. Currently, CPU:Cas01, network:LV08 (with cross traffic enabled), and disk:S19",
     &surf_host_model_init_current_default},
    {"compound", "Host model that is automatically chosen if you change the CPU, network, and disk models",
     &surf_host_model_init_compound},
    {"ptask_L07", "Host model somehow similar to Cas01+CM02+S19 but allowing parallel tasks",
     &surf_host_model_init_ptask_L07},
};

const std::vector<surf_model_description_t> surf_optimization_mode_description = {
    {"Lazy", "Lazy action management (partial invalidation in lmm + heap in action remaining).", nullptr},
    {"TI",
     "Trace integration. Highly optimized mode when using availability traces (only available for the Cas01 CPU "
     "model for now).",
     nullptr},
    {"Full", "Full update of remaining and variables. Slow but may be useful when debugging.", nullptr},
};

/** Displays the long description of all registered models, and quit */
void model_help(const char* category, const std::vector<surf_model_description_t>& table)
{
  XBT_HELP("Long description of the %s models accepted by this simulator:", category);
  for (auto const& item : table)
    XBT_HELP("  %s: %s", item.name, item.description);
}

const surf_model_description_t* find_model_description(const std::vector<surf_model_description_t>& table,
                                                       const std::string& name)
{
  if (auto pos = std::find_if(table.begin(), table.end(),
                              [&name](const surf_model_description_t& item) { return item.name == name; });
      pos != table.end())
    return &*pos;

  std::string sep;
  std::string name_list;
  for (auto const& item : table) {
    name_list += sep + item.name;
    sep = ", ";
  }
  xbt_die("Model '%s' is invalid! Valid models are: %s.", name.c_str(), name_list.c_str());
}
