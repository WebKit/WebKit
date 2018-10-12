/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/rtpparameters.h"
#include "api/stats/rtcstats_objects.h"
#include "api/stats/rtcstatsreport.h"
#include "api/units/time_delta.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/port.h"
#include "pc/mediastream.h"
#include "pc/mediastreamtrack.h"
#include "pc/rtcstatscollector.h"
#include "pc/test/fakepeerconnectionforstats.h"
#include "pc/test/mock_datachannel.h"
#include "pc/test/mock_rtpreceiverinternal.h"
#include "pc/test/mock_rtpsenderinternal.h"
#include "pc/test/rtcstatsobtainer.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/fakesslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/timeutils.h"

using testing::AtLeast;
using testing::Invoke;
using testing::Return;

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
    rtc::replace_substrs("-----BEGIN CERTIFICATE-----", 27, "", 0,
                         &info->pems[i]);
    rtc::replace_substrs("-----END CERTIFICATE-----", 25, "", 0,
                         &info->pems[i]);
    rtc::replace_substrs("\n", 1, "", 0, &info->pems[i]);
  }
  // Fingerprints for the whole certificate chain, starting with leaf
  // certificate.
  const rtc::SSLCertChain& chain = info->certificate->ssl_cert_chain();
  std::unique_ptr<rtc::SSLFingerprint> fp;
  for (size_t i = 0; i < chain.GetSize(); i++) {
    fp.reset(rtc::SSLFingerprint::Create("sha-1", &chain.Get(i)));
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

class FakeVideoTrackForStats : public MediaStreamTrack<VideoTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeVideoTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state) {
    rtc::scoped_refptr<FakeVideoTrackForStats> video_track(
        new rtc::RefCountedObject<FakeVideoTrackForStats>(id));
    video_track->set_state(state);
    return video_track;
  }

  explicit FakeVideoTrackForStats(const std::string& id)
      : MediaStreamTrack<VideoTrackInterface>(id) {}

  std::string kind() const override {
    return MediaStreamTrackInterface::kVideoKind;
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override{};
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override{};

  VideoTrackSourceInterface* GetSource() const override { return nullptr; }
};

rtc::scoped_refptr<MediaStreamTrackInterface> CreateFakeTrack(
    cricket::MediaType media_type,
    const std::string& track_id,
    MediaStreamTrackInterface::TrackState track_state) {
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    return FakeAudioTrackForStats::Create(track_id, track_state);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    return FakeVideoTrackForStats::Create(track_id, track_state);
  }
}

