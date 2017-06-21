/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_UNITTEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_UNITTEST_H_

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_video/include/video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/include/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Return;

namespace webrtc {
namespace testing {

const int kDefaultWidth = 1280;
const int kDefaultHeight = 720;
const int kNumberOfSimulcastStreams = 3;
const int kColorY = 66;
const int kColorU = 22;
const int kColorV = 33;
const int kMaxBitrates[kNumberOfSimulcastStreams] = {150, 600, 1200};
const int kMinBitrates[kNumberOfSimulcastStreams] = {50, 150, 600};
const int kTargetBitrates[kNumberOfSimulcastStreams] = {100, 450, 1000};
const int kDefaultTemporalLayerProfile[3] = {3, 3, 3};

template <typename T>
void SetExpectedValues3(T value0, T value1, T value2, T* expected_values) {
  expected_values[0] = value0;
  expected_values[1] = value1;
  expected_values[2] = value2;
}

enum PlaneType {
  kYPlane = 0,
  kUPlane = 1,
  kVPlane = 2,
  kNumOfPlanes = 3,
};

class Vp8TestEncodedImageCallback : public EncodedImageCallback {
 public:
  Vp8TestEncodedImageCallback() : picture_id_(-1) {
    memset(temporal_layer_, -1, sizeof(temporal_layer_));
    memset(layer_sync_, false, sizeof(layer_sync_));
  }

  ~Vp8TestEncodedImageCallback() {
    delete[] encoded_key_frame_._buffer;
    delete[] encoded_frame_._buffer;
  }

  virtual Result OnEncodedImage(const EncodedImage& encoded_image,
                                const CodecSpecificInfo* codec_specific_info,
                                const RTPFragmentationHeader* fragmentation) {
    // Only store the base layer.
    if (codec_specific_info->codecSpecific.VP8.simulcastIdx == 0) {
      if (encoded_image._frameType == kVideoFrameKey) {
        delete[] encoded_key_frame_._buffer;
        encoded_key_frame_._buffer = new uint8_t[encoded_image._size];
        encoded_key_frame_._size = encoded_image._size;
        encoded_key_frame_._length = encoded_image._length;
        encoded_key_frame_._frameType = kVideoFrameKey;
        encoded_key_frame_._completeFrame = encoded_image._completeFrame;
        memcpy(encoded_key_frame_._buffer, encoded_image._buffer,
               encoded_image._length);
      } else {
        delete[] encoded_frame_._buffer;
        encoded_frame_._buffer = new uint8_t[encoded_image._size];
        encoded_frame_._size = encoded_image._size;
        encoded_frame_._length = encoded_image._length;
        memcpy(encoded_frame_._buffer, encoded_image._buffer,
               encoded_image._length);
      }
    }
    picture_id_ = codec_specific_info->codecSpecific.VP8.pictureId;
    layer_sync_[codec_specific_info->codecSpecific.VP8.simulcastIdx] =
        codec_specific_info->codecSpecific.VP8.layerSync;
    temporal_layer_[codec_specific_info->codecSpecific.VP8.simulcastIdx] =
        codec_specific_info->codecSpecific.VP8.temporalIdx;
    return Result(Result::OK, encoded_image._timeStamp);
  }
  void GetLastEncodedFrameInfo(int* picture_id,
                               int* temporal_layer,
                               bool* layer_sync,
                               int stream) {
    *picture_id = picture_id_;
    *temporal_layer = temporal_layer_[stream];
    *layer_sync = layer_sync_[stream];
  }
  void GetLastEncodedKeyFrame(EncodedImage* encoded_key_frame) {
    *encoded_key_frame = encoded_key_frame_;
  }
  void GetLastEncodedFrame(EncodedImage* encoded_frame) {
    *encoded_frame = encoded_frame_;
  }

