/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <memory>
#include <vector>

#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_unittest.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace testing {

class TestSimulcastEncoderAdapter : public TestVp8Simulcast {
 public:
  TestSimulcastEncoderAdapter()
      : TestVp8Simulcast(new SimulcastEncoderAdapter(new Vp8EncoderFactory()),
                         VP8Decoder::Create()) {}

 protected:
  class Vp8EncoderFactory : public VideoEncoderFactory {
   public:
    VideoEncoder* Create() override { return VP8Encoder::Create(); }

    void Destroy(VideoEncoder* encoder) override { delete encoder; }

    virtual ~Vp8EncoderFactory() {}
  };

  virtual void SetUp() { TestVp8Simulcast::SetUp(); }
  virtual void TearDown() { TestVp8Simulcast::TearDown(); }
};

TEST_F(TestSimulcastEncoderAdapter, TestKeyFrameRequestsOnAllStreams) {
  TestVp8Simulcast::TestKeyFrameRequestsOnAllStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingAllStreams) {
  TestVp8Simulcast::TestPaddingAllStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingTwoStreams) {
  TestVp8Simulcast::TestPaddingTwoStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingTwoStreamsOneMaxedOut) {
  TestVp8Simulcast::TestPaddingTwoStreamsOneMaxedOut();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingOneStream) {
  TestVp8Simulcast::TestPaddingOneStream();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingOneStreamTwoMaxedOut) {
  TestVp8Simulcast::TestPaddingOneStreamTwoMaxedOut();
}

TEST_F(TestSimulcastEncoderAdapter, TestSendAllStreams) {
  TestVp8Simulcast::TestSendAllStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestDisablingStreams) {
  TestVp8Simulcast::TestDisablingStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestSwitchingToOneStream) {
  TestVp8Simulcast::TestSwitchingToOneStream();
}

TEST_F(TestSimulcastEncoderAdapter, TestSwitchingToOneOddStream) {
  TestVp8Simulcast::TestSwitchingToOneOddStream();
}

TEST_F(TestSimulcastEncoderAdapter, TestStrideEncodeDecode) {
  TestVp8Simulcast::TestStrideEncodeDecode();
}

TEST_F(TestSimulcastEncoderAdapter, TestSaptioTemporalLayers333PatternEncoder) {
  TestVp8Simulcast::TestSaptioTemporalLayers333PatternEncoder();
}

TEST_F(TestSimulcastEncoderAdapter, TestSpatioTemporalLayers321PatternEncoder) {
  TestVp8Simulcast::TestSpatioTemporalLayers321PatternEncoder();
}

class MockVideoEncoder : public VideoEncoder {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  int32_t InitEncode(const VideoCodec* codecSettings,
                     int32_t numberOfCores,
                     size_t maxPayloadSize) /* override */ {
    codec_ = *codecSettings;
    return init_encode_return_value_;
  }

  MOCK_METHOD3(
      Encode,
      int32_t(const VideoFrame& inputImage,
              const CodecSpecificInfo* codecSpecificInfo,
              const std::vector<FrameType>* frame_types) /* override */);

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) /* override */ {
    callback_ = callback;
    return 0;
  }

  MOCK_METHOD0(Release, int32_t());

  int32_t SetRateAllocation(const BitrateAllocation& bitrate_allocation,
                            uint32_t framerate) {
    last_set_bitrate_ = bitrate_allocation;
    return 0;
  }

  MOCK_METHOD2(SetChannelParameters, int32_t(uint32_t packetLoss, int64_t rtt));

  bool SupportsNativeHandle() const /* override */ {
    return supports_native_handle_;
  }

  virtual ~MockVideoEncoder() {}

  const VideoCodec& codec() const { return codec_; }

  void SendEncodedImage(int width, int height) {
    // Sends a fake image of the given width/height.
    EncodedImage image;
    image._encodedWidth = width;
    image._encodedHeight = height;
    CodecSpecificInfo codec_specific_info;
    memset(&codec_specific_info, 0, sizeof(codec_specific_info));
    callback_->OnEncodedImage(image, &codec_specific_info, nullptr);
  }

  void set_supports_native_handle(bool enabled) {
    supports_native_handle_ = enabled;
  }

  void set_init_encode_return_value(int32_t value) {
    init_encode_return_value_ = value;
  }

  BitrateAllocation last_set_bitrate() const { return last_set_bitrate_; }

  MOCK_CONST_METHOD0(ImplementationName, const char*());

 private:
  bool supports_native_handle_ = false;
  int32_t init_encode_return_value_ = 0;
  BitrateAllocation last_set_bitrate_;

  VideoCodec codec_;
  EncodedImageCallback* callback_;
};