rtc::scoped_refptr<MockRtpSenderInternal> CreateMockSender(
    const rtc::scoped_refptr<MediaStreamTrackInterface>& track,
    uint32_t ssrc,
    int attachment_id,
    std::vector<std::string> local_stream_ids) {
  rtc::scoped_refptr<MockRtpSenderInternal> sender(
      new rtc::RefCountedObject<MockRtpSenderInternal>());
  EXPECT_CALL(*sender, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*sender, ssrc()).WillRepeatedly(Return(ssrc));
  EXPECT_CALL(*sender, media_type())
      .WillRepeatedly(
          Return(track->kind() == MediaStreamTrackInterface::kAudioKind
                     ? cricket::MEDIA_TYPE_AUDIO
                     : cricket::MEDIA_TYPE_VIDEO));
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
      bool add_stream) {
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
        CreateMockSender(track, ssrc, 50, {});
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
    int attachment_id = 147;
    for (auto& pair : local_audio_track_info_pairs) {
      MediaStreamTrackInterface* local_audio_track = pair.first;
      const cricket::VoiceSenderInfo& voice_sender_info = pair.second;
      RTC_DCHECK_EQ(local_audio_track->kind(),
                    MediaStreamTrackInterface::kAudioKind);

      voice_media_info.senders.push_back(voice_sender_info);
      rtc::scoped_refptr<MockRtpSenderInternal> rtp_sender = CreateMockSender(
          rtc::scoped_refptr<MediaStreamTrackInterface>(local_audio_track),
          voice_sender_info.local_stats[0].ssrc, attachment_id++,
          local_stream_ids);
      pc_->AddSender(rtp_sender);
    }

    // Remote audio tracks and voice receiver infos
    attachment_id = 181;
    for (auto& pair : remote_audio_track_info_pairs) {
      MediaStreamTrackInterface* remote_audio_track = pair.first;
      const cricket::VoiceReceiverInfo& voice_receiver_info = pair.second;
      RTC_DCHECK_EQ(remote_audio_track->kind(),
                    MediaStreamTrackInterface::kAudioKind);

      voice_media_info.receivers.push_back(voice_receiver_info);
      rtc::scoped_refptr<MockRtpReceiverInternal> rtp_receiver =
          CreateMockReceiver(
              rtc::scoped_refptr<MediaStreamTrackInterface>(remote_audio_track),
              voice_receiver_info.local_stats[0].ssrc, attachment_id++);
      EXPECT_CALL(*rtp_receiver, streams())
          .WillRepeatedly(Return(remote_streams));
      pc_->AddReceiver(rtp_receiver);
    }

    // Local video tracks and video sender infos
    attachment_id = 151;
    for (auto& pair : local_video_track_info_pairs) {
      MediaStreamTrackInterface* local_video_track = pair.first;
      const cricket::VideoSenderInfo& video_sender_info = pair.second;
      RTC_DCHECK_EQ(local_video_track->kind(),
                    MediaStreamTrackInterface::kVideoKind);

      video_media_info.senders.push_back(video_sender_info);
      rtc::scoped_refptr<MockRtpSenderInternal> rtp_sender = CreateMockSender(
          rtc::scoped_refptr<MediaStreamTrackInterface>(local_video_track),
          video_sender_info.local_stats[0].ssrc, attachment_id++,
          local_stream_ids);
      pc_->AddSender(rtp_sender);
    }

    // Remote video tracks and video receiver infos
    attachment_id = 191;
    for (auto& pair : remote_video_track_info_pairs) {
      MediaStreamTrackInterface* remote_video_track = pair.first;
      const cricket::VideoReceiverInfo& video_receiver_info = pair.second;
      RTC_DCHECK_EQ(remote_video_track->kind(),
                    MediaStreamTrackInterface::kVideoKind);

      video_media_info.receivers.push_back(video_receiver_info);
      rtc::scoped_refptr<MockRtpReceiverInternal> rtp_receiver =
          CreateMockReceiver(
              rtc::scoped_refptr<MediaStreamTrackInterface>(remote_video_track),
              video_receiver_info.local_stats[0].ssrc, attachment_id++);
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

class RTCStatsCollectorTest : public testing::Test {
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
        cricket::MEDIA_TYPE_VIDEO, "LocalVideoTrackID", 3, false);
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

    // Expected stats graph:
    //
    // track (sender)      stream (remote stream) ---> track (receiver)
    //          ^                                        ^
    //          |                                        |
    //         outbound-rtp   inbound-rtp ---------------+
    //          |        |     |       |
    //          v        v     v       v
    // codec (send)     transport     codec (recv)     peer-connection

    // Verify the stats graph is set up correctly.
    graph.full_report = stats_->GetStatsReport();
    EXPECT_EQ(graph.full_report->size(), 9u);
    EXPECT_TRUE(graph.full_report->Get(graph.send_codec_id));
    EXPECT_TRUE(graph.full_report->Get(graph.recv_codec_id));
    EXPECT_TRUE(graph.full_report->Get(graph.outbound_rtp_id));
    EXPECT_TRUE(graph.full_report->Get(graph.inbound_rtp_id));
    EXPECT_TRUE(graph.full_report->Get(graph.transport_id));
    EXPECT_TRUE(graph.full_report->Get(graph.sender_track_id));
    EXPECT_TRUE(graph.full_report->Get(graph.receiver_track_id));
    EXPECT_TRUE(graph.full_report->Get(graph.remote_stream_id));
    EXPECT_TRUE(graph.full_report->Get(graph.peer_connection_id));
    const auto& outbound_rtp = graph.full_report->Get(graph.outbound_rtp_id)
                                   ->cast_to<RTCOutboundRTPStreamStats>();
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
  fake_clock_.AdvanceTime(TimeDelta::ms(51));
  rtc::scoped_refptr<const RTCStatsReport> d = stats_->GetStatsReport();
  EXPECT_TRUE(d);
  EXPECT_NE(c.get(), d.get());
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacksWithInvalidatedCacheInBetween) {
  rtc::scoped_refptr<const RTCStatsReport> a, b, c;
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&a));
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&b));
  // Cache is invalidated after 50 ms.
  fake_clock_.AdvanceTime(TimeDelta::ms(51));
  stats_->stats_collector()->GetStatsReport(RTCStatsObtainer::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);
  EXPECT_EQ(a.get(), b.get());
  // The act of doing |AdvanceTime| processes all messages. If this was not the
  // case we might not require |c| to be fresher than |b|.
  EXPECT_NE(c.get(), b.get());
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
      remote_certinfo->certificate->ssl_cert_chain().UniqueCopy());

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
  voice_media_info.receive_codecs.insert(
      std::make_pair(inbound_audio_codec.payload_type, inbound_audio_codec));

  RtpCodecParameters outbound_audio_codec;
  outbound_audio_codec.payload_type = 2;
  outbound_audio_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  outbound_audio_codec.name = "isac";
  outbound_audio_codec.clock_rate = 1338;
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

  RTCCodecStats expected_outbound_audio_codec("RTCCodec_AudioMid_Outbound_2",
                                              report->timestamp_us());
  expected_outbound_audio_codec.payload_type = 2;
  expected_outbound_audio_codec.mime_type = "audio/isac";
  expected_outbound_audio_codec.clock_rate = 1338;

  RTCCodecStats expected_inbound_video_codec("RTCCodec_VideoMid_Inbound_3",
                                             report->timestamp_us());
  expected_inbound_video_codec.payload_type = 3;
  expected_inbound_video_codec.mime_type = "video/H264";
  expected_inbound_video_codec.clock_rate = 1339;

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
      audio_remote_certinfo->certificate->ssl_cert_chain().UniqueCopy());

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
      video_remote_certinfo->certificate->ssl_cert_chain().UniqueCopy());

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
      remote_certinfo->certificate->ssl_cert_chain().UniqueCopy());

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();
  ExpectReportContainsCertificateInfo(report, *local_certinfo);
  ExpectReportContainsCertificateInfo(report, *remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectRTCDataChannelStats) {
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
  a_transport_channel_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  a_transport_channel_stats.connection_infos[0].local_candidate =
      *a_local_host.get();
  a_transport_channel_stats.connection_infos[0].remote_candidate =
      *a_remote_srflx.get();
  a_transport_channel_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  a_transport_channel_stats.connection_infos[1].local_candidate =
      *a_local_prflx.get();
  a_transport_channel_stats.connection_infos[1].remote_candidate =
      *a_remote_relay.get();
  a_transport_channel_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  a_transport_channel_stats.connection_infos[2].local_candidate =
      *a_local_relay.get();
  a_transport_channel_stats.connection_infos[2].remote_candidate =
      *a_remote_relay.get();

  pc_->AddVoiceChannel("audio", "a");
  pc_->SetTransportStats("a", a_transport_channel_stats);

  cricket::TransportChannelStats b_transport_channel_stats;
  b_transport_channel_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  b_transport_channel_stats.connection_infos[0].local_candidate =
      *b_local.get();
  b_transport_channel_stats.connection_infos[0].remote_candidate =
      *b_remote.get();

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
  transport_channel_stats.connection_infos.push_back(connection_info);

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
  transport_channel_stats.connection_infos[0].nominated = true;
  pc_->SetTransportStats(kTransportName, transport_channel_stats);
  report = stats_->GetFreshStatsReport();
  expected_pair.nominated = true;
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Set round trip times and "GetStats" again.
  transport_channel_stats.connection_infos[0].total_round_trip_time_ms = 7331;
  transport_channel_stats.connection_infos[0].current_round_trip_time_ms = 1337;
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
  transport_channel_stats.connection_infos[0].best_connection = true;
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
  voice_sender_info_ssrc1.audio_level = 32767;
  voice_sender_info_ssrc1.total_input_energy = 0.25;
  voice_sender_info_ssrc1.total_input_duration = 0.5;
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
  expected_local_audio_track_ssrc1.remote_source = false;
  expected_local_audio_track_ssrc1.ended = true;
  expected_local_audio_track_ssrc1.detached = false;
  expected_local_audio_track_ssrc1.audio_level = 1.0;
  expected_local_audio_track_ssrc1.total_audio_energy = 0.25;
  expected_local_audio_track_ssrc1.total_samples_duration = 0.5;
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
  voice_receiver_info.jitter_buffer_delay_seconds = 3456;

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
  expected_remote_audio_track.remote_source = true;
  expected_remote_audio_track.ended = false;
  expected_remote_audio_track.detached = false;
  expected_remote_audio_track.audio_level = 16383.0 / 32767.0;
  expected_remote_audio_track.total_audio_energy = 0.125;
  expected_remote_audio_track.total_samples_received = 4567;
  expected_remote_audio_track.total_samples_duration = 0.25;
  expected_remote_audio_track.concealed_samples = 123;
  expected_remote_audio_track.concealment_events = 12;
  expected_remote_audio_track.jitter_buffer_delay = 3456;
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
  video_receiver_info_ssrc3.frames_received = 1000;
  video_receiver_info_ssrc3.frames_decoded = 995;
  video_receiver_info_ssrc3.frames_rendered = 990;

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
  expected_remote_video_track_ssrc3.remote_source = true;
  expected_remote_video_track_ssrc3.ended = true;
  expected_remote_video_track_ssrc3.detached = false;
  expected_remote_video_track_ssrc3.frame_width = 6789;
  expected_remote_video_track_ssrc3.frame_height = 9876;
  expected_remote_video_track_ssrc3.frames_received = 1000;
  expected_remote_video_track_ssrc3.frames_decoded = 995;
  expected_remote_video_track_ssrc3.frames_dropped = 1000 - 990;
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
  voice_media_info.receivers[0].bytes_rcvd = 3;
  voice_media_info.receivers[0].codec_payload_type = 42;
  voice_media_info.receivers[0].jitter_ms = 4500;
  voice_media_info.receivers[0].fraction_lost = 5.5f;

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
  expected_audio.bytes_received = 3;
  expected_audio.packets_lost = -1;
  expected_audio.jitter = 4.5;
  expected_audio.fraction_lost = 5.5;
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
  video_media_info.receivers[0].bytes_rcvd = 3;
  video_media_info.receivers[0].fraction_lost = 4.5f;
  video_media_info.receivers[0].codec_payload_type = 42;
  video_media_info.receivers[0].firs_sent = 5;
  video_media_info.receivers[0].plis_sent = 6;
  video_media_info.receivers[0].nacks_sent = 7;
  video_media_info.receivers[0].frames_decoded = 8;
  video_media_info.receivers[0].qp_sum = absl::nullopt;

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
  expected_video.packets_lost = 42;
  expected_video.fraction_lost = 4.5;
  expected_video.frames_decoded = 8;
  // |expected_video.qp_sum| should be undefined.

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCInboundRTPStreamStats>(),
      expected_video);

  // Set previously undefined values and "GetStats" again.
  video_media_info.receivers[0].qp_sum = 9;
  expected_video.qp_sum = 9;
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
  voice_media_info.senders[0].bytes_sent = 3;
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
                                   "LocalAudioTrackID", 1, true);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCOutboundRTPStreamStats expected_audio("RTCOutboundRTPAudioStream_1",
                                           report->timestamp_us());
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.kind = "audio";
  expected_audio.track_id = IdForType<RTCMediaStreamTrackStats>(report);
  expected_audio.transport_id = "RTCTransport_TransportName_1";
  expected_audio.codec_id = "RTCCodec_AudioMid_Outbound_42";
  expected_audio.packets_sent = 2;
  expected_audio.bytes_sent = 3;

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
  video_media_info.senders[0].bytes_sent = 6;
  video_media_info.senders[0].codec_payload_type = 42;
  video_media_info.senders[0].frames_encoded = 8;
  video_media_info.senders[0].qp_sum = absl::nullopt;

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
                                   "LocalVideoTrackID", 1, true);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  auto stats_of_my_type = report->GetStatsOfType<RTCOutboundRTPStreamStats>();
  ASSERT_EQ(1U, stats_of_my_type.size());
  auto stats_of_track_type = report->GetStatsOfType<RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, stats_of_track_type.size());

  RTCOutboundRTPStreamStats expected_video(stats_of_my_type[0]->id(),
                                           report->timestamp_us());
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
  expected_video.bytes_sent = 6;
  expected_video.frames_encoded = 8;
  // |expected_video.qp_sum| should be undefined.
  ASSERT_TRUE(report->Get(expected_video.id()));

  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_video);

  // Set previously undefined values and "GetStats" again.
  video_media_info.senders[0].qp_sum = 9;
  expected_video.qp_sum = 9;
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
  rtp_transport_channel_stats.connection_infos.push_back(rtp_connection_info);
  rtp_transport_channel_stats.dtls_state = cricket::DTLS_TRANSPORT_NEW;
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
  rtcp_transport_channel_stats.connection_infos.push_back(rtcp_connection_info);
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
  rtcp_transport_channel_stats.connection_infos[0].best_connection = true;
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
      remote_certinfo->certificate->ssl_cert_chain().UniqueCopy());

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

