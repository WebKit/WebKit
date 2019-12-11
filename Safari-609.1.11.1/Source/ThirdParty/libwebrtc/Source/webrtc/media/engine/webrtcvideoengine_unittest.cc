/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/rtpparameters.h"
#include "api/test/mock_video_bitrate_allocator.h"
#include "api/test/mock_video_bitrate_allocator_factory.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/units/time_delta.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/flexfec_receive_stream.h"
#include "common_video/h264/profile_level_id.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/base/fakenetworkinterface.h"
#include "media/base/fakevideocapturer.h"
#include "media/base/fakevideorenderer.h"
#include "media/base/mediaconstants.h"
#include "media/base/rtputils.h"
#include "media/base/testutils.h"
#include "media/engine/constants.h"
#include "media/engine/fakewebrtccall.h"
#include "media/engine/fakewebrtcvideoengine.h"
#include "media/engine/simulcast.h"
#include "media/engine/webrtcvideoengine.h"
#include "media/engine/webrtcvoiceengine.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/stringutils.h"
#include "test/field_trial.h"
#include "test/gmock.h"

using cricket::FakeVideoCapturerWithTaskQueue;
using webrtc::BitrateConstraints;
using webrtc::RtpExtension;
using testing::Field;

namespace {
static const int kDefaultQpMax = 56;

static const uint8_t kRedRtxPayloadType = 125;

static const uint32_t kTimeout = 5000U;
static const uint32_t kDefaultReceiveSsrc = 0;
static const uint32_t kSsrc = 1234u;
static const uint32_t kSsrcs4[] = {1, 2, 3, 4};
static const int kVideoWidth = 640;
static const int kVideoHeight = 360;
static const int kFramerate = 30;

static const uint32_t kSsrcs1[] = {1};
static const uint32_t kSsrcs3[] = {1, 2, 3};
static const uint32_t kRtxSsrcs1[] = {4};
static const uint32_t kFlexfecSsrc = 5;
static const uint32_t kIncomingUnsignalledSsrc = 0xC0FFEE;
static const uint32_t kDefaultRecvSsrc = 0;

static const char kUnsupportedExtensionName[] =
    "urn:ietf:params:rtp-hdrext:unsupported";

cricket::VideoCodec RemoveFeedbackParams(cricket::VideoCodec&& codec) {
  codec.feedback_params = cricket::FeedbackParams();
  return std::move(codec);
}

void VerifyCodecHasDefaultFeedbackParams(const cricket::VideoCodec& codec) {
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamNack, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamNack, cricket::kRtcpFbNackParamPli)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamRemb, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamTransportCc, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamCcm, cricket::kRtcpFbCcmParamFir)));
}

// Return true if any codec in |codecs| is an RTX codec with associated payload
// type |payload_type|.
bool HasRtxCodec(const std::vector<cricket::VideoCodec>& codecs,
                 int payload_type) {
  for (const cricket::VideoCodec& codec : codecs) {
    int associated_payload_type;
    if (absl::EqualsIgnoreCase(codec.name.c_str(), "rtx") &&
        codec.GetParam(cricket::kCodecParamAssociatedPayloadType,
                       &associated_payload_type) &&
        associated_payload_type == payload_type) {
      return true;
    }
  }
  return false;
}

// TODO(nisse): Duplicated in call.cc.
const int* FindKeyByValue(const std::map<int, int>& m, int v) {
  for (const auto& kv : m) {
    if (kv.second == v)
      return &kv.first;
  }
  return nullptr;
}

bool HasRtxReceiveAssociation(const webrtc::VideoReceiveStream::Config& config,
                              int payload_type) {
  return FindKeyByValue(config.rtp.rtx_associated_payload_types,
                        payload_type) != nullptr;
}

// Check that there's an Rtx payload type for each decoder.
bool VerifyRtxReceiveAssociations(
    const webrtc::VideoReceiveStream::Config& config) {
  for (const auto& decoder : config.decoders) {
    if (!HasRtxReceiveAssociation(config, decoder.payload_type))
      return false;
  }
  return true;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateBlackFrameBuffer(
    int width,
    int height) {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(width, height);
  webrtc::I420Buffer::SetBlack(buffer);
  return buffer;
}

void VerifySendStreamHasRtxTypes(const webrtc::VideoSendStream::Config& config,
                                 const std::map<int, int>& rtx_types) {
  std::map<int, int>::const_iterator it;
  it = rtx_types.find(config.rtp.payload_type);
  EXPECT_TRUE(it != rtx_types.end() &&
              it->second == config.rtp.rtx.payload_type);

  if (config.rtp.ulpfec.red_rtx_payload_type != -1) {
    it = rtx_types.find(config.rtp.ulpfec.red_payload_type);
    EXPECT_TRUE(it != rtx_types.end() &&
                it->second == config.rtp.ulpfec.red_rtx_payload_type);
  }
}

cricket::MediaConfig GetMediaConfig() {
  cricket::MediaConfig media_config;
  media_config.video.enable_cpu_adaptation = false;
  return media_config;
}

// Values from GetMaxDefaultVideoBitrateKbps in webrtcvideoengine.cc.
int GetMaxDefaultBitrateBps(size_t width, size_t height) {
  if (width * height <= 320 * 240) {
    return 600000;
  } else if (width * height <= 640 * 480) {
    return 1700000;
  } else if (width * height <= 960 * 540) {
    return 2000000;
  } else {
    return 2500000;
  }
}

class MockVideoSource : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  MOCK_METHOD2(AddOrUpdateSink,
               void(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                    const rtc::VideoSinkWants& wants));
  MOCK_METHOD1(RemoveSink,
               void(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink));
};

}  // namespace

#define EXPECT_FRAME_WAIT(c, w, h, t)                        \
  EXPECT_EQ_WAIT((c), renderer_.num_rendered_frames(), (t)); \
  EXPECT_EQ((w), renderer_.width());                         \
  EXPECT_EQ((h), renderer_.height());                        \
  EXPECT_EQ(0, renderer_.errors());

#define EXPECT_FRAME_ON_RENDERER_WAIT(r, c, w, h, t)   \
  EXPECT_EQ_WAIT((c), (r).num_rendered_frames(), (t)); \
  EXPECT_EQ((w), (r).width());                         \
  EXPECT_EQ((h), (r).height());                        \
  EXPECT_EQ(0, (r).errors());

namespace cricket {
class WebRtcVideoEngineTest : public ::testing::Test {
 public:
  WebRtcVideoEngineTest() : WebRtcVideoEngineTest("") {}
  explicit WebRtcVideoEngineTest(const char* field_trials)
      : override_field_trials_(field_trials),
        call_(webrtc::Call::Create(webrtc::Call::Config(&event_log_))),
        encoder_factory_(new cricket::FakeWebRtcVideoEncoderFactory),
        decoder_factory_(new cricket::FakeWebRtcVideoDecoderFactory),
        engine_(std::unique_ptr<cricket::FakeWebRtcVideoEncoderFactory>(
                    encoder_factory_),
                std::unique_ptr<cricket::FakeWebRtcVideoDecoderFactory>(
                    decoder_factory_),
                webrtc::CreateBuiltinVideoBitrateAllocatorFactory()) {
    // Ensure fake clock doesn't return 0, which will cause some initializations
    // fail inside RTP senders.
    fake_clock_.AdvanceTimeMicros(1);
  }

 protected:
  void AssignDefaultAptRtxTypes();
  void AssignDefaultCodec();

  // Find the index of the codec in the engine with the given name. The codec
  // must be present.
  size_t GetEngineCodecIndex(const std::string& name) const;

  // Find the codec in the engine with the given name. The codec must be
  // present.
  cricket::VideoCodec GetEngineCodec(const std::string& name) const;

  VideoMediaChannel* SetSendParamsWithAllSupportedCodecs();

  VideoMediaChannel* SetRecvParamsWithSupportedCodecs(
      const std::vector<VideoCodec>& codecs);

  void TestExtendedEncoderOveruse(bool use_external_encoder);

  // Has to be the first one, so it is initialized before the call or there is a
  // race condition in the clock access.
  rtc::ScopedFakeClock fake_clock_;
  webrtc::test::ScopedFieldTrials override_field_trials_;
  webrtc::RtcEventLogNullImpl event_log_;
  // Used in WebRtcVideoEngineVoiceTest, but defined here so it's properly
  // initialized when the constructor is called.
  std::unique_ptr<webrtc::Call> call_;
  cricket::FakeWebRtcVideoEncoderFactory* encoder_factory_;
  cricket::FakeWebRtcVideoDecoderFactory* decoder_factory_;
  WebRtcVideoEngine engine_;
  VideoCodec default_codec_;
  std::map<int, int> default_apt_rtx_types_;
};

TEST_F(WebRtcVideoEngineTest, DefaultRtxCodecHasAssociatedPayloadTypeSet) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  AssignDefaultCodec();

  std::vector<VideoCodec> engine_codecs = engine_.codecs();
  for (size_t i = 0; i < engine_codecs.size(); ++i) {
    if (engine_codecs[i].name != kRtxCodecName)
      continue;
    int associated_payload_type;
    EXPECT_TRUE(engine_codecs[i].GetParam(kCodecParamAssociatedPayloadType,
                                          &associated_payload_type));
    EXPECT_EQ(default_codec_.id, associated_payload_type);
    return;
  }
  FAIL() << "No RTX codec found among default codecs.";
}

TEST_F(WebRtcVideoEngineTest, SupportsTimestampOffsetHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kTimestampOffsetUri) {
      EXPECT_EQ(RtpExtension::kTimestampOffsetDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Timestamp offset extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, SupportsAbsoluteSenderTimeHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kAbsSendTimeUri) {
      EXPECT_EQ(RtpExtension::kAbsSendTimeDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Absolute Sender Time extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, SupportsTransportSequenceNumberHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
      EXPECT_EQ(RtpExtension::kTransportSequenceNumberDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Transport sequence number extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, SupportsVideoRotationHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kVideoRotationUri) {
      EXPECT_EQ(RtpExtension::kVideoRotationDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Video Rotation extension not in header-extension list.";
}

class WebRtcVideoEngineTestWithGenericDescriptor
    : public WebRtcVideoEngineTest {
 public:
  WebRtcVideoEngineTestWithGenericDescriptor()
      : WebRtcVideoEngineTest("WebRTC-GenericDescriptorAdvertised/Enabled/") {}
};

TEST_F(WebRtcVideoEngineTestWithGenericDescriptor, AdvertiseGenericDescriptor) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kGenericFrameDescriptorUri) {
      EXPECT_EQ(RtpExtension::kGenericFrameDescriptorDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Generic descriptor extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, CVOSetHeaderExtensionBeforeCapturer) {
  // Allocate the source first to prevent early destruction before channel's
  // dtor is called.
  testing::NiceMock<MockVideoSource> video_source;

  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Add CVO extension.
  const int id = 1;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, id));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  EXPECT_CALL(
      video_source,
      AddOrUpdateSink(testing::_,
                      Field(&rtc::VideoSinkWants::rotation_applied, false)));
  // Set capturer.
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &video_source));

  // Verify capturer has turned off applying rotation.
  testing::Mock::VerifyAndClear(&video_source);

  // Verify removing header extension turns on applying rotation.
  parameters.extensions.clear();
  EXPECT_CALL(
      video_source,
      AddOrUpdateSink(testing::_,
                      Field(&rtc::VideoSinkWants::rotation_applied, true)));

  EXPECT_TRUE(channel->SetSendParameters(parameters));
}

TEST_F(WebRtcVideoEngineTest, CVOSetHeaderExtensionBeforeAddSendStream) {
  // Allocate the source first to prevent early destruction before channel's
  // dtor is called.
  testing::NiceMock<MockVideoSource> video_source;

  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());
  // Add CVO extension.
  const int id = 1;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, id));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Set source.
  EXPECT_CALL(
      video_source,
      AddOrUpdateSink(testing::_,
                      Field(&rtc::VideoSinkWants::rotation_applied, false)));
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &video_source));
}

TEST_F(WebRtcVideoEngineTest, CVOSetHeaderExtensionAfterCapturer) {
  testing::NiceMock<MockVideoSource> video_source;

  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("VP9");

  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Set capturer.
  EXPECT_CALL(
      video_source,
      AddOrUpdateSink(testing::_,
                      Field(&rtc::VideoSinkWants::rotation_applied, true)));
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &video_source));

  // Verify capturer has turned on applying rotation.
  testing::Mock::VerifyAndClear(&video_source);

  // Add CVO extension.
  const int id = 1;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  parameters.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, id));
  // Also remove the first codec to trigger a codec change as well.
  parameters.codecs.erase(parameters.codecs.begin());
  EXPECT_CALL(
      video_source,
      AddOrUpdateSink(testing::_,
                      Field(&rtc::VideoSinkWants::rotation_applied, false)));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  // Verify capturer has turned off applying rotation.
  testing::Mock::VerifyAndClear(&video_source);

  // Verify removing header extension turns on applying rotation.
  parameters.extensions.clear();
  EXPECT_CALL(
      video_source,
      AddOrUpdateSink(testing::_,
                      Field(&rtc::VideoSinkWants::rotation_applied, true)));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
}

TEST_F(WebRtcVideoEngineTest, SetSendFailsBeforeSettingCodecs) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));

  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));

  EXPECT_FALSE(channel->SetSend(true))
      << "Channel should not start without codecs.";
  EXPECT_TRUE(channel->SetSend(false))
      << "Channel should be stoppable even without set codecs.";
}

TEST_F(WebRtcVideoEngineTest, GetStatsWithoutSendCodecsSetDoesNotCrash) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));
  VideoMediaInfo info;
  channel->GetStats(&info);
}

TEST_F(WebRtcVideoEngineTest, UseFactoryForVp8WhenSupported) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());
  channel->OnReadyToSend(true);

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  EXPECT_EQ(0, encoder_factory_->GetNumCreatedEncoders());
  EXPECT_TRUE(channel->SetSend(true));
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Sending one frame will have allocate the encoder.
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(1));
  EXPECT_TRUE_WAIT(encoder_factory_->encoders()[0]->GetNumEncodedFrames() > 0,
                   kTimeout);

  int num_created_encoders = encoder_factory_->GetNumCreatedEncoders();
  EXPECT_EQ(num_created_encoders, 1);

  // Setting codecs of the same type should not reallocate any encoders
  // (expecting a no-op).
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_EQ(num_created_encoders, encoder_factory_->GetNumCreatedEncoders());

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
  EXPECT_EQ(0u, encoder_factory_->encoders().size());
}

// Test that when an encoder factory supports H264, we add an RTX
// codec for it.
// TODO(deadbeef): This test should be updated if/when we start
// adding RTX codecs for unrecognized codec names.
TEST_F(WebRtcVideoEngineTest, RtxCodecAddedForH264Codec) {
  using webrtc::H264::ProfileLevelIdToString;
  using webrtc::H264::ProfileLevelId;
  using webrtc::H264::kLevel1;
  webrtc::SdpVideoFormat h264_constrained_baseline("H264");
  h264_constrained_baseline.parameters[kH264FmtpProfileLevelId] =
      *ProfileLevelIdToString(
          ProfileLevelId(webrtc::H264::kProfileConstrainedBaseline, kLevel1));
  webrtc::SdpVideoFormat h264_constrained_high("H264");
  h264_constrained_high.parameters[kH264FmtpProfileLevelId] =
      *ProfileLevelIdToString(
          ProfileLevelId(webrtc::H264::kProfileConstrainedHigh, kLevel1));
  webrtc::SdpVideoFormat h264_high("H264");
  h264_high.parameters[kH264FmtpProfileLevelId] = *ProfileLevelIdToString(
      ProfileLevelId(webrtc::H264::kProfileHigh, kLevel1));

  encoder_factory_->AddSupportedVideoCodec(h264_constrained_baseline);
  encoder_factory_->AddSupportedVideoCodec(h264_constrained_high);
  encoder_factory_->AddSupportedVideoCodec(h264_high);

  // First figure out what payload types the test codecs got assigned.
  const std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  // Now search for RTX codecs for them. Expect that they all have associated
  // RTX codecs.
  EXPECT_TRUE(HasRtxCodec(
      codecs,
      FindMatchingCodec(codecs, cricket::VideoCodec(h264_constrained_baseline))
          ->id));
  EXPECT_TRUE(HasRtxCodec(
      codecs,
      FindMatchingCodec(codecs, cricket::VideoCodec(h264_constrained_high))
          ->id));
  EXPECT_TRUE(HasRtxCodec(
      codecs, FindMatchingCodec(codecs, cricket::VideoCodec(h264_high))->id));
}

#if defined(RTC_ENABLE_VP9)
TEST_F(WebRtcVideoEngineTest, CanConstructDecoderForVp9EncoderFactory) {
  encoder_factory_->AddSupportedVideoCodecType("VP9");

  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
}
#endif  // defined(RTC_ENABLE_VP9)

TEST_F(WebRtcVideoEngineTest, PropagatesInputFrameTimestamp) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  FakeCall* fake_call = new FakeCall();
  call_.reset(fake_call);
  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &capturer));
  capturer.Start(cricket::VideoFormat(1280, 720,
                                      cricket::VideoFormat::FpsToInterval(60),
                                      cricket::FOURCC_I420));
  channel->SetSend(true);

  FakeVideoSendStream* stream = fake_call->GetVideoSendStreams()[0];

  EXPECT_TRUE(capturer.CaptureFrame());
  int64_t last_timestamp = stream->GetLastTimestamp();
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(capturer.CaptureFrame());
    int64_t timestamp = stream->GetLastTimestamp();
    int64_t interval = timestamp - last_timestamp;

    // Precision changes from nanosecond to millisecond.
    // Allow error to be no more than 1.
    EXPECT_NEAR(cricket::VideoFormat::FpsToInterval(60) / 1E6, interval, 1);

    last_timestamp = timestamp;
  }

  capturer.Start(cricket::VideoFormat(1280, 720,
                                      cricket::VideoFormat::FpsToInterval(30),
                                      cricket::FOURCC_I420));

  EXPECT_TRUE(capturer.CaptureFrame());
  last_timestamp = stream->GetLastTimestamp();
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(capturer.CaptureFrame());
    int64_t timestamp = stream->GetLastTimestamp();
    int64_t interval = timestamp - last_timestamp;

    // Precision changes from nanosecond to millisecond.
    // Allow error to be no more than 1.
    EXPECT_NEAR(cricket::VideoFormat::FpsToInterval(30) / 1E6, interval, 1);

    last_timestamp = timestamp;
  }

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
}

void WebRtcVideoEngineTest::AssignDefaultAptRtxTypes() {
  std::vector<VideoCodec> engine_codecs = engine_.codecs();
  RTC_DCHECK(!engine_codecs.empty());
  for (const cricket::VideoCodec& codec : engine_codecs) {
    if (codec.name == "rtx") {
      int associated_payload_type;
      if (codec.GetParam(kCodecParamAssociatedPayloadType,
                         &associated_payload_type)) {
        default_apt_rtx_types_[associated_payload_type] = codec.id;
      }
    }
  }
}

void WebRtcVideoEngineTest::AssignDefaultCodec() {
  std::vector<VideoCodec> engine_codecs = engine_.codecs();
  RTC_DCHECK(!engine_codecs.empty());
  bool codec_set = false;
  for (const cricket::VideoCodec& codec : engine_codecs) {
    if (!codec_set && codec.name != "rtx" && codec.name != "red" &&
        codec.name != "ulpfec") {
      default_codec_ = codec;
      codec_set = true;
    }
  }

  RTC_DCHECK(codec_set);
}

size_t WebRtcVideoEngineTest::GetEngineCodecIndex(
    const std::string& name) const {
  const std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  for (size_t i = 0; i < codecs.size(); ++i) {
    const cricket::VideoCodec engine_codec = codecs[i];
    if (!absl::EqualsIgnoreCase(name, engine_codec.name))
      continue;
    // The tests only use H264 Constrained Baseline. Make sure we don't return
    // an internal H264 codec from the engine with a different H264 profile.
    if (absl::EqualsIgnoreCase(name.c_str(), kH264CodecName)) {
      const absl::optional<webrtc::H264::ProfileLevelId> profile_level_id =
          webrtc::H264::ParseSdpProfileLevelId(engine_codec.params);
      if (profile_level_id->profile !=
          webrtc::H264::kProfileConstrainedBaseline) {
        continue;
      }
    }
    return i;
  }
  // This point should never be reached.
  ADD_FAILURE() << "Unrecognized codec name: " << name;
  return -1;
}

cricket::VideoCodec WebRtcVideoEngineTest::GetEngineCodec(
    const std::string& name) const {
  return engine_.codecs()[GetEngineCodecIndex(name)];
}

VideoMediaChannel*
WebRtcVideoEngineTest::SetSendParamsWithAllSupportedCodecs() {
  VideoMediaChannel* channel = engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions());
  cricket::VideoSendParameters parameters;
  // We need to look up the codec in the engine to get the correct payload type.
  for (const webrtc::SdpVideoFormat& format :
       encoder_factory_->GetSupportedFormats()) {
    cricket::VideoCodec engine_codec = GetEngineCodec(format.name);
    if (std::find(parameters.codecs.begin(), parameters.codecs.end(),
                  engine_codec) == parameters.codecs.end()) {
      parameters.codecs.push_back(engine_codec);
    }
  }

  EXPECT_TRUE(channel->SetSendParameters(parameters));

  return channel;
}

VideoMediaChannel* WebRtcVideoEngineTest::SetRecvParamsWithSupportedCodecs(
    const std::vector<VideoCodec>& codecs) {
  VideoMediaChannel* channel = engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions());
  cricket::VideoRecvParameters parameters;
  parameters.codecs = codecs;
  EXPECT_TRUE(channel->SetRecvParameters(parameters));

  return channel;
}

TEST_F(WebRtcVideoEngineTest, UsesSimulcastAdapterForVp8Factories) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  EXPECT_TRUE(channel->AddSendStream(CreateSimStreamParams("cname", ssrcs)));
  EXPECT_TRUE(channel->SetSend(true));

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel->SetVideoSend(ssrcs.front(), nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(2));

  // Verify that encoders are configured for simulcast through adapter
  // (increasing resolution and only configured to send one stream each).
  int prev_width = -1;
  for (size_t i = 0; i < encoder_factory_->encoders().size(); ++i) {
    ASSERT_TRUE(encoder_factory_->encoders()[i]->WaitForInitEncode());
    webrtc::VideoCodec codec_settings =
        encoder_factory_->encoders()[i]->GetCodecSettings();
    EXPECT_EQ(0, codec_settings.numberOfSimulcastStreams);
    EXPECT_GT(codec_settings.width, prev_width);
    prev_width = codec_settings.width;
  }

  EXPECT_TRUE(channel->SetVideoSend(ssrcs.front(), nullptr, nullptr));

  channel.reset();
  ASSERT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest, ChannelWithH264CanChangeToVp8) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("H264");

  // Set capturer.
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));

  std::unique_ptr<VideoMediaChannel> channel(engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("H264"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &capturer));
  // Sending one frame will have allocate the encoder.
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_EQ_WAIT(1u, encoder_factory_->encoders().size(), kTimeout);

  cricket::VideoSendParameters new_parameters;
  new_parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(new_parameters));

  // Sending one frame will switch encoder.
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_EQ_WAIT(1u, encoder_factory_->encoders().size(), kTimeout);
}

TEST_F(WebRtcVideoEngineTest,
       UsesSimulcastAdapterForVp8WithCombinedVP8AndH264Factory) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  EXPECT_TRUE(channel->AddSendStream(CreateSimStreamParams("cname", ssrcs)));
  EXPECT_TRUE(channel->SetSend(true));

  // Send a fake frame, or else the media engine will configure the simulcast
  // encoder adapter at a low-enough size that it'll only create a single
  // encoder layer.
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel->SetVideoSend(ssrcs.front(), nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(2));
  ASSERT_TRUE(encoder_factory_->encoders()[0]->WaitForInitEncode());
  EXPECT_EQ(webrtc::kVideoCodecVP8,
            encoder_factory_->encoders()[0]->GetCodecSettings().codecType);

  channel.reset();
  // Make sure DestroyVideoEncoder was called on the factory.
  EXPECT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest,
       DestroysNonSimulcastEncoderFromCombinedVP8AndH264Factory) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("H264"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));

  // Send a frame of 720p. This should trigger a "real" encoder initialization.
  cricket::VideoFormat format(
      1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(format));
  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(1));
  ASSERT_EQ(1u, encoder_factory_->encoders().size());
  ASSERT_TRUE(encoder_factory_->encoders()[0]->WaitForInitEncode());
  EXPECT_EQ(webrtc::kVideoCodecH264,
            encoder_factory_->encoders()[0]->GetCodecSettings().codecType);

  channel.reset();
  // Make sure DestroyVideoEncoder was called on the factory.
  ASSERT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest, SimulcastEnabledForH264BehindFieldTrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-H264Simulcast/Enabled/");
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(engine_.CreateMediaChannel(
      call_.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("H264"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
  EXPECT_TRUE(
      channel->AddSendStream(cricket::CreateSimStreamParams("cname", ssrcs)));

  // Send a frame of 720p. This should trigger a "real" encoder initialization.
  cricket::VideoFormat format(
      1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel->SetVideoSend(ssrcs[0], nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(format));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(1));
  ASSERT_EQ(1u, encoder_factory_->encoders().size());
  FakeWebRtcVideoEncoder* encoder = encoder_factory_->encoders()[0];
  ASSERT_TRUE(encoder_factory_->encoders()[0]->WaitForInitEncode());
  EXPECT_EQ(webrtc::kVideoCodecH264, encoder->GetCodecSettings().codecType);
  EXPECT_LT(1u, encoder->GetCodecSettings().numberOfSimulcastStreams);
  EXPECT_TRUE(channel->SetVideoSend(ssrcs[0], nullptr, nullptr));
}

// Test that the FlexFEC field trial properly alters the output of
// WebRtcVideoEngine::codecs(), for an existing |engine_| object.
//
// TODO(brandtr): Remove this test, when the FlexFEC field trial is gone.
TEST_F(WebRtcVideoEngineTest,
       Flexfec03SupportedAsInternalCodecBehindFieldTrial) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  auto is_flexfec = [](const VideoCodec& codec) {
    if (codec.name == "flexfec-03")
      return true;
    return false;
  };

  // FlexFEC is not active without field trial.
  const std::vector<VideoCodec> codecs_before = engine_.codecs();
  EXPECT_EQ(codecs_before.end(), std::find_if(codecs_before.begin(),
                                              codecs_before.end(), is_flexfec));

  // FlexFEC is active with field trial.
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-FlexFEC-03-Advertised/Enabled/");
  const std::vector<VideoCodec> codecs_after = engine_.codecs();
  EXPECT_NE(codecs_after.end(),
            std::find_if(codecs_after.begin(), codecs_after.end(), is_flexfec));
}

// Test that codecs are added in the order they are reported from the factory.
TEST_F(WebRtcVideoEngineTest, ReportSupportedCodecs) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  const char* kFakeCodecName = "FakeCodec";
  encoder_factory_->AddSupportedVideoCodecType(kFakeCodecName);

  // The last reported codec should appear after the first codec in the vector.
  const size_t vp8_index = GetEngineCodecIndex("VP8");
  const size_t fake_codec_index = GetEngineCodecIndex(kFakeCodecName);
  EXPECT_LT(vp8_index, fake_codec_index);
}

// Test that a codec that was added after the engine was initialized
// does show up in the codec list after it was added.
TEST_F(WebRtcVideoEngineTest, ReportSupportedAddedCodec) {
  const char* kFakeExternalCodecName1 = "FakeExternalCodec1";
  const char* kFakeExternalCodecName2 = "FakeExternalCodec2";

  // Set up external encoder factory with first codec, and initialize engine.
  encoder_factory_->AddSupportedVideoCodecType(kFakeExternalCodecName1);

  std::vector<cricket::VideoCodec> codecs_before(engine_.codecs());

  // Add second codec.
  encoder_factory_->AddSupportedVideoCodecType(kFakeExternalCodecName2);
  std::vector<cricket::VideoCodec> codecs_after(engine_.codecs());
  // The codec itself and RTX should have been added.
  EXPECT_EQ(codecs_before.size() + 2, codecs_after.size());

  // Check that both fake codecs are present and that the second fake codec
  // appears after the first fake codec.
  const size_t fake_codec_index1 = GetEngineCodecIndex(kFakeExternalCodecName1);
  const size_t fake_codec_index2 = GetEngineCodecIndex(kFakeExternalCodecName2);
  EXPECT_LT(fake_codec_index1, fake_codec_index2);
}

TEST_F(WebRtcVideoEngineTest, ReportRtxForExternalCodec) {
  const char* kFakeCodecName = "FakeCodec";
  encoder_factory_->AddSupportedVideoCodecType(kFakeCodecName);

  const size_t fake_codec_index = GetEngineCodecIndex(kFakeCodecName);
  EXPECT_EQ("rtx", engine_.codecs().at(fake_codec_index + 1).name);
}