 private:
  EncodedImage encoded_key_frame_;
  EncodedImage encoded_frame_;
  int picture_id_;
  int temporal_layer_[kNumberOfSimulcastStreams];
  bool layer_sync_[kNumberOfSimulcastStreams];
};

class Vp8TestDecodedImageCallback : public DecodedImageCallback {
 public:
  Vp8TestDecodedImageCallback() : decoded_frames_(0) {}
  int32_t Decoded(VideoFrame& decoded_image) override {
    rtc::scoped_refptr<I420BufferInterface> i420_buffer =
        decoded_image.video_frame_buffer()->ToI420();
    for (int i = 0; i < decoded_image.width(); ++i) {
      EXPECT_NEAR(kColorY, i420_buffer->DataY()[i], 1);
    }

    // TODO(mikhal): Verify the difference between U,V and the original.
    for (int i = 0; i < i420_buffer->ChromaWidth(); ++i) {
      EXPECT_NEAR(kColorU, i420_buffer->DataU()[i], 4);
      EXPECT_NEAR(kColorV, i420_buffer->DataV()[i], 4);
    }
    decoded_frames_++;
    return 0;
  }
  int32_t Decoded(VideoFrame& decoded_image, int64_t decode_time_ms) override {
    RTC_NOTREACHED();
    return -1;
  }
  void Decoded(VideoFrame& decoded_image,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp) override {
    Decoded(decoded_image);
  }
  int DecodedFrames() { return decoded_frames_; }

 private:
  int decoded_frames_;
};

class TestVp8Simulcast : public ::testing::Test {
 public:
  TestVp8Simulcast(VP8Encoder* encoder, VP8Decoder* decoder)
      : encoder_(encoder), decoder_(decoder) {}

  static void SetPlane(uint8_t* data,
                       uint8_t value,
                       int width,
                       int height,
                       int stride) {
    for (int i = 0; i < height; i++, data += stride) {
      // Setting allocated area to zero - setting only image size to
      // requested values - will make it easier to distinguish between image
      // size and frame size (accounting for stride).
      memset(data, value, width);
      memset(data + width, 0, stride - width);
    }
  }

  // Fills in an I420Buffer from |plane_colors|.
  static void CreateImage(const rtc::scoped_refptr<I420Buffer>& buffer,
                          int plane_colors[kNumOfPlanes]) {
    SetPlane(buffer->MutableDataY(), plane_colors[0], buffer->width(),
             buffer->height(), buffer->StrideY());

    SetPlane(buffer->MutableDataU(), plane_colors[1], buffer->ChromaWidth(),
             buffer->ChromaHeight(), buffer->StrideU());

    SetPlane(buffer->MutableDataV(), plane_colors[2], buffer->ChromaWidth(),
             buffer->ChromaHeight(), buffer->StrideV());
  }

  static void DefaultSettings(VideoCodec* settings,
                              const int* temporal_layer_profile) {
    RTC_CHECK(settings);
    memset(settings, 0, sizeof(VideoCodec));
    strncpy(settings->plName, "VP8", 4);
    settings->codecType = kVideoCodecVP8;
    // 96 to 127 dynamic payload types for video codecs
    settings->plType = 120;
    settings->startBitrate = 300;
    settings->minBitrate = 30;
    settings->maxBitrate = 0;
    settings->maxFramerate = 30;
    settings->width = kDefaultWidth;
    settings->height = kDefaultHeight;
    settings->numberOfSimulcastStreams = kNumberOfSimulcastStreams;
    ASSERT_EQ(3, kNumberOfSimulcastStreams);
    settings->timing_frame_thresholds = {kDefaultTimingFramesDelayMs,
                                         kDefaultOutlierFrameSizePercent};
    ConfigureStream(kDefaultWidth / 4, kDefaultHeight / 4, kMaxBitrates[0],
                    kMinBitrates[0], kTargetBitrates[0],
                    &settings->simulcastStream[0], temporal_layer_profile[0]);
    ConfigureStream(kDefaultWidth / 2, kDefaultHeight / 2, kMaxBitrates[1],
                    kMinBitrates[1], kTargetBitrates[1],
                    &settings->simulcastStream[1], temporal_layer_profile[1]);
    ConfigureStream(kDefaultWidth, kDefaultHeight, kMaxBitrates[2],
                    kMinBitrates[2], kTargetBitrates[2],
                    &settings->simulcastStream[2], temporal_layer_profile[2]);
    settings->VP8()->resilience = kResilientStream;
    settings->VP8()->denoisingOn = true;
    settings->VP8()->errorConcealmentOn = false;
    settings->VP8()->automaticResizeOn = false;
    settings->VP8()->frameDroppingOn = true;
    settings->VP8()->keyFrameInterval = 3000;
  }

