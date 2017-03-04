/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/cpu_info.h"

#if defined(WEBRTC_WIN)
#include <winsock2.h>
#include <windows.h>
#ifndef EXCLUDE_D3D9
#include <d3d9.h>
#endif
#elif defined(WEBRTC_LINUX)
#include <unistd.h>
#endif
#if defined(WEBRTC_MAC)
#include <sys/sysctl.h>
#endif

#include "webrtc/base/logging.h"

namespace internal {
static int DetectNumberOfCores() {
  // We fall back on assuming a single core in case of errors.
  int number_of_cores = 1;

#if defined(WEBRTC_WIN)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  number_of_cores = static_cast<int>(si.dwNumberOfProcessors);
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
  number_of_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#elif defined(WEBRTC_MAC)
  int name[] = {CTL_HW, HW_AVAILCPU};
  size_t size = sizeof(number_of_cores);
  if (0 != sysctl(name, 2, &number_of_cores, &size, NULL, 0)) {
    LOG(LS_ERROR) << "Failed to get number of cores";
    number_of_cores = 1;
  }
#else
  LOG(LS_ERROR) << "No function to get number of cores";
#endif

  LOG(LS_INFO) << "Available number of cores: " << number_of_cores;

  return number_of_cores;
}
}

namespace webrtc {

uint32_t CpuInfo::DetectNumberOfCores() {
  // Statically cache the number of system cores available since if the process
  // is running in a sandbox, we may only be able to read the value once (before
  // the sandbox is initialized) and not thereafter.
  // For more information see crbug.com/176522.
  static uint32_t logical_cpus = 0;
  if (!logical_cpus)
    logical_cpus = static_cast<uint32_t>(internal::DetectNumberOfCores());
  return logical_cpus;
}

}  // namespace webrtc
