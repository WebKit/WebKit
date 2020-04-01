/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_PERF_TEST_GRAPHJSON_WRITER_H_
#define TEST_TESTSUPPORT_PERF_TEST_GRAPHJSON_WRITER_H_

#include <string>

#include "test/testsupport/perf_test.h"
#include "test/testsupport/perf_test_result_writer.h"

namespace webrtc {
namespace test {

PerfTestResultWriter* CreateGraphJsonWriter();

// Utilities that happen to be useful to perf_test.cc. Just move these back
// to perf_test.cc when this file goes away.
std::string UnitWithDirection(const std::string& units,
                              webrtc::test::ImproveDirection improve_direction);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_PERF_TEST_GRAPHJSON_WRITER_H_
