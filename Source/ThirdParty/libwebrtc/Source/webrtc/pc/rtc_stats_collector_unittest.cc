/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtc_stats_collector.h"

#include <ctype.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_replace.h"
#include "api/rtp_parameters.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "pc/media_stream.h"
#include "pc/media_stream_track.h"
#include "pc/test/fake_peer_connection_for_stats.h"
#include "pc/test/mock_data_channel.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "pc/test/rtc_stats_obtainer.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_ssl_identity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/time_utils.h"

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;

namespace webrtc {

// These are used by gtest code, such as if |EXPECT_EQ| fails.
void PrintTo(const RTCCertificateStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCCodecStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCDataChannelStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCIceCandidatePairStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCLocalIceCandidateStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCRemoteIceCandidateStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCPeerConnectionStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCMediaStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCMediaStreamTrackStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCInboundRTPStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCOutboundRTPStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCRemoteInboundRtpStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCAudioSourceStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCVideoSourceStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

void PrintTo(const RTCTransportStats& stats, ::std::ostream* os) {
  *os << stats.ToJson();
}

namespace {

const int64_t kGetStatsReportTimeoutMs = 1000;

struct CertificateInfo {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  std::vector<std::string> ders;
  std::vector<std::string> pems;
  std::vector<std::string> fingerprints;
};

// Return the ID for an object of the given type in a report.
// The object must be present and be unique.
template <typename T>
std::string IdForType(const RTCStatsReport* report) {
  auto stats_of_my_type = report->RTCStatsReport::GetStatsOfType<T>();
  // We cannot use ASSERT here, since we're within a function.
  EXPECT_EQ(1U, stats_of_my_type.size())
      << "Unexpected number of stats of this type";
  if (stats_of_my_type.size() == 1) {
    return stats_of_my_type[0]->id();
  } else {
    // Return something that is not going to be a valid stas ID.
    return "Type not found";
  }
}

std::unique_ptr<CertificateInfo> CreateFakeCertificateAndInfoFromDers(
    const std::vector<std::string>& ders) {
  RTC_CHECK(!ders.empty());
  std::unique_ptr<CertificateInfo> info(new CertificateInfo());
  info->ders = ders;
  for (const std::string& der : ders) {
    info->pems.push_back(rtc::SSLIdentity::DerToPem(
        "CERTIFICATE", reinterpret_cast<const unsigned char*>(der.c_str()),
        der.length()));
  }
  info->certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::FakeSSLIdentity>(
          new rtc::FakeSSLIdentity(info->pems)));
  // Strip header/footer and newline characters of PEM strings.
  for (size_t i = 0; i < info->pems.size(); ++i) {
    absl::StrReplaceAll({{"-----BEGIN CERTIFICATE-----", ""},
                         {"-----END CERTIFICATE-----", ""},
                         {"\n", ""}},
                        &info->pems[i]);
  }
  // Fingerprints for the whole certificate chain, starting with leaf
  // certificate.
  const rtc::SSLCertChain& chain = info->certificate->GetSSLCertificateChain();
  std::unique_ptr<rtc::SSLFingerprint> fp;
  for (size_t i = 0; i < chain.GetSize(); i++) {
    fp = rtc::SSLFingerprint::Create("sha-1", chain.Get(i));
    EXPECT_TRUE(fp);
    info->fingerprints.push_back(fp->GetRfc4572Fingerprint());
  }
  EXPECT_EQ(info->ders.size(), info->fingerprints.size());
  return info;
}

std::unique_ptr<cricket::Candidate> CreateFakeCandidate(
    const std::string& hostname,
    int port,
    const std::string& protocol,
    const rtc::AdapterType adapter_type,
    const std::string& candidate_type,
    uint32_t priority) {
  std::unique_ptr<cricket::Candidate> candidate(new cricket::Candidate());
  candidate->set_address(rtc::SocketAddress(hostname, port));
  candidate->set_protocol(protocol);
  candidate->set_network_type(adapter_type);
  candidate->set_type(candidate_type);
  candidate->set_priority(priority);
  return candidate;
}

class FakeAudioTrackForStats : public MediaStreamTrack<AudioTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeAudioTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state) {
    rtc::scoped_refptr<FakeAudioTrackForStats> audio_track_stats(
        new rtc::RefCountedObject<FakeAudioTrackForStats>(id));
    audio_track_stats->set_state(state);
    return audio_track_stats;
  }

  explicit FakeAudioTrackForStats(const std::string& id)
      : MediaStreamTrack<AudioTrackInterface>(id) {}

  std::string kind() const override {
    return MediaStreamTrackInterface::kAudioKind;
  }
  webrtc::AudioSourceInterface* GetSource() const override { return nullptr; }
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {}
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {}
  bool GetSignalLevel(int* level) override { return false; }
  rtc::scoped_refptr<AudioProcessorInterface> GetAudioProcessor() override {
    return nullptr;
  }
};

class FakeVideoTrackSourceForStats : public VideoTrackSourceInterface {
 public:
  static rtc::scoped_refptr<FakeVideoTrackSourceForStats> Create(
      int input_width,
      int input_height) {
    return rtc::scoped_refptr<FakeVideoTrackSourceForStats>(
        new rtc::RefCountedObject<FakeVideoTrackSourceForStats>(input_width,
                                                                input_height));
  }

  FakeVideoTrackSourceForStats(int input_width, int input_height)
      : input_width_(input_width), input_height_(input_height) {}
  ~FakeVideoTrackSourceForStats() override {}

  // VideoTrackSourceInterface
  bool is_screencast() const override { return false; }
  absl::optional<bool> needs_denoising() const override { return false; }
  bool GetStats(VideoTrackSourceInterface::Stats* stats) override {
    stats->input_width = input_width_;
    stats->input_height = input_height_;
    return true;
  }
  // MediaSourceInterface (part of VideoTrackSourceInterface)
  MediaSourceInterface::SourceState state() const override {
    return MediaSourceInterface::SourceState::kLive;
  }
  bool remote() const override { return false; }
  // NotifierInterface (part of MediaSourceInterface)
  void RegisterObserver(ObserverInterface* observer) override {}
  void UnregisterObserver(ObserverInterface* observer) override {}
  // rtc::VideoSourceInterface<VideoFrame> (part of VideoTrackSourceInterface)
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {}

 private:
  int input_width_;
  int input_height_;
};

class FakeVideoTrackForStats : public MediaStreamTrack<VideoTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeVideoTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state,
      rtc::scoped_refptr<VideoTrackSourceInterface> source) {
    rtc::scoped_refptr<FakeVideoTrackForStats> video_track(
        new rtc::RefCountedObject<FakeVideoTrackForStats>(id,
                                                          std::move(source)));
    video_track->set_state(state);
    return video_track;
  }

  FakeVideoTrackForStats(const std::string& id,
                         rtc::scoped_refptr<VideoTrackSourceInterface> source)
      : MediaStreamTrack<VideoTrackInterface>(id), source_(source) {}

  std::string kind() const override {
    return MediaStreamTrackInterface::kVideoKind;
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {}

  VideoTrackSourceInterface* GetSource() const override {
    return source_.get();
  }

 private:
  rtc::scoped_refptr<VideoTrackSourceInterface> source_;
};

rtc::scoped_refptr<MediaStreamTrackInterface> CreateFakeTrack(
    cricket::MediaType media_type,
    const std::string& track_id,
    MediaStreamTrackInterface::TrackState track_state) {
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    return FakeAudioTrackForStats::Create(track_id, track_state);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    return FakeVideoTrackForStats::Create(track_id, track_state, nullptr);
  }
}

rtc::scoped_refptr<MockRtpSenderInternal> CreateMockSender(
    cricket::MediaType media_type,
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    uint32_t ssrc,
    int attachment_id,
    std::vector<std::string> local_stream_ids) {
  RTC_DCHECK(!track ||
             (track->kind() == MediaStreamTrackInterface::kAudioKind &&
              media_type == cricket::MEDIA_TYPE_AUDIO) ||
             (track->kind() == MediaStreamTrackInterface::kVideoKind &&
              media_type == cricket::MEDIA_TYPE_VIDEO));
  rtc::scoped_refptr<MockRtpSenderInternal> sender(
      new rtc::RefCountedObject<MockRtpSenderInternal>());
  EXPECT_CALL(*sender, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*sender, ssrc()).WillRepeatedly(Return(ssrc));
  EXPECT_CALL(*sender, media_type()).WillRepeatedly(Return(media_type));
  EXPECT_CALL(*sender, GetParameters()).WillRepeatedly(Invoke([ssrc]() {
    RtpParameters params;
    params.encodings.push_back(RtpEncodingParameters());
    params.encodings[0].ssrc = ssrc;
    return params;
  }));
  EXPECT_CALL(*sender, AttachmentId()).WillRepeatedly(Return(attachment_id));
  EXPECT_CALL(*sender, stream_ids()).WillRepeatedly(Return(local_stream_ids));
  return sender;
}