class MockVideoEncoderFactory : public VideoEncoderFactory {
 public:
  VideoEncoder* Create() override {
    MockVideoEncoder* encoder = new
        ::testing::NiceMock<MockVideoEncoder>();
    encoder->set_init_encode_return_value(init_encode_return_value_);
    const char* encoder_name = encoder_names_.empty()
                                   ? "codec_implementation_name"
                                   : encoder_names_[encoders_.size()];
    ON_CALL(*encoder, ImplementationName()).WillByDefault(Return(encoder_name));
    encoders_.push_back(encoder);
    return encoder;
  }

  void Destroy(VideoEncoder* encoder) override {
    for (size_t i = 0; i < encoders_.size(); ++i) {
      if (encoders_[i] == encoder) {
        encoders_.erase(encoders_.begin() + i);
        break;
      }
    }
    delete encoder;
  }

  virtual ~MockVideoEncoderFactory() {}

  const std::vector<MockVideoEncoder*>& encoders() const { return encoders_; }
  void SetEncoderNames(const std::vector<const char*>& encoder_names) {
    encoder_names_ = encoder_names;
  }
  void set_init_encode_return_value(int32_t value) {
    init_encode_return_value_ = value;
  }

 private:
  int32_t init_encode_return_value_ = 0;
  std::vector<MockVideoEncoder*> encoders_;
  std::vector<const char*> encoder_names_;
};

class TestSimulcastEncoderAdapterFakeHelper {
 public:
  TestSimulcastEncoderAdapterFakeHelper()
      : factory_(new MockVideoEncoderFactory()) {}

  // Can only be called once as the SimulcastEncoderAdapter will take the
  // ownership of |factory_|.
  VP8Encoder* CreateMockEncoderAdapter() {
    return new SimulcastEncoderAdapter(factory_);
  }

  void ExpectCallSetChannelParameters(uint32_t packetLoss, int64_t rtt) {
    EXPECT_TRUE(!factory_->encoders().empty());
    for (size_t i = 0; i < factory_->encoders().size(); ++i) {
      EXPECT_CALL(*factory_->encoders()[i],
                  SetChannelParameters(packetLoss, rtt))
          .Times(1);
    }
  }

  MockVideoEncoderFactory* factory() { return factory_; }

 private:
  MockVideoEncoderFactory* factory_;
};

static const int kTestTemporalLayerProfile[3] = {3, 2, 1};

