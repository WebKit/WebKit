/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/opus_audio_decoder_factory.h"
#include "api/audio_codecs/opus_audio_encoder_factory.h"
#include "api/rtp_parameters.h"
#include "api/stats/rtcstats_objects.h"
#include "api/units/data_rate.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "pc/sdp_utils.h"
#include "pc/simulcast_description.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/test/peer_connection_test_wrapper.h"
#include "pc/test/simulcast_layer_util.h"
#include "rtc_base/gunit.h"
#include "rtc_base/physical_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Eq;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::StrCaseEq;
using ::testing::StrEq;

namespace webrtc {

namespace {

constexpr TimeDelta kDefaultTimeout = TimeDelta::Seconds(5);
// Most tests pass in 20-30 seconds, but some tests take longer such as AV1
// requiring additional ramp-up time (https://crbug.com/webrtc/15006) or SVC
// (LxTx_KEY) being slower than simulcast to send top spatial layer.
// TODO(https://crbug.com/webrtc/15076): Remove need for long rampup timeouts by
// using simulated time.
constexpr TimeDelta kLongTimeoutForRampingUp = TimeDelta::Minutes(1);

// The max bitrate 1500 kbps may be subject to change in the future. What we're
// interested in here is that all code paths that result in L1T3 result in the
// same target bitrate which does not exceed this limit.
constexpr DataRate kVp9ExpectedMaxBitrateForL1T3 =
    DataRate::KilobitsPerSec(1500);

struct StringParamToString {
  std::string operator()(const ::testing::TestParamInfo<std::string>& info) {
    return info.param;
  }
};

// RTX, RED and FEC are reliability mechanisms used in combinations with other
// codecs, but are not themselves a specific codec. Typically you don't want to
// filter these out of the list of codec preferences.
bool IsReliabilityMechanism(const webrtc::RtpCodecCapability& codec) {
  return absl::EqualsIgnoreCase(codec.name, cricket::kRtxCodecName) ||
         absl::EqualsIgnoreCase(codec.name, cricket::kRedCodecName) ||
         absl::EqualsIgnoreCase(codec.name, cricket::kUlpfecCodecName);
}

std::string GetCurrentCodecMimeType(
    rtc::scoped_refptr<const webrtc::RTCStatsReport> report,
    const webrtc::RTCOutboundRtpStreamStats& outbound_rtp) {
  return outbound_rtp.codec_id.is_defined()
             ? *report->GetAs<webrtc::RTCCodecStats>(*outbound_rtp.codec_id)
                    ->mime_type
             : "";
}

struct RidAndResolution {
  std::string rid;
  uint32_t width;
  uint32_t height;
};

const webrtc::RTCOutboundRtpStreamStats* FindOutboundRtpByRid(
    const std::vector<const webrtc::RTCOutboundRtpStreamStats*>& outbound_rtps,
    const absl::string_view& rid) {
  for (const auto* outbound_rtp : outbound_rtps) {
    if (outbound_rtp->rid.is_defined() && *outbound_rtp->rid == rid) {
      return outbound_rtp;
    }
  }
  return nullptr;
}

}  // namespace

class PeerConnectionEncodingsIntegrationTest : public ::testing::Test {
 public:
  PeerConnectionEncodingsIntegrationTest()
      : background_thread_(std::make_unique<rtc::Thread>(&pss_)) {
    RTC_CHECK(background_thread_->Start());
  }

