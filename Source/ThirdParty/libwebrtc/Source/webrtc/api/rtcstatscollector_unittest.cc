/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtcstatscollector.h"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "webrtc/api/mediastream.h"
#include "webrtc/api/mediastreamtrack.h"
#include "webrtc/api/jsepsessiondescription.h"
#include "webrtc/api/stats/rtcstats_objects.h"
#include "webrtc/api/stats/rtcstatsreport.h"
#include "webrtc/api/test/mock_datachannel.h"
#include "webrtc/api/test/mock_peerconnection.h"
#include "webrtc/api/test/mock_webrtcsession.h"
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

class FakeAudioProcessorForStats
    : public rtc::RefCountedObject<AudioProcessorInterface> {
 public:
  FakeAudioProcessorForStats(
      AudioProcessorInterface::AudioProcessorStats stats)
      : stats_(stats) {
  }

  void GetStats(AudioProcessorInterface::AudioProcessorStats* stats) override {
    *stats = stats_;
  }

 private:
  AudioProcessorInterface::AudioProcessorStats stats_;
};

class FakeAudioTrackForStats
    : public MediaStreamTrack<AudioTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeAudioTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state,
      int signal_level,
      rtc::scoped_refptr<FakeAudioProcessorForStats> processor) {
    rtc::scoped_refptr<FakeAudioTrackForStats> audio_track_stats(
        new rtc::RefCountedObject<FakeAudioTrackForStats>(
            id, signal_level, processor));
    audio_track_stats->set_state(state);
    return audio_track_stats;
  }

  FakeAudioTrackForStats(
      const std::string& id,
      int signal_level,
      rtc::scoped_refptr<FakeAudioProcessorForStats> processor)
      : MediaStreamTrack<AudioTrackInterface>(id),
        signal_level_(signal_level),
        processor_(processor) {
  }

  std::string kind() const override {
    return MediaStreamTrackInterface::kAudioKind;
  }
  webrtc::AudioSourceInterface* GetSource() const override { return nullptr; }
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {}
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {}
  bool GetSignalLevel(int* level) override {
    *level = signal_level_;
    return true;
  }
  rtc::scoped_refptr<AudioProcessorInterface> GetAudioProcessor() override {
    return processor_;
  }

 private:
  int signal_level_;
  rtc::scoped_refptr<FakeAudioProcessorForStats> processor_;
};

class FakeVideoTrackSourceForStats
    : public rtc::RefCountedObject<VideoTrackSourceInterface> {
 public:
  FakeVideoTrackSourceForStats(VideoTrackSourceInterface::Stats stats)
      : stats_(stats) {
  }

  MediaSourceInterface::SourceState state() const override {
    return MediaSourceInterface::kLive;
  }
  bool remote() const override { return false; }
  void RegisterObserver(ObserverInterface* observer) override {}
  void UnregisterObserver(ObserverInterface* observer) override {}
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
  }
  bool is_screencast() const override { return false; }
  rtc::Optional<bool> needs_denoising() const override {
    return rtc::Optional<bool>();
  }
  bool GetStats(VideoTrackSourceInterface::Stats* stats) override {
    *stats = stats_;
    return true;
  }

 private:
  VideoTrackSourceInterface::Stats stats_;
};

class FakeVideoTrackForStats
    : public MediaStreamTrack<VideoTrackInterface> {
 public:
  static rtc::scoped_refptr<FakeVideoTrackForStats> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state,
      rtc::scoped_refptr<VideoTrackSourceInterface> source) {
    rtc::scoped_refptr<FakeVideoTrackForStats> video_track(
        new rtc::RefCountedObject<FakeVideoTrackForStats>(id, source));
    video_track->set_state(state);
    return video_track;
  }

  FakeVideoTrackForStats(
      const std::string& id,
      rtc::scoped_refptr<VideoTrackSourceInterface> source)
      : MediaStreamTrack<VideoTrackInterface>(id),
        source_(source) {
  }

  std::string kind() const override {
    return MediaStreamTrackInterface::kVideoKind;
  }
  VideoTrackSourceInterface* GetSource() const override {
    return source_;
  }

 private:
  rtc::scoped_refptr<VideoTrackSourceInterface> source_;
};