  static void ConfigureStream(int width,
                              int height,
                              int max_bitrate,
                              int min_bitrate,
                              int target_bitrate,
                              SimulcastStream* stream,
                              int num_temporal_layers) {
    assert(stream);
    stream->width = width;
    stream->height = height;
    stream->maxBitrate = max_bitrate;
    stream->minBitrate = min_bitrate;
    stream->targetBitrate = target_bitrate;
    stream->numberOfTemporalLayers = num_temporal_layers;
    stream->qpMax = 45;
  }

 protected:
  void SetUp() override { SetUpCodec(kDefaultTemporalLayerProfile); }

  void TearDown() override {
    encoder_->Release();
    decoder_->Release();
  }

  void SetUpCodec(const int* temporal_layer_profile) {
    encoder_->RegisterEncodeCompleteCallback(&encoder_callback_);
    decoder_->RegisterDecodeCompleteCallback(&decoder_callback_);
    DefaultSettings(&settings_, temporal_layer_profile);
    SetUpRateAllocator();
    EXPECT_EQ(0, encoder_->InitEncode(&settings_, 1, 1200));
    EXPECT_EQ(0, decoder_->InitDecode(&settings_, 1));
    input_buffer_ = I420Buffer::Create(kDefaultWidth, kDefaultHeight);
    input_buffer_->InitializeData();
    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));
  }

  void SetUpRateAllocator() {
    TemporalLayersFactory* tl_factory = new TemporalLayersFactory();
    rate_allocator_.reset(new SimulcastRateAllocator(
        settings_, std::unique_ptr<TemporalLayersFactory>(tl_factory)));
    settings_.VP8()->tl_factory = tl_factory;
  }

  void SetRates(uint32_t bitrate_kbps, uint32_t fps) {
    encoder_->SetRateAllocation(
        rate_allocator_->GetAllocation(bitrate_kbps * 1000, fps), fps);
  }

  void ExpectStreams(FrameType frame_type, int expected_video_streams) {
    ASSERT_GE(expected_video_streams, 0);
    ASSERT_LE(expected_video_streams, kNumberOfSimulcastStreams);
    if (expected_video_streams >= 1) {
      EXPECT_CALL(
          encoder_callback_,
          OnEncodedImage(
              AllOf(Field(&EncodedImage::_frameType, frame_type),
                    Field(&EncodedImage::_encodedWidth, kDefaultWidth / 4),
                    Field(&EncodedImage::_encodedHeight, kDefaultHeight / 4)),
              _, _))
          .Times(1)
          .WillRepeatedly(Return(EncodedImageCallback::Result(
              EncodedImageCallback::Result::OK, 0)));
    }
    if (expected_video_streams >= 2) {
      EXPECT_CALL(
          encoder_callback_,
          OnEncodedImage(
              AllOf(Field(&EncodedImage::_frameType, frame_type),
                    Field(&EncodedImage::_encodedWidth, kDefaultWidth / 2),
                    Field(&EncodedImage::_encodedHeight, kDefaultHeight / 2)),
              _, _))
          .Times(1)
          .WillRepeatedly(Return(EncodedImageCallback::Result(
              EncodedImageCallback::Result::OK, 0)));
    }
    if (expected_video_streams >= 3) {
      EXPECT_CALL(
          encoder_callback_,
          OnEncodedImage(
              AllOf(Field(&EncodedImage::_frameType, frame_type),
                    Field(&EncodedImage::_encodedWidth, kDefaultWidth),
                    Field(&EncodedImage::_encodedHeight, kDefaultHeight)),
              _, _))
          .Times(1)
          .WillRepeatedly(Return(EncodedImageCallback::Result(
              EncodedImageCallback::Result::OK, 0)));
    }
  }