TEST_F(RTCStatsCollectorTest, CollectNoStreamRTCOutboundRTPStreamStats_Audio) {
  cricket::VoiceMediaInfo voice_media_info;

  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = 1;
  voice_media_info.senders[0].packets_sent = 2;
  voice_media_info.senders[0].bytes_sent = 3;
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
                                   "LocalAudioTrackID", 1, false);

  rtc::scoped_refptr<const RTCStatsReport> report = stats_->GetStatsReport();

  RTCOutboundRTPStreamStats expected_audio("RTCOutboundRTPAudioStream_1",
                                           report->timestamp_us());
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.kind = "audio";
  expected_audio.track_id = IdForType<RTCMediaStreamTrackStats>(report);
  expected_audio.transport_id = "RTCTransport_TransportName_1";
  expected_audio.codec_id = "RTCCodec_AudioMid_Outbound_42";
  expected_audio.packets_sent = 2;
  expected_audio.bytes_sent = 3;

  ASSERT_TRUE(report->Get(expected_audio.id()));
  EXPECT_EQ(
      report->Get(expected_audio.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_audio);
  EXPECT_TRUE(report->Get(*expected_audio.track_id));
  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
  EXPECT_TRUE(report->Get(*expected_audio.codec_id));
}

TEST_F(RTCStatsCollectorTest, GetStatsWithSenderSelector) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  // Expected stats graph when filtered by sender:
  //
  // track (sender)
  //          ^
  //          |
  //         outbound-rtp
  //          |        |
  //          v        v
  // codec (send)     transport
  rtc::scoped_refptr<const RTCStatsReport> sender_report =
      stats_->GetStatsReportWithSenderSelector(graph.sender);
  EXPECT_TRUE(sender_report);
  EXPECT_EQ(sender_report->timestamp_us(), graph.full_report->timestamp_us());
  EXPECT_EQ(sender_report->size(), 4u);
  EXPECT_TRUE(sender_report->Get(graph.send_codec_id));
  EXPECT_FALSE(sender_report->Get(graph.recv_codec_id));
  EXPECT_TRUE(sender_report->Get(graph.outbound_rtp_id));
  EXPECT_FALSE(sender_report->Get(graph.inbound_rtp_id));
  EXPECT_TRUE(sender_report->Get(graph.transport_id));
  EXPECT_TRUE(sender_report->Get(graph.sender_track_id));
  EXPECT_FALSE(sender_report->Get(graph.receiver_track_id));
  EXPECT_FALSE(sender_report->Get(graph.remote_stream_id));
  EXPECT_FALSE(sender_report->Get(graph.peer_connection_id));
}

