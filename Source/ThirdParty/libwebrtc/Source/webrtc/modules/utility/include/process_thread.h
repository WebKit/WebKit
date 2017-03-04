/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_
#define WEBRTC_MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_

#include <memory>

#include "webrtc/typedefs.h"

#if defined(WEBRTC_WIN)
// Due to a bug in the std::unique_ptr implementation that ships with MSVS,
// we need the full definition of QueuedTask, on Windows.
#include "webrtc/base/task_queue.h"
#else
namespace rtc {
class QueuedTask;
}
#endif

namespace rtc {
class Location;
}

namespace webrtc {
class Module;

// TODO(tommi): ProcessThread probably doesn't need to be a virtual
// interface.  There exists one override besides ProcessThreadImpl,
// MockProcessThread, but when looking at how it is used, it seems
// a nullptr might suffice (or simply an actual ProcessThread instance).
class ProcessThread {
 public:
  virtual ~ProcessThread();

  static std::unique_ptr<ProcessThread> Create(const char* thread_name);

  // Starts the worker thread.  Must be called from the construction thread.
  virtual void Start() = 0;

  // Stops the worker thread.  Must be called from the construction thread.
  virtual void Stop() = 0;

  // Wakes the thread up to give a module a chance to do processing right
  // away.  This causes the worker thread to wake up and requery the specified
  // module for when it should be called back. (Typically the module should
  // return 0 from TimeUntilNextProcess on the worker thread at that point).
  // Can be called on any thread.
  virtual void WakeUp(Module* module) = 0;

  // Queues a task object to run on the worker thread.  Ownership of the
  // task object is transferred to the ProcessThread and the object will
  // either be deleted after running on the worker thread, or on the
  // construction thread of the ProcessThread instance, if the task did not
  // get a chance to run (e.g. posting the task while shutting down or when
  // the thread never runs).
  virtual void PostTask(std::unique_ptr<rtc::QueuedTask> task) = 0;

  // Adds a module that will start to receive callbacks on the worker thread.
  // Can be called from any thread.
  virtual void RegisterModule(Module* module, const rtc::Location& from) = 0;

  // Removes a previously registered module.
  // Can be called from any thread.
  virtual void DeRegisterModule(Module* module) = 0;
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_