  void VerifyTemporalIdxAndSyncForAllSpatialLayers(
      Vp8TestEncodedImageCallback* encoder_callback,
      const int* expected_temporal_idx,
      const bool* expected_layer_sync,
      int num_spatial_layers) {
    int picture_id = -1;
    int temporal_layer = -1;
    bool layer_sync = false;
    for (int i = 0; i < num_spatial_layers; i++) {
      encoder_callback->GetLastEncodedFrameInfo(&picture_id, &temporal_layer,
                                                &layer_sync, i);
      EXPECT_EQ(expected_temporal_idx[i], temporal_layer);
      EXPECT_EQ(expected_layer_sync[i], layer_sync);
    }
  }

  // We currently expect all active streams to generate a key frame even though
  // a key frame was only requested for some of them.
  void TestKeyFrameRequestsOnAllStreams() {
    SetRates(kMaxBitrates[2], 30);  // To get all three streams.
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    frame_types[0] = kVideoFrameKey;
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    std::fill(frame_types.begin(), frame_types.end(), kVideoFrameDelta);
    frame_types[1] = kVideoFrameKey;
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    std::fill(frame_types.begin(), frame_types.end(), kVideoFrameDelta);
    frame_types[2] = kVideoFrameKey;
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    std::fill(frame_types.begin(), frame_types.end(), kVideoFrameDelta);
    ExpectStreams(kVideoFrameDelta, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingAllStreams() {
    // We should always encode the base layer.
    SetRates(kMinBitrates[0] - 1, 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 1);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingTwoStreams() {
    // We have just enough to get only the first stream and padding for two.
    SetRates(kMinBitrates[0], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 1);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingTwoStreamsOneMaxedOut() {
    // We are just below limit of sending second stream, so we should get
    // the first stream maxed out (at |maxBitrate|), and padding for two.
    SetRates(kTargetBitrates[0] + kMinBitrates[1] - 1, 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 1);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingOneStream() {
    // We have just enough to send two streams, so padding for one stream.
    SetRates(kTargetBitrates[0] + kMinBitrates[1], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 2);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingOneStreamTwoMaxedOut() {
    // We are just below limit of sending third stream, so we should get
    // first stream's rate maxed out at |targetBitrate|, second at |maxBitrate|.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2] - 1, 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 2);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestSendAllStreams() {
    // We have just enough to send all streams.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 3);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 3);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestDisablingStreams() {
    // We should get three media streams.
    SetRates(kMaxBitrates[0] + kMaxBitrates[1] + kMaxBitrates[2], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 3);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 3);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should only get two streams and padding for one.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2] / 2, 30);
    ExpectStreams(kVideoFrameDelta, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should only get the first stream and padding for two.
    SetRates(kTargetBitrates[0] + kMinBitrates[1] / 2, 30);
    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We don't have enough bitrate for the thumbnail stream, but we should get
    // it anyway with current configuration.
    SetRates(kTargetBitrates[0] - 1, 30);
    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should only get two streams and padding for one.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2] / 2, 30);
    // We get a key frame because a new stream is being enabled.
    ExpectStreams(kVideoFrameKey, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should get all three streams.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kTargetBitrates[2], 30);
    // We get a key frame because a new stream is being enabled.
    ExpectStreams(kVideoFrameKey, 3);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void SwitchingToOneStream(int width, int height) {
    // Disable all streams except the last and set the bitrate of the last to
    // 100 kbps. This verifies the way GTP switches to screenshare mode.
    settings_.VP8()->numberOfTemporalLayers = 1;
    settings_.maxBitrate = 100;
    settings_.startBitrate = 100;
    settings_.width = width;
    settings_.height = height;
    for (int i = 0; i < settings_.numberOfSimulcastStreams - 1; ++i) {
      settings_.simulcastStream[i].maxBitrate = 0;
      settings_.simulcastStream[i].width = settings_.width;
      settings_.simulcastStream[i].height = settings_.height;
    }
    // Setting input image to new resolution.
    input_buffer_ = I420Buffer::Create(settings_.width, settings_.height);
    input_buffer_->InitializeData();

    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));

    // The for loop above did not set the bitrate of the highest layer.
    settings_.simulcastStream[settings_.numberOfSimulcastStreams - 1]
        .maxBitrate = 0;
    // The highest layer has to correspond to the non-simulcast resolution.
    settings_.simulcastStream[settings_.numberOfSimulcastStreams - 1].width =
        settings_.width;
    settings_.simulcastStream[settings_.numberOfSimulcastStreams - 1].height =
        settings_.height;
    SetUpRateAllocator();
    EXPECT_EQ(0, encoder_->InitEncode(&settings_, 1, 1200));

    // Encode one frame and verify.
    SetRates(kMaxBitrates[0] + kMaxBitrates[1], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    EXPECT_CALL(
        encoder_callback_,
        OnEncodedImage(AllOf(Field(&EncodedImage::_frameType, kVideoFrameKey),
                             Field(&EncodedImage::_encodedWidth, width),
                             Field(&EncodedImage::_encodedHeight, height)),
                       _, _))
        .Times(1)
        .WillRepeatedly(Return(
            EncodedImageCallback::Result(EncodedImageCallback::Result::OK, 0)));
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // Switch back.
    DefaultSettings(&settings_, kDefaultTemporalLayerProfile);
    // Start at the lowest bitrate for enabling base stream.
    settings_.startBitrate = kMinBitrates[0];
    SetUpRateAllocator();
    EXPECT_EQ(0, encoder_->InitEncode(&settings_, 1, 1200));
    SetRates(settings_.startBitrate, 30);
    ExpectStreams(kVideoFrameKey, 1);
    // Resize |input_frame_| to the new resolution.
    input_buffer_ = I420Buffer::Create(settings_.width, settings_.height);
    input_buffer_->InitializeData();
    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestSwitchingToOneStream() { SwitchingToOneStream(1024, 768); }

  void TestSwitchingToOneOddStream() { SwitchingToOneStream(1023, 769); }

  void TestSwitchingToOneSmallStream() { SwitchingToOneStream(4, 4); }

  // Test the layer pattern and sync flag for various spatial-temporal patterns.
  // 3-3-3 pattern: 3 temporal layers for all spatial streams, so same
  // temporal_layer id and layer_sync is expected for all streams.
  void TestSaptioTemporalLayers333PatternEncoder() {
    Vp8TestEncodedImageCallback encoder_callback;
    encoder_->RegisterEncodeCompleteCallback(&encoder_callback);
    SetRates(kMaxBitrates[2], 30);  // To get all three streams.

    int expected_temporal_idx[3] = {-1, -1, -1};
    bool expected_layer_sync[3] = {false, false, false};

    // First frame: #0.
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(0, 0, 0, expected_temporal_idx);
    SetExpectedValues3<bool>(true, true, true, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #1.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(2, 2, 2, expected_temporal_idx);
    SetExpectedValues3<bool>(true, true, true, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #2.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(1, 1, 1, expected_temporal_idx);
    SetExpectedValues3<bool>(true, true, true, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #3.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(2, 2, 2, expected_temporal_idx);
    SetExpectedValues3<bool>(false, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #4.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(0, 0, 0, expected_temporal_idx);
    SetExpectedValues3<bool>(false, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #5.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(2, 2, 2, expected_temporal_idx);
    SetExpectedValues3<bool>(false, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);
  }

  // Test the layer pattern and sync flag for various spatial-temporal patterns.
  // 3-2-1 pattern: 3 temporal layers for lowest resolution, 2 for middle, and
  // 1 temporal layer for highest resolution.
  // For this profile, we expect the temporal index pattern to be:
  // 1st stream: 0, 2, 1, 2, ....
  // 2nd stream: 0, 1, 0, 1, ...
  // 3rd stream: -1, -1, -1, -1, ....
  // Regarding the 3rd stream, note that a stream/encoder with 1 temporal layer
  // should always have temporal layer idx set to kNoTemporalIdx = -1.
  // Since CodecSpecificInfoVP8.temporalIdx is uint8_t, this will wrap to 255.
  // TODO(marpan): Although this seems safe for now, we should fix this.
  void TestSpatioTemporalLayers321PatternEncoder() {
    int temporal_layer_profile[3] = {3, 2, 1};
    SetUpCodec(temporal_layer_profile);
    Vp8TestEncodedImageCallback encoder_callback;
    encoder_->RegisterEncodeCompleteCallback(&encoder_callback);
    SetRates(kMaxBitrates[2], 30);  // To get all three streams.

    int expected_temporal_idx[3] = {-1, -1, -1};
    bool expected_layer_sync[3] = {false, false, false};

    // First frame: #0.
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(0, 0, 255, expected_temporal_idx);
    SetExpectedValues3<bool>(true, true, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #1.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(2, 1, 255, expected_temporal_idx);
    SetExpectedValues3<bool>(true, true, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #2.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(1, 0, 255, expected_temporal_idx);
    SetExpectedValues3<bool>(true, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #3.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(2, 1, 255, expected_temporal_idx);
    SetExpectedValues3<bool>(false, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #4.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(0, 0, 255, expected_temporal_idx);
    SetExpectedValues3<bool>(false, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);

    // Next frame: #5.
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));
    SetExpectedValues3<int>(2, 1, 255, expected_temporal_idx);
    SetExpectedValues3<bool>(false, false, false, expected_layer_sync);
    VerifyTemporalIdxAndSyncForAllSpatialLayers(
        &encoder_callback, expected_temporal_idx, expected_layer_sync, 3);
  }

  void TestStrideEncodeDecode() {
    Vp8TestEncodedImageCallback encoder_callback;
    Vp8TestDecodedImageCallback decoder_callback;
    encoder_->RegisterEncodeCompleteCallback(&encoder_callback);
    decoder_->RegisterDecodeCompleteCallback(&decoder_callback);

    SetRates(kMaxBitrates[2], 30);  // To get all three streams.
    // Setting two (possibly) problematic use cases for stride:
    // 1. stride > width 2. stride_y != stride_uv/2
    int stride_y = kDefaultWidth + 20;
    int stride_uv = ((kDefaultWidth + 1) / 2) + 5;
    input_buffer_ = I420Buffer::Create(kDefaultWidth, kDefaultHeight, stride_y,
                                       stride_uv, stride_uv);
    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));

    // Set color.
    int plane_offset[kNumOfPlanes];
    plane_offset[kYPlane] = kColorY;
    plane_offset[kUPlane] = kColorU;
    plane_offset[kVPlane] = kColorV;
    CreateImage(input_buffer_, plane_offset);

    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));

    // Change color.
    plane_offset[kYPlane] += 1;
    plane_offset[kUPlane] += 1;
    plane_offset[kVPlane] += 1;
    CreateImage(input_buffer_, plane_offset);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));

    EncodedImage encoded_frame;
    // Only encoding one frame - so will be a key frame.
    encoder_callback.GetLastEncodedKeyFrame(&encoded_frame);
    EXPECT_EQ(0, decoder_->Decode(encoded_frame, false, NULL));
    encoder_callback.GetLastEncodedFrame(&encoded_frame);
    decoder_->Decode(encoded_frame, false, NULL);
    EXPECT_EQ(2, decoder_callback.DecodedFrames());
  }

  std::unique_ptr<VP8Encoder> encoder_;
  MockEncodedImageCallback encoder_callback_;
  std::unique_ptr<VP8Decoder> decoder_;
  MockDecodedImageCallback decoder_callback_;
  VideoCodec settings_;
  rtc::scoped_refptr<I420Buffer> input_buffer_;
  std::unique_ptr<VideoFrame> input_frame_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_UNITTEST_H_