rtc::scoped_refptr<MockRtpReceiverInternal> CreateMockReceiver(
    const rtc::scoped_refptr<MediaStreamTrackInterface>& track,
    uint32_t ssrc,
    int attachment_id) {
  rtc::scoped_refptr<MockRtpReceiverInternal> receiver(
      new rtc::RefCountedObject<MockRtpReceiverInternal>());
  EXPECT_CALL(*receiver, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*receiver, streams())
      .WillRepeatedly(
          Return(std::vector<rtc::scoped_refptr<MediaStreamInterface>>({})));

  EXPECT_CALL(*receiver, media_type())
      .WillRepeatedly(
          Return(track->kind() == MediaStreamTrackInterface::kAudioKind
                     ? cricket::MEDIA_TYPE_AUDIO
                     : cricket::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(*receiver, GetParameters()).WillRepeatedly(Invoke([ssrc]() {
    RtpParameters params;
    params.encodings.push_back(RtpEncodingParameters());
    params.encodings[0].ssrc = ssrc;
    return params;
  }));
  EXPECT_CALL(*receiver, AttachmentId()).WillRepeatedly(Return(attachment_id));
  return receiver;
}

class RTCStatsCollectorWrapper {
 public:
  explicit RTCStatsCollectorWrapper(
      rtc::scoped_refptr<FakePeerConnectionForStats> pc)
      : pc_(pc),
        stats_collector_(
            RTCStatsCollector::Create(pc, 50 * rtc::kNumMicrosecsPerMillisec)) {
  }

  rtc::scoped_refptr<RTCStatsCollector> stats_collector() {
    return stats_collector_;
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsReport() {
    rtc::scoped_refptr<RTCStatsObtainer> callback = RTCStatsObtainer::Create();
    stats_collector_->GetStatsReport(callback);
    return WaitForReport(callback);
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsReportWithSenderSelector(
      rtc::scoped_refptr<RtpSenderInternal> selector) {
    rtc::scoped_refptr<RTCStatsObtainer> callback = RTCStatsObtainer::Create();
    stats_collector_->GetStatsReport(selector, callback);
    return WaitForReport(callback);
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsReportWithReceiverSelector(
      rtc::scoped_refptr<RtpReceiverInternal> selector) {
    rtc::scoped_refptr<RTCStatsObtainer> callback = RTCStatsObtainer::Create();
    stats_collector_->GetStatsReport(selector, callback);
    return WaitForReport(callback);
  }

  rtc::scoped_refptr<const RTCStatsReport> GetFreshStatsReport() {
    stats_collector_->ClearCachedStatsReport();
    return GetStatsReport();
  }

  rtc::scoped_refptr<MockRtpSenderInternal> SetupLocalTrackAndSender(
      cricket::MediaType media_type,
      const std::string& track_id,
      uint32_t ssrc,
      bool add_stream,
      int attachment_id) {
    rtc::scoped_refptr<MediaStream> local_stream;
    if (add_stream) {
      local_stream = MediaStream::Create("LocalStreamId");
      pc_->mutable_local_streams()->AddStream(local_stream);
    }

    rtc::scoped_refptr<MediaStreamTrackInterface> track;
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      track = CreateFakeTrack(media_type, track_id,
                              MediaStreamTrackInterface::kLive);
      if (add_stream) {
        local_stream->AddTrack(static_cast<AudioTrackInterface*>(track.get()));
      }
    } else {
      track = CreateFakeTrack(media_type, track_id,
                              MediaStreamTrackInterface::kLive);
      if (add_stream) {
        local_stream->AddTrack(static_cast<VideoTrackInterface*>(track.get()));
      }
    }

    rtc::scoped_refptr<MockRtpSenderInternal> sender =
        CreateMockSender(media_type, track, ssrc, attachment_id, {});
    pc_->AddSender(sender);
    return sender;
  }

  rtc::scoped_refptr<MockRtpReceiverInternal> SetupRemoteTrackAndReceiver(
      cricket::MediaType media_type,
      const std::string& track_id,
      const std::string& stream_id,
      uint32_t ssrc) {
    rtc::scoped_refptr<MediaStream> remote_stream =
        MediaStream::Create(stream_id);
    pc_->mutable_remote_streams()->AddStream(remote_stream);

    rtc::scoped_refptr<MediaStreamTrackInterface> track;
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      track = CreateFakeTrack(media_type, track_id,
                              MediaStreamTrackInterface::kLive);
      remote_stream->AddTrack(static_cast<AudioTrackInterface*>(track.get()));
    } else {
      track = CreateFakeTrack(media_type, track_id,
                              MediaStreamTrackInterface::kLive);
      remote_stream->AddTrack(static_cast<VideoTrackInterface*>(track.get()));
    }

    rtc::scoped_refptr<MockRtpReceiverInternal> receiver =
        CreateMockReceiver(track, ssrc, 62);
    EXPECT_CALL(*receiver, streams())
        .WillRepeatedly(
            Return(std::vector<rtc::scoped_refptr<MediaStreamInterface>>(
                {remote_stream})));
    pc_->AddReceiver(receiver);
    return receiver;
  }

  // Attaches tracks to peer connections by configuring RTP senders and RTP
  // receivers according to the tracks' pairings with
  // |[Voice/Video][Sender/Receiver]Info| and their SSRCs. Local tracks can be
  // associated with multiple |[Voice/Video]SenderInfo|s, remote tracks can only
  // be associated with one |[Voice/Video]ReceiverInfo|.
  // Senders get assigned attachment ID "ssrc + 10".
  void CreateMockRtpSendersReceiversAndChannels(
      std::initializer_list<
          std::pair<MediaStreamTrackInterface*, cricket::VoiceSenderInfo>>
          local_audio_track_info_pairs,
      std::initializer_list<
          std::pair<MediaStreamTrackInterface*, cricket::VoiceReceiverInfo>>
          remote_audio_track_info_pairs,
      std::initializer_list<
          std::pair<MediaStreamTrackInterface*, cricket::VideoSenderInfo>>
          local_video_track_info_pairs,
      std::initializer_list<
          std::pair<MediaStreamTrackInterface*, cricket::VideoReceiverInfo>>
          remote_video_track_info_pairs,
      std::vector<std::string> local_stream_ids,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> remote_streams) {
    cricket::VoiceMediaInfo voice_media_info;
    cricket::VideoMediaInfo video_media_info;

    // Local audio tracks and voice sender infos
    for (auto& pair : local_audio_track_info_pairs) {
      MediaStreamTrackInterface* local_audio_track = pair.first;
      const cricket::VoiceSenderInfo& voice_sender_info = pair.second;
      RTC_DCHECK_EQ(local_audio_track->kind(),
                    MediaStreamTrackInterface::kAudioKind);

      voice_media_info.senders.push_back(voice_sender_info);
      rtc::scoped_refptr<MockRtpSenderInternal> rtp_sender = CreateMockSender(
          cricket::MEDIA_TYPE_AUDIO,
          rtc::scoped_refptr<MediaStreamTrackInterface>(local_audio_track),
          voice_sender_info.local_stats[0].ssrc,
          voice_sender_info.local_stats[0].ssrc + 10, local_stream_ids);
      pc_->AddSender(rtp_sender);
    }

    // Remote audio tracks and voice receiver infos
    for (auto& pair : remote_audio_track_info_pairs) {
      MediaStreamTrackInterface* remote_audio_track = pair.first;
      const cricket::VoiceReceiverInfo& voice_receiver_info = pair.second;
      RTC_DCHECK_EQ(remote_audio_track->kind(),
                    MediaStreamTrackInterface::kAudioKind);

      voice_media_info.receivers.push_back(voice_receiver_info);
      rtc::scoped_refptr<MockRtpReceiverInternal> rtp_receiver =
          CreateMockReceiver(
              rtc::scoped_refptr<MediaStreamTrackInterface>(remote_audio_track),
              voice_receiver_info.local_stats[0].ssrc,
              voice_receiver_info.local_stats[0].ssrc + 10);
      EXPECT_CALL(*rtp_receiver, streams())
          .WillRepeatedly(Return(remote_streams));
      pc_->AddReceiver(rtp_receiver);
    }

    // Local video tracks and video sender infos
    for (auto& pair : local_video_track_info_pairs) {
      MediaStreamTrackInterface* local_video_track = pair.first;
      const cricket::VideoSenderInfo& video_sender_info = pair.second;
      RTC_DCHECK_EQ(local_video_track->kind(),
                    MediaStreamTrackInterface::kVideoKind);

      video_media_info.senders.push_back(video_sender_info);
      rtc::scoped_refptr<MockRtpSenderInternal> rtp_sender = CreateMockSender(
          cricket::MEDIA_TYPE_VIDEO,
          rtc::scoped_refptr<MediaStreamTrackInterface>(local_video_track),
          video_sender_info.local_stats[0].ssrc,
          video_sender_info.local_stats[0].ssrc + 10, local_stream_ids);
      pc_->AddSender(rtp_sender);
    }

    // Remote video tracks and video receiver infos
    for (auto& pair : remote_video_track_info_pairs) {
      MediaStreamTrackInterface* remote_video_track = pair.first;
      const cricket::VideoReceiverInfo& video_receiver_info = pair.second;
      RTC_DCHECK_EQ(remote_video_track->kind(),
                    MediaStreamTrackInterface::kVideoKind);

      video_media_info.receivers.push_back(video_receiver_info);
      rtc::scoped_refptr<MockRtpReceiverInternal> rtp_receiver =
          CreateMockReceiver(
              rtc::scoped_refptr<MediaStreamTrackInterface>(remote_video_track),
              video_receiver_info.local_stats[0].ssrc,
              video_receiver_info.local_stats[0].ssrc + 10);
      EXPECT_CALL(*rtp_receiver, streams())
          .WillRepeatedly(Return(remote_streams));
      pc_->AddReceiver(rtp_receiver);
    }

    auto* voice_media_channel = pc_->AddVoiceChannel("audio", "transport");
    voice_media_channel->SetStats(voice_media_info);

    auto* video_media_channel = pc_->AddVideoChannel("video", "transport");
    video_media_channel->SetStats(video_media_info);
  }

 private:
  rtc::scoped_refptr<const RTCStatsReport> WaitForReport(
      rtc::scoped_refptr<RTCStatsObtainer> callback) {
    EXPECT_TRUE_WAIT(callback->report(), kGetStatsReportTimeoutMs);
    int64_t after = rtc::TimeUTCMicros();
    for (const RTCStats& stats : *callback->report()) {
      EXPECT_LE(stats.timestamp_us(), after);
    }
    return callback->report();
  }

  rtc::scoped_refptr<FakePeerConnectionForStats> pc_;
  rtc::scoped_refptr<RTCStatsCollector> stats_collector_;
};

class RTCStatsCollectorTest : public ::testing::Test {
 public:
  RTCStatsCollectorTest()
      : pc_(new rtc::RefCountedObject<FakePeerConnectionForStats>()),
        stats_(new RTCStatsCollectorWrapper(pc_)) {}

  void ExpectReportContainsCertificateInfo(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const CertificateInfo& certinfo) {
    for (size_t i = 0; i < certinfo.fingerprints.size(); ++i) {
      RTCCertificateStats expected_certificate_stats(
          "RTCCertificate_" + certinfo.fingerprints[i], report->timestamp_us());
      expected_certificate_stats.fingerprint = certinfo.fingerprints[i];
      expected_certificate_stats.fingerprint_algorithm = "sha-1";
      expected_certificate_stats.base64_certificate = certinfo.pems[i];
      if (i + 1 < certinfo.fingerprints.size()) {
        expected_certificate_stats.issuer_certificate_id =
            "RTCCertificate_" + certinfo.fingerprints[i + 1];
      }
      ASSERT_TRUE(report->Get(expected_certificate_stats.id()));
      EXPECT_EQ(expected_certificate_stats,
                report->Get(expected_certificate_stats.id())
                    ->cast_to<RTCCertificateStats>());
    }
  }

  struct ExampleStatsGraph {
    rtc::scoped_refptr<RtpSenderInternal> sender;
    rtc::scoped_refptr<RtpReceiverInternal> receiver;

    rtc::scoped_refptr<const RTCStatsReport> full_report;
    std::string send_codec_id;
    std::string recv_codec_id;
    std::string outbound_rtp_id;
    std::string inbound_rtp_id;
    std::string transport_id;
    std::string sender_track_id;
    std::string receiver_track_id;
    std::string remote_stream_id;
    std::string peer_connection_id;
    std::string media_source_id;
  };

  // Sets up the example stats graph (see ASCII art below) used for testing the
  // stats selection algorithm,
  // https://w3c.github.io/webrtc-pc/#dfn-stats-selection-algorithm.
  // These tests test the integration of the stats traversal algorithm inside of
  // RTCStatsCollector. See rtcstatstraveral_unittest.cc for more stats
  // traversal tests.
  ExampleStatsGraph SetupExampleStatsGraphForSelectorTests() {
    ExampleStatsGraph graph;

    // codec (send)
    graph.send_codec_id = "RTCCodec_VideoMid_Outbound_1";
    cricket::VideoMediaInfo video_media_info;
    RtpCodecParameters send_codec;
    send_codec.payload_type = 1;
    send_codec.clock_rate = 0;
    video_media_info.send_codecs.insert(
        std::make_pair(send_codec.payload_type, send_codec));
    // codec (recv)
    graph.recv_codec_id = "RTCCodec_VideoMid_Inbound_2";
    RtpCodecParameters recv_codec;
    recv_codec.payload_type = 2;
    recv_codec.clock_rate = 0;
    video_media_info.receive_codecs.insert(
        std::make_pair(recv_codec.payload_type, recv_codec));
    // outbound-rtp
    graph.outbound_rtp_id = "RTCOutboundRTPVideoStream_3";
    video_media_info.senders.push_back(cricket::VideoSenderInfo());
    video_media_info.senders[0].local_stats.push_back(
        cricket::SsrcSenderInfo());
    video_media_info.senders[0].local_stats[0].ssrc = 3;
    video_media_info.senders[0].codec_payload_type = send_codec.payload_type;
    // inbound-rtp
    graph.inbound_rtp_id = "RTCInboundRTPVideoStream_4";
    video_media_info.receivers.push_back(cricket::VideoReceiverInfo());
    video_media_info.receivers[0].local_stats.push_back(
        cricket::SsrcReceiverInfo());
    video_media_info.receivers[0].local_stats[0].ssrc = 4;
    video_media_info.receivers[0].codec_payload_type = recv_codec.payload_type;
    // transport
    graph.transport_id = "RTCTransport_TransportName_1";
    auto* video_media_channel =
        pc_->AddVideoChannel("VideoMid", "TransportName");
    video_media_channel->SetStats(video_media_info);
    // track (sender)
    graph.sender = stats_->SetupLocalTrackAndSender(
        cricket::MEDIA_TYPE_VIDEO, "LocalVideoTrackID", 3, false, 50);
    graph.sender_track_id = "RTCMediaStreamTrack_sender_" +
                            rtc::ToString(graph.sender->AttachmentId());
    // track (receiver) and stream (remote stream)
    graph.receiver = stats_->SetupRemoteTrackAndReceiver(
        cricket::MEDIA_TYPE_VIDEO, "RemoteVideoTrackID", "RemoteStreamId", 4);
    graph.receiver_track_id = "RTCMediaStreamTrack_receiver_" +
                              rtc::ToString(graph.receiver->AttachmentId());
    graph.remote_stream_id = "RTCMediaStream_RemoteStreamId";
    // peer-connection
    graph.peer_connection_id = "RTCPeerConnection";
    // media-source (kind: video)
    graph.media_source_id =
        "RTCVideoSource_" + rtc::ToString(graph.sender->AttachmentId());

    // Expected stats graph:
    //
    //  +--- track (sender)      stream (remote stream) ---> track (receiver)
    //  |             ^                                        ^
    //  |             |                                        |
    //  | +--------- outbound-rtp   inbound-rtp ---------------+
    //  | |           |        |     |       |
    //  | |           v        v     v       v
    //  | |  codec (send)     transport     codec (recv)     peer-connection
    //  v v
    //  media-source

    // Verify the stats graph is set up correctly.
    graph.full_report = stats_->GetStatsReport();
    EXPECT_EQ(graph.full_report->size(), 10u);
    EXPECT_TRUE(graph.full_report->Get(graph.send_codec_id));
    EXPECT_TRUE(graph.full_report->Get(graph.recv_codec_id));
    EXPECT_TRUE(graph.full_report->Get(graph.outbound_rtp_id));
    EXPECT_TRUE(graph.full_report->Get(graph.inbound_rtp_id));
    EXPECT_TRUE(graph.full_report->Get(graph.transport_id));
    EXPECT_TRUE(graph.full_report->Get(graph.sender_track_id));
    EXPECT_TRUE(graph.full_report->Get(graph.receiver_track_id));
    EXPECT_TRUE(graph.full_report->Get(graph.remote_stream_id));
    EXPECT_TRUE(graph.full_report->Get(graph.peer_connection_id));
    EXPECT_TRUE(graph.full_report->Get(graph.media_source_id));
    const auto& sender_track = graph.full_report->Get(graph.sender_track_id)
                                   ->cast_to<RTCMediaStreamTrackStats>();
    EXPECT_EQ(*sender_track.media_source_id, graph.media_source_id);
    const auto& outbound_rtp = graph.full_report->Get(graph.outbound_rtp_id)
                                   ->cast_to<RTCOutboundRTPStreamStats>();
    EXPECT_EQ(*outbound_rtp.media_source_id, graph.media_source_id);
    EXPECT_EQ(*outbound_rtp.codec_id, graph.send_codec_id);
    EXPECT_EQ(*outbound_rtp.track_id, graph.sender_track_id);
    EXPECT_EQ(*outbound_rtp.transport_id, graph.transport_id);
    const auto& inbound_rtp = graph.full_report->Get(graph.inbound_rtp_id)
                                  ->cast_to<RTCInboundRTPStreamStats>();
    EXPECT_EQ(*inbound_rtp.codec_id, graph.recv_codec_id);
    EXPECT_EQ(*inbound_rtp.track_id, graph.receiver_track_id);
    EXPECT_EQ(*inbound_rtp.transport_id, graph.transport_id);

    return graph;
  }

 protected:
  rtc::ScopedFakeClock fake_clock_;
  rtc::scoped_refptr<FakePeerConnectionForStats> pc_;
  std::unique_ptr<RTCStatsCollectorWrapper> stats_;
};

TEST_F(RTCStatsCollectorTest, SingleCallback) {
  rtc::scoped_refptr<const RTCStatsReport> result;
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&result));
  EXPECT_TRUE_WAIT(result, kGetStatsReportTimeoutMs);
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacks) {
  rtc::scoped_refptr<const RTCStatsReport> a, b, c;
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&a));
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&b));
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);

  EXPECT_EQ(a.get(), b.get());
  EXPECT_EQ(b.get(), c.get());
}

TEST_F(RTCStatsCollectorTest, CachedStatsReports) {
  // Caching should ensure |a| and |b| are the same report.
  rtc::scoped_refptr<const RTCStatsReport> a = stats_->GetStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> b = stats_->GetStatsReport();
  EXPECT_EQ(a.get(), b.get());
  // Invalidate cache by clearing it.
  stats_->stats_collector()->ClearCachedStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> c = stats_->GetStatsReport();
  EXPECT_NE(b.get(), c.get());
  // Invalidate cache by advancing time.
  fake_clock_.AdvanceTime(TimeDelta::Millis(51));
  rtc::scoped_refptr<const RTCStatsReport> d = stats_->GetStatsReport();
  EXPECT_TRUE(d);
  EXPECT_NE(c.get(), d.get());
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacksWithInvalidatedCacheInBetween) {
  rtc::scoped_refptr<const RTCStatsReport> a, b, c;
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&a));
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&b));
  // Cache is invalidated after 50 ms.
  fake_clock_.AdvanceTime(TimeDelta::Millis(51));
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);
  EXPECT_EQ(a.get(), b.get());
  // The act of doing |AdvanceTime| processes all messages. If this was not the
  // case we might not require |c| to be fresher than |b|.
  EXPECT_NE(c.get(), b.get());
}

TEST_F(RTCStatsCollectorTest, ToJsonProducesParseableJson) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  std::string json_format = report->ToJson();
  Json::Reader reader;
  Json::Value json_value;
  ASSERT_TRUE(reader.parse(json_format, json_value));
  // A very brief sanity check on the result.
  EXPECT_EQ(report->size(), json_value.size());
}

