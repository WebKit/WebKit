/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/default_temporal_layers.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "webrtc/modules/video_coding/codecs/vp8/vp8_impl.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

enum {
  kTemporalUpdateLast = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_REF_GF |
                        VP8_EFLAG_NO_REF_ARF,
  kTemporalUpdateGoldenWithoutDependency =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGolden =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltrefWithoutDependency =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF |
      VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltref = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateNone = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_UPD_LAST |
                        VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefAltRef = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF |
                                   VP8_EFLAG_NO_UPD_ARF |
                                   VP8_EFLAG_NO_UPD_LAST |
                                   VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefGolden = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF |
                                   VP8_EFLAG_NO_UPD_ARF |
                                   VP8_EFLAG_NO_UPD_LAST |
                                   VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateGoldenWithoutDependencyRefAltRef =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGoldenRefAltRef = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateLastRefAltRef =
      VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF,
  kTemporalUpdateLastAndGoldenRefAltRef =
      VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF,
};

TEST(TemporalLayersTest, 2Layers) {
  DefaultTemporalLayers tl(2, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[16] = {
      kTemporalUpdateLastAndGoldenRefAltRef,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastAndGoldenRefAltRef,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] = {0, 1, 0, 1, 0, 1, 0, 1,
                                   0, 1, 0, 1, 0, 1, 0, 1};

  bool expected_layer_sync[16] = {false, true,  false, false, false, false,
                                  false, false, false, true,  false, false,
                                  false, false, false, false};

  uint32_t timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config));
    tl.PopulateCodecSpecific(false, tl_config, &vp8_info, 0);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_layer_sync[i], vp8_info.layerSync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, 3Layers) {
  DefaultTemporalLayers tl(3, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[16] = {
      kTemporalUpdateLastAndGoldenRefAltRef,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastAndGoldenRefAltRef,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] = {0, 2, 1, 2, 0, 2, 1, 2,
                                   0, 2, 1, 2, 0, 2, 1, 2};

  bool expected_layer_sync[16] = {false, true,  true,  false, false, false,
                                  false, false, false, true,  true,  false,
                                  false, false, false, false};

  unsigned int timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config));
    tl.PopulateCodecSpecific(false, tl_config, &vp8_info, 0);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_layer_sync[i], vp8_info.layerSync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, 4Layers) {
  DefaultTemporalLayers tl(4, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);
  int expected_flags[16] = {
      kTemporalUpdateLast,
      kTemporalUpdateNone,
      kTemporalUpdateAltrefWithoutDependency,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
      kTemporalUpdateLast,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
      kTemporalUpdateGolden,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] = {0, 3, 2, 3, 1, 3, 2, 3,
                                   0, 3, 2, 3, 1, 3, 2, 3};

  bool expected_layer_sync[16] = {false, true, true,  true, true,  true,
                                  false, true, false, true, false, true,
                                  false, true, false, true};

  uint32_t timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config));
    tl.PopulateCodecSpecific(false, tl_config, &vp8_info, 0);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_layer_sync[i], vp8_info.layerSync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, KeyFrame) {
  DefaultTemporalLayers tl(3, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[8] = {
      kTemporalUpdateLastAndGoldenRefAltRef,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[8] = {0, 0, 0, 0, 0, 0, 0, 2};

  uint32_t timestamp = 0;
  for (int i = 0; i < 7; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config));
    tl.PopulateCodecSpecific(true, tl_config, &vp8_info, 0);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(true, vp8_info.layerSync);
    timestamp += 3000;
  }
  TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
  EXPECT_EQ(expected_flags[7], VP8EncoderImpl::EncodeFlags(tl_config));
  tl.PopulateCodecSpecific(false, tl_config, &vp8_info, 0);
  EXPECT_EQ(expected_temporal_idx[7], vp8_info.temporalIdx);
  EXPECT_EQ(true, vp8_info.layerSync);
}
}  // namespace webrtc
