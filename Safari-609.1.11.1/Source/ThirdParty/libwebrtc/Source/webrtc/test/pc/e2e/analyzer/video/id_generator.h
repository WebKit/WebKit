/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_ID_GENERATOR_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_ID_GENERATOR_H_

#include <atomic>

namespace webrtc {
namespace webrtc_pc_e2e {

// IdGenerator generates ids. All provided ids have to be unique. There is no
// any order guarantees for provided ids.
template <typename T>
class IdGenerator {
 public:
  virtual ~IdGenerator() = default;

  // Returns next unique id. There is no any order guarantees for provided ids.
  virtual T GetNextId() = 0;
};

// Generates int ids. It is assumed, that no more then max int value ids will be
// requested from this generator.
class IntIdGenerator : public IdGenerator<int> {
 public:
  explicit IntIdGenerator(int start_value);
  ~IntIdGenerator() override;

  int GetNextId() override;

 private:
  std::atomic<int> next_id_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_ID_GENERATOR_H_
