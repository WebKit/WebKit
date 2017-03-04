/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_

#include <list>
#include <memory>
#include <queue>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/location.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ProcessThreadImpl : public ProcessThread {
 public:
  explicit ProcessThreadImpl(const char* thread_name);
  ~ProcessThreadImpl() override;

  void Start() override;
  void Stop() override;

  void WakeUp(Module* module) override;
  void PostTask(std::unique_ptr<rtc::QueuedTask> task) override;

  void RegisterModule(Module* module, const rtc::Location& from) override;
  void DeRegisterModule(Module* module) override;

 protected:
  static bool Run(void* obj);
  bool Process();

 private:
  struct ModuleCallback {
    ModuleCallback() = delete;
    ModuleCallback(ModuleCallback&& cb) = default;
    ModuleCallback(const ModuleCallback& cb) = default;
    ModuleCallback(Module* module, const rtc::Location& location)
        : module(module), location(location) {}
    bool operator==(const ModuleCallback& cb) const {
      return cb.module == module;
    }

    Module* const module;
    int64_t next_callback = 0;  // Absolute timestamp.
    const rtc::Location location;

   private:
    ModuleCallback& operator=(ModuleCallback&);
  };

  typedef std::list<ModuleCallback> ModuleList;

  // Warning: For some reason, if |lock_| comes immediately before |modules_|
  // with the current class layout, we will  start to have mysterious crashes
  // on Mac 10.9 debug.  I (Tommi) suspect we're hitting some obscure alignemnt
  // issues, but I haven't figured out what they are, if there are alignment
  // requirements for mutexes on Mac or if there's something else to it.
  // So be careful with changing the layout.
  rtc::CriticalSection lock_;  // Used to guard modules_, tasks_ and stop_.

  rtc::ThreadChecker thread_checker_;
  const std::unique_ptr<EventWrapper> wake_up_;
  // TODO(pbos): Remove unique_ptr and stop recreating the thread.
  std::unique_ptr<rtc::PlatformThread> thread_;

  ModuleList modules_;
  std::queue<rtc::QueuedTask*> queue_;
  bool stop_;
  const char* thread_name_;
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_