TEST_F(RTCStatsCollectorTest, CollectRTCCertificateStatsSingle) {
  const char kTransportName[] = "transport";

  pc_->AddVoiceChannel("audio", kTransportName);

  std::unique_ptr<CertificateInfo> local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({"(local) single certificate"}));
  pc_->SetLocalCertificate(kTransportName, local_certinfo->certificate);

  std::unique_ptr<CertificateInfo> remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({"(remote) single certificate"}));
  pc_->SetRemoteCertChain(
      kTransportName,
      remote_certinfo->certificate->GetSSLCertificateChain().Clone());

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  ExpectReportContainsCertificateInfo(report, *local_certinfo);
  ExpectReportContainsCertificateInfo(report, *remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectRTCCodecStats) {
  // Audio
  cricket::VoiceMediaInfo voice_media_info;

  RtpCodecParameters inbound_audio_codec;
  inbound_audio_codec.payload_type = 1;
  inbound_audio_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  inbound_audio_codec.name = "opus";
  inbound_audio_codec.clock_rate = 1337;
  inbound_audio_codec.num_channels = 1;
  inbound_audio_codec.parameters = {{"minptime", "10"}, {"useinbandfec", "1"}};
  voice_media_info.receive_codecs.insert(
      std::make_pair(inbound_audio_codec.payload_type, inbound_audio_codec));

  RtpCodecParameters outbound_audio_codec;
  outbound_audio_codec.payload_type = 2;
  outbound_audio_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  outbound_audio_codec.name = "isac";
  outbound_audio_codec.clock_rate = 1338;
  outbound_audio_codec.num_channels = 2;
  voice_media_info.send_codecs.insert(
      std::make_pair(outbound_audio_codec.payload_type, outbound_audio_codec));

  auto* voice_media_channel = pc_->AddVoiceChannel("AudioMid", "TransportName");
  voice_media_channel->SetStats(voice_media_info);

  // Video
  cricket::VideoMediaInfo video_media_info;

  RtpCodecParameters inbound_video_codec;
  inbound_video_codec.payload_type = 3;
  inbound_video_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  inbound_video_codec.name = "H264";
  inbound_video_codec.clock_rate = 1339;
  inbound_video_codec.parameters = {{"level-asymmetry-allowed", "1"},
                                    {"packetization-mode", "1"},
                                    {"profile-level-id", "42001f"}};
  video_media_info.receive_codecs.insert(
      std::make_pair(inbound_video_codec.payload_type, inbound_video_codec));

  RtpCodecParameters outbound_video_codec;
  outbound_video_codec.payload_type = 4;
  outbound_video_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  outbound_video_codec.name = "VP8";
  outbound_video_codec.clock_rate = 1340;
  video_media_info.send_codecs.insert(
      std::make_pair(outbound_video_codec.payload_type, outbound_video_codec));

  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCCodecStats expected_inbound_audio_codec("RTCCodec_AudioMid_Inbound_1",
                                             report->timestamp_us());
  expected_inbound_audio_codec.payload_type = 1;
  expected_inbound_audio_codec.mime_type = "audio/opus";
  expected_inbound_audio_codec.clock_rate = 1337;
  expected_inbound_audio_codec.channels = 1;
  expected_inbound_audio_codec.sdp_fmtp_line = "minptime=10;useinbandfec=1";

  RTCCodecStats expected_outbound_audio_codec("RTCCodec_AudioMid_Outbound_2",
                                              report->timestamp_us());
  expected_outbound_audio_codec.payload_type = 2;
  expected_outbound_audio_codec.mime_type = "audio/isac";
  expected_outbound_audio_codec.clock_rate = 1338;
  expected_outbound_audio_codec.channels = 2;

  RTCCodecStats expected_inbound_video_codec("RTCCodec_VideoMid_Inbound_3",
                                             report->timestamp_us());
  expected_inbound_video_codec.payload_type = 3;
  expected_inbound_video_codec.mime_type = "video/H264";
  expected_inbound_video_codec.clock_rate = 1339;
  expected_inbound_video_codec.sdp_fmtp_line =
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f";

  RTCCodecStats expected_outbound_video_codec("RTCCodec_VideoMid_Outbound_4",
                                              report->timestamp_us());
  expected_outbound_video_codec.payload_type = 4;
  expected_outbound_video_codec.mime_type = "video/VP8";
  expected_outbound_video_codec.clock_rate = 1340;

  ASSERT_TRUE(report->Get(expected_inbound_audio_codec.id()));
  EXPECT_EQ(
      expected_inbound_audio_codec,
      report->Get(expected_inbound_audio_codec.id())->cast_to<RTCCodecStats>());

  ASSERT_TRUE(report->Get(expected_outbound_audio_codec.id()));
  EXPECT_EQ(expected_outbound_audio_codec,
            report->Get(expected_outbound_audio_codec.id())
                ->cast_to<RTCCodecStats>());

  ASSERT_TRUE(report->Get(expected_inbound_video_codec.id()));
  EXPECT_EQ(
      expected_inbound_video_codec,
      report->Get(expected_inbound_video_codec.id())->cast_to<RTCCodecStats>());

  ASSERT_TRUE(report->Get(expected_outbound_video_codec.id()));
  EXPECT_EQ(expected_outbound_video_codec,
            report->Get(expected_outbound_video_codec.id())
                ->cast_to<RTCCodecStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCCertificateStatsMultiple) {
  const char kAudioTransport[] = "audio";
  const char kVideoTransport[] = "video";

  pc_->AddVoiceChannel("audio", kAudioTransport);
  std::unique_ptr<CertificateInfo> audio_local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({"(local) audio"}));
  pc_->SetLocalCertificate(kAudioTransport, audio_local_certinfo->certificate);
  std::unique_ptr<CertificateInfo> audio_remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({"(remote) audio"}));
  pc_->SetRemoteCertChain(
      kAudioTransport,
      audio_remote_certinfo->certificate->GetSSLCertificateChain().Clone());

  pc_->AddVideoChannel("video", kVideoTransport);
  std::unique_ptr<CertificateInfo> video_local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({"(local) video"}));
  pc_->SetLocalCertificate(kVideoTransport, video_local_certinfo->certificate);
  std::unique_ptr<CertificateInfo> video_remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({"(remote) video"}));
  pc_->SetRemoteCertChain(
      kVideoTransport,
      video_remote_certinfo->certificate->GetSSLCertificateChain().Clone());

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  ExpectReportContainsCertificateInfo(report, *audio_local_certinfo);
  ExpectReportContainsCertificateInfo(report, *audio_remote_certinfo);
  ExpectReportContainsCertificateInfo(report, *video_local_certinfo);
  ExpectReportContainsCertificateInfo(report, *video_remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectRTCCertificateStatsChain) {
  const char kTransportName[] = "transport";

  pc_->AddVoiceChannel("audio", kTransportName);

  std::unique_ptr<CertificateInfo> local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          {"(local) this", "(local) is", "(local) a", "(local) chain"});
  pc_->SetLocalCertificate(kTransportName, local_certinfo->certificate);

  std::unique_ptr<CertificateInfo> remote_certinfo =
      CreateFakeCertificateAndInfoFromDers({"(remote) this", "(remote) is",
                                            "(remote) another",
                                            "(remote) chain"});
  pc_->SetRemoteCertChain(
      kTransportName,
      remote_certinfo->certificate->GetSSLCertificateChain().Clone());

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  ExpectReportContainsCertificateInfo(report, *local_certinfo);
  ExpectReportContainsCertificateInfo(report, *remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectTwoRTCDataChannelStatsWithPendingId) {
  pc_->AddSctpDataChannel(
      new MockDataChannel(/*id=*/-1, DataChannelInterface::kConnecting));
  pc_->AddSctpDataChannel(
      new MockDataChannel(/*id=*/-1, DataChannelInterface::kConnecting));

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
}

TEST_F(RTCStatsCollectorTest, CollectRTCDataChannelStats) {
  // Note: The test assumes data channel IDs are predictable.
  // This is not a safe assumption, but in order to make it work for
  // the test, we reset the ID allocator at test start.
  DataChannel::ResetInternalIdAllocatorForTesting(-1);
  pc_->AddSctpDataChannel(new MockDataChannel(0, "MockDataChannel0",
                                              DataChannelInterface::kConnecting,
                                              "udp", 1, 2, 3, 4));
  RTCDataChannelStats expected_data_channel0("RTCDataChannel_0", 0);
  expected_data_channel0.label = "MockDataChannel0";
  expected_data_channel0.protocol = "udp";
  expected_data_channel0.datachannelid = 0;
  expected_data_channel0.state = "connecting";
  expected_data_channel0.messages_sent = 1;
  expected_data_channel0.bytes_sent = 2;
  expected_data_channel0.messages_received = 3;
  expected_data_channel0.bytes_received = 4;

  pc_->AddSctpDataChannel(new MockDataChannel(
      1, "MockDataChannel1", DataChannelInterface::kOpen, "tcp", 5, 6, 7, 8));
  RTCDataChannelStats expected_data_channel1("RTCDataChannel_1", 0);
  expected_data_channel1.label = "MockDataChannel1";
  expected_data_channel1.protocol = "tcp";
  expected_data_channel1.datachannelid = 1;
  expected_data_channel1.state = "open";
  expected_data_channel1.messages_sent = 5;
  expected_data_channel1.bytes_sent = 6;
  expected_data_channel1.messages_received = 7;
  expected_data_channel1.bytes_received = 8;

  pc_->AddSctpDataChannel(new MockDataChannel(2, "MockDataChannel2",
                                              DataChannelInterface::kClosing,
                                              "udp", 9, 10, 11, 12));
  RTCDataChannelStats expected_data_channel2("RTCDataChannel_2", 0);
  expected_data_channel2.label = "MockDataChannel2";
  expected_data_channel2.protocol = "udp";
  expected_data_channel2.datachannelid = 2;
  expected_data_channel2.state = "closing";
  expected_data_channel2.messages_sent = 9;
  expected_data_channel2.bytes_sent = 10;
  expected_data_channel2.messages_received = 11;
  expected_data_channel2.bytes_received = 12;

  pc_->AddSctpDataChannel(new MockDataChannel(3, "MockDataChannel3",
                                              DataChannelInterface::kClosed,
                                              "tcp", 13, 14, 15, 16));
  RTCDataChannelStats expected_data_channel3("RTCDataChannel_3", 0);
  expected_data_channel3.label = "MockDataChannel3";
  expected_data_channel3.protocol = "tcp";
  expected_data_channel3.datachannelid = 3;
  expected_data_channel3.state = "closed";
  expected_data_channel3.messages_sent = 13;
  expected_data_channel3.bytes_sent = 14;
  expected_data_channel3.messages_received = 15;
  expected_data_channel3.bytes_received = 16;

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  ASSERT_TRUE(report->Get(expected_data_channel0.id()));
  EXPECT_EQ(
      expected_data_channel0,
      report->Get(expected_data_channel0.id())->cast_to<RTCDataChannelStats>());
  ASSERT_TRUE(report->Get(expected_data_channel1.id()));
  EXPECT_EQ(
      expected_data_channel1,
      report->Get(expected_data_channel1.id())->cast_to<RTCDataChannelStats>());
  ASSERT_TRUE(report->Get(expected_data_channel2.id()));
  EXPECT_EQ(
      expected_data_channel2,
      report->Get(expected_data_channel2.id())->cast_to<RTCDataChannelStats>());
  ASSERT_TRUE(report->Get(expected_data_channel3.id()));
  EXPECT_EQ(
      expected_data_channel3,
      report->Get(expected_data_channel3.id())->cast_to<RTCDataChannelStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCIceCandidateStats) {
  // Candidates in the first transport stats.
  std::unique_ptr<cricket::Candidate> a_local_host =
      CreateFakeCandidate("1.2.3.4", 5, "a_local_host's protocol",
                          rtc::ADAPTER_TYPE_VPN, cricket::LOCAL_PORT_TYPE, 0);
  RTCLocalIceCandidateStats expected_a_local_host(
      "RTCIceCandidate_" + a_local_host->id(), 0);
  expected_a_local_host.transport_id = "RTCTransport_a_0";
  expected_a_local_host.network_type = "vpn";
  expected_a_local_host.ip = "1.2.3.4";
  expected_a_local_host.port = 5;
  expected_a_local_host.protocol = "a_local_host's protocol";
  expected_a_local_host.candidate_type = "host";
  expected_a_local_host.priority = 0;
  EXPECT_FALSE(*expected_a_local_host.is_remote);

  std::unique_ptr<cricket::Candidate> a_remote_srflx = CreateFakeCandidate(
      "6.7.8.9", 10, "remote_srflx's protocol", rtc::ADAPTER_TYPE_UNKNOWN,
      cricket::STUN_PORT_TYPE, 1);
  RTCRemoteIceCandidateStats expected_a_remote_srflx(
      "RTCIceCandidate_" + a_remote_srflx->id(), 0);
  expected_a_remote_srflx.transport_id = "RTCTransport_a_0";
  expected_a_remote_srflx.ip = "6.7.8.9";
  expected_a_remote_srflx.port = 10;
  expected_a_remote_srflx.protocol = "remote_srflx's protocol";
  expected_a_remote_srflx.candidate_type = "srflx";
  expected_a_remote_srflx.priority = 1;
  expected_a_remote_srflx.deleted = false;
  EXPECT_TRUE(*expected_a_remote_srflx.is_remote);

  std::unique_ptr<cricket::Candidate> a_local_prflx = CreateFakeCandidate(
      "11.12.13.14", 15, "a_local_prflx's protocol", rtc::ADAPTER_TYPE_CELLULAR,
      cricket::PRFLX_PORT_TYPE, 2);
  RTCLocalIceCandidateStats expected_a_local_prflx(
      "RTCIceCandidate_" + a_local_prflx->id(), 0);
  expected_a_local_prflx.transport_id = "RTCTransport_a_0";
  expected_a_local_prflx.network_type = "cellular";
  expected_a_local_prflx.ip = "11.12.13.14";
  expected_a_local_prflx.port = 15;
  expected_a_local_prflx.protocol = "a_local_prflx's protocol";
  expected_a_local_prflx.candidate_type = "prflx";
  expected_a_local_prflx.priority = 2;
  expected_a_local_prflx.deleted = false;
  EXPECT_FALSE(*expected_a_local_prflx.is_remote);

  std::unique_ptr<cricket::Candidate> a_remote_relay = CreateFakeCandidate(
      "16.17.18.19", 20, "a_remote_relay's protocol", rtc::ADAPTER_TYPE_UNKNOWN,
      cricket::RELAY_PORT_TYPE, 3);
  RTCRemoteIceCandidateStats expected_a_remote_relay(
      "RTCIceCandidate_" + a_remote_relay->id(), 0);
  expected_a_remote_relay.transport_id = "RTCTransport_a_0";
  expected_a_remote_relay.ip = "16.17.18.19";
  expected_a_remote_relay.port = 20;
  expected_a_remote_relay.protocol = "a_remote_relay's protocol";
  expected_a_remote_relay.candidate_type = "relay";
  expected_a_remote_relay.priority = 3;
  expected_a_remote_relay.deleted = false;
  EXPECT_TRUE(*expected_a_remote_relay.is_remote);

  std::unique_ptr<cricket::Candidate> a_local_relay = CreateFakeCandidate(
      "16.17.18.19", 21, "a_local_relay's protocol", rtc::ADAPTER_TYPE_UNKNOWN,
      cricket::RELAY_PORT_TYPE, 1);
  a_local_relay->set_relay_protocol("tcp");

  RTCRemoteIceCandidateStats expected_a_local_relay(
      "RTCIceCandidate_" + a_local_relay->id(), 0);
  expected_a_local_relay.transport_id = "RTCTransport_a_0";
  expected_a_local_relay.ip = "16.17.18.19";
  expected_a_local_relay.port = 21;
  expected_a_local_relay.protocol = "a_local_relay's protocol";
  expected_a_local_relay.relay_protocol = "tcp";
  expected_a_local_relay.candidate_type = "relay";
  expected_a_local_relay.priority = 1;
  expected_a_local_relay.deleted = false;
  EXPECT_TRUE(*expected_a_local_relay.is_remote);

  // Candidates in the second transport stats.
  std::unique_ptr<cricket::Candidate> b_local =
      CreateFakeCandidate("42.42.42.42", 42, "b_local's protocol",
                          rtc::ADAPTER_TYPE_WIFI, cricket::LOCAL_PORT_TYPE, 42);
  RTCLocalIceCandidateStats expected_b_local("RTCIceCandidate_" + b_local->id(),
                                             0);
  expected_b_local.transport_id = "RTCTransport_b_0";
  expected_b_local.network_type = "wifi";
  expected_b_local.ip = "42.42.42.42";
  expected_b_local.port = 42;
  expected_b_local.protocol = "b_local's protocol";
  expected_b_local.candidate_type = "host";
  expected_b_local.priority = 42;
  expected_b_local.deleted = false;
  EXPECT_FALSE(*expected_b_local.is_remote);

  std::unique_ptr<cricket::Candidate> b_remote = CreateFakeCandidate(
      "42.42.42.42", 42, "b_remote's protocol", rtc::ADAPTER_TYPE_UNKNOWN,
      cricket::LOCAL_PORT_TYPE, 42);
  RTCRemoteIceCandidateStats expected_b_remote(
      "RTCIceCandidate_" + b_remote->id(), 0);
  expected_b_remote.transport_id = "RTCTransport_b_0";
  expected_b_remote.ip = "42.42.42.42";
  expected_b_remote.port = 42;
  expected_b_remote.protocol = "b_remote's protocol";
  expected_b_remote.candidate_type = "host";
  expected_b_remote.priority = 42;
  expected_b_remote.deleted = false;
  EXPECT_TRUE(*expected_b_remote.is_remote);

  // Add candidate pairs to connection.
  cricket::TransportChannelStats a_transport_channel_stats;
  a_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  a_transport_channel_stats.ice_transport_stats.connection_infos[0]
      .local_candidate = *a_local_host.get();
  a_transport_channel_stats.ice_transport_stats.connection_infos[0]
      .remote_candidate = *a_remote_srflx.get();
  a_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  a_transport_channel_stats.ice_transport_stats.connection_infos[1]
      .local_candidate = *a_local_prflx.get();
  a_transport_channel_stats.ice_transport_stats.connection_infos[1]
      .remote_candidate = *a_remote_relay.get();
  a_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  a_transport_channel_stats.ice_transport_stats.connection_infos[2]
      .local_candidate = *a_local_relay.get();
  a_transport_channel_stats.ice_transport_stats.connection_infos[2]
      .remote_candidate = *a_remote_relay.get();

  pc_->AddVoiceChannel("audio", "a");
  pc_->SetTransportStats("a", a_transport_channel_stats);

  cricket::TransportChannelStats b_transport_channel_stats;
  b_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  b_transport_channel_stats.ice_transport_stats.connection_infos[0]
      .local_candidate = *b_local.get();
  b_transport_channel_stats.ice_transport_stats.connection_infos[0]
      .remote_candidate = *b_remote.get();

  pc_->AddVideoChannel("video", "b");
  pc_->SetTransportStats("b", b_transport_channel_stats);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  ASSERT_TRUE(report->Get(expected_a_local_host.id()));
  EXPECT_EQ(expected_a_local_host, report->Get(expected_a_local_host.id())
                                       ->cast_to<RTCLocalIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_a_remote_srflx.id()));
  EXPECT_EQ(expected_a_remote_srflx,
            report->Get(expected_a_remote_srflx.id())
                ->cast_to<RTCRemoteIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_a_local_prflx.id()));
  EXPECT_EQ(expected_a_local_prflx, report->Get(expected_a_local_prflx.id())
                                        ->cast_to<RTCLocalIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_a_remote_relay.id()));
  EXPECT_EQ(expected_a_remote_relay,
            report->Get(expected_a_remote_relay.id())
                ->cast_to<RTCRemoteIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_b_local.id()));
  EXPECT_EQ(
      expected_b_local,
      report->Get(expected_b_local.id())->cast_to<RTCLocalIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_b_remote.id()));
  EXPECT_EQ(expected_b_remote, report->Get(expected_b_remote.id())
                                   ->cast_to<RTCRemoteIceCandidateStats>());
  EXPECT_TRUE(report->Get("RTCTransport_a_0"));
  EXPECT_TRUE(report->Get("RTCTransport_b_0"));
}

