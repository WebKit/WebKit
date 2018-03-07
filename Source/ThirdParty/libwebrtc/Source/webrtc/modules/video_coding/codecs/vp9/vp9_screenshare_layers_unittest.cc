/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <memory>

#include "vpx/vp8cx.h"
#include "modules/video_coding/codecs/vp9/screenshare_layers.h"
#include "modules/video_coding/codecs/vp9/vp9_impl.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

typedef VP9EncoderImpl::SuperFrameRefSettings Settings;

const uint32_t kTickFrequency = 90000;

class ScreenshareLayerTestVP9 : public ::testing::Test {
 protected:
  ScreenshareLayerTestVP9() : clock_(0) {}
  virtual ~ScreenshareLayerTestVP9() {}

  void InitScreenshareLayers(int layers) {
    layers_.reset(new ScreenshareLayersVP9(layers));
  }

  void ConfigureBitrateForLayer(int kbps, uint8_t layer_id) {
    layers_->ConfigureBitrate(kbps, layer_id);
  }

  void AdvanceTime(int64_t milliseconds) {
    clock_.AdvanceTimeMilliseconds(milliseconds);
  }

  void AddKilobitsToLayer(int kilobits, uint8_t layer_id) {
    layers_->LayerFrameEncoded(kilobits * 1000 / 8, layer_id);
  }

  void EqualRefsForLayer(const Settings& actual, uint8_t layer_id) {
    EXPECT_EQ(expected_.layer[layer_id].upd_buf,
              actual.layer[layer_id].upd_buf);
    EXPECT_EQ(expected_.layer[layer_id].ref_buf1,
              actual.layer[layer_id].ref_buf1);
    EXPECT_EQ(expected_.layer[layer_id].ref_buf2,
              actual.layer[layer_id].ref_buf2);
    EXPECT_EQ(expected_.layer[layer_id].ref_buf3,
              actual.layer[layer_id].ref_buf3);
  }

  void EqualRefs(const Settings& actual) {
    for (unsigned int layer_id = 0; layer_id < kMaxVp9NumberOfSpatialLayers;
         ++layer_id) {
      EqualRefsForLayer(actual, layer_id);
    }
  }

  void EqualStartStopKeyframe(const Settings& actual) {
    EXPECT_EQ(expected_.start_layer, actual.start_layer);
    EXPECT_EQ(expected_.stop_layer, actual.stop_layer);
    EXPECT_EQ(expected_.is_keyframe, actual.is_keyframe);
  }

  // Check that the settings returned by GetSuperFrameSettings() is
  // equal to the expected_ settings.
  void EqualToExpected() {
    uint32_t frame_timestamp_ =
        clock_.TimeInMilliseconds() * (kTickFrequency / 1000);
    Settings actual =
        layers_->GetSuperFrameSettings(frame_timestamp_, expected_.is_keyframe);
    EqualRefs(actual);
    EqualStartStopKeyframe(actual);
  }

  Settings expected_;
  SimulatedClock clock_;
  std::unique_ptr<ScreenshareLayersVP9> layers_;
};

TEST_F(ScreenshareLayerTestVP9, NoRefsOnKeyFrame) {
  const int kNumLayers = kMaxVp9NumberOfSpatialLayers;
  InitScreenshareLayers(kNumLayers);
  expected_.start_layer = 0;
  expected_.stop_layer = kNumLayers - 1;

  for (int l = 0; l < kNumLayers; ++l) {
    expected_.layer[l].upd_buf = l;
  }
  expected_.is_keyframe = true;
  EqualToExpected();

  for (int l = 0; l < kNumLayers; ++l) {
    expected_.layer[l].ref_buf1 = l;
  }
  expected_.is_keyframe = false;
  EqualToExpected();
}

// Test if it is possible to send at a high bitrate (over the threshold)
// after a longer period of low bitrate. This should not be possible.
TEST_F(ScreenshareLayerTestVP9, DontAccumelateAvailableBitsOverTime) {
  InitScreenshareLayers(2);
  ConfigureBitrateForLayer(100, 0);

  expected_.layer[0].upd_buf = 0;
  expected_.layer[0].ref_buf1 = 0;
  expected_.layer[1].upd_buf = 1;
  expected_.layer[1].ref_buf1 = 1;
  expected_.start_layer = 0;
  expected_.stop_layer = 1;

  // Send 10 frames at a low bitrate (50 kbps)
  for (int i = 0; i < 10; ++i) {
    AdvanceTime(200);
    EqualToExpected();
    AddKilobitsToLayer(10, 0);
  }

  AdvanceTime(200);
  EqualToExpected();
  AddKilobitsToLayer(301, 0);

  // Send 10 frames at a high bitrate (200 kbps)
  expected_.start_layer = 1;
  for (int i = 0; i < 10; ++i) {
    AdvanceTime(200);
    EqualToExpected();
    AddKilobitsToLayer(40, 1);
  }
}

// Test if used bits are accumelated over layers, as they should;
TEST_F(ScreenshareLayerTestVP9, AccumelateUsedBitsOverLayers) {
  const int kNumLayers = kMaxVp9NumberOfSpatialLayers;
  InitScreenshareLayers(kNumLayers);
  for (int l = 0; l < kNumLayers - 1; ++l)
    ConfigureBitrateForLayer(100, l);
  for (int l = 0; l < kNumLayers; ++l) {
    expected_.layer[l].upd_buf = l;
    expected_.layer[l].ref_buf1 = l;
  }

  expected_.start_layer = 0;
  expected_.stop_layer = kNumLayers - 1;
  EqualToExpected();

  for (int layer = 0; layer < kNumLayers - 1; ++layer) {
    expected_.start_layer = layer;
    EqualToExpected();
    AddKilobitsToLayer(101, layer);
  }
}

