/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/win/windows_version.h"

#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"

namespace rtc {
namespace rtc_win {
namespace {

void MethodSupportedOnWin10AndLater() {
  RTC_DLOG(LS_INFO) << "MethodSupportedOnWin10AndLater";
}

void MethodNotSupportedOnWin10AndLater() {
  RTC_DLOG(LS_INFO) << "MethodNotSupportedOnWin10AndLater";
}

// Use global GetVersion() and use it in a way a user would typically use it
// when checking for support of a certain API:
// "if (rtc_win::GetVersion() < VERSION_WIN10) ...".
TEST(WindowsVersion, GetVersionGlobalScopeAccessor) {
  if (GetVersion() < VERSION_WIN10) {
    MethodNotSupportedOnWin10AndLater();
  } else {
    MethodSupportedOnWin10AndLater();
  }
}

TEST(WindowsVersion, ProcessorModelName) {
  std::string name = OSInfo::GetInstance()->processor_model_name();
  EXPECT_FALSE(name.empty());
  RTC_DLOG(LS_INFO) << "processor_model_name: " << name;
}

}  // namespace
}  // namespace rtc_win
}  // namespace rtc
