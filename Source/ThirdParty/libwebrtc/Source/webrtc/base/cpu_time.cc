/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/cpu_time.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"

#if defined(WEBRTC_LINUX)
#include <time.h>
#elif defined(WEBRTC_MAC)
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/times.h>
#include <mach/thread_info.h>
#include <mach/thread_act.h>
#include <mach/mach_init.h>
#include <unistd.h>
#elif defined(WEBRTC_WIN)
#include <windows.h>
#endif

#if defined(WEBRTC_WIN)
namespace {
// FILETIME resolution is 100 nanosecs.
const int64_t kNanosecsPerFiletime = 100;
}  // namespace
#endif

namespace rtc {

int64_t GetProcessCpuTimeNanos() {
#if defined(WEBRTC_LINUX)
  struct timespec ts;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
    return ts.tv_sec * kNumNanosecsPerSec + ts.tv_nsec;
  } else {
    LOG_ERR(LS_ERROR) << "clock_gettime() failed.";
  }
#elif defined(WEBRTC_MAC)
  struct rusage rusage;
  if (getrusage(RUSAGE_SELF, &rusage) == 0) {
    return rusage.ru_utime.tv_sec * kNumNanosecsPerSec +
           rusage.ru_utime.tv_usec * kNumNanosecsPerMicrosec;
  } else {
    LOG_ERR(LS_ERROR) << "getrusage() failed.";
  }
#elif defined(WEBRTC_WIN)
  FILETIME createTime;
  FILETIME exitTime;
  FILETIME kernelTime;
  FILETIME userTime;
  if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime,
                      &userTime) != 0) {
    return ((static_cast<uint64_t>(userTime.dwHighDateTime) << 32) +
            userTime.dwLowDateTime) *
           kNanosecsPerFiletime;
  } else {
    LOG_ERR(LS_ERROR) << "GetProcessTimes() failed.";
  }
#else
  // Not implemented yet.
  static_assert(
      false, "GetProcessCpuTimeNanos() platform support not yet implemented.");
#endif
  return -1;
}

int64_t GetThreadCpuTimeNanos() {
#if defined(WEBRTC_LINUX)
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return ts.tv_sec * kNumNanosecsPerSec + ts.tv_nsec;
  } else {
    LOG_ERR(LS_ERROR) << "clock_gettime() failed.";
  }
#elif defined(WEBRTC_MAC)
  thread_basic_info_data_t info;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  if (thread_info(mach_thread_self(), THREAD_BASIC_INFO, (thread_info_t)&info,
                  &count) == KERN_SUCCESS) {
    return info.user_time.seconds * kNumNanosecsPerSec +
           info.user_time.microseconds * kNumNanosecsPerMicrosec;
  } else {
    LOG_ERR(LS_ERROR) << "thread_info() failed.";
  }
#elif defined(WEBRTC_WIN)
  FILETIME createTime;
  FILETIME exitTime;
  FILETIME kernelTime;
  FILETIME userTime;
  if (GetThreadTimes(GetCurrentThread(), &createTime, &exitTime, &kernelTime,
                     &userTime) != 0) {
    return ((static_cast<uint64_t>(userTime.dwHighDateTime) << 32) +
            userTime.dwLowDateTime) *
           kNanosecsPerFiletime;
  } else {
    LOG_ERR(LS_ERROR) << "GetThreadTimes() failed.";
  }
#else
  // Not implemented yet.
  static_assert(
      false, "GetProcessCpuTimeNanos() platform support not yet implemented.");
#endif
  return -1;
}

}  // namespace rtc
