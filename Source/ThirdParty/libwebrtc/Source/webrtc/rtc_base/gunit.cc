/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/gunit.h"

#include <string>

#include "rtc_base/stringutils.h"

::testing::AssertionResult AssertStartsWith(const char* str_expr,
                                            const char* prefix_expr,
                                            const std::string& str,
                                            const std::string& prefix) {
  if (rtc::starts_with(str.c_str(), prefix.c_str())) {
    return ::testing::AssertionSuccess();
  } else {
    return ::testing::AssertionFailure()
           << str_expr << "\nwhich is\n\"" << str << "\"\ndoes not start with\n"
           << prefix_expr << "\nwhich is\n\"" << prefix << "\"";
  }
}

::testing::AssertionResult AssertStringContains(const char* str_expr,
                                                const char* substr_expr,
                                                const std::string& str,
                                                const std::string& substr) {
  if (str.find(substr) != std::string::npos) {
    return ::testing::AssertionSuccess();
  } else {
    return ::testing::AssertionFailure()
           << str_expr << "\nwhich is\n\"" << str << "\"\ndoes not contain\n"
           << substr_expr << "\nwhich is\n\"" << substr << "\"";
  }
}