TEST_F(RTCStatsCollectorTest, GetStatsWithReceiverSelector) {
  ExampleStatsGraph graph = SetupExampleStatsGraphForSelectorTests();
  // Expected stats graph when filtered by receiver:
  //
  //                                                 track (receiver)
  //                                                   ^
  //                                                   |
  //                        inbound-rtp ---------------+
  //                         |       |
  //                         v       v
  //                  transport     codec (recv)
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
      CreateMockSender(track, 0, 49, {});
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
      CreateMockSender(track, 4711, 49, {});
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

WEBRTC_RTCSTATS_IMPL(RTCTestStats, RTCStats, "test-stats", &dummy_stat);

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

  void ProducePartialResultsOnSignalingThread(int64_t timestamp_us) override {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_signaling_thread_;
    }

    rtc::scoped_refptr<RTCStatsReport> signaling_report =
        RTCStatsReport::Create(0);
    signaling_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("SignalingThreadStats", timestamp_us)));
    AddPartialResults(signaling_report);
  }
  void ProducePartialResultsOnNetworkThread(int64_t timestamp_us) override {
    EXPECT_TRUE(network_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_network_thread_;
    }

    rtc::scoped_refptr<RTCStatsReport> network_report =
        RTCStatsReport::Create(0);
    network_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("NetworkThreadStats", timestamp_us)));
    AddPartialResults(network_report);
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