TEST_F(WebRtcVideoEngineTest, RegisterDecodersIfSupported) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  decoder_factory_->AddSupportedVideoCodecType(webrtc::SdpVideoFormat("VP8"));
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));

  std::unique_ptr<VideoMediaChannel> channel(
      SetRecvParamsWithSupportedCodecs(parameters.codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory_->decoders().size());

  // Setting codecs of the same type should not reallocate the decoder.
  EXPECT_TRUE(channel->SetRecvParameters(parameters));
  EXPECT_EQ(1, decoder_factory_->GetNumCreatedDecoders());

  // Remove stream previously added to free the external decoder instance.
  EXPECT_TRUE(channel->RemoveRecvStream(kSsrc));
  EXPECT_EQ(0u, decoder_factory_->decoders().size());
}

// Verifies that we can set up decoders.
TEST_F(WebRtcVideoEngineTest, RegisterH264DecoderIfSupported) {
  // TODO(pbos): Do not assume that encoder/decoder support is symmetric. We
  // can't even query the WebRtcVideoDecoderFactory for supported codecs.
  // For now we add a FakeWebRtcVideoEncoderFactory to add H264 to supported
  // codecs.
  encoder_factory_->AddSupportedVideoCodecType("H264");
  decoder_factory_->AddSupportedVideoCodecType(webrtc::SdpVideoFormat("H264"));
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(GetEngineCodec("H264"));

  std::unique_ptr<VideoMediaChannel> channel(
      SetRecvParamsWithSupportedCodecs(codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory_->decoders().size());
}

// Tests when GetSources is called with non-existing ssrc, it will return an
// empty list of RtpSource without crashing.
TEST_F(WebRtcVideoEngineTest, GetSourcesWithNonExistingSsrc) {
  // Setup an recv stream with |kSsrc|.
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  decoder_factory_->AddSupportedVideoCodecType(webrtc::SdpVideoFormat("VP8"));
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  std::unique_ptr<VideoMediaChannel> channel(
      SetRecvParamsWithSupportedCodecs(parameters.codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));

  // Call GetSources with |kSsrc + 1| which doesn't exist.
  std::vector<webrtc::RtpSource> sources = channel->GetSources(kSsrc + 1);
  EXPECT_EQ(0u, sources.size());
}

TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, NullFactories) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;
  std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory;
  std::unique_ptr<webrtc::VideoBitrateAllocatorFactory> rate_allocator_factory;
  WebRtcVideoEngine engine(std::move(encoder_factory),
                           std::move(decoder_factory),
                           std::move(rate_allocator_factory));
  EXPECT_EQ(0u, engine.codecs().size());
}

TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, EmptyFactories) {
  // |engine| take ownership of the factories.
  webrtc::MockVideoEncoderFactory* encoder_factory =
      new webrtc::MockVideoEncoderFactory();
  webrtc::MockVideoDecoderFactory* decoder_factory =
      new webrtc::MockVideoDecoderFactory();
  webrtc::MockVideoBitrateAllocatorFactory* rate_allocator_factory =
      new webrtc::MockVideoBitrateAllocatorFactory();
  WebRtcVideoEngine engine(
      (std::unique_ptr<webrtc::VideoEncoderFactory>(encoder_factory)),
      (std::unique_ptr<webrtc::VideoDecoderFactory>(decoder_factory)),
      (std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>(
          rate_allocator_factory)));
  EXPECT_CALL(*encoder_factory, GetSupportedFormats());
  EXPECT_EQ(0u, engine.codecs().size());
  EXPECT_CALL(*encoder_factory, Die());
  EXPECT_CALL(*decoder_factory, Die());
  EXPECT_CALL(*rate_allocator_factory, Die());
}

// Test full behavior in the video engine when video codec factories of the new
// type are injected supporting the single codec Vp8. Check the returned codecs
// from the engine and that we will create a Vp8 encoder and decoder using the
// new factories.
TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, Vp8) {
  // |engine| take ownership of the factories.
  webrtc::MockVideoEncoderFactory* encoder_factory =
      new webrtc::MockVideoEncoderFactory();
  webrtc::MockVideoDecoderFactory* decoder_factory =
      new webrtc::MockVideoDecoderFactory();
  webrtc::MockVideoBitrateAllocatorFactory* rate_allocator_factory =
      new webrtc::MockVideoBitrateAllocatorFactory();
  EXPECT_CALL(*rate_allocator_factory,
              CreateVideoBitrateAllocatorProxy(Field(
                  &webrtc::VideoCodec::codecType, webrtc::kVideoCodecVP8)))
      .WillOnce(testing::Return(new webrtc::MockVideoBitrateAllocator()));
  WebRtcVideoEngine engine(
      (std::unique_ptr<webrtc::VideoEncoderFactory>(encoder_factory)),
      (std::unique_ptr<webrtc::VideoDecoderFactory>(decoder_factory)),
      (std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>(
          rate_allocator_factory)));
  const webrtc::SdpVideoFormat vp8_format("VP8");
  const std::vector<webrtc::SdpVideoFormat> supported_formats = {vp8_format};
  EXPECT_CALL(*encoder_factory, GetSupportedFormats())
      .WillRepeatedly(testing::Return(supported_formats));

  // Verify the codecs from the engine.
  const std::vector<VideoCodec> engine_codecs = engine.codecs();
  // Verify default codecs has been added correctly.
  EXPECT_EQ(5u, engine_codecs.size());
  EXPECT_EQ("VP8", engine_codecs.at(0).name);

  // RTX codec for VP8.
  EXPECT_EQ("rtx", engine_codecs.at(1).name);
  int vp8_associated_payload;
  EXPECT_TRUE(engine_codecs.at(1).GetParam(kCodecParamAssociatedPayloadType,
                                           &vp8_associated_payload));
  EXPECT_EQ(vp8_associated_payload, engine_codecs.at(0).id);

  EXPECT_EQ(kRedCodecName, engine_codecs.at(2).name);

  // RTX codec for RED.
  EXPECT_EQ("rtx", engine_codecs.at(3).name);
  int red_associated_payload;
  EXPECT_TRUE(engine_codecs.at(3).GetParam(kCodecParamAssociatedPayloadType,
                                           &red_associated_payload));
  EXPECT_EQ(red_associated_payload, engine_codecs.at(2).id);

  EXPECT_EQ(kUlpfecCodecName, engine_codecs.at(4).name);

  int associated_payload_type;
  EXPECT_TRUE(engine_codecs.at(1).GetParam(
      cricket::kCodecParamAssociatedPayloadType, &associated_payload_type));
  EXPECT_EQ(engine_codecs.at(0).id, associated_payload_type);
  // Verify default parameters has been added to the VP8 codec.
  VerifyCodecHasDefaultFeedbackParams(engine_codecs.at(0));

  // Mock encoder creation. |engine| take ownership of the encoder.
  webrtc::VideoEncoderFactory::CodecInfo codec_info;
  codec_info.is_hardware_accelerated = false;
  codec_info.has_internal_source = false;
  const webrtc::SdpVideoFormat format("VP8");
  EXPECT_CALL(*encoder_factory, QueryVideoEncoder(format))
      .WillRepeatedly(testing::Return(codec_info));
  FakeWebRtcVideoEncoder* const encoder = new FakeWebRtcVideoEncoder(nullptr);
  rtc::Event encoder_created;
  EXPECT_CALL(*encoder_factory, CreateVideoEncoderProxy(format))
      .WillOnce(
          ::testing::DoAll(::testing::InvokeWithoutArgs(
                               [&encoder_created]() { encoder_created.Set(); }),
                           ::testing::Return(encoder)));

  // Mock decoder creation. |engine| take ownership of the decoder.
  FakeWebRtcVideoDecoder* const decoder = new FakeWebRtcVideoDecoder(nullptr);
  EXPECT_CALL(*decoder_factory, CreateVideoDecoderProxy(format))
      .WillOnce(testing::Return(decoder));

  // Create a call.
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));

  // Create send channel.
  const int send_ssrc = 123;
  std::unique_ptr<VideoMediaChannel> send_channel(engine.CreateMediaChannel(
      call.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(engine_codecs.at(0));
  EXPECT_TRUE(send_channel->SetSendParameters(send_parameters));
  send_channel->OnReadyToSend(true);
  EXPECT_TRUE(
      send_channel->AddSendStream(StreamParams::CreateLegacy(send_ssrc)));
  EXPECT_TRUE(send_channel->SetSend(true));

  // Set capturer.
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(send_channel->SetVideoSend(send_ssrc, nullptr, &capturer));
  // Sending one frame will allocate the encoder.
  EXPECT_TRUE(capturer.CaptureFrame());
  encoder_created.Wait(kTimeout);

  // Create recv channel.
  const int recv_ssrc = 321;
  std::unique_ptr<VideoMediaChannel> recv_channel(engine.CreateMediaChannel(
      call.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(engine_codecs.at(0));
  EXPECT_TRUE(recv_channel->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(recv_channel->AddRecvStream(
      cricket::StreamParams::CreateLegacy(recv_ssrc)));

  // Remove streams previously added to free the encoder and decoder instance.
  EXPECT_CALL(*encoder_factory, Die());
  EXPECT_CALL(*decoder_factory, Die());
  EXPECT_CALL(*rate_allocator_factory, Die());
  EXPECT_TRUE(send_channel->RemoveSendStream(send_ssrc));
  EXPECT_TRUE(recv_channel->RemoveRecvStream(recv_ssrc));
}

// Test behavior when decoder factory fails to create a decoder (returns null).
TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, NullDecoder) {
  // |engine| take ownership of the factories.
  webrtc::MockVideoEncoderFactory* encoder_factory =
      new webrtc::MockVideoEncoderFactory();
  webrtc::MockVideoDecoderFactory* decoder_factory =
      new webrtc::MockVideoDecoderFactory();
  webrtc::MockVideoBitrateAllocatorFactory* rate_allocator_factory =
      new webrtc::MockVideoBitrateAllocatorFactory();
  WebRtcVideoEngine engine(
      (std::unique_ptr<webrtc::VideoEncoderFactory>(encoder_factory)),
      (std::unique_ptr<webrtc::VideoDecoderFactory>(decoder_factory)),
      (std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>(
          rate_allocator_factory)));
  const webrtc::SdpVideoFormat vp8_format("VP8");
  const std::vector<webrtc::SdpVideoFormat> supported_formats = {vp8_format};
  EXPECT_CALL(*encoder_factory, GetSupportedFormats())
      .WillRepeatedly(testing::Return(supported_formats));

  // Decoder creation fails.
  EXPECT_CALL(*decoder_factory, CreateVideoDecoderProxy(testing::_))
      .WillOnce(testing::Return(nullptr));

  // Create a call.
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));

  // Create recv channel.
  const int recv_ssrc = 321;
  std::unique_ptr<VideoMediaChannel> recv_channel(engine.CreateMediaChannel(
      call.get(), GetMediaConfig(), VideoOptions(), webrtc::CryptoOptions()));
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(engine.codecs().front());
  EXPECT_TRUE(recv_channel->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(recv_channel->AddRecvStream(
      cricket::StreamParams::CreateLegacy(recv_ssrc)));

  // Remove streams previously added to free the encoder and decoder instance.
  EXPECT_TRUE(recv_channel->RemoveRecvStream(recv_ssrc));
}

TEST_F(WebRtcVideoEngineTest, DISABLED_RecreatesEncoderOnContentTypeChange) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  std::unique_ptr<FakeCall> fake_call(new FakeCall());
  std::unique_ptr<VideoMediaChannel> channel(
      SetSendParamsWithAllSupportedCodecs());
  ASSERT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);
  channel->OnReadyToSend(true);
  channel->SetSend(true);
  ASSERT_TRUE(channel->SetSendParameters(parameters));

  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, &options, &capturer));

  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(1));
  EXPECT_EQ(webrtc::VideoCodecMode::kRealtimeVideo,
            encoder_factory_->encoders().back()->GetCodecSettings().mode);

  EXPECT_TRUE(channel->SetVideoSend(kSsrc, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // No change in content type, keep current encoder.
  EXPECT_EQ(1, encoder_factory_->GetNumCreatedEncoders());

  options.is_screencast.emplace(true);
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Change to screen content, recreate encoder. For the simulcast encoder
  // adapter case, this will result in two calls since InitEncode triggers a
  // a new instance.
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(2));
  EXPECT_EQ(webrtc::VideoCodecMode::kScreensharing,
            encoder_factory_->encoders().back()->GetCodecSettings().mode);

  EXPECT_TRUE(channel->SetVideoSend(kSsrc, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Still screen content, no need to update encoder.
  EXPECT_EQ(2, encoder_factory_->GetNumCreatedEncoders());

  options.is_screencast.emplace(false);
  options.video_noise_reduction.emplace(false);
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, &options, &capturer));
  // Change back to regular video content, update encoder. Also change
  // a non |is_screencast| option just to verify it doesn't affect recreation.
  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(3));
  EXPECT_EQ(webrtc::VideoCodecMode::kRealtimeVideo,
            encoder_factory_->encoders().back()->GetCodecSettings().mode);

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
  EXPECT_EQ(0u, encoder_factory_->encoders().size());
}

class WebRtcVideoChannelBaseTest : public testing::Test {
 protected:
  WebRtcVideoChannelBaseTest()
      : engine_(webrtc::CreateBuiltinVideoEncoderFactory(),
                webrtc::CreateBuiltinVideoDecoderFactory(),
                webrtc::CreateBuiltinVideoBitrateAllocatorFactory()) {}

  virtual void SetUp() {
    // One testcase calls SetUp in a loop, only create call_ once.
    if (!call_) {
      call_.reset(webrtc::Call::Create(webrtc::Call::Config(&event_log_)));
    }
    cricket::MediaConfig media_config;
    // Disabling cpu overuse detection actually disables quality scaling too; it
    // implies DegradationPreference kMaintainResolution. Automatic scaling
    // needs to be disabled, otherwise, tests which check the size of received
    // frames become flaky.
    media_config.video.enable_cpu_adaptation = false;
    channel_.reset(
        static_cast<cricket::WebRtcVideoChannel*>(engine_.CreateMediaChannel(
            call_.get(), media_config, cricket::VideoOptions(),
            webrtc::CryptoOptions())));
    channel_->OnReadyToSend(true);
    EXPECT_TRUE(channel_.get() != NULL);
    network_interface_.SetDestination(channel_.get());
    channel_->SetInterface(&network_interface_, /*media_transport=*/nullptr);
    cricket::VideoRecvParameters parameters;
    parameters.codecs = engine_.codecs();
    channel_->SetRecvParameters(parameters);
    EXPECT_TRUE(channel_->AddSendStream(DefaultSendStreamParams()));
    video_capturer_.reset(CreateFakeVideoCapturer());
    cricket::VideoFormat format(640, 480,
                                cricket::VideoFormat::FpsToInterval(kFramerate),
                                cricket::FOURCC_I420);
    EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(format));
    EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, video_capturer_.get()));
  }

  virtual cricket::FakeVideoCapturerWithTaskQueue* CreateFakeVideoCapturer() {
    return new cricket::FakeVideoCapturerWithTaskQueue();
  }

  // Utility method to setup an additional stream to send and receive video.
  // Used to test send and recv between two streams.
  void SetUpSecondStream() {
    SetUpSecondStreamWithNoRecv();
    // Setup recv for second stream.
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc + 2)));
    // Make the second renderer available for use by a new stream.
    EXPECT_TRUE(channel_->SetSink(kSsrc + 2, &renderer2_));
  }
  // Setup an additional stream just to send video. Defer add recv stream.
  // This is required if you want to test unsignalled recv of video rtp packets.
  void SetUpSecondStreamWithNoRecv() {
    // SetUp() already added kSsrc make sure duplicate SSRCs cant be added.
    EXPECT_TRUE(
        channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
    EXPECT_TRUE(channel_->SetSink(kSsrc, &renderer_));
    EXPECT_FALSE(
        channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc + 2)));
    // We dont add recv for the second stream.

    // Setup the receive and renderer for second stream after send.
    video_capturer_2_.reset(CreateFakeVideoCapturer());
    cricket::VideoFormat format(640, 480,
                                cricket::VideoFormat::FpsToInterval(kFramerate),
                                cricket::FOURCC_I420);
    EXPECT_EQ(cricket::CS_RUNNING, video_capturer_2_->Start(format));

    EXPECT_TRUE(
        channel_->SetVideoSend(kSsrc + 2, nullptr, video_capturer_2_.get()));
  }
  virtual void TearDown() { channel_.reset(); }
  bool SetDefaultCodec() { return SetOneCodec(DefaultCodec()); }

  bool SetOneCodec(const cricket::VideoCodec& codec) {
    cricket::VideoFormat capture_format(
        kVideoWidth, kVideoHeight,
        cricket::VideoFormat::FpsToInterval(kFramerate), cricket::FOURCC_I420);

    if (video_capturer_) {
      EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format));
    }
    if (video_capturer_2_) {
      EXPECT_EQ(cricket::CS_RUNNING, video_capturer_2_->Start(capture_format));
    }

    bool sending = channel_->sending();
    bool success = SetSend(false);
    if (success) {
      cricket::VideoSendParameters parameters;
      parameters.codecs.push_back(codec);
      success = channel_->SetSendParameters(parameters);
    }
    if (success) {
      success = SetSend(sending);
    }
    return success;
  }
  bool SetSend(bool send) { return channel_->SetSend(send); }
  bool SendFrame() {
    if (video_capturer_2_) {
      video_capturer_2_->CaptureFrame();
    }
    return video_capturer_.get() && video_capturer_->CaptureFrame();
  }
  bool WaitAndSendFrame(int wait_ms) {
    bool ret = rtc::Thread::Current()->ProcessMessages(wait_ms);
    ret &= SendFrame();
    return ret;
  }
  int NumRtpBytes() { return network_interface_.NumRtpBytes(); }
  int NumRtpBytes(uint32_t ssrc) {
    return network_interface_.NumRtpBytes(ssrc);
  }
  int NumRtpPackets() { return network_interface_.NumRtpPackets(); }
  int NumRtpPackets(uint32_t ssrc) {
    return network_interface_.NumRtpPackets(ssrc);
  }
  int NumSentSsrcs() { return network_interface_.NumSentSsrcs(); }
  const rtc::CopyOnWriteBuffer* GetRtpPacket(int index) {
    return network_interface_.GetRtpPacket(index);
  }
  static int GetPayloadType(const rtc::CopyOnWriteBuffer* p) {
    webrtc::RTPHeader header;
    EXPECT_TRUE(ParseRtpPacket(p, &header));
    return header.payloadType;
  }

  static bool ParseRtpPacket(const rtc::CopyOnWriteBuffer* p,
                             webrtc::RTPHeader* header) {
    std::unique_ptr<webrtc::RtpHeaderParser> parser(
        webrtc::RtpHeaderParser::Create());
    return parser->Parse(p->cdata(), p->size(), header);
  }

  // Tests that we can send and receive frames.
  void SendAndReceive(const cricket::VideoCodec& codec) {
    EXPECT_TRUE(SetOneCodec(codec));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    EXPECT_EQ(codec.id, GetPayloadType(p.get()));
  }

  void SendReceiveManyAndGetStats(const cricket::VideoCodec& codec,
                                  int duration_sec,
                                  int fps) {
    EXPECT_TRUE(SetOneCodec(codec));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    for (int i = 0; i < duration_sec; ++i) {
      for (int frame = 1; frame <= fps; ++frame) {
        EXPECT_TRUE(WaitAndSendFrame(1000 / fps));
        EXPECT_FRAME_WAIT(frame + i * fps, kVideoWidth, kVideoHeight, kTimeout);
      }
    }
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    EXPECT_EQ(codec.id, GetPayloadType(p.get()));
  }

  cricket::VideoSenderInfo GetSenderStats(size_t i) {
    cricket::VideoMediaInfo info;
    EXPECT_TRUE(channel_->GetStats(&info));
    return info.senders[i];
  }

  cricket::VideoReceiverInfo GetReceiverStats(size_t i) {
    cricket::VideoMediaInfo info;
    EXPECT_TRUE(channel_->GetStats(&info));
    return info.receivers[i];
  }

  // Two streams one channel tests.

  // Tests that we can send and receive frames.
  void TwoStreamsSendAndReceive(const cricket::VideoCodec& codec) {
    SetUpSecondStream();
    // Test sending and receiving on first stream.
    SendAndReceive(codec);
    // Test sending and receiving on second stream.
    EXPECT_EQ_WAIT(1, renderer2_.num_rendered_frames(), kTimeout);
    EXPECT_GT(NumRtpPackets(), 0);
    EXPECT_EQ(1, renderer2_.num_rendered_frames());
  }

  cricket::VideoCodec GetEngineCodec(const std::string& name) {
    for (const cricket::VideoCodec& engine_codec : engine_.codecs()) {
      if (absl::EqualsIgnoreCase(name, engine_codec.name))
        return engine_codec;
    }
    // This point should never be reached.
    ADD_FAILURE() << "Unrecognized codec name: " << name;
    return cricket::VideoCodec();
  }

  cricket::VideoCodec DefaultCodec() { return GetEngineCodec("VP8"); }

  cricket::StreamParams DefaultSendStreamParams() {
    return cricket::StreamParams::CreateLegacy(kSsrc);
  }

  webrtc::RtcEventLogNullImpl event_log_;
  std::unique_ptr<webrtc::Call> call_;
  WebRtcVideoEngine engine_;
  std::unique_ptr<cricket::FakeVideoCapturerWithTaskQueue> video_capturer_;
  std::unique_ptr<cricket::FakeVideoCapturerWithTaskQueue> video_capturer_2_;
  std::unique_ptr<WebRtcVideoChannel> channel_;
  cricket::FakeNetworkInterface network_interface_;
  cricket::FakeVideoRenderer renderer_;

  // Used by test cases where 2 streams are run on the same channel.
  cricket::FakeVideoRenderer renderer2_;
};

// Test that SetSend works.
TEST_F(WebRtcVideoChannelBaseTest, SetSend) {
  EXPECT_FALSE(channel_->sending());
  EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, video_capturer_.get()));
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  EXPECT_FALSE(channel_->sending());
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->sending());
  EXPECT_TRUE(SendFrame());
  EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
  EXPECT_TRUE(SetSend(false));
  EXPECT_FALSE(channel_->sending());
}

// Test that SetSend fails without codecs being set.
TEST_F(WebRtcVideoChannelBaseTest, SetSendWithoutCodecs) {
  EXPECT_FALSE(channel_->sending());
  EXPECT_FALSE(SetSend(true));
  EXPECT_FALSE(channel_->sending());
}

// Test that we properly set the send and recv buffer sizes by the time
// SetSend is called.
TEST_F(WebRtcVideoChannelBaseTest, SetSendSetsTransportBufferSizes) {
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  EXPECT_TRUE(SetSend(true));
  EXPECT_EQ(64 * 1024, network_interface_.sendbuf_size());
  EXPECT_EQ(256 * 1024, network_interface_.recvbuf_size());
}

// Test that stats work properly for a 1-1 call.
TEST_F(WebRtcVideoChannelBaseTest, GetStats) {
  const int kDurationSec = 3;
  const int kFps = 10;
  SendReceiveManyAndGetStats(DefaultCodec(), kDurationSec, kFps);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));

  ASSERT_EQ(1U, info.senders.size());
  // TODO(whyuan): bytes_sent and bytes_rcvd are different. Are both payload?
  // For webrtc, bytes_sent does not include the RTP header length.
  EXPECT_GT(info.senders[0].bytes_sent, 0);
  EXPECT_EQ(NumRtpPackets(), info.senders[0].packets_sent);
  EXPECT_EQ(0.0, info.senders[0].fraction_lost);
  ASSERT_TRUE(info.senders[0].codec_payload_type);
  EXPECT_EQ(DefaultCodec().id, *info.senders[0].codec_payload_type);
  EXPECT_EQ(0, info.senders[0].firs_rcvd);
  EXPECT_EQ(0, info.senders[0].plis_rcvd);
  EXPECT_EQ(0, info.senders[0].nacks_rcvd);
  EXPECT_EQ(kVideoWidth, info.senders[0].send_frame_width);
  EXPECT_EQ(kVideoHeight, info.senders[0].send_frame_height);
  EXPECT_GT(info.senders[0].framerate_input, 0);
  EXPECT_GT(info.senders[0].framerate_sent, 0);

  EXPECT_EQ(1U, info.send_codecs.count(DefaultCodec().id));
  EXPECT_EQ(DefaultCodec().ToCodecParameters(),
            info.send_codecs[DefaultCodec().id]);

  ASSERT_EQ(1U, info.receivers.size());
  EXPECT_EQ(1U, info.senders[0].ssrcs().size());
  EXPECT_EQ(1U, info.receivers[0].ssrcs().size());
  EXPECT_EQ(info.senders[0].ssrcs()[0], info.receivers[0].ssrcs()[0]);
  ASSERT_TRUE(info.receivers[0].codec_payload_type);
  EXPECT_EQ(DefaultCodec().id, *info.receivers[0].codec_payload_type);
  EXPECT_EQ(NumRtpBytes(), info.receivers[0].bytes_rcvd);
  EXPECT_EQ(NumRtpPackets(), info.receivers[0].packets_rcvd);
  EXPECT_EQ(0.0, info.receivers[0].fraction_lost);
  EXPECT_EQ(0, info.receivers[0].packets_lost);
  // TODO(asapersson): Not set for webrtc. Handle missing stats.
  // EXPECT_EQ(0, info.receivers[0].packets_concealed);
  EXPECT_EQ(0, info.receivers[0].firs_sent);
  EXPECT_EQ(0, info.receivers[0].plis_sent);
  EXPECT_EQ(0, info.receivers[0].nacks_sent);
  EXPECT_EQ(kVideoWidth, info.receivers[0].frame_width);
  EXPECT_EQ(kVideoHeight, info.receivers[0].frame_height);
  EXPECT_GT(info.receivers[0].framerate_rcvd, 0);
  EXPECT_GT(info.receivers[0].framerate_decoded, 0);
  EXPECT_GT(info.receivers[0].framerate_output, 0);

  EXPECT_EQ(1U, info.receive_codecs.count(DefaultCodec().id));
  EXPECT_EQ(DefaultCodec().ToCodecParameters(),
            info.receive_codecs[DefaultCodec().id]);
}

// Test that stats work properly for a conf call with multiple recv streams.
TEST_F(WebRtcVideoChannelBaseTest, GetStatsMultipleRecvStreams) {
  cricket::FakeVideoRenderer renderer1, renderer2;
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(DefaultCodec());
  parameters.conference_mode = true;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_TRUE(channel_->SetSink(1, &renderer1));
  EXPECT_TRUE(channel_->SetSink(2, &renderer2));
  EXPECT_EQ(0, renderer1.num_rendered_frames());
  EXPECT_EQ(0, renderer2.num_rendered_frames());
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(1);
  ssrcs.push_back(2);
  network_interface_.SetConferenceMode(true, ssrcs);
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1, kVideoWidth, kVideoHeight,
                                kTimeout);
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, kVideoWidth, kVideoHeight,
                                kTimeout);

  EXPECT_TRUE(channel_->SetSend(false));

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  // TODO(whyuan): bytes_sent and bytes_rcvd are different. Are both payload?
  // For webrtc, bytes_sent does not include the RTP header length.
  EXPECT_GT(GetSenderStats(0).bytes_sent, 0);
  EXPECT_EQ_WAIT(NumRtpPackets(), GetSenderStats(0).packets_sent, kTimeout);
  EXPECT_EQ(kVideoWidth, GetSenderStats(0).send_frame_width);
  EXPECT_EQ(kVideoHeight, GetSenderStats(0).send_frame_height);

  ASSERT_EQ(2U, info.receivers.size());
  for (size_t i = 0; i < info.receivers.size(); ++i) {
    EXPECT_EQ(1U, GetReceiverStats(i).ssrcs().size());
    EXPECT_EQ(i + 1, GetReceiverStats(i).ssrcs()[0]);
    EXPECT_EQ_WAIT(NumRtpBytes(), GetReceiverStats(i).bytes_rcvd, kTimeout);
    EXPECT_EQ_WAIT(NumRtpPackets(), GetReceiverStats(i).packets_rcvd, kTimeout);
    EXPECT_EQ_WAIT(kVideoWidth, GetReceiverStats(i).frame_width, kTimeout);
    EXPECT_EQ_WAIT(kVideoHeight, GetReceiverStats(i).frame_height, kTimeout);
  }
}

