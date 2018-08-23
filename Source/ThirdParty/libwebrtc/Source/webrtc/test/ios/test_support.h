/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_IOS_TEST_SUPPORT_H_
#define TEST_IOS_TEST_SUPPORT_H_

namespace rtc {
namespace test {
// Launches an iOS app that serves as a host for a test suite.
// This is necessary as iOS doesn't like processes without a gui
// running for longer than a few seconds.
void RunTestsFromIOSApp();
void InitTestSuite(int (*test_suite)(void),
                   int argc,
                   char* argv[],
                   bool save_chartjson_result);

}  // namespace test
}  // namespace rtc

#endif  // TEST_IOS_TEST_SUPPORT_H_
