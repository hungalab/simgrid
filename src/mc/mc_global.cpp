/* Copyright (c) 2008-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "mc/mc.h"
#include "src/kernel/actor/ActorImpl.hpp"

#if SIMGRID_HAVE_MC
#include "src/mc/api/RemoteApp.hpp"
#include "src/mc/explo/Exploration.hpp"
#include "src/mc/inspect/mc_unw.hpp"
#include "src/mc/mc_config.hpp"
#include "src/mc/mc_private.hpp"
#include "src/mc/remote/AppSide.hpp"
#include "src/mc/sosp/Snapshot.hpp"

#include <array>
#include <boost/core/demangle.hpp>
#include <cerrno>
#include <cstring>
#include <libunwind.h>
#endif

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(mc_global, mc, "Logging specific to MC (global)");

namespace simgrid::mc {

std::vector<double> processes_time;

}

#if SIMGRID_HAVE_MC

namespace simgrid::mc {

/*******************************  Core of MC *******************************/
/**************************************************************************/
void dumpStack(FILE* file, unw_cursor_t* cursor)
{
  int nframe = 0;
  std::array<char, 100> buffer;

  unw_word_t off;
  do {
    const char* name = not unw_get_proc_name(cursor, buffer.data(), buffer.size(), &off) ? buffer.data() : "?";
    // Unmangle C++ names:
    std::string realname = boost::core::demangle(name);

#if defined(__x86_64__)
    unw_word_t rip = 0;
    unw_word_t rsp = 0;
    unw_get_reg(cursor, UNW_X86_64_RIP, &rip);
    unw_get_reg(cursor, UNW_X86_64_RSP, &rsp);
    fprintf(file, "  %i: %s (RIP=0x%" PRIx64 " RSP=0x%" PRIx64 ")\n", nframe, realname.c_str(), (std::uint64_t)rip,
            (std::uint64_t)rsp);
#else
    fprintf(file, "  %i: %s\n", nframe, realname.c_str());
#endif

    ++nframe;
  } while (unw_step(cursor));
}

} // namespace simgrid::mc
#endif

double MC_process_clock_get(const simgrid::kernel::actor::ActorImpl* process)
{
  if (process) {
    auto pid = static_cast<size_t>(process->get_pid());
    if (pid < simgrid::mc::processes_time.size())
      return simgrid::mc::processes_time[pid];
  }
  return 0.0;
}

void MC_process_clock_add(const simgrid::kernel::actor::ActorImpl* process, double amount)
{
  simgrid::mc::processes_time.at(process->get_pid()) += amount;
}