// Test that stats work properly for a conf call with multiple send streams.
TEST_F(WebRtcVideoChannelBaseTest, GetStatsMultipleSendStreams) {
  // Normal setup; note that we set the SSRC explicitly to ensure that
  // it will come first in the senders map.
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(DefaultCodec());
  parameters.conference_mode = true;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(
      channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  EXPECT_TRUE(channel_->SetSink(kSsrc, &renderer_));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(SendFrame());
  EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
  EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);

  // Add an additional capturer, and hook up a renderer to receive it.
  cricket::FakeVideoRenderer renderer2;
  std::unique_ptr<cricket::FakeVideoCapturerWithTaskQueue> capturer(
      CreateFakeVideoCapturer());
  const int kTestWidth = 160;
  const int kTestHeight = 120;
  cricket::VideoFormat format(kTestWidth, kTestHeight,
                              cricket::VideoFormat::FpsToInterval(5),
                              cricket::FOURCC_I420);
  EXPECT_EQ(cricket::CS_RUNNING, capturer->Start(format));
  EXPECT_TRUE(
      channel_->AddSendStream(cricket::StreamParams::CreateLegacy(5678)));
  EXPECT_TRUE(channel_->SetVideoSend(5678, nullptr, capturer.get()));
  EXPECT_TRUE(
      channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(5678)));
  EXPECT_TRUE(channel_->SetSink(5678, &renderer2));
  EXPECT_TRUE(capturer->CaptureCustomFrame(kTestWidth, kTestHeight));
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, kTestWidth, kTestHeight,
                                kTimeout);

  // Get stats, and make sure they are correct for two senders. We wait until
  // the number of expected packets have been sent to avoid races where we
  // check stats before it has been updated.
  cricket::VideoMediaInfo info;
  for (uint32_t i = 0; i < kTimeout; ++i) {
    rtc::Thread::Current()->ProcessMessages(1);
    EXPECT_TRUE(channel_->GetStats(&info));
    ASSERT_EQ(2U, info.senders.size());
    if (info.senders[0].packets_sent + info.senders[1].packets_sent ==
        NumRtpPackets()) {
      // Stats have been updated for both sent frames, expectations can be
      // checked now.
      break;
    }
  }
  EXPECT_EQ(NumRtpPackets(),
            info.senders[0].packets_sent + info.senders[1].packets_sent)
      << "Timed out while waiting for packet counts for all sent packets.";
  EXPECT_EQ(1U, info.senders[0].ssrcs().size());
  EXPECT_EQ(1234U, info.senders[0].ssrcs()[0]);
  EXPECT_EQ(kVideoWidth, info.senders[0].send_frame_width);
  EXPECT_EQ(kVideoHeight, info.senders[0].send_frame_height);
  EXPECT_EQ(1U, info.senders[1].ssrcs().size());
  EXPECT_EQ(5678U, info.senders[1].ssrcs()[0]);
  EXPECT_EQ(kTestWidth, info.senders[1].send_frame_width);
  EXPECT_EQ(kTestHeight, info.senders[1].send_frame_height);
  // The capturer must be unregistered here as it runs out of it's scope next.
  channel_->SetVideoSend(5678, nullptr, nullptr);
}

// Test that we can set the bandwidth.
TEST_F(WebRtcVideoChannelBaseTest, SetSendBandwidth) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(DefaultCodec());
  parameters.max_bandwidth_bps = -1;  // <= 0 means unlimited.
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  parameters.max_bandwidth_bps = 128 * 1024;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
}

// Test that we can set the SSRC for the default send source.
TEST_F(WebRtcVideoChannelBaseTest, SetSendSsrc) {
  EXPECT_TRUE(SetDefaultCodec());
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(SendFrame());
  EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
  webrtc::RTPHeader header;
  std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
  EXPECT_TRUE(ParseRtpPacket(p.get(), &header));
  EXPECT_EQ(kSsrc, header.ssrc);

  // Packets are being paced out, so these can mismatch between the first and
  // second call to NumRtpPackets until pending packets are paced out.
  EXPECT_EQ_WAIT(NumRtpPackets(), NumRtpPackets(header.ssrc), kTimeout);
  EXPECT_EQ_WAIT(NumRtpBytes(), NumRtpBytes(header.ssrc), kTimeout);
  EXPECT_EQ(1, NumSentSsrcs());
  EXPECT_EQ(0, NumRtpPackets(kSsrc - 1));
  EXPECT_EQ(0, NumRtpBytes(kSsrc - 1));
}

// Test that we can set the SSRC even after codecs are set.
TEST_F(WebRtcVideoChannelBaseTest, SetSendSsrcAfterSetCodecs) {
  // Remove stream added in Setup.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
  EXPECT_TRUE(SetDefaultCodec());
  EXPECT_TRUE(
      channel_->AddSendStream(cricket::StreamParams::CreateLegacy(999)));
  EXPECT_TRUE(channel_->SetVideoSend(999u, nullptr, video_capturer_.get()));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(WaitAndSendFrame(0));
  EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
  webrtc::RTPHeader header;
  std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
  EXPECT_TRUE(ParseRtpPacket(p.get(), &header));
  EXPECT_EQ(999u, header.ssrc);
  // Packets are being paced out, so these can mismatch between the first and
  // second call to NumRtpPackets until pending packets are paced out.
  EXPECT_EQ_WAIT(NumRtpPackets(), NumRtpPackets(header.ssrc), kTimeout);
  EXPECT_EQ_WAIT(NumRtpBytes(), NumRtpBytes(header.ssrc), kTimeout);
  EXPECT_EQ(1, NumSentSsrcs());
  EXPECT_EQ(0, NumRtpPackets(kSsrc));
  EXPECT_EQ(0, NumRtpBytes(kSsrc));
}

// Test that we can set the default video renderer before and after
// media is received.
TEST_F(WebRtcVideoChannelBaseTest, SetSink) {
  uint8_t data1[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  rtc::CopyOnWriteBuffer packet1(data1, sizeof(data1));
  rtc::SetBE32(packet1.data() + 8, kSsrc);
  channel_->SetSink(kDefaultReceiveSsrc, NULL);
  EXPECT_TRUE(SetDefaultCodec());
  EXPECT_TRUE(SetSend(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  channel_->OnPacketReceived(&packet1, /* packet_time_us */ -1);
  EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
}

// Tests setting up and configuring a send stream.
TEST_F(WebRtcVideoChannelBaseTest, AddRemoveSendStreams) {
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
  EXPECT_GT(NumRtpPackets(), 0);
  webrtc::RTPHeader header;
  size_t last_packet = NumRtpPackets() - 1;
  std::unique_ptr<const rtc::CopyOnWriteBuffer> p(
      GetRtpPacket(static_cast<int>(last_packet)));
  EXPECT_TRUE(ParseRtpPacket(p.get(), &header));
  EXPECT_EQ(kSsrc, header.ssrc);

  // Remove the send stream that was added during Setup.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
  int rtp_packets = NumRtpPackets();

  EXPECT_TRUE(
      channel_->AddSendStream(cricket::StreamParams::CreateLegacy(789u)));
  EXPECT_TRUE(channel_->SetVideoSend(789u, nullptr, video_capturer_.get()));
  EXPECT_EQ(rtp_packets, NumRtpPackets());
  // Wait 30ms to guarantee the engine does not drop the frame.
  EXPECT_TRUE(WaitAndSendFrame(30));
  EXPECT_TRUE_WAIT(NumRtpPackets() > rtp_packets, kTimeout);

  last_packet = NumRtpPackets() - 1;
  p.reset(GetRtpPacket(static_cast<int>(last_packet)));
  EXPECT_TRUE(ParseRtpPacket(p.get(), &header));
  EXPECT_EQ(789u, header.ssrc);
}

// Tests the behavior of incoming streams in a conference scenario.
TEST_F(WebRtcVideoChannelBaseTest, SimulateConference) {
  cricket::FakeVideoRenderer renderer1, renderer2;
  EXPECT_TRUE(SetDefaultCodec());
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(DefaultCodec());
  parameters.conference_mode = true;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_TRUE(channel_->SetSink(1, &renderer1));
  EXPECT_TRUE(channel_->SetSink(2, &renderer2));
  EXPECT_EQ(0, renderer1.num_rendered_frames());
  EXPECT_EQ(0, renderer2.num_rendered_frames());
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(1);
  ssrcs.push_back(2);
  network_interface_.SetConferenceMode(true, ssrcs);
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1, kVideoWidth, kVideoHeight,
                                kTimeout);
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, kVideoWidth, kVideoHeight,
                                kTimeout);

  std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
  EXPECT_EQ(DefaultCodec().id, GetPayloadType(p.get()));
  EXPECT_EQ(kVideoWidth, renderer1.width());
  EXPECT_EQ(kVideoHeight, renderer1.height());
  EXPECT_EQ(kVideoWidth, renderer2.width());
  EXPECT_EQ(kVideoHeight, renderer2.height());
  EXPECT_TRUE(channel_->RemoveRecvStream(2));
  EXPECT_TRUE(channel_->RemoveRecvStream(1));
}

// Tests that we can add and remove capturers and frames are sent out properly
TEST_F(WebRtcVideoChannelBaseTest, DISABLED_AddRemoveCapturer) {
  using cricket::VideoCodec;
  using cricket::VideoOptions;
  using cricket::VideoFormat;
  using cricket::FOURCC_I420;

  VideoCodec codec = DefaultCodec();
  const int time_between_send_ms = VideoFormat::FpsToInterval(kFramerate);
  EXPECT_TRUE(SetOneCodec(codec));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
  std::unique_ptr<cricket::FakeVideoCapturerWithTaskQueue> capturer(
      CreateFakeVideoCapturer());

  // TODO(nisse): This testcase fails if we don't configure
  // screencast. It's unclear why, I see nothing obvious in this
  // test which is related to screencast logic.
  VideoOptions video_options;
  video_options.is_screencast = true;
  channel_->SetVideoSend(kSsrc, &video_options, nullptr);

  VideoFormat format(480, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  EXPECT_EQ(cricket::CS_RUNNING, capturer->Start(format));
  // All capturers start generating frames with the same timestamp. ViE does
  // not allow the same timestamp to be used. Capture one frame before
  // associating the capturer with the channel.
  EXPECT_TRUE(capturer->CaptureCustomFrame(format.width, format.height));

  int captured_frames = 1;
  for (int iterations = 0; iterations < 2; ++iterations) {
    EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, capturer.get()));
    rtc::Thread::Current()->ProcessMessages(time_between_send_ms);
    EXPECT_TRUE(capturer->CaptureCustomFrame(format.width, format.height));
    ++captured_frames;
    // Wait until frame of right size is captured.
    EXPECT_TRUE_WAIT(renderer_.num_rendered_frames() >= captured_frames &&
                         format.width == renderer_.width() &&
                         format.height == renderer_.height() &&
                         !renderer_.black_frame(),
                     kTimeout);
    EXPECT_GE(renderer_.num_rendered_frames(), captured_frames);
    EXPECT_EQ(format.width, renderer_.width());
    EXPECT_EQ(format.height, renderer_.height());
    captured_frames = renderer_.num_rendered_frames() + 1;
    EXPECT_FALSE(renderer_.black_frame());
    EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, nullptr));
    // Make sure a black frame is generated within the specified timeout.
    // The black frame should be the resolution of the previous frame to
    // prevent expensive encoder reconfigurations.
    EXPECT_TRUE_WAIT(renderer_.num_rendered_frames() >= captured_frames &&
                         format.width == renderer_.width() &&
                         format.height == renderer_.height() &&
                         renderer_.black_frame(),
                     kTimeout);
    EXPECT_GE(renderer_.num_rendered_frames(), captured_frames);
    EXPECT_EQ(format.width, renderer_.width());
    EXPECT_EQ(format.height, renderer_.height());
    EXPECT_TRUE(renderer_.black_frame());

    // The black frame has the same timestamp as the next frame since it's
    // timestamp is set to the last frame's timestamp + interval. WebRTC will
    // not render a frame with the same timestamp so capture another frame
    // with the frame capturer to increment the next frame's timestamp.
    EXPECT_TRUE(capturer->CaptureCustomFrame(format.width, format.height));
  }
}

// Tests that if SetVideoSend is called with a NULL capturer after the
// capturer was already removed, the application doesn't crash (and no black
// frame is sent).
TEST_F(WebRtcVideoChannelBaseTest, RemoveCapturerWithoutAdd) {
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
  // Wait for one frame so they don't get dropped because we send frames too
  // tightly.
  rtc::Thread::Current()->ProcessMessages(30);
  // Remove the capturer.
  EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, nullptr));

  // No capturer was added, so this SetVideoSend shouldn't do anything.
  EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, nullptr));
  rtc::Thread::Current()->ProcessMessages(300);
  // Verify no more frames were sent.
  EXPECT_EQ(1, renderer_.num_rendered_frames());
}

// Tests that we can add and remove capturer as unique sources.
TEST_F(WebRtcVideoChannelBaseTest, AddRemoveCapturerMultipleSources) {
  // WebRTC implementation will drop frames if pushed to quickly. Wait the
  // interval time to avoid that.
  // WebRTC implementation will drop frames if pushed to quickly. Wait the
  // interval time to avoid that.
  // Set up the stream associated with the engine.
  EXPECT_TRUE(
      channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  EXPECT_TRUE(channel_->SetSink(kSsrc, &renderer_));
  cricket::VideoFormat capture_format(
      kVideoWidth, kVideoHeight,
      cricket::VideoFormat::FpsToInterval(kFramerate), cricket::FOURCC_I420);
  // Set up additional stream 1.
  cricket::FakeVideoRenderer renderer1;
  EXPECT_FALSE(channel_->SetSink(1, &renderer1));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->SetSink(1, &renderer1));
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  std::unique_ptr<cricket::FakeVideoCapturerWithTaskQueue> capturer1(
      CreateFakeVideoCapturer());
  EXPECT_EQ(cricket::CS_RUNNING, capturer1->Start(capture_format));
  // Set up additional stream 2.
  cricket::FakeVideoRenderer renderer2;
  EXPECT_FALSE(channel_->SetSink(2, &renderer2));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_TRUE(channel_->SetSink(2, &renderer2));
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(2)));
  std::unique_ptr<cricket::FakeVideoCapturerWithTaskQueue> capturer2(
      CreateFakeVideoCapturer());
  EXPECT_EQ(cricket::CS_RUNNING, capturer2->Start(capture_format));
  // State for all the streams.
  EXPECT_TRUE(SetOneCodec(DefaultCodec()));
  // A limitation in the lmi implementation requires that SetVideoSend() is
  // called after SetOneCodec().
  // TODO(hellner): this seems like an unnecessary constraint, fix it.
  EXPECT_TRUE(channel_->SetVideoSend(1, nullptr, capturer1.get()));
  EXPECT_TRUE(channel_->SetVideoSend(2, nullptr, capturer2.get()));
  EXPECT_TRUE(SetSend(true));
  // Test capturer associated with engine.
  const int kTestWidth = 160;
  const int kTestHeight = 120;
  EXPECT_TRUE(capturer1->CaptureCustomFrame(kTestWidth, kTestHeight));
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1, kTestWidth, kTestHeight,
                                kTimeout);
  // Capture a frame with additional capturer2, frames should be received
  EXPECT_TRUE(capturer2->CaptureCustomFrame(kTestWidth, kTestHeight));
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, kTestWidth, kTestHeight,
                                kTimeout);
  // Successfully remove the capturer.
  EXPECT_TRUE(channel_->SetVideoSend(kSsrc, nullptr, nullptr));
  // The capturers must be unregistered here as it runs out of it's scope
  // next.
  EXPECT_TRUE(channel_->SetVideoSend(1, nullptr, nullptr));
  EXPECT_TRUE(channel_->SetVideoSend(2, nullptr, nullptr));
}

// Tests empty StreamParams is rejected.
TEST_F(WebRtcVideoChannelBaseTest, RejectEmptyStreamParams) {
  // Remove the send stream that was added during Setup.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

  cricket::StreamParams empty;
  EXPECT_FALSE(channel_->AddSendStream(empty));
  EXPECT_TRUE(
      channel_->AddSendStream(cricket::StreamParams::CreateLegacy(789u)));
}

// Test that multiple send streams can be created and deleted properly.
TEST_F(WebRtcVideoChannelBaseTest, MultipleSendStreams) {
  // Remove stream added in Setup. I.e. remove stream corresponding to default
  // channel.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
  const unsigned int kSsrcsSize = sizeof(kSsrcs4) / sizeof(kSsrcs4[0]);
  for (unsigned int i = 0; i < kSsrcsSize; ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs4[i])));
  }
  // Delete one of the non default channel streams, let the destructor delete
  // the remaining ones.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs4[kSsrcsSize - 1]));
  // Stream should already be deleted.
  EXPECT_FALSE(channel_->RemoveSendStream(kSsrcs4[kSsrcsSize - 1]));
}

TEST_F(WebRtcVideoChannelBaseTest, SendAndReceiveVp8Vga) {
  SendAndReceive(GetEngineCodec("VP8"));
}

TEST_F(WebRtcVideoChannelBaseTest, SendAndReceiveVp8Qvga) {
  SendAndReceive(GetEngineCodec("VP8"));
}

TEST_F(WebRtcVideoChannelBaseTest, SendAndReceiveVp8SvcQqvga) {
  SendAndReceive(GetEngineCodec("VP8"));
}

TEST_F(WebRtcVideoChannelBaseTest, TwoStreamsSendAndReceive) {
  // Set a high bitrate to not be downscaled by VP8 due to low initial start
  // bitrates. This currently happens at <250k, and two streams sharing 300k
  // initially will use QVGA instead of VGA.
  // TODO(pbos): Set up the quality scaler so that both senders reliably start
  // at QVGA, then verify that instead.
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  codec.params[kCodecParamStartBitrate] = "1000000";
  TwoStreamsSendAndReceive(codec);
}

class WebRtcVideoChannelTest : public WebRtcVideoEngineTest {
 public:
  WebRtcVideoChannelTest() : WebRtcVideoChannelTest("") {}
  explicit WebRtcVideoChannelTest(const char* field_trials)
      : WebRtcVideoEngineTest(field_trials), last_ssrc_(0) {}
  void SetUp() override {
    encoder_factory_->AddSupportedVideoCodecType("VP8");
    encoder_factory_->AddSupportedVideoCodecType("VP9");
#if defined(WEBRTC_USE_H264)
    encoder_factory_->AddSupportedVideoCodecType("H264");
#endif

    fake_call_.reset(new FakeCall());
    channel_.reset(engine_.CreateMediaChannel(fake_call_.get(),
                                              GetMediaConfig(), VideoOptions(),
                                              webrtc::CryptoOptions()));
    channel_->OnReadyToSend(true);
    last_ssrc_ = 123;
    send_parameters_.codecs = engine_.codecs();
    recv_parameters_.codecs = engine_.codecs();
    ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  }

 protected:
  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(++last_ssrc_));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams = fake_call_->GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_call_->GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_call_->GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(++last_ssrc_));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams = fake_call_->GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_->GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  void SetSendCodecsShouldWorkForBitrates(const char* min_bitrate_kbps,
                                          int expected_min_bitrate_bps,
                                          const char* start_bitrate_kbps,
                                          int expected_start_bitrate_bps,
                                          const char* max_bitrate_kbps,
                                          int expected_max_bitrate_bps) {
    ExpectSetBitrateParameters(expected_min_bitrate_bps,
                               expected_start_bitrate_bps,
                               expected_max_bitrate_bps);
    auto& codecs = send_parameters_.codecs;
    codecs.clear();
    codecs.push_back(GetEngineCodec("VP8"));
    codecs[0].params[kCodecParamMinBitrate] = min_bitrate_kbps;
    codecs[0].params[kCodecParamStartBitrate] = start_bitrate_kbps;
    codecs[0].params[kCodecParamMaxBitrate] = max_bitrate_kbps;
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  }

  void ExpectSetBitrateParameters(int min_bitrate_bps,
                                  int start_bitrate_bps,
                                  int max_bitrate_bps) {
    EXPECT_CALL(
        *fake_call_->GetMockTransportControllerSend(),
        SetSdpBitrateParameters(AllOf(
            Field(&BitrateConstraints::min_bitrate_bps, min_bitrate_bps),
            Field(&BitrateConstraints::start_bitrate_bps, start_bitrate_bps),
            Field(&BitrateConstraints::max_bitrate_bps, max_bitrate_bps))));
  }

  void ExpectSetMaxBitrate(int max_bitrate_bps) {
    EXPECT_CALL(*fake_call_->GetMockTransportControllerSend(),
                SetSdpBitrateParameters(Field(
                    &BitrateConstraints::max_bitrate_bps, max_bitrate_bps)));
  }

  void TestExtmapAllowMixedCaller(bool extmap_allow_mixed) {
    // For a caller, the answer will be applied in set remote description
    // where SetSendParameters() is called.
    EXPECT_TRUE(
        channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
    send_parameters_.extmap_allow_mixed = extmap_allow_mixed;
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    const webrtc::VideoSendStream::Config& config =
        fake_call_->GetVideoSendStreams()[0]->GetConfig();
    EXPECT_EQ(extmap_allow_mixed, config.rtp.extmap_allow_mixed);
  }

  void TestExtmapAllowMixedCallee(bool extmap_allow_mixed) {
    // For a callee, the answer will be applied in set local description
    // where SetExtmapAllowMixed() and AddSendStream() are called.
    channel_->SetExtmapAllowMixed(extmap_allow_mixed);
    EXPECT_TRUE(
        channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
    const webrtc::VideoSendStream::Config& config =
        fake_call_->GetVideoSendStreams()[0]->GetConfig();
    EXPECT_EQ(extmap_allow_mixed, config.rtp.extmap_allow_mixed);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& ext_uri) {
    // Enable extension.
    const int id = 1;
    cricket::VideoSendParameters parameters = send_parameters_;
    parameters.extensions.push_back(RtpExtension(ext_uri, id));
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the send extension id.
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, send_stream->GetConfig().rtp.extensions[0].uri);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    // Verify that SetSendRtpHeaderExtensions doesn't implicitly add them for
    // receivers.
    EXPECT_TRUE(AddRecvStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
    send_stream = fake_call_->GetVideoSendStreams()[0];
    EXPECT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    send_stream = fake_call_->GetVideoSendStreams()[0];
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, send_stream->GetConfig().rtp.extensions[0].uri);
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& ext_uri) {
    // Enable extension.
    const int id = 1;
    cricket::VideoRecvParameters parameters = recv_parameters_;
    parameters.extensions.push_back(RtpExtension(ext_uri, id));
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));

    FakeVideoReceiveStream* recv_stream =
        AddRecvStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the recv extension id.
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, recv_stream->GetConfig().rtp.extensions[0].uri);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));

    // Verify that SetRecvRtpHeaderExtensions doesn't implicitly add them for
    // senders.
    EXPECT_TRUE(AddSendStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
    recv_stream = fake_call_->GetVideoReceiveStreams()[0];
    EXPECT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    recv_stream = fake_call_->GetVideoReceiveStreams()[0];
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, recv_stream->GetConfig().rtp.extensions[0].uri);
  }

  void TestExtensionFilter(const std::vector<std::string>& extensions,
                           const std::string& expected_extension) {
    cricket::VideoSendParameters parameters = send_parameters_;
    int expected_id = -1;
    int id = 1;
    for (const std::string& extension : extensions) {
      if (extension == expected_extension)
        expected_id = id;
      parameters.extensions.push_back(RtpExtension(extension, id++));
    }
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify that only one of them has been set, and that it is the one with
    // highest priority (transport sequence number).
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(expected_id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(expected_extension,
              send_stream->GetConfig().rtp.extensions[0].uri);
  }

  void TestDegradationPreference(bool resolution_scaling_enabled,
                                 bool fps_scaling_enabled);

  void TestCpuAdaptation(bool enable_overuse, bool is_screenshare);
  void TestReceiverLocalSsrcConfiguration(bool receiver_first);
  void TestReceiveUnsignaledSsrcPacket(uint8_t payload_type,
                                       bool expect_created_receive_stream);

  FakeVideoSendStream* SetDenoisingOption(
      uint32_t ssrc,
      FakeVideoCapturerWithTaskQueue* capturer,
      bool enabled) {
    cricket::VideoOptions options;
    options.video_noise_reduction = enabled;
    EXPECT_TRUE(channel_->SetVideoSend(ssrc, &options, capturer));
    // Options only take effect on the next frame.
    EXPECT_TRUE(capturer->CaptureFrame());

    return fake_call_->GetVideoSendStreams().back();
  }

  FakeVideoSendStream* SetUpSimulcast(bool enabled, bool with_rtx) {
    const int kRtxSsrcOffset = 0xDEADBEEF;
    last_ssrc_ += 3;
    std::vector<uint32_t> ssrcs;
    std::vector<uint32_t> rtx_ssrcs;
    uint32_t num_streams = enabled ? 3 : 1;
    for (uint32_t i = 0; i < num_streams; ++i) {
      uint32_t ssrc = last_ssrc_ + i;
      ssrcs.push_back(ssrc);
      if (with_rtx) {
        rtx_ssrcs.push_back(ssrc + kRtxSsrcOffset);
      }
    }
    if (with_rtx) {
      return AddSendStream(
          cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
    }
    return AddSendStream(CreateSimStreamParams("cname", ssrcs));
  }

  int GetMaxEncoderBitrate() {
    std::vector<FakeVideoSendStream*> streams =
        fake_call_->GetVideoSendStreams();
    EXPECT_EQ(1u, streams.size());
    FakeVideoSendStream* stream = streams[streams.size() - 1];
    EXPECT_EQ(1u, stream->GetEncoderConfig().number_of_streams);
    return stream->GetVideoStreams()[0].max_bitrate_bps;
  }

  void SetAndExpectMaxBitrate(int global_max,
                              int stream_max,
                              int expected_encoder_bitrate) {
    VideoSendParameters limited_send_params = send_parameters_;
    limited_send_params.max_bandwidth_bps = global_max;
    EXPECT_TRUE(channel_->SetSendParameters(limited_send_params));
    webrtc::RtpParameters parameters =
        channel_->GetRtpSendParameters(last_ssrc_);
    EXPECT_EQ(1UL, parameters.encodings.size());
    parameters.encodings[0].max_bitrate_bps = stream_max;
    EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
    // Read back the parameteres and verify they have the correct value
    parameters = channel_->GetRtpSendParameters(last_ssrc_);
    EXPECT_EQ(1UL, parameters.encodings.size());
    EXPECT_EQ(stream_max, parameters.encodings[0].max_bitrate_bps);
    // Verify that the new value propagated down to the encoder
    EXPECT_EQ(expected_encoder_bitrate, GetMaxEncoderBitrate());
  }

  // Values from kSimulcastConfigs in simulcast.cc.
  const std::vector<webrtc::VideoStream> GetSimulcastBitrates720p() const {
    std::vector<webrtc::VideoStream> layers(3);
    layers[0].min_bitrate_bps = 30000;
    layers[0].target_bitrate_bps = 150000;
    layers[0].max_bitrate_bps = 200000;
    layers[1].min_bitrate_bps = 150000;
    layers[1].target_bitrate_bps = 500000;
    layers[1].max_bitrate_bps = 700000;
    layers[2].min_bitrate_bps = 600000;
    layers[2].target_bitrate_bps = 2500000;
    layers[2].max_bitrate_bps = 2500000;
    return layers;
  }

  std::unique_ptr<FakeCall> fake_call_;
  std::unique_ptr<VideoMediaChannel> channel_;
  cricket::VideoSendParameters send_parameters_;
  cricket::VideoRecvParameters recv_parameters_;
  uint32_t last_ssrc_;
};

TEST_F(WebRtcVideoChannelTest, SetsSyncGroupFromSyncLabel) {
  const uint32_t kVideoSsrc = 123;
  const std::string kSyncLabel = "AvSyncLabel";

  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(kVideoSsrc);
  sp.set_stream_ids({kSyncLabel});
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(kSyncLabel,
            fake_call_->GetVideoReceiveStreams()[0]->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
}

TEST_F(WebRtcVideoChannelTest, RecvStreamWithSimAndRtx) {
  cricket::VideoSendParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));
  parameters.conference_mode = true;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  // Send side.
  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* send_stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));

  ASSERT_EQ(rtx_ssrcs.size(), send_stream->GetConfig().rtp.rtx.ssrcs.size());
  for (size_t i = 0; i < rtx_ssrcs.size(); ++i)
    EXPECT_EQ(rtx_ssrcs[i], send_stream->GetConfig().rtp.rtx.ssrcs[i]);

  // Receiver side.
  FakeVideoReceiveStream* recv_stream = AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  EXPECT_FALSE(
      recv_stream->GetConfig().rtp.rtx_associated_payload_types.empty());
  EXPECT_TRUE(VerifyRtxReceiveAssociations(recv_stream->GetConfig()))
      << "RTX should be mapped for all decoders/payload types.";
  EXPECT_TRUE(HasRtxReceiveAssociation(recv_stream->GetConfig(),
                                       GetEngineCodec("red").id))
      << "RTX should be mapped for the RED payload type";

  EXPECT_EQ(rtx_ssrcs[0], recv_stream->GetConfig().rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, RecvStreamWithRtx) {
  // Setup one channel with an associated RTX stream.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  FakeVideoReceiveStream* recv_stream = AddRecvStream(params);
  EXPECT_EQ(kRtxSsrcs1[0], recv_stream->GetConfig().rtp.rtx_ssrc);

  EXPECT_TRUE(VerifyRtxReceiveAssociations(recv_stream->GetConfig()))
      << "RTX should be mapped for all decoders/payload types.";
  EXPECT_TRUE(HasRtxReceiveAssociation(recv_stream->GetConfig(),
                                       GetEngineCodec("red").id))
      << "RTX should be mapped for the RED payload type";
}

TEST_F(WebRtcVideoChannelTest, RecvStreamNoRtx) {
  // Setup one channel without an associated RTX stream.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  FakeVideoReceiveStream* recv_stream = AddRecvStream(params);
  ASSERT_EQ(0U, recv_stream->GetConfig().rtp.rtx_ssrc);
}