class RTCStatsCollectorTestHelper : public SetSessionDescriptionObserver {
 public:
  RTCStatsCollectorTestHelper()
      : worker_thread_(rtc::Thread::Current()),
        network_thread_(rtc::Thread::Current()),
        media_engine_(new cricket::FakeMediaEngine()),
        channel_manager_(
            new cricket::ChannelManager(media_engine_,
                                        worker_thread_,
                                        network_thread_)),
        media_controller_(
            MediaControllerInterface::Create(cricket::MediaConfig(),
                                             worker_thread_,
                                             channel_manager_.get(),
                                             &event_log_)),
        session_(media_controller_.get()),
        pc_() {
    // Default return values for mocks.
    EXPECT_CALL(pc_, local_streams()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(pc_, remote_streams()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(pc_, session()).WillRepeatedly(Return(&session_));
    EXPECT_CALL(pc_, sctp_data_channels()).WillRepeatedly(
        ReturnRef(data_channels_));
    EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, GetTransportStats(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(session_, GetLocalCertificate(_, _)).WillRepeatedly(
        Return(false));
    EXPECT_CALL(session_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
        .WillRepeatedly(Return(nullptr));
  }

  rtc::ScopedFakeClock& fake_clock() { return fake_clock_; }
  rtc::Thread* worker_thread() { return worker_thread_; }
  rtc::Thread* network_thread() { return network_thread_; }
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

 private:
  rtc::ScopedFakeClock fake_clock_;
  RtcEventLogNullImpl event_log_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  cricket::FakeMediaEngine* media_engine_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  std::unique_ptr<MediaControllerInterface> media_controller_;
  MockWebRtcSession session_;
  MockPeerConnection pc_;

  std::vector<rtc::scoped_refptr<DataChannel>> data_channels_;
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
    EXPECT_EQ(produced_on_worker_thread_, 1);
    EXPECT_EQ(produced_on_network_thread_, 1);

    EXPECT_TRUE(delivered_report_->Get("SignalingThreadStats"));
    EXPECT_TRUE(delivered_report_->Get("WorkerThreadStats"));
    EXPECT_TRUE(delivered_report_->Get("NetworkThreadStats"));

    produced_on_signaling_thread_ = 0;
    produced_on_worker_thread_ = 0;
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
  void ProducePartialResultsOnWorkerThread(int64_t timestamp_us) override {
    EXPECT_TRUE(worker_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_worker_thread_;
    }

    rtc::scoped_refptr<RTCStatsReport> worker_report =
        RTCStatsReport::Create(0);
    worker_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("WorkerThreadStats", timestamp_us)));
    AddPartialResults(worker_report);
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
  int produced_on_worker_thread_ = 0;
  int produced_on_network_thread_ = 0;
};

class StatsCallback : public RTCStatsCollectorCallback {
 public:
  static rtc::scoped_refptr<StatsCallback> Create(
      rtc::scoped_refptr<const RTCStatsReport>* report_ptr = nullptr) {
    return rtc::scoped_refptr<StatsCallback>(
        new rtc::RefCountedObject<StatsCallback>(report_ptr));
  }

  void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    report_ = report;
    if (report_ptr_)
      *report_ptr_ = report_;
  }

  rtc::scoped_refptr<const RTCStatsReport> report() const {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    return report_;
  }

 protected:
  explicit StatsCallback(rtc::scoped_refptr<const RTCStatsReport>* report_ptr)
      : report_ptr_(report_ptr) {}

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::scoped_refptr<const RTCStatsReport> report_;
  rtc::scoped_refptr<const RTCStatsReport>* report_ptr_;
};

class RTCStatsCollectorTest : public testing::Test {
 public:
  RTCStatsCollectorTest()
    : test_(new rtc::RefCountedObject<RTCStatsCollectorTestHelper>()),
      collector_(RTCStatsCollector::Create(
          &test_->pc(), 50 * rtc::kNumMicrosecsPerMillisec)) {
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsReport() {
    rtc::scoped_refptr<StatsCallback> callback = StatsCallback::Create();
    collector_->GetStatsReport(callback);
    EXPECT_TRUE_WAIT(callback->report(), kGetStatsReportTimeoutMs);
    int64_t after = rtc::TimeUTCMicros();
    for (const RTCStats& stats : *callback->report()) {
      EXPECT_LE(stats.timestamp_us(), after);
    }
    return callback->report();
  }

  const RTCIceCandidateStats* ExpectReportContainsCandidate(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const cricket::Candidate& candidate,
      bool is_local) {
    const RTCStats* stats = report->Get("RTCIceCandidate_" + candidate.id());
    EXPECT_TRUE(stats);
    const RTCIceCandidateStats* candidate_stats;
    if (is_local)
        candidate_stats = &stats->cast_to<RTCLocalIceCandidateStats>();
    else
        candidate_stats = &stats->cast_to<RTCRemoteIceCandidateStats>();
    EXPECT_EQ(*candidate_stats->ip, candidate.address().ipaddr().ToString());
    EXPECT_EQ(*candidate_stats->port,
              static_cast<int32_t>(candidate.address().port()));
    EXPECT_EQ(*candidate_stats->protocol, candidate.protocol());
    EXPECT_EQ(*candidate_stats->candidate_type,
              CandidateTypeToRTCIceCandidateTypeForTesting(candidate.type()));
    EXPECT_EQ(*candidate_stats->priority,
              static_cast<int32_t>(candidate.priority()));
    // TODO(hbos): Define candidate_stats->url. crbug.com/632723
    EXPECT_FALSE(candidate_stats->url.is_defined());
    return candidate_stats;
  }

  void ExpectReportContainsCandidatePair(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const cricket::TransportStats& transport_stats) {
    for (const auto& channel_stats : transport_stats.channel_stats) {
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        const std::string& id = "RTCIceCandidatePair_" +
            info.local_candidate.id() + "_" + info.remote_candidate.id();
        const RTCStats* stats = report->Get(id);
        ASSERT_TRUE(stats);
        const RTCIceCandidatePairStats& candidate_pair_stats =
            stats->cast_to<RTCIceCandidatePairStats>();

        // TODO(hbos): Define all the undefined |candidate_pair_stats| stats.
        // The EXPECT_FALSE are for the undefined stats, see also todos listed
        // in rtcstats_objects.h. crbug.com/633550
        EXPECT_FALSE(candidate_pair_stats.transport_id.is_defined());
        const RTCIceCandidateStats* local_candidate =
            ExpectReportContainsCandidate(report, info.local_candidate, true);
        EXPECT_EQ(*candidate_pair_stats.local_candidate_id,
                  local_candidate->id());
        const RTCIceCandidateStats* remote_candidate =
            ExpectReportContainsCandidate(report, info.remote_candidate, false);
        EXPECT_EQ(*candidate_pair_stats.remote_candidate_id,
                  remote_candidate->id());

        EXPECT_FALSE(candidate_pair_stats.state.is_defined());
        EXPECT_FALSE(candidate_pair_stats.priority.is_defined());
        EXPECT_FALSE(candidate_pair_stats.nominated.is_defined());
        EXPECT_EQ(*candidate_pair_stats.writable, info.writable);
        EXPECT_FALSE(candidate_pair_stats.readable.is_defined());
        EXPECT_EQ(*candidate_pair_stats.bytes_sent,
                  static_cast<uint64_t>(info.sent_total_bytes));
        EXPECT_EQ(*candidate_pair_stats.bytes_received,
                  static_cast<uint64_t>(info.recv_total_bytes));
        EXPECT_FALSE(candidate_pair_stats.total_rtt.is_defined());
        EXPECT_EQ(*candidate_pair_stats.current_rtt,
                  static_cast<double>(info.rtt) / 1000.0);
        EXPECT_FALSE(
            candidate_pair_stats.available_outgoing_bitrate.is_defined());
        EXPECT_FALSE(
            candidate_pair_stats.available_incoming_bitrate.is_defined());
        EXPECT_FALSE(candidate_pair_stats.requests_received.is_defined());
        EXPECT_EQ(*candidate_pair_stats.requests_sent,
                  static_cast<uint64_t>(info.sent_ping_requests_total));
        EXPECT_EQ(*candidate_pair_stats.responses_received,
                  static_cast<uint64_t>(info.recv_ping_responses));
        EXPECT_EQ(*candidate_pair_stats.responses_sent,
                  static_cast<uint64_t>(info.sent_ping_responses));
        EXPECT_FALSE(
            candidate_pair_stats.retransmissions_received.is_defined());
        EXPECT_FALSE(candidate_pair_stats.retransmissions_sent.is_defined());
        EXPECT_FALSE(
            candidate_pair_stats.consent_requests_received.is_defined());
        EXPECT_FALSE(candidate_pair_stats.consent_requests_sent.is_defined());
        EXPECT_FALSE(
            candidate_pair_stats.consent_responses_received.is_defined());
        EXPECT_FALSE(candidate_pair_stats.consent_responses_sent.is_defined());
      }
    }
  }

  void ExpectReportContainsCertificateInfo(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const CertificateInfo& cert_info) {
    for (size_t i = 0; i < cert_info.fingerprints.size(); ++i) {
      const RTCStats* stats = report->Get(
          "RTCCertificate_" + cert_info.fingerprints[i]);
      ASSERT_TRUE(stats);
      const RTCCertificateStats& cert_stats =
          stats->cast_to<const RTCCertificateStats>();
      EXPECT_EQ(*cert_stats.fingerprint, cert_info.fingerprints[i]);
      EXPECT_EQ(*cert_stats.fingerprint_algorithm, "sha-1");
      EXPECT_EQ(*cert_stats.base64_certificate, cert_info.pems[i]);
      if (i + 1 < cert_info.fingerprints.size()) {
        EXPECT_EQ(*cert_stats.issuer_certificate_id,
                  "RTCCertificate_" + cert_info.fingerprints[i + 1]);
      } else {
        EXPECT_FALSE(cert_stats.issuer_certificate_id.is_defined());
      }
    }
  }

  void ExpectReportContainsTransportStats(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const cricket::TransportStats& transport,
      const CertificateInfo* local_certinfo,
      const CertificateInfo* remote_certinfo) {
    std::string rtcp_transport_stats_id;
    for (const auto& channel_stats : transport.channel_stats) {
      if (channel_stats.component == cricket::ICE_CANDIDATE_COMPONENT_RTCP) {
        rtcp_transport_stats_id = "RTCTransport_" + transport.transport_name +
            "_" + rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTCP);
      }
    }
    for (const auto& channel_stats : transport.channel_stats) {
      const cricket::ConnectionInfo* best_connection_info = nullptr;
      const RTCStats* stats = report->Get(
          "RTCTransport_" + transport.transport_name + "_" +
          rtc::ToString<>(channel_stats.component));
      ASSERT_TRUE(stats);
      const RTCTransportStats& transport_stats =
          stats->cast_to<const RTCTransportStats>();
      uint64_t bytes_sent = 0;
      uint64_t bytes_received = 0;
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        bytes_sent += info.sent_total_bytes;
        bytes_received += info.recv_total_bytes;
        if (info.best_connection)
          best_connection_info = &info;
      }
      EXPECT_EQ(*transport_stats.bytes_sent, bytes_sent);
      EXPECT_EQ(*transport_stats.bytes_received, bytes_received);
      if (best_connection_info) {
        EXPECT_EQ(*transport_stats.active_connection, true);
        // TODO(hbos): Instead of testing how the ID looks, test that the
        // corresponding pair's IP addresses are equal to the IP addresses of
        // the |best_connection_info| data. crbug.com/653873
        EXPECT_EQ(*transport_stats.selected_candidate_pair_id,
                  "RTCIceCandidatePair_" +
                  best_connection_info->local_candidate.id() + "_" +
                  best_connection_info->remote_candidate.id());
        EXPECT_TRUE(report->Get(*transport_stats.selected_candidate_pair_id));
      } else {
        EXPECT_EQ(*transport_stats.active_connection, false);
        EXPECT_FALSE(transport_stats.selected_candidate_pair_id.is_defined());
      }
      if (channel_stats.component != cricket::ICE_CANDIDATE_COMPONENT_RTCP &&
          !rtcp_transport_stats_id.empty()) {
        EXPECT_EQ(*transport_stats.rtcp_transport_stats_id,
                  rtcp_transport_stats_id);
      } else {
        EXPECT_FALSE(transport_stats.rtcp_transport_stats_id.is_defined());
      }
      if (local_certinfo && remote_certinfo) {
        EXPECT_EQ(*transport_stats.local_certificate_id,
                  "RTCCertificate_" + local_certinfo->fingerprints[0]);
        EXPECT_EQ(*transport_stats.remote_certificate_id,
                  "RTCCertificate_" + remote_certinfo->fingerprints[0]);
        EXPECT_TRUE(report->Get(*transport_stats.local_certificate_id));
        EXPECT_TRUE(report->Get(*transport_stats.remote_certificate_id));
      } else {
        EXPECT_FALSE(transport_stats.local_certificate_id.is_defined());
        EXPECT_FALSE(transport_stats.remote_certificate_id.is_defined());
      }
    }
  }

