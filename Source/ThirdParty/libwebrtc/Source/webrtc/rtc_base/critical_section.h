/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CRITICAL_SECTION_H_
#define RTC_BASE_CRITICAL_SECTION_H_

#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_WIN)
// clang-format off
// clang formating would change include order.

// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h. To include win32.h directly, it must be broken out into its own
// build target.
#include <winsock2.h>
#include <windows.h>
#include <sal.h>  // must come after windows headers.
// clang-format on
#endif  // defined(WEBRTC_WIN)

#if defined(WEBRTC_POSIX)
#include <pthread.h>
#endif

// See notes in the 'Performance' unit test for the effects of this flag.
#define RTC_USE_NATIVE_MUTEX_ON_MAC 1

#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
#include <dispatch/dispatch.h>
#endif

namespace rtc {

// Locking methods (Enter, TryEnter, Leave)are const to permit protecting
// members inside a const context without requiring mutable CriticalSections
// everywhere. CriticalSection is reentrant lock.
class RTC_LOCKABLE RTC_EXPORT CriticalSection {
 public:
  CriticalSection();
  ~CriticalSection();

  void Enter() const RTC_EXCLUSIVE_LOCK_FUNCTION();
  bool TryEnter() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void Leave() const RTC_UNLOCK_FUNCTION();

 private:
  // Use only for RTC_DCHECKing.
  bool CurrentThreadIsOwner() const;

#if defined(WEBRTC_WIN)
  mutable CRITICAL_SECTION crit_;
#elif defined(WEBRTC_POSIX)
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
  // Number of times the lock has been locked + number of threads waiting.
  // TODO(tommi): We could use this number and subtract the recursion count
  // to find places where we have multiple threads contending on the same lock.
  mutable volatile int lock_queue_;
  // |recursion_| represents the recursion count + 1 for the thread that owns
  // the lock. Only modified by the thread that owns the lock.
  mutable int recursion_;
  // Used to signal a single waiting thread when the lock becomes available.
  mutable dispatch_semaphore_t semaphore_;
  // The thread that currently holds the lock. Required to handle recursion.
  mutable PlatformThreadRef owning_thread_;
#else
  mutable pthread_mutex_t mutex_;
#endif
  mutable PlatformThreadRef thread_;  // Only used by RTC_DCHECKs.
  mutable int recursion_count_;       // Only used by RTC_DCHECKs.
#else  // !defined(WEBRTC_WIN) && !defined(WEBRTC_POSIX)
#error Unsupported platform.
#endif
};

// CritScope, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE CritScope {
 public:
  explicit CritScope(const CriticalSection* cs) RTC_EXCLUSIVE_LOCK_FUNCTION(cs);
  ~CritScope() RTC_UNLOCK_FUNCTION();

 private:
  const CriticalSection* const cs_;
  RTC_DISALLOW_COPY_AND_ASSIGN(CritScope);
};

// A lock used to protect global variables. Do NOT use for other purposes.
class RTC_LOCKABLE GlobalLock {
 public:
  constexpr GlobalLock() : lock_acquired_(0) {}

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION();
  void Unlock() RTC_UNLOCK_FUNCTION();

 private:
  volatile int lock_acquired_;
};

// GlobalLockScope, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE GlobalLockScope {
 public:
  explicit GlobalLockScope(GlobalLock* lock) RTC_EXCLUSIVE_LOCK_FUNCTION(lock);
  ~GlobalLockScope() RTC_UNLOCK_FUNCTION();

 private:
  GlobalLock* const lock_;
  RTC_DISALLOW_COPY_AND_ASSIGN(GlobalLockScope);
};

}  // namespace rtc

#endif  // RTC_BASE_CRITICAL_SECTION_H_