  rtc::scoped_refptr<PeerConnectionTestWrapper> CreatePc() {
    auto pc_wrapper = rtc::make_ref_counted<PeerConnectionTestWrapper>(
        "pc", &pss_, background_thread_.get(), background_thread_.get());
    pc_wrapper->CreatePc({}, webrtc::CreateOpusAudioEncoderFactory(),
                         webrtc::CreateOpusAudioDecoderFactory());
    return pc_wrapper;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiverWithSimulcastLayers(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local,
      rtc::scoped_refptr<PeerConnectionTestWrapper> remote,
      std::vector<cricket::SimulcastLayer> init_layers) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
        local->GetUserMedia(
            /*audio=*/false, cricket::AudioOptions(), /*video=*/true,
            {.width = 1280, .height = 720});
    rtc::scoped_refptr<VideoTrackInterface> track = stream->GetVideoTracks()[0];

    RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
        transceiver_or_error = local->pc()->AddTransceiver(
            track, CreateTransceiverInit(init_layers));
    EXPECT_TRUE(transceiver_or_error.ok());
    return transceiver_or_error.value();
  }

  bool HasSenderVideoCodecCapability(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      absl::string_view codec_name) {
    std::vector<RtpCodecCapability> codecs =
        pc_wrapper->pc_factory()
            ->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO)
            .codecs;
    return std::find_if(codecs.begin(), codecs.end(),
                        [&codec_name](const RtpCodecCapability& codec) {
                          return absl::EqualsIgnoreCase(codec.name, codec_name);
                        }) != codecs.end();
  }

