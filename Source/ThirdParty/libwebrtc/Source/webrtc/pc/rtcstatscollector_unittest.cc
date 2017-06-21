/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/rtcstatscollector.h"

#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "webrtc/api/jsepsessiondescription.h"
#include "webrtc/api/rtpparameters.h"
#include "webrtc/api/stats/rtcstats_objects.h"
#include "webrtc/api/stats/rtcstatsreport.h"
#include "webrtc/api/test/mock_rtpreceiver.h"
#include "webrtc/api/test/mock_rtpsender.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timedelta.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/media/base/test/mock_mediachannel.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/pc/mediastream.h"
#include "webrtc/pc/mediastreamtrack.h"
#include "webrtc/pc/test/mock_datachannel.h"
#include "webrtc/pc/test/mock_peerconnection.h"
#include "webrtc/pc/test/mock_webrtcsession.h"
#include "webrtc/pc/test/rtcstatsobtainer.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::ReturnNull;
using testing::ReturnRef;
using testing::SetArgPointee;

namespace webrtc {

// These are used by gtest code, such as if |EXPECT_EQ| fails.
void PrintTo(const RTCCertificateStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCCodecStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCDataChannelStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCIceCandidatePairStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCLocalIceCandidateStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCRemoteIceCandidateStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCPeerConnectionStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCMediaStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCMediaStreamTrackStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCInboundRTPStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCOutboundRTPStreamStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

void PrintTo(const RTCTransportStats& stats, ::std::ostream* os) {
  *os << stats.ToString();
}

namespace {

const int64_t kGetStatsReportTimeoutMs = 1000;
const bool kDefaultRtcpMuxRequired = true;
const bool kDefaultSrtpRequired = true;

struct CertificateInfo {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  std::vector<std::string> ders;
  std::vector<std::string> pems;
  std::vector<std::string> fingerprints;
};

std::unique_ptr<CertificateInfo> CreateFakeCertificateAndInfoFromDers(
    const std::vector<std::string>& ders) {
  RTC_CHECK(!ders.empty());
  std::unique_ptr<CertificateInfo> info(new CertificateInfo());
  info->ders = ders;
  for (const std::string& der : ders) {
    info->pems.push_back(rtc::SSLIdentity::DerToPem(
        "CERTIFICATE",
        reinterpret_cast<const unsigned char*>(der.c_str()),
        der.length()));
  }
  info->certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::FakeSSLIdentity>(
          new rtc::FakeSSLIdentity(rtc::FakeSSLCertificate(info->pems))));
  // Strip header/footer and newline characters of PEM strings.
  for (size_t i = 0; i < info->pems.size(); ++i) {
    rtc::replace_substrs("-----BEGIN CERTIFICATE-----", 27,
                         "", 0, &info->pems[i]);
    rtc::replace_substrs("-----END CERTIFICATE-----", 25,
                         "", 0, &info->pems[i]);
    rtc::replace_substrs("\n", 1,
                         "", 0, &info->pems[i]);
  }
  // Fingerprint of leaf certificate.
  std::unique_ptr<rtc::SSLFingerprint> fp(
      rtc::SSLFingerprint::Create("sha-1",
                                  &info->certificate->ssl_certificate()));
  EXPECT_TRUE(fp);
  info->fingerprints.push_back(fp->GetRfc4572Fingerprint());
  // Fingerprints of the rest of the chain.
  std::unique_ptr<rtc::SSLCertChain> chain =
      info->certificate->ssl_certificate().GetChain();
  if (chain) {
    for (size_t i = 0; i < chain->GetSize(); i++) {
      fp.reset(rtc::SSLFingerprint::Create("sha-1", &chain->Get(i)));
      EXPECT_TRUE(fp);
      info->fingerprints.push_back(fp->GetRfc4572Fingerprint());
    }
  }
  EXPECT_EQ(info->ders.size(), info->fingerprints.size());
  return info;
}

std::unique_ptr<cricket::Candidate> CreateFakeCandidate(
    const std::string& hostname,
    int port,
    const std::string& protocol,
    const std::string& candidate_type,
    uint32_t priority) {
  std::unique_ptr<cricket::Candidate> candidate(new cricket::Candidate());
  candidate->set_address(rtc::SocketAddress(hostname, port));
  candidate->set_protocol(protocol);
  candidate->set_type(candidate_type);
  candidate->set_priority(priority);
  return candidate;
}

class FakeAudioTrackForStats
    : public MediaStreamTrack<AudioTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeAudioTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state) {
    rtc::scoped_refptr<FakeAudioTrackForStats> audio_track_stats(
        new rtc::RefCountedObject<FakeAudioTrackForStats>(id));
    audio_track_stats->set_state(state);
    return audio_track_stats;
  }

  FakeAudioTrackForStats(const std::string& id)
      : MediaStreamTrack<AudioTrackInterface>(id) {
  }

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

class FakeVideoTrackForStats
    : public MediaStreamTrack<VideoTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeVideoTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state) {
    rtc::scoped_refptr<FakeVideoTrackForStats> video_track(
        new rtc::RefCountedObject<FakeVideoTrackForStats>(id));
    video_track->set_state(state);
    return video_track;
  }

  FakeVideoTrackForStats(const std::string& id)
      : MediaStreamTrack<VideoTrackInterface>(id) {
  }

  std::string kind() const override {
    return MediaStreamTrackInterface::kVideoKind;
  }
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