// Test propagation of extmap allow mixed setting.
TEST_F(WebRtcVideoChannelTest, SetExtmapAllowMixedAsCaller) {
  TestExtmapAllowMixedCaller(/*extmap_allow_mixed=*/true);
}
TEST_F(WebRtcVideoChannelTest, SetExtmapAllowMixedDisabledAsCaller) {
  TestExtmapAllowMixedCaller(/*extmap_allow_mixed=*/false);
}
TEST_F(WebRtcVideoChannelTest, SetExtmapAllowMixedAsCallee) {
  TestExtmapAllowMixedCallee(/*extmap_allow_mixed=*/true);
}
TEST_F(WebRtcVideoChannelTest, SetExtmapAllowMixedDisabledAsCallee) {
  TestExtmapAllowMixedCallee(/*extmap_allow_mixed=*/false);
}

TEST_F(WebRtcVideoChannelTest, NoHeaderExtesionsByDefault) {
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(kSsrcs1[0]));
  ASSERT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrcs1[0]));
  ASSERT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());
}

// Test support for RTP timestamp offset header extension.
TEST_F(WebRtcVideoChannelTest, SendRtpTimestampOffsetHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kTimestampOffsetUri);
}

TEST_F(WebRtcVideoChannelTest, RecvRtpTimestampOffsetHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kTimestampOffsetUri);
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVideoChannelTest, SendAbsoluteSendTimeHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kAbsSendTimeUri);
}

TEST_F(WebRtcVideoChannelTest, RecvAbsoluteSendTimeHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kAbsSendTimeUri);
}

TEST_F(WebRtcVideoChannelTest, FiltersExtensionsPicksTransportSeqNum) {
  // Enable three redundant extensions.
  std::vector<std::string> extensions;
  extensions.push_back(RtpExtension::kAbsSendTimeUri);
  extensions.push_back(RtpExtension::kTimestampOffsetUri);
  extensions.push_back(RtpExtension::kTransportSequenceNumberUri);
  TestExtensionFilter(extensions, RtpExtension::kTransportSequenceNumberUri);
}

TEST_F(WebRtcVideoChannelTest, FiltersExtensionsPicksAbsSendTime) {
  // Enable two redundant extensions.
  std::vector<std::string> extensions;
  extensions.push_back(RtpExtension::kAbsSendTimeUri);
  extensions.push_back(RtpExtension::kTimestampOffsetUri);
  TestExtensionFilter(extensions, RtpExtension::kAbsSendTimeUri);
}

// Test support for transport sequence number header extension.
TEST_F(WebRtcVideoChannelTest, SendTransportSequenceNumberHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kTransportSequenceNumberUri);
}
TEST_F(WebRtcVideoChannelTest, RecvTransportSequenceNumberHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kTransportSequenceNumberUri);
}

// Test support for video rotation header extension.
TEST_F(WebRtcVideoChannelTest, SendVideoRotationHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kVideoRotationUri);
}
TEST_F(WebRtcVideoChannelTest, RecvVideoRotationHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kVideoRotationUri);
}

TEST_F(WebRtcVideoChannelTest, IdenticalSendExtensionsDoesntRecreateStream) {
  const int kAbsSendTimeId = 1;
  const int kVideoRotationId = 2;
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeId));
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, kVideoRotationId));

  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
  ASSERT_EQ(2u, send_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(send_parameters_.extensions.begin(),
               send_parameters_.extensions.end());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());

  // Setting different extensions should recreate the stream.
  send_parameters_.extensions.resize(1);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_EQ(2, fake_call_->GetNumCreatedSendStreams());
}

TEST_F(WebRtcVideoChannelTest, IdenticalRecvExtensionsDoesntRecreateStream) {
  const int kTOffsetId = 1;
  const int kAbsSendTimeId = 2;
  const int kVideoRotationId = 3;
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeId));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, kTOffsetId));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, kVideoRotationId));

  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
  ASSERT_EQ(3u, recv_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(recv_parameters_.extensions.begin(),
               recv_parameters_.extensions.end());
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  // Setting different extensions should recreate the stream.
  recv_parameters_.extensions.resize(1);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannelTest,
       SetSendRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  send_parameters_.extensions.push_back(
      RtpExtension(kUnsupportedExtensionName, kUnsupportedId));
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, kTOffsetId));
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to send stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(RtpExtension::kTimestampOffsetUri,
               send_stream->GetConfig().rtp.extensions[0].uri.c_str());
}

TEST_F(WebRtcVideoChannelTest,
       SetRecvRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  recv_parameters_.extensions.push_back(
      RtpExtension(kUnsupportedExtensionName, kUnsupportedId));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, kTOffsetId));
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to receive stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(RtpExtension::kTimestampOffsetUri,
               recv_stream->GetConfig().rtp.extensions[0].uri.c_str());
}

TEST_F(WebRtcVideoChannelTest, SetSendRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 0, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    send_parameters_.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetSendParameters(send_parameters_))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetRecvRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 0, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    recv_parameters_.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetSendRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, id));
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));

  // Duplicate entries are also not supported.
  send_parameters_.extensions.clear();
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  send_parameters_.extensions.push_back(send_parameters_.extensions.back());
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannelTest, SetRecvRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, id));
  EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_));

  // Duplicate entries are also not supported.
  recv_parameters_.extensions.clear();
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  recv_parameters_.extensions.push_back(recv_parameters_.extensions.back());
  EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_));
}

TEST_F(WebRtcVideoChannelTest, AddRecvStreamOnlyUsesOneReceiveStream) {
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
}

TEST_F(WebRtcVideoChannelTest, RtcpIsCompoundByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream->GetConfig().rtp.rtcp_mode);
}

TEST_F(WebRtcVideoChannelTest, RembIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannelTest, TransportCcIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);
}

TEST_F(WebRtcVideoChannelTest, RembCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned off when send(!) codecs without REMB are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(RemoveFeedbackParams(GetEngineCodec("VP8")));
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned on when setting default codecs since the
  // default codecs have REMB enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannelTest, TransportCcCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);

  // Verify that transport cc feedback is turned off when send(!) codecs without
  // transport cc feedback are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(RemoveFeedbackParams(GetEngineCodec("VP8")));
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.transport_cc);

  // Verify that transport cc feedback is turned on when setting default codecs
  // since the default codecs have transport cc feedback enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);
}

TEST_F(WebRtcVideoChannelTest, NackIsEnabledByDefault) {
  AssignDefaultCodec();
  VerifyCodecHasDefaultFeedbackParams(default_codec_);

  cricket::VideoSendParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));

  // Send side.
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Receiver side.
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Nack history size should match between sender and receiver.
  EXPECT_EQ(send_stream->GetConfig().rtp.nack.rtp_history_ms,
            recv_stream->GetConfig().rtp.nack.rtp_history_ms);
}

TEST_F(WebRtcVideoChannelTest, NackCanBeEnabledAndDisabled) {
  FakeVideoSendStream* send_stream = AddSendStream();
  FakeVideoReceiveStream* recv_stream = AddRecvStream();

  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Verify that NACK is turned off when send(!) codecs without NACK are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(RemoveFeedbackParams(GetEngineCodec("VP8")));
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(0, recv_stream->GetConfig().rtp.nack.rtp_history_ms);
  send_stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_EQ(0, send_stream->GetConfig().rtp.nack.rtp_history_ms);

  // Verify that NACK is turned on when setting default codecs since the
  // default codecs have NACK enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
  send_stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
}

// This test verifies that new frame sizes reconfigures encoders even though not
// (yet) sending. The purpose of this is to permit encoding as quickly as
// possible once we start sending. Likely the frames being input are from the
// same source that will be sent later, which just means that we're ready
// earlier.
TEST_F(WebRtcVideoChannelTest, ReconfiguresEncodersWhenNotSending) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(false);

  FakeVideoSendStream* stream = AddSendStream();

  // No frames entered.
  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  EXPECT_EQ(0u, streams[0].width);
  EXPECT_EQ(0u, streams[0].height);

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  VideoFormat capture_format = capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format));
  EXPECT_TRUE(capturer.CaptureFrame());

  // Frame entered, should be reconfigured to new dimensions.
  streams = stream->GetVideoStreams();
  EXPECT_EQ(rtc::checked_cast<size_t>(capture_format.width), streams[0].width);
  EXPECT_EQ(rtc::checked_cast<size_t>(capture_format.height),
            streams[0].height);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, UsesCorrectSettingsForScreencast) {
  static const int kScreenshareMinBitrateKbps = 800;
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions min_bitrate_options;
  min_bitrate_options.screencast_min_bitrate_kbps = kScreenshareMinBitrateKbps;
  EXPECT_TRUE(
      channel_->SetVideoSend(last_ssrc_, &min_bitrate_options, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(1280, capture_format_hd.width);
  EXPECT_EQ(720, capture_format_hd.height);
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));

  EXPECT_TRUE(channel_->SetSend(true));

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  // Verify non-screencast settings.
  webrtc::VideoEncoderConfig encoder_config =
      send_stream->GetEncoderConfig().Copy();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo,
            encoder_config.content_type);
  std::vector<webrtc::VideoStream> streams = send_stream->GetVideoStreams();
  EXPECT_EQ(rtc::checked_cast<size_t>(capture_format_hd.width),
            streams.front().width);
  EXPECT_EQ(rtc::checked_cast<size_t>(capture_format_hd.height),
            streams.front().height);
  EXPECT_EQ(0, encoder_config.min_transmit_bitrate_bps)
      << "Non-screenshare shouldn't use min-transmit bitrate.";

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  VideoOptions screencast_options;
  screencast_options.is_screencast = true;
  EXPECT_TRUE(
      channel_->SetVideoSend(last_ssrc_, &screencast_options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Send stream recreated after option change.
  ASSERT_EQ(2, fake_call_->GetNumCreatedSendStreams());
  send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  // Verify screencast settings.
  encoder_config = send_stream->GetEncoderConfig().Copy();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kScreen,
            encoder_config.content_type);
  EXPECT_EQ(kScreenshareMinBitrateKbps * 1000,
            encoder_config.min_transmit_bitrate_bps);

  streams = send_stream->GetVideoStreams();
  EXPECT_EQ(rtc::checked_cast<size_t>(capture_format_hd.width),
            streams.front().width);
  EXPECT_EQ(rtc::checked_cast<size_t>(capture_format_hd.height),
            streams.front().height);
  EXPECT_FALSE(streams[0].num_temporal_layers.has_value());
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       ConferenceModeScreencastConfiguresTemporalLayer) {
  static const int kConferenceScreencastTemporalBitrateBps = 200 * 1000;
  send_parameters_.conference_mode = true;
  channel_->SetSendParameters(send_parameters_);

  AddSendStream();
  VideoOptions options;
  options.is_screencast = true;
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));

  EXPECT_TRUE(channel_->SetSend(true));

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  webrtc::VideoEncoderConfig encoder_config =
      send_stream->GetEncoderConfig().Copy();

  // Verify screencast settings.
  encoder_config = send_stream->GetEncoderConfig().Copy();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kScreen,
            encoder_config.content_type);

  std::vector<webrtc::VideoStream> streams = send_stream->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  ASSERT_EQ(2u, streams[0].num_temporal_layers);
  EXPECT_EQ(kConferenceScreencastTemporalBitrateBps,
            streams[0].target_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, SuspendBelowMinBitrateDisabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannelTest, SetMediaConfigSuspendBelowMinBitrate) {
  MediaConfig media_config = GetMediaConfig();
  media_config.video.suspend_below_min_bitrate = true;

  channel_.reset(engine_.CreateMediaChannel(
      fake_call_.get(), media_config, VideoOptions(), webrtc::CryptoOptions()));
  channel_->OnReadyToSend(true);

  channel_->SetSendParameters(send_parameters_);

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(stream->GetConfig().suspend_below_min_bitrate);

  media_config.video.suspend_below_min_bitrate = false;
  channel_.reset(engine_.CreateMediaChannel(
      fake_call_.get(), media_config, VideoOptions(), webrtc::CryptoOptions()));
  channel_->OnReadyToSend(true);

  channel_->SetSendParameters(send_parameters_);

  stream = AddSendStream();
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannelTest, Vp8DenoisingEnabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
}

TEST_F(WebRtcVideoChannelTest, VerifyVp8SpecificSettings) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  // Single-stream settings should apply with RTX as well (verifies that we
  // check number of regular SSRCs and not StreamParams::ssrcs which contains
  // both RTX and regular SSRCs).
  FakeVideoSendStream* stream = SetUpSimulcast(false, true);

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn)
      << "VP8 denoising should be on by default.";

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  EXPECT_TRUE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, true);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
  EXPECT_TRUE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
  stream = SetUpSimulcast(true, false);
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_EQ(3u, stream->GetVideoStreams().size());
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  // Autmatic resize off when using simulcast.
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  // In screen-share mode, denoising is forced off and simulcast disabled.
  VideoOptions options;
  options.is_screencast = true;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  EXPECT_EQ(1u, stream->GetVideoStreams().size());
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  // Resizing and frame dropping always off for screen sharing.
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_FALSE(vp8_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, true);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_FALSE(vp8_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

// Test that setting the same options doesn't result in the encoder being
// reconfigured.
TEST_F(WebRtcVideoChannelTest, SetIdenticalOptionsDoesntReconfigureEncoder) {
  VideoOptions options;
  FakeVideoCapturerWithTaskQueue capturer;

  AddSendStream();
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Expect 1 reconfigurations at this point from the initial configuration.
  EXPECT_EQ(1, send_stream->num_encoder_reconfigurations());

  // Set the options one more time and expect no additional reconfigurations.
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(1, send_stream->num_encoder_reconfigurations());

  // Change |options| and expect 2 reconfigurations.
  options.video_noise_reduction = true;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(2, send_stream->num_encoder_reconfigurations());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

class Vp9SettingsTest : public WebRtcVideoChannelTest {
 public:
  Vp9SettingsTest() : Vp9SettingsTest("") {}
  explicit Vp9SettingsTest(const char* field_trials)
      : WebRtcVideoChannelTest(field_trials) {
    encoder_factory_->AddSupportedVideoCodecType("VP9");
  }
  virtual ~Vp9SettingsTest() {}

 protected:
  void TearDown() override {
    // Remove references to encoder_factory_ since this will be destroyed
    // before channel_ and engine_.
    ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  }
};

TEST_F(Vp9SettingsTest, VerifyVp9SpecificSettings) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = SetUpSimulcast(false, false);

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP9 vp9_settings;
  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_TRUE(vp9_settings.denoisingOn)
      << "VP9 denoising should be on by default.";

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  // Frame dropping always on for real time video.
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, true);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_TRUE(vp9_settings.denoisingOn);
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  // In screen-share mode, denoising is forced off.
  VideoOptions options;
  options.is_screencast = true;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  // Frame dropping always on for screen sharing.
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(Vp9SettingsTest, MultipleSsrcsEnablesSvc) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  FakeVideoSendStream* stream =
      AddSendStream(CreateSimStreamParams("cname", ssrcs));

  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();
  EXPECT_EQ(ssrcs.size(), config.rtp.ssrcs.size());

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetVideoSend(ssrcs[0], nullptr, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP9 vp9_settings;
  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";

  const size_t kNumSpatialLayers = ssrcs.size();
  const size_t kNumTemporalLayers = 3;
  EXPECT_EQ(vp9_settings.numberOfSpatialLayers, kNumSpatialLayers);
  EXPECT_EQ(vp9_settings.numberOfTemporalLayers, kNumTemporalLayers);

  EXPECT_TRUE(channel_->SetVideoSend(ssrcs[0], nullptr, nullptr));
}

TEST_F(Vp9SettingsTest, AllEncodingParametersCopied) {
  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP9"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));

  const size_t kNumSpatialLayers = 3;
  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  FakeVideoSendStream* stream =
      AddSendStream(CreateSimStreamParams("cname", ssrcs));

  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(ssrcs[0]);
  ASSERT_EQ(kNumSpatialLayers, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  ASSERT_TRUE(parameters.encodings[1].active);
  ASSERT_TRUE(parameters.encodings[2].active);
  // Invert value to verify copying.
  parameters.encodings[1].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(ssrcs[0], parameters).ok());

  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();

  // number_of_streams should be 1 since all spatial layers are sent on the
  // same SSRC. But encoding parameters of all layers is supposed to be copied
  // and stored in simulcast_layers[].
  EXPECT_EQ(1u, encoder_config.number_of_streams);
  EXPECT_EQ(encoder_config.simulcast_layers.size(), kNumSpatialLayers);
  EXPECT_TRUE(encoder_config.simulcast_layers[0].active);
  EXPECT_FALSE(encoder_config.simulcast_layers[1].active);
  EXPECT_TRUE(encoder_config.simulcast_layers[2].active);
}

class Vp9SettingsTestWithFieldTrial : public Vp9SettingsTest {
 public:
  explicit Vp9SettingsTestWithFieldTrial(const char* field_trials)
      : Vp9SettingsTest(field_trials) {}

 protected:
  void VerifySettings(int num_spatial_layers, int num_temporal_layers) {
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(GetEngineCodec("VP9"));
    ASSERT_TRUE(channel_->SetSendParameters(parameters));

    FakeVideoSendStream* stream = SetUpSimulcast(false, false);

    FakeVideoCapturerWithTaskQueue capturer;
    EXPECT_EQ(cricket::CS_RUNNING,
              capturer.Start(capturer.GetSupportedFormats()->front()));
    EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
    channel_->SetSend(true);

    EXPECT_TRUE(capturer.CaptureFrame());

    webrtc::VideoCodecVP9 vp9_settings;
    ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
    EXPECT_EQ(num_spatial_layers, vp9_settings.numberOfSpatialLayers);
    EXPECT_EQ(num_temporal_layers, vp9_settings.numberOfTemporalLayers);

    EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
  }
};

class Vp9SettingsTestWithNoFlag : public Vp9SettingsTestWithFieldTrial {
 public:
  Vp9SettingsTestWithNoFlag() : Vp9SettingsTestWithFieldTrial("") {}
};

TEST_F(Vp9SettingsTestWithNoFlag, VerifySettings) {
  const int kNumSpatialLayers = 1;
  const int kNumTemporalLayers = 1;
  VerifySettings(kNumSpatialLayers, kNumTemporalLayers);
}

class Vp9SettingsTestWithInvalidFlag : public Vp9SettingsTestWithFieldTrial {
 public:
  Vp9SettingsTestWithInvalidFlag()
      : Vp9SettingsTestWithFieldTrial("WebRTC-SupportVP9SVC/Default/") {}
};

TEST_F(Vp9SettingsTestWithInvalidFlag, VerifySettings) {
  const int kNumSpatialLayers = 1;
  const int kNumTemporalLayers = 1;
  VerifySettings(kNumSpatialLayers, kNumTemporalLayers);
}

class Vp9SettingsTestWith2SL3TLFlag : public Vp9SettingsTestWithFieldTrial {
 public:
  Vp9SettingsTestWith2SL3TLFlag()
      : Vp9SettingsTestWithFieldTrial(
            "WebRTC-SupportVP9SVC/EnabledByFlag_2SL3TL/") {}
};

TEST_F(Vp9SettingsTestWith2SL3TLFlag, VerifySettings) {
  const int kNumSpatialLayers = 2;
  const int kNumTemporalLayers = 3;
  VerifySettings(kNumSpatialLayers, kNumTemporalLayers);
}

TEST_F(WebRtcVideoChannelTest, VerifyMinBitrate) {
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(cricket::kMinVideoBitrateBps, streams[0].min_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, VerifyMinBitrateWithForcedFallbackFieldTrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-VP8-Forced-Fallback-Encoder-v2/Enabled-1,2,34567/");
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(34567, streams[0].min_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       BalancedDegradationPreferenceNotSupportedWithoutFieldtrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-Video-BalancedDegradation/Disabled/");
  const bool kResolutionScalingEnabled = true;
  const bool kFpsScalingEnabled = false;
  TestDegradationPreference(kResolutionScalingEnabled, kFpsScalingEnabled);
}

TEST_F(WebRtcVideoChannelTest,
       BalancedDegradationPreferenceSupportedBehindFieldtrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-Video-BalancedDegradation/Enabled/");
  const bool kResolutionScalingEnabled = true;
  const bool kFpsScalingEnabled = true;
  TestDegradationPreference(kResolutionScalingEnabled, kFpsScalingEnabled);
}

TEST_F(WebRtcVideoChannelTest, AdaptsOnOveruse) {
  TestCpuAdaptation(true, false);
}

TEST_F(WebRtcVideoChannelTest, DoesNotAdaptOnOveruseWhenDisabled) {
  TestCpuAdaptation(false, false);
}

TEST_F(WebRtcVideoChannelTest, DoesNotAdaptOnOveruseWhenScreensharing) {
  TestCpuAdaptation(true, true);
}

TEST_F(WebRtcVideoChannelTest, AdaptsOnOveruseAndChangeResolution) {
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  channel_.reset(engine_.CreateMediaChannel(
      fake_call_.get(), media_config, VideoOptions(), webrtc::CryptoOptions()));
  channel_->OnReadyToSend(true);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  ASSERT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  ASSERT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  ASSERT_TRUE(channel_->SetSend(true));

  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720));
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280, send_stream->GetLastWidth());
  EXPECT_EQ(720, send_stream->GetLastHeight());

  // Trigger overuse.
  rtc::VideoSinkWants wants;
  wants.max_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight() - 1;
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720));
  EXPECT_EQ(2, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 3 / 4, send_stream->GetLastHeight());

  // Trigger overuse again.
  wants.max_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight() - 1;
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720));
  EXPECT_EQ(3, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 2 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 2 / 4, send_stream->GetLastHeight());

  // Change input resolution.
  EXPECT_TRUE(capturer.CaptureCustomFrame(1284, 724));
  EXPECT_EQ(4, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284 / 2, send_stream->GetLastWidth());
  EXPECT_EQ(724 / 2, send_stream->GetLastHeight());

  // Trigger underuse which should go back up in resolution.
  int current_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight();
  // Cap the max to 4x the pixel count (assuming max 1/2 x 1/2 scale downs)
  // of the current stream, so we don't take too large steps.
  wants.max_pixel_count = current_pixel_count * 4;
  // Default step down is 3/5 pixel count, so go up by 5/3.
  wants.target_pixel_count = (current_pixel_count * 5 / 3);
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1284, 724));
  EXPECT_EQ(5, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(724 * 3 / 4, send_stream->GetLastHeight());

  // Trigger underuse again, should go back up to full resolution.
  current_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight();
  wants.max_pixel_count = current_pixel_count * 4;
  wants.target_pixel_count = (current_pixel_count * 5 / 3);
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1284, 724));
  EXPECT_EQ(6, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284, send_stream->GetLastWidth());
  EXPECT_EQ(724, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, PreviousAdaptationDoesNotApplyToScreenshare) {
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  media_config.video.enable_cpu_adaptation = true;
  channel_.reset(engine_.CreateMediaChannel(
      fake_call_.get(), media_config, VideoOptions(), webrtc::CryptoOptions()));
  channel_->OnReadyToSend(true);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  ASSERT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  ASSERT_TRUE(channel_->SetSend(true));
  cricket::VideoOptions camera_options;
  camera_options.is_screencast = false;
  channel_->SetVideoSend(last_ssrc_, &camera_options, &capturer);

  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720));
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280, send_stream->GetLastWidth());
  EXPECT_EQ(720, send_stream->GetLastHeight());

  // Trigger overuse.
  rtc::VideoSinkWants wants;
  wants.max_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight() - 1;
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720));
  EXPECT_EQ(2, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 3 / 4, send_stream->GetLastHeight());

  // Switch to screen share. Expect no CPU adaptation.
  FakeVideoCapturerWithTaskQueue screen_share(true);
  ASSERT_EQ(cricket::CS_RUNNING,
            screen_share.Start(screen_share.GetSupportedFormats()->front()));
  cricket::VideoOptions screenshare_options;
  screenshare_options.is_screencast = true;
  channel_->SetVideoSend(last_ssrc_, &screenshare_options, &screen_share);
  EXPECT_TRUE(screen_share.CaptureCustomFrame(1284, 724));
  ASSERT_EQ(2, fake_call_->GetNumCreatedSendStreams());
  send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284, send_stream->GetLastWidth());
  EXPECT_EQ(724, send_stream->GetLastHeight());

  // Switch back to the normal capturer. Expect the frame to be CPU adapted.
  channel_->SetVideoSend(last_ssrc_, &camera_options, &capturer);
  send_stream = fake_call_->GetVideoSendStreams().front();
  // We have a new fake send stream, so it doesn't remember the old sink wants.
  // In practice, it will be populated from
  // VideoStreamEncoder::VideoSourceProxy::SetSource(), so simulate that here.
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720));
  ASSERT_EQ(3, fake_call_->GetNumCreatedSendStreams());
  send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 3 / 4, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

// TODO(asapersson): Remove this test when the balanced field trial is removed.
void WebRtcVideoChannelTest::TestDegradationPreference(
    bool resolution_scaling_enabled,
    bool fps_scaling_enabled) {
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  media_config.video.enable_cpu_adaptation = true;
  channel_.reset(engine_.CreateMediaChannel(
      fake_call_.get(), media_config, VideoOptions(), webrtc::CryptoOptions()));
  channel_->OnReadyToSend(true);

  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  cricket::VideoFormat capture_format = capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format));

  EXPECT_TRUE(channel_->SetSend(true));

  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(resolution_scaling_enabled,
            send_stream->resolution_scaling_enabled());
  EXPECT_EQ(fps_scaling_enabled, send_stream->framerate_scaling_enabled());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

void WebRtcVideoChannelTest::TestCpuAdaptation(bool enable_overuse,
                                               bool is_screenshare) {
  const int kDefaultFps = 30;
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  if (enable_overuse) {
    media_config.video.enable_cpu_adaptation = true;
  }
  channel_.reset(engine_.CreateMediaChannel(
      fake_call_.get(), media_config, VideoOptions(), webrtc::CryptoOptions()));
  channel_->OnReadyToSend(true);

  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  options.is_screencast = is_screenshare;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  cricket::VideoFormat capture_format = capturer.GetSupportedFormats()->front();
  capture_format.interval = rtc::kNumNanosecsPerSec / kDefaultFps;
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format));

  EXPECT_TRUE(channel_->SetSend(true));

  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  if (!enable_overuse) {
    EXPECT_FALSE(send_stream->resolution_scaling_enabled());
    EXPECT_FALSE(send_stream->framerate_scaling_enabled());
    EXPECT_EQ(is_screenshare, send_stream->framerate_scaling_enabled());

    EXPECT_TRUE(capturer.CaptureFrame());
    EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

    EXPECT_EQ(capture_format.width, send_stream->GetLastWidth());
    EXPECT_EQ(capture_format.height, send_stream->GetLastHeight());

    EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
    return;
  }

  if (is_screenshare) {
    EXPECT_FALSE(send_stream->resolution_scaling_enabled());
    EXPECT_TRUE(send_stream->framerate_scaling_enabled());
  } else {
    EXPECT_TRUE(send_stream->resolution_scaling_enabled());
    EXPECT_FALSE(send_stream->framerate_scaling_enabled());
  }

  // Trigger overuse.
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());

  rtc::VideoSinkWants wants;
  if (is_screenshare) {
    wants.max_framerate_fps = (kDefaultFps * 2) / 3;
  } else {
    wants.max_pixel_count = capture_format.width * capture_format.height - 1;
  }
  send_stream->InjectVideoSinkWants(wants);

  for (int i = 0; i < kDefaultFps; ++i)
    EXPECT_TRUE(capturer.CaptureFrame());

  if (is_screenshare) {
    // Drops every third frame.
    EXPECT_EQ(kDefaultFps * 2 / 3, send_stream->GetNumberOfSwappedFrames());
    EXPECT_EQ(send_stream->GetLastWidth(), capture_format.width);
    EXPECT_EQ(send_stream->GetLastHeight(), capture_format.height);
  } else {
    EXPECT_EQ(kDefaultFps, send_stream->GetNumberOfSwappedFrames());
    EXPECT_LT(send_stream->GetLastWidth(), capture_format.width);
    EXPECT_LT(send_stream->GetLastHeight(), capture_format.height);
  }

  // Trigger underuse which should go back to normal resolution.
  int last_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight();
  if (is_screenshare) {
    wants.max_framerate_fps = kDefaultFps;
  } else {
    wants.max_pixel_count = last_pixel_count * 4;
    wants.target_pixel_count.emplace((last_pixel_count * 5) / 3);
  }
  send_stream->InjectVideoSinkWants(wants);

  for (int i = 0; i < kDefaultFps; ++i)
    EXPECT_TRUE(capturer.CaptureFrame());

  if (is_screenshare) {
    EXPECT_EQ(kDefaultFps + (kDefaultFps * 2 / 3),
              send_stream->GetNumberOfSwappedFrames());
  } else {
    EXPECT_EQ(kDefaultFps * 2, send_stream->GetNumberOfSwappedFrames());
  }

  EXPECT_EQ(capture_format.width, send_stream->GetLastWidth());
  EXPECT_EQ(capture_format.height, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, EstimatesNtpStartTimeCorrectly) {
  // Start at last timestamp to verify that wraparounds are estimated correctly.
  static const uint32_t kInitialTimestamp = 0xFFFFFFFFu;
  static const int64_t kInitialNtpTimeMs = 1247891230;
  static const int kFrameOffsetMs = 20;
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  FakeVideoReceiveStream* stream = AddRecvStream();
  cricket::FakeVideoRenderer renderer;
  EXPECT_TRUE(channel_->SetSink(last_ssrc_, &renderer));

  webrtc::VideoFrame video_frame(CreateBlackFrameBuffer(4, 4),
                                 kInitialTimestamp, 0,
                                 webrtc::kVideoRotation_0);
  // Initial NTP time is not available on the first frame, but should still be
  // able to be estimated.
  stream->InjectFrame(video_frame);

  EXPECT_EQ(1, renderer.num_rendered_frames());

  // This timestamp is kInitialTimestamp (-1) + kFrameOffsetMs * 90, which
  // triggers a constant-overflow warning, hence we're calculating it explicitly
  // here.
  fake_clock_.AdvanceTimeMicros(kFrameOffsetMs * rtc::kNumMicrosecsPerMillisec);
  video_frame.set_timestamp(kFrameOffsetMs * 90 - 1);
  video_frame.set_ntp_time_ms(kInitialNtpTimeMs + kFrameOffsetMs);
  stream->InjectFrame(video_frame);

  EXPECT_EQ(2, renderer.num_rendered_frames());

  // Verify that NTP time has been correctly deduced.
  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.receivers.size());
  EXPECT_EQ(kInitialNtpTimeMs, info.receivers[0].capture_start_ntp_time_ms);
}

