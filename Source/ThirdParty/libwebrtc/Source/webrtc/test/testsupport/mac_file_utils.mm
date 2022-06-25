/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdlib.h>

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

void GetNSExecutablePath(std::string* path) {
  RTC_DCHECK(path);
  // Executable path can have relative references ("..") depending on
  // how the app was launched.
  uint32_t executable_length = 0;
  _NSGetExecutablePath(NULL, &executable_length);
  RTC_DCHECK_GT(executable_length, 1u);
  char executable_path[PATH_MAX + 1];
  int rv = _NSGetExecutablePath(executable_path, &executable_length);
  RTC_DCHECK_EQ(rv, 0);

  char full_path[PATH_MAX];
  if (realpath(executable_path, full_path) == nullptr) {
    *path = "";
    return;
  }

  *path = full_path;
}

}  // namespace test
}  // namespace webrtc
