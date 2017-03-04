/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/internaldecoderfactory.h"

#include "webrtc/test/gtest.h"

TEST(InternalDecoderFactory, TestVP8) {
  cricket::InternalDecoderFactory factory;
  webrtc::VideoDecoder* decoder =
      factory.CreateVideoDecoder(webrtc::kVideoCodecVP8);
  EXPECT_TRUE(decoder);
  factory.DestroyVideoDecoder(decoder);
}