  std::vector<RtpCodecCapability> GetCapabilitiesAndRestrictToCodec(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      absl::string_view codec_name) {
    std::vector<RtpCodecCapability> codecs =
        pc_wrapper->pc_factory()
            ->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO)
            .codecs;
    codecs.erase(std::remove_if(codecs.begin(), codecs.end(),
                                [&codec_name](const RtpCodecCapability& codec) {
                                  return !IsReliabilityMechanism(codec) &&
                                         !absl::EqualsIgnoreCase(codec.name,
                                                                 codec_name);
                                }),
                 codecs.end());
    RTC_DCHECK(std::find_if(codecs.begin(), codecs.end(),
                            [&codec_name](const RtpCodecCapability& codec) {
                              return absl::EqualsIgnoreCase(codec.name,
                                                            codec_name);
                            }) != codecs.end());
    return codecs;
  }

  void ExchangeIceCandidates(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper,
      rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper) {
    local_pc_wrapper->SignalOnIceCandidateReady.connect(
        remote_pc_wrapper.get(), &PeerConnectionTestWrapper::AddIceCandidate);
    remote_pc_wrapper->SignalOnIceCandidateReady.connect(
        local_pc_wrapper.get(), &PeerConnectionTestWrapper::AddIceCandidate);
  }

  void NegotiateWithSimulcastTweaks(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper,
      rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper,
      std::vector<cricket::SimulcastLayer> init_layers) {
    // Create and set offer for `local_pc_wrapper`.
    std::unique_ptr<SessionDescriptionInterface> offer =
        CreateOffer(local_pc_wrapper);
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> p1 =
        SetLocalDescription(local_pc_wrapper, offer.get());
    // Modify the offer before handoff because `remote_pc_wrapper` only supports
    // receiving singlecast.
    cricket::SimulcastDescription simulcast_description =
        RemoveSimulcast(offer.get());
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> p2 =
        SetRemoteDescription(remote_pc_wrapper, offer.get());
    EXPECT_TRUE(Await({p1, p2}));

    // Create and set answer for `remote_pc_wrapper`.
    std::unique_ptr<SessionDescriptionInterface> answer =
        CreateAnswer(remote_pc_wrapper);
    p1 = SetLocalDescription(remote_pc_wrapper, answer.get());
    // Modify the answer before handoff because `local_pc_wrapper` should still
    // send simulcast.
    cricket::MediaContentDescription* mcd_answer =
        answer->description()->contents()[0].media_description();
    mcd_answer->mutable_streams().clear();
    std::vector<cricket::SimulcastLayer> simulcast_layers =
        simulcast_description.send_layers().GetAllLayers();
    cricket::SimulcastLayerList& receive_layers =
        mcd_answer->simulcast_description().receive_layers();
    for (const auto& layer : simulcast_layers) {
      receive_layers.AddLayer(layer);
    }
    p2 = SetRemoteDescription(local_pc_wrapper, answer.get());
    EXPECT_TRUE(Await({p1, p2}));
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStats(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto callback = rtc::make_ref_counted<MockRTCStatsCollectorCallback>();
    pc_wrapper->pc()->GetStats(callback.get());
    EXPECT_TRUE_WAIT(callback->called(), kDefaultTimeout.ms());
    return callback->report();
  }

  bool HasOutboundRtpBytesSent(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      size_t num_layers) {
    return HasOutboundRtpBytesSent(pc_wrapper, num_layers, num_layers);
  }

  bool HasOutboundRtpBytesSent(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      size_t num_layers,
      size_t num_active_layers) {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStats(pc_wrapper);
    std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
        report->GetStatsOfType<RTCOutboundRtpStreamStats>();
    if (outbound_rtps.size() != num_layers) {
      return false;
    }
    size_t num_sending_layers = 0;
    for (const auto* outbound_rtp : outbound_rtps) {
      if (outbound_rtp->bytes_sent.is_defined() &&
          *outbound_rtp->bytes_sent > 0u) {
        ++num_sending_layers;
      }
    }
    return num_sending_layers == num_active_layers;
  }

  bool HasOutboundRtpWithRidAndScalabilityMode(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      absl::string_view rid,
      absl::string_view expected_scalability_mode,
      uint32_t frame_height) {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStats(pc_wrapper);
    std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
        report->GetStatsOfType<RTCOutboundRtpStreamStats>();
    auto* outbound_rtp = FindOutboundRtpByRid(outbound_rtps, rid);
    if (!outbound_rtp || !outbound_rtp->scalability_mode.is_defined() ||
        *outbound_rtp->scalability_mode != expected_scalability_mode) {
      return false;
    }
    if (outbound_rtp->frame_height.is_defined()) {
      RTC_LOG(LS_INFO) << "Waiting for target resolution (" << frame_height
                       << "p). Currently at " << *outbound_rtp->frame_height
                       << "p...";
    } else {
      RTC_LOG(LS_INFO)
          << "Waiting for target resolution. No frames encoded yet...";
    }
    if (!outbound_rtp->frame_height.is_defined() ||
        *outbound_rtp->frame_height != frame_height) {
      // Sleep to avoid log spam when this is used in ASSERT_TRUE_WAIT().
      rtc::Thread::Current()->SleepMs(1000);
      return false;
    }
    return true;
  }

  bool OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      std::vector<RidAndResolution> resolutions) {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStats(pc_wrapper);
    std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
        report->GetStatsOfType<RTCOutboundRtpStreamStats>();
    for (const RidAndResolution& resolution : resolutions) {
      const RTCOutboundRtpStreamStats* outbound_rtp = nullptr;
      if (!resolution.rid.empty()) {
        outbound_rtp = FindOutboundRtpByRid(outbound_rtps, resolution.rid);
      } else if (outbound_rtps.size() == 1u) {
        outbound_rtp = outbound_rtps[0];
      }
      if (!outbound_rtp || !outbound_rtp->frame_width.is_defined() ||
          !outbound_rtp->frame_height.is_defined()) {
        // RTP not found by rid or has not encoded a frame yet.
        RTC_LOG(LS_ERROR) << "rid=" << resolution.rid << " does not have "
                          << "resolution metrics";
        return false;
      }
      if (*outbound_rtp->frame_width > resolution.width ||
          *outbound_rtp->frame_height > resolution.height) {
        RTC_LOG(LS_ERROR) << "rid=" << resolution.rid << " is "
                          << *outbound_rtp->frame_width << "x"
                          << *outbound_rtp->frame_height
                          << ", this is greater than the "
                          << "expected " << resolution.width << "x"
                          << resolution.height;
        return false;
      }
    }
    return true;
  }

 protected:
  std::unique_ptr<SessionDescriptionInterface> CreateOffer(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc_wrapper->pc()->CreateOffer(observer.get(), {});
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout.ms());
    return observer->MoveDescription();
  }

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc_wrapper->pc()->CreateAnswer(observer.get(), {});
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout.ms());
    return observer->MoveDescription();
  }

  rtc::scoped_refptr<MockSetSessionDescriptionObserver> SetLocalDescription(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      SessionDescriptionInterface* sdp) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    pc_wrapper->pc()->SetLocalDescription(
        observer.get(), CloneSessionDescription(sdp).release());
    return observer;
  }

  rtc::scoped_refptr<MockSetSessionDescriptionObserver> SetRemoteDescription(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      SessionDescriptionInterface* sdp) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    pc_wrapper->pc()->SetRemoteDescription(
        observer.get(), CloneSessionDescription(sdp).release());
    return observer;
  }

  // To avoid ICE candidates arriving before the remote endpoint has received
  // the offer it is important to SetLocalDescription() and
  // SetRemoteDescription() are kicked off without awaiting in-between. This
  // helper is used to await multiple observers.
  bool Await(std::vector<rtc::scoped_refptr<MockSetSessionDescriptionObserver>>
                 observers) {
    for (auto& observer : observers) {
      EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout.ms());
      if (!observer->result()) {
        return false;
      }
    }
    return true;
  }

  rtc::PhysicalSocketServer pss_;
  std::unique_ptr<rtc::Thread> background_thread_;
};

TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP8_SingleEncodingDefaultsToL1T1) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP8");
  transceiver->SetCodecPreferences(codecs);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T1"));
}

TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP8_RejectsSvcAndDefaultsToL1T1) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  // Restricting codecs restricts what SetParameters() will accept or reject.
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP8");
  transceiver->SetCodecPreferences(codecs);
  // Attempt SVC (L3T3_KEY). This is not possible because only VP8 is up for
  // negotiation and VP8 does not support it.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  parameters.encodings[0].scale_resolution_down_by = 1;
  EXPECT_FALSE(sender->SetParameters(parameters).ok());
  // `scalability_mode` remains unset because SetParameters() failed.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode, Eq(absl::nullopt));

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  // When `scalability_mode` is not set, VP8 defaults to L1T1.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T1"));
  // GetParameters() confirms `scalability_mode` is still not set.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode, Eq(absl::nullopt));
}

TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP8_FallbackFromSvcResultsInL1T2) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  // Verify test assumption that VP8 is first in the list, but don't modify the
  // codec preferences because we want the sender to think SVC is a possibility.
  std::vector<RtpCodecCapability> codecs =
      local_pc_wrapper->pc_factory()
          ->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO)
          .codecs;
  EXPECT_THAT(codecs[0].name, StrCaseEq("VP8"));
  // Attempt SVC (L3T3_KEY), which is not possible with VP8, but the sender does
  // not yet know which codec we'll use so the parameters will be accepted.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  parameters.encodings[0].scale_resolution_down_by = 1;
  EXPECT_TRUE(sender->SetParameters(parameters).ok());
  // Verify fallback has not happened yet.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L3T3_KEY")));

  // Negotiate, this results in VP8 being picked and fallback happening.
  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();
  // `scalaiblity_mode` is assigned the fallback value "L1T2" which is different
  // than the default of absl::nullopt.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L1T2")));

  // Wait until media is flowing, no significant time needed because we only
  // have one layer.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  // GetStats() confirms "L1T2" is used which is different than the "L1T1"
  // default or the "L3T3_KEY" that was attempted.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T2"));
}