TEST_F(RTCStatsCollectorTest, CollectRTCIceCandidatePairStats) {
  const char kTransportName[] = "transport";

  std::unique_ptr<cricket::Candidate> local_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol", rtc::ADAPTER_TYPE_WIFI,
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> remote_candidate = CreateFakeCandidate(
      "42.42.42.42", 42, "protocol", rtc::ADAPTER_TYPE_UNKNOWN,
      cricket::LOCAL_PORT_TYPE, 42);

  cricket::ConnectionInfo connection_info;
  connection_info.best_connection = false;
  connection_info.local_candidate = *local_candidate.get();
  connection_info.remote_candidate = *remote_candidate.get();
  connection_info.writable = true;
  connection_info.sent_total_bytes = 42;
  connection_info.recv_total_bytes = 1234;
  connection_info.total_round_trip_time_ms = 0;
  connection_info.current_round_trip_time_ms = absl::nullopt;
  connection_info.recv_ping_requests = 2020;
  connection_info.sent_ping_requests_total = 2020;
  connection_info.sent_ping_requests_before_first_response = 2000;
  connection_info.recv_ping_responses = 4321;
  connection_info.sent_ping_responses = 1000;
  connection_info.state = cricket::IceCandidatePairState::IN_PROGRESS;
  connection_info.priority = 5555;
  connection_info.nominated = false;

  cricket::TransportChannelStats transport_channel_stats;
  transport_channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      connection_info);

  pc_->AddVideoChannel("video", kTransportName);
  pc_->SetTransportStats(kTransportName, transport_channel_stats);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCIceCandidatePairStats expected_pair("RTCIceCandidatePair_" +
                                             local_candidate->id() + "_" +
                                             remote_candidate->id(),
                                         report->timestamp_us());
  expected_pair.transport_id =
      "RTCTransport_transport_" +
      rtc::ToString(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_pair.local_candidate_id = "RTCIceCandidate_" + local_candidate->id();
  expected_pair.remote_candidate_id =
      "RTCIceCandidate_" + remote_candidate->id();
  expected_pair.state = RTCStatsIceCandidatePairState::kInProgress;
  expected_pair.priority = 5555;
  expected_pair.nominated = false;
  expected_pair.writable = true;
  expected_pair.bytes_sent = 42;
  expected_pair.bytes_received = 1234;
  expected_pair.total_round_trip_time = 0.0;
  expected_pair.requests_received = 2020;
  expected_pair.requests_sent = 2000;
  expected_pair.responses_received = 4321;
  expected_pair.responses_sent = 1000;
  expected_pair.consent_requests_sent = (2020 - 2000);
  // |expected_pair.current_round_trip_time| should be undefined because the
  // current RTT is not set.
  // |expected_pair.available_[outgoing/incoming]_bitrate| should be undefined
  // because is is not the current pair.

  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Set nominated and "GetStats" again.
  transport_channel_stats.ice_transport_stats.connection_infos[0].nominated =
      true;
  pc_->SetTransportStats(kTransportName, transport_channel_stats);
  report = stats_->GetFreshStatsReport();
  expected_pair.nominated = true;
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Set round trip times and "GetStats" again.
  transport_channel_stats.ice_transport_stats.connection_infos[0]
      .total_round_trip_time_ms = 7331;
  transport_channel_stats.ice_transport_stats.connection_infos[0]
      .current_round_trip_time_ms = 1337;
  pc_->SetTransportStats(kTransportName, transport_channel_stats);
  report = stats_->GetFreshStatsReport();
  expected_pair.total_round_trip_time = 7.331;
  expected_pair.current_round_trip_time = 1.337;
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Make pair the current pair, clear bandwidth and "GetStats" again.
  transport_channel_stats.ice_transport_stats.connection_infos[0]
      .best_connection = true;
  pc_->SetTransportStats(kTransportName, transport_channel_stats);
  report = stats_->GetFreshStatsReport();
  // |expected_pair.available_[outgoing/incoming]_bitrate| should still be
  // undefined because bandwidth is not set.
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Set bandwidth and "GetStats" again.
  webrtc::Call::Stats call_stats;
  const int kSendBandwidth = 888;
  call_stats.send_bandwidth_bps = kSendBandwidth;
  const int kRecvBandwidth = 999;
  call_stats.recv_bandwidth_bps = kRecvBandwidth;
  pc_->SetCallStats(call_stats);
  report = stats_->GetFreshStatsReport();
  expected_pair.available_outgoing_bitrate = kSendBandwidth;
  expected_pair.available_incoming_bitrate = kRecvBandwidth;
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  RTCLocalIceCandidateStats expected_local_candidate(
      *expected_pair.local_candidate_id, report->timestamp_us());
  expected_local_candidate.transport_id = *expected_pair.transport_id;
  expected_local_candidate.network_type = "wifi";
  expected_local_candidate.ip = "42.42.42.42";
  expected_local_candidate.port = 42;
  expected_local_candidate.protocol = "protocol";
  expected_local_candidate.candidate_type = "host";
  expected_local_candidate.priority = 42;
  expected_local_candidate.deleted = false;
  EXPECT_FALSE(*expected_local_candidate.is_remote);
  ASSERT_TRUE(report->Get(expected_local_candidate.id()));
  EXPECT_EQ(expected_local_candidate,
            report->Get(expected_local_candidate.id())
                ->cast_to<RTCLocalIceCandidateStats>());

  RTCRemoteIceCandidateStats expected_remote_candidate(
      *expected_pair.remote_candidate_id, report->timestamp_us());
  expected_remote_candidate.transport_id = *expected_pair.transport_id;
  expected_remote_candidate.ip = "42.42.42.42";
  expected_remote_candidate.port = 42;
  expected_remote_candidate.protocol = "protocol";
  expected_remote_candidate.candidate_type = "host";
  expected_remote_candidate.priority = 42;
  expected_remote_candidate.deleted = false;
  EXPECT_TRUE(*expected_remote_candidate.is_remote);
  ASSERT_TRUE(report->Get(expected_remote_candidate.id()));
  EXPECT_EQ(expected_remote_candidate,
            report->Get(expected_remote_candidate.id())
                ->cast_to<RTCRemoteIceCandidateStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCPeerConnectionStats) {
  {
    rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 0;
    expected.data_channels_closed = 0;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(
        expected,
        report->Get("RTCPeerConnection")->cast_to<RTCPeerConnectionStats>());
  }

  rtc::scoped_refptr<DataChannel> dummy_channel_a = DataChannel::Create(
      nullptr, cricket::DCT_NONE, "DummyChannelA", InternalDataChannelInit());
  pc_->SignalDataChannelCreated()(dummy_channel_a.get());
  rtc::scoped_refptr<DataChannel> dummy_channel_b = DataChannel::Create(
      nullptr, cricket::DCT_NONE, "DummyChannelB", InternalDataChannelInit());
  pc_->SignalDataChannelCreated()(dummy_channel_b.get());

  dummy_channel_a->SignalOpened(dummy_channel_a.get());
  // Closing a channel that is not opened should not affect the counts.
  dummy_channel_b->SignalClosed(dummy_channel_b.get());

  {
    rtc::scoped_refptr<const RTCStatsReport> report =
        stats_->GetFreshStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 1;
    expected.data_channels_closed = 0;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(
        expected,
        report->Get("RTCPeerConnection")->cast_to<RTCPeerConnectionStats>());
  }

  dummy_channel_b->SignalOpened(dummy_channel_b.get());
  dummy_channel_b->SignalClosed(dummy_channel_b.get());

  {
    rtc::scoped_refptr<const RTCStatsReport> report =
        stats_->GetFreshStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 2;
    expected.data_channels_closed = 1;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(
        expected,
        report->Get("RTCPeerConnection")->cast_to<RTCPeerConnectionStats>());
  }

  // Re-opening a data channel (or opening a new data channel that is re-using
  // the same address in memory) should increase the opened count.
  dummy_channel_b->SignalOpened(dummy_channel_b.get());

  {
    rtc::scoped_refptr<const RTCStatsReport> report =
        stats_->GetFreshStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 3;
    expected.data_channels_closed = 1;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(
        expected,
        report->Get("RTCPeerConnection")->cast_to<RTCPeerConnectionStats>());
  }

  dummy_channel_a->SignalClosed(dummy_channel_a.get());
  dummy_channel_b->SignalClosed(dummy_channel_b.get());

  {
    rtc::scoped_refptr<const RTCStatsReport> report =
        stats_->GetFreshStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 3;
    expected.data_channels_closed = 3;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(
        expected,
        report->Get("RTCPeerConnection")->cast_to<RTCPeerConnectionStats>());
  }
}

TEST_F(RTCStatsCollectorTest,
       CollectLocalRTCMediaStreamStatsAndRTCMediaStreamTrackStats_Audio) {
  rtc::scoped_refptr<MediaStream> local_stream =
      MediaStream::Create("LocalStreamId");
  pc_->mutable_local_streams()->AddStream(local_stream);

  // Local audio track
  rtc::scoped_refptr<MediaStreamTrackInterface> local_audio_track =
      CreateFakeTrack(cricket::MEDIA_TYPE_AUDIO, "LocalAudioTrackID",
                      MediaStreamTrackInterface::kEnded);
  local_stream->AddTrack(
      static_cast<AudioTrackInterface*>(local_audio_track.get()));

  cricket::VoiceSenderInfo voice_sender_info_ssrc1;
  voice_sender_info_ssrc1.local_stats.push_back(cricket::SsrcSenderInfo());
  voice_sender_info_ssrc1.local_stats[0].ssrc = 1;
  voice_sender_info_ssrc1.apm_statistics.echo_return_loss = 42.0;
  voice_sender_info_ssrc1.apm_statistics.echo_return_loss_enhancement = 52.0;

  stats_->CreateMockRtpSendersReceiversAndChannels(
      {std::make_pair(local_audio_track.get(), voice_sender_info_ssrc1)}, {},
      {}, {}, {local_stream->id()}, {});

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCMediaStreamStats expected_local_stream(
      IdForType<RTCMediaStreamStats>(report), report->timestamp_us());
  expected_local_stream.stream_identifier = local_stream->id();
  expected_local_stream.track_ids = {
      IdForType<RTCMediaStreamTrackStats>(report)};
  ASSERT_TRUE(report->Get(expected_local_stream.id()))
      << "Did not find " << expected_local_stream.id() << " in "
      << report->ToJson();
  EXPECT_EQ(
      expected_local_stream,
      report->Get(expected_local_stream.id())->cast_to<RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_local_audio_track_ssrc1(
      IdForType<RTCMediaStreamTrackStats>(report), report->timestamp_us(),
      RTCMediaStreamTrackKind::kAudio);
  expected_local_audio_track_ssrc1.track_identifier = local_audio_track->id();
  expected_local_audio_track_ssrc1.media_source_id =
      "RTCAudioSource_11";  // Attachment ID = SSRC + 10
  expected_local_audio_track_ssrc1.remote_source = false;
  expected_local_audio_track_ssrc1.ended = true;
  expected_local_audio_track_ssrc1.detached = false;
  expected_local_audio_track_ssrc1.echo_return_loss = 42.0;
  expected_local_audio_track_ssrc1.echo_return_loss_enhancement = 52.0;
  ASSERT_TRUE(report->Get(expected_local_audio_track_ssrc1.id()))
      << "Did not find " << expected_local_audio_track_ssrc1.id() << " in "
      << report->ToJson();
  EXPECT_EQ(expected_local_audio_track_ssrc1,
            report->Get(expected_local_audio_track_ssrc1.id())
                ->cast_to<RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest,
       CollectRemoteRTCMediaStreamStatsAndRTCMediaStreamTrackStats_Audio) {
  rtc::scoped_refptr<MediaStream> remote_stream =
      MediaStream::Create("RemoteStreamId");
  pc_->mutable_remote_streams()->AddStream(remote_stream);

  // Remote audio track
  rtc::scoped_refptr<MediaStreamTrackInterface> remote_audio_track =
      CreateFakeTrack(cricket::MEDIA_TYPE_AUDIO, "RemoteAudioTrackID",
                      MediaStreamTrackInterface::kLive);
  remote_stream->AddTrack(
      static_cast<AudioTrackInterface*>(remote_audio_track.get()));

  cricket::VoiceReceiverInfo voice_receiver_info;
  voice_receiver_info.local_stats.push_back(cricket::SsrcReceiverInfo());
  voice_receiver_info.local_stats[0].ssrc = 3;
  voice_receiver_info.audio_level = 16383;
  voice_receiver_info.total_output_energy = 0.125;
  voice_receiver_info.total_samples_received = 4567;
  voice_receiver_info.total_output_duration = 0.25;
  voice_receiver_info.concealed_samples = 123;
  voice_receiver_info.concealment_events = 12;
  voice_receiver_info.inserted_samples_for_deceleration = 987;
  voice_receiver_info.removed_samples_for_acceleration = 876;
  voice_receiver_info.silent_concealed_samples = 765;
  voice_receiver_info.jitter_buffer_delay_seconds = 3456;
  voice_receiver_info.jitter_buffer_emitted_count = 13;
  voice_receiver_info.jitter_buffer_target_delay_seconds = 7.894;
  voice_receiver_info.jitter_buffer_flushes = 7;
  voice_receiver_info.delayed_packet_outage_samples = 15;
  voice_receiver_info.relative_packet_arrival_delay_seconds = 16;
  voice_receiver_info.interruption_count = 7788;
  voice_receiver_info.total_interruption_duration_ms = 778899;

  stats_->CreateMockRtpSendersReceiversAndChannels(
      {}, {std::make_pair(remote_audio_track.get(), voice_receiver_info)}, {},
      {}, {}, {remote_stream});

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCMediaStreamStats expected_remote_stream(
      IdForType<RTCMediaStreamStats>(report), report->timestamp_us());
  expected_remote_stream.stream_identifier = remote_stream->id();
  expected_remote_stream.track_ids =
      std::vector<std::string>({IdForType<RTCMediaStreamTrackStats>(report)});
  ASSERT_TRUE(report->Get(expected_remote_stream.id()))
      << "Did not find " << expected_remote_stream.id() << " in "
      << report->ToJson();
  EXPECT_EQ(
      expected_remote_stream,
      report->Get(expected_remote_stream.id())->cast_to<RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_remote_audio_track(
      IdForType<RTCMediaStreamTrackStats>(report), report->timestamp_us(),
      RTCMediaStreamTrackKind::kAudio);
  expected_remote_audio_track.track_identifier = remote_audio_track->id();
  // |expected_remote_audio_track.media_source_id| should be undefined
  // because the track is remote.
  expected_remote_audio_track.remote_source = true;
  expected_remote_audio_track.ended = false;
  expected_remote_audio_track.detached = false;
  expected_remote_audio_track.audio_level = 16383.0 / 32767.0;
  expected_remote_audio_track.total_audio_energy = 0.125;
  expected_remote_audio_track.total_samples_received = 4567;
  expected_remote_audio_track.total_samples_duration = 0.25;
  expected_remote_audio_track.concealed_samples = 123;
  expected_remote_audio_track.concealment_events = 12;
  expected_remote_audio_track.inserted_samples_for_deceleration = 987;
  expected_remote_audio_track.removed_samples_for_acceleration = 876;
  expected_remote_audio_track.silent_concealed_samples = 765;
  expected_remote_audio_track.jitter_buffer_delay = 3456;
  expected_remote_audio_track.jitter_buffer_emitted_count = 13;
  expected_remote_audio_track.jitter_buffer_target_delay = 7.894;
  expected_remote_audio_track.jitter_buffer_flushes = 7;
  expected_remote_audio_track.delayed_packet_outage_samples = 15;
  expected_remote_audio_track.relative_packet_arrival_delay = 16;
  expected_remote_audio_track.interruption_count = 7788;
  expected_remote_audio_track.total_interruption_duration = 778.899;
  ASSERT_TRUE(report->Get(expected_remote_audio_track.id()));
  EXPECT_EQ(expected_remote_audio_track,
            report->Get(expected_remote_audio_track.id())
                ->cast_to<RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest,
       CollectLocalRTCMediaStreamStatsAndRTCMediaStreamTrackStats_Video) {
  rtc::scoped_refptr<MediaStream> local_stream =
      MediaStream::Create("LocalStreamId");
  pc_->mutable_local_streams()->AddStream(local_stream);

  // Local video track
  rtc::scoped_refptr<MediaStreamTrackInterface> local_video_track =
      CreateFakeTrack(cricket::MEDIA_TYPE_VIDEO, "LocalVideoTrackID",
                      MediaStreamTrackInterface::kLive);
  local_stream->AddTrack(
      static_cast<VideoTrackInterface*>(local_video_track.get()));

  cricket::VideoSenderInfo video_sender_info_ssrc1;
  video_sender_info_ssrc1.local_stats.push_back(cricket::SsrcSenderInfo());
  video_sender_info_ssrc1.local_stats[0].ssrc = 1;
  video_sender_info_ssrc1.send_frame_width = 1234;
  video_sender_info_ssrc1.send_frame_height = 4321;
  video_sender_info_ssrc1.frames_encoded = 11;
  video_sender_info_ssrc1.huge_frames_sent = 1;

  stats_->CreateMockRtpSendersReceiversAndChannels(
      {}, {},
      {std::make_pair(local_video_track.get(), video_sender_info_ssrc1)}, {},
      {local_stream->id()}, {});

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  auto stats_of_my_type = report->GetStatsOfType<RTCMediaStreamStats>();
  ASSERT_EQ(1U, stats_of_my_type.size()) << "No stream in " << report->ToJson();
  auto stats_of_track_type = report->GetStatsOfType<RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, stats_of_track_type.size())
      << "Wrong number of tracks in " << report->ToJson();

  RTCMediaStreamStats expected_local_stream(stats_of_my_type[0]->id(),
                                            report->timestamp_us());
  expected_local_stream.stream_identifier = local_stream->id();
  expected_local_stream.track_ids =
      std::vector<std::string>({stats_of_track_type[0]->id()});
  ASSERT_TRUE(report->Get(expected_local_stream.id()));
  EXPECT_EQ(
      expected_local_stream,
      report->Get(expected_local_stream.id())->cast_to<RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_local_video_track_ssrc1(
      stats_of_track_type[0]->id(), report->timestamp_us(),
      RTCMediaStreamTrackKind::kVideo);
  expected_local_video_track_ssrc1.track_identifier = local_video_track->id();
  expected_local_video_track_ssrc1.media_source_id =
      "RTCVideoSource_11";  // Attachment ID = SSRC + 10
  expected_local_video_track_ssrc1.remote_source = false;
  expected_local_video_track_ssrc1.ended = false;
  expected_local_video_track_ssrc1.detached = false;
  expected_local_video_track_ssrc1.frame_width = 1234;
  expected_local_video_track_ssrc1.frame_height = 4321;
  expected_local_video_track_ssrc1.frames_sent = 11;
  expected_local_video_track_ssrc1.huge_frames_sent = 1;
  ASSERT_TRUE(report->Get(expected_local_video_track_ssrc1.id()));
  EXPECT_EQ(expected_local_video_track_ssrc1,
            report->Get(expected_local_video_track_ssrc1.id())
                ->cast_to<RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest,
       CollectRemoteRTCMediaStreamStatsAndRTCMediaStreamTrackStats_Video) {
  rtc::scoped_refptr<MediaStream> remote_stream =
      MediaStream::Create("RemoteStreamId");
  pc_->mutable_remote_streams()->AddStream(remote_stream);

  // Remote video track with values
  rtc::scoped_refptr<MediaStreamTrackInterface> remote_video_track_ssrc3 =
      CreateFakeTrack(cricket::MEDIA_TYPE_VIDEO, "RemoteVideoTrackID3",
                      MediaStreamTrackInterface::kEnded);
  remote_stream->AddTrack(
      static_cast<VideoTrackInterface*>(remote_video_track_ssrc3.get()));

  cricket::VideoReceiverInfo video_receiver_info_ssrc3;
  video_receiver_info_ssrc3.local_stats.push_back(cricket::SsrcReceiverInfo());
  video_receiver_info_ssrc3.local_stats[0].ssrc = 3;
  video_receiver_info_ssrc3.frame_width = 6789;
  video_receiver_info_ssrc3.frame_height = 9876;
  video_receiver_info_ssrc3.jitter_buffer_delay_seconds = 2.5;
  video_receiver_info_ssrc3.jitter_buffer_emitted_count = 25;
  video_receiver_info_ssrc3.frames_received = 1000;
  video_receiver_info_ssrc3.frames_decoded = 995;
  video_receiver_info_ssrc3.frames_dropped = 10;
  video_receiver_info_ssrc3.frames_rendered = 990;
  video_receiver_info_ssrc3.freeze_count = 3;
  video_receiver_info_ssrc3.pause_count = 2;
  video_receiver_info_ssrc3.total_freezes_duration_ms = 1000;
  video_receiver_info_ssrc3.total_pauses_duration_ms = 10000;
  video_receiver_info_ssrc3.total_frames_duration_ms = 15000;
  video_receiver_info_ssrc3.sum_squared_frame_durations = 1.5;

  stats_->CreateMockRtpSendersReceiversAndChannels(
      {}, {}, {},
      {std::make_pair(remote_video_track_ssrc3.get(),
                      video_receiver_info_ssrc3)},
      {}, {remote_stream});

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  auto stats_of_my_type = report->GetStatsOfType<RTCMediaStreamStats>();
  ASSERT_EQ(1U, stats_of_my_type.size()) << "No stream in " << report->ToJson();
  auto stats_of_track_type = report->GetStatsOfType<RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, stats_of_track_type.size())
      << "Wrong number of tracks in " << report->ToJson();
  ASSERT_TRUE(*(stats_of_track_type[0]->remote_source));

  RTCMediaStreamStats expected_remote_stream(stats_of_my_type[0]->id(),
                                             report->timestamp_us());
  expected_remote_stream.stream_identifier = remote_stream->id();
  expected_remote_stream.track_ids =
      std::vector<std::string>({stats_of_track_type[0]->id()});
  ASSERT_TRUE(report->Get(expected_remote_stream.id()));
  EXPECT_EQ(
      expected_remote_stream,
      report->Get(expected_remote_stream.id())->cast_to<RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_remote_video_track_ssrc3(
      stats_of_track_type[0]->id(), report->timestamp_us(),
      RTCMediaStreamTrackKind::kVideo);
  expected_remote_video_track_ssrc3.track_identifier =
      remote_video_track_ssrc3->id();
  // |expected_remote_video_track_ssrc3.media_source_id| should be undefined
  // because the track is remote.
  expected_remote_video_track_ssrc3.remote_source = true;
  expected_remote_video_track_ssrc3.ended = true;
  expected_remote_video_track_ssrc3.detached = false;
  expected_remote_video_track_ssrc3.frame_width = 6789;
  expected_remote_video_track_ssrc3.frame_height = 9876;
  expected_remote_video_track_ssrc3.jitter_buffer_delay = 2.5;
  expected_remote_video_track_ssrc3.jitter_buffer_emitted_count = 25;
  expected_remote_video_track_ssrc3.frames_received = 1000;
  expected_remote_video_track_ssrc3.frames_decoded = 995;
  expected_remote_video_track_ssrc3.frames_dropped = 10;
  expected_remote_video_track_ssrc3.freeze_count = 3;
  expected_remote_video_track_ssrc3.pause_count = 2;
  expected_remote_video_track_ssrc3.total_freezes_duration = 1;
  expected_remote_video_track_ssrc3.total_pauses_duration = 10;
  expected_remote_video_track_ssrc3.total_frames_duration = 15;
  expected_remote_video_track_ssrc3.sum_squared_frame_durations = 1.5;

  ASSERT_TRUE(report->Get(expected_remote_video_track_ssrc3.id()));
  EXPECT_EQ(expected_remote_video_track_ssrc3,
            report->Get(expected_remote_video_track_ssrc3.id())
                ->cast_to<RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCInboundRTPStreamStats_Audio) {
  cricket::VoiceMediaInfo voice_media_info;

  voice_media_info.receivers.push_back(cricket::VoiceReceiverInfo());
  voice_media_info.receivers[0].local_stats.push_back(
      cricket::SsrcReceiverInfo());
  voice_media_info.receivers[0].local_stats[0].ssrc = 1;
  voice_media_info.receivers[0].packets_lost = -1;  // Signed per RFC3550
  voice_media_info.receivers[0].packets_rcvd = 2;
  voice_media_info.receivers[0].fec_packets_discarded = 5566;
  voice_media_info.receivers[0].fec_packets_received = 6677;
  voice_media_info.receivers[0].payload_bytes_rcvd = 3;
  voice_media_info.receivers[0].header_and_padding_bytes_rcvd = 4;
  voice_media_info.receivers[0].codec_payload_type = 42;
  voice_media_info.receivers[0].jitter_ms = 4500;
  voice_media_info.receivers[0].last_packet_received_timestamp_ms =
      absl::nullopt;

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = 0;
  voice_media_info.receive_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  auto* voice_media_channel = pc_->AddVoiceChannel("AudioMid", "TransportName");
  voice_media_channel->SetStats(voice_media_info);
  stats_->SetupRemoteTrackAndReceiver(
      cricket::MEDIA_TYPE_AUDIO, "RemoteAudioTrackID", "RemoteStreamId", 1);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  auto stats_of_track_type = report->GetStatsOfType<RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, stats_of_track_type.size());

  RTCInboundRTPStreamStats expected_audio("RTCInboundRTPAudioStream_1",
                                          report->timestamp_us());
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.kind = "audio";
  expected_audio.track_id = stats_of_track_type[0]->id();
  expected_audio.transport_id = "RTCTransport_TransportName_1";
  expected_audio.codec_id = "RTCCodec_AudioMid_Inbound_42";
  expected_audio.packets_received = 2;
  expected_audio.fec_packets_discarded = 5566;
  expected_audio.fec_packets_received = 6677;
  expected_audio.bytes_received = 3;
  expected_audio.header_bytes_received = 4;
  expected_audio.packets_lost = -1;
  // |expected_audio.last_packet_received_timestamp| should be undefined.
  expected_audio.jitter = 4.5;
  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(
      report->Get(expected_audio.id())->cast_to<RTCInboundRTPStreamStats>(),
      expected_audio);

  // Set previously undefined values and "GetStats" again.
  voice_media_info.receivers[0].last_packet_received_timestamp_ms = 3000;
  expected_audio.last_packet_received_timestamp = 3.0;
  voice_media_info.receivers[0].estimated_playout_ntp_timestamp_ms = 4567;
  expected_audio.estimated_playout_timestamp = 4567;
  voice_media_channel->SetStats(voice_media_info);

  report = stats_->GetFreshStatsReport();

  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(
      report->Get(expected_audio.id())->cast_to<RTCInboundRTPStreamStats>(),
      expected_audio);
  EXPECT_TRUE(report->Get(*expected_audio.track_id));
  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
  EXPECT_TRUE(report->Get(*expected_audio.codec_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCInboundRTPStreamStats_Video) {
  cricket::VideoMediaInfo video_media_info;

  video_media_info.receivers.push_back(cricket::VideoReceiverInfo());
  video_media_info.receivers[0].local_stats.push_back(
      cricket::SsrcReceiverInfo());
  video_media_info.receivers[0].local_stats[0].ssrc = 1;
  video_media_info.receivers[0].packets_rcvd = 2;
  video_media_info.receivers[0].packets_lost = 42;
  video_media_info.receivers[0].payload_bytes_rcvd = 3;
  video_media_info.receivers[0].header_and_padding_bytes_rcvd = 12;
  video_media_info.receivers[0].codec_payload_type = 42;
  video_media_info.receivers[0].firs_sent = 5;
  video_media_info.receivers[0].plis_sent = 6;
  video_media_info.receivers[0].nacks_sent = 7;
  video_media_info.receivers[0].frames_decoded = 8;
  video_media_info.receivers[0].key_frames_decoded = 3;
  video_media_info.receivers[0].qp_sum = absl::nullopt;
  video_media_info.receivers[0].total_decode_time_ms = 9000;
  video_media_info.receivers[0].total_inter_frame_delay = 0.123;
  video_media_info.receivers[0].total_squared_inter_frame_delay = 0.00456;

  video_media_info.receivers[0].last_packet_received_timestamp_ms =
      absl::nullopt;
  video_media_info.receivers[0].content_type = VideoContentType::UNSPECIFIED;
  video_media_info.receivers[0].estimated_playout_ntp_timestamp_ms =
      absl::nullopt;
  video_media_info.receivers[0].decoder_implementation_name = "";

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = 0;
  video_media_info.receive_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);
  stats_->SetupRemoteTrackAndReceiver(
      cricket::MEDIA_TYPE_VIDEO, "RemoteVideoTrackID", "RemoteStreamId", 1);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCInboundRTPStreamStats expected_video("RTCInboundRTPVideoStream_1",
                                          report->timestamp_us());
  expected_video.ssrc = 1;
  expected_video.is_remote = false;
  expected_video.media_type = "video";
  expected_video.kind = "video";
  expected_video.track_id = IdForType<RTCMediaStreamTrackStats>(report);
  expected_video.transport_id = "RTCTransport_TransportName_1";
  expected_video.codec_id = "RTCCodec_VideoMid_Inbound_42";
  expected_video.fir_count = 5;
  expected_video.pli_count = 6;
  expected_video.nack_count = 7;
  expected_video.packets_received = 2;
  expected_video.bytes_received = 3;
  expected_video.header_bytes_received = 12;
  expected_video.packets_lost = 42;
  expected_video.frames_decoded = 8;
  expected_video.key_frames_decoded = 3;
  // |expected_video.qp_sum| should be undefined.
  expected_video.total_decode_time = 9.0;
  expected_video.total_inter_frame_delay = 0.123;
  expected_video.total_squared_inter_frame_delay = 0.00456;
  // |expected_video.last_packet_received_timestamp| should be undefined.
  // |expected_video.content_type| should be undefined.
  // |expected_video.decoder_implementation| should be undefined.

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCInboundRTPStreamStats>(),
      expected_video);

  // Set previously undefined values and "GetStats" again.
  video_media_info.receivers[0].qp_sum = 9;
  expected_video.qp_sum = 9;
  video_media_info.receivers[0].last_packet_received_timestamp_ms = 1000;
  expected_video.last_packet_received_timestamp = 1.0;
  video_media_info.receivers[0].content_type = VideoContentType::SCREENSHARE;
  expected_video.content_type = "screenshare";
  video_media_info.receivers[0].estimated_playout_ntp_timestamp_ms = 1234;
  expected_video.estimated_playout_timestamp = 1234;
  video_media_info.receivers[0].decoder_implementation_name = "libfoodecoder";
  expected_video.decoder_implementation = "libfoodecoder";
  video_media_channel->SetStats(video_media_info);

  report = stats_->GetFreshStatsReport();

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCInboundRTPStreamStats>(),
      expected_video);
  EXPECT_TRUE(report->Get(*expected_video.track_id));
  EXPECT_TRUE(report->Get(*expected_video.transport_id));
  EXPECT_TRUE(report->Get(*expected_video.codec_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCOutboundRTPStreamStats_Audio) {
  cricket::VoiceMediaInfo voice_media_info;

  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = 1;
  voice_media_info.senders[0].packets_sent = 2;
  voice_media_info.senders[0].retransmitted_packets_sent = 20;
  voice_media_info.senders[0].payload_bytes_sent = 3;
  voice_media_info.senders[0].header_and_padding_bytes_sent = 12;
  voice_media_info.senders[0].retransmitted_bytes_sent = 30;
  voice_media_info.senders[0].codec_payload_type = 42;

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = 0;
  voice_media_info.send_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  auto* voice_media_channel = pc_->AddVoiceChannel("AudioMid", "TransportName");
  voice_media_channel->SetStats(voice_media_info);
  stats_->SetupLocalTrackAndSender(cricket::MEDIA_TYPE_AUDIO,
                                   "LocalAudioTrackID", 1, true,
                                   /*attachment_id=*/50);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCOutboundRTPStreamStats expected_audio("RTCOutboundRTPAudioStream_1",
                                           report->timestamp_us());
  expected_audio.media_source_id = "RTCAudioSource_50";
  // |expected_audio.remote_id| should be undefined.
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.kind = "audio";
  expected_audio.track_id = IdForType<RTCMediaStreamTrackStats>(report);
  expected_audio.transport_id = "RTCTransport_TransportName_1";
  expected_audio.codec_id = "RTCCodec_AudioMid_Outbound_42";
  expected_audio.packets_sent = 2;
  expected_audio.retransmitted_packets_sent = 20;
  expected_audio.bytes_sent = 3;
  expected_audio.header_bytes_sent = 12;
  expected_audio.retransmitted_bytes_sent = 30;

  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(
      report->Get(expected_audio.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_audio);

  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(
      report->Get(expected_audio.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_audio);
  EXPECT_TRUE(report->Get(*expected_audio.track_id));
  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
  EXPECT_TRUE(report->Get(*expected_audio.codec_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCOutboundRTPStreamStats_Video) {
  cricket::VideoMediaInfo video_media_info;

  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].local_stats[0].ssrc = 1;
  video_media_info.senders[0].firs_rcvd = 2;
  video_media_info.senders[0].plis_rcvd = 3;
  video_media_info.senders[0].nacks_rcvd = 4;
  video_media_info.senders[0].packets_sent = 5;
  video_media_info.senders[0].retransmitted_packets_sent = 50;
  video_media_info.senders[0].payload_bytes_sent = 6;
  video_media_info.senders[0].header_and_padding_bytes_sent = 12;
  video_media_info.senders[0].retransmitted_bytes_sent = 60;
  video_media_info.senders[0].codec_payload_type = 42;
  video_media_info.senders[0].frames_encoded = 8;
  video_media_info.senders[0].key_frames_encoded = 3;
  video_media_info.senders[0].total_encode_time_ms = 9000;
  video_media_info.senders[0].total_encoded_bytes_target = 1234;
  video_media_info.senders[0].total_packet_send_delay_ms = 10000;
  video_media_info.senders[0].quality_limitation_reason =
      QualityLimitationReason::kBandwidth;
  video_media_info.senders[0].quality_limitation_resolution_changes = 56u;
  video_media_info.senders[0].qp_sum = absl::nullopt;
  video_media_info.senders[0].content_type = VideoContentType::UNSPECIFIED;
  video_media_info.senders[0].encoder_implementation_name = "";

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = 0;
  video_media_info.send_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);
  stats_->SetupLocalTrackAndSender(cricket::MEDIA_TYPE_VIDEO,
                                   "LocalVideoTrackID", 1, true,
                                   /*attachment_id=*/50);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  auto stats_of_my_type = report->GetStatsOfType<RTCOutboundRTPStreamStats>();
  ASSERT_EQ(1U, stats_of_my_type.size());
  auto stats_of_track_type = report->GetStatsOfType<RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, stats_of_track_type.size());

  RTCOutboundRTPStreamStats expected_video(stats_of_my_type[0]->id(),
                                           report->timestamp_us());
  expected_video.media_source_id = "RTCVideoSource_50";
  // |expected_video.remote_id| should be undefined.
  expected_video.ssrc = 1;
  expected_video.is_remote = false;
  expected_video.media_type = "video";
  expected_video.kind = "video";
  expected_video.track_id = stats_of_track_type[0]->id();
  expected_video.transport_id = "RTCTransport_TransportName_1";
  expected_video.codec_id = "RTCCodec_VideoMid_Outbound_42";
  expected_video.fir_count = 2;
  expected_video.pli_count = 3;
  expected_video.nack_count = 4;
  expected_video.packets_sent = 5;
  expected_video.retransmitted_packets_sent = 50;
  expected_video.bytes_sent = 6;
  expected_video.header_bytes_sent = 12;
  expected_video.retransmitted_bytes_sent = 60;
  expected_video.frames_encoded = 8;
  expected_video.key_frames_encoded = 3;
  expected_video.total_encode_time = 9.0;
  expected_video.total_encoded_bytes_target = 1234;
  expected_video.total_packet_send_delay = 10.0;
  expected_video.quality_limitation_reason = "bandwidth";
  expected_video.quality_limitation_resolution_changes = 56u;
  // |expected_video.content_type| should be undefined.
  // |expected_video.qp_sum| should be undefined.
  // |expected_video.encoder_implementation| should be undefined.
  ASSERT_TRUE(report->Get(expected_video.id()));

  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_video);

  // Set previously undefined values and "GetStats" again.
  video_media_info.senders[0].qp_sum = 9;
  expected_video.qp_sum = 9;
  video_media_info.senders[0].content_type = VideoContentType::SCREENSHARE;
  expected_video.content_type = "screenshare";
  video_media_info.senders[0].encoder_implementation_name = "libfooencoder";
  expected_video.encoder_implementation = "libfooencoder";
  video_media_channel->SetStats(video_media_info);

  report = stats_->GetFreshStatsReport();

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_video);
  EXPECT_TRUE(report->Get(*expected_video.track_id));
  EXPECT_TRUE(report->Get(*expected_video.transport_id));
  EXPECT_TRUE(report->Get(*expected_video.codec_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCTransportStats) {
  const char kTransportName[] = "transport";

  pc_->AddVoiceChannel("audio", kTransportName);

  std::unique_ptr<cricket::Candidate> rtp_local_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol", rtc::ADAPTER_TYPE_WIFI,
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtp_remote_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          rtc::ADAPTER_TYPE_UNKNOWN, cricket::LOCAL_PORT_TYPE,
                          42);
  std::unique_ptr<cricket::Candidate> rtcp_local_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol", rtc::ADAPTER_TYPE_WIFI,
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtcp_remote_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          rtc::ADAPTER_TYPE_UNKNOWN, cricket::LOCAL_PORT_TYPE,
                          42);

  cricket::ConnectionInfo rtp_connection_info;
  rtp_connection_info.best_connection = false;
  rtp_connection_info.local_candidate = *rtp_local_candidate.get();
  rtp_connection_info.remote_candidate = *rtp_remote_candidate.get();
  rtp_connection_info.sent_total_bytes = 42;
  rtp_connection_info.recv_total_bytes = 1337;
  cricket::TransportChannelStats rtp_transport_channel_stats;
  rtp_transport_channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  rtp_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      rtp_connection_info);
  rtp_transport_channel_stats.dtls_state = cricket::DTLS_TRANSPORT_NEW;
  rtp_transport_channel_stats.ice_transport_stats
      .selected_candidate_pair_changes = 1;
  pc_->SetTransportStats(kTransportName, {rtp_transport_channel_stats});

  // Get stats without RTCP, an active connection or certificates.
  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCTransportStats expected_rtp_transport(
      "RTCTransport_transport_" +
          rtc::ToString(cricket::ICE_CANDIDATE_COMPONENT_RTP),
      report->timestamp_us());
  expected_rtp_transport.bytes_sent = 42;
  expected_rtp_transport.bytes_received = 1337;
  expected_rtp_transport.dtls_state = RTCDtlsTransportState::kNew;
  expected_rtp_transport.selected_candidate_pair_changes = 1;

  ASSERT_TRUE(report->Get(expected_rtp_transport.id()));
  EXPECT_EQ(
      expected_rtp_transport,
      report->Get(expected_rtp_transport.id())->cast_to<RTCTransportStats>());

  cricket::ConnectionInfo rtcp_connection_info;
  rtcp_connection_info.best_connection = false;
  rtcp_connection_info.local_candidate = *rtcp_local_candidate.get();
  rtcp_connection_info.remote_candidate = *rtcp_remote_candidate.get();
  rtcp_connection_info.sent_total_bytes = 1337;
  rtcp_connection_info.recv_total_bytes = 42;
  cricket::TransportChannelStats rtcp_transport_channel_stats;
  rtcp_transport_channel_stats.component =
      cricket::ICE_CANDIDATE_COMPONENT_RTCP;
  rtcp_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      rtcp_connection_info);
  rtcp_transport_channel_stats.dtls_state = cricket::DTLS_TRANSPORT_CONNECTING;
  pc_->SetTransportStats(kTransportName, {rtp_transport_channel_stats,
                                          rtcp_transport_channel_stats});

  // Get stats with RTCP and without an active connection or certificates.
  report = stats_->GetFreshStatsReport();

  RTCTransportStats expected_rtcp_transport(
      "RTCTransport_transport_" +
          rtc::ToString(cricket::ICE_CANDIDATE_COMPONENT_RTCP),
      report->timestamp_us());
  expected_rtcp_transport.bytes_sent = 1337;
  expected_rtcp_transport.bytes_received = 42;
  expected_rtcp_transport.dtls_state = RTCDtlsTransportState::kConnecting;
  expected_rtcp_transport.selected_candidate_pair_changes = 0;

  expected_rtp_transport.rtcp_transport_stats_id = expected_rtcp_transport.id();
  ASSERT_TRUE(report->Get(expected_rtp_transport.id()));
  EXPECT_EQ(
      expected_rtp_transport,
      report->Get(expected_rtp_transport.id())->cast_to<RTCTransportStats>());
  ASSERT_TRUE(report->Get(expected_rtcp_transport.id()));
  EXPECT_EQ(
      expected_rtcp_transport,
      report->Get(expected_rtcp_transport.id())->cast_to<RTCTransportStats>());

  // Get stats with an active connection (selected candidate pair).
  rtcp_transport_channel_stats.ice_transport_stats.connection_infos[0]
      .best_connection = true;
  pc_->SetTransportStats(kTransportName, {rtp_transport_channel_stats,
                                          rtcp_transport_channel_stats});

  report = stats_->GetFreshStatsReport();

  expected_rtcp_transport.selected_candidate_pair_id =
      "RTCIceCandidatePair_" + rtcp_local_candidate->id() + "_" +
      rtcp_remote_candidate->id();

  ASSERT_TRUE(report->Get(expected_rtp_transport.id()));
  EXPECT_EQ(
      expected_rtp_transport,
      report->Get(expected_rtp_transport.id())->cast_to<RTCTransportStats>());
  ASSERT_TRUE(report->Get(expected_rtcp_transport.id()));
  EXPECT_EQ(
      expected_rtcp_transport,
      report->Get(expected_rtcp_transport.id())->cast_to<RTCTransportStats>());

  // Get stats with certificates.
  std::unique_ptr<CertificateInfo> local_certinfo =
      CreateFakeCertificateAndInfoFromDers({"(local) local", "(local) chain"});
  pc_->SetLocalCertificate(kTransportName, local_certinfo->certificate);
  std::unique_ptr<CertificateInfo> remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          {"(remote) local", "(remote) chain"});
  pc_->SetRemoteCertChain(
      kTransportName,
      remote_certinfo->certificate->GetSSLCertificateChain().Clone());

  report = stats_->GetFreshStatsReport();

  expected_rtp_transport.local_certificate_id =
      "RTCCertificate_" + local_certinfo->fingerprints[0];
  expected_rtp_transport.remote_certificate_id =
      "RTCCertificate_" + remote_certinfo->fingerprints[0];

  expected_rtcp_transport.local_certificate_id =
      *expected_rtp_transport.local_certificate_id;
  expected_rtcp_transport.remote_certificate_id =
      *expected_rtp_transport.remote_certificate_id;

  ASSERT_TRUE(report->Get(expected_rtp_transport.id()));
  EXPECT_EQ(
      expected_rtp_transport,
      report->Get(expected_rtp_transport.id())->cast_to<RTCTransportStats>());
  ASSERT_TRUE(report->Get(expected_rtcp_transport.id()));
  EXPECT_EQ(
      expected_rtcp_transport,
      report->Get(expected_rtcp_transport.id())->cast_to<RTCTransportStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCTransportStatsWithCrypto) {
  const char kTransportName[] = "transport";

  pc_->AddVoiceChannel("audio", kTransportName);

  std::unique_ptr<cricket::Candidate> rtp_local_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol", rtc::ADAPTER_TYPE_WIFI,
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtp_remote_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          rtc::ADAPTER_TYPE_UNKNOWN, cricket::LOCAL_PORT_TYPE,
                          42);
  std::unique_ptr<cricket::Candidate> rtcp_local_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol", rtc::ADAPTER_TYPE_WIFI,
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtcp_remote_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          rtc::ADAPTER_TYPE_UNKNOWN, cricket::LOCAL_PORT_TYPE,
                          42);

  cricket::ConnectionInfo rtp_connection_info;
  rtp_connection_info.best_connection = false;
  rtp_connection_info.local_candidate = *rtp_local_candidate.get();
  rtp_connection_info.remote_candidate = *rtp_remote_candidate.get();
  rtp_connection_info.sent_total_bytes = 42;
  rtp_connection_info.recv_total_bytes = 1337;
  cricket::TransportChannelStats rtp_transport_channel_stats;
  rtp_transport_channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  rtp_transport_channel_stats.ice_transport_stats.connection_infos.push_back(
      rtp_connection_info);
  // The state must be connected in order for crypto parameters to show up.
  rtp_transport_channel_stats.dtls_state = cricket::DTLS_TRANSPORT_CONNECTED;
  rtp_transport_channel_stats.ice_transport_stats
      .selected_candidate_pair_changes = 1;
  rtp_transport_channel_stats.ssl_version_bytes = 0x0203;
  // 0x2F is TLS_RSA_WITH_AES_128_CBC_SHA according to IANA
  rtp_transport_channel_stats.ssl_cipher_suite = 0x2F;
  rtp_transport_channel_stats.srtp_crypto_suite = rtc::SRTP_AES128_CM_SHA1_80;
  pc_->SetTransportStats(kTransportName, {rtp_transport_channel_stats});

  // Get stats
  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCTransportStats expected_rtp_transport(
      "RTCTransport_transport_" +
          rtc::ToString(cricket::ICE_CANDIDATE_COMPONENT_RTP),
      report->timestamp_us());
  expected_rtp_transport.bytes_sent = 42;
  expected_rtp_transport.bytes_received = 1337;
  expected_rtp_transport.dtls_state = RTCDtlsTransportState::kConnected;
  expected_rtp_transport.selected_candidate_pair_changes = 1;
  // Crypto parameters
  expected_rtp_transport.tls_version = "0203";
  expected_rtp_transport.dtls_cipher = "TLS_RSA_WITH_AES_128_CBC_SHA";
  expected_rtp_transport.srtp_cipher = "AES_CM_128_HMAC_SHA1_80";

  ASSERT_TRUE(report->Get(expected_rtp_transport.id()));
  EXPECT_EQ(
      expected_rtp_transport,
      report->Get(expected_rtp_transport.id())->cast_to<RTCTransportStats>());
}

TEST_F(RTCStatsCollectorTest, CollectNoStreamRTCOutboundRTPStreamStats_Audio) {
  cricket::VoiceMediaInfo voice_media_info;

  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = 1;
  voice_media_info.senders[0].packets_sent = 2;
  voice_media_info.senders[0].retransmitted_packets_sent = 20;
  voice_media_info.senders[0].payload_bytes_sent = 3;
  voice_media_info.senders[0].header_and_padding_bytes_sent = 4;
  voice_media_info.senders[0].retransmitted_bytes_sent = 30;
  voice_media_info.senders[0].codec_payload_type = 42;

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = 0;
  voice_media_info.send_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  // Emulates the case where AddTrack is used without an associated MediaStream
  auto* voice_media_channel = pc_->AddVoiceChannel("AudioMid", "TransportName");
  voice_media_channel->SetStats(voice_media_info);
  stats_->SetupLocalTrackAndSender(cricket::MEDIA_TYPE_AUDIO,
                                   "LocalAudioTrackID", 1, false,
                                   /*attachment_id=*/50);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCOutboundRTPStreamStats expected_audio("RTCOutboundRTPAudioStream_1",
                                           report->timestamp_us());
  expected_audio.media_source_id = "RTCAudioSource_50";
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.kind = "audio";
  expected_audio.track_id = IdForType<RTCMediaStreamTrackStats>(report);
  expected_audio.transport_id = "RTCTransport_TransportName_1";
  expected_audio.codec_id = "RTCCodec_AudioMid_Outbound_42";
  expected_audio.packets_sent = 2;
  expected_audio.retransmitted_packets_sent = 20;
  expected_audio.bytes_sent = 3;
  expected_audio.header_bytes_sent = 4;
  expected_audio.retransmitted_bytes_sent = 30;

  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(
      report->Get(expected_audio.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_audio);
  EXPECT_TRUE(report->Get(*expected_audio.track_id));
  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
  EXPECT_TRUE(report->Get(*expected_audio.codec_id));
}

TEST_F(RTCStatsCollectorTest, RTCAudioSourceStatsCollectedForSenderWithTrack) {
  const uint32_t kSsrc = 4;
  const int kAttachmentId = 42;

  cricket::VoiceMediaInfo voice_media_info;
  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = kSsrc;
  voice_media_info.senders[0].audio_level = 32767;  // [0,32767]
  voice_media_info.senders[0].total_input_energy = 2.0;
  voice_media_info.senders[0].total_input_duration = 3.0;
  auto* voice_media_channel = pc_->AddVoiceChannel("AudioMid", "TransportName");
  voice_media_channel->SetStats(voice_media_info);
  stats_->SetupLocalTrackAndSender(cricket::MEDIA_TYPE_AUDIO,
                                   "LocalAudioTrackID", kSsrc, false,
                                   kAttachmentId);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCAudioSourceStats expected_audio("RTCAudioSource_42",
                                     report->timestamp_us());
  expected_audio.track_identifier = "LocalAudioTrackID";
  expected_audio.kind = "audio";
  expected_audio.audio_level = 1.0;  // [0,1]
  expected_audio.total_audio_energy = 2.0;
  expected_audio.total_samples_duration = 3.0;

  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(report->Get(expected_audio.id())->cast_to<RTCAudioSourceStats>(),
            expected_audio);
}

TEST_F(RTCStatsCollectorTest, RTCVideoSourceStatsCollectedForSenderWithTrack) {
  const uint32_t kSsrc = 4;
  const int kAttachmentId = 42;
  const int kVideoSourceWidth = 12;
  const int kVideoSourceHeight = 34;

  cricket::VideoMediaInfo video_media_info;
  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].local_stats[0].ssrc = kSsrc;
  video_media_info.senders[0].framerate_input = 29;
  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);

  auto video_source = FakeVideoTrackSourceForStats::Create(kVideoSourceWidth,
                                                           kVideoSourceHeight);
  auto video_track = FakeVideoTrackForStats::Create(
      "LocalVideoTrackID", MediaStreamTrackInterface::kLive, video_source);
  rtc::scoped_refptr<MockRtpSenderInternal> sender = CreateMockSender(
      cricket::MEDIA_TYPE_VIDEO, video_track, kSsrc, kAttachmentId, {});
  pc_->AddSender(sender);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCVideoSourceStats expected_video("RTCVideoSource_42",
                                     report->timestamp_us());
  expected_video.track_identifier = "LocalVideoTrackID";
  expected_video.kind = "video";
  expected_video.width = kVideoSourceWidth;
  expected_video.height = kVideoSourceHeight;
  // |expected_video.frames| is expected to be undefined because it is not set.
  // TODO(hbos): When implemented, set its expected value here.
  expected_video.frames_per_second = 29;

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(report->Get(expected_video.id())->cast_to<RTCVideoSourceStats>(),
            expected_video);
}

// This test exercises the current behavior and code path, but the correct
// behavior is to report frame rate even if we have no SSRC.
// TODO(hbos): When we know the frame rate even if we have no SSRC, update the
// expectations of this test.
TEST_F(RTCStatsCollectorTest,
       RTCVideoSourceStatsMissingFrameRateWhenSenderHasNoSsrc) {
  // TODO(https://crbug.com/webrtc/8694): When 0 is no longer a magic value for
  // "none", update this test.
  const uint32_t kNoSsrc = 0;
  const int kAttachmentId = 42;
  const int kVideoSourceWidth = 12;
  const int kVideoSourceHeight = 34;

  cricket::VideoMediaInfo video_media_info;
  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].framerate_input = 29;
  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);

  auto video_source = FakeVideoTrackSourceForStats::Create(kVideoSourceWidth,
                                                           kVideoSourceHeight);
  auto video_track = FakeVideoTrackForStats::Create(
      "LocalVideoTrackID", MediaStreamTrackInterface::kLive, video_source);
  rtc::scoped_refptr<MockRtpSenderInternal> sender = CreateMockSender(
      cricket::MEDIA_TYPE_VIDEO, video_track, kNoSsrc, kAttachmentId, {});
  pc_->AddSender(sender);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  ASSERT_TRUE(report->Get("RTCVideoSource_42"));
  auto video_stats =
      report->Get("RTCVideoSource_42")->cast_to<RTCVideoSourceStats>();
  EXPECT_FALSE(video_stats.frames_per_second.is_defined());
}

