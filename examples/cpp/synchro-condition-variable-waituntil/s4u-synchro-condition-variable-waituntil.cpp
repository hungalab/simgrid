/* Copyright (c) 2006-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <mutex>           /* std::mutex and std::lock_guard */
#include <simgrid/s4u.hpp> /* All of S4U */

XBT_LOG_NEW_DEFAULT_CATEGORY(s4u_test, "a sample log category");
namespace sg4 = simgrid::s4u;

sg4::MutexPtr mtx            = nullptr;
sg4::ConditionVariablePtr cv = nullptr;
bool ready = false;

static void competitor(int id)
{
  XBT_INFO("Entering the race...");
  std::unique_lock lck(*mtx);
  while (not ready) {
    auto now = sg4::Engine::get_clock();
    if (cv->wait_until(lck, now + (id+1)*0.25) == std::cv_status::timeout) {
      XBT_INFO("Out of wait_until (timeout)");
    }
    else {
      XBT_INFO("Out of wait_until (YAY!)");
    }
  }
  XBT_INFO("Running!");
}

static void go()
{
  XBT_INFO("Are you ready? ...");
  sg4::this_actor::sleep_for(3);
  std::unique_lock lck(*mtx);
  XBT_INFO("Go go go!");
  ready = true;
  cv->notify_all();
}

static void main_actor()
{
  mtx = sg4::Mutex::create();
  cv  = sg4::ConditionVariable::create();

  auto host = sg4::this_actor::get_host();
  for (int i = 0; i < 10; ++i)
    sg4::Actor::create("competitor", host, competitor, i);
  sg4::Actor::create("go", host, go);
}

int main(int argc, char* argv[])
{
  sg4::Engine e(&argc, argv);
  e.load_platform("../../platforms/small_platform.xml");

  sg4::Actor::create("main", e.host_by_name("Tremblay"), main_actor);

  e.run();
  return 0;
}