// The legacy SVC path is triggered when VP9 us used, but `scalability_mode` has
// not been specified.
// TODO(https://crbug.com/webrtc/14889): When legacy VP9 SVC path has been
// deprecated and removed, update this test to assert that simulcast is used
// (i.e. VP9 is not treated differently than VP8).
TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP9_LegacySvcWhenScalabilityModeNotSpecified) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing. We only expect a single RTP stream.
  // We expect to see bytes flowing almost immediately on the lowest layer.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  // Wait until scalability mode is reported and expected resolution reached.
  // Ramp up time may be significant.
  ASSERT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(
                       local_pc_wrapper, "f", "L3T3_KEY", 720),
                   kLongTimeoutForRampingUp.ms());

  // Despite SVC being used on a single RTP stream, GetParameters() returns the
  // three encodings that we configured earlier (this is not spec-compliant but
  // it is how legacy SVC behaves).
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  std::vector<RtpEncodingParameters> encodings =
      sender->GetParameters().encodings;
  ASSERT_EQ(encodings.size(), 3u);
  // When legacy SVC is used, `scalability_mode` is not specified.
  EXPECT_FALSE(encodings[0].scalability_mode.has_value());
  EXPECT_FALSE(encodings[1].scalability_mode.has_value());
  EXPECT_FALSE(encodings[2].scalability_mode.has_value());
}

// The spec-compliant way to configure SVC for a single stream. The expected
// outcome is the same as for the legacy SVC case except that we only have one
// encoding in GetParameters().
TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP9_StandardSvcWithOnlyOneEncoding) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);
  // Configure SVC, a.k.a. "L3T3_KEY".
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  parameters.encodings[0].scale_resolution_down_by = 1;
  EXPECT_TRUE(sender->SetParameters(parameters).ok());

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing. We only expect a single RTP stream.
  // We expect to see bytes flowing almost immediately on the lowest layer.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP9"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L3T3_KEY"));

  // GetParameters() is consistent with what we asked for and got.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L3T3_KEY")));
}

// The {active,inactive,inactive} case is technically simulcast but since we
// only have one active stream, we're able to do SVC (multiple spatial layers
// is not supported if multiple encodings are active). The expected outcome is
// the same as above except we end up with two inactive RTP streams which are
// observable in GetStats().
TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP9_StandardSvcWithSingleActiveEncoding) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);
  // Configure SVC, a.k.a. "L3T3_KEY".
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  parameters.encodings[0].scale_resolution_down_by = 1;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  EXPECT_TRUE(sender->SetParameters(parameters).ok());

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Since the standard API is configuring simulcast we get three outbound-rtps,
  // but only one is active.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u, 1u),
                   kDefaultTimeout.ms());
  // Wait until scalability mode is reported and expected resolution reached.
  // Ramp up time is significant.
  ASSERT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(
                       local_pc_wrapper, "f", "L3T3_KEY", 720),
                   kLongTimeoutForRampingUp.ms());

  // GetParameters() is consistent with what we asked for and got.
  parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L3T3_KEY")));
  EXPECT_FALSE(parameters.encodings[1].scalability_mode.has_value());
  EXPECT_FALSE(parameters.encodings[2].scalability_mode.has_value());
}

// Exercise common path where `scalability_mode` is not specified until after
// negotiation, requring us to recreate the stream when the number of streams
// changes from 1 (legacy SVC) to 3 (standard simulcast).
TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP9_SwitchFromLegacySvcToStandardSingleActiveEncodingSvc) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // The original negotiation triggers legacy SVC because we didn't specify
  // any scalability mode.
  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Switch to the standard mode. Despite only having a single active stream in
  // both cases, this internally reconfigures from 1 stream to 3 streams.
  // Test coverage for https://crbug.com/webrtc/15016.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  parameters.encodings[0].active = true;
  parameters.encodings[0].scalability_mode = "L2T2_KEY";
  parameters.encodings[0].scale_resolution_down_by = 2.0;
  parameters.encodings[1].active = false;
  parameters.encodings[1].scalability_mode = absl::nullopt;
  parameters.encodings[2].active = false;
  parameters.encodings[2].scalability_mode = absl::nullopt;
  sender->SetParameters(parameters);

  // Since the standard API is configuring simulcast we get three outbound-rtps,
  // but only one is active.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u, 1u),
                   kDefaultTimeout.ms());
  // Wait until scalability mode is reported and expected resolution reached.
  // Ramp up time may be significant.
  ASSERT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(
                       local_pc_wrapper, "f", "L2T2_KEY", 720 / 2),
                   kLongTimeoutForRampingUp.ms());

  // GetParameters() does not report any fallback.
  parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L2T2_KEY")));
  EXPECT_FALSE(parameters.encodings[1].scalability_mode.has_value());
  EXPECT_FALSE(parameters.encodings[2].scalability_mode.has_value());
}

TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP9_AllLayersInactive_LegacySvc) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // Legacy SVC mode and all layers inactive.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  parameters.encodings[0].active = false;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Ensure no media is flowing (1 second should be enough).
  rtc::Thread::Current()->SleepMs(1000);
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_EQ(*outbound_rtps[0]->bytes_sent, 0u);
}

TEST_F(PeerConnectionEncodingsIntegrationTest,
       VP9_AllLayersInactive_StandardSvc) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // Standard mode and all layers inactive.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  parameters.encodings[0].scale_resolution_down_by = 1;
  parameters.encodings[0].active = false;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Ensure no media is flowing (1 second should be enough).
  rtc::Thread::Current()->SleepMs(1000);
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_EQ(*outbound_rtps[0]->bytes_sent, 0u);
  EXPECT_EQ(*outbound_rtps[1]->bytes_sent, 0u);
  EXPECT_EQ(*outbound_rtps[2]->bytes_sent, 0u);
}

TEST_F(PeerConnectionEncodingsIntegrationTest, VP9_TargetBitrate_LegacyL1T3) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // In legacy SVC, disabling the bottom two layers encodings is interpreted as
  // disabling the bottom two spatial layers resulting in L1T3.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  parameters.encodings[0].active = false;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = true;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until 720p L1T3 has ramped up to 720p. It may take additional time
  // for the target bitrate to reach its maximum.
  ASSERT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(local_pc_wrapper,
                                                           "f", "L1T3", 720),
                   kLongTimeoutForRampingUp.ms());

  // The target bitrate typically reaches `kVp9ExpectedMaxBitrateForL1T3`
  // in a short period of time. However to reduce risk of flakiness in bot
  // environments, this test only fails if we we exceed the expected target.
  rtc::Thread::Current()->SleepMs(1000);
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1));
  DataRate target_bitrate =
      DataRate::BitsPerSec(*outbound_rtps[0]->target_bitrate);
  EXPECT_LE(target_bitrate.kbps(), kVp9ExpectedMaxBitrateForL1T3.kbps());
}

// Test coverage for https://crbug.com/1455039.
TEST_F(PeerConnectionEncodingsIntegrationTest, VP9_TargetBitrate_StandardL1T3) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // With standard APIs, L1T3 is explicitly specified and the encodings refers
  // to the RTP streams, not the spatial layers. The end result should be
  // equivalent to the legacy L1T3 case.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  parameters.encodings[0].active = true;
  parameters.encodings[0].scale_resolution_down_by = 1.0;
  parameters.encodings[0].scalability_mode = "L1T3";
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until 720p L1T3 has ramped up to 720p. It may take additional time
  // for the target bitrate to reach its maximum.
  ASSERT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(local_pc_wrapper,
                                                           "f", "L1T3", 720),
                   kLongTimeoutForRampingUp.ms());

  // The target bitrate typically reaches `kVp9ExpectedMaxBitrateForL1T3`
  // in a short period of time. However to reduce risk of flakiness in bot
  // environments, this test only fails if we we exceed the expected target.
  rtc::Thread::Current()->SleepMs(1000);
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3));
  auto* outbound_rtp = FindOutboundRtpByRid(outbound_rtps, "f");
  ASSERT_TRUE(outbound_rtp);
  DataRate target_bitrate = DataRate::BitsPerSec(*outbound_rtp->target_bitrate);
  EXPECT_LE(target_bitrate.kbps(), kVp9ExpectedMaxBitrateForL1T3.kbps());
}