rtc::scoped_refptr<MockRtpSender> CreateMockSender(
    rtc::scoped_refptr<MediaStreamTrackInterface> track, uint32_t ssrc) {
  rtc::scoped_refptr<MockRtpSender> sender(
      new rtc::RefCountedObject<MockRtpSender>());
  EXPECT_CALL(*sender, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*sender, ssrc()).WillRepeatedly(Return(ssrc));
  EXPECT_CALL(*sender, media_type()).WillRepeatedly(Return(
      track->kind() == MediaStreamTrackInterface::kAudioKind
          ? cricket::MEDIA_TYPE_AUDIO : cricket::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(*sender, GetParameters()).WillRepeatedly(Invoke(
    [ssrc]() {
      RtpParameters params;
      params.encodings.push_back(RtpEncodingParameters());
      params.encodings[0].ssrc = rtc::Optional<uint32_t>(ssrc);
      return params;
    }));
  return sender;
}

rtc::scoped_refptr<MockRtpReceiver> CreateMockReceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track, uint32_t ssrc) {
  rtc::scoped_refptr<MockRtpReceiver> receiver(
      new rtc::RefCountedObject<MockRtpReceiver>());
  EXPECT_CALL(*receiver, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*receiver, media_type()).WillRepeatedly(Return(
      track->kind() == MediaStreamTrackInterface::kAudioKind
          ? cricket::MEDIA_TYPE_AUDIO : cricket::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(*receiver, GetParameters()).WillRepeatedly(Invoke(
    [ssrc]() {
      RtpParameters params;
      params.encodings.push_back(RtpEncodingParameters());
      params.encodings[0].ssrc = rtc::Optional<uint32_t>(ssrc);
      return params;
    }));
  return receiver;
}

class RTCStatsCollectorTestHelper : public SetSessionDescriptionObserver {
 public:
  RTCStatsCollectorTestHelper()
      : worker_thread_(rtc::Thread::Current()),
        network_thread_(rtc::Thread::Current()),
        signaling_thread_(rtc::Thread::Current()),
        media_engine_(new cricket::FakeMediaEngine()),
        channel_manager_(new cricket::ChannelManager(
            std::unique_ptr<cricket::MediaEngineInterface>(media_engine_),
            worker_thread_,
            network_thread_)),
        session_(channel_manager_.get(), cricket::MediaConfig()),
        pc_() {
    // Default return values for mocks.
    EXPECT_CALL(pc_, local_streams()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(pc_, remote_streams()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(pc_, session()).WillRepeatedly(Return(&session_));
    EXPECT_CALL(pc_, GetSenders()).WillRepeatedly(Return(
        std::vector<rtc::scoped_refptr<RtpSenderInterface>>()));
    EXPECT_CALL(pc_, GetReceivers()).WillRepeatedly(Return(
        std::vector<rtc::scoped_refptr<RtpReceiverInterface>>()));
    EXPECT_CALL(pc_, sctp_data_channels()).WillRepeatedly(
        ReturnRef(data_channels_));
    EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, GetStats(_)).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, GetLocalCertificate(_, _)).WillRepeatedly(
        Return(false));
    EXPECT_CALL(session_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
        .WillRepeatedly(Return(nullptr));
  }

  rtc::ScopedFakeClock& fake_clock() { return fake_clock_; }
  rtc::Thread* worker_thread() { return worker_thread_; }
  rtc::Thread* network_thread() { return network_thread_; }
  rtc::Thread* signaling_thread() { return signaling_thread_; }
  cricket::FakeMediaEngine* media_engine() { return media_engine_; }
  MockWebRtcSession& session() { return session_; }
  MockPeerConnection& pc() { return pc_; }
  std::vector<rtc::scoped_refptr<DataChannel>>& data_channels() {
    return data_channels_;
  }

  // SetSessionDescriptionObserver overrides.
  void OnSuccess() override {}
  void OnFailure(const std::string& error) override {
    RTC_NOTREACHED() << error;
  }

  void SetupLocalTrackAndSender(cricket::MediaType media_type,
                                const std::string& track_id,
                                uint32_t ssrc) {
    rtc::scoped_refptr<StreamCollection> local_streams =
        StreamCollection::Create();
    EXPECT_CALL(pc_, local_streams())
        .WillRepeatedly(Return(local_streams));

    rtc::scoped_refptr<MediaStream> local_stream =
        MediaStream::Create("LocalStreamLabel");
    local_streams->AddStream(local_stream);

    rtc::scoped_refptr<MediaStreamTrackInterface> track;
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      track = CreateFakeTrack(media_type, track_id,
                              MediaStreamTrackInterface::kLive);
      local_stream->AddTrack(static_cast<AudioTrackInterface*>(track.get()));
    } else {
      track = CreateFakeTrack(media_type, track_id,
                              MediaStreamTrackInterface::kLive);
      local_stream->AddTrack(static_cast<VideoTrackInterface*>(track.get()));
    }

    rtc::scoped_refptr<MockRtpSender> sender = CreateMockSender(track, ssrc);
    EXPECT_CALL(pc_, GetSenders()).WillRepeatedly(Return(
        std::vector<rtc::scoped_refptr<RtpSenderInterface>>({
            rtc::scoped_refptr<RtpSenderInterface>(sender.get()) })));
  }

  void SetupRemoteTrackAndReceiver(cricket::MediaType media_type,
                                   const std::string& track_id,
                                   uint32_t ssrc) {
    rtc::scoped_refptr<StreamCollection> remote_streams =
        StreamCollection::Create();
    EXPECT_CALL(pc_, remote_streams())
        .WillRepeatedly(Return(remote_streams));

    rtc::scoped_refptr<MediaStream> remote_stream =
        MediaStream::Create("RemoteStreamLabel");
    remote_streams->AddStream(remote_stream);

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

    rtc::scoped_refptr<MockRtpReceiver> receiver =
        CreateMockReceiver(track, ssrc);
    EXPECT_CALL(pc_, GetReceivers()).WillRepeatedly(Return(
        std::vector<rtc::scoped_refptr<RtpReceiverInterface>>({
            rtc::scoped_refptr<RtpReceiverInterface>(receiver.get()) })));
  }

  // Attaches tracks to peer connections by configuring RTP senders and RTP
  // receivers according to the tracks' pairings with
  // |[Voice/Video][Sender/Receiver]Info| and their SSRCs. Local tracks can be
  // associated with multiple |[Voice/Video]SenderInfo|s, remote tracks can only
  // be associated with one |[Voice/Video]ReceiverInfo|.
  void CreateMockRtpSendersReceiversAndChannels(
      std::initializer_list<std::pair<MediaStreamTrackInterface*,
          cricket::VoiceSenderInfo>> local_audio_track_info_pairs,
      std::initializer_list<std::pair<MediaStreamTrackInterface*,
          cricket::VoiceReceiverInfo>> remote_audio_track_info_pairs,
      std::initializer_list<std::pair<MediaStreamTrackInterface*,
          cricket::VideoSenderInfo>> local_video_track_info_pairs,
      std::initializer_list<std::pair<MediaStreamTrackInterface*,
          cricket::VideoReceiverInfo>> remote_video_track_info_pairs) {
    voice_media_info_.reset(new cricket::VoiceMediaInfo());
    video_media_info_.reset(new cricket::VideoMediaInfo());
    rtp_senders_.clear();
    rtp_receivers_.clear();
    // Local audio tracks and voice sender infos
    for (auto& pair : local_audio_track_info_pairs) {
      MediaStreamTrackInterface* local_audio_track = pair.first;
      const cricket::VoiceSenderInfo& voice_sender_info = pair.second;
      RTC_DCHECK_EQ(local_audio_track->kind(),
                    MediaStreamTrackInterface::kAudioKind);

      voice_media_info_->senders.push_back(voice_sender_info);
      rtc::scoped_refptr<MockRtpSender> rtp_sender =
          CreateMockSender(rtc::scoped_refptr<MediaStreamTrackInterface>(
                               local_audio_track),
                           voice_sender_info.local_stats[0].ssrc);
      rtp_senders_.push_back(rtc::scoped_refptr<RtpSenderInterface>(
          rtp_sender.get()));
    }
    // Remote audio tracks and voice receiver infos
    for (auto& pair : remote_audio_track_info_pairs) {
      MediaStreamTrackInterface* remote_audio_track = pair.first;
      const cricket::VoiceReceiverInfo& voice_receiver_info = pair.second;
      RTC_DCHECK_EQ(remote_audio_track->kind(),
                    MediaStreamTrackInterface::kAudioKind);

      voice_media_info_->receivers.push_back(voice_receiver_info);
      rtc::scoped_refptr<MockRtpReceiver> rtp_receiver =
          CreateMockReceiver(rtc::scoped_refptr<MediaStreamTrackInterface>(
                               remote_audio_track),
                             voice_receiver_info.local_stats[0].ssrc);
      rtp_receivers_.push_back(rtc::scoped_refptr<RtpReceiverInterface>(
          rtp_receiver.get()));
    }
    // Local video tracks and video sender infos
    for (auto& pair : local_video_track_info_pairs) {
      MediaStreamTrackInterface* local_video_track = pair.first;
      const cricket::VideoSenderInfo& video_sender_info = pair.second;
      RTC_DCHECK_EQ(local_video_track->kind(),
                    MediaStreamTrackInterface::kVideoKind);

      video_media_info_->senders.push_back(video_sender_info);
      rtc::scoped_refptr<MockRtpSender> rtp_sender =
          CreateMockSender(rtc::scoped_refptr<MediaStreamTrackInterface>(
                               local_video_track),
                           video_sender_info.local_stats[0].ssrc);
      rtp_senders_.push_back(rtc::scoped_refptr<RtpSenderInterface>(
          rtp_sender.get()));
    }
    // Remote video tracks and video receiver infos
    for (auto& pair : remote_video_track_info_pairs) {
      MediaStreamTrackInterface* remote_video_track = pair.first;
      const cricket::VideoReceiverInfo& video_receiver_info = pair.second;
      RTC_DCHECK_EQ(remote_video_track->kind(),
                    MediaStreamTrackInterface::kVideoKind);

      video_media_info_->receivers.push_back(video_receiver_info);
      rtc::scoped_refptr<MockRtpReceiver> rtp_receiver =
          CreateMockReceiver(rtc::scoped_refptr<MediaStreamTrackInterface>(
                               remote_video_track),
                             video_receiver_info.local_stats[0].ssrc);
      rtp_receivers_.push_back(rtc::scoped_refptr<RtpReceiverInterface>(
          rtp_receiver.get()));
    }
    EXPECT_CALL(pc_, GetSenders()).WillRepeatedly(Return(rtp_senders_));
    EXPECT_CALL(pc_, GetReceivers()).WillRepeatedly(Return(rtp_receivers_));

    MockVoiceMediaChannel* voice_media_channel = new MockVoiceMediaChannel();
    voice_channel_.reset(new cricket::VoiceChannel(
        worker_thread_, network_thread_, nullptr, media_engine_,
        voice_media_channel, "VoiceContentName", kDefaultRtcpMuxRequired,
        kDefaultSrtpRequired));
    EXPECT_CALL(session_, voice_channel())
        .WillRepeatedly(Return(voice_channel_.get()));
    EXPECT_CALL(*voice_media_channel, GetStats(_))
        .WillOnce(DoAll(SetArgPointee<0>(*voice_media_info_), Return(true)));

    MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
    video_channel_.reset(new cricket::VideoChannel(
        worker_thread_, network_thread_, nullptr, video_media_channel,
        "VideoContentName", kDefaultRtcpMuxRequired, kDefaultSrtpRequired));
    EXPECT_CALL(session_, video_channel())
        .WillRepeatedly(Return(video_channel_.get()));
    EXPECT_CALL(*video_media_channel, GetStats(_))
        .WillOnce(DoAll(SetArgPointee<0>(*video_media_info_), Return(true)));
  }

 private:
  rtc::ScopedFakeClock fake_clock_;
  RtcEventLogNullImpl event_log_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  rtc::Thread* const signaling_thread_;
  // |media_engine_| is actually owned by |channel_manager_|.
  cricket::FakeMediaEngine* media_engine_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  MockWebRtcSession session_;
  MockPeerConnection pc_;

  std::vector<rtc::scoped_refptr<DataChannel>> data_channels_;
  std::unique_ptr<cricket::VoiceChannel> voice_channel_;
  std::unique_ptr<cricket::VideoChannel> video_channel_;
  std::unique_ptr<cricket::VoiceMediaInfo> voice_media_info_;
  std::unique_ptr<cricket::VideoMediaInfo> video_media_info_;
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> rtp_senders_;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> rtp_receivers_;
};

class RTCTestStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTestStats(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        dummy_stat("dummyStat") {}

  RTCStatsMember<int32_t> dummy_stat;
};

WEBRTC_RTCSTATS_IMPL(RTCTestStats, RTCStats, "test-stats",
    &dummy_stat);

// Overrides the stats collection to verify thread usage and that the resulting
// partial reports are merged.
class FakeRTCStatsCollector : public RTCStatsCollector,
                              public RTCStatsCollectorCallback {
 public:
  static rtc::scoped_refptr<FakeRTCStatsCollector> Create(
      PeerConnection* pc,
      int64_t cache_lifetime_us) {
    return rtc::scoped_refptr<FakeRTCStatsCollector>(
        new rtc::RefCountedObject<FakeRTCStatsCollector>(
            pc, cache_lifetime_us));
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
  FakeRTCStatsCollector(
      PeerConnection* pc,
      int64_t cache_lifetime)
      : RTCStatsCollector(pc, cache_lifetime),
        signaling_thread_(pc->session()->signaling_thread()),
        worker_thread_(pc->session()->worker_thread()),
        network_thread_(pc->session()->network_thread()) {
  }

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

class RTCStatsCollectorTest : public testing::Test {
 public:
  RTCStatsCollectorTest()
    : test_(new rtc::RefCountedObject<RTCStatsCollectorTestHelper>()),
      collector_(RTCStatsCollector::Create(
          &test_->pc(), 50 * rtc::kNumMicrosecsPerMillisec)) {
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsReport() {
    rtc::scoped_refptr<RTCStatsObtainer> callback = RTCStatsObtainer::Create();
    collector_->GetStatsReport(callback);
    EXPECT_TRUE_WAIT(callback->report(), kGetStatsReportTimeoutMs);
    int64_t after = rtc::TimeUTCMicros();
    for (const RTCStats& stats : *callback->report()) {
      EXPECT_LE(stats.timestamp_us(), after);
    }
    return callback->report();
  }

  void ExpectReportContainsCertificateInfo(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const CertificateInfo& certinfo) {
    for (size_t i = 0; i < certinfo.fingerprints.size(); ++i) {
      RTCCertificateStats expected_certificate_stats(
          "RTCCertificate_" + certinfo.fingerprints[i],
          report->timestamp_us());
      expected_certificate_stats.fingerprint = certinfo.fingerprints[i];
      expected_certificate_stats.fingerprint_algorithm = "sha-1";
      expected_certificate_stats.base64_certificate = certinfo.pems[i];
      if (i + 1 < certinfo.fingerprints.size()) {
        expected_certificate_stats.issuer_certificate_id =
            "RTCCertificate_" + certinfo.fingerprints[i + 1];
      }
      ASSERT_TRUE(report->Get(expected_certificate_stats.id()));
      EXPECT_EQ(expected_certificate_stats,
                report->Get(expected_certificate_stats.id())->cast_to<
                      RTCCertificateStats>());
    }
  }

 protected:
  rtc::scoped_refptr<RTCStatsCollectorTestHelper> test_;
  rtc::scoped_refptr<RTCStatsCollector> collector_;
};

TEST_F(RTCStatsCollectorTest, SingleCallback) {
  rtc::scoped_refptr<const RTCStatsReport> result;
  collector_->GetStatsReport(RTCStatsObtainer::Create(&result));
  EXPECT_TRUE_WAIT(result, kGetStatsReportTimeoutMs);
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacks) {
  rtc::scoped_refptr<const RTCStatsReport> a;
  rtc::scoped_refptr<const RTCStatsReport> b;
  rtc::scoped_refptr<const RTCStatsReport> c;
  collector_->GetStatsReport(RTCStatsObtainer::Create(&a));
  collector_->GetStatsReport(RTCStatsObtainer::Create(&b));
  collector_->GetStatsReport(RTCStatsObtainer::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);
  EXPECT_EQ(a.get(), b.get());
  EXPECT_EQ(b.get(), c.get());
}

TEST_F(RTCStatsCollectorTest, CachedStatsReports) {
  // Caching should ensure |a| and |b| are the same report.
  rtc::scoped_refptr<const RTCStatsReport> a = GetStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> b = GetStatsReport();
  EXPECT_EQ(a.get(), b.get());
  // Invalidate cache by clearing it.
  collector_->ClearCachedStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> c = GetStatsReport();
  EXPECT_NE(b.get(), c.get());
  // Invalidate cache by advancing time.
  test_->fake_clock().AdvanceTime(rtc::TimeDelta::FromMilliseconds(51));
  rtc::scoped_refptr<const RTCStatsReport> d = GetStatsReport();
  EXPECT_TRUE(d);
  EXPECT_NE(c.get(), d.get());
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacksWithInvalidatedCacheInBetween) {
  rtc::scoped_refptr<const RTCStatsReport> a;
  rtc::scoped_refptr<const RTCStatsReport> b;
  rtc::scoped_refptr<const RTCStatsReport> c;
  collector_->GetStatsReport(RTCStatsObtainer::Create(&a));
  collector_->GetStatsReport(RTCStatsObtainer::Create(&b));
  // Cache is invalidated after 50 ms.
  test_->fake_clock().AdvanceTime(rtc::TimeDelta::FromMilliseconds(51));
  collector_->GetStatsReport(RTCStatsObtainer::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);
  EXPECT_EQ(a.get(), b.get());
  // The act of doing |AdvanceTime| processes all messages. If this was not the
  // case we might not require |c| to be fresher than |b|.
  EXPECT_NE(c.get(), b.get());
}

TEST_F(RTCStatsCollectorTest, CollectRTCCertificateStatsSingle) {
  std::unique_ptr<CertificateInfo> local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(local) single certificate" }));
  std::unique_ptr<CertificateInfo> remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(remote) single certificate" }));

  // Mock the session to return the local and remote certificates.
  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [this](const ChannelNamePairs&) {
        std::unique_ptr<SessionStats> stats(new SessionStats());
        stats->transport_stats["transport"].transport_name = "transport";
        return stats;
      }));
  EXPECT_CALL(test_->session(), GetLocalCertificate(_, _)).WillRepeatedly(
      Invoke([this, &local_certinfo](const std::string& transport_name,
             rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
        if (transport_name == "transport") {
          *certificate = local_certinfo->certificate;
          return true;
        }
        return false;
      }));
  EXPECT_CALL(test_->session(),
      GetRemoteSSLCertificate_ReturnsRawPointer(_)).WillRepeatedly(Invoke(
      [this, &remote_certinfo](const std::string& transport_name) {
        if (transport_name == "transport")
          return remote_certinfo->certificate->ssl_certificate().GetReference();
        return static_cast<rtc::SSLCertificate*>(nullptr);
      }));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsCertificateInfo(report, *local_certinfo);
  ExpectReportContainsCertificateInfo(report, *remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectRTCCodecStats) {
  MockVoiceMediaChannel* voice_media_channel = new MockVoiceMediaChannel();
  cricket::VoiceChannel voice_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), test_->media_engine(), voice_media_channel,
      "VoiceContentName", kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), video_media_channel, "VideoContentName",
      kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  // Audio
  cricket::VoiceMediaInfo voice_media_info;

  RtpCodecParameters inbound_audio_codec;
  inbound_audio_codec.payload_type = 1;
  inbound_audio_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  inbound_audio_codec.name = "opus";
  inbound_audio_codec.clock_rate = rtc::Optional<int>(1337);
  voice_media_info.receive_codecs.insert(
      std::make_pair(inbound_audio_codec.payload_type, inbound_audio_codec));

  RtpCodecParameters outbound_audio_codec;
  outbound_audio_codec.payload_type = 2;
  outbound_audio_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  outbound_audio_codec.name = "isac";
  outbound_audio_codec.clock_rate = rtc::Optional<int>(1338);
  voice_media_info.send_codecs.insert(
      std::make_pair(outbound_audio_codec.payload_type, outbound_audio_codec));

  EXPECT_CALL(*voice_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(voice_media_info), Return(true)));

  // Video
  cricket::VideoMediaInfo video_media_info;

  RtpCodecParameters inbound_video_codec;
  inbound_video_codec.payload_type = 3;
  inbound_video_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  inbound_video_codec.name = "H264";
  inbound_video_codec.clock_rate = rtc::Optional<int>(1339);
  video_media_info.receive_codecs.insert(
      std::make_pair(inbound_video_codec.payload_type, inbound_video_codec));

  RtpCodecParameters outbound_video_codec;
  outbound_video_codec.payload_type = 4;
  outbound_video_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  outbound_video_codec.name = "VP8";
  outbound_video_codec.clock_rate = rtc::Optional<int>(1340);
  video_media_info.send_codecs.insert(
      std::make_pair(outbound_video_codec.payload_type, outbound_video_codec));

  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));

  SessionStats session_stats;
  session_stats.proxy_to_transport["VoiceContentName"] = "TransportName";
  session_stats.proxy_to_transport["VideoContentName"] = "TransportName";
  session_stats.transport_stats["TransportName"].transport_name =
      "TransportName";

  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));
  EXPECT_CALL(test_->session(), voice_channel())
      .WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(test_->session(), video_channel())
      .WillRepeatedly(Return(&video_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCCodecStats expected_inbound_audio_codec(
      "RTCCodec_InboundAudio_1", report->timestamp_us());
  expected_inbound_audio_codec.payload_type = 1;
  expected_inbound_audio_codec.mime_type = "audio/opus";
  expected_inbound_audio_codec.clock_rate = 1337;

  RTCCodecStats expected_outbound_audio_codec(
      "RTCCodec_OutboundAudio_2", report->timestamp_us());
  expected_outbound_audio_codec.payload_type = 2;
  expected_outbound_audio_codec.mime_type = "audio/isac";
  expected_outbound_audio_codec.clock_rate = 1338;

  RTCCodecStats expected_inbound_video_codec(
      "RTCCodec_InboundVideo_3", report->timestamp_us());
  expected_inbound_video_codec.payload_type = 3;
  expected_inbound_video_codec.mime_type = "video/H264";
  expected_inbound_video_codec.clock_rate = 1339;

  RTCCodecStats expected_outbound_video_codec(
      "RTCCodec_OutboundVideo_4", report->timestamp_us());
  expected_outbound_video_codec.payload_type = 4;
  expected_outbound_video_codec.mime_type = "video/VP8";
  expected_outbound_video_codec.clock_rate = 1340;

  ASSERT_TRUE(report->Get(expected_inbound_audio_codec.id()));
  EXPECT_EQ(expected_inbound_audio_codec,
            report->Get(expected_inbound_audio_codec.id())->cast_to<
                  RTCCodecStats>());

  ASSERT_TRUE(report->Get(expected_outbound_audio_codec.id()));
  EXPECT_EQ(expected_outbound_audio_codec,
            report->Get(expected_outbound_audio_codec.id())->cast_to<
                  RTCCodecStats>());

  ASSERT_TRUE(report->Get(expected_inbound_video_codec.id()));
  EXPECT_EQ(expected_inbound_video_codec,
            report->Get(expected_inbound_video_codec.id())->cast_to<
                  RTCCodecStats>());

  ASSERT_TRUE(report->Get(expected_outbound_video_codec.id()));
  EXPECT_EQ(expected_outbound_video_codec,
            report->Get(expected_outbound_video_codec.id())->cast_to<
                  RTCCodecStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCCertificateStatsMultiple) {
  std::unique_ptr<CertificateInfo> audio_local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(local) audio" }));
  audio_local_certinfo = CreateFakeCertificateAndInfoFromDers(
      audio_local_certinfo->ders);
  std::unique_ptr<CertificateInfo> audio_remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(remote) audio" }));
  audio_remote_certinfo = CreateFakeCertificateAndInfoFromDers(
      audio_remote_certinfo->ders);

  std::unique_ptr<CertificateInfo> video_local_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(local) video" }));
  video_local_certinfo = CreateFakeCertificateAndInfoFromDers(
      video_local_certinfo->ders);
  std::unique_ptr<CertificateInfo> video_remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(remote) video" }));
  video_remote_certinfo = CreateFakeCertificateAndInfoFromDers(
      video_remote_certinfo->ders);

  // Mock the session to return the local and remote certificates.
  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [this](const ChannelNamePairs&) {
        std::unique_ptr<SessionStats> stats(new SessionStats());
        stats->transport_stats["audio"].transport_name = "audio";
        stats->transport_stats["video"].transport_name = "video";
        return stats;
      }));
  EXPECT_CALL(test_->session(), GetLocalCertificate(_, _)).WillRepeatedly(
      Invoke([this, &audio_local_certinfo, &video_local_certinfo](
            const std::string& transport_name,
            rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
        if (transport_name == "audio") {
          *certificate = audio_local_certinfo->certificate;
          return true;
        }
        if (transport_name == "video") {
          *certificate = video_local_certinfo->certificate;
          return true;
        }
        return false;
      }));
  EXPECT_CALL(test_->session(),
      GetRemoteSSLCertificate_ReturnsRawPointer(_)).WillRepeatedly(Invoke(
      [this, &audio_remote_certinfo, &video_remote_certinfo](
          const std::string& transport_name) {
        if (transport_name == "audio") {
          return audio_remote_certinfo->certificate->ssl_certificate()
              .GetReference();
        }
        if (transport_name == "video") {
          return video_remote_certinfo->certificate->ssl_certificate()
              .GetReference();
        }
        return static_cast<rtc::SSLCertificate*>(nullptr);
      }));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsCertificateInfo(report, *audio_local_certinfo);
  ExpectReportContainsCertificateInfo(report, *audio_remote_certinfo);
  ExpectReportContainsCertificateInfo(report, *video_local_certinfo);
  ExpectReportContainsCertificateInfo(report, *video_remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectRTCCertificateStatsChain) {
  std::vector<std::string> local_ders;
  local_ders.push_back("(local) this");
  local_ders.push_back("(local) is");
  local_ders.push_back("(local) a");
  local_ders.push_back("(local) chain");
  std::unique_ptr<CertificateInfo> local_certinfo =
      CreateFakeCertificateAndInfoFromDers(local_ders);
  std::vector<std::string> remote_ders;
  remote_ders.push_back("(remote) this");
  remote_ders.push_back("(remote) is");
  remote_ders.push_back("(remote) another");
  remote_ders.push_back("(remote) chain");
  std::unique_ptr<CertificateInfo> remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(remote_ders);

  // Mock the session to return the local and remote certificates.
  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [this](const ChannelNamePairs&) {
        std::unique_ptr<SessionStats> stats(new SessionStats());
        stats->transport_stats["transport"].transport_name = "transport";
        return stats;
      }));
  EXPECT_CALL(test_->session(), GetLocalCertificate(_, _)).WillRepeatedly(
      Invoke([this, &local_certinfo](const std::string& transport_name,
             rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
        if (transport_name == "transport") {
          *certificate = local_certinfo->certificate;
          return true;
        }
        return false;
      }));
  EXPECT_CALL(test_->session(),
      GetRemoteSSLCertificate_ReturnsRawPointer(_)).WillRepeatedly(Invoke(
      [this, &remote_certinfo](const std::string& transport_name) {
        if (transport_name == "transport")
          return remote_certinfo->certificate->ssl_certificate().GetReference();
        return static_cast<rtc::SSLCertificate*>(nullptr);
      }));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsCertificateInfo(report, *local_certinfo);
  ExpectReportContainsCertificateInfo(report, *remote_certinfo);
}