TEST_F(WebRtcVideoChannelTest, SetDefaultSendCodecs) {
  AssignDefaultAptRtxTypes();
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_TRUE(codec.Matches(engine_.codecs()[0]));

  // Using a RTX setup to verify that the default RTX payload type is good.
  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  // Make sure NACK and FEC are enabled on the correct payload types.
  EXPECT_EQ(1000, config.rtp.nack.rtp_history_ms);
  EXPECT_EQ(GetEngineCodec("ulpfec").id, config.rtp.ulpfec.ulpfec_payload_type);
  EXPECT_EQ(GetEngineCodec("red").id, config.rtp.ulpfec.red_payload_type);

  EXPECT_EQ(1u, config.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config.rtp.rtx.ssrcs[0]);
  VerifySendStreamHasRtxTypes(config, default_apt_rtx_types_);
  // TODO(juberti): Check RTCP, PLI, TMMBR.
}

// The following four tests ensures that FlexFEC is not activated by default
// when the field trials are not enabled.
// TODO(brandtr): Remove or update these tests when FlexFEC _is_ enabled by
// default.
TEST_F(WebRtcVideoChannelTest, FlexfecSendCodecWithoutSsrcNotExposedByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0U, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelTest, FlexfecSendCodecWithSsrcNotExposedByDefault) {
  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0U, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelTest, FlexfecRecvCodecWithoutSsrcNotExposedByDefault) {
  AddRecvStream();

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  EXPECT_TRUE(streams.empty());
}

TEST_F(WebRtcVideoChannelTest, FlexfecRecvCodecWithSsrcNotExposedByDefault) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  EXPECT_TRUE(streams.empty());
}

// TODO(brandtr): When FlexFEC is no longer behind a field trial, merge all
// tests that use this test fixture into the corresponding "non-field trial"
// tests.
class WebRtcVideoChannelFlexfecRecvTest : public WebRtcVideoChannelTest {
 public:
  WebRtcVideoChannelFlexfecRecvTest()
      : WebRtcVideoChannelTest("WebRTC-FlexFEC-03-Advertised/Enabled/") {}
};

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       DefaultFlexfecCodecHasTransportCcAndRembFeedbackParam) {
  EXPECT_TRUE(cricket::HasTransportCc(GetEngineCodec("flexfec-03")));
  EXPECT_TRUE(cricket::HasRemb(GetEngineCodec("flexfec-03")));
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetDefaultRecvCodecsWithoutSsrc) {
  AddRecvStream();

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  EXPECT_TRUE(streams.empty());

  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1U, video_streams.size());
  const FakeVideoReceiveStream& video_stream = *video_streams.front();
  EXPECT_EQ(0, video_stream.GetNumAddedSecondarySinks());
  EXPECT_EQ(0, video_stream.GetNumRemovedSecondarySinks());
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetDefaultRecvCodecsWithSsrc) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream = streams.front();
  const webrtc::FlexfecReceiveStream::Config& config = stream->GetConfig();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.payload_type);
  EXPECT_EQ(kFlexfecSsrc, config.remote_ssrc);
  ASSERT_EQ(1U, config.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], config.protected_media_ssrcs[0]);

  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1U, video_streams.size());
  const FakeVideoReceiveStream& video_stream = *video_streams.front();
  EXPECT_EQ(1, video_stream.GetNumAddedSecondarySinks());
  const webrtc::VideoReceiveStream::Config& video_config =
      video_stream.GetConfig();
  EXPECT_TRUE(video_config.rtp.protected_by_flexfec);
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       EnablingFlexfecDoesNotRecreateVideoReceiveStream) {
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));

  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1U, video_streams.size());
  const FakeVideoReceiveStream& video_stream = *video_streams.front();
  EXPECT_EQ(0, video_stream.GetNumAddedSecondarySinks());
  EXPECT_EQ(0, video_stream.GetNumRemovedSecondarySinks());

  // Enable FlexFEC.
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams())
      << "Enabling FlexFEC should create FlexfecReceiveStream.";
  EXPECT_EQ(1U, fake_call_->GetVideoReceiveStreams().size())
      << "Enabling FlexFEC should not create VideoReceiveStream.";
  EXPECT_EQ(1U, fake_call_->GetFlexfecReceiveStreams().size())
      << "Enabling FlexFEC should create a single FlexfecReceiveStream.";
  EXPECT_EQ(1, video_stream.GetNumAddedSecondarySinks());
  EXPECT_EQ(0, video_stream.GetNumRemovedSecondarySinks());
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       DisablingFlexfecDoesNotRecreateVideoReceiveStream) {
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));

  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams());
  EXPECT_EQ(1U, fake_call_->GetFlexfecReceiveStreams().size());
  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1U, video_streams.size());
  const FakeVideoReceiveStream& video_stream = *video_streams.front();
  EXPECT_EQ(1, video_stream.GetNumAddedSecondarySinks());
  EXPECT_EQ(0, video_stream.GetNumRemovedSecondarySinks());

  // Disable FlexFEC.
  recv_parameters.codecs.clear();
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams())
      << "Disabling FlexFEC should not recreate VideoReceiveStream.";
  EXPECT_EQ(1U, fake_call_->GetVideoReceiveStreams().size())
      << "Disabling FlexFEC should not destroy VideoReceiveStream.";
  EXPECT_TRUE(fake_call_->GetFlexfecReceiveStreams().empty())
      << "Disabling FlexFEC should destroy FlexfecReceiveStream.";
  EXPECT_EQ(1, video_stream.GetNumAddedSecondarySinks());
  EXPECT_EQ(1, video_stream.GetNumRemovedSecondarySinks());
}

// TODO(brandtr): When FlexFEC is no longer behind a field trial, merge all
// tests that use this test fixture into the corresponding "non-field trial"
// tests.
class WebRtcVideoChannelFlexfecSendRecvTest : public WebRtcVideoChannelTest {
 public:
  WebRtcVideoChannelFlexfecSendRecvTest()
      : WebRtcVideoChannelTest(
            "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/") {
  }
};

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest, SetDefaultSendCodecsWithoutSsrc) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0U, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest, SetDefaultSendCodecsWithSsrc) {
  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.rtp.flexfec.payload_type);
  EXPECT_EQ(kFlexfecSsrc, config.rtp.flexfec.ssrc);
  ASSERT_EQ(1U, config.rtp.flexfec.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], config.rtp.flexfec.protected_media_ssrcs[0]);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithoutFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.ulpfec.ulpfec_payload_type);
  EXPECT_EQ(-1, config.rtp.ulpfec.red_payload_type);
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest, SetSendCodecsWithoutFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetRecvCodecsWithFec) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));

  const std::vector<FakeFlexfecReceiveStream*>& flexfec_streams =
      fake_call_->GetFlexfecReceiveStreams();
  ASSERT_EQ(1U, flexfec_streams.size());
  const FakeFlexfecReceiveStream* flexfec_stream = flexfec_streams.front();
  const webrtc::FlexfecReceiveStream::Config& flexfec_stream_config =
      flexfec_stream->GetConfig();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id,
            flexfec_stream_config.payload_type);
  EXPECT_EQ(kFlexfecSsrc, flexfec_stream_config.remote_ssrc);
  ASSERT_EQ(1U, flexfec_stream_config.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], flexfec_stream_config.protected_media_ssrcs[0]);
  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  const FakeVideoReceiveStream* video_stream = video_streams.front();
  const webrtc::VideoReceiveStream::Config& video_stream_config =
      video_stream->GetConfig();
  EXPECT_EQ(video_stream_config.rtp.local_ssrc,
            flexfec_stream_config.local_ssrc);
  EXPECT_EQ(video_stream_config.rtp.rtcp_mode, flexfec_stream_config.rtcp_mode);
  EXPECT_EQ(video_stream_config.rtcp_send_transport,
            flexfec_stream_config.rtcp_send_transport);
  // TODO(brandtr): Update this EXPECT when we set |transport_cc| in a
  // spec-compliant way.
  EXPECT_EQ(video_stream_config.rtp.transport_cc,
            flexfec_stream_config.transport_cc);
  EXPECT_EQ(video_stream_config.rtp.rtcp_mode, flexfec_stream_config.rtcp_mode);
  EXPECT_EQ(video_stream_config.rtp.extensions,
            flexfec_stream_config.rtp_header_extensions);
}

// We should not send FlexFEC, even if we advertise it, unless the right
// field trial is set.
// TODO(brandtr): Remove when FlexFEC is enabled by default.
TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       SetSendCodecsWithoutSsrcWithFecDoesNotEnableFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0u, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       SetSendCodecsWithSsrcWithFecDoesNotEnableFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0u, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelTest,
       SetSendCodecRejectsRtxWithoutAssociatedPayloadType) {
  const int kUnusedPayloadType = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType));

  cricket::VideoSendParameters parameters;
  cricket::VideoCodec rtx_codec(kUnusedPayloadType, "rtx");
  parameters.codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetSendParameters(parameters))
      << "RTX codec without associated payload type should be rejected.";
}

TEST_F(WebRtcVideoChannelTest,
       SetSendCodecRejectsRtxWithoutMatchingVideoCodec) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));
  {
    cricket::VideoCodec rtx_codec = cricket::VideoCodec::CreateRtxCodec(
        kUnusedPayloadType1, GetEngineCodec("VP8").id);
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(GetEngineCodec("VP8"));
    parameters.codecs.push_back(rtx_codec);
    ASSERT_TRUE(channel_->SetSendParameters(parameters));
  }
  {
    cricket::VideoCodec rtx_codec = cricket::VideoCodec::CreateRtxCodec(
        kUnusedPayloadType1, kUnusedPayloadType2);
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(GetEngineCodec("VP8"));
    parameters.codecs.push_back(rtx_codec);
    EXPECT_FALSE(channel_->SetSendParameters(parameters))
        << "RTX without matching video codec should be rejected.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithChangedRtxPayloadType) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));

  // SSRCs for RTX.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  AddSendStream(params);

  // Original payload type for RTX.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  cricket::VideoCodec rtx_codec(kUnusedPayloadType1, "rtx");
  rtx_codec.SetParam("apt", GetEngineCodec("VP8").id);
  parameters.codecs.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoSendStreams().size());
  const webrtc::VideoSendStream::Config& config_before =
      fake_call_->GetVideoSendStreams()[0]->GetConfig();
  EXPECT_EQ(kUnusedPayloadType1, config_before.rtp.rtx.payload_type);
  ASSERT_EQ(1U, config_before.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config_before.rtp.rtx.ssrcs[0]);

  // Change payload type for RTX.
  parameters.codecs[1].id = kUnusedPayloadType2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoSendStreams().size());
  const webrtc::VideoSendStream::Config& config_after =
      fake_call_->GetVideoSendStreams()[0]->GetConfig();
  EXPECT_EQ(kUnusedPayloadType2, config_after.rtp.rtx.payload_type);
  ASSERT_EQ(1U, config_after.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config_after.rtp.rtx.ssrcs[0]);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("ulpfec").id, config.rtp.ulpfec.ulpfec_payload_type);

  parameters.codecs.pop_back();
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  config = stream->GetConfig().Copy();
  EXPECT_EQ(-1, config.rtp.ulpfec.ulpfec_payload_type)
      << "SetSendCodec without ULPFEC should disable current ULPFEC.";
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest,
       SetSendCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.rtp.flexfec.payload_type);
  EXPECT_EQ(kFlexfecSsrc, config.rtp.flexfec.ssrc);
  ASSERT_EQ(1U, config.rtp.flexfec.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], config.rtp.flexfec.protected_media_ssrcs[0]);

  parameters.codecs.pop_back();
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  config = stream->GetConfig().Copy();
  EXPECT_EQ(-1, config.rtp.flexfec.payload_type)
      << "SetSendCodec without FlexFEC should disable current FlexFEC.";
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsChangesExistingStreams) {
  cricket::VideoSendParameters parameters;
  cricket::VideoCodec codec(100, "VP8");
  codec.SetParam(kCodecParamMaxQuantization, kDefaultQpMax);
  parameters.codecs.push_back(codec);

  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream();
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  EXPECT_EQ(kDefaultQpMax, streams[0].max_qp);

  parameters.codecs.clear();
  codec.SetParam(kCodecParamMaxQuantization, kDefaultQpMax + 1);
  parameters.codecs.push_back(codec);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  streams = fake_call_->GetVideoSendStreams()[0]->GetVideoStreams();
  EXPECT_EQ(kDefaultQpMax + 1, streams[0].max_qp);
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithHighMaxBitrate) {
  SetSendCodecsShouldWorkForBitrates("", 0, "", -1, "10000", 10000000);
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(10000000, streams[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       SetSendCodecsWithoutBitratesUsesCorrectDefaults) {
  SetSendCodecsShouldWorkForBitrates("", 0, "", -1, "", -1);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsCapsMinAndStartBitrate) {
  SetSendCodecsShouldWorkForBitrates("-1", 0, "-100", -1, "", -1);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsRejectsMaxLessThanMinBitrate) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "300";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "200";
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
}

// Test that when both the codec-specific bitrate params and max_bandwidth_bps
// are present in the same send parameters, the settings are combined correctly.
TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithBitratesAndMaxSendBandwidth) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "100";
  send_parameters_.codecs[0].params[kCodecParamStartBitrate] = "200";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "300";
  send_parameters_.max_bandwidth_bps = 400000;
  // We expect max_bandwidth_bps to take priority, if set.
  ExpectSetBitrateParameters(100000, 200000, 400000);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  // Since the codec isn't changing, start_bitrate_bps should be -1.
  ExpectSetBitrateParameters(100000, -1, 350000);

  // Decrease max_bandwidth_bps.
  send_parameters_.max_bandwidth_bps = 350000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  // Now try again with the values flipped around.
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "400";
  send_parameters_.max_bandwidth_bps = 300000;
  ExpectSetBitrateParameters(100000, 200000, 300000);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  // If we change the codec max, max_bandwidth_bps should still apply.
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "350";
  ExpectSetBitrateParameters(100000, 200000, 300000);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBandwidthShouldPreserveOtherBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
  send_parameters_.max_bandwidth_bps = 300000;
  // Setting max bitrate should keep previous min bitrate.
  // Setting max bitrate should not reset start bitrate.
  ExpectSetBitrateParameters(100000, -1, 300000);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBandwidthShouldBeRemovable) {
  send_parameters_.max_bandwidth_bps = 300000;
  ExpectSetMaxBitrate(300000);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  // -1 means to disable max bitrate (set infinite).
  send_parameters_.max_bandwidth_bps = -1;
  ExpectSetMaxBitrate(-1);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBandwidthAndAddSendStream) {
  send_parameters_.max_bandwidth_bps = 99999;
  FakeVideoSendStream* stream = AddSendStream();
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  ASSERT_EQ(1u, stream->GetVideoStreams().size());
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);

  send_parameters_.max_bandwidth_bps = 77777;
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);
}

// Tests that when the codec specific max bitrate and VideoSendParameters
// max_bandwidth_bps are used, that it sets the VideoStream's max bitrate
// appropriately.
TEST_F(WebRtcVideoChannelTest,
       MaxBitratePrioritizesVideoSendParametersOverCodecMaxBitrate) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "100";
  send_parameters_.codecs[0].params[kCodecParamStartBitrate] = "200";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "300";
  send_parameters_.max_bandwidth_bps = -1;
  AddSendStream();
  ExpectSetMaxBitrate(300000);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  std::vector<FakeVideoSendStream*> video_send_streams = GetFakeSendStreams();
  ASSERT_EQ(1u, video_send_streams.size());
  FakeVideoSendStream* video_send_stream = video_send_streams[0];
  ASSERT_EQ(1u, video_send_streams[0]->GetVideoStreams().size());
  // First the max bitrate is set based upon the codec param.
  EXPECT_EQ(300000,
            video_send_streams[0]->GetVideoStreams()[0].max_bitrate_bps);

  // The VideoSendParameters max bitrate overrides the codec's.
  send_parameters_.max_bandwidth_bps = 500000;
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  ASSERT_EQ(1u, video_send_stream->GetVideoStreams().size());
  EXPECT_EQ(500000, video_send_stream->GetVideoStreams()[0].max_bitrate_bps);
}

// Tests that when the codec specific max bitrate and RtpParameters
// max_bitrate_bps are used, that it sets the VideoStream's max bitrate
// appropriately.
TEST_F(WebRtcVideoChannelTest,
       MaxBitratePrioritizesRtpParametersOverCodecMaxBitrate) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "100";
  send_parameters_.codecs[0].params[kCodecParamStartBitrate] = "200";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "300";
  send_parameters_.max_bandwidth_bps = -1;
  AddSendStream();
  ExpectSetMaxBitrate(300000);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  std::vector<FakeVideoSendStream*> video_send_streams = GetFakeSendStreams();
  ASSERT_EQ(1u, video_send_streams.size());
  FakeVideoSendStream* video_send_stream = video_send_streams[0];
  ASSERT_EQ(1u, video_send_stream->GetVideoStreams().size());
  // First the max bitrate is set based upon the codec param.
  EXPECT_EQ(300000, video_send_stream->GetVideoStreams()[0].max_bitrate_bps);

  // The RtpParameter max bitrate overrides the codec's.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, parameters.encodings.size());
  parameters.encodings[0].max_bitrate_bps = 500000;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  ASSERT_EQ(1u, video_send_stream->GetVideoStreams().size());
  EXPECT_EQ(parameters.encodings[0].max_bitrate_bps,
            video_send_stream->GetVideoStreams()[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       MaxBitrateIsMinimumOfMaxSendBandwidthAndMaxEncodingBitrate) {
  send_parameters_.max_bandwidth_bps = 99999;
  FakeVideoSendStream* stream = AddSendStream();
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  ASSERT_EQ(1u, stream->GetVideoStreams().size());
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1u, parameters.encodings.size());

  parameters.encodings[0].max_bitrate_bps = 99999 - 1;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_EQ(parameters.encodings[0].max_bitrate_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);

  parameters.encodings[0].max_bitrate_bps = 99999 + 1;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBitrateCanIncreaseSenderBitrate) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  int initial_max_bitrate_bps = streams[0].max_bitrate_bps;
  EXPECT_GT(initial_max_bitrate_bps, 0);

  parameters.max_bandwidth_bps = initial_max_bitrate_bps * 2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  // Insert a frame to update the encoder config.
  EXPECT_TRUE(capturer.CaptureFrame());
  streams = stream->GetVideoStreams();
  EXPECT_EQ(initial_max_bitrate_bps * 2, streams[0].max_bitrate_bps);
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       SetMaxSendBitrateCanIncreaseSimulcastSenderBitrate) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3)));

  // Send a frame to make sure this scales up to >1 stream (simulcast).
  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(kSsrcs3[0], nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  ASSERT_GT(streams.size(), 1u)
      << "Without simulcast this test doesn't make sense.";
  int initial_max_bitrate_bps = GetTotalMaxBitrateBps(streams);
  EXPECT_GT(initial_max_bitrate_bps, 0);

  parameters.max_bandwidth_bps = initial_max_bitrate_bps * 2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  // Insert a frame to update the encoder config.
  EXPECT_TRUE(capturer.CaptureFrame());
  streams = stream->GetVideoStreams();
  int increased_max_bitrate_bps = GetTotalMaxBitrateBps(streams);
  EXPECT_EQ(initial_max_bitrate_bps * 2, increased_max_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(kSsrcs3[0], nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithMaxQuantization) {
  static const char* kMaxQuantization = "21";
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs[0].params[kCodecParamMaxQuantization] = kMaxQuantization;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(atoi(kMaxQuantization),
            AddSendStream()->GetVideoStreams().back().max_qp);

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ(kMaxQuantization, codec.params[kCodecParamMaxQuantization]);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsRejectBadPayloadTypes) {
  // TODO(pbos): Should we only allow the dynamic range?
  static const int kIncorrectPayloads[] = {-2, -1, 128, 129};
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  for (size_t i = 0; i < arraysize(kIncorrectPayloads); ++i) {
    parameters.codecs[0].id = kIncorrectPayloads[i];
    EXPECT_FALSE(channel_->SetSendParameters(parameters))
        << "Bad payload type '" << kIncorrectPayloads[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsAcceptAllValidPayloadTypes) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  for (int payload_type = 96; payload_type <= 127; ++payload_type) {
    parameters.codecs[0].id = payload_type;
    EXPECT_TRUE(channel_->SetSendParameters(parameters))
        << "Payload type '" << payload_type << "' rejected.";
  }
}

// Test that setting the a different set of codecs but with an identical front
// codec doesn't result in the stream being recreated.
// This may happen when a subsequent negotiation includes fewer codecs, as a
// result of one of the codecs being rejected.
TEST_F(WebRtcVideoChannelTest,
       SetSendCodecsIdenticalFirstCodecDoesntRecreateStream) {
  cricket::VideoSendParameters parameters1;
  parameters1.codecs.push_back(GetEngineCodec("VP8"));
  parameters1.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters1));

  AddSendStream();
  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());

  cricket::VideoSendParameters parameters2;
  parameters2.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters2));
  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithOnlyVp8) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

// Test that we set our inbound RTX codecs properly.
TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithRtx) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));

  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  cricket::VideoCodec rtx_codec(kUnusedPayloadType1, "rtx");
  parameters.codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec without associated payload should be rejected.";

  parameters.codecs[1].SetParam("apt", kUnusedPayloadType2);
  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec with invalid associated payload type should be rejected.";

  parameters.codecs[1].SetParam("apt", GetEngineCodec("VP8").id);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  cricket::VideoCodec rtx_codec2(kUnusedPayloadType2, "rtx");
  rtx_codec2.SetParam("apt", rtx_codec.id);
  parameters.codecs.push_back(rtx_codec2);

  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec with another RTX as associated payload type should be "
         "rejected.";
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithChangedRtxPayloadType) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));

  // SSRCs for RTX.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  AddRecvStream(params);

  // Original payload type for RTX.
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  cricket::VideoCodec rtx_codec(kUnusedPayloadType1, "rtx");
  rtx_codec.SetParam("apt", GetEngineCodec("VP8").id);
  parameters.codecs.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoReceiveStreams().size());
  const webrtc::VideoReceiveStream::Config& config_before =
      fake_call_->GetVideoReceiveStreams()[0]->GetConfig();
  EXPECT_EQ(1U, config_before.rtp.rtx_associated_payload_types.size());
  const int* payload_type_before = FindKeyByValue(
      config_before.rtp.rtx_associated_payload_types, GetEngineCodec("VP8").id);
  ASSERT_NE(payload_type_before, nullptr);
  EXPECT_EQ(kUnusedPayloadType1, *payload_type_before);
  EXPECT_EQ(kRtxSsrcs1[0], config_before.rtp.rtx_ssrc);

  // Change payload type for RTX.
  parameters.codecs[1].id = kUnusedPayloadType2;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoReceiveStreams().size());
  const webrtc::VideoReceiveStream::Config& config_after =
      fake_call_->GetVideoReceiveStreams()[0]->GetConfig();
  EXPECT_EQ(1U, config_after.rtp.rtx_associated_payload_types.size());
  const int* payload_type_after = FindKeyByValue(
      config_after.rtp.rtx_associated_payload_types, GetEngineCodec("VP8").id);
  ASSERT_NE(payload_type_after, nullptr);
  EXPECT_EQ(kUnusedPayloadType2, *payload_type_after);
  EXPECT_EQ(kRtxSsrcs1[0], config_after.rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsDifferentPayloadType) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs[0].id = 99;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsAcceptDefaultCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  FakeVideoReceiveStream* stream = AddRecvStream();
  const webrtc::VideoReceiveStream::Config& config = stream->GetConfig();
  EXPECT_EQ(engine_.codecs()[0].name, config.decoders[0].video_format.name);
  EXPECT_EQ(engine_.codecs()[0].id, config.decoders[0].payload_type);
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsRejectUnsupportedCodec) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(VideoCodec(101, "WTF3"));
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsAcceptsMultipleVideoCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP8"));
  send_parameters.codecs.push_back(GetEngineCodec("red"));
  send_parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));

  FakeVideoReceiveStream* stream = AddRecvStream();

  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(-1, stream->GetConfig().rtp.ulpfec_payload_type)
      << "SetSendCodec without ULPFEC should disable current ULPFEC.";
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetRecvParamsWithoutFecDisablesFec) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();

  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream = streams.front();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id, stream->GetConfig().payload_type);
  EXPECT_EQ(kFlexfecSsrc, stream->GetConfig().remote_ssrc);
  ASSERT_EQ(1U, stream->GetConfig().protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], stream->GetConfig().protected_media_ssrcs[0]);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(streams.empty())
      << "SetSendCodec without FlexFEC should disable current FlexFEC.";
}

TEST_F(WebRtcVideoChannelTest, SetSendParamsWithFecEnablesFec) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("red"));
  recv_parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type)
      << "ULPFEC should be enabled on the receive stream.";

  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP8"));
  send_parameters.codecs.push_back(GetEngineCodec("red"));
  send_parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type)
      << "ULPFEC should be enabled on the receive stream.";
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest,
       SetSendRecvParamsWithFecEnablesFec) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream_with_recv_params = streams.front();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id,
            stream_with_recv_params->GetConfig().payload_type);
  EXPECT_EQ(kFlexfecSsrc, stream_with_recv_params->GetConfig().remote_ssrc);
  EXPECT_EQ(1U,
            stream_with_recv_params->GetConfig().protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0],
            stream_with_recv_params->GetConfig().protected_media_ssrcs[0]);

  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP8"));
  send_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));
  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream_with_send_params = streams.front();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id,
            stream_with_send_params->GetConfig().payload_type);
  EXPECT_EQ(kFlexfecSsrc, stream_with_send_params->GetConfig().remote_ssrc);
  EXPECT_EQ(1U,
            stream_with_send_params->GetConfig().protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0],
            stream_with_send_params->GetConfig().protected_media_ssrcs[0]);
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsRejectDuplicateFecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("red"));
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       SetRecvCodecsRejectDuplicateFecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsRejectDuplicateCodecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest,
       SetRecvCodecsAcceptSameCodecOnMultiplePayloadTypes) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs[1].id += 1;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

// Test that setting the same codecs but with a different order
// doesn't result in the stream being recreated.
TEST_F(WebRtcVideoChannelTest,
       SetRecvCodecsDifferentOrderDoesntRecreateStream) {
  cricket::VideoRecvParameters parameters1;
  parameters1.codecs.push_back(GetEngineCodec("VP8"));
  parameters1.codecs.push_back(GetEngineCodec("red"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters1));

  AddRecvStream(cricket::StreamParams::CreateLegacy(123));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  cricket::VideoRecvParameters parameters2;
  parameters2.codecs.push_back(GetEngineCodec("red"));
  parameters2.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters2));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannelTest, SendStreamNotSendingByDefault) {
  EXPECT_FALSE(AddSendStream()->IsSending());
}

TEST_F(WebRtcVideoChannelTest, ReceiveStreamReceivingByDefault) {
  EXPECT_TRUE(AddRecvStream()->IsReceiving());
}

TEST_F(WebRtcVideoChannelTest, SetSend) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_FALSE(stream->IsSending());

  // false->true
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());
  // true->true
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());
  // true->false
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(stream->IsSending());
  // false->false
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(stream->IsSending());

  EXPECT_TRUE(channel_->SetSend(true));
  FakeVideoSendStream* new_stream = AddSendStream();
  EXPECT_TRUE(new_stream->IsSending())
      << "Send stream created after SetSend(true) not sending initially.";
}

