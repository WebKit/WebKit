/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/multiplexcodecfactory.h"

#include <utility>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/mediaconstants.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "test/gtest.h"

namespace webrtc {

TEST(MultiplexDecoderFactory, CreateVideoDecoder) {
  std::unique_ptr<VideoDecoderFactory> internal_factory(
      new InternalDecoderFactory());
  MultiplexDecoderFactory factory(std::move(internal_factory));
  std::unique_ptr<VideoDecoder> decoder =
      factory.CreateVideoDecoder(SdpVideoFormat(
          cricket::kMultiplexCodecName,
          {{cricket::kCodecParamAssociatedCodecName, cricket::kVp9CodecName}}));
  EXPECT_TRUE(decoder);
}

TEST(MultiplexEncoderFactory, CreateVideoEncoder) {
  std::unique_ptr<VideoEncoderFactory> internal_factory(
      new InternalEncoderFactory());
  MultiplexEncoderFactory factory(std::move(internal_factory));
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(
          cricket::kMultiplexCodecName,
          {{cricket::kCodecParamAssociatedCodecName, cricket::kVp9CodecName}}));
  EXPECT_TRUE(encoder);
}

}  // namespace webrtc
