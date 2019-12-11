/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internaldecoderfactory.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "media/base/mediaconstants.h"
#include "test/gtest.h"

namespace webrtc {

TEST(InternalDecoderFactory, TestVP8) {
  InternalDecoderFactory factory;
  std::unique_ptr<VideoDecoder> decoder =
      factory.CreateVideoDecoder(SdpVideoFormat(cricket::kVp8CodecName));
  EXPECT_TRUE(decoder);
}

}  // namespace webrtc