// The track not having a source is not expected to be true in practise, but
// this is true in some tests relying on fakes. This test covers that code path.
TEST_F(RTCStatsCollectorTest,
       RTCVideoSourceStatsMissingResolutionWhenTrackHasNoSource) {
  const uint32_t kSsrc = 4;
  const int kAttachmentId = 42;

  cricket::VideoMediaInfo video_media_info;
  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].local_stats[0].ssrc = kSsrc;
  video_media_info.senders[0].framerate_input = 29;
  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);

  auto video_track = FakeVideoTrackForStats::Create(
      "LocalVideoTrackID", MediaStreamTrackInterface::kLive,
      /*source=*/nullptr);
  rtc::scoped_refptr<MockRtpSenderInternal> sender = CreateMockSender(
      cricket::MEDIA_TYPE_VIDEO, video_track, kSsrc, kAttachmentId, {});
  pc_->AddSender(sender);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  ASSERT_TRUE(report->Get("RTCVideoSource_42"));
  auto video_stats =
      report->Get("RTCVideoSource_42")->cast_to<RTCVideoSourceStats>();
  EXPECT_FALSE(video_stats.width.is_defined());
  EXPECT_FALSE(video_stats.height.is_defined());
}

TEST_F(RTCStatsCollectorTest,
       RTCAudioSourceStatsNotCollectedForSenderWithoutTrack) {
  const uint32_t kSsrc = 4;
  const int kAttachmentId = 42;

  cricket::VoiceMediaInfo voice_media_info;
  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = kSsrc;
  auto* voice_media_channel = pc_->AddVoiceChannel("AudioMid", "TransportName");
  voice_media_channel->SetStats(voice_media_info);
  rtc::scoped_refptr<MockRtpSenderInternal> sender = CreateMockSender(
      cricket::MEDIA_TYPE_AUDIO, /*track=*/nullptr, kSsrc, kAttachmentId, {});
  pc_->AddSender(sender);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  EXPECT_FALSE(report->Get("RTCAudioSource_42"));
}