TEST_F(RTCStatsCollectorTest, CollectRTCDataChannelStats) {
  test_->data_channels().push_back(
      new MockDataChannel(
          0, "MockDataChannel0", DataChannelInterface::kConnecting, "udp",
          1, 2, 3, 4));
  RTCDataChannelStats expected_data_channel0("RTCDataChannel_0", 0);
  expected_data_channel0.label = "MockDataChannel0";
  expected_data_channel0.protocol = "udp";
  expected_data_channel0.datachannelid = 0;
  expected_data_channel0.state = "connecting";
  expected_data_channel0.messages_sent = 1;
  expected_data_channel0.bytes_sent = 2;
  expected_data_channel0.messages_received = 3;
  expected_data_channel0.bytes_received = 4;

  test_->data_channels().push_back(
      new MockDataChannel(
          1, "MockDataChannel1", DataChannelInterface::kOpen, "tcp",
          5, 6, 7, 8));
  RTCDataChannelStats expected_data_channel1("RTCDataChannel_1", 0);
  expected_data_channel1.label = "MockDataChannel1";
  expected_data_channel1.protocol = "tcp";
  expected_data_channel1.datachannelid = 1;
  expected_data_channel1.state = "open";
  expected_data_channel1.messages_sent = 5;
  expected_data_channel1.bytes_sent = 6;
  expected_data_channel1.messages_received = 7;
  expected_data_channel1.bytes_received = 8;

  test_->data_channels().push_back(
      new MockDataChannel(
          2, "MockDataChannel2", DataChannelInterface::kClosing, "udp",
          9, 10, 11, 12));
  RTCDataChannelStats expected_data_channel2("RTCDataChannel_2", 0);
  expected_data_channel2.label = "MockDataChannel2";
  expected_data_channel2.protocol = "udp";
  expected_data_channel2.datachannelid = 2;
  expected_data_channel2.state = "closing";
  expected_data_channel2.messages_sent = 9;
  expected_data_channel2.bytes_sent = 10;
  expected_data_channel2.messages_received = 11;
  expected_data_channel2.bytes_received = 12;

  test_->data_channels().push_back(
      new MockDataChannel(
          3, "MockDataChannel3", DataChannelInterface::kClosed, "tcp",
          13, 14, 15, 16));
  RTCDataChannelStats expected_data_channel3("RTCDataChannel_3", 0);
  expected_data_channel3.label = "MockDataChannel3";
  expected_data_channel3.protocol = "tcp";
  expected_data_channel3.datachannelid = 3;
  expected_data_channel3.state = "closed";
  expected_data_channel3.messages_sent = 13;
  expected_data_channel3.bytes_sent = 14;
  expected_data_channel3.messages_received = 15;
  expected_data_channel3.bytes_received = 16;

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ASSERT_TRUE(report->Get(expected_data_channel0.id()));
  EXPECT_EQ(expected_data_channel0,
            report->Get(expected_data_channel0.id())->cast_to<
                RTCDataChannelStats>());
  ASSERT_TRUE(report->Get(expected_data_channel1.id()));
  EXPECT_EQ(expected_data_channel1,
            report->Get(expected_data_channel1.id())->cast_to<
                RTCDataChannelStats>());
  ASSERT_TRUE(report->Get(expected_data_channel2.id()));
  EXPECT_EQ(expected_data_channel2,
            report->Get(expected_data_channel2.id())->cast_to<
                RTCDataChannelStats>());
  ASSERT_TRUE(report->Get(expected_data_channel3.id()));
  EXPECT_EQ(expected_data_channel3,
            report->Get(expected_data_channel3.id())->cast_to<
                RTCDataChannelStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCIceCandidateStats) {
  // Candidates in the first transport stats.
  std::unique_ptr<cricket::Candidate> a_local_host = CreateFakeCandidate(
      "1.2.3.4", 5,
      "a_local_host's protocol",
      cricket::LOCAL_PORT_TYPE,
      0);
  RTCLocalIceCandidateStats expected_a_local_host(
      "RTCIceCandidate_" + a_local_host->id(), 0);
  expected_a_local_host.transport_id = "RTCTransport_a_0";
  expected_a_local_host.ip = "1.2.3.4";
  expected_a_local_host.port = 5;
  expected_a_local_host.protocol = "a_local_host's protocol";
  expected_a_local_host.candidate_type = "host";
  expected_a_local_host.priority = 0;
  EXPECT_FALSE(*expected_a_local_host.is_remote);

  std::unique_ptr<cricket::Candidate> a_remote_srflx = CreateFakeCandidate(
      "6.7.8.9", 10,
      "remote_srflx's protocol",
      cricket::STUN_PORT_TYPE,
      1);
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
      "11.12.13.14", 15,
      "a_local_prflx's protocol",
      cricket::PRFLX_PORT_TYPE,
      2);
  RTCLocalIceCandidateStats expected_a_local_prflx(
      "RTCIceCandidate_" + a_local_prflx->id(), 0);
  expected_a_local_prflx.transport_id = "RTCTransport_a_0";
  expected_a_local_prflx.ip = "11.12.13.14";
  expected_a_local_prflx.port = 15;
  expected_a_local_prflx.protocol = "a_local_prflx's protocol";
  expected_a_local_prflx.candidate_type = "prflx";
  expected_a_local_prflx.priority = 2;
  expected_a_local_prflx.deleted = false;
  EXPECT_FALSE(*expected_a_local_prflx.is_remote);

  std::unique_ptr<cricket::Candidate> a_remote_relay = CreateFakeCandidate(
      "16.17.18.19", 20,
      "a_remote_relay's protocol",
      cricket::RELAY_PORT_TYPE,
      3);
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

  // Candidates in the second transport stats.
  std::unique_ptr<cricket::Candidate> b_local = CreateFakeCandidate(
      "42.42.42.42", 42,
      "b_local's protocol",
      cricket::LOCAL_PORT_TYPE,
      42);
  RTCLocalIceCandidateStats expected_b_local(
      "RTCIceCandidate_" + b_local->id(), 0);
  expected_b_local.transport_id = "RTCTransport_b_0";
  expected_b_local.ip = "42.42.42.42";
  expected_b_local.port = 42;
  expected_b_local.protocol = "b_local's protocol";
  expected_b_local.candidate_type = "host";
  expected_b_local.priority = 42;
  expected_b_local.deleted = false;
  EXPECT_FALSE(*expected_b_local.is_remote);

  std::unique_ptr<cricket::Candidate> b_remote = CreateFakeCandidate(
      "42.42.42.42", 42,
      "b_remote's protocol",
      cricket::LOCAL_PORT_TYPE,
      42);
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

  SessionStats session_stats;

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
  session_stats.transport_stats["a"].transport_name = "a";
  session_stats.transport_stats["a"].channel_stats.push_back(
      a_transport_channel_stats);

  cricket::TransportChannelStats b_transport_channel_stats;
  b_transport_channel_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  b_transport_channel_stats.connection_infos[0].local_candidate =
      *b_local.get();
  b_transport_channel_stats.connection_infos[0].remote_candidate =
      *b_remote.get();
  session_stats.transport_stats["b"].transport_name = "b";
  session_stats.transport_stats["b"].channel_stats.push_back(
      b_transport_channel_stats);

  // Mock the session to return the desired candidates.
  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  ASSERT_TRUE(report->Get(expected_a_local_host.id()));
  EXPECT_EQ(expected_a_local_host,
            report->Get(expected_a_local_host.id())->cast_to<
                RTCLocalIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_a_remote_srflx.id()));
  EXPECT_EQ(expected_a_remote_srflx,
            report->Get(expected_a_remote_srflx.id())->cast_to<
                RTCRemoteIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_a_local_prflx.id()));
  EXPECT_EQ(expected_a_local_prflx,
            report->Get(expected_a_local_prflx.id())->cast_to<
                RTCLocalIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_a_remote_relay.id()));
  EXPECT_EQ(expected_a_remote_relay,
            report->Get(expected_a_remote_relay.id())->cast_to<
                RTCRemoteIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_b_local.id()));
  EXPECT_EQ(expected_b_local,
            report->Get(expected_b_local.id())->cast_to<
                RTCLocalIceCandidateStats>());
  ASSERT_TRUE(report->Get(expected_b_remote.id()));
  EXPECT_EQ(expected_b_remote,
            report->Get(expected_b_remote.id())->cast_to<
                RTCRemoteIceCandidateStats>());
  EXPECT_TRUE(report->Get("RTCTransport_a_0"));
  EXPECT_TRUE(report->Get("RTCTransport_b_0"));
}