// This test verifies DSCP settings are properly applied on video media channel.
TEST_F(WebRtcVideoChannelTest, TestSetDscpOptions) {
  std::unique_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  MediaConfig config;
  std::unique_ptr<cricket::WebRtcVideoChannel> channel;
  webrtc::RtpParameters parameters;

  channel.reset(
      static_cast<cricket::WebRtcVideoChannel*>(engine_.CreateMediaChannel(
          call_.get(), config, VideoOptions(), webrtc::CryptoOptions())));
  channel->SetInterface(network_interface.get(), /*media_transport=*/nullptr);
  // Default value when DSCP is disabled should be DSCP_DEFAULT.
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());

  // Default value when DSCP is enabled is also DSCP_DEFAULT, until it is set
  // through rtp parameters.
  config.enable_dscp = true;
  channel.reset(
      static_cast<cricket::WebRtcVideoChannel*>(engine_.CreateMediaChannel(
          call_.get(), config, VideoOptions(), webrtc::CryptoOptions())));
  channel->SetInterface(network_interface.get(), /*media_transport=*/nullptr);
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());

  // Create a send stream to configure
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));
  parameters = channel->GetRtpSendParameters(kSsrc);
  ASSERT_FALSE(parameters.encodings.empty());

  // Various priorities map to various dscp values.
  parameters.encodings[0].network_priority = 4.0;
  ASSERT_TRUE(channel->SetRtpSendParameters(kSsrc, parameters).ok());
  EXPECT_EQ(rtc::DSCP_AF41, network_interface->dscp());
  parameters.encodings[0].network_priority = 0.5;
  ASSERT_TRUE(channel->SetRtpSendParameters(kSsrc, parameters).ok());
  EXPECT_EQ(rtc::DSCP_CS1, network_interface->dscp());

  // A bad priority does not change the dscp value.
  parameters.encodings[0].network_priority = 0.0;
  ASSERT_FALSE(channel->SetRtpSendParameters(kSsrc, parameters).ok());
  EXPECT_EQ(rtc::DSCP_CS1, network_interface->dscp());

  // Packets should also self-identify their dscp in PacketOptions.
  const uint8_t kData[10] = {0};
  EXPECT_TRUE(static_cast<webrtc::Transport*>(channel.get())
                  ->SendRtcp(kData, sizeof(kData)));
  EXPECT_EQ(rtc::DSCP_CS1, network_interface->options().dscp);

  // Verify that setting the option to false resets the
  // DiffServCodePoint.
  config.enable_dscp = false;
  channel.reset(
      static_cast<cricket::WebRtcVideoChannel*>(engine_.CreateMediaChannel(
          call_.get(), config, VideoOptions(), webrtc::CryptoOptions())));
  channel->SetInterface(network_interface.get(), /*media_transport=*/nullptr);
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());
}

// This test verifies that the RTCP reduced size mode is properly applied to
// send video streams.
TEST_F(WebRtcVideoChannelTest, TestSetSendRtcpReducedSize) {
  // Create stream, expecting that default mode is "compound".
  FakeVideoSendStream* stream1 = AddSendStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream1->GetConfig().rtp.rtcp_mode);
  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_FALSE(rtp_parameters.rtcp.reduced_size);

  // Now enable reduced size mode.
  send_parameters_.rtcp.reduced_size = true;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  stream1 = fake_call_->GetVideoSendStreams()[0];
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream1->GetConfig().rtp.rtcp_mode);
  rtp_parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_TRUE(rtp_parameters.rtcp.reduced_size);

  // Create a new stream and ensure it picks up the reduced size mode.
  FakeVideoSendStream* stream2 = AddSendStream();
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream2->GetConfig().rtp.rtcp_mode);
}

// This test verifies that the RTCP reduced size mode is properly applied to
// receive video streams.
TEST_F(WebRtcVideoChannelTest, TestSetRecvRtcpReducedSize) {
  // Create stream, expecting that default mode is "compound".
  FakeVideoReceiveStream* stream1 = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream1->GetConfig().rtp.rtcp_mode);

  // Now enable reduced size mode.
  // TODO(deadbeef): Once "recv_parameters" becomes "receiver_parameters",
  // the reduced_size flag should come from that.
  send_parameters_.rtcp.reduced_size = true;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  stream1 = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream1->GetConfig().rtp.rtcp_mode);

  // Create a new stream and ensure it picks up the reduced size mode.
  FakeVideoReceiveStream* stream2 = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream2->GetConfig().rtp.rtcp_mode);
}

TEST_F(WebRtcVideoChannelTest, OnReadyToSendSignalsNetworkState) {
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::VIDEO));
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::AUDIO));

  channel_->OnReadyToSend(false);
  EXPECT_EQ(webrtc::kNetworkDown,
            fake_call_->GetNetworkState(webrtc::MediaType::VIDEO));
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::AUDIO));

  channel_->OnReadyToSend(true);
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::VIDEO));
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::AUDIO));
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsSentCodecName) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ("VP8", info.senders[0].codec_name);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsEncoderImplementationName) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.encoder_implementation_name = "encoder_implementation_name";
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.encoder_implementation_name,
            info.senders[0].encoder_implementation_name);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsCpuOveruseMetrics) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.avg_encode_time_ms = 13;
  stats.encode_usage_percent = 42;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.avg_encode_time_ms, info.senders[0].avg_encode_ms);
  EXPECT_EQ(stats.encode_usage_percent, info.senders[0].encode_usage_percent);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsFramesEncoded) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.frames_encoded = 13;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.frames_encoded, info.senders[0].frames_encoded);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsQpSum) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.qp_sum = 13;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.qp_sum, info.senders[0].qp_sum);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsUpperResolution) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.substreams[17].width = 123;
  stats.substreams[17].height = 40;
  stats.substreams[42].width = 80;
  stats.substreams[42].height = 31;
  stats.substreams[11].width = 20;
  stats.substreams[11].height = 90;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.senders.size());
  EXPECT_EQ(123, info.senders[0].send_frame_width);
  EXPECT_EQ(90, info.senders[0].send_frame_height);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsCpuAdaptationStats) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.number_of_cpu_adapt_changes = 2;
  stats.cpu_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(WebRtcVideoChannel::ADAPTREASON_CPU, info.senders[0].adapt_reason);
  EXPECT_EQ(stats.number_of_cpu_adapt_changes, info.senders[0].adapt_changes);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsAdaptationAndBandwidthStats) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.number_of_cpu_adapt_changes = 2;
  stats.cpu_limited_resolution = true;
  stats.bw_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(WebRtcVideoChannel::ADAPTREASON_CPU |
                WebRtcVideoChannel::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);
  EXPECT_EQ(stats.number_of_cpu_adapt_changes, info.senders[0].adapt_changes);
}

TEST_F(WebRtcVideoChannelTest,
       GetStatsTranslatesBandwidthLimitedResolutionCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.bw_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(WebRtcVideoChannel::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);
}

TEST_F(WebRtcVideoChannelTest, GetStatsTranslatesSendRtcpPacketTypesCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.substreams[17].rtcp_packet_type_counts.fir_packets = 2;
  stats.substreams[17].rtcp_packet_type_counts.nack_packets = 3;
  stats.substreams[17].rtcp_packet_type_counts.pli_packets = 4;

  stats.substreams[42].rtcp_packet_type_counts.fir_packets = 5;
  stats.substreams[42].rtcp_packet_type_counts.nack_packets = 7;
  stats.substreams[42].rtcp_packet_type_counts.pli_packets = 9;

  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(7, info.senders[0].firs_rcvd);
  EXPECT_EQ(10, info.senders[0].nacks_rcvd);
  EXPECT_EQ(13, info.senders[0].plis_rcvd);
}

TEST_F(WebRtcVideoChannelTest,
       GetStatsTranslatesReceiveRtcpPacketTypesCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.rtcp_packet_type_counts.fir_packets = 2;
  stats.rtcp_packet_type_counts.nack_packets = 3;
  stats.rtcp_packet_type_counts.pli_packets = 4;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.rtcp_packet_type_counts.fir_packets,
            rtc::checked_cast<unsigned int>(info.receivers[0].firs_sent));
  EXPECT_EQ(stats.rtcp_packet_type_counts.nack_packets,
            rtc::checked_cast<unsigned int>(info.receivers[0].nacks_sent));
  EXPECT_EQ(stats.rtcp_packet_type_counts.pli_packets,
            rtc::checked_cast<unsigned int>(info.receivers[0].plis_sent));
}

TEST_F(WebRtcVideoChannelTest, GetStatsTranslatesDecodeStatsCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.decoder_implementation_name = "decoder_implementation_name";
  stats.decode_ms = 2;
  stats.max_decode_ms = 3;
  stats.current_delay_ms = 4;
  stats.target_delay_ms = 5;
  stats.jitter_buffer_ms = 6;
  stats.min_playout_delay_ms = 7;
  stats.render_delay_ms = 8;
  stats.width = 9;
  stats.height = 10;
  stats.frame_counts.key_frames = 11;
  stats.frame_counts.delta_frames = 12;
  stats.frames_rendered = 13;
  stats.frames_decoded = 14;
  stats.qp_sum = 15;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.decoder_implementation_name,
            info.receivers[0].decoder_implementation_name);
  EXPECT_EQ(stats.decode_ms, info.receivers[0].decode_ms);
  EXPECT_EQ(stats.max_decode_ms, info.receivers[0].max_decode_ms);
  EXPECT_EQ(stats.current_delay_ms, info.receivers[0].current_delay_ms);
  EXPECT_EQ(stats.target_delay_ms, info.receivers[0].target_delay_ms);
  EXPECT_EQ(stats.jitter_buffer_ms, info.receivers[0].jitter_buffer_ms);
  EXPECT_EQ(stats.min_playout_delay_ms, info.receivers[0].min_playout_delay_ms);
  EXPECT_EQ(stats.render_delay_ms, info.receivers[0].render_delay_ms);
  EXPECT_EQ(stats.width, info.receivers[0].frame_width);
  EXPECT_EQ(stats.height, info.receivers[0].frame_height);
  EXPECT_EQ(rtc::checked_cast<unsigned int>(stats.frame_counts.key_frames +
                                            stats.frame_counts.delta_frames),
            info.receivers[0].frames_received);
  EXPECT_EQ(stats.frames_rendered, info.receivers[0].frames_rendered);
  EXPECT_EQ(stats.frames_decoded, info.receivers[0].frames_decoded);
  EXPECT_EQ(stats.qp_sum, info.receivers[0].qp_sum);
}

TEST_F(WebRtcVideoChannelTest, GetStatsTranslatesReceivePacketStatsCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.rtp_stats.transmitted.payload_bytes = 2;
  stats.rtp_stats.transmitted.header_bytes = 3;
  stats.rtp_stats.transmitted.padding_bytes = 4;
  stats.rtp_stats.transmitted.packets = 5;
  stats.rtcp_stats.packets_lost = 6;
  stats.rtcp_stats.fraction_lost = 7;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.rtp_stats.transmitted.payload_bytes +
                stats.rtp_stats.transmitted.header_bytes +
                stats.rtp_stats.transmitted.padding_bytes,
            rtc::checked_cast<size_t>(info.receivers[0].bytes_rcvd));
  EXPECT_EQ(stats.rtp_stats.transmitted.packets,
            rtc::checked_cast<unsigned int>(info.receivers[0].packets_rcvd));
  EXPECT_EQ(stats.rtcp_stats.packets_lost, info.receivers[0].packets_lost);
  EXPECT_EQ(rtc::checked_cast<float>(stats.rtcp_stats.fraction_lost) / (1 << 8),
            info.receivers[0].fraction_lost);
}

TEST_F(WebRtcVideoChannelTest, TranslatesCallStatsCorrectly) {
  AddSendStream();
  AddSendStream();
  webrtc::Call::Stats stats;
  stats.rtt_ms = 123;
  fake_call_->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(2u, info.senders.size());
  EXPECT_EQ(stats.rtt_ms, info.senders[0].rtt_ms);
  EXPECT_EQ(stats.rtt_ms, info.senders[1].rtt_ms);
}

TEST_F(WebRtcVideoChannelTest, TranslatesSenderBitrateStatsCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.target_media_bitrate_bps = 156;
  stats.media_bitrate_bps = 123;
  stats.substreams[17].total_bitrate_bps = 1;
  stats.substreams[17].retransmit_bitrate_bps = 2;
  stats.substreams[42].total_bitrate_bps = 3;
  stats.substreams[42].retransmit_bitrate_bps = 4;
  stream->SetStats(stats);

  FakeVideoSendStream* stream2 = AddSendStream();
  webrtc::VideoSendStream::Stats stats2;
  stats2.target_media_bitrate_bps = 200;
  stats2.media_bitrate_bps = 321;
  stats2.substreams[13].total_bitrate_bps = 5;
  stats2.substreams[13].retransmit_bitrate_bps = 6;
  stats2.substreams[21].total_bitrate_bps = 7;
  stats2.substreams[21].retransmit_bitrate_bps = 8;
  stream2->SetStats(stats2);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(2u, info.senders.size());
  BandwidthEstimationInfo bwe_info;
  channel_->FillBitrateInfo(&bwe_info);
  // Assuming stream and stream2 corresponds to senders[0] and [1] respectively
  // is OK as std::maps are sorted and AddSendStream() gives increasing SSRCs.
  EXPECT_EQ(stats.media_bitrate_bps, info.senders[0].nominal_bitrate);
  EXPECT_EQ(stats2.media_bitrate_bps, info.senders[1].nominal_bitrate);
  EXPECT_EQ(stats.target_media_bitrate_bps + stats2.target_media_bitrate_bps,
            bwe_info.target_enc_bitrate);
  EXPECT_EQ(stats.media_bitrate_bps + stats2.media_bitrate_bps,
            bwe_info.actual_enc_bitrate);
  EXPECT_EQ(1 + 3 + 5 + 7, bwe_info.transmit_bitrate)
      << "Bandwidth stats should take all streams into account.";
  EXPECT_EQ(2 + 4 + 6 + 8, bwe_info.retransmit_bitrate)
      << "Bandwidth stats should take all streams into account.";
}

TEST_F(WebRtcVideoChannelTest, DefaultReceiveStreamReconfiguresToUseRtx) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  rtc::SetBE32(&data[8], ssrcs[0]);
  rtc::CopyOnWriteBuffer packet(data, kDataLength);
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);

  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
      << "No default receive stream created.";
  FakeVideoReceiveStream* recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(0u, recv_stream->GetConfig().rtp.rtx_ssrc)
      << "Default receive stream should not have configured RTX";

  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs)));
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
      << "AddRecvStream should have reconfigured, not added a new receiver.";
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(
      recv_stream->GetConfig().rtp.rtx_associated_payload_types.empty());
  EXPECT_TRUE(VerifyRtxReceiveAssociations(recv_stream->GetConfig()))
      << "RTX should be mapped for all decoders/payload types.";
  EXPECT_TRUE(HasRtxReceiveAssociation(recv_stream->GetConfig(),
                                       GetEngineCodec("red").id))
      << "RTX should be mapped also for the RED payload type";
  EXPECT_EQ(rtx_ssrcs[0], recv_stream->GetConfig().rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, RejectsAddingStreamsWithMissingSsrcsForRtx) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  StreamParams sp =
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs);
  sp.ssrcs = ssrcs;  // Without RTXs, this is the important part.

  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannelTest, RejectsAddingStreamsWithOverlappingRtxSsrcs) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  StreamParams sp =
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs);

  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  // The RTX SSRC is already used in previous streams, using it should fail.
  sp = cricket::StreamParams::CreateLegacy(rtx_ssrcs[0]);
  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));

  // After removing the original stream this should be fine to add (makes sure
  // that RTX ssrcs are not forever taken).
  EXPECT_TRUE(channel_->RemoveSendStream(ssrcs[0]));
  EXPECT_TRUE(channel_->RemoveRecvStream(ssrcs[0]));
  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannelTest,
       RejectsAddingStreamsWithOverlappingSimulcastSsrcs) {
  static const uint32_t kFirstStreamSsrcs[] = {1, 2, 3};
  static const uint32_t kOverlappingStreamSsrcs[] = {4, 3, 5};
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  StreamParams sp =
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kFirstStreamSsrcs));

  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  // One of the SSRCs is already used in previous streams, using it should fail.
  sp = cricket::CreateSimStreamParams("cname",
                                      MAKE_VECTOR(kOverlappingStreamSsrcs));
  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));

  // After removing the original stream this should be fine to add (makes sure
  // that RTX ssrcs are not forever taken).
  EXPECT_TRUE(channel_->RemoveSendStream(kFirstStreamSsrcs[0]));
  EXPECT_TRUE(channel_->RemoveRecvStream(kFirstStreamSsrcs[0]));
  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannelTest, ReportsSsrcGroupsInStats) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  static const uint32_t kSenderSsrcs[] = {4, 7, 10};
  static const uint32_t kSenderRtxSsrcs[] = {5, 8, 11};

  StreamParams sender_sp = cricket::CreateSimWithRtxStreamParams(
      "cname", MAKE_VECTOR(kSenderSsrcs), MAKE_VECTOR(kSenderRtxSsrcs));

  EXPECT_TRUE(channel_->AddSendStream(sender_sp));

  static const uint32_t kReceiverSsrcs[] = {3};
  static const uint32_t kReceiverRtxSsrcs[] = {2};

  StreamParams receiver_sp = cricket::CreateSimWithRtxStreamParams(
      "cname", MAKE_VECTOR(kReceiverSsrcs), MAKE_VECTOR(kReceiverRtxSsrcs));
  EXPECT_TRUE(channel_->AddRecvStream(receiver_sp));

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));

  ASSERT_EQ(1u, info.senders.size());
  ASSERT_EQ(1u, info.receivers.size());

  EXPECT_NE(sender_sp.ssrc_groups, receiver_sp.ssrc_groups);
  EXPECT_EQ(sender_sp.ssrc_groups, info.senders[0].ssrc_groups);
  EXPECT_EQ(receiver_sp.ssrc_groups, info.receivers[0].ssrc_groups);
}

TEST_F(WebRtcVideoChannelTest, MapsReceivedPayloadTypeToCodecName) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  cricket::VideoMediaInfo info;

  // Report no codec name before receiving.
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ("", info.receivers[0].codec_name.c_str());

  // Report VP8 if we're receiving it.
  stats.current_payload_type = GetEngineCodec("VP8").id;
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ(kVp8CodecName, info.receivers[0].codec_name.c_str());

  // Report no codec name for unknown playload types.
  stats.current_payload_type = 3;
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ("", info.receivers[0].codec_name.c_str());
}

// Tests that when we add a stream without SSRCs, but contains a stream_id
// that it is stored and its stream id is later used when the first packet
// arrives to properly create a receive stream with a sync label.
TEST_F(WebRtcVideoChannelTest, RecvUnsignaledSsrcWithSignaledStreamId) {
  const char kSyncLabel[] = "sync_label";
  cricket::StreamParams unsignaled_stream;
  unsignaled_stream.set_stream_ids({kSyncLabel});
  ASSERT_TRUE(channel_->AddRecvStream(unsignaled_stream));
  // The stream shouldn't have been created at this point because it doesn't
  // have any SSRCs.
  EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());

  // Create and deliver packet.
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  rtc::SetBE32(&data[8], kIncomingUnsignalledSsrc);
  rtc::CopyOnWriteBuffer packet(data, kDataLength);
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);

  // The stream should now be created with the appropriate sync label.
  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(kSyncLabel,
            fake_call_->GetVideoReceiveStreams()[0]->GetConfig().sync_group);

  // Removing the unsignaled stream should clear the cache. This time when
  // a default video receive stream is created it won't have a sync_group.
  ASSERT_TRUE(channel_->RemoveRecvStream(0));
  ASSERT_TRUE(channel_->RemoveRecvStream(kIncomingUnsignalledSsrc));
  EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());

  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);
  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_TRUE(
      fake_call_->GetVideoReceiveStreams()[0]->GetConfig().sync_group.empty());
}

void WebRtcVideoChannelTest::TestReceiveUnsignaledSsrcPacket(
    uint8_t payload_type,
    bool expect_created_receive_stream) {
  // kRedRtxPayloadType must currently be unused.
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kRedRtxPayloadType));

  // Add a RED RTX codec.
  VideoCodec red_rtx_codec =
      VideoCodec::CreateRtxCodec(kRedRtxPayloadType, GetEngineCodec("red").id);
  recv_parameters_.codecs.push_back(red_rtx_codec);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));

  rtc::Set8(data, 1, payload_type);
  rtc::SetBE32(&data[8], kIncomingUnsignalledSsrc);
  rtc::CopyOnWriteBuffer packet(data, kDataLength);
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);

  if (expect_created_receive_stream) {
    EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
        << "Should have created a receive stream for payload type: "
        << payload_type;
  } else {
    EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size())
        << "Shouldn't have created a receive stream for payload type: "
        << payload_type;
  }
}

class WebRtcVideoChannelDiscardUnknownSsrcTest : public WebRtcVideoChannelTest {
 public:
  WebRtcVideoChannelDiscardUnknownSsrcTest()
      : WebRtcVideoChannelTest(
            "WebRTC-Video-DiscardPacketsWithUnknownSsrc/Enabled/") {}
};

TEST_F(WebRtcVideoChannelDiscardUnknownSsrcTest, NoUnsignalledStreamCreated) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("VP8").id,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, Vp8PacketCreatesUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("VP8").id,
                                  true /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, Vp9PacketCreatesUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("VP9").id,
                                  true /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, RtxPacketDoesntCreateUnsignalledStream) {
  AssignDefaultAptRtxTypes();
  const cricket::VideoCodec vp8 = GetEngineCodec("VP8");
  const int rtx_vp8_payload_type = default_apt_rtx_types_[vp8.id];
  TestReceiveUnsignaledSsrcPacket(rtx_vp8_payload_type,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, UlpfecPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("ulpfec").id,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       FlexfecPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("flexfec-03").id,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, RedRtxPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(kRedRtxPayloadType,
                                  false /* expect_created_receive_stream */);
}

// Test that receiving any unsignalled SSRC works even if it changes.
// The first unsignalled SSRC received will create a default receive stream.
// Any different unsignalled SSRC received will replace the default.
TEST_F(WebRtcVideoChannelTest, ReceiveDifferentUnsignaledSsrc) {
  // Allow receiving VP8, VP9, H264 (if enabled).
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));

#if defined(WEBRTC_USE_H264)
  cricket::VideoCodec H264codec(126, "H264");
  parameters.codecs.push_back(H264codec);
#endif

  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  // No receive streams yet.
  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  cricket::FakeVideoRenderer renderer;
  EXPECT_TRUE(channel_->SetSink(kDefaultRecvSsrc, &renderer));

  // Receive VP8 packet on first SSRC.
  uint8_t data[kMinRtpPacketLen];
  cricket::RtpHeader rtpHeader;
  rtpHeader.payload_type = GetEngineCodec("VP8").id;
  rtpHeader.seq_num = rtpHeader.timestamp = 0;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc + 1;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);
  // VP8 packet should create default receive stream.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  FakeVideoReceiveStream* recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(rtpHeader.ssrc, recv_stream->GetConfig().rtp.remote_ssrc);
  // Verify that the receive stream sinks to a renderer.
  webrtc::VideoFrame video_frame(CreateBlackFrameBuffer(4, 4), 100, 0,
                                 webrtc::kVideoRotation_0);
  recv_stream->InjectFrame(video_frame);
  EXPECT_EQ(1, renderer.num_rendered_frames());

  // Receive VP9 packet on second SSRC.
  rtpHeader.payload_type = GetEngineCodec("VP9").id;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc + 2;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet2(data, sizeof(data));
  channel_->OnPacketReceived(&packet2, /* packet_time_us */ -1);
  // VP9 packet should replace the default receive SSRC.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(rtpHeader.ssrc, recv_stream->GetConfig().rtp.remote_ssrc);
  // Verify that the receive stream sinks to a renderer.
  webrtc::VideoFrame video_frame2(CreateBlackFrameBuffer(4, 4), 200, 0,
                                  webrtc::kVideoRotation_0);
  recv_stream->InjectFrame(video_frame2);
  EXPECT_EQ(2, renderer.num_rendered_frames());

#if defined(WEBRTC_USE_H264)
  // Receive H264 packet on third SSRC.
  rtpHeader.payload_type = 126;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc + 3;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet3(data, sizeof(data));
  channel_->OnPacketReceived(&packet3, /* packet_time_us */ -1);
  // H264 packet should replace the default receive SSRC.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(rtpHeader.ssrc, recv_stream->GetConfig().rtp.remote_ssrc);
  // Verify that the receive stream sinks to a renderer.
  webrtc::VideoFrame video_frame3(CreateBlackFrameBuffer(4, 4), 300, 0,
                                  webrtc::kVideoRotation_0);
  recv_stream->InjectFrame(video_frame3);
  EXPECT_EQ(3, renderer.num_rendered_frames());
#endif
}

// This test verifies that when a new default stream is created for a new
// unsignaled SSRC, the new stream does not overwrite any old stream that had
// been the default receive stream before being properly signaled.
TEST_F(WebRtcVideoChannelTest,
       NewUnsignaledStreamDoesNotDestroyPreviouslyUnsignaledStream) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(parameters));

  // No streams signaled and no packets received, so we should not have any
  // stream objects created yet.
  EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());

  // Receive packet on an unsignaled SSRC.
  uint8_t data[kMinRtpPacketLen];
  cricket::RtpHeader rtp_header;
  rtp_header.payload_type = GetEngineCodec("VP8").id;
  rtp_header.seq_num = rtp_header.timestamp = 0;
  rtp_header.ssrc = kSsrcs3[0];
  cricket::SetRtpHeader(data, sizeof(data), rtp_header);
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);
  // Default receive stream should be created.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  FakeVideoReceiveStream* recv_stream0 =
      fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(kSsrcs3[0], recv_stream0->GetConfig().rtp.remote_ssrc);

  // Signal the SSRC.
  EXPECT_TRUE(
      channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrcs3[0])));
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  recv_stream0 = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(kSsrcs3[0], recv_stream0->GetConfig().rtp.remote_ssrc);

  // Receive packet on a different unsignaled SSRC.
  rtp_header.ssrc = kSsrcs3[1];
  cricket::SetRtpHeader(data, sizeof(data), rtp_header);
  packet.SetData(data, sizeof(data));
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);
  // New default receive stream should be created, but old stream should remain.
  ASSERT_EQ(2u, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(recv_stream0, fake_call_->GetVideoReceiveStreams()[0]);
  FakeVideoReceiveStream* recv_stream1 =
      fake_call_->GetVideoReceiveStreams()[1];
  EXPECT_EQ(kSsrcs3[1], recv_stream1->GetConfig().rtp.remote_ssrc);
}

TEST_F(WebRtcVideoChannelTest, CanSetMaxBitrateForExistingStream) {
  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(1280, capture_format_hd.width);
  EXPECT_EQ(720, capture_format_hd.height);
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetSend(true));
  capturer.CaptureFrame();

  int default_encoder_bitrate = GetMaxEncoderBitrate();
  EXPECT_GT(default_encoder_bitrate, 1000);

  // TODO(skvlad): Resolve the inconsistency between the interpretation
  // of the global bitrate limit for audio and video:
  // - Audio: max_bandwidth_bps = 0 - fail the operation,
  //          max_bandwidth_bps = -1 - remove the bandwidth limit
  // - Video: max_bandwidth_bps = 0 - remove the bandwidth limit,
  //          max_bandwidth_bps = -1 - remove the bandwidth limit

  SetAndExpectMaxBitrate(1000, 0, 1000);
  SetAndExpectMaxBitrate(1000, 800, 800);
  SetAndExpectMaxBitrate(600, 800, 600);
  SetAndExpectMaxBitrate(0, 800, 800);
  SetAndExpectMaxBitrate(0, 0, default_encoder_bitrate);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, CannotSetMaxBitrateForNonexistentStream) {
  webrtc::RtpParameters nonexistent_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(0u, nonexistent_parameters.encodings.size());

  nonexistent_parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(
      channel_->SetRtpSendParameters(last_ssrc_, nonexistent_parameters).ok());
}