// Parameterized tests on cricket::MediaType (audio or video).
class RTCStatsCollectorTestWithParamKind
    : public RTCStatsCollectorTest,
      public ::testing::WithParamInterface<cricket::MediaType> {
 public:
  RTCStatsCollectorTestWithParamKind() : media_type_(GetParam()) {
    RTC_DCHECK(media_type_ == cricket::MEDIA_TYPE_AUDIO ||
               media_type_ == cricket::MEDIA_TYPE_VIDEO);
  }

  std::string MediaTypeUpperCase() const {
    switch (media_type_) {
      case cricket::MEDIA_TYPE_AUDIO:
        return "Audio";
      case cricket::MEDIA_TYPE_VIDEO:
        return "Video";
      case cricket::MEDIA_TYPE_DATA:
        RTC_NOTREACHED();
        return "";
    }
  }

  std::string MediaTypeLowerCase() const {
    std::string str = MediaTypeUpperCase();
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
  }

  // Adds a sender and channel of the appropriate kind, creating a sender info
  // with the report block's |source_ssrc| and report block data.
  void AddSenderInfoAndMediaChannel(std::string transport_name,
                                    ReportBlockData report_block_data,
                                    absl::optional<RtpCodecParameters> codec) {
    switch (media_type_) {
      case cricket::MEDIA_TYPE_AUDIO: {
        cricket::VoiceMediaInfo voice_media_info;
        voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
        voice_media_info.senders[0].local_stats.push_back(
            cricket::SsrcSenderInfo());
        voice_media_info.senders[0].local_stats[0].ssrc =
            report_block_data.report_block().source_ssrc;
        if (codec.has_value()) {
          voice_media_info.senders[0].codec_payload_type = codec->payload_type;
          voice_media_info.send_codecs.insert(
              std::make_pair(codec->payload_type, *codec));
        }
        voice_media_info.senders[0].report_block_datas.push_back(
            report_block_data);
        auto* voice_media_channel = pc_->AddVoiceChannel("mid", transport_name);
        voice_media_channel->SetStats(voice_media_info);
        return;
      }
      case cricket::MEDIA_TYPE_VIDEO: {
        cricket::VideoMediaInfo video_media_info;
        video_media_info.senders.push_back(cricket::VideoSenderInfo());
        video_media_info.senders[0].local_stats.push_back(
            cricket::SsrcSenderInfo());
        video_media_info.senders[0].local_stats[0].ssrc =
            report_block_data.report_block().source_ssrc;
        if (codec.has_value()) {
          video_media_info.senders[0].codec_payload_type = codec->payload_type;
          video_media_info.send_codecs.insert(
              std::make_pair(codec->payload_type, *codec));
        }
        video_media_info.senders[0].report_block_datas.push_back(
            report_block_data);
        auto* video_media_channel = pc_->AddVideoChannel("mid", transport_name);
        video_media_channel->SetStats(video_media_info);
        return;
      }
      case cricket::MEDIA_TYPE_DATA:
        RTC_NOTREACHED();
    }
  }

 protected:
  cricket::MediaType media_type_;
};