  void ExpectReportContainsDataChannel(
      const rtc::scoped_refptr<const RTCStatsReport>& report,
      const DataChannel& data_channel) {
    const RTCStats* stats = report->Get("RTCDataChannel_" +
                                        rtc::ToString<>(data_channel.id()));
    EXPECT_TRUE(stats);
    const RTCDataChannelStats& data_channel_stats =
        stats->cast_to<const RTCDataChannelStats>();
    EXPECT_EQ(*data_channel_stats.label, data_channel.label());
    EXPECT_EQ(*data_channel_stats.protocol, data_channel.protocol());
    EXPECT_EQ(*data_channel_stats.datachannelid, data_channel.id());
    EXPECT_EQ(*data_channel_stats.state,
        DataStateToRTCDataChannelStateForTesting(data_channel.state()));
    EXPECT_EQ(*data_channel_stats.messages_sent, data_channel.messages_sent());
    EXPECT_EQ(*data_channel_stats.bytes_sent, data_channel.bytes_sent());
    EXPECT_EQ(*data_channel_stats.messages_received,
              data_channel.messages_received());
    EXPECT_EQ(*data_channel_stats.bytes_received,
              data_channel.bytes_received());
  }

 protected:
  rtc::scoped_refptr<RTCStatsCollectorTestHelper> test_;
  rtc::scoped_refptr<RTCStatsCollector> collector_;
};