TEST_F(WebRtcVideoChannelTest,
       SetLowMaxBitrateOverwritesVideoStreamMinBitrate) {
  FakeVideoSendStream* stream = AddSendStream();

  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_FALSE(parameters.encodings[0].max_bitrate_bps.has_value());
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Note that this is testing the behavior of the FakeVideoSendStream, which
  // also calls to CreateEncoderStreams to get the VideoStreams, so essentially
  // we are just testing the behavior of
  // EncoderStreamFactory::CreateEncoderStreams.
  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(kMinVideoBitrateBps, stream->GetVideoStreams()[0].min_bitrate_bps);

  // Set a low max bitrate & check that VideoStream.min_bitrate_bps is limited
  // by this amount.
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  int low_max_bitrate_bps = kMinVideoBitrateBps - 1000;
  parameters.encodings[0].max_bitrate_bps = low_max_bitrate_bps;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(low_max_bitrate_bps, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(low_max_bitrate_bps, stream->GetVideoStreams()[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       SetHighMinBitrateOverwritesVideoStreamMaxBitrate) {
  FakeVideoSendStream* stream = AddSendStream();

  // Note that this is testing the behavior of the FakeVideoSendStream, which
  // also calls to CreateEncoderStreams to get the VideoStreams, so essentially
  // we are just testing the behavior of
  // EncoderStreamFactory::CreateEncoderStreams.
  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  int high_min_bitrate_bps = stream->GetVideoStreams()[0].max_bitrate_bps + 1;

  // Set a high min bitrate and check that max_bitrate_bps is adjusted up.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  parameters.encodings[0].min_bitrate_bps = high_min_bitrate_bps;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(high_min_bitrate_bps, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(high_min_bitrate_bps, stream->GetVideoStreams()[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       SetMinBitrateAboveMaxBitrateLimitAdjustsMinBitrateDown) {
  send_parameters_.max_bandwidth_bps = 99999;
  FakeVideoSendStream* stream = AddSendStream();
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(kMinVideoBitrateBps, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);

  // Set min bitrate above global max bitrate and check that min_bitrate_bps is
  // adjusted down.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  parameters.encodings[0].min_bitrate_bps = 99999 + 1;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, SetMaxFramerateOneStream) {
  FakeVideoSendStream* stream = AddSendStream();

  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_FALSE(parameters.encodings[0].max_framerate.has_value());
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Note that this is testing the behavior of the FakeVideoSendStream, which
  // also calls to CreateEncoderStreams to get the VideoStreams, so essentially
  // we are just testing the behavior of
  // EncoderStreamFactory::CreateEncoderStreams.
  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(kDefaultVideoMaxFramerate,
            stream->GetVideoStreams()[0].max_framerate);

  // Set max framerate and check that VideoStream.max_framerate is set.
  const int kNewMaxFramerate = kDefaultVideoMaxFramerate - 1;
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  parameters.encodings[0].max_framerate = kNewMaxFramerate;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(kNewMaxFramerate, stream->GetVideoStreams()[0].max_framerate);
}

TEST_F(WebRtcVideoChannelTest, SetNumTemporalLayersForSingleStream) {
  FakeVideoSendStream* stream = AddSendStream();

  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_FALSE(parameters.encodings[0].num_temporal_layers.has_value());
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Note that this is testing the behavior of the FakeVideoSendStream, which
  // also calls to CreateEncoderStreams to get the VideoStreams, so essentially
  // we are just testing the behavior of
  // EncoderStreamFactory::CreateEncoderStreams.
  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_FALSE(stream->GetVideoStreams()[0].num_temporal_layers.has_value());

  // Set temporal layers and check that VideoStream.num_temporal_layers is set.
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  parameters.encodings[0].num_temporal_layers = 2;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  ASSERT_EQ(1UL, stream->GetVideoStreams().size());
  EXPECT_EQ(2UL, stream->GetVideoStreams()[0].num_temporal_layers);
}

TEST_F(WebRtcVideoChannelTest,
       CannotSetRtpSendParametersWithIncorrectNumberOfEncodings) {
  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  // Two or more encodings should result in failure.
  parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  // Zero encodings should also fail.
  parameters.encodings.clear();
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
}

TEST_F(WebRtcVideoChannelTest,
       CannotSetSimulcastRtpSendParametersWithIncorrectNumberOfEncodings) {
  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
  StreamParams sp = CreateSimStreamParams("cname", ssrcs);
  AddSendStream(sp);

  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);

  // Additional encodings should result in failure.
  parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  // Zero encodings should also fail.
  parameters.encodings.clear();
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
}

// Changing the SSRC through RtpParameters is not allowed.
TEST_F(WebRtcVideoChannelTest, CannotSetSsrcInRtpSendParameters) {
  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  parameters.encodings[0].ssrc = 0xdeadbeef;
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
}

// Tests that when RTCRtpEncodingParameters.bitrate_priority gets set to
// a value <= 0, setting the parameters returns false.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersInvalidBitratePriority) {
  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_EQ(webrtc::kDefaultBitratePriority,
            parameters.encodings[0].bitrate_priority);

  parameters.encodings[0].bitrate_priority = 0;
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  parameters.encodings[0].bitrate_priority = -2;
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
}

// Tests when the the RTCRtpEncodingParameters.bitrate_priority gets set
// properly on the VideoChannel and propogates down to the video encoder.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersPriorityOneStream) {
  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_EQ(webrtc::kDefaultBitratePriority,
            parameters.encodings[0].bitrate_priority);

  // Change the value and set it on the VideoChannel.
  double new_bitrate_priority = 2.0;
  parameters.encodings[0].bitrate_priority = new_bitrate_priority;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the encoding parameters bitrate_priority is set for the
  // VideoChannel.
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_EQ(new_bitrate_priority, parameters.encodings[0].bitrate_priority);

  // Verify that the new value propagated down to the encoder.
  std::vector<FakeVideoSendStream*> video_send_streams =
      fake_call_->GetVideoSendStreams();
  EXPECT_EQ(1UL, video_send_streams.size());
  FakeVideoSendStream* video_send_stream = video_send_streams.front();
  // Check that the WebRtcVideoSendStream updated the VideoEncoderConfig
  // appropriately.
  EXPECT_EQ(new_bitrate_priority,
            video_send_stream->GetEncoderConfig().bitrate_priority);
  // Check that the vector of VideoStreams also was propagated correctly. Note
  // that this is testing the behavior of the FakeVideoSendStream, which mimics
  // the calls to CreateEncoderStreams to get the VideoStreams.
  EXPECT_EQ(absl::optional<double>(new_bitrate_priority),
            video_send_stream->GetVideoStreams()[0].bitrate_priority);
}

// Tests that the RTCRtpEncodingParameters.bitrate_priority is set for the
// VideoChannel and the value propogates to the video encoder with all simulcast
// streams.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersPrioritySimulcastStreams) {
  // Create the stream params with multiple ssrcs for simulcast.
  const size_t kNumSimulcastStreams = 3;
  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
  StreamParams stream_params = CreateSimStreamParams("cname", ssrcs);
  AddSendStream(stream_params);
  uint32_t primary_ssrc = stream_params.first_ssrc();

  // Using the FakeVideoCapturerWithTaskQueue, we manually send a full size
  // frame. This creates multiple VideoStreams for all simulcast layers when
  // reconfiguring, and allows us to test this behavior.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(primary_ssrc, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(cricket::VideoFormat(
                1920, 1080, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());
  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters =
      channel_->GetRtpSendParameters(primary_ssrc);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_EQ(webrtc::kDefaultBitratePriority,
            parameters.encodings[0].bitrate_priority);
  // Change the value and set it on the VideoChannel.
  double new_bitrate_priority = 2.0;
  parameters.encodings[0].bitrate_priority = new_bitrate_priority;
  EXPECT_TRUE(channel_->SetRtpSendParameters(primary_ssrc, parameters).ok());

  // Verify that the encoding parameters priority is set on the VideoChannel.
  parameters = channel_->GetRtpSendParameters(primary_ssrc);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_EQ(new_bitrate_priority, parameters.encodings[0].bitrate_priority);

  // Verify that the new value propagated down to the encoder.
  std::vector<FakeVideoSendStream*> video_send_streams =
      fake_call_->GetVideoSendStreams();
  EXPECT_EQ(1UL, video_send_streams.size());
  FakeVideoSendStream* video_send_stream = video_send_streams.front();
  // Check that the WebRtcVideoSendStream updated the VideoEncoderConfig
  // appropriately.
  EXPECT_EQ(kNumSimulcastStreams,
            video_send_stream->GetEncoderConfig().number_of_streams);
  EXPECT_EQ(new_bitrate_priority,
            video_send_stream->GetEncoderConfig().bitrate_priority);
  // Check that the vector of VideoStreams also propagated correctly. The
  // FakeVideoSendStream calls CreateEncoderStreams, and we are testing that
  // these are created appropriately for the simulcast case.
  EXPECT_EQ(kNumSimulcastStreams, video_send_stream->GetVideoStreams().size());
  EXPECT_EQ(absl::optional<double>(new_bitrate_priority),
            video_send_stream->GetVideoStreams()[0].bitrate_priority);
  // Since we are only setting bitrate priority per-sender, the other
  // VideoStreams should have a bitrate priority of 0.
  EXPECT_EQ(absl::nullopt,
            video_send_stream->GetVideoStreams()[1].bitrate_priority);
  EXPECT_EQ(absl::nullopt,
            video_send_stream->GetVideoStreams()[2].bitrate_priority);
  EXPECT_TRUE(channel_->SetVideoSend(primary_ssrc, nullptr, nullptr));
}

// RTCRtpEncodingParameters.network_priority must be one of a few values
// derived from the default priority, corresponding to very-low, low, medium,
// or high.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersInvalidNetworkPriority) {
  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1UL, parameters.encodings.size());
  EXPECT_EQ(webrtc::kDefaultBitratePriority,
            parameters.encodings[0].network_priority);

  double good_values[] = {0.5, 1.0, 2.0, 4.0};
  double bad_values[] = {-1.0, 0.0, 0.49, 0.51, 1.1, 3.99, 4.1, 5.0};
  for (auto it : good_values) {
    parameters.encodings[0].network_priority = it;
    EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  }
  for (auto it : bad_values) {
    parameters.encodings[0].network_priority = it;
    EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  }
}

TEST_F(WebRtcVideoChannelTest, GetAndSetRtpSendParametersMaxFramerate) {
  const size_t kNumSimulcastStreams = 3;
  SetUpSimulcast(true, false);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  for (const auto& encoding : parameters.encodings) {
    EXPECT_FALSE(encoding.max_framerate);
  }

  // Change the value and set it on the VideoChannel.
  parameters.encodings[0].max_framerate = 10;
  parameters.encodings[1].max_framerate = 20;
  parameters.encodings[2].max_framerate = 25;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the bitrates are set on the VideoChannel.
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_EQ(10, parameters.encodings[0].max_framerate);
  EXPECT_EQ(20, parameters.encodings[1].max_framerate);
  EXPECT_EQ(25, parameters.encodings[2].max_framerate);
}

TEST_F(WebRtcVideoChannelTest,
       SetRtpSendParametersNumTemporalLayersFailsForInvalidRange) {
  const size_t kNumSimulcastStreams = 3;
  SetUpSimulcast(true, false);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());

  // Num temporal layers should be in the range [1, kMaxTemporalStreams].
  parameters.encodings[0].num_temporal_layers = 0;
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_RANGE,
            channel_->SetRtpSendParameters(last_ssrc_, parameters).type());
  parameters.encodings[0].num_temporal_layers = webrtc::kMaxTemporalStreams + 1;
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_RANGE,
            channel_->SetRtpSendParameters(last_ssrc_, parameters).type());
}

TEST_F(WebRtcVideoChannelTest,
       SetRtpSendParametersNumTemporalLayersFailsForInvalidModification) {
  const size_t kNumSimulcastStreams = 3;
  SetUpSimulcast(true, false);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());

  // No/all layers should be set.
  parameters.encodings[0].num_temporal_layers = 1;
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_MODIFICATION,
            channel_->SetRtpSendParameters(last_ssrc_, parameters).type());

  // Different values not supported.
  parameters.encodings[0].num_temporal_layers = 1;
  parameters.encodings[1].num_temporal_layers = 2;
  parameters.encodings[2].num_temporal_layers = 2;
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_MODIFICATION,
            channel_->SetRtpSendParameters(last_ssrc_, parameters).type());
}

TEST_F(WebRtcVideoChannelTest, GetAndSetRtpSendParametersNumTemporalLayers) {
  const size_t kNumSimulcastStreams = 3;
  SetUpSimulcast(true, false);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  for (const auto& encoding : parameters.encodings)
    EXPECT_FALSE(encoding.num_temporal_layers);

  // Change the value and set it on the VideoChannel.
  parameters.encodings[0].num_temporal_layers = 3;
  parameters.encodings[1].num_temporal_layers = 3;
  parameters.encodings[2].num_temporal_layers = 3;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the number of temporal layers are set on the VideoChannel.
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_EQ(3, parameters.encodings[0].num_temporal_layers);
  EXPECT_EQ(3, parameters.encodings[1].num_temporal_layers);
  EXPECT_EQ(3, parameters.encodings[2].num_temporal_layers);
}

TEST_F(WebRtcVideoChannelTest, NumTemporalLayersPropagatedToEncoder) {
  const size_t kNumSimulcastStreams = 3;
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Get and set the rtp encoding parameters.
  // Change the value and set it on the VideoChannel.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[0].num_temporal_layers = 2;
  parameters.encodings[1].num_temporal_layers = 2;
  parameters.encodings[2].num_temporal_layers = 2;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the new value is propagated down to the encoder.
  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  EXPECT_EQ(2, stream->num_encoder_reconfigurations());
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.number_of_streams);
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.simulcast_layers.size());
  EXPECT_EQ(2UL, encoder_config.simulcast_layers[0].num_temporal_layers);
  EXPECT_EQ(2UL, encoder_config.simulcast_layers[1].num_temporal_layers);
  EXPECT_EQ(2UL, encoder_config.simulcast_layers[2].num_temporal_layers);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(2UL, stream->GetVideoStreams()[0].num_temporal_layers);
  EXPECT_EQ(2UL, stream->GetVideoStreams()[1].num_temporal_layers);
  EXPECT_EQ(2UL, stream->GetVideoStreams()[2].num_temporal_layers);

  // No parameter changed, encoder should not be reconfigured.
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_EQ(2, stream->num_encoder_reconfigurations());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       DefaultValuePropagatedToEncoderForUnsetNumTemporalLayers) {
  const size_t kDefaultNumTemporalLayers = 3;
  const size_t kNumSimulcastStreams = 3;
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Change rtp encoding parameters, num_temporal_layers not changed.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[0].min_bitrate_bps = 33000;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that no value is propagated down to the encoder.
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.number_of_streams);
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.simulcast_layers.size());
  EXPECT_FALSE(encoder_config.simulcast_layers[0].num_temporal_layers);
  EXPECT_FALSE(encoder_config.simulcast_layers[1].num_temporal_layers);
  EXPECT_FALSE(encoder_config.simulcast_layers[2].num_temporal_layers);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(kDefaultNumTemporalLayers,
            stream->GetVideoStreams()[0].num_temporal_layers);
  EXPECT_EQ(kDefaultNumTemporalLayers,
            stream->GetVideoStreams()[1].num_temporal_layers);
  EXPECT_EQ(kDefaultNumTemporalLayers,
            stream->GetVideoStreams()[2].num_temporal_layers);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, MaxSimulcastFrameratePropagatedToEncoder) {
  const size_t kNumSimulcastStreams = 3;
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Get and set the rtp encoding parameters.
  // Change the value and set it on the VideoChannel.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[0].max_framerate = 15;
  parameters.encodings[1].max_framerate = 25;
  parameters.encodings[2].max_framerate = 20;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the new value propagated down to the encoder.
  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  EXPECT_EQ(2, stream->num_encoder_reconfigurations());
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.number_of_streams);
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.simulcast_layers.size());
  EXPECT_EQ(15, encoder_config.simulcast_layers[0].max_framerate);
  EXPECT_EQ(25, encoder_config.simulcast_layers[1].max_framerate);
  EXPECT_EQ(20, encoder_config.simulcast_layers[2].max_framerate);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  // Currently the maximum |max_framerate| is used.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(25, stream->GetVideoStreams()[0].max_framerate);
  EXPECT_EQ(25, stream->GetVideoStreams()[1].max_framerate);
  EXPECT_EQ(25, stream->GetVideoStreams()[2].max_framerate);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       DefaultValuePropagatedToEncoderForUnsetFramerate) {
  const size_t kNumSimulcastStreams = 3;
  const std::vector<webrtc::VideoStream> kDefault = GetSimulcastBitrates720p();
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Get and set the rtp encoding parameters.
  // Change the value and set it on the VideoChannel.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[0].max_framerate = 15;
  parameters.encodings[2].max_framerate = 20;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the new value propagated down to the encoder.
  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.number_of_streams);
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.simulcast_layers.size());
  EXPECT_EQ(15, encoder_config.simulcast_layers[0].max_framerate);
  EXPECT_EQ(-1, encoder_config.simulcast_layers[1].max_framerate);
  EXPECT_EQ(20, encoder_config.simulcast_layers[2].max_framerate);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  // The maximum |max_framerate| is used, kDefaultVideoMaxFramerate: 60.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(kDefaultVideoMaxFramerate,
            stream->GetVideoStreams()[0].max_framerate);
  EXPECT_EQ(kDefaultVideoMaxFramerate,
            stream->GetVideoStreams()[1].max_framerate);
  EXPECT_EQ(kDefaultVideoMaxFramerate,
            stream->GetVideoStreams()[2].max_framerate);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, GetAndSetRtpSendParametersMinAndMaxBitrate) {
  const size_t kNumSimulcastStreams = 3;
  SetUpSimulcast(true, false);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  for (const auto& encoding : parameters.encodings) {
    EXPECT_FALSE(encoding.min_bitrate_bps);
    EXPECT_FALSE(encoding.max_bitrate_bps);
  }

  // Change the value and set it on the VideoChannel.
  parameters.encodings[0].min_bitrate_bps = 100000;
  parameters.encodings[0].max_bitrate_bps = 200000;
  parameters.encodings[1].min_bitrate_bps = 300000;
  parameters.encodings[1].max_bitrate_bps = 400000;
  parameters.encodings[2].min_bitrate_bps = 500000;
  parameters.encodings[2].max_bitrate_bps = 600000;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the bitrates are set on the VideoChannel.
  parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_EQ(100000, parameters.encodings[0].min_bitrate_bps);
  EXPECT_EQ(200000, parameters.encodings[0].max_bitrate_bps);
  EXPECT_EQ(300000, parameters.encodings[1].min_bitrate_bps);
  EXPECT_EQ(400000, parameters.encodings[1].max_bitrate_bps);
  EXPECT_EQ(500000, parameters.encodings[2].min_bitrate_bps);
  EXPECT_EQ(600000, parameters.encodings[2].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersFailsWithIncorrectBitrate) {
  const size_t kNumSimulcastStreams = 3;
  SetUpSimulcast(true, false);

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());

  // Max bitrate lower than min bitrate should fail.
  parameters.encodings[2].min_bitrate_bps = 100000;
  parameters.encodings[2].max_bitrate_bps = 100000 - 1;
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_RANGE,
            channel_->SetRtpSendParameters(last_ssrc_, parameters).type());
}

// Test that min and max bitrate values set via RtpParameters are correctly
// propagated to the underlying encoder, and that the target is set to 3/4 of
// the maximum (3/4 was chosen because it's similar to the simulcast defaults
// that are used if no min/max are specified).
TEST_F(WebRtcVideoChannelTest, MinAndMaxSimulcastBitratePropagatedToEncoder) {
  const size_t kNumSimulcastStreams = 3;
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Get and set the rtp encoding parameters.
  // Change the value and set it on the VideoChannel.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[0].min_bitrate_bps = 100000;
  parameters.encodings[0].max_bitrate_bps = 200000;
  parameters.encodings[1].min_bitrate_bps = 300000;
  parameters.encodings[1].max_bitrate_bps = 400000;
  parameters.encodings[2].min_bitrate_bps = 500000;
  parameters.encodings[2].max_bitrate_bps = 600000;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the new value propagated down to the encoder.
  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  EXPECT_EQ(2, stream->num_encoder_reconfigurations());
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.number_of_streams);
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.simulcast_layers.size());
  EXPECT_EQ(100000, encoder_config.simulcast_layers[0].min_bitrate_bps);
  EXPECT_EQ(200000, encoder_config.simulcast_layers[0].max_bitrate_bps);
  EXPECT_EQ(300000, encoder_config.simulcast_layers[1].min_bitrate_bps);
  EXPECT_EQ(400000, encoder_config.simulcast_layers[1].max_bitrate_bps);
  EXPECT_EQ(500000, encoder_config.simulcast_layers[2].min_bitrate_bps);
  EXPECT_EQ(600000, encoder_config.simulcast_layers[2].max_bitrate_bps);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  // Target bitrate: 200000 * 3 / 4 = 150000.
  EXPECT_EQ(100000, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(150000, stream->GetVideoStreams()[0].target_bitrate_bps);
  EXPECT_EQ(200000, stream->GetVideoStreams()[0].max_bitrate_bps);
  // Target bitrate: 400000 * 3 / 4 = 300000.
  EXPECT_EQ(300000, stream->GetVideoStreams()[1].min_bitrate_bps);
  EXPECT_EQ(300000, stream->GetVideoStreams()[1].target_bitrate_bps);
  EXPECT_EQ(400000, stream->GetVideoStreams()[1].max_bitrate_bps);
  // Target bitrate: 600000 * 3 / 4 = 450000, less than min -> max.
  EXPECT_EQ(500000, stream->GetVideoStreams()[2].min_bitrate_bps);
  EXPECT_EQ(600000, stream->GetVideoStreams()[2].target_bitrate_bps);
  EXPECT_EQ(600000, stream->GetVideoStreams()[2].max_bitrate_bps);

  // No parameter changed, encoder should not be reconfigured.
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_EQ(2, stream->num_encoder_reconfigurations());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

// Test to only specify the min or max bitrate value for a layer via
// RtpParameters. The unspecified min/max and target value should be set to the
// simulcast default that is used if no min/max are specified.
TEST_F(WebRtcVideoChannelTest, MinOrMaxSimulcastBitratePropagatedToEncoder) {
  const size_t kNumSimulcastStreams = 3;
  const std::vector<webrtc::VideoStream> kDefault = GetSimulcastBitrates720p();
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());

  // Change the value and set it on the VideoChannel.
  // Layer 0: only configure min bitrate.
  const int kMinBpsLayer0 = kDefault[0].min_bitrate_bps + 1;
  parameters.encodings[0].min_bitrate_bps = kMinBpsLayer0;
  // Layer 1: only configure max bitrate.
  const int kMaxBpsLayer1 = kDefault[1].max_bitrate_bps - 1;
  parameters.encodings[1].max_bitrate_bps = kMaxBpsLayer1;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the new value propagated down to the encoder.
  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.number_of_streams);
  EXPECT_EQ(kNumSimulcastStreams, encoder_config.simulcast_layers.size());
  EXPECT_EQ(kMinBpsLayer0, encoder_config.simulcast_layers[0].min_bitrate_bps);
  EXPECT_EQ(-1, encoder_config.simulcast_layers[0].max_bitrate_bps);
  EXPECT_EQ(-1, encoder_config.simulcast_layers[1].min_bitrate_bps);
  EXPECT_EQ(kMaxBpsLayer1, encoder_config.simulcast_layers[1].max_bitrate_bps);
  EXPECT_EQ(-1, encoder_config.simulcast_layers[2].min_bitrate_bps);
  EXPECT_EQ(-1, encoder_config.simulcast_layers[2].max_bitrate_bps);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  // Layer 0: min configured bitrate should overwrite min default.
  EXPECT_EQ(kMinBpsLayer0, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(kDefault[0].target_bitrate_bps,
            stream->GetVideoStreams()[0].target_bitrate_bps);
  EXPECT_EQ(kDefault[0].max_bitrate_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);
  // Layer 1: max configured bitrate should overwrite max default.
  EXPECT_EQ(kDefault[1].min_bitrate_bps,
            stream->GetVideoStreams()[1].min_bitrate_bps);
  EXPECT_EQ(kDefault[1].target_bitrate_bps,
            stream->GetVideoStreams()[1].target_bitrate_bps);
  EXPECT_EQ(kMaxBpsLayer1, stream->GetVideoStreams()[1].max_bitrate_bps);
  // Layer 2: min and max bitrate not configured, default expected.
  EXPECT_EQ(kDefault[2].min_bitrate_bps,
            stream->GetVideoStreams()[2].min_bitrate_bps);
  EXPECT_EQ(kDefault[2].target_bitrate_bps,
            stream->GetVideoStreams()[2].target_bitrate_bps);
  EXPECT_EQ(kDefault[2].max_bitrate_bps,
            stream->GetVideoStreams()[2].max_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

// Test that specifying the min (or max) bitrate value for a layer via
// RtpParameters above (or below) the simulcast default max (or min) adjusts the
// unspecified values accordingly.
TEST_F(WebRtcVideoChannelTest, SetMinAndMaxSimulcastBitrateAboveBelowDefault) {
  const size_t kNumSimulcastStreams = 3;
  const std::vector<webrtc::VideoStream> kDefault = GetSimulcastBitrates720p();
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Get and set the rtp encoding parameters.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());

  // Change the value and set it on the VideoChannel.
  // For layer 0, set the min bitrate above the default max.
  const int kMinBpsLayer0 = kDefault[0].max_bitrate_bps + 1;
  parameters.encodings[0].min_bitrate_bps = kMinBpsLayer0;
  // For layer 1, set the max bitrate below the default min.
  const int kMaxBpsLayer1 = kDefault[1].min_bitrate_bps - 1;
  parameters.encodings[1].max_bitrate_bps = kMaxBpsLayer1;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Verify that the new value propagated down to the encoder.
  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately for the simulcast case.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  // Layer 0: Min bitrate above default max (target/max should be adjusted).
  EXPECT_EQ(kMinBpsLayer0, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(kMinBpsLayer0, stream->GetVideoStreams()[0].target_bitrate_bps);
  EXPECT_EQ(kMinBpsLayer0, stream->GetVideoStreams()[0].max_bitrate_bps);
  // Layer 1: Max bitrate below default min (min/target should be adjusted).
  EXPECT_EQ(kMaxBpsLayer1, stream->GetVideoStreams()[1].min_bitrate_bps);
  EXPECT_EQ(kMaxBpsLayer1, stream->GetVideoStreams()[1].target_bitrate_bps);
  EXPECT_EQ(kMaxBpsLayer1, stream->GetVideoStreams()[1].max_bitrate_bps);
  // Layer 2: min and max bitrate not configured, default expected.
  EXPECT_EQ(kDefault[2].min_bitrate_bps,
            stream->GetVideoStreams()[2].min_bitrate_bps);
  EXPECT_EQ(kDefault[2].target_bitrate_bps,
            stream->GetVideoStreams()[2].target_bitrate_bps);
  EXPECT_EQ(kDefault[2].max_bitrate_bps,
            stream->GetVideoStreams()[2].max_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, BandwidthAboveTotalMaxBitrateGivenToMaxLayer) {
  const size_t kNumSimulcastStreams = 3;
  const std::vector<webrtc::VideoStream> kDefault = GetSimulcastBitrates720p();
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Set max bitrate for all but the highest layer.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[0].max_bitrate_bps = kDefault[0].max_bitrate_bps;
  parameters.encodings[1].max_bitrate_bps = kDefault[1].max_bitrate_bps;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Set max bandwidth equal to total max bitrate.
  send_parameters_.max_bandwidth_bps =
      GetTotalMaxBitrateBps(stream->GetVideoStreams());
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  // No bitrate above the total max to give to the highest layer.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(kDefault[2].max_bitrate_bps,
            stream->GetVideoStreams()[2].max_bitrate_bps);

  // Set max bandwidth above the total max bitrate.
  send_parameters_.max_bandwidth_bps =
      GetTotalMaxBitrateBps(stream->GetVideoStreams()) + 1;
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  // The highest layer has no max bitrate set -> the bitrate above the total
  // max should be given to the highest layer.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            GetTotalMaxBitrateBps(stream->GetVideoStreams()));
  EXPECT_EQ(kDefault[2].max_bitrate_bps + 1,
            stream->GetVideoStreams()[2].max_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       BandwidthAboveTotalMaxBitrateNotGivenToMaxLayerIfMaxBitrateSet) {
  const size_t kNumSimulcastStreams = 3;
  const std::vector<webrtc::VideoStream> kDefault = GetSimulcastBitrates720p();
  EXPECT_EQ(kNumSimulcastStreams, kDefault.size());
  FakeVideoSendStream* stream = SetUpSimulcast(true, false);

  // Send a full size frame so all simulcast layers are used when reconfiguring.
  FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Set max bitrate for the highest layer.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  parameters.encodings[2].max_bitrate_bps = kDefault[2].max_bitrate_bps;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Set max bandwidth above the total max bitrate.
  send_parameters_.max_bandwidth_bps =
      GetTotalMaxBitrateBps(stream->GetVideoStreams()) + 1;
  ExpectSetMaxBitrate(send_parameters_.max_bandwidth_bps);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  // The highest layer has the max bitrate set -> the bitrate above the total
  // max should not be given to the highest layer.
  EXPECT_EQ(kNumSimulcastStreams, stream->GetVideoStreams().size());
  EXPECT_EQ(*parameters.encodings[2].max_bitrate_bps,
            stream->GetVideoStreams()[2].max_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

// Test that min and max bitrate values set via RtpParameters are correctly
// propagated to the underlying encoder for a single stream.
TEST_F(WebRtcVideoChannelTest, MinAndMaxBitratePropagatedToEncoder) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());

  // Set min and max bitrate.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(1u, parameters.encodings.size());
  parameters.encodings[0].min_bitrate_bps = 80000;
  parameters.encodings[0].max_bitrate_bps = 150000;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());

  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(1u, encoder_config.number_of_streams);
  EXPECT_EQ(1u, encoder_config.simulcast_layers.size());
  EXPECT_EQ(80000, encoder_config.simulcast_layers[0].min_bitrate_bps);
  EXPECT_EQ(150000, encoder_config.simulcast_layers[0].max_bitrate_bps);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately.
  EXPECT_EQ(1u, stream->GetVideoStreams().size());
  EXPECT_EQ(80000, stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(150000, stream->GetVideoStreams()[0].target_bitrate_bps);
  EXPECT_EQ(150000, stream->GetVideoStreams()[0].max_bitrate_bps);
}

// Test the default min and max bitrate value are correctly propagated to the
// underlying encoder for a single stream (when the values are not set via
// RtpParameters).
TEST_F(WebRtcVideoChannelTest, DefaultMinAndMaxBitratePropagatedToEncoder) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());

  // Check that WebRtcVideoSendStream updates VideoEncoderConfig correctly.
  webrtc::VideoEncoderConfig encoder_config = stream->GetEncoderConfig().Copy();
  EXPECT_EQ(1u, encoder_config.number_of_streams);
  EXPECT_EQ(1u, encoder_config.simulcast_layers.size());
  EXPECT_EQ(-1, encoder_config.simulcast_layers[0].min_bitrate_bps);
  EXPECT_EQ(-1, encoder_config.simulcast_layers[0].max_bitrate_bps);

  // FakeVideoSendStream calls CreateEncoderStreams, test that the vector of
  // VideoStreams are created appropriately.
  EXPECT_EQ(1u, stream->GetVideoStreams().size());
  EXPECT_EQ(cricket::kMinVideoBitrateBps,
            stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_GT(stream->GetVideoStreams()[0].max_bitrate_bps,
            stream->GetVideoStreams()[0].min_bitrate_bps);
  EXPECT_EQ(stream->GetVideoStreams()[0].max_bitrate_bps,
            stream->GetVideoStreams()[0].target_bitrate_bps);
}

// Test that a stream will not be sending if its encoding is made inactive
// through SetRtpSendParameters.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersOneEncodingActive) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());

  // Get current parameters and change "active" to false.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  parameters.encodings[0].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_FALSE(stream->IsSending());

  // Now change it back to active and verify we resume sending.
  parameters.encodings[0].active = true;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_TRUE(stream->IsSending());
}

// Tests that when active is updated for any simulcast layer then the send
// stream's sending state will be updated and it will be reconfigured with the
// new appropriate active simulcast streams.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersMultipleEncodingsActive) {
  // Create the stream params with multiple ssrcs for simulcast.
  const size_t kNumSimulcastStreams = 3;
  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
  StreamParams stream_params = CreateSimStreamParams("cname", ssrcs);
  FakeVideoSendStream* fake_video_send_stream = AddSendStream(stream_params);
  uint32_t primary_ssrc = stream_params.first_ssrc();

  // Using the FakeVideoCapturerWithTaskQueue, we manually send a full size
  // frame. This allows us to test that ReconfigureEncoder is called
  // appropriately.
  cricket::FakeVideoCapturerWithTaskQueue capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(primary_ssrc, &options, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(cricket::VideoFormat(
                1920, 1080, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  // Check that all encodings are initially active.
  webrtc::RtpParameters parameters =
      channel_->GetRtpSendParameters(primary_ssrc);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_TRUE(parameters.encodings[0].active);
  EXPECT_TRUE(parameters.encodings[1].active);
  EXPECT_TRUE(parameters.encodings[2].active);
  EXPECT_TRUE(fake_video_send_stream->IsSending());

  // Only turn on only the middle stream.
  parameters.encodings[0].active = false;
  parameters.encodings[1].active = true;
  parameters.encodings[2].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(primary_ssrc, parameters).ok());
  // Verify that the active fields are set on the VideoChannel.
  parameters = channel_->GetRtpSendParameters(primary_ssrc);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_FALSE(parameters.encodings[0].active);
  EXPECT_TRUE(parameters.encodings[1].active);
  EXPECT_FALSE(parameters.encodings[2].active);
  // Check that the VideoSendStream is updated appropriately. This means its
  // send state was updated and it was reconfigured.
  EXPECT_TRUE(fake_video_send_stream->IsSending());
  std::vector<webrtc::VideoStream> simulcast_streams =
      fake_video_send_stream->GetVideoStreams();
  EXPECT_EQ(kNumSimulcastStreams, simulcast_streams.size());
  EXPECT_FALSE(simulcast_streams[0].active);
  EXPECT_TRUE(simulcast_streams[1].active);
  EXPECT_FALSE(simulcast_streams[2].active);

  // Turn off all streams.
  parameters.encodings[0].active = false;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(primary_ssrc, parameters).ok());
  // Verify that the active fields are set on the VideoChannel.
  parameters = channel_->GetRtpSendParameters(primary_ssrc);
  EXPECT_EQ(kNumSimulcastStreams, parameters.encodings.size());
  EXPECT_FALSE(parameters.encodings[0].active);
  EXPECT_FALSE(parameters.encodings[1].active);
  EXPECT_FALSE(parameters.encodings[2].active);
  // Check that the VideoSendStream is off.
  EXPECT_FALSE(fake_video_send_stream->IsSending());
  simulcast_streams = fake_video_send_stream->GetVideoStreams();
  EXPECT_EQ(kNumSimulcastStreams, simulcast_streams.size());
  EXPECT_FALSE(simulcast_streams[0].active);
  EXPECT_FALSE(simulcast_streams[1].active);
  EXPECT_FALSE(simulcast_streams[2].active);

  EXPECT_TRUE(channel_->SetVideoSend(primary_ssrc, nullptr, nullptr));
}

// Test that if a stream is reconfigured (due to a codec change or other
// change) while its encoding is still inactive, it doesn't start sending.
TEST_F(WebRtcVideoChannelTest,
       InactiveStreamDoesntStartSendingWhenReconfigured) {
  // Set an initial codec list, which will be modified later.
  cricket::VideoSendParameters parameters1;
  parameters1.codecs.push_back(GetEngineCodec("VP8"));
  parameters1.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters1));

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());

  // Get current parameters and change "active" to false.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  parameters.encodings[0].active = false;
  EXPECT_EQ(1u, GetFakeSendStreams().size());
  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters).ok());
  EXPECT_FALSE(stream->IsSending());

  // Reorder the codec list, causing the stream to be reconfigured.
  cricket::VideoSendParameters parameters2;
  parameters2.codecs.push_back(GetEngineCodec("VP9"));
  parameters2.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters2));
  auto new_streams = GetFakeSendStreams();
  // Assert that a new underlying stream was created due to the codec change.
  // Otherwise, this test isn't testing what it set out to test.
  EXPECT_EQ(1u, GetFakeSendStreams().size());
  EXPECT_EQ(2, fake_call_->GetNumCreatedSendStreams());

  // Verify that we still are not sending anything, due to the inactive
  // encoding.
  EXPECT_FALSE(new_streams[0]->IsSending());
}

