/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/ortc/mediadescription.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

class MediaDescriptionTest : public testing::Test {};

TEST_F(MediaDescriptionTest, CreateMediaDescription) {
  MediaDescription m("a");
  EXPECT_EQ("a", m.mid());
}

TEST_F(MediaDescriptionTest, AddSdesParam) {
  MediaDescription m("a");
  m.sdes_params().push_back(cricket::CryptoParams());
  const std::vector<cricket::CryptoParams>& params = m.sdes_params();
  EXPECT_EQ(1u, params.size());
}

}  // namespace webrtc