// Verifies RTCRemoteInboundRtpStreamStats members that don't require
// RTCCodecStats (codecId, jitter) and without setting up an RTCP transport.
TEST_P(RTCStatsCollectorTestWithParamKind,
       RTCRemoteInboundRtpStreamStatsCollectedFromReportBlock) {
  const int64_t kReportBlockTimestampUtcUs = 123456789;
  const int64_t kRoundTripTimeMs = 13000;
  const double kRoundTripTimeSeconds = 13.0;

  // The report block's timestamp cannot be from the future, set the fake clock
  // to match.
  fake_clock_.SetTime(Timestamp::Micros(kReportBlockTimestampUtcUs));

  RTCPReportBlock report_block;
  // The remote-inbound-rtp SSRC and the outbound-rtp SSRC is the same as the
  // |source_ssrc|, "SSRC of the RTP packet sender".
  report_block.source_ssrc = 12;
  report_block.packets_lost = 7;
  ReportBlockData report_block_data;
  report_block_data.SetReportBlock(report_block, kReportBlockTimestampUtcUs);
  report_block_data.AddRoundTripTimeSample(1234);
  // Only the last sample should be exposed as the
  // |RTCRemoteInboundRtpStreamStats::round_trip_time|.
  report_block_data.AddRoundTripTimeSample(kRoundTripTimeMs);

  AddSenderInfoAndMediaChannel("TransportName", report_block_data,
                               absl::nullopt);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCRemoteInboundRtpStreamStats expected_remote_inbound_rtp(
      "RTCRemoteInboundRtp" + MediaTypeUpperCase() + "Stream_12",
      kReportBlockTimestampUtcUs);
  expected_remote_inbound_rtp.ssrc = 12;
  expected_remote_inbound_rtp.kind = MediaTypeLowerCase();
  expected_remote_inbound_rtp.transport_id =
      "RTCTransport_TransportName_1";  // 1 for RTP (we have no RTCP transport)
  expected_remote_inbound_rtp.packets_lost = 7;
  expected_remote_inbound_rtp.local_id =
      "RTCOutboundRTP" + MediaTypeUpperCase() + "Stream_12";
  expected_remote_inbound_rtp.round_trip_time = kRoundTripTimeSeconds;
  // This test does not set up RTCCodecStats, so |codec_id| and |jitter| are
  // expected to be missing. These are tested separately.

  ASSERT_TRUE(report->Get(expected_remote_inbound_rtp.id()));
  EXPECT_EQ(report->Get(expected_remote_inbound_rtp.id())
                ->cast_to<RTCRemoteInboundRtpStreamStats>(),
            expected_remote_inbound_rtp);
  EXPECT_TRUE(report->Get(*expected_remote_inbound_rtp.transport_id));
  ASSERT_TRUE(report->Get(*expected_remote_inbound_rtp.local_id));
  // Lookup works in both directions.
  EXPECT_EQ(*report->Get(*expected_remote_inbound_rtp.local_id)
                 ->cast_to<RTCOutboundRTPStreamStats>()
                 .remote_id,
            expected_remote_inbound_rtp.id());
}

TEST_P(RTCStatsCollectorTestWithParamKind,
       RTCRemoteInboundRtpStreamStatsWithTimestampFromReportBlock) {
  const int64_t kReportBlockTimestampUtcUs = 123456789;
  fake_clock_.SetTime(Timestamp::Micros(kReportBlockTimestampUtcUs));

  RTCPReportBlock report_block;
  // The remote-inbound-rtp SSRC and the outbound-rtp SSRC is the same as the
  // |source_ssrc|, "SSRC of the RTP packet sender".
  report_block.source_ssrc = 12;
  ReportBlockData report_block_data;
  report_block_data.SetReportBlock(report_block, kReportBlockTimestampUtcUs);

  AddSenderInfoAndMediaChannel("TransportName", report_block_data,
                               absl::nullopt);

  // Advance time, it should be OK to have fresher reports than report blocks.
  fake_clock_.AdvanceTime(TimeDelta::Micros(1234));

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  std::string remote_inbound_rtp_id =
      "RTCRemoteInboundRtp" + MediaTypeUpperCase() + "Stream_12";
  ASSERT_TRUE(report->Get(remote_inbound_rtp_id));
  auto& remote_inbound_rtp = report->Get(remote_inbound_rtp_id)
                                 ->cast_to<RTCRemoteInboundRtpStreamStats>();

  // Even though the report time is different, the remote-inbound-rtp timestamp
  // is of the time that the report block was received.
  EXPECT_EQ(kReportBlockTimestampUtcUs + 1234, report->timestamp_us());
  EXPECT_EQ(kReportBlockTimestampUtcUs, remote_inbound_rtp.timestamp_us());
}