// Test that GetRtpSendParameters returns the currently configured codecs.
TEST_F(WebRtcVideoChannelTest, GetRtpSendParametersCodecs) {
  AddSendStream();
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(GetEngineCodec("VP8").ToCodecParameters(),
            rtp_parameters.codecs[0]);
  EXPECT_EQ(GetEngineCodec("VP9").ToCodecParameters(),
            rtp_parameters.codecs[1]);
}

// Test that GetRtpSendParameters returns the currently configured RTCP CNAME.
TEST_F(WebRtcVideoChannelTest, GetRtpSendParametersRtcpCname) {
  StreamParams params = StreamParams::CreateLegacy(kSsrc);
  params.cname = "rtcpcname";
  AddSendStream(params);

  webrtc::RtpParameters rtp_parameters = channel_->GetRtpSendParameters(kSsrc);
  EXPECT_STREQ("rtcpcname", rtp_parameters.rtcp.cname.c_str());
}

// Test that RtpParameters for send stream has one encoding and it has
// the correct SSRC.
TEST_F(WebRtcVideoChannelTest, GetRtpSendParametersSsrc) {
  AddSendStream();

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_EQ(last_ssrc_, rtp_parameters.encodings[0].ssrc);
}

TEST_F(WebRtcVideoChannelTest, DetectRtpSendParameterHeaderExtensionsChange) {
  AddSendStream();

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  rtp_parameters.header_extensions.emplace_back();

  EXPECT_NE(0u, rtp_parameters.header_extensions.size());

  webrtc::RTCError result =
      channel_->SetRtpSendParameters(last_ssrc_, rtp_parameters);
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_MODIFICATION, result.type());
}

TEST_F(WebRtcVideoChannelTest, GetRtpSendParametersDegradationPreference) {
  AddSendStream();

  FakeVideoCapturerWithTaskQueue capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, &capturer));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(rtp_parameters.degradation_preference,
            webrtc::DegradationPreference::BALANCED);
  rtp_parameters.degradation_preference =
      webrtc::DegradationPreference::MAINTAIN_FRAMERATE;

  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, rtp_parameters).ok());

  webrtc::RtpParameters updated_rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(updated_rtp_parameters.degradation_preference,
            webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  // Remove the source since it will be destroyed before the channel
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, nullptr, nullptr));
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVideoChannelTest, SetAndGetRtpSendParameters) {
  AddSendStream();
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  webrtc::RtpParameters initial_params =
      channel_->GetRtpSendParameters(last_ssrc_);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, initial_params).ok());

  // ... And this shouldn't change the params returned by GetRtpSendParameters.
  EXPECT_EQ(initial_params, channel_->GetRtpSendParameters(last_ssrc_));
}

// Test that GetRtpReceiveParameters returns the currently configured codecs.
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveParametersCodecs) {
  AddRecvStream();
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(last_ssrc_);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(GetEngineCodec("VP8").ToCodecParameters(),
            rtp_parameters.codecs[0]);
  EXPECT_EQ(GetEngineCodec("VP9").ToCodecParameters(),
            rtp_parameters.codecs[1]);
}

#if defined(WEBRTC_USE_H264)
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveFmtpSprop) {
#else
TEST_F(WebRtcVideoChannelTest, DISABLED_GetRtpReceiveFmtpSprop) {
#endif
  cricket::VideoRecvParameters parameters;
  cricket::VideoCodec kH264sprop1(101, "H264");
  kH264sprop1.SetParam(kH264FmtpSpropParameterSets, "uvw");
  parameters.codecs.push_back(kH264sprop1);
  cricket::VideoCodec kH264sprop2(102, "H264");
  kH264sprop2.SetParam(kH264FmtpSpropParameterSets, "xyz");
  parameters.codecs.push_back(kH264sprop2);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  FakeVideoReceiveStream* recv_stream = AddRecvStream();
  const webrtc::VideoReceiveStream::Config& cfg = recv_stream->GetConfig();
  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(last_ssrc_);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(kH264sprop1.ToCodecParameters(), rtp_parameters.codecs[0]);
  ASSERT_EQ(2u, cfg.decoders.size());
  EXPECT_EQ(101, cfg.decoders[0].payload_type);
  EXPECT_EQ("H264", cfg.decoders[0].video_format.name);
  const auto it0 =
      cfg.decoders[0].video_format.parameters.find(kH264FmtpSpropParameterSets);
  ASSERT_TRUE(it0 != cfg.decoders[0].video_format.parameters.end());
  EXPECT_EQ("uvw", it0->second);

  EXPECT_EQ(102, cfg.decoders[1].payload_type);
  EXPECT_EQ("H264", cfg.decoders[1].video_format.name);
  const auto it1 =
      cfg.decoders[1].video_format.parameters.find(kH264FmtpSpropParameterSets);
  ASSERT_TRUE(it1 != cfg.decoders[1].video_format.parameters.end());
  EXPECT_EQ("xyz", it1->second);
}

// Test that RtpParameters for receive stream has one encoding and it has
// the correct SSRC.
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveParametersSsrc) {
  AddRecvStream();

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(last_ssrc_);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_EQ(last_ssrc_, rtp_parameters.encodings[0].ssrc);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVideoChannelTest, SetAndGetRtpReceiveParameters) {
  AddRecvStream();
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters initial_params =
      channel_->GetRtpReceiveParameters(last_ssrc_);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpReceiveParameters(last_ssrc_, initial_params));

  // ... And this shouldn't change the params returned by
  // GetRtpReceiveParameters.
  EXPECT_EQ(initial_params, channel_->GetRtpReceiveParameters(last_ssrc_));
}

// Test that GetRtpReceiveParameters returns parameters correctly when SSRCs
// aren't signaled. It should always return an empty "RtpEncodingParameters",
// even after a packet is received and the unsignaled SSRC is known.
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveParametersWithUnsignaledSsrc) {
  // Call necessary methods to configure receiving a default stream as
  // soon as it arrives.
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  // Call GetRtpReceiveParameters before configured to receive an unsignaled
  // stream. Should return nothing.
  EXPECT_EQ(webrtc::RtpParameters(), channel_->GetRtpReceiveParameters(0));

  // Set a sink for an unsignaled stream.
  cricket::FakeVideoRenderer renderer;
  // Value of "0" means "unsignaled stream".
  EXPECT_TRUE(channel_->SetSink(0, &renderer));

  // Call GetRtpReceiveParameters before the SSRC is known. Value of "0"
  // in this method means "unsignaled stream".
  webrtc::RtpParameters rtp_parameters = channel_->GetRtpReceiveParameters(0);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_FALSE(rtp_parameters.encodings[0].ssrc);

  // Receive VP8 packet.
  uint8_t data[kMinRtpPacketLen];
  cricket::RtpHeader rtpHeader;
  rtpHeader.payload_type = GetEngineCodec("VP8").id;
  rtpHeader.seq_num = rtpHeader.timestamp = 0;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));
  channel_->OnPacketReceived(&packet, /* packet_time_us */ -1);

  // The |ssrc| member should still be unset.
  rtp_parameters = channel_->GetRtpReceiveParameters(0);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_FALSE(rtp_parameters.encodings[0].ssrc);
}

void WebRtcVideoChannelTest::TestReceiverLocalSsrcConfiguration(
    bool receiver_first) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const uint32_t kSenderSsrc = 0xC0FFEE;
  const uint32_t kSecondSenderSsrc = 0xBADCAFE;
  const uint32_t kReceiverSsrc = 0x4711;
  const uint32_t kExpectedDefaultReceiverSsrc = 1;

  if (receiver_first) {
    AddRecvStream(StreamParams::CreateLegacy(kReceiverSsrc));
    std::vector<FakeVideoReceiveStream*> receive_streams =
        fake_call_->GetVideoReceiveStreams();
    ASSERT_EQ(1u, receive_streams.size());
    // Default local SSRC when we have no sender.
    EXPECT_EQ(kExpectedDefaultReceiverSsrc,
              receive_streams[0]->GetConfig().rtp.local_ssrc);
  }
  AddSendStream(StreamParams::CreateLegacy(kSenderSsrc));
  if (!receiver_first)
    AddRecvStream(StreamParams::CreateLegacy(kReceiverSsrc));
  std::vector<FakeVideoReceiveStream*> receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kSenderSsrc, receive_streams[0]->GetConfig().rtp.local_ssrc);

  // Removing first sender should fall back to another (in this case the second)
  // local send stream's SSRC.
  AddSendStream(StreamParams::CreateLegacy(kSecondSenderSsrc));
  ASSERT_TRUE(channel_->RemoveSendStream(kSenderSsrc));
  receive_streams = fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kSecondSenderSsrc, receive_streams[0]->GetConfig().rtp.local_ssrc);

  // Removing the last sender should fall back to default local SSRC.
  ASSERT_TRUE(channel_->RemoveSendStream(kSecondSenderSsrc));
  receive_streams = fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kExpectedDefaultReceiverSsrc,
            receive_streams[0]->GetConfig().rtp.local_ssrc);
}

TEST_F(WebRtcVideoChannelTest, ConfiguresLocalSsrc) {
  TestReceiverLocalSsrcConfiguration(false);
}

TEST_F(WebRtcVideoChannelTest, ConfiguresLocalSsrcOnExistingReceivers) {
  TestReceiverLocalSsrcConfiguration(true);
}

class WebRtcVideoChannelSimulcastTest : public testing::Test {
 public:
  WebRtcVideoChannelSimulcastTest()
      : fake_call_(),
        encoder_factory_(new cricket::FakeWebRtcVideoEncoderFactory),
        decoder_factory_(new cricket::FakeWebRtcVideoDecoderFactory),
        mock_rate_allocator_factory_(
            new webrtc::MockVideoBitrateAllocatorFactory),
        engine_(std::unique_ptr<cricket::FakeWebRtcVideoEncoderFactory>(
                    encoder_factory_),
                std::unique_ptr<cricket::FakeWebRtcVideoDecoderFactory>(
                    decoder_factory_),
                std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>(
                    mock_rate_allocator_factory_)),
        last_ssrc_(0) {}

  void SetUp() override {
    encoder_factory_->AddSupportedVideoCodecType("VP8");
    channel_.reset(engine_.CreateMediaChannel(&fake_call_, GetMediaConfig(),
                                              VideoOptions(),
                                              webrtc::CryptoOptions()));
    channel_->OnReadyToSend(true);
    last_ssrc_ = 123;
  }

 protected:
  void VerifySimulcastSettings(const VideoCodec& codec,
                               int capture_width,
                               int capture_height,
                               size_t num_configured_streams,
                               size_t expected_num_streams,
                               bool screenshare,
                               bool conference_mode) {
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(codec);
    parameters.conference_mode = conference_mode;
    ASSERT_TRUE(channel_->SetSendParameters(parameters));

    std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
    RTC_DCHECK(num_configured_streams <= ssrcs.size());
    ssrcs.resize(num_configured_streams);

    AddSendStream(CreateSimStreamParams("cname", ssrcs));
    // Send a full-size frame to trigger a stream reconfiguration to use all
    // expected simulcast layers.
    FakeVideoCapturerWithTaskQueue capturer;
    VideoOptions options;
    if (screenshare)
      options.is_screencast = screenshare;
    EXPECT_TRUE(channel_->SetVideoSend(ssrcs.front(), &options, &capturer));
    // Fetch the latest stream since SetVideoSend() may recreate it if the
    // screen content setting is changed.
    FakeVideoSendStream* stream = fake_call_.GetVideoSendStreams().front();
    EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(cricket::VideoFormat(
                                       capture_width, capture_height,
                                       cricket::VideoFormat::FpsToInterval(30),
                                       cricket::FOURCC_I420)));
    channel_->SetSend(true);
    EXPECT_TRUE(capturer.CaptureFrame());

    auto rtp_parameters = channel_->GetRtpSendParameters(kSsrcs3[0]);
    EXPECT_EQ(num_configured_streams, rtp_parameters.encodings.size());

    std::vector<webrtc::VideoStream> video_streams = stream->GetVideoStreams();
    ASSERT_EQ(expected_num_streams, video_streams.size());

    std::vector<webrtc::VideoStream> expected_streams;
    if (conference_mode) {
      expected_streams = GetSimulcastConfig(
          num_configured_streams, capture_width, capture_height, 0,
          webrtc::kDefaultBitratePriority, kDefaultQpMax,
          kDefaultVideoMaxFramerate, screenshare, true);
      if (screenshare) {
        for (const webrtc::VideoStream& stream : expected_streams) {
          // Never scale screen content.
          EXPECT_EQ(stream.width, rtc::checked_cast<size_t>(capture_width));
          EXPECT_EQ(stream.height, rtc::checked_cast<size_t>(capture_height));
        }
      }
    } else {
      webrtc::VideoStream stream;
      stream.width = capture_width;
      stream.height = capture_height;
      stream.max_framerate = kDefaultVideoMaxFramerate;
      stream.min_bitrate_bps = cricket::kMinVideoBitrateBps;
      stream.target_bitrate_bps = stream.max_bitrate_bps =
          GetMaxDefaultBitrateBps(capture_width, capture_height);
      stream.max_qp = kDefaultQpMax;
      expected_streams.push_back(stream);
    }

    ASSERT_EQ(expected_streams.size(), video_streams.size());

    size_t num_streams = video_streams.size();
    int total_max_bitrate_bps = 0;
    for (size_t i = 0; i < num_streams; ++i) {
      EXPECT_EQ(expected_streams[i].width, video_streams[i].width);
      EXPECT_EQ(expected_streams[i].height, video_streams[i].height);

      EXPECT_GT(video_streams[i].max_framerate, 0);
      EXPECT_EQ(expected_streams[i].max_framerate,
                video_streams[i].max_framerate);

      EXPECT_GT(video_streams[i].min_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].min_bitrate_bps,
                video_streams[i].min_bitrate_bps);

      EXPECT_GT(video_streams[i].target_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].target_bitrate_bps,
                video_streams[i].target_bitrate_bps);

      EXPECT_GT(video_streams[i].max_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].max_bitrate_bps,
                video_streams[i].max_bitrate_bps);

      EXPECT_GT(video_streams[i].max_qp, 0);
      EXPECT_EQ(expected_streams[i].max_qp, video_streams[i].max_qp);

      EXPECT_EQ(conference_mode,
                expected_streams[i].num_temporal_layers.has_value());

      if (conference_mode) {
        EXPECT_EQ(expected_streams[i].num_temporal_layers,
                  video_streams[i].num_temporal_layers);
      }

      if (i == num_streams - 1) {
        total_max_bitrate_bps += video_streams[i].max_bitrate_bps;
      } else {
        total_max_bitrate_bps += video_streams[i].target_bitrate_bps;
      }
    }

    EXPECT_TRUE(channel_->SetVideoSend(ssrcs.front(), nullptr, nullptr));
  }

  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams = fake_call_.GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_call_.GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_call_.GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams = fake_call_.GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_.GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  webrtc::RtcEventLogNullImpl event_log_;
  FakeCall fake_call_;
  cricket::FakeWebRtcVideoEncoderFactory* encoder_factory_;
  cricket::FakeWebRtcVideoDecoderFactory* decoder_factory_;
  webrtc::MockVideoBitrateAllocatorFactory* mock_rate_allocator_factory_;
  WebRtcVideoEngine engine_;
  std::unique_ptr<VideoMediaChannel> channel_;
  uint32_t last_ssrc_;
};

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsWith2SimulcastStreams) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 640, 360, 2, 2, false,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsWith3SimulcastStreams) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 3, false,
                          true);
}

// Test that we normalize send codec format size in simulcast.
TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsWithOddSizeInSimulcast) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 541, 271, 2, 2, false,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsForScreenshare) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 1, true,
                          false);
}

TEST_F(WebRtcVideoChannelSimulcastTest,
       SetSendCodecsForConferenceModeScreenshare) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 1, true,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsForSimulcastScreenshare) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-SimulcastScreenshare/Enabled/");
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 2, true,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest,
       NoSimulcastScreenshareWithoutConference) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-SimulcastScreenshare/Enabled/");
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 1, true,
                          false);
}

class WebRtcVideoFakeClock {
 public:
  WebRtcVideoFakeClock() {
    fake_clock_.AdvanceTime(webrtc::TimeDelta::ms(1));  // avoid time=0
  }
  rtc::ScopedFakeClock fake_clock_;
};

// The fake clock needs to be initialized before the call, and not
// destroyed until after all threads spawned by the test have been stopped.
// This mixin ensures that.
class WebRtcVideoChannelTestWithClock : public WebRtcVideoFakeClock,
                                        public WebRtcVideoChannelBaseTest {};

TEST_F(WebRtcVideoChannelTestWithClock, GetSources) {
  uint8_t data1[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  EXPECT_EQ(0u, channel_->GetSources(kSsrc).size());

  rtc::CopyOnWriteBuffer packet1(data1, sizeof(data1));
  rtc::SetBE32(packet1.data() + 8, kSsrc);
  channel_->SetSink(kDefaultReceiveSsrc, NULL);
  EXPECT_TRUE(SetDefaultCodec());
  EXPECT_TRUE(SetSend(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  channel_->OnPacketReceived(&packet1, rtc::PacketTime());

  std::vector<webrtc::RtpSource> sources = channel_->GetSources(kSsrc);
  EXPECT_EQ(1u, sources.size());
  EXPECT_EQ(webrtc::RtpSourceType::SSRC, sources[0].source_type());
  int64_t timestamp1 = sources[0].timestamp_ms();

  // a new packet.
  int64_t timeDeltaMs = 1;
  fake_clock_.AdvanceTime(webrtc::TimeDelta::ms(timeDeltaMs));
  channel_->OnPacketReceived(&packet1, rtc::PacketTime());
  int64_t timestamp2 = channel_->GetSources(kSsrc)[0].timestamp_ms();
  EXPECT_EQ(timestamp2, timestamp1 + timeDeltaMs);

  // It only keeps 10s of history.
  fake_clock_.AdvanceTime(webrtc::TimeDelta::seconds(10));
  fake_clock_.AdvanceTime(webrtc::TimeDelta::ms(1));
  EXPECT_EQ(0u, channel_->GetSources(kSsrc).size());
}

TEST_F(WebRtcVideoChannelTestWithClock, GetContributingSources) {
  uint8_t data1[] = {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  uint32_t kCsrc = 4321u;
  EXPECT_EQ(0u, channel_->GetSources(kSsrc).size());
  EXPECT_EQ(0u, channel_->GetSources(kCsrc).size());

  rtc::CopyOnWriteBuffer packet1(data1, sizeof(data1));
  rtc::SetBE32(packet1.data() + 8, kSsrc);
  rtc::SetBE32(packet1.data() + 12, kCsrc);
  channel_->SetSink(kDefaultReceiveSsrc, NULL);
  EXPECT_TRUE(SetDefaultCodec());
  EXPECT_TRUE(SetSend(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  channel_->OnPacketReceived(&packet1, rtc::PacketTime());

  {
    ASSERT_EQ(2u, channel_->GetSources(kSsrc).size());
    EXPECT_EQ(0u, channel_->GetSources(kCsrc).size());
    std::vector<webrtc::RtpSource> sources = channel_->GetSources(kSsrc);
    EXPECT_EQ(sources[0].timestamp_ms(), sources[1].timestamp_ms());
    // 1 SSRC and 1 CSRC.
    EXPECT_EQ(1, std::count_if(sources.begin(), sources.end(),
                               [](const webrtc::RtpSource& source) {
                                 return source.source_type() ==
                                        webrtc::RtpSourceType::SSRC;
                               }));
    EXPECT_EQ(1, std::count_if(sources.begin(), sources.end(),
                               [](const webrtc::RtpSource& source) {
                                 return source.source_type() ==
                                        webrtc::RtpSourceType::CSRC;
                               }));
  }
  int64_t timestamp1 = channel_->GetSources(kSsrc)[0].timestamp_ms();

  // a new packet with only ssrc (i.e no csrc).
  uint8_t data2[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  rtc::CopyOnWriteBuffer packet2(data2, sizeof(data2));
  rtc::SetBE32(packet2.data() + 8, kSsrc);

  int64_t timeDeltaMs = 1;
  fake_clock_.AdvanceTime(webrtc::TimeDelta::ms(timeDeltaMs));
  channel_->OnPacketReceived(&packet2, rtc::PacketTime());

  {
    ASSERT_EQ(2u, channel_->GetSources(kSsrc).size());
    EXPECT_EQ(0u, channel_->GetSources(kCsrc).size());
    std::vector<webrtc::RtpSource> sources = channel_->GetSources(kSsrc);
    EXPECT_NE(sources[0].timestamp_ms(), sources[1].timestamp_ms());
    // 1 SSRC and 1 CSRC.
    EXPECT_EQ(1, std::count_if(sources.begin(), sources.end(),
                               [](const webrtc::RtpSource& source) {
                                 return source.source_type() ==
                                        webrtc::RtpSourceType::SSRC;
                               }));
    EXPECT_EQ(1, std::count_if(sources.begin(), sources.end(),
                               [](const webrtc::RtpSource& source) {
                                 return source.source_type() ==
                                        webrtc::RtpSourceType::CSRC;
                               }));
    auto ssrcSource = std::find_if(
        sources.begin(), sources.end(), [](const webrtc::RtpSource& source) {
          return source.source_type() == webrtc::RtpSourceType::SSRC;
        });
    auto csrcSource = std::find_if(
        sources.begin(), sources.end(), [](const webrtc::RtpSource& source) {
          return source.source_type() == webrtc::RtpSourceType::CSRC;
        });

    EXPECT_EQ(ssrcSource->timestamp_ms(), timestamp1 + timeDeltaMs);
    EXPECT_EQ(csrcSource->timestamp_ms(), timestamp1);
  }

  // It only keeps 10s of history.
  fake_clock_.AdvanceTime(webrtc::TimeDelta::seconds(10));

  {
    ASSERT_EQ(1u, channel_->GetSources(kSsrc).size());
    EXPECT_EQ(0u, channel_->GetSources(kCsrc).size());
    std::vector<webrtc::RtpSource> sources = channel_->GetSources(kSsrc);
    EXPECT_EQ(1, std::count_if(sources.begin(), sources.end(),
                               [](const webrtc::RtpSource& source) {
                                 return source.source_type() ==
                                        webrtc::RtpSourceType::SSRC;
                               }));
    EXPECT_EQ(0, std::count_if(sources.begin(), sources.end(),
                               [](const webrtc::RtpSource& source) {
                                 return source.source_type() ==
                                        webrtc::RtpSourceType::CSRC;
                               }));
  }

  fake_clock_.AdvanceTime(webrtc::TimeDelta::ms(1));
  EXPECT_EQ(0u, channel_->GetSources(kSsrc).size());
  EXPECT_EQ(0u, channel_->GetSources(kCsrc).size());
}

}  // namespace cricket
