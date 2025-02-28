/* Copyright (c) 2004-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef SURF_MODEL_H_
#define SURF_MODEL_H_

#include <xbt/asserts.h>
#include <xbt/function_types.h>

#include "src/internal_config.h"

#include <cfloat>
#include <cmath>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/*********
 * Utils *
 *********/

/* user-visible parameters */
XBT_PUBLIC_DATA double sg_maxmin_precision;
XBT_PUBLIC_DATA double sg_surf_precision;
XBT_PUBLIC_DATA int sg_concurrency_limit;

extern XBT_PRIVATE std::unordered_map<std::string, simgrid::kernel::profile::Profile*> traces_set_list;

/** set of hosts for which one want to be notified if they ever restart */
inline auto& watched_hosts() // avoid static initialization order fiasco
{
  static std::set<std::string, std::less<>> value;
  return value;
}

static inline void double_update(double* variable, double value, double precision)
{
  if (false) { // debug
    fprintf(stderr, "Updating %g -= %g +- %g\n", *variable, value, precision);
    xbt_assert(value == 0.0 || value > precision);
    // Check that precision is higher than the machine-dependent size of the mantissa. If not, brutal rounding  may
    // happen, and the precision mechanism is not active...
    xbt_assert(FLT_RADIX == 2 && *variable < precision * exp2(DBL_MANT_DIG));
  }
  *variable -= value;
  if (*variable < precision)
    *variable = 0.0;
}

static inline int double_positive(double value, double precision)
{
  return (value > precision);
}

static inline int double_equals(double value1, double value2, double precision)
{
  return (fabs(value1 - value2) < precision);
}

/** @ingroup SURF_models
 *  @brief Initializes the CPU model with the model Cas01
 *
 *  By default, this model uses the lazy optimization mechanism that relies on partial invalidation in LMM and a heap
 *  for lazy action update.
 *  You can change this behavior by setting the cpu/optim configuration variable to a different value.
 *
 *  You shouldn't have to call it by yourself.
 */
XBT_PUBLIC void surf_cpu_model_init_Cas01();

XBT_PUBLIC void surf_disk_model_init_S19();

/** @ingroup SURF_models
 *  @brief Same as network model 'LagrangeVelho', only with different correction factors.
 *
 * This model is proposed by Pierre-Nicolas Clauss and Martin Quinson and Stéphane Génaud based on the model 'LV08' and
 * different correction factors depending on the communication size (< 1KiB, < 64KiB, >= 64KiB).
 * See comments in the code for more information.
 *
 *  @see surf_host_model_init_SMPI()
 */
XBT_PUBLIC void surf_network_model_init_SMPI();

/** @ingroup SURF_models
 *  @brief Same as network model 'LagrangeVelho', only with different correction factors.
 *
 * This model implements a variant of the contention model on Infiniband networks based on
 * the works of Jérôme Vienne : http://mescal.imag.fr/membres/jean-marc.vincent/index.html/PhD/Vienne.pdf
 *
 *  @see surf_host_model_init_IB()
 */
#if !HAVE_SMPI
XBT_ATTRIB_NORETURN
#endif
XBT_PUBLIC void surf_network_model_init_IB();

/** @ingroup SURF_models
 *  @brief Initializes the platform with the network model 'LegrandVelho'
 *
 * This model is proposed by Arnaud Legrand and Pedro Velho based on the results obtained with the GTNets simulator for
 * onelink and dogbone sharing scenarios. See comments in the code for more information.
 *
 *  @see surf_host_model_init_LegrandVelho()
 */
XBT_PUBLIC void surf_network_model_init_LegrandVelho();

/** @ingroup SURF_models
 *  @brief Initializes the platform with the network model 'Constant'
 *
 *  In this model, the communication time between two network cards is constant, hence no need for a routing table.
 *  This is particularly useful when simulating huge distributed algorithms where scalability is really an issue. This
 *  function is called in conjunction with surf_host_model_init_compound.
 *
 *  @see surf_host_model_init_compound()
 */
XBT_PUBLIC void surf_network_model_init_Constant();