TEST_P(RTCStatsCollectorTestWithParamKind,
       RTCRemoteInboundRtpStreamStatsWithCodecBasedMembers) {
  const int64_t kReportBlockTimestampUtcUs = 123456789;
  fake_clock_.SetTime(Timestamp::Micros(kReportBlockTimestampUtcUs));

  RTCPReportBlock report_block;
  // The remote-inbound-rtp SSRC and the outbound-rtp SSRC is the same as the
  // |source_ssrc|, "SSRC of the RTP packet sender".
  report_block.source_ssrc = 12;
  report_block.jitter = 5000;
  ReportBlockData report_block_data;
  report_block_data.SetReportBlock(report_block, kReportBlockTimestampUtcUs);

  RtpCodecParameters codec;
  codec.payload_type = 3;
  codec.kind = media_type_;
  codec.clock_rate = 1000;

  AddSenderInfoAndMediaChannel("TransportName", report_block_data, codec);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  std::string remote_inbound_rtp_id =
      "RTCRemoteInboundRtp" + MediaTypeUpperCase() + "Stream_12";
  ASSERT_TRUE(report->Get(remote_inbound_rtp_id));
  auto& remote_inbound_rtp = report->Get(remote_inbound_rtp_id)
                                 ->cast_to<RTCRemoteInboundRtpStreamStats>();

  EXPECT_TRUE(remote_inbound_rtp.codec_id.is_defined());
  EXPECT_TRUE(report->Get(*remote_inbound_rtp.codec_id));

  EXPECT_TRUE(remote_inbound_rtp.jitter.is_defined());
  // The jitter (in seconds) is the report block's jitter divided by the codec's
  // clock rate.
  EXPECT_EQ(5.0, *remote_inbound_rtp.jitter);
}

TEST_P(RTCStatsCollectorTestWithParamKind,
       RTCRemoteInboundRtpStreamStatsWithRtcpTransport) {
  const int64_t kReportBlockTimestampUtcUs = 123456789;
  fake_clock_.SetTime(Timestamp::Micros(kReportBlockTimestampUtcUs));

  RTCPReportBlock report_block;
  // The remote-inbound-rtp SSRC and the outbound-rtp SSRC is the same as the
  // |source_ssrc|, "SSRC of the RTP packet sender".
  report_block.source_ssrc = 12;
  ReportBlockData report_block_data;
  report_block_data.SetReportBlock(report_block, kReportBlockTimestampUtcUs);

  cricket::TransportChannelStats rtp_transport_channel_stats;
  rtp_transport_channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  rtp_transport_channel_stats.dtls_state = cricket::DTLS_TRANSPORT_NEW;
  cricket::TransportChannelStats rtcp_transport_channel_stats;
  rtcp_transport_channel_stats.component =
      cricket::ICE_CANDIDATE_COMPONENT_RTCP;
  rtcp_transport_channel_stats.dtls_state = cricket::DTLS_TRANSPORT_NEW;
  pc_->SetTransportStats("TransportName", {rtp_transport_channel_stats,
                                           rtcp_transport_channel_stats});
  AddSenderInfoAndMediaChannel("TransportName", report_block_data,
                               absl::nullopt);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  std::string remote_inbound_rtp_id =
      "RTCRemoteInboundRtp" + MediaTypeUpperCase() + "Stream_12";
  ASSERT_TRUE(report->Get(remote_inbound_rtp_id));
  auto& remote_inbound_rtp = report->Get(remote_inbound_rtp_id)
                                 ->cast_to<RTCRemoteInboundRtpStreamStats>();

  EXPECT_TRUE(remote_inbound_rtp.transport_id.is_defined());
  EXPECT_EQ("RTCTransport_TransportName_2",  // 2 for RTCP
            *remote_inbound_rtp.transport_id);
  EXPECT_TRUE(report->Get(*remote_inbound_rtp.transport_id));
}

INSTANTIATE_TEST_SUITE_P(All,
                         RTCStatsCollectorTestWithParamKind,
                         ::testing::Values(cricket::MEDIA_TYPE_AUDIO,    // "/0"
                                           cricket::MEDIA_TYPE_VIDEO));  // "/1"

TEST_F(RTCStatsCollectorTest,
       RTCVideoSourceStatsNotCollectedForSenderWithoutTrack) {
  const uint32_t kSsrc = 4;
  const int kAttachmentId = 42;

  cricket::VideoMediaInfo video_media_info;
  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].local_stats[0].ssrc = kSsrc;
  video_media_info.senders[0].framerate_input = 29;
  auto* video_media_channel = pc_->AddVideoChannel("VideoMid", "TransportName");
  video_media_channel->SetStats(video_media_info);

  rtc::scoped_refptr<MockRtpSenderInternal> sender = CreateMockSender(
      cricket::MEDIA_TYPE_VIDEO, /*track=*/nullptr, kSsrc, kAttachmentId, {});
  pc_->AddSender(sender);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  EXPECT_FALSE(report->Get("RTCVideoSource_42"));
}

TEST_F(RTCStatsCollectorTest, GetStatsWithSenderSelector) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  // Expected stats graph when filtered by sender:
  //
  //  +--- track (sender)
  //  |             ^
  //  |             |
  //  | +--------- outbound-rtp
  //  | |           |        |
  //  | |           v        v
  //  | |  codec (send)     transport
  //  v v
  //  media-source
  rtc::scoped_refptr<const RTCStatsReport> sender_report =
      stats_->GetStatsReportWithSenderSelector(graph.sender);
  EXPECT_TRUE(sender_report);
  EXPECT_EQ(sender_report->timestamp_us(), graph.full_report->timestamp_us());
  EXPECT_EQ(sender_report->size(), 5u);
  EXPECT_TRUE(sender_report->Get(graph.send_codec_id));
  EXPECT_FALSE(sender_report->Get(graph.recv_codec_id));
  EXPECT_TRUE(sender_report->Get(graph.outbound_rtp_id));
  EXPECT_FALSE(sender_report->Get(graph.inbound_rtp_id));
  EXPECT_TRUE(sender_report->Get(graph.transport_id));
  EXPECT_TRUE(sender_report->Get(graph.sender_track_id));
  EXPECT_FALSE(sender_report->Get(graph.receiver_track_id));
  EXPECT_FALSE(sender_report->Get(graph.remote_stream_id));
  EXPECT_FALSE(sender_report->Get(graph.peer_connection_id));
  EXPECT_TRUE(sender_report->Get(graph.media_source_id));
}

TEST_F(RTCStatsCollectorTest, GetStatsWithReceiverSelector) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  // Expected stats graph when filtered by receiver:
  //
  //                                                       track (receiver)
  //                                                         ^
  //                                                         |
  //                              inbound-rtp ---------------+
  //                               |       |
  //                               v       v
  //                        transport     codec (recv)
  rtc::scoped_refptr<const RTCStatsReport> receiver_report =
      stats_->GetStatsReportWithReceiverSelector(graph.receiver);
  EXPECT_TRUE(receiver_report);
  EXPECT_EQ(receiver_report->size(), 4u);
  EXPECT_EQ(receiver_report->timestamp_us(), graph.full_report->timestamp_us());
  EXPECT_FALSE(receiver_report->Get(graph.send_codec_id));
  EXPECT_TRUE(receiver_report->Get(graph.recv_codec_id));
  EXPECT_FALSE(receiver_report->Get(graph.outbound_rtp_id));
  EXPECT_TRUE(receiver_report->Get(graph.inbound_rtp_id));
  EXPECT_TRUE(receiver_report->Get(graph.transport_id));
  EXPECT_FALSE(receiver_report->Get(graph.sender_track_id));
  EXPECT_TRUE(receiver_report->Get(graph.receiver_track_id));
  EXPECT_FALSE(receiver_report->Get(graph.remote_stream_id));
  EXPECT_FALSE(receiver_report->Get(graph.peer_connection_id));
  EXPECT_FALSE(receiver_report->Get(graph.media_source_id));
}

TEST_F(RTCStatsCollectorTest, GetStatsWithNullSenderSelector) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  rtc::scoped_refptr<const RTCStatsReport> empty_report =
      stats_->GetStatsReportWithSenderSelector(nullptr);
  EXPECT_TRUE(empty_report);
  EXPECT_EQ(empty_report->timestamp_us(), graph.full_report->timestamp_us());
  EXPECT_EQ(empty_report->size(), 0u);
}

TEST_F(RTCStatsCollectorTest, GetStatsWithNullReceiverSelector) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  rtc::scoped_refptr<const RTCStatsReport> empty_report =
      stats_->GetStatsReportWithReceiverSelector(nullptr);
  EXPECT_TRUE(empty_report);
  EXPECT_EQ(empty_report->timestamp_us(), graph.full_report->timestamp_us());
  EXPECT_EQ(empty_report->size(), 0u);
}

// When the PC has not had SetLocalDescription done, tracks all have
// SSRC 0, meaning "unconnected".
// In this state, we report on track stats, but not RTP stats.
TEST_F(RTCStatsCollectorTest, StatsReportedOnZeroSsrc) {
  rtc::scoped_refptr<MediaStreamTrackInterface> track =
      CreateFakeTrack(cricket::MEDIA_TYPE_AUDIO, "audioTrack",
                      MediaStreamTrackInterface::kLive);
  rtc::scoped_refptr<MockRtpSenderInternal> sender =
      CreateMockSender(cricket::MEDIA_TYPE_AUDIO, track, 0, 49, {});
  pc_->AddSender(sender);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  std::vector<const RTCMediaStreamTrackStats*> track_stats =
      report->GetStatsOfType<RTCMediaStreamTrackStats>();
  EXPECT_EQ(1U, track_stats.size());

  std::vector<const RTCRTPStreamStats*> rtp_stream_stats =
      report->GetStatsOfType<RTCRTPStreamStats>();
  EXPECT_EQ(0U, rtp_stream_stats.size());
}

TEST_F(RTCStatsCollectorTest, DoNotCrashOnSsrcChange) {
  rtc::scoped_refptr<MediaStreamTrackInterface> track =
      CreateFakeTrack(cricket::MEDIA_TYPE_AUDIO, "audioTrack",
                      MediaStreamTrackInterface::kLive);
  rtc::scoped_refptr<MockRtpSenderInternal> sender =
      CreateMockSender(cricket::MEDIA_TYPE_AUDIO, track, 4711, 49, {});
  pc_->AddSender(sender);

  // We do not generate any matching voice_sender_info stats.
  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  std::vector<const RTCMediaStreamTrackStats*> track_stats =
      report->GetStatsOfType<RTCMediaStreamTrackStats>();
  EXPECT_EQ(1U, track_stats.size());
}

// Used for test below, to test calling GetStatsReport during a callback.
class RecursiveCallback : public RTCStatsCollectorCallback {
 public:
  explicit RecursiveCallback(RTCStatsCollectorWrapper* stats) : stats_(stats) {}

  void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {
    stats_->GetStatsReport();
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  RTCStatsCollectorWrapper* stats_;
  bool called_ = false;
};

// Test that nothing bad happens if a callback causes GetStatsReport to be
// called again recursively. Regression test for crbug.com/webrtc/8973.
TEST_F(RTCStatsCollectorTest, DoNotCrashWhenGetStatsCalledDuringCallback) {
  rtc::scoped_refptr<RecursiveCallback> callback1(
      new rtc::RefCountedObject<RecursiveCallback>(stats_.get()));
  rtc::scoped_refptr<RecursiveCallback> callback2(
      new rtc::RefCountedObject<RecursiveCallback>(stats_.get()));
  stats_->stats_collector()->GetStatsReport(callback1);
  stats_->stats_collector()->GetStatsReport(callback2);
  EXPECT_TRUE_WAIT(callback1->called(), kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(callback2->called(), kGetStatsReportTimeoutMs);
}

class RTCTestStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTestStats(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us), dummy_stat("dummyStat") {}

  RTCStatsMember<int32_t> dummy_stat;
};

WEBRTC_RTCSTATS_IMPL(RTCTestStats, RTCStats, "test-stats", &dummy_stat)

// Overrides the stats collection to verify thread usage and that the resulting
// partial reports are merged.
class FakeRTCStatsCollector : public RTCStatsCollector,
                              public RTCStatsCollectorCallback {
 public:
  static rtc::scoped_refptr<FakeRTCStatsCollector> Create(
      PeerConnectionInternal* pc,
      int64_t cache_lifetime_us) {
    return rtc::scoped_refptr<FakeRTCStatsCollector>(
        new rtc::RefCountedObject<FakeRTCStatsCollector>(pc,
                                                         cache_lifetime_us));
  }

  // RTCStatsCollectorCallback implementation.
  void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    rtc::CritScope cs(&lock_);
    delivered_report_ = report;
  }

  void VerifyThreadUsageAndResultsMerging() {
    GetStatsReport(rtc::scoped_refptr<RTCStatsCollectorCallback>(this));
    EXPECT_TRUE_WAIT(HasVerifiedResults(), kGetStatsReportTimeoutMs);
  }

  bool HasVerifiedResults() {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    rtc::CritScope cs(&lock_);
    if (!delivered_report_)
      return false;
    EXPECT_EQ(produced_on_signaling_thread_, 1);
    EXPECT_EQ(produced_on_network_thread_, 1);

    EXPECT_TRUE(delivered_report_->Get("SignalingThreadStats"));
    EXPECT_TRUE(delivered_report_->Get("NetworkThreadStats"));

    produced_on_signaling_thread_ = 0;
    produced_on_network_thread_ = 0;
    delivered_report_ = nullptr;
    return true;
  }

 protected:
  FakeRTCStatsCollector(PeerConnectionInternal* pc, int64_t cache_lifetime)
      : RTCStatsCollector(pc, cache_lifetime),
        signaling_thread_(pc->signaling_thread()),
        worker_thread_(pc->worker_thread()),
        network_thread_(pc->network_thread()) {}

  void ProducePartialResultsOnSignalingThreadImpl(
      int64_t timestamp_us,
      RTCStatsReport* partial_report) override {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_signaling_thread_;
    }

    partial_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("SignalingThreadStats", timestamp_us)));
  }
  void ProducePartialResultsOnNetworkThreadImpl(
      int64_t timestamp_us,
      const std::map<std::string, cricket::TransportStats>&
          transport_stats_by_name,
      const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
      RTCStatsReport* partial_report) override {
    EXPECT_TRUE(network_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_network_thread_;
    }

    partial_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("NetworkThreadStats", timestamp_us)));
  }

 private:
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;

  rtc::CriticalSection lock_;
  rtc::scoped_refptr<const RTCStatsReport> delivered_report_;
  int produced_on_signaling_thread_ = 0;
  int produced_on_network_thread_ = 0;
};

TEST(RTCStatsCollectorTestWithFakeCollector, ThreadUsageAndResultsMerging) {
  rtc::scoped_refptr<FakePeerConnectionForStats> pc(
      new rtc::RefCountedObject<FakePeerConnectionForStats>());
  rtc::scoped_refptr<FakeRTCStatsCollector> stats_collector(
      FakeRTCStatsCollector::Create(pc, 50 * rtc::kNumMicrosecsPerMillisec));
  stats_collector->VerifyThreadUsageAndResultsMerging();
}

}  // namespace

}  // namespace webrtc