// General testing of the bitrate controller.
TEST_F(ScreenshareLayerTestVP9, 2LayerBitrate) {
  InitScreenshareLayers(2);
  ConfigureBitrateForLayer(100, 0);

  expected_.layer[0].upd_buf = 0;
  expected_.layer[1].upd_buf = 1;
  expected_.layer[0].ref_buf1 = -1;
  expected_.layer[1].ref_buf1 = -1;
  expected_.start_layer = 0;
  expected_.stop_layer = 1;

  expected_.is_keyframe = true;
  EqualToExpected();
  AddKilobitsToLayer(100, 0);

  expected_.layer[0].ref_buf1 = 0;
  expected_.layer[1].ref_buf1 = 1;
  expected_.is_keyframe = false;
  AdvanceTime(199);
  EqualToExpected();
  AddKilobitsToLayer(100, 0);

  expected_.start_layer = 1;
  for (int frame = 0; frame < 3; ++frame) {
    AdvanceTime(200);
    EqualToExpected();
    AddKilobitsToLayer(100, 1);
  }

  // Just before enough bits become available for L0 @0.999 seconds.
  AdvanceTime(199);
  EqualToExpected();
  AddKilobitsToLayer(100, 1);

  // Just after enough bits become available for L0 @1.0001 seconds.
  expected_.start_layer = 0;
  AdvanceTime(2);
  EqualToExpected();
  AddKilobitsToLayer(100, 0);

  // Keyframes always encode all layers, even if it is over budget.
  expected_.layer[0].ref_buf1 = -1;
  expected_.layer[1].ref_buf1 = -1;
  expected_.is_keyframe = true;
  AdvanceTime(499);
  EqualToExpected();
  expected_.layer[0].ref_buf1 = 0;
  expected_.layer[1].ref_buf1 = 1;
  expected_.start_layer = 1;
  expected_.is_keyframe = false;
  EqualToExpected();
  AddKilobitsToLayer(100, 0);

  // 400 kb in L0 --> @3 second mark to fall below the threshold..
  // just before @2.999 seconds.
  expected_.is_keyframe = false;
  AdvanceTime(1499);
  EqualToExpected();
  AddKilobitsToLayer(100, 1);

  // just after @3.001 seconds.
  expected_.start_layer = 0;
  AdvanceTime(2);
  EqualToExpected();
  AddKilobitsToLayer(100, 0);
}

// General testing of the bitrate controller.
TEST_F(ScreenshareLayerTestVP9, 3LayerBitrate) {
  InitScreenshareLayers(3);
  ConfigureBitrateForLayer(100, 0);
  ConfigureBitrateForLayer(100, 1);

  for (int l = 0; l < 3; ++l) {
    expected_.layer[l].upd_buf = l;
    expected_.layer[l].ref_buf1 = l;
  }
  expected_.start_layer = 0;
  expected_.stop_layer = 2;

  EqualToExpected();
  AddKilobitsToLayer(105, 0);
  AddKilobitsToLayer(30, 1);

  AdvanceTime(199);
  EqualToExpected();
  AddKilobitsToLayer(105, 0);
  AddKilobitsToLayer(30, 1);

  expected_.start_layer = 1;
  AdvanceTime(200);
  EqualToExpected();
  AddKilobitsToLayer(130, 1);

  expected_.start_layer = 2;
  AdvanceTime(200);
  EqualToExpected();

  // 400 kb in L1 --> @1.0 second mark to fall below threshold.
  // 210 kb in L0 --> @1.1 second mark to fall below threshold.
  // Just before L1 @0.999 seconds.
  AdvanceTime(399);
  EqualToExpected();

  // Just after L1 @1.001 seconds.
  expected_.start_layer = 1;
  AdvanceTime(2);
  EqualToExpected();

  // Just before L0 @1.099 seconds.
  AdvanceTime(99);
  EqualToExpected();

  // Just after L0 @1.101 seconds.
  expected_.start_layer = 0;
  AdvanceTime(2);
  EqualToExpected();

  // @1.1 seconds
  AdvanceTime(99);
  EqualToExpected();
  AddKilobitsToLayer(200, 1);

  expected_.is_keyframe = true;
  for (int l = 0; l < 3; ++l)
    expected_.layer[l].ref_buf1 = -1;
  AdvanceTime(200);
  EqualToExpected();

  expected_.is_keyframe = false;
  expected_.start_layer = 2;
  for (int l = 0; l < 3; ++l)
    expected_.layer[l].ref_buf1 = l;
  AdvanceTime(200);
  EqualToExpected();
}

// Test that the bitrate calculations are
// correct when the timestamp wrap.
TEST_F(ScreenshareLayerTestVP9, TimestampWrap) {
  InitScreenshareLayers(2);
  ConfigureBitrateForLayer(100, 0);

  expected_.layer[0].upd_buf = 0;
  expected_.layer[0].ref_buf1 = 0;
  expected_.layer[1].upd_buf = 1;
  expected_.layer[1].ref_buf1 = 1;
  expected_.start_layer = 0;
  expected_.stop_layer = 1;

  // Advance time to just before the timestamp wraps.
  AdvanceTime(std::numeric_limits<uint32_t>::max() / (kTickFrequency / 1000));
  EqualToExpected();
  AddKilobitsToLayer(200, 0);

  // Wrap
  expected_.start_layer = 1;
  AdvanceTime(1);
  EqualToExpected();
}

}  // namespace webrtc