/** @ingroup SURF_models
 *  @brief Initializes the platform with the network model CM02
 *
 *  You should call this function by yourself only if you plan using surf_host_model_init_compound.
 *  See comments in the code for more information.
 */
XBT_PUBLIC void surf_network_model_init_CM02();

/** @ingroup SURF_models
 *  @brief Initializes the platform with the network model NS3
 *
 *  This function is called by surf_host_model_init_NS3 or by yourself only if you plan using
 *  surf_host_model_init_compound
 *
 *  @see surf_host_model_init_NS3()
 */
#if !SIMGRID_HAVE_NS3
XBT_ATTRIB_NORETURN
#endif
XBT_PUBLIC void surf_network_model_init_NS3();

/** @ingroup SURF_models
 *  @brief Initializes the VM model used in the platform
 *
 *  A VM model depends on the physical CPU model to share the resources inside the VM
 *  It will also creates the CPU model for actions running inside the VM
 *
 *  Such model is subject to modification with warning in the ChangeLog so monitor it!
 */
XBT_PUBLIC void surf_vm_model_init_HL13(simgrid::kernel::resource::CpuModel* cpu_pm_model);

/** @ingroup SURF_models
 *  @brief Initializes the platform with a compound host model
 *
 *  This function should be called after a cpu_model and a network_model have been set up.
 */
XBT_PUBLIC void surf_host_model_init_compound();

/** @ingroup SURF_models
 *  @brief Initializes the platform with the current best network and cpu models at hand
 *
 *  This platform model separates the host model and the network model.
 *  The host model will be initialized with the model compound, the network model with the model LV08 (with cross
 *  traffic support) and the CPU model with the model Cas01.
 *  Such model is subject to modification with warning in the ChangeLog so monitor it!
 */
XBT_PUBLIC void surf_host_model_init_current_default();

/** @ingroup SURF_models
 *  @brief Initializes the platform with the model L07
 *
 *  With this model, only parallel tasks can be used. Resource sharing is done by identifying bottlenecks and giving an
 *  equal share of the model to each action.
 */
XBT_PUBLIC void surf_host_model_init_ptask_L07();

/* --------------------
 *  Model Descriptions
 * -------------------- */
/** @brief Resource model description */
struct surf_model_description_t {
  const char* name;
  const char* description;
  std::function<void()> model_init_preparse;
};

XBT_PUBLIC const surf_model_description_t* find_model_description(const std::vector<surf_model_description_t>& table,
                                                                  const std::string& name);
XBT_PUBLIC void model_help(const char* category, const std::vector<surf_model_description_t>& table);

#define SIMGRID_REGISTER_PLUGIN(id, desc, init)                                                                        \
  static void XBT_ATTRIB_CONSTRUCTOR(800) _XBT_CONCAT3(simgrid_, id, _plugin_register)()                               \
  {                                                                                                                    \
    surf_plugin_description().emplace_back(surf_model_description_t{_XBT_STRINGIFY(id), (desc), (init)});              \
  }

/** @brief The list of all available plugins */
inline auto& surf_plugin_description() // Function to avoid static initialization order fiasco
{
  static std::vector<surf_model_description_t> plugin_description_table;
  return plugin_description_table;
}
/** @brief The list of all available optimization modes (both for cpu and networks).
 *  These optimization modes can be set using --cfg=cpu/optim:... and --cfg=network/optim:... */
XBT_PUBLIC_DATA const std::vector<surf_model_description_t> surf_optimization_mode_description;
/** @brief The list of all cpu models (pick one with --cfg=cpu/model) */
XBT_PUBLIC_DATA const std::vector<surf_model_description_t> surf_cpu_model_description;
/** @brief The list of all network models (pick one with --cfg=network/model) */
XBT_PUBLIC_DATA const std::vector<surf_model_description_t> surf_network_model_description;
/** @brief The list of all disk models (pick one with --cfg=disk/model) */
XBT_PUBLIC_DATA const std::vector<surf_model_description_t> surf_disk_model_description;
/** @brief The list of all host models (pick one with --cfg=host/model:) */
XBT_PUBLIC_DATA const std::vector<surf_model_description_t> surf_host_model_description;

#endif /* SURF_MODEL_H_ */