class TestSimulcastEncoderAdapterFake : public ::testing::Test,
                                        public EncodedImageCallback {
 public:
  TestSimulcastEncoderAdapterFake()
      : helper_(new TestSimulcastEncoderAdapterFakeHelper()),
        adapter_(helper_->CreateMockEncoderAdapter()),
        last_encoded_image_width_(-1),
        last_encoded_image_height_(-1),
        last_encoded_image_simulcast_index_(-1) {}
  virtual ~TestSimulcastEncoderAdapterFake() {
    if (adapter_) {
      adapter_->Release();
    }
  }

  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    last_encoded_image_width_ = encoded_image._encodedWidth;
    last_encoded_image_height_ = encoded_image._encodedHeight;
    if (codec_specific_info) {
      last_encoded_image_simulcast_index_ =
          codec_specific_info->codecSpecific.VP8.simulcastIdx;
    }
    return Result(Result::OK, encoded_image._timeStamp);
  }

  bool GetLastEncodedImageInfo(int* out_width,
                               int* out_height,
                               int* out_simulcast_index) {
    if (last_encoded_image_width_ == -1) {
      return false;
    }
    *out_width = last_encoded_image_width_;
    *out_height = last_encoded_image_height_;
    *out_simulcast_index = last_encoded_image_simulcast_index_;
    return true;
  }

  void SetupCodec() {
    TestVp8Simulcast::DefaultSettings(
        &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
    rate_allocator_.reset(new SimulcastRateAllocator(codec_, nullptr));
    tl_factory_.SetListener(rate_allocator_.get());
    codec_.VP8()->tl_factory = &tl_factory_;
    EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
    adapter_->RegisterEncodeCompleteCallback(this);
  }

  void VerifyCodec(const VideoCodec& ref, int stream_index) {
    const VideoCodec& target =
        helper_->factory()->encoders()[stream_index]->codec();
    EXPECT_EQ(ref.codecType, target.codecType);
    EXPECT_EQ(0, strcmp(ref.plName, target.plName));
    EXPECT_EQ(ref.plType, target.plType);
    EXPECT_EQ(ref.width, target.width);
    EXPECT_EQ(ref.height, target.height);
    EXPECT_EQ(ref.startBitrate, target.startBitrate);
    EXPECT_EQ(ref.maxBitrate, target.maxBitrate);
    EXPECT_EQ(ref.minBitrate, target.minBitrate);
    EXPECT_EQ(ref.maxFramerate, target.maxFramerate);
    EXPECT_EQ(ref.VP8().pictureLossIndicationOn,
              target.VP8().pictureLossIndicationOn);
    EXPECT_EQ(ref.VP8().complexity, target.VP8().complexity);
    EXPECT_EQ(ref.VP8().resilience, target.VP8().resilience);
    EXPECT_EQ(ref.VP8().numberOfTemporalLayers,
              target.VP8().numberOfTemporalLayers);
    EXPECT_EQ(ref.VP8().denoisingOn, target.VP8().denoisingOn);
    EXPECT_EQ(ref.VP8().errorConcealmentOn, target.VP8().errorConcealmentOn);
    EXPECT_EQ(ref.VP8().automaticResizeOn, target.VP8().automaticResizeOn);
    EXPECT_EQ(ref.VP8().frameDroppingOn, target.VP8().frameDroppingOn);
    EXPECT_EQ(ref.VP8().keyFrameInterval, target.VP8().keyFrameInterval);
    EXPECT_EQ(ref.qpMax, target.qpMax);
    EXPECT_EQ(0, target.numberOfSimulcastStreams);
    EXPECT_EQ(ref.mode, target.mode);

    // No need to compare simulcastStream as numberOfSimulcastStreams should
    // always be 0.
  }

  void InitRefCodec(int stream_index, VideoCodec* ref_codec) {
    *ref_codec = codec_;
    ref_codec->VP8()->numberOfTemporalLayers =
        kTestTemporalLayerProfile[stream_index];
    ref_codec->VP8()->tl_factory = &tl_factory_;
    ref_codec->width = codec_.simulcastStream[stream_index].width;
    ref_codec->height = codec_.simulcastStream[stream_index].height;
    ref_codec->maxBitrate = codec_.simulcastStream[stream_index].maxBitrate;
    ref_codec->minBitrate = codec_.simulcastStream[stream_index].minBitrate;
    ref_codec->qpMax = codec_.simulcastStream[stream_index].qpMax;
  }

  void VerifyCodecSettings() {
    EXPECT_EQ(3u, helper_->factory()->encoders().size());
    VideoCodec ref_codec;

    // stream 0, the lowest resolution stream.
    InitRefCodec(0, &ref_codec);
    ref_codec.qpMax = 45;
    ref_codec.VP8()->complexity = webrtc::kComplexityHigher;
    ref_codec.VP8()->denoisingOn = false;
    ref_codec.startBitrate = 100;  // Should equal to the target bitrate.
    VerifyCodec(ref_codec, 0);

    // stream 1
    InitRefCodec(1, &ref_codec);
    ref_codec.VP8()->denoisingOn = false;
    // The start bitrate (300kbit) minus what we have for the lower layers
    // (100kbit).
    ref_codec.startBitrate = 200;
    VerifyCodec(ref_codec, 1);

    // stream 2, the biggest resolution stream.
    InitRefCodec(2, &ref_codec);
    // We don't have enough bits to send this, so the adapter should have
    // configured it to use the min bitrate for this layer (600kbit) but turn
    // off sending.
    ref_codec.startBitrate = 600;
    VerifyCodec(ref_codec, 2);
  }

 protected:
  std::unique_ptr<TestSimulcastEncoderAdapterFakeHelper> helper_;
  std::unique_ptr<VP8Encoder> adapter_;
  VideoCodec codec_;
  int last_encoded_image_width_;
  int last_encoded_image_height_;
  int last_encoded_image_simulcast_index_;
  TemporalLayersFactory tl_factory_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

TEST_F(TestSimulcastEncoderAdapterFake, InitEncode) {
  SetupCodec();
  VerifyCodecSettings();
}

TEST_F(TestSimulcastEncoderAdapterFake, ReleaseWithoutInitEncode) {
  EXPECT_EQ(0, adapter_->Release());
}

TEST_F(TestSimulcastEncoderAdapterFake, Reinit) {
  SetupCodec();
  EXPECT_EQ(0, adapter_->Release());

  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
}

TEST_F(TestSimulcastEncoderAdapterFake, SetChannelParameters) {
  SetupCodec();
  const uint32_t packetLoss = 5;
  const int64_t rtt = 30;
  helper_->ExpectCallSetChannelParameters(packetLoss, rtt);
  adapter_->SetChannelParameters(packetLoss, rtt);
}

TEST_F(TestSimulcastEncoderAdapterFake, EncodedCallbackForDifferentEncoders) {
  SetupCodec();

  // Set bitrates so that we send all layers.
  adapter_->SetRateAllocation(rate_allocator_->GetAllocation(1200, 30), 30);

  // At this point, the simulcast encoder adapter should have 3 streams: HD,
  // quarter HD, and quarter quarter HD. We're going to mostly ignore the exact
  // resolutions, to test that the adapter forwards on the correct resolution
  // and simulcast index values, going only off the encoder that generates the
  // image.
  std::vector<MockVideoEncoder*> encoders = helper_->factory()->encoders();
  ASSERT_EQ(3u, encoders.size());
  encoders[0]->SendEncodedImage(1152, 704);
  int width;
  int height;
  int simulcast_index;
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(1152, width);
  EXPECT_EQ(704, height);
  EXPECT_EQ(0, simulcast_index);

  encoders[1]->SendEncodedImage(300, 620);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(300, width);
  EXPECT_EQ(620, height);
  EXPECT_EQ(1, simulcast_index);

  encoders[2]->SendEncodedImage(120, 240);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(120, width);
  EXPECT_EQ(240, height);
  EXPECT_EQ(2, simulcast_index);
}

// This test verifies that the underlying encoders are reused, when the adapter
// is reinited with different number of simulcast streams. It further checks
// that the allocated encoders are reused in the same order as before, starting
// with the lowest stream.
TEST_F(TestSimulcastEncoderAdapterFake, ReusesEncodersInOrder) {
  // Set up common settings for three streams.
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  rate_allocator_.reset(new SimulcastRateAllocator(codec_, nullptr));
  tl_factory_.SetListener(rate_allocator_.get());
  codec_.VP8()->tl_factory = &tl_factory_;
  adapter_->RegisterEncodeCompleteCallback(this);

  // Input data.
  rtc::scoped_refptr<VideoFrameBuffer> buffer(I420Buffer::Create(1280, 720));
  VideoFrame input_frame(buffer, 100, 1000, kVideoRotation_180);
  std::vector<FrameType> frame_types;

  // Encode with three streams.
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  VerifyCodecSettings();
  std::vector<MockVideoEncoder*> original_encoders =
      helper_->factory()->encoders();
  ASSERT_EQ(3u, original_encoders.size());
  EXPECT_CALL(*original_encoders[0], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*original_encoders[1], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*original_encoders[2], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  frame_types.resize(3, kVideoFrameKey);
  EXPECT_EQ(0, adapter_->Encode(input_frame, nullptr, &frame_types));
  EXPECT_CALL(*original_encoders[0], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*original_encoders[1], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*original_encoders[2], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_EQ(0, adapter_->Release());

  // Encode with two streams.
  codec_.width /= 2;
  codec_.height /= 2;
  codec_.numberOfSimulcastStreams = 2;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  std::vector<MockVideoEncoder*> new_encoders = helper_->factory()->encoders();
  ASSERT_EQ(2u, new_encoders.size());
  ASSERT_EQ(original_encoders[0], new_encoders[0]);
  EXPECT_CALL(*original_encoders[0], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  ASSERT_EQ(original_encoders[1], new_encoders[1]);
  EXPECT_CALL(*original_encoders[1], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  frame_types.resize(2, kVideoFrameKey);
  EXPECT_EQ(0, adapter_->Encode(input_frame, nullptr, &frame_types));
  EXPECT_CALL(*original_encoders[0], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*original_encoders[1], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_EQ(0, adapter_->Release());

  // Encode with single stream.
  codec_.width /= 2;
  codec_.height /= 2;
  codec_.numberOfSimulcastStreams = 1;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  new_encoders = helper_->factory()->encoders();
  ASSERT_EQ(1u, new_encoders.size());
  ASSERT_EQ(original_encoders[0], new_encoders[0]);
  EXPECT_CALL(*original_encoders[0], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  frame_types.resize(1, kVideoFrameKey);
  EXPECT_EQ(0, adapter_->Encode(input_frame, nullptr, &frame_types));
  EXPECT_CALL(*original_encoders[0], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_EQ(0, adapter_->Release());

  // Encode with three streams, again.
  codec_.width *= 4;
  codec_.height *= 4;
  codec_.numberOfSimulcastStreams = 3;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  new_encoders = helper_->factory()->encoders();
  ASSERT_EQ(3u, new_encoders.size());
  // The first encoder is reused.
  ASSERT_EQ(original_encoders[0], new_encoders[0]);
  EXPECT_CALL(*original_encoders[0], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  // The second and third encoders are new.
  EXPECT_CALL(*new_encoders[1], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*new_encoders[2], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  frame_types.resize(3, kVideoFrameKey);
  EXPECT_EQ(0, adapter_->Encode(input_frame, nullptr, &frame_types));
  EXPECT_CALL(*original_encoders[0], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*new_encoders[1], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*new_encoders[2], Release())
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_EQ(0, adapter_->Release());
}

TEST_F(TestSimulcastEncoderAdapterFake, DoesNotLeakEncoders) {
  SetupCodec();
  VerifyCodecSettings();

  EXPECT_EQ(3u, helper_->factory()->encoders().size());

  // The adapter should destroy all encoders it has allocated. Since
  // |helper_->factory()| is owned by |adapter_|, however, we need to rely on
  // lsan to find leaks here.
  EXPECT_EQ(0, adapter_->Release());
  adapter_.reset();
}

// This test verifies that an adapter reinit with the same codec settings as
// before does not change the underlying encoder codec settings.
TEST_F(TestSimulcastEncoderAdapterFake, ReinitDoesNotReorderEncoderSettings) {
  SetupCodec();
  VerifyCodecSettings();

  // Capture current codec settings.
  std::vector<MockVideoEncoder*> encoders = helper_->factory()->encoders();
  ASSERT_EQ(3u, encoders.size());
  std::array<VideoCodec, 3> codecs_before;
  for (int i = 0; i < 3; ++i) {
    codecs_before[i] = encoders[i]->codec();
  }

  // Reinitialize and verify that the new codec settings are the same.
  EXPECT_EQ(0, adapter_->Release());
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  for (int i = 0; i < 3; ++i) {
    const VideoCodec& codec_before = codecs_before[i];
    const VideoCodec& codec_after = encoders[i]->codec();

    // webrtc::VideoCodec does not implement operator==.
    EXPECT_EQ(codec_before.codecType, codec_after.codecType);
    EXPECT_EQ(codec_before.plType, codec_after.plType);
    EXPECT_EQ(codec_before.width, codec_after.width);
    EXPECT_EQ(codec_before.height, codec_after.height);
    EXPECT_EQ(codec_before.startBitrate, codec_after.startBitrate);
    EXPECT_EQ(codec_before.maxBitrate, codec_after.maxBitrate);
    EXPECT_EQ(codec_before.minBitrate, codec_after.minBitrate);
    EXPECT_EQ(codec_before.targetBitrate, codec_after.targetBitrate);
    EXPECT_EQ(codec_before.maxFramerate, codec_after.maxFramerate);
    EXPECT_EQ(codec_before.qpMax, codec_after.qpMax);
    EXPECT_EQ(codec_before.numberOfSimulcastStreams,
              codec_after.numberOfSimulcastStreams);
    EXPECT_EQ(codec_before.mode, codec_after.mode);
    EXPECT_EQ(codec_before.expect_encode_from_texture,
              codec_after.expect_encode_from_texture);
  }
}

// This test is similar to the one above, except that it tests the simulcastIdx
// from the CodecSpecificInfo that is connected to an encoded frame. The
// PayloadRouter demuxes the incoming encoded frames on different RTP modules
// using the simulcastIdx, so it's important that there is no corresponding
// encoder reordering in between adapter reinits as this would lead to PictureID
// discontinuities.
TEST_F(TestSimulcastEncoderAdapterFake, ReinitDoesNotReorderFrameSimulcastIdx) {
  SetupCodec();
  adapter_->SetRateAllocation(rate_allocator_->GetAllocation(1200, 30), 30);
  VerifyCodecSettings();

  // Send frames on all streams.
  std::vector<MockVideoEncoder*> encoders = helper_->factory()->encoders();
  ASSERT_EQ(3u, encoders.size());
  encoders[0]->SendEncodedImage(1152, 704);
  int width;
  int height;
  int simulcast_index;
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(0, simulcast_index);

  encoders[1]->SendEncodedImage(300, 620);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(1, simulcast_index);

  encoders[2]->SendEncodedImage(120, 240);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(2, simulcast_index);

  // Reinitialize.
  EXPECT_EQ(0, adapter_->Release());
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->SetRateAllocation(rate_allocator_->GetAllocation(1200, 30), 30);

  // Verify that the same encoder sends out frames on the same simulcast index.
  encoders[0]->SendEncodedImage(1152, 704);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(0, simulcast_index);

  encoders[1]->SendEncodedImage(300, 620);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(1, simulcast_index);

  encoders[2]->SendEncodedImage(120, 240);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(2, simulcast_index);
}

TEST_F(TestSimulcastEncoderAdapterFake, SupportsNativeHandleForSingleStreams) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  codec_.numberOfSimulcastStreams = 1;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(1u, helper_->factory()->encoders().size());
  helper_->factory()->encoders()[0]->set_supports_native_handle(true);
  EXPECT_TRUE(adapter_->SupportsNativeHandle());
  helper_->factory()->encoders()[0]->set_supports_native_handle(false);
  EXPECT_FALSE(adapter_->SupportsNativeHandle());
}

TEST_F(TestSimulcastEncoderAdapterFake, SetRatesUnderMinBitrate) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  codec_.minBitrate = 50;
  codec_.numberOfSimulcastStreams = 1;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  rate_allocator_.reset(new SimulcastRateAllocator(codec_, nullptr));

  // Above min should be respected.
  BitrateAllocation target_bitrate =
      rate_allocator_->GetAllocation(codec_.minBitrate * 1000, 30);
  adapter_->SetRateAllocation(target_bitrate, 30);
  EXPECT_EQ(target_bitrate,
            helper_->factory()->encoders()[0]->last_set_bitrate());

  // Below min but non-zero should be replaced with the min bitrate.
  BitrateAllocation too_low_bitrate =
      rate_allocator_->GetAllocation((codec_.minBitrate - 1) * 1000, 30);
  adapter_->SetRateAllocation(too_low_bitrate, 30);
  EXPECT_EQ(target_bitrate,
            helper_->factory()->encoders()[0]->last_set_bitrate());

  // Zero should be passed on as is, since it means "pause".
  adapter_->SetRateAllocation(BitrateAllocation(), 30);
  EXPECT_EQ(BitrateAllocation(),
            helper_->factory()->encoders()[0]->last_set_bitrate());
}

TEST_F(TestSimulcastEncoderAdapterFake, SupportsImplementationName) {
  EXPECT_STREQ("SimulcastEncoderAdapter", adapter_->ImplementationName());
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  std::vector<const char*> encoder_names;
  encoder_names.push_back("codec1");
  encoder_names.push_back("codec2");
  encoder_names.push_back("codec3");
  helper_->factory()->SetEncoderNames(encoder_names);
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  EXPECT_STREQ("SimulcastEncoderAdapter (codec1, codec2, codec3)",
               adapter_->ImplementationName());

  // Single streams should not expose "SimulcastEncoderAdapter" in name.
  EXPECT_EQ(0, adapter_->Release());
  codec_.numberOfSimulcastStreams = 1;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(1u, helper_->factory()->encoders().size());
  EXPECT_STREQ("codec1", adapter_->ImplementationName());
}

TEST_F(TestSimulcastEncoderAdapterFake,
       SupportsNativeHandleForMultipleStreams) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  codec_.numberOfSimulcastStreams = 3;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(3u, helper_->factory()->encoders().size());
  for (MockVideoEncoder* encoder : helper_->factory()->encoders())
    encoder->set_supports_native_handle(true);
  // If one encoder doesn't support it, then overall support is disabled.
  helper_->factory()->encoders()[0]->set_supports_native_handle(false);
  EXPECT_FALSE(adapter_->SupportsNativeHandle());
  // Once all do, then the adapter claims support.
  helper_->factory()->encoders()[0]->set_supports_native_handle(true);
  EXPECT_TRUE(adapter_->SupportsNativeHandle());
}

// TODO(nisse): Reuse definition in webrtc/test/fake_texture_handle.h.
class FakeNativeBuffer : public VideoFrameBuffer {
 public:
  FakeNativeBuffer(int width, int height) : width_(width), height_(height) {}

  Type type() const override { return Type::kNative; }
  int width() const override { return width_; }
  int height() const override { return height_; }

  rtc::scoped_refptr<I420BufferInterface> ToI420() override {
    RTC_NOTREACHED();
    return nullptr;
  }

 private:
  const int width_;
  const int height_;
};

TEST_F(TestSimulcastEncoderAdapterFake,
       NativeHandleForwardingForMultipleStreams) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  codec_.numberOfSimulcastStreams = 3;
  // High start bitrate, so all streams are enabled.
  codec_.startBitrate = 3000;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(3u, helper_->factory()->encoders().size());
  for (MockVideoEncoder* encoder : helper_->factory()->encoders())
    encoder->set_supports_native_handle(true);
  EXPECT_TRUE(adapter_->SupportsNativeHandle());

  rtc::scoped_refptr<VideoFrameBuffer> buffer(
      new rtc::RefCountedObject<FakeNativeBuffer>(1280, 720));
  VideoFrame input_frame(buffer, 100, 1000, kVideoRotation_180);
  // Expect calls with the given video frame verbatim, since it's a texture
  // frame and can't otherwise be modified/resized.
  for (MockVideoEncoder* encoder : helper_->factory()->encoders())
    EXPECT_CALL(*encoder, Encode(::testing::Ref(input_frame), _, _)).Times(1);
  std::vector<FrameType> frame_types(3, kVideoFrameKey);
  EXPECT_EQ(0, adapter_->Encode(input_frame, nullptr, &frame_types));
}

TEST_F(TestSimulcastEncoderAdapterFake, TestFailureReturnCodesFromEncodeCalls) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  codec_.numberOfSimulcastStreams = 3;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(3u, helper_->factory()->encoders().size());
  // Tell the 2nd encoder to request software fallback.
  EXPECT_CALL(*helper_->factory()->encoders()[1], Encode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE));

  // Send a fake frame and assert the return is software fallback.
  rtc::scoped_refptr<I420Buffer> input_buffer =
      I420Buffer::Create(kDefaultWidth, kDefaultHeight);
  input_buffer->InitializeData();
  VideoFrame input_frame(input_buffer, 0, 0, webrtc::kVideoRotation_0);
  std::vector<FrameType> frame_types(3, kVideoFrameKey);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            adapter_->Encode(input_frame, nullptr, &frame_types));
}

TEST_F(TestSimulcastEncoderAdapterFake, TestInitFailureCleansUpEncoders) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.VP8()->tl_factory = &tl_factory_;
  codec_.numberOfSimulcastStreams = 3;
  helper_->factory()->set_init_encode_return_value(
      WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            adapter_->InitEncode(&codec_, 1, 1200));
  EXPECT_TRUE(helper_->factory()->encoders().empty());
}

}  // namespace testing
}  // namespace webrtc
