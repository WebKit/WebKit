/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_CALLBACK_LIST_WITH_LOCKS_H_
#define RTC_BASE_CALLBACK_LIST_WITH_LOCKS_H_

#include <utility>

#include "rtc_base/callback_list.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
// This class wraps a CallbackList in a mutex, so that the methods
// can be called from any thread.
// Note that recursive calls *will* cause a deadlock.
// TOOD: https://issues.webrtc.org/457303638 - remove the need for locks.
template <typename... ArgT>
class CallbackListWithLocks {
 public:
  CallbackListWithLocks() = default;
  CallbackListWithLocks(const CallbackListWithLocks&) = delete;
  CallbackListWithLocks& operator=(const CallbackListWithLocks&) = delete;
  CallbackListWithLocks(CallbackListWithLocks&&) = delete;
  CallbackListWithLocks& operator=(CallbackListWithLocks&&) = delete;

  // Adds a new receiver. The receiver (a callable object or a function pointer)
  // must be movable, but need not be copyable. Its call signature should be
  // `void(ArgT...)`. The removal tag is a pointer to an arbitrary object that
  // you own, and that will stay alive until the CallbackList is gone, or until
  // all receivers using it as a removal tag have been removed; you can use it
  // to remove the receiver.
  template <typename F>
  void AddReceiver(const void* removal_tag, F&& f) {
    MutexLock lock(&callback_mutex_);
    callbacks_.AddReceiver(removal_tag, std::forward<F>(f));
  }

  // Adds a new receiver with no removal tag.
  template <typename F>
  void AddReceiver(F&& f) {
    MutexLock lock(&callback_mutex_);
    callbacks_.AddReceiver(std::forward<F>(f));
  }

  // Removes all receivers that were added with the given removal tag.
  // Must not be called from within a callback.
  void RemoveReceivers(const void* removal_tag) {
    MutexLock lock(&callback_mutex_);
    callbacks_.RemoveReceivers(removal_tag);
  }

  // Calls all receivers with the given arguments. While the Send is in
  // progress, no method calls are allowed; specifically, this means that the
  // callbacks may not do anything with this CallbackList instance.
  //
  // Note: Receivers are called serially, but not necessarily in the same order
  // they were added.
  template <typename... ArgU>
  void Send(ArgU&&... args) {
    MutexLock lock(&callback_mutex_);
    callbacks_.Send(std::forward<ArgU...>(args...));
  }

 private:
  Mutex callback_mutex_;
  CallbackList<ArgT...> callbacks_ RTC_GUARDED_BY(callback_mutex_);
};

}  // namespace webrtc
#endif  // RTC_BASE_CALLBACK_LIST_WITH_LOCKS_H_