TEST_F(RTCStatsCollectorTest, CollectRTCIceCandidatePairStats) {
  MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), video_media_channel, "VideoContentName",
      kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  std::unique_ptr<cricket::Candidate> local_candidate = CreateFakeCandidate(
      "42.42.42.42", 42, "protocol", cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> remote_candidate = CreateFakeCandidate(
      "42.42.42.42", 42, "protocol", cricket::LOCAL_PORT_TYPE, 42);

  SessionStats session_stats;

  cricket::ConnectionInfo connection_info;
  connection_info.best_connection = false;
  connection_info.local_candidate = *local_candidate.get();
  connection_info.remote_candidate = *remote_candidate.get();
  connection_info.writable = true;
  connection_info.sent_total_bytes = 42;
  connection_info.recv_total_bytes = 1234;
  connection_info.total_round_trip_time_ms = 0;
  connection_info.current_round_trip_time_ms = rtc::Optional<uint32_t>();
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
  session_stats.proxy_to_transport["VideoContentName"] = "transport";
  session_stats.transport_stats["transport"].transport_name = "transport";
  session_stats.transport_stats["transport"].channel_stats.push_back(
      transport_channel_stats);

  // Mock the session to return the desired candidates.
  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));

  // Mock the session to return bandwidth estimation info. These should only
  // be used for a selected candidate pair.
  cricket::VideoMediaInfo video_media_info;
  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  EXPECT_CALL(test_->session(), video_channel())
      .WillRepeatedly(Return(&video_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCIceCandidatePairStats expected_pair("RTCIceCandidatePair_" +
                                             local_candidate->id() + "_" +
                                             remote_candidate->id(),
                                         report->timestamp_us());
  expected_pair.transport_id =
      "RTCTransport_transport_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
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
  session_stats.transport_stats["transport"]
      .channel_stats[0]
      .connection_infos[0]
      .nominated = true;
  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
  expected_pair.nominated = true;
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Set round trip times and "GetStats" again.
  session_stats.transport_stats["transport"].channel_stats[0]
      .connection_infos[0].total_round_trip_time_ms = 7331;
  session_stats.transport_stats["transport"].channel_stats[0]
      .connection_infos[0].current_round_trip_time_ms =
          rtc::Optional<uint32_t>(1337);
  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
  expected_pair.total_round_trip_time = 7.331;
  expected_pair.current_round_trip_time = 1.337;
  ASSERT_TRUE(report->Get(expected_pair.id()));
  EXPECT_EQ(
      expected_pair,
      report->Get(expected_pair.id())->cast_to<RTCIceCandidatePairStats>());
  EXPECT_TRUE(report->Get(*expected_pair.transport_id));

  // Make pair the current pair, clear bandwidth and "GetStats" again.
  session_stats.transport_stats["transport"]
      .channel_stats[0]
      .connection_infos[0]
      .best_connection = true;
  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
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
  EXPECT_CALL(test_->session(), GetCallStats())
      .WillRepeatedly(Return(call_stats));
  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
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
  expected_local_candidate.ip = "42.42.42.42";
  expected_local_candidate.port = 42;
  expected_local_candidate.protocol = "protocol";
  expected_local_candidate.candidate_type = "host";
  expected_local_candidate.priority = 42;
  expected_local_candidate.deleted = false;
  EXPECT_FALSE(*expected_local_candidate.is_remote);
  ASSERT_TRUE(report->Get(expected_local_candidate.id()));
  EXPECT_EQ(expected_local_candidate,
            report->Get(expected_local_candidate.id())->cast_to<
                RTCLocalIceCandidateStats>());

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
            report->Get(expected_remote_candidate.id())->cast_to<
                RTCRemoteIceCandidateStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCPeerConnectionStats) {
  {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 0;
    expected.data_channels_closed = 0;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(expected,
              report->Get("RTCPeerConnection")->cast_to<
                  RTCPeerConnectionStats>());
  }

  rtc::scoped_refptr<DataChannel> dummy_channel_a = DataChannel::Create(
      nullptr, cricket::DCT_NONE, "DummyChannelA", InternalDataChannelInit());
  test_->pc().SignalDataChannelCreated(dummy_channel_a.get());
  rtc::scoped_refptr<DataChannel> dummy_channel_b = DataChannel::Create(
      nullptr, cricket::DCT_NONE, "DummyChannelB", InternalDataChannelInit());
  test_->pc().SignalDataChannelCreated(dummy_channel_b.get());

  dummy_channel_a->SignalOpened(dummy_channel_a.get());
  // Closing a channel that is not opened should not affect the counts.
  dummy_channel_b->SignalClosed(dummy_channel_b.get());

  {
    collector_->ClearCachedStatsReport();
    rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 1;
    expected.data_channels_closed = 0;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(expected,
              report->Get("RTCPeerConnection")->cast_to<
                  RTCPeerConnectionStats>());
  }

  dummy_channel_b->SignalOpened(dummy_channel_b.get());
  dummy_channel_b->SignalClosed(dummy_channel_b.get());

  {
    collector_->ClearCachedStatsReport();
    rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 2;
    expected.data_channels_closed = 1;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(expected,
              report->Get("RTCPeerConnection")->cast_to<
                  RTCPeerConnectionStats>());
  }

  // Re-opening a data channel (or opening a new data channel that is re-using
  // the same address in memory) should increase the opened count.
  dummy_channel_b->SignalOpened(dummy_channel_b.get());

  {
    collector_->ClearCachedStatsReport();
    rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 3;
    expected.data_channels_closed = 1;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(expected,
              report->Get("RTCPeerConnection")->cast_to<
                  RTCPeerConnectionStats>());
  }

  dummy_channel_a->SignalClosed(dummy_channel_a.get());
  dummy_channel_b->SignalClosed(dummy_channel_b.get());

  {
    collector_->ClearCachedStatsReport();
    rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 3;
    expected.data_channels_closed = 3;
    ASSERT_TRUE(report->Get("RTCPeerConnection"));
    EXPECT_EQ(expected,
              report->Get("RTCPeerConnection")->cast_to<
                  RTCPeerConnectionStats>());
  }
}

TEST_F(RTCStatsCollectorTest,
       CollectRTCMediaStreamStatsAndRTCMediaStreamTrackStats_Audio) {
  rtc::scoped_refptr<StreamCollection> local_streams =
      StreamCollection::Create();
  rtc::scoped_refptr<StreamCollection> remote_streams =
      StreamCollection::Create();
  EXPECT_CALL(test_->pc(), local_streams())
      .WillRepeatedly(Return(local_streams));
  EXPECT_CALL(test_->pc(), remote_streams())
      .WillRepeatedly(Return(remote_streams));

  rtc::scoped_refptr<MediaStream> local_stream =
      MediaStream::Create("LocalStreamLabel");
  local_streams->AddStream(local_stream);
  rtc::scoped_refptr<MediaStream> remote_stream =
      MediaStream::Create("RemoteStreamLabel");
  remote_streams->AddStream(remote_stream);

  // Local audio track
  rtc::scoped_refptr<MediaStreamTrackInterface> local_audio_track =
      CreateFakeTrack(cricket::MEDIA_TYPE_AUDIO, "LocalAudioTrackID",
                      MediaStreamTrackInterface::kEnded);
  local_stream->AddTrack(static_cast<AudioTrackInterface*>(
      local_audio_track.get()));

  cricket::VoiceSenderInfo voice_sender_info_ssrc1;
  voice_sender_info_ssrc1.local_stats.push_back(cricket::SsrcSenderInfo());
  voice_sender_info_ssrc1.local_stats[0].ssrc = 1;
  voice_sender_info_ssrc1.audio_level = 32767;
  voice_sender_info_ssrc1.echo_return_loss = 42;
  voice_sender_info_ssrc1.echo_return_loss_enhancement = 52;

  // Uses default values, the corresponding stats object should contain
  // undefined members.
  cricket::VoiceSenderInfo voice_sender_info_ssrc2;
  voice_sender_info_ssrc2.local_stats.push_back(cricket::SsrcSenderInfo());
  voice_sender_info_ssrc2.local_stats[0].ssrc = 2;
  voice_sender_info_ssrc2.audio_level = 0;
  voice_sender_info_ssrc2.echo_return_loss = -100;
  voice_sender_info_ssrc2.echo_return_loss_enhancement = -100;

  // Remote audio track
  rtc::scoped_refptr<MediaStreamTrackInterface> remote_audio_track =
      CreateFakeTrack(cricket::MEDIA_TYPE_AUDIO, "RemoteAudioTrackID",
                      MediaStreamTrackInterface::kLive);
  remote_stream->AddTrack(static_cast<AudioTrackInterface*>(
      remote_audio_track.get()));

  cricket::VoiceReceiverInfo voice_receiver_info;
  voice_receiver_info.local_stats.push_back(cricket::SsrcReceiverInfo());
  voice_receiver_info.local_stats[0].ssrc = 3;
  voice_receiver_info.audio_level = 16383;

  test_->CreateMockRtpSendersReceiversAndChannels(
      { std::make_pair(local_audio_track.get(), voice_sender_info_ssrc1),
        std::make_pair(local_audio_track.get(), voice_sender_info_ssrc2) },
      { std::make_pair(remote_audio_track.get(), voice_receiver_info) },
      {}, {});

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCMediaStreamStats expected_local_stream(
      "RTCMediaStream_local_LocalStreamLabel", report->timestamp_us());
  expected_local_stream.stream_identifier = local_stream->label();
  expected_local_stream.track_ids = std::vector<std::string>(
      { "RTCMediaStreamTrack_local_audio_LocalAudioTrackID_1",
        "RTCMediaStreamTrack_local_audio_LocalAudioTrackID_2" });
  ASSERT_TRUE(report->Get(expected_local_stream.id()));
  EXPECT_EQ(expected_local_stream,
            report->Get(expected_local_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamStats expected_remote_stream(
      "RTCMediaStream_remote_RemoteStreamLabel", report->timestamp_us());
  expected_remote_stream.stream_identifier = remote_stream->label();
  expected_remote_stream.track_ids = std::vector<std::string>({
      "RTCMediaStreamTrack_remote_audio_RemoteAudioTrackID_3" });
  ASSERT_TRUE(report->Get(expected_remote_stream.id()));
  EXPECT_EQ(expected_remote_stream,
            report->Get(expected_remote_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_local_audio_track_ssrc1(
      "RTCMediaStreamTrack_local_audio_LocalAudioTrackID_1",
      report->timestamp_us(), RTCMediaStreamTrackKind::kAudio);
  expected_local_audio_track_ssrc1.track_identifier = local_audio_track->id();
  expected_local_audio_track_ssrc1.remote_source = false;
  expected_local_audio_track_ssrc1.ended = true;
  expected_local_audio_track_ssrc1.detached = false;
  expected_local_audio_track_ssrc1.audio_level = 1.0;
  expected_local_audio_track_ssrc1.echo_return_loss = 42.0;
  expected_local_audio_track_ssrc1.echo_return_loss_enhancement = 52.0;
  ASSERT_TRUE(report->Get(expected_local_audio_track_ssrc1.id()));
  EXPECT_EQ(expected_local_audio_track_ssrc1,
            report->Get(expected_local_audio_track_ssrc1.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_local_audio_track_ssrc2(
      "RTCMediaStreamTrack_local_audio_LocalAudioTrackID_2",
      report->timestamp_us(), RTCMediaStreamTrackKind::kAudio);
  expected_local_audio_track_ssrc2.track_identifier = local_audio_track->id();
  expected_local_audio_track_ssrc2.remote_source = false;
  expected_local_audio_track_ssrc2.ended = true;
  expected_local_audio_track_ssrc2.detached = false;
  expected_local_audio_track_ssrc2.audio_level = 0.0;
  // Should be undefined: |expected_local_audio_track_ssrc2.echo_return_loss|
  // and |expected_local_audio_track_ssrc2.echo_return_loss_enhancement|.
  ASSERT_TRUE(report->Get(expected_local_audio_track_ssrc2.id()));
  EXPECT_EQ(expected_local_audio_track_ssrc2,
            report->Get(expected_local_audio_track_ssrc2.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_remote_audio_track(
      "RTCMediaStreamTrack_remote_audio_RemoteAudioTrackID_3",
      report->timestamp_us(), RTCMediaStreamTrackKind::kAudio);
  expected_remote_audio_track.track_identifier = remote_audio_track->id();
  expected_remote_audio_track.remote_source = true;
  expected_remote_audio_track.ended = false;
  expected_remote_audio_track.detached = false;
  expected_remote_audio_track.audio_level = 16383.0 / 32767.0;
  ASSERT_TRUE(report->Get(expected_remote_audio_track.id()));
  EXPECT_EQ(expected_remote_audio_track,
            report->Get(expected_remote_audio_track.id())->cast_to<
                RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest,
       CollectRTCMediaStreamStatsAndRTCMediaStreamTrackStats_Video) {
  rtc::scoped_refptr<StreamCollection> local_streams =
      StreamCollection::Create();
  rtc::scoped_refptr<StreamCollection> remote_streams =
      StreamCollection::Create();
  EXPECT_CALL(test_->pc(), local_streams())
      .WillRepeatedly(Return(local_streams));
  EXPECT_CALL(test_->pc(), remote_streams())
      .WillRepeatedly(Return(remote_streams));

  rtc::scoped_refptr<MediaStream> local_stream =
      MediaStream::Create("LocalStreamLabel");
  local_streams->AddStream(local_stream);
  rtc::scoped_refptr<MediaStream> remote_stream =
      MediaStream::Create("RemoteStreamLabel");
  remote_streams->AddStream(remote_stream);

  // Local video track
  rtc::scoped_refptr<MediaStreamTrackInterface> local_video_track =
      CreateFakeTrack(cricket::MEDIA_TYPE_VIDEO, "LocalVideoTrackID",
                      MediaStreamTrackInterface::kLive);
  local_stream->AddTrack(static_cast<VideoTrackInterface*>(
      local_video_track.get()));

  cricket::VideoSenderInfo video_sender_info_ssrc1;
  video_sender_info_ssrc1.local_stats.push_back(cricket::SsrcSenderInfo());
  video_sender_info_ssrc1.local_stats[0].ssrc = 1;
  video_sender_info_ssrc1.send_frame_width = 1234;
  video_sender_info_ssrc1.send_frame_height = 4321;
  video_sender_info_ssrc1.frames_encoded = 11;

  cricket::VideoSenderInfo video_sender_info_ssrc2;
  video_sender_info_ssrc2.local_stats.push_back(cricket::SsrcSenderInfo());
  video_sender_info_ssrc2.local_stats[0].ssrc = 2;
  video_sender_info_ssrc2.send_frame_width = 4321;
  video_sender_info_ssrc2.send_frame_height = 1234;
  video_sender_info_ssrc2.frames_encoded = 22;

  // Remote video track with values
  rtc::scoped_refptr<MediaStreamTrackInterface> remote_video_track_ssrc3 =
      CreateFakeTrack(cricket::MEDIA_TYPE_VIDEO, "RemoteVideoTrackID3",
                      MediaStreamTrackInterface::kEnded);
  remote_stream->AddTrack(static_cast<VideoTrackInterface*>(
      remote_video_track_ssrc3.get()));

  cricket::VideoReceiverInfo video_receiver_info_ssrc3;
  video_receiver_info_ssrc3.local_stats.push_back(cricket::SsrcReceiverInfo());
  video_receiver_info_ssrc3.local_stats[0].ssrc = 3;
  video_receiver_info_ssrc3.frame_width = 6789;
  video_receiver_info_ssrc3.frame_height = 9876;
  video_receiver_info_ssrc3.frames_received = 1000;
  video_receiver_info_ssrc3.frames_decoded = 995;
  video_receiver_info_ssrc3.frames_rendered = 990;

  // Remote video track with undefined (default) values
  rtc::scoped_refptr<MediaStreamTrackInterface> remote_video_track_ssrc4 =
      CreateFakeTrack(cricket::MEDIA_TYPE_VIDEO, "RemoteVideoTrackID4",
                      MediaStreamTrackInterface::kLive);
  remote_stream->AddTrack(static_cast<VideoTrackInterface*>(
      remote_video_track_ssrc4.get()));

  cricket::VideoReceiverInfo video_receiver_info_ssrc4;
  video_receiver_info_ssrc4.local_stats.push_back(cricket::SsrcReceiverInfo());
  video_receiver_info_ssrc4.local_stats[0].ssrc = 4;
  video_receiver_info_ssrc4.frame_width = 0;
  video_receiver_info_ssrc4.frame_height = 0;
  video_receiver_info_ssrc4.frames_received = 0;
  video_receiver_info_ssrc4.frames_decoded = 0;
  video_receiver_info_ssrc4.frames_rendered = 0;

  test_->CreateMockRtpSendersReceiversAndChannels(
      {}, {},
      { std::make_pair(local_video_track.get(), video_sender_info_ssrc1),
        std::make_pair(local_video_track.get(), video_sender_info_ssrc2) },
      { std::make_pair(remote_video_track_ssrc3.get(),
                       video_receiver_info_ssrc3),
        std::make_pair(remote_video_track_ssrc4.get(),
                       video_receiver_info_ssrc4) });

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCMediaStreamStats expected_local_stream(
      "RTCMediaStream_local_LocalStreamLabel", report->timestamp_us());
  expected_local_stream.stream_identifier = local_stream->label();
  expected_local_stream.track_ids = std::vector<std::string>({
      "RTCMediaStreamTrack_local_video_LocalVideoTrackID_1",
      "RTCMediaStreamTrack_local_video_LocalVideoTrackID_2" });
  ASSERT_TRUE(report->Get(expected_local_stream.id()));
  EXPECT_EQ(expected_local_stream,
            report->Get(expected_local_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamStats expected_remote_stream(
      "RTCMediaStream_remote_RemoteStreamLabel", report->timestamp_us());
  expected_remote_stream.stream_identifier = remote_stream->label();
  expected_remote_stream.track_ids = std::vector<std::string>({
      "RTCMediaStreamTrack_remote_video_RemoteVideoTrackID3_3",
      "RTCMediaStreamTrack_remote_video_RemoteVideoTrackID4_4" });
  ASSERT_TRUE(report->Get(expected_remote_stream.id()));
  EXPECT_EQ(expected_remote_stream,
            report->Get(expected_remote_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_local_video_track_ssrc1(
      "RTCMediaStreamTrack_local_video_LocalVideoTrackID_1",
      report->timestamp_us(), RTCMediaStreamTrackKind::kVideo);
  expected_local_video_track_ssrc1.track_identifier = local_video_track->id();
  expected_local_video_track_ssrc1.remote_source = false;
  expected_local_video_track_ssrc1.ended = false;
  expected_local_video_track_ssrc1.detached = false;
  expected_local_video_track_ssrc1.frame_width = 1234;
  expected_local_video_track_ssrc1.frame_height = 4321;
  expected_local_video_track_ssrc1.frames_sent = 11;
  ASSERT_TRUE(report->Get(expected_local_video_track_ssrc1.id()));
  EXPECT_EQ(expected_local_video_track_ssrc1,
            report->Get(expected_local_video_track_ssrc1.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_local_video_track_ssrc2(
      "RTCMediaStreamTrack_local_video_LocalVideoTrackID_2",
      report->timestamp_us(), RTCMediaStreamTrackKind::kVideo);
  expected_local_video_track_ssrc2.track_identifier = local_video_track->id();
  expected_local_video_track_ssrc2.remote_source = false;
  expected_local_video_track_ssrc2.ended = false;
  expected_local_video_track_ssrc2.detached = false;
  expected_local_video_track_ssrc2.frame_width = 4321;
  expected_local_video_track_ssrc2.frame_height = 1234;
  expected_local_video_track_ssrc2.frames_sent = 22;
  ASSERT_TRUE(report->Get(expected_local_video_track_ssrc2.id()));
  EXPECT_EQ(expected_local_video_track_ssrc2,
            report->Get(expected_local_video_track_ssrc2.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_remote_video_track_ssrc3(
      "RTCMediaStreamTrack_remote_video_RemoteVideoTrackID3_3",
      report->timestamp_us(), RTCMediaStreamTrackKind::kVideo);
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
            report->Get(expected_remote_video_track_ssrc3.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_remote_video_track_ssrc4(
      "RTCMediaStreamTrack_remote_video_RemoteVideoTrackID4_4",
      report->timestamp_us(), RTCMediaStreamTrackKind::kVideo);
  expected_remote_video_track_ssrc4.track_identifier =
      remote_video_track_ssrc4->id();
  expected_remote_video_track_ssrc4.remote_source = true;
  expected_remote_video_track_ssrc4.ended = false;
  expected_remote_video_track_ssrc4.detached = false;
  expected_remote_video_track_ssrc4.frames_received = 0;
  expected_remote_video_track_ssrc4.frames_decoded = 0;
  expected_remote_video_track_ssrc4.frames_dropped = 0;
  // Should be undefined: |expected_remote_video_track_ssrc4.frame_width| and
  // |expected_remote_video_track_ssrc4.frame_height|.
  ASSERT_TRUE(report->Get(expected_remote_video_track_ssrc4.id()));
  EXPECT_EQ(expected_remote_video_track_ssrc4,
            report->Get(expected_remote_video_track_ssrc4.id())->cast_to<
                RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCInboundRTPStreamStats_Audio) {
  MockVoiceMediaChannel* voice_media_channel = new MockVoiceMediaChannel();
  cricket::VoiceChannel voice_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), test_->media_engine(), voice_media_channel,
      "VoiceContentName", kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  test_->SetupRemoteTrackAndReceiver(
      cricket::MEDIA_TYPE_AUDIO, "RemoteAudioTrackID", 1);

  cricket::VoiceMediaInfo voice_media_info;

  voice_media_info.receivers.push_back(cricket::VoiceReceiverInfo());
  voice_media_info.receivers[0].local_stats.push_back(
      cricket::SsrcReceiverInfo());
  voice_media_info.receivers[0].local_stats[0].ssrc = 1;
  voice_media_info.receivers[0].packets_lost = 42;
  voice_media_info.receivers[0].packets_rcvd = 2;
  voice_media_info.receivers[0].bytes_rcvd = 3;
  voice_media_info.receivers[0].codec_payload_type = rtc::Optional<int>(42);
  voice_media_info.receivers[0].jitter_ms = 4500;
  voice_media_info.receivers[0].fraction_lost = 5.5f;

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = rtc::Optional<int>(0);
  voice_media_info.receive_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  EXPECT_CALL(*voice_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(voice_media_info), Return(true)));

  SessionStats session_stats;
  session_stats.proxy_to_transport["VoiceContentName"] = "TransportName";
  session_stats.transport_stats["TransportName"].transport_name =
      "TransportName";

  // Make sure the associated |RTCTransportStats| is created.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  session_stats.transport_stats["TransportName"].channel_stats.push_back(
      channel_stats);

  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));
  EXPECT_CALL(test_->session(), voice_channel())
      .WillRepeatedly(Return(&voice_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCInboundRTPStreamStats expected_audio(
      "RTCInboundRTPAudioStream_1", report->timestamp_us());
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.track_id =
      "RTCMediaStreamTrack_remote_audio_RemoteAudioTrackID_1";
  expected_audio.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_audio.codec_id = "RTCCodec_InboundAudio_42";
  expected_audio.packets_received = 2;
  expected_audio.bytes_received = 3;
  expected_audio.packets_lost = 42;
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
  MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), video_media_channel, "VideoContentName",
      kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  test_->SetupRemoteTrackAndReceiver(
      cricket::MEDIA_TYPE_VIDEO, "RemoteVideoTrackID", 1);

  cricket::VideoMediaInfo video_media_info;

  video_media_info.receivers.push_back(cricket::VideoReceiverInfo());
  video_media_info.receivers[0].local_stats.push_back(
      cricket::SsrcReceiverInfo());
  video_media_info.receivers[0].local_stats[0].ssrc = 1;
  video_media_info.receivers[0].packets_rcvd = 2;
  video_media_info.receivers[0].packets_lost = 42;
  video_media_info.receivers[0].bytes_rcvd = 3;
  video_media_info.receivers[0].fraction_lost = 4.5f;
  video_media_info.receivers[0].codec_payload_type = rtc::Optional<int>(42);
  video_media_info.receivers[0].firs_sent = 5;
  video_media_info.receivers[0].plis_sent = 6;
  video_media_info.receivers[0].nacks_sent = 7;
  video_media_info.receivers[0].frames_decoded = 8;
  video_media_info.receivers[0].qp_sum = rtc::Optional<uint64_t>();

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = rtc::Optional<int>(0);
  video_media_info.receive_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));

  SessionStats session_stats;
  session_stats.proxy_to_transport["VideoContentName"] = "TransportName";
  session_stats.transport_stats["TransportName"].transport_name =
      "TransportName";

  // Make sure the associated |RTCTransportStats| is created.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  session_stats.transport_stats["TransportName"].channel_stats.push_back(
      channel_stats);

  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));
  EXPECT_CALL(test_->session(), video_channel())
      .WillRepeatedly(Return(&video_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCInboundRTPStreamStats expected_video(
      "RTCInboundRTPVideoStream_1", report->timestamp_us());
  expected_video.ssrc = 1;
  expected_video.is_remote = false;
  expected_video.media_type = "video";
  expected_video.track_id =
      "RTCMediaStreamTrack_remote_video_RemoteVideoTrackID_1";
  expected_video.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_video.codec_id = "RTCCodec_InboundVideo_42";
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
  video_media_info.receivers[0].qp_sum = rtc::Optional<uint64_t>(9);
  expected_video.qp_sum = 9;

  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  collector_->ClearCachedStatsReport();
  report = GetStatsReport();

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCInboundRTPStreamStats>(),
      expected_video);
  EXPECT_TRUE(report->Get(*expected_video.track_id));
  EXPECT_TRUE(report->Get(*expected_video.transport_id));
  EXPECT_TRUE(report->Get(*expected_video.codec_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCOutboundRTPStreamStats_Audio) {
  MockVoiceMediaChannel* voice_media_channel = new MockVoiceMediaChannel();
  cricket::VoiceChannel voice_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), test_->media_engine(), voice_media_channel,
      "VoiceContentName", kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  test_->SetupLocalTrackAndSender(
      cricket::MEDIA_TYPE_AUDIO, "LocalAudioTrackID", 1);

  cricket::VoiceMediaInfo voice_media_info;

  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = 1;
  voice_media_info.senders[0].packets_sent = 2;
  voice_media_info.senders[0].bytes_sent = 3;
  voice_media_info.senders[0].codec_payload_type = rtc::Optional<int>(42);

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = rtc::Optional<int>(0);
  voice_media_info.send_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  EXPECT_CALL(*voice_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(voice_media_info), Return(true)));

  SessionStats session_stats;
  session_stats.proxy_to_transport["VoiceContentName"] = "TransportName";
  session_stats.transport_stats["TransportName"].transport_name =
      "TransportName";

  // Make sure the associated |RTCTransportStats| is created.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  session_stats.transport_stats["TransportName"].channel_stats.push_back(
      channel_stats);

  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));
  EXPECT_CALL(test_->session(), voice_channel())
      .WillRepeatedly(Return(&voice_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCOutboundRTPStreamStats expected_audio(
      "RTCOutboundRTPAudioStream_1", report->timestamp_us());
  expected_audio.ssrc = 1;
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.track_id =
      "RTCMediaStreamTrack_local_audio_LocalAudioTrackID_1";
  expected_audio.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_audio.codec_id = "RTCCodec_OutboundAudio_42";
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
  MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      test_->worker_thread(), test_->network_thread(),
      test_->signaling_thread(), video_media_channel, "VideoContentName",
      kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  test_->SetupLocalTrackAndSender(
      cricket::MEDIA_TYPE_VIDEO, "LocalVideoTrackID", 1);

  cricket::VideoMediaInfo video_media_info;

  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].local_stats[0].ssrc = 1;
  video_media_info.senders[0].firs_rcvd = 2;
  video_media_info.senders[0].plis_rcvd = 3;
  video_media_info.senders[0].nacks_rcvd = 4;
  video_media_info.senders[0].packets_sent = 5;
  video_media_info.senders[0].bytes_sent = 6;
  video_media_info.senders[0].codec_payload_type = rtc::Optional<int>(42);
  video_media_info.senders[0].frames_encoded = 8;
  video_media_info.senders[0].qp_sum = rtc::Optional<uint64_t>();

  RtpCodecParameters codec_parameters;
  codec_parameters.payload_type = 42;
  codec_parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  codec_parameters.name = "dummy";
  codec_parameters.clock_rate = rtc::Optional<int>(0);
  video_media_info.send_codecs.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));

  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));

  SessionStats session_stats;
  session_stats.proxy_to_transport["VideoContentName"] = "TransportName";
  session_stats.transport_stats["TransportName"].transport_name =
      "TransportName";

  // Make sure the associated |RTCTransportStats| is created.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
  session_stats.transport_stats["TransportName"].channel_stats.push_back(
      channel_stats);

  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));
  EXPECT_CALL(test_->session(), video_channel())
      .WillRepeatedly(Return(&video_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCOutboundRTPStreamStats expected_video(
      "RTCOutboundRTPVideoStream_1", report->timestamp_us());
  expected_video.ssrc = 1;
  expected_video.is_remote = false;
  expected_video.media_type = "video";
  expected_video.track_id =
      "RTCMediaStreamTrack_local_video_LocalVideoTrackID_1";
  expected_video.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_video.codec_id = "RTCCodec_OutboundVideo_42";
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
  video_media_info.senders[0].qp_sum = rtc::Optional<uint64_t>(9);
  expected_video.qp_sum = 9;

  EXPECT_CALL(*video_media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(video_media_info), Return(true)));
  collector_->ClearCachedStatsReport();
  report = GetStatsReport();

  ASSERT_TRUE(report->Get(expected_video.id()));
  EXPECT_EQ(
      report->Get(expected_video.id())->cast_to<RTCOutboundRTPStreamStats>(),
      expected_video);
  EXPECT_TRUE(report->Get(*expected_video.track_id));
  EXPECT_TRUE(report->Get(*expected_video.transport_id));
  EXPECT_TRUE(report->Get(*expected_video.codec_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCTransportStats) {
  std::unique_ptr<cricket::Candidate> rtp_local_candidate = CreateFakeCandidate(
      "42.42.42.42", 42, "protocol", cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtp_remote_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtcp_local_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> rtcp_remote_candidate =
      CreateFakeCandidate("42.42.42.42", 42, "protocol",
                          cricket::LOCAL_PORT_TYPE, 42);

  SessionStats session_stats;
  session_stats.transport_stats["transport"].transport_name = "transport";

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
  session_stats.transport_stats["transport"].channel_stats.push_back(
      rtp_transport_channel_stats);


  // Mock the session to return the desired candidates.
  EXPECT_CALL(test_->session(), GetStats(_)).WillRepeatedly(Invoke(
      [&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats));
      }));

  // Get stats without RTCP, an active connection or certificates.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCTransportStats expected_rtp_transport(
      "RTCTransport_transport_" +
          rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP),
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
  session_stats.transport_stats["transport"].channel_stats.push_back(
      rtcp_transport_channel_stats);

  collector_->ClearCachedStatsReport();
  // Get stats with RTCP and without an active connection or certificates.
  report = GetStatsReport();

  RTCTransportStats expected_rtcp_transport(
      "RTCTransport_transport_" +
          rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTCP),
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
  session_stats.transport_stats["transport"]
      .channel_stats[1]
      .connection_infos[0]
      .best_connection = true;

  collector_->ClearCachedStatsReport();
  report = GetStatsReport();

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
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(local) local", "(local) chain" }));
  std::unique_ptr<CertificateInfo> remote_certinfo =
      CreateFakeCertificateAndInfoFromDers(
          std::vector<std::string>({ "(remote) local", "(remote) chain" }));
  EXPECT_CALL(test_->session(), GetLocalCertificate(_, _)).WillRepeatedly(
    Invoke([this, &local_certinfo](const std::string& transport_name,
           rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
      if (transport_name == "transport") {
        *certificate = local_certinfo->certificate;
        return true;
      }
      return false;
    }));
  EXPECT_CALL(test_->session(),
      GetRemoteSSLCertificate_ReturnsRawPointer(_)).WillRepeatedly(Invoke(
      [this, &remote_certinfo](const std::string& transport_name) {
        if (transport_name == "transport")
          return remote_certinfo->certificate->ssl_certificate().GetReference();
        return static_cast<rtc::SSLCertificate*>(nullptr);
      }));

  collector_->ClearCachedStatsReport();
  report = GetStatsReport();

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

class RTCStatsCollectorTestWithFakeCollector : public testing::Test {
 public:
  RTCStatsCollectorTestWithFakeCollector()
    : test_(new rtc::RefCountedObject<RTCStatsCollectorTestHelper>()),
      collector_(FakeRTCStatsCollector::Create(
          &test_->pc(), 50 * rtc::kNumMicrosecsPerMillisec)) {
  }

 protected:
  rtc::scoped_refptr<RTCStatsCollectorTestHelper> test_;
  rtc::scoped_refptr<FakeRTCStatsCollector> collector_;
};

TEST_F(RTCStatsCollectorTestWithFakeCollector, ThreadUsageAndResultsMerging) {
  collector_->VerifyThreadUsageAndResultsMerging();
}

}  // namespace

}  // namespace webrtc