TEST_F(RTCStatsCollectorTest, SingleCallback) {
  rtc::scoped_refptr<const RTCStatsReport> result;
  collector_->GetStatsReport(StatsCallback::Create(&result));
  EXPECT_TRUE_WAIT(result, kGetStatsReportTimeoutMs);
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacks) {
  rtc::scoped_refptr<const RTCStatsReport> a;
  rtc::scoped_refptr<const RTCStatsReport> b;
  rtc::scoped_refptr<const RTCStatsReport> c;
  collector_->GetStatsReport(StatsCallback::Create(&a));
  collector_->GetStatsReport(StatsCallback::Create(&b));
  collector_->GetStatsReport(StatsCallback::Create(&c));
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
  collector_->GetStatsReport(StatsCallback::Create(&a));
  collector_->GetStatsReport(StatsCallback::Create(&b));
  // Cache is invalidated after 50 ms.
  test_->fake_clock().AdvanceTime(rtc::TimeDelta::FromMilliseconds(51));
  collector_->GetStatsReport(StatsCallback::Create(&c));
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
  EXPECT_CALL(test_->session(), GetTransportStats(_)).WillRepeatedly(Invoke(
      [this](SessionStats* stats) {
        stats->transport_stats["transport"].transport_name = "transport";
        return true;
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
  ExpectReportContainsCertificateInfo(report, *local_certinfo.get());
  ExpectReportContainsCertificateInfo(report, *remote_certinfo.get());
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
  EXPECT_CALL(test_->session(), GetTransportStats(_)).WillRepeatedly(Invoke(
      [this](SessionStats* stats) {
        stats->transport_stats["audio"].transport_name = "audio";
        stats->transport_stats["video"].transport_name = "video";
        return true;
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
  ExpectReportContainsCertificateInfo(report, *audio_local_certinfo.get());
  ExpectReportContainsCertificateInfo(report, *audio_remote_certinfo.get());
  ExpectReportContainsCertificateInfo(report, *video_local_certinfo.get());
  ExpectReportContainsCertificateInfo(report, *video_remote_certinfo.get());
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
  EXPECT_CALL(test_->session(), GetTransportStats(_)).WillRepeatedly(Invoke(
      [this](SessionStats* stats) {
        stats->transport_stats["transport"].transport_name = "transport";
        return true;
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
  ExpectReportContainsCertificateInfo(report, *local_certinfo.get());
  ExpectReportContainsCertificateInfo(report, *remote_certinfo.get());
}

TEST_F(RTCStatsCollectorTest, CollectRTCDataChannelStats) {
  test_->data_channels().push_back(
      new MockDataChannel(0, DataChannelInterface::kConnecting));
  test_->data_channels().push_back(
      new MockDataChannel(1, DataChannelInterface::kOpen));
  test_->data_channels().push_back(
      new MockDataChannel(2, DataChannelInterface::kClosing));
  test_->data_channels().push_back(
      new MockDataChannel(3, DataChannelInterface::kClosed));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsDataChannel(report, *test_->data_channels()[0]);
  ExpectReportContainsDataChannel(report, *test_->data_channels()[1]);
  ExpectReportContainsDataChannel(report, *test_->data_channels()[2]);
  ExpectReportContainsDataChannel(report, *test_->data_channels()[3]);

  test_->data_channels().clear();
  test_->data_channels().push_back(
      new MockDataChannel(0, DataChannelInterface::kConnecting,
                          1, 2, 3, 4));
  test_->data_channels().push_back(
      new MockDataChannel(1, DataChannelInterface::kOpen,
                          5, 6, 7, 8));
  test_->data_channels().push_back(
      new MockDataChannel(2, DataChannelInterface::kClosing,
                          9, 10, 11, 12));
  test_->data_channels().push_back(
      new MockDataChannel(3, DataChannelInterface::kClosed,
                          13, 14, 15, 16));

  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
  ExpectReportContainsDataChannel(report, *test_->data_channels()[0]);
  ExpectReportContainsDataChannel(report, *test_->data_channels()[1]);
  ExpectReportContainsDataChannel(report, *test_->data_channels()[2]);
  ExpectReportContainsDataChannel(report, *test_->data_channels()[3]);
}

TEST_F(RTCStatsCollectorTest, CollectRTCIceCandidateStats) {
  // Candidates in the first transport stats.
  std::unique_ptr<cricket::Candidate> a_local_host = CreateFakeCandidate(
      "1.2.3.4", 5,
      "a_local_host's protocol",
      cricket::LOCAL_PORT_TYPE,
      0);
  std::unique_ptr<cricket::Candidate> a_remote_srflx = CreateFakeCandidate(
      "6.7.8.9", 10,
      "remote_srflx's protocol",
      cricket::STUN_PORT_TYPE,
      1);
  std::unique_ptr<cricket::Candidate> a_local_prflx = CreateFakeCandidate(
      "11.12.13.14", 15,
      "a_local_prflx's protocol",
      cricket::PRFLX_PORT_TYPE,
      2);
  std::unique_ptr<cricket::Candidate> a_remote_relay = CreateFakeCandidate(
      "16.17.18.19", 20,
      "a_remote_relay's protocol",
      cricket::RELAY_PORT_TYPE,
      3);
  // Candidates in the second transport stats.
  std::unique_ptr<cricket::Candidate> b_local = CreateFakeCandidate(
      "42.42.42.42", 42,
      "b_local's protocol",
      cricket::LOCAL_PORT_TYPE,
      42);
  std::unique_ptr<cricket::Candidate> b_remote = CreateFakeCandidate(
      "42.42.42.42", 42,
      "b_remote's protocol",
      cricket::LOCAL_PORT_TYPE,
      42);

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
  session_stats.transport_stats["a"].channel_stats.push_back(
      a_transport_channel_stats);

  cricket::TransportChannelStats b_transport_channel_stats;
  b_transport_channel_stats.connection_infos.push_back(
      cricket::ConnectionInfo());
  b_transport_channel_stats.connection_infos[0].local_candidate =
      *b_local.get();
  b_transport_channel_stats.connection_infos[0].remote_candidate =
      *b_remote.get();
  session_stats.transport_stats["b"].channel_stats.push_back(
      b_transport_channel_stats);

  // Mock the session to return the desired candidates.
  EXPECT_CALL(test_->session(), GetTransportStats(_)).WillRepeatedly(Invoke(
      [this, &session_stats](SessionStats* stats) {
        *stats = session_stats;
        return true;
      }));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsCandidate(report, *a_local_host.get(), true);
  ExpectReportContainsCandidate(report, *a_remote_srflx.get(), false);
  ExpectReportContainsCandidate(report, *a_local_prflx.get(), true);
  ExpectReportContainsCandidate(report, *a_remote_relay.get(), false);
  ExpectReportContainsCandidate(report, *b_local.get(), true);
  ExpectReportContainsCandidate(report, *b_remote.get(), false);
}

TEST_F(RTCStatsCollectorTest, CollectRTCIceCandidatePairStats) {
  std::unique_ptr<cricket::Candidate> local_candidate = CreateFakeCandidate(
      "42.42.42.42", 42, "protocol", cricket::LOCAL_PORT_TYPE, 42);
  std::unique_ptr<cricket::Candidate> remote_candidate = CreateFakeCandidate(
      "42.42.42.42", 42, "protocol", cricket::LOCAL_PORT_TYPE, 42);

  SessionStats session_stats;

  cricket::ConnectionInfo connection_info;
  connection_info.local_candidate = *local_candidate.get();
  connection_info.remote_candidate = *remote_candidate.get();
  connection_info.writable = true;
  connection_info.sent_total_bytes = 42;
  connection_info.recv_total_bytes = 1234;
  connection_info.rtt = 1337;
  connection_info.sent_ping_requests_total = 1010;
  connection_info.recv_ping_responses = 4321;
  connection_info.sent_ping_responses = 1000;

  cricket::TransportChannelStats transport_channel_stats;
  transport_channel_stats.connection_infos.push_back(connection_info);
  session_stats.transport_stats["transport"].transport_name = "transport";
  session_stats.transport_stats["transport"].channel_stats.push_back(
      transport_channel_stats);

  // Mock the session to return the desired candidates.
  EXPECT_CALL(test_->session(), GetTransportStats(_)).WillRepeatedly(Invoke(
      [this, &session_stats](SessionStats* stats) {
        *stats = session_stats;
        return true;
      }));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsCandidatePair(
      report, session_stats.transport_stats["transport"]);
}

TEST_F(RTCStatsCollectorTest, CollectRTCPeerConnectionStats) {
  {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
    RTCPeerConnectionStats expected("RTCPeerConnection",
                                    report->timestamp_us());
    expected.data_channels_opened = 0;
    expected.data_channels_closed = 0;
    EXPECT_TRUE(report->Get("RTCPeerConnection"));
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
    EXPECT_TRUE(report->Get("RTCPeerConnection"));
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
    EXPECT_TRUE(report->Get("RTCPeerConnection"));
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
  AudioProcessorInterface::AudioProcessorStats local_audio_processor_stats;
  local_audio_processor_stats.echo_return_loss = 42;
  local_audio_processor_stats.echo_return_loss_enhancement = 52;
  rtc::scoped_refptr<FakeAudioTrackForStats> local_audio_track =
      FakeAudioTrackForStats::Create(
          "LocalAudioTrackID",
          MediaStreamTrackInterface::TrackState::kEnded,
          32767,
          new FakeAudioProcessorForStats(local_audio_processor_stats));
  local_stream->AddTrack(local_audio_track);

  // Remote audio track
  AudioProcessorInterface::AudioProcessorStats remote_audio_processor_stats;
  remote_audio_processor_stats.echo_return_loss = 13;
  remote_audio_processor_stats.echo_return_loss_enhancement = 37;
  rtc::scoped_refptr<FakeAudioTrackForStats> remote_audio_track =
      FakeAudioTrackForStats::Create(
          "RemoteAudioTrackID",
          MediaStreamTrackInterface::TrackState::kLive,
          0,
          new FakeAudioProcessorForStats(remote_audio_processor_stats));
  remote_stream->AddTrack(remote_audio_track);

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCMediaStreamStats expected_local_stream(
      "RTCMediaStream_LocalStreamLabel", report->timestamp_us());
  expected_local_stream.stream_identifier = local_stream->label();
  expected_local_stream.track_ids = std::vector<std::string>();
  expected_local_stream.track_ids->push_back(
      "RTCMediaStreamTrack_LocalAudioTrackID");
  EXPECT_TRUE(report->Get(expected_local_stream.id()));
  EXPECT_EQ(expected_local_stream,
            report->Get(expected_local_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamStats expected_remote_stream(
      "RTCMediaStream_RemoteStreamLabel", report->timestamp_us());
  expected_remote_stream.stream_identifier = remote_stream->label();
  expected_remote_stream.track_ids = std::vector<std::string>();
  expected_remote_stream.track_ids->push_back(
      "RTCMediaStreamTrack_RemoteAudioTrackID");
  EXPECT_TRUE(report->Get(expected_remote_stream.id()));
  EXPECT_EQ(expected_remote_stream,
            report->Get(expected_remote_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_local_audio_track(
      "RTCMediaStreamTrack_LocalAudioTrackID", report->timestamp_us());
  expected_local_audio_track.track_identifier = local_audio_track->id();
  expected_local_audio_track.remote_source = false;
  expected_local_audio_track.ended = true;
  expected_local_audio_track.detached = false;
  expected_local_audio_track.audio_level = 1.0;
  expected_local_audio_track.echo_return_loss = 42.0;
  expected_local_audio_track.echo_return_loss_enhancement = 52.0;
  EXPECT_TRUE(report->Get(expected_local_audio_track.id()));
  EXPECT_EQ(expected_local_audio_track,
            report->Get(expected_local_audio_track.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_remote_audio_track(
      "RTCMediaStreamTrack_RemoteAudioTrackID", report->timestamp_us());
  expected_remote_audio_track.track_identifier = remote_audio_track->id();
  expected_remote_audio_track.remote_source = true;
  expected_remote_audio_track.ended = false;
  expected_remote_audio_track.detached = false;
  expected_remote_audio_track.audio_level = 0.0;
  expected_remote_audio_track.echo_return_loss = 13.0;
  expected_remote_audio_track.echo_return_loss_enhancement = 37.0;
  EXPECT_TRUE(report->Get(expected_remote_audio_track.id()));
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
  VideoTrackSourceInterface::Stats local_video_track_source_stats;
  local_video_track_source_stats.input_width = 1234;
  local_video_track_source_stats.input_height = 4321;
  rtc::scoped_refptr<FakeVideoTrackSourceForStats> local_video_track_source =
      new FakeVideoTrackSourceForStats(local_video_track_source_stats);
  rtc::scoped_refptr<FakeVideoTrackForStats> local_video_track =
      FakeVideoTrackForStats::Create(
          "LocalVideoTrackID",
          MediaStreamTrackInterface::TrackState::kLive,
          local_video_track_source);
  local_stream->AddTrack(local_video_track);

  // Remote video track
  VideoTrackSourceInterface::Stats remote_video_track_source_stats;
  remote_video_track_source_stats.input_width = 1234;
  remote_video_track_source_stats.input_height = 4321;
  rtc::scoped_refptr<FakeVideoTrackSourceForStats> remote_video_track_source =
      new FakeVideoTrackSourceForStats(remote_video_track_source_stats);
  rtc::scoped_refptr<FakeVideoTrackForStats> remote_video_track =
      FakeVideoTrackForStats::Create(
          "RemoteVideoTrackID",
          MediaStreamTrackInterface::TrackState::kEnded,
          remote_video_track_source);
  remote_stream->AddTrack(remote_video_track);

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCMediaStreamStats expected_local_stream(
      "RTCMediaStream_LocalStreamLabel", report->timestamp_us());
  expected_local_stream.stream_identifier = local_stream->label();
  expected_local_stream.track_ids = std::vector<std::string>();
  expected_local_stream.track_ids->push_back(
      "RTCMediaStreamTrack_LocalVideoTrackID");
  EXPECT_TRUE(report->Get(expected_local_stream.id()));
  EXPECT_EQ(expected_local_stream,
            report->Get(expected_local_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamStats expected_remote_stream(
      "RTCMediaStream_RemoteStreamLabel", report->timestamp_us());
  expected_remote_stream.stream_identifier = remote_stream->label();
  expected_remote_stream.track_ids = std::vector<std::string>();
  expected_remote_stream.track_ids->push_back(
      "RTCMediaStreamTrack_RemoteVideoTrackID");
  EXPECT_TRUE(report->Get(expected_remote_stream.id()));
  EXPECT_EQ(expected_remote_stream,
            report->Get(expected_remote_stream.id())->cast_to<
                RTCMediaStreamStats>());

  RTCMediaStreamTrackStats expected_local_video_track(
      "RTCMediaStreamTrack_LocalVideoTrackID", report->timestamp_us());
  expected_local_video_track.track_identifier = local_video_track->id();
  expected_local_video_track.remote_source = false;
  expected_local_video_track.ended = false;
  expected_local_video_track.detached = false;
  expected_local_video_track.frame_width = 1234;
  expected_local_video_track.frame_height = 4321;
  EXPECT_TRUE(report->Get(expected_local_video_track.id()));
  EXPECT_EQ(expected_local_video_track,
            report->Get(expected_local_video_track.id())->cast_to<
                RTCMediaStreamTrackStats>());

  RTCMediaStreamTrackStats expected_remote_video_track(
      "RTCMediaStreamTrack_RemoteVideoTrackID", report->timestamp_us());
  expected_remote_video_track.track_identifier = remote_video_track->id();
  expected_remote_video_track.remote_source = true;
  expected_remote_video_track.ended = true;
  expected_remote_video_track.detached = false;
  expected_remote_video_track.frame_width = 1234;
  expected_remote_video_track.frame_height = 4321;
  EXPECT_TRUE(report->Get(expected_remote_video_track.id()));
  EXPECT_EQ(expected_remote_video_track,
            report->Get(expected_remote_video_track.id())->cast_to<
                RTCMediaStreamTrackStats>());
}

TEST_F(RTCStatsCollectorTest, CollectRTCInboundRTPStreamStats_Audio) {
  MockVoiceMediaChannel* voice_media_channel = new MockVoiceMediaChannel();
  cricket::VoiceChannel voice_channel(
      test_->worker_thread(), test_->network_thread(), test_->media_engine(),
      voice_media_channel, nullptr, "VoiceContentName", false);

  cricket::VoiceMediaInfo voice_media_info;
  voice_media_info.receivers.push_back(cricket::VoiceReceiverInfo());
  voice_media_info.receivers[0].local_stats.push_back(
      cricket::SsrcReceiverInfo());
  voice_media_info.receivers[0].local_stats[0].ssrc = 1;
  voice_media_info.receivers[0].packets_rcvd = 2;
  voice_media_info.receivers[0].bytes_rcvd = 3;
  voice_media_info.receivers[0].jitter_ms = 4500;
  voice_media_info.receivers[0].fraction_lost = 5.5f;
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

  EXPECT_CALL(test_->session(), GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats), Return(true)));
  EXPECT_CALL(test_->session(), voice_channel())
      .WillRepeatedly(Return(&voice_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCInboundRTPStreamStats expected_audio(
      "RTCInboundRTPAudioStream_1", report->timestamp_us());
  expected_audio.ssrc = "1";
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_audio.packets_received = 2;
  expected_audio.bytes_received = 3;
  expected_audio.jitter = 4.5;
  expected_audio.fraction_lost = 5.5;

  ASSERT(report->Get(expected_audio.id()));
  const RTCInboundRTPStreamStats& audio = report->Get(
      expected_audio.id())->cast_to<RTCInboundRTPStreamStats>();
  EXPECT_EQ(audio, expected_audio);

  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCInboundRTPStreamStats_Video) {
  MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      test_->worker_thread(), test_->network_thread(), video_media_channel,
      nullptr, "VideoContentName", false);

  cricket::VideoMediaInfo video_media_info;
  video_media_info.receivers.push_back(cricket::VideoReceiverInfo());
  video_media_info.receivers[0].local_stats.push_back(
      cricket::SsrcReceiverInfo());
  video_media_info.receivers[0].local_stats[0].ssrc = 1;
  video_media_info.receivers[0].packets_rcvd = 2;
  video_media_info.receivers[0].bytes_rcvd = 3;
  video_media_info.receivers[0].fraction_lost = 4.5f;
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

  EXPECT_CALL(test_->session(), GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats), Return(true)));
  EXPECT_CALL(test_->session(), video_channel())
      .WillRepeatedly(Return(&video_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCInboundRTPStreamStats expected_audio(
      "RTCInboundRTPVideoStream_1", report->timestamp_us());
  expected_audio.ssrc = "1";
  expected_audio.is_remote = false;
  expected_audio.media_type = "video";
  expected_audio.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_audio.packets_received = 2;
  expected_audio.bytes_received = 3;
  expected_audio.fraction_lost = 4.5;

  ASSERT(report->Get(expected_audio.id()));
  const RTCInboundRTPStreamStats& audio = report->Get(
      expected_audio.id())->cast_to<RTCInboundRTPStreamStats>();
  EXPECT_EQ(audio, expected_audio);

  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCOutboundRTPStreamStats_Audio) {
  MockVoiceMediaChannel* voice_media_channel = new MockVoiceMediaChannel();
  cricket::VoiceChannel voice_channel(
      test_->worker_thread(), test_->network_thread(), test_->media_engine(),
      voice_media_channel, nullptr, "VoiceContentName", false);

  cricket::VoiceMediaInfo voice_media_info;
  voice_media_info.senders.push_back(cricket::VoiceSenderInfo());
  voice_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  voice_media_info.senders[0].local_stats[0].ssrc = 1;
  voice_media_info.senders[0].packets_sent = 2;
  voice_media_info.senders[0].bytes_sent = 3;
  voice_media_info.senders[0].rtt_ms = 4500;
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

  EXPECT_CALL(test_->session(), GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats), Return(true)));
  EXPECT_CALL(test_->session(), voice_channel())
      .WillRepeatedly(Return(&voice_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCOutboundRTPStreamStats expected_audio(
      "RTCOutboundRTPAudioStream_1", report->timestamp_us());
  expected_audio.ssrc = "1";
  expected_audio.is_remote = false;
  expected_audio.media_type = "audio";
  expected_audio.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_audio.packets_sent = 2;
  expected_audio.bytes_sent = 3;
  expected_audio.round_trip_time = 4.5;

  ASSERT(report->Get(expected_audio.id()));
  const RTCOutboundRTPStreamStats& audio = report->Get(
      expected_audio.id())->cast_to<RTCOutboundRTPStreamStats>();
  EXPECT_EQ(audio, expected_audio);

  EXPECT_TRUE(report->Get(*expected_audio.transport_id));
}

TEST_F(RTCStatsCollectorTest, CollectRTCOutboundRTPStreamStats_Video) {
  MockVideoMediaChannel* video_media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      test_->worker_thread(), test_->network_thread(), video_media_channel,
      nullptr, "VideoContentName", false);

  cricket::VideoMediaInfo video_media_info;
  video_media_info.senders.push_back(cricket::VideoSenderInfo());
  video_media_info.senders[0].local_stats.push_back(cricket::SsrcSenderInfo());
  video_media_info.senders[0].local_stats[0].ssrc = 1;
  video_media_info.senders[0].firs_rcvd = 2;
  video_media_info.senders[0].plis_rcvd = 3;
  video_media_info.senders[0].nacks_rcvd = 4;
  video_media_info.senders[0].packets_sent = 5;
  video_media_info.senders[0].bytes_sent = 6;
  video_media_info.senders[0].rtt_ms = 7500;
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

  EXPECT_CALL(test_->session(), GetTransportStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats), Return(true)));
  EXPECT_CALL(test_->session(), video_channel())
      .WillRepeatedly(Return(&video_channel));

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();

  RTCOutboundRTPStreamStats expected_video(
      "RTCOutboundRTPVideoStream_1", report->timestamp_us());
  expected_video.ssrc = "1";
  expected_video.is_remote = false;
  expected_video.media_type = "video";
  expected_video.transport_id = "RTCTransport_TransportName_" +
      rtc::ToString<>(cricket::ICE_CANDIDATE_COMPONENT_RTP);
  expected_video.fir_count = 2;
  expected_video.pli_count = 3;
  expected_video.nack_count = 4;
  expected_video.packets_sent = 5;
  expected_video.bytes_sent = 6;
  expected_video.round_trip_time = 7.5;

  ASSERT(report->Get(expected_video.id()));
  const RTCOutboundRTPStreamStats& video = report->Get(
      expected_video.id())->cast_to<RTCOutboundRTPStreamStats>();
  EXPECT_EQ(video, expected_video);

  EXPECT_TRUE(report->Get(*expected_video.transport_id));
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
  session_stats.transport_stats["transport"].channel_stats.push_back(
      rtp_transport_channel_stats);


  // Mock the session to return the desired candidates.
  EXPECT_CALL(test_->session(), GetTransportStats(_)).WillRepeatedly(Invoke(
      [this, &session_stats](SessionStats* stats) {
        *stats = session_stats;
        return true;
      }));

  // Get stats without RTCP, an active connection or certificates.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  ExpectReportContainsTransportStats(
      report, session_stats.transport_stats["transport"], nullptr, nullptr);

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
  session_stats.transport_stats["transport"].channel_stats.push_back(
      rtcp_transport_channel_stats);

  collector_->ClearCachedStatsReport();
  // Get stats with RTCP and without an active connection or certificates.
  report = GetStatsReport();
  ExpectReportContainsTransportStats(
      report, session_stats.transport_stats["transport"], nullptr, nullptr);

  // Get stats with an active connection.
  rtcp_connection_info.best_connection = true;

  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
  ExpectReportContainsTransportStats(
      report, session_stats.transport_stats["transport"], nullptr, nullptr);

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
  ExpectReportContainsTransportStats(
      report, session_stats.transport_stats["transport"],
      local_certinfo.get(), remote_certinfo.get());
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