// Tests that use the standard path (specifying both `scalability_mode` and
// `scale_resolution_down_by`) should pass for all codecs.
class PeerConnectionEncodingsIntegrationParameterizedTest
    : public PeerConnectionEncodingsIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  PeerConnectionEncodingsIntegrationParameterizedTest()
      : codec_name_(GetParam()), mime_type_("video/" + codec_name_) {}

  // Work-around for the fact that whether or not AV1 is supported is not known
  // at compile-time so we have to skip tests early if missing.
  // TODO(https://crbug.com/webrtc/15011): Increase availability of AV1 or make
  // it possible to check support at compile-time.
  bool SkipTestDueToAv1Missing(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper) {
    if (codec_name_ == "AV1" &&
        !HasSenderVideoCodecCapability(local_pc_wrapper, "AV1")) {
      RTC_LOG(LS_WARNING) << "\n***\nAV1 is not available, skipping test.\n***";
      return true;
    }
    return false;
  }

 protected:
  const std::string codec_name_;  // E.g. "VP9"
  const std::string mime_type_;   // E.g. "video/VP9"
};

TEST_P(PeerConnectionEncodingsIntegrationParameterizedTest, AllLayersInactive) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  if (SkipTestDueToAv1Missing(local_pc_wrapper)) {
    return;
  }
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, codec_name_);
  transceiver->SetCodecPreferences(codecs);

  // Standard mode and all layers inactive.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  parameters.encodings[0].scalability_mode = "L1T3";
  parameters.encodings[0].scale_resolution_down_by = 1;
  parameters.encodings[0].active = false;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Ensure no media is flowing (1 second should be enough).
  rtc::Thread::Current()->SleepMs(1000);
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_EQ(*outbound_rtps[0]->bytes_sent, 0u);
  EXPECT_EQ(*outbound_rtps[1]->bytes_sent, 0u);
  EXPECT_EQ(*outbound_rtps[2]->bytes_sent, 0u);
}

TEST_P(PeerConnectionEncodingsIntegrationParameterizedTest, Simulcast) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  if (SkipTestDueToAv1Missing(local_pc_wrapper)) {
    return;
  }
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<cricket::SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, codec_name_);
  transceiver->SetCodecPreferences(codecs);

  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  parameters.encodings[0].scalability_mode = "L1T3";
  parameters.encodings[0].scale_resolution_down_by = 4;
  parameters.encodings[1].scalability_mode = "L1T3";
  parameters.encodings[1].scale_resolution_down_by = 2;
  parameters.encodings[2].scalability_mode = "L1T3";
  parameters.encodings[2].scale_resolution_down_by = 1;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // GetParameters() does not report any fallback.
  parameters = sender->GetParameters();
  ASSERT_THAT(parameters.encodings, SizeIs(3));
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L1T3")));
  EXPECT_THAT(parameters.encodings[1].scalability_mode,
              Optional(std::string("L1T3")));
  EXPECT_THAT(parameters.encodings[2].scalability_mode,
              Optional(std::string("L1T3")));

  // Wait until media is flowing on all three layers.
  // Ramp up time is needed before all three layers are sending.
  ASSERT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u),
                   kLongTimeoutForRampingUp.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"f", 320, 180}, {"h", 640, 360}, {"q", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq(mime_type_));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[1]),
              StrCaseEq(mime_type_));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[2]),
              StrCaseEq(mime_type_));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[1]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[2]->scalability_mode, StrEq("L1T3"));
}

INSTANTIATE_TEST_SUITE_P(StandardPath,
                         PeerConnectionEncodingsIntegrationParameterizedTest,
                         ::testing::Values("VP8",
                                           "VP9",
#if defined(WEBRTC_USE_H264)
                                           "H264",
#endif  // defined(WEBRTC_USE_H264)
                                           "AV1"),
                         StringParamToString());

}  // namespace webrtc
