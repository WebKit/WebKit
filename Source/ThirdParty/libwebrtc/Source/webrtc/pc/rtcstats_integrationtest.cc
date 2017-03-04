/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <set>
#include <vector>

#include "webrtc/api/datachannelinterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/stats/rtcstats_objects.h"
#include "webrtc/api/stats/rtcstatsreport.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/refcountedobject.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/pc/test/peerconnectiontestwrapper.h"
#include "webrtc/pc/test/rtcstatsobtainer.h"

namespace webrtc {

namespace {

const int64_t kGetStatsTimeoutMs = 10000;

class RTCStatsIntegrationTest : public testing::Test {
 public:
  RTCStatsIntegrationTest()
      : physical_socket_server_(),
        virtual_socket_server_(&physical_socket_server_),
        network_thread_(&virtual_socket_server_),
        worker_thread_() {
    RTC_CHECK(network_thread_.Start());
    RTC_CHECK(worker_thread_.Start());

    caller_ = new rtc::RefCountedObject<PeerConnectionTestWrapper>(
        "caller", &network_thread_, &worker_thread_);
    callee_ = new rtc::RefCountedObject<PeerConnectionTestWrapper>(
        "callee", &network_thread_, &worker_thread_);
  }

  void StartCall() {
    // Create PeerConnections and "connect" sigslots
    PeerConnectionInterface::RTCConfiguration config;
    PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:1.1.1.1:3478";
    config.servers.push_back(ice_server);
    EXPECT_TRUE(caller_->CreatePc(nullptr, config));
    EXPECT_TRUE(callee_->CreatePc(nullptr, config));
    PeerConnectionTestWrapper::Connect(caller_.get(), callee_.get());

    // Get user media for audio and video
    caller_->GetAndAddUserMedia(true, FakeConstraints(),
                                true, FakeConstraints());
    callee_->GetAndAddUserMedia(true, FakeConstraints(),
                                true, FakeConstraints());

    // Create data channels
    DataChannelInit init;
    caller_->CreateDataChannel("data", init);
    callee_->CreateDataChannel("data", init);

    // Negotiate and wait for call to establish
    caller_->CreateOffer(nullptr);
    caller_->WaitForCallEstablished();
    callee_->WaitForCallEstablished();
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsFromCaller() {
    return GetStats(caller_->pc());
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsFromCallee() {
    return GetStats(callee_->pc());
  }

 protected:
  static rtc::scoped_refptr<const RTCStatsReport> GetStats(
      PeerConnectionInterface* pc) {
    rtc::scoped_refptr<RTCStatsObtainer> stats_obtainer =
        RTCStatsObtainer::Create();
    pc->GetStats(stats_obtainer);
    EXPECT_TRUE_WAIT(stats_obtainer->report(), kGetStatsTimeoutMs);
    return stats_obtainer->report();
  }

  // These objects use each other and must be constructed/destroyed in this
  // order. Relationship:
  // |physical_socket_server_| <- |virtual_socket_server_| <- |network_thread_|
  rtc::PhysicalSocketServer physical_socket_server_;
  rtc::VirtualSocketServer virtual_socket_server_;
  rtc::Thread network_thread_;
  rtc::Thread worker_thread_;
  rtc::scoped_refptr<PeerConnectionTestWrapper> caller_;
  rtc::scoped_refptr<PeerConnectionTestWrapper> callee_;
};

class RTCStatsVerifier {
 public:
  RTCStatsVerifier(const RTCStatsReport* report, const RTCStats* stats)
      : report_(report), stats_(stats), all_tests_successful_(true) {
    RTC_CHECK(report_);
    RTC_CHECK(stats_);
    for (const RTCStatsMemberInterface* member : stats_->Members()) {
      untested_members_.insert(member);
    }
  }

  void MarkMemberTested(
      const RTCStatsMemberInterface& member, bool test_successful) {
    untested_members_.erase(&member);
    all_tests_successful_ &= test_successful;
  }

  void TestMemberIsDefined(const RTCStatsMemberInterface& member) {
    EXPECT_TRUE(member.is_defined()) <<
        stats_->type() << "." << member.name() << "[" << stats_->id() <<
        "] was undefined.";
    MarkMemberTested(member, member.is_defined());
  }

  void TestMemberIsUndefined(const RTCStatsMemberInterface& member) {
    EXPECT_FALSE(member.is_defined()) <<
        stats_->type() << "." << member.name() << "[" << stats_->id() <<
        "] was defined (" << member.ValueToString() << ").";
    MarkMemberTested(member, !member.is_defined());
  }

  template<typename T>
  void TestMemberIsPositive(const RTCStatsMemberInterface& member) {
    EXPECT_TRUE(member.is_defined()) <<
        stats_->type() << "." << member.name() << "[" << stats_->id() <<
        "] was undefined.";
    if (!member.is_defined()) {
      MarkMemberTested(member, false);
      return;
    }
    bool is_positive = *member.cast_to<RTCStatsMember<T>>() > T(0);
    EXPECT_TRUE(is_positive) <<
        stats_->type() << "." << member.name() << "[" << stats_->id() <<
        "] was not positive (" << member.ValueToString() << ").";
    MarkMemberTested(member, is_positive);
  }

  template<typename T>
  void TestMemberIsNonNegative(const RTCStatsMemberInterface& member) {
    EXPECT_TRUE(member.is_defined()) <<
        stats_->type() << "." << member.name() << "[" << stats_->id() <<
        "] was undefined.";
    if (!member.is_defined()) {
      MarkMemberTested(member, false);
      return;
    }
    bool is_non_negative = *member.cast_to<RTCStatsMember<T>>() >= T(0);
    EXPECT_TRUE(is_non_negative) <<
        stats_->type() << "." << member.name() << "[" << stats_->id() <<
        "] was not non-negative (" << member.ValueToString() << ").";
    MarkMemberTested(member, is_non_negative);
  }

  void TestMemberIsIDReference(
      const RTCStatsMemberInterface& member,
      const char* expected_type) {
    TestMemberIsIDReference(member, expected_type, false);
  }

  void TestMemberIsOptionalIDReference(
      const RTCStatsMemberInterface& member,
      const char* expected_type) {
    TestMemberIsIDReference(member, expected_type, true);
  }

  bool ExpectAllMembersSuccessfullyTested() {
    if (untested_members_.empty())
      return all_tests_successful_;
    for (const RTCStatsMemberInterface* member : untested_members_) {
      EXPECT_TRUE(false) <<
          stats_->type() << "." << member->name() << "[" << stats_->id() <<
          "] was not tested.";
    }
    return false;
  }

 private:
  void TestMemberIsIDReference(
      const RTCStatsMemberInterface& member,
      const char* expected_type,
      bool optional) {
    if (optional && !member.is_defined()) {
      MarkMemberTested(member, true);
      return;
    }
    bool valid_reference = false;
    if (member.is_defined()) {
      if (member.type() == RTCStatsMemberInterface::kString) {
        // A single ID.
        const RTCStatsMember<std::string>& id =
            member.cast_to<RTCStatsMember<std::string>>();
        const RTCStats* referenced_stats = report_->Get(*id);
        valid_reference =
            referenced_stats && referenced_stats->type() == expected_type;
      } else if (member.type() == RTCStatsMemberInterface::kSequenceString) {
        // A vector of IDs.
        valid_reference = true;
        const RTCStatsMember<std::vector<std::string>>& ids =
            member.cast_to<RTCStatsMember<std::vector<std::string>>>();
        for (const std::string id : *ids) {
          const RTCStats* referenced_stats = report_->Get(id);
          if (!referenced_stats || referenced_stats->type() != expected_type) {
            valid_reference = false;
            break;
          }
        }
      }
    }
    EXPECT_TRUE(valid_reference) <<
      stats_->type() << "." << member.name() << " is not a reference to an " <<
      "existing dictionary of type " << expected_type << " (" <<
      member.ValueToString() << ").";
    MarkMemberTested(member, valid_reference);
  }

  rtc::scoped_refptr<const RTCStatsReport> report_;
  const RTCStats* stats_;
  std::set<const RTCStatsMemberInterface*> untested_members_;
  bool all_tests_successful_;
};

class RTCStatsReportVerifier {
 public:
  static std::set<const char*> StatsTypes() {
    std::set<const char*> stats_types;
    stats_types.insert(RTCCertificateStats::kType);
    stats_types.insert(RTCCodecStats::kType);
    stats_types.insert(RTCDataChannelStats::kType);
    stats_types.insert(RTCIceCandidatePairStats::kType);
    stats_types.insert(RTCLocalIceCandidateStats::kType);
    stats_types.insert(RTCRemoteIceCandidateStats::kType);
    stats_types.insert(RTCMediaStreamStats::kType);
    stats_types.insert(RTCMediaStreamTrackStats::kType);
    stats_types.insert(RTCPeerConnectionStats::kType);
    stats_types.insert(RTCInboundRTPStreamStats::kType);
    stats_types.insert(RTCOutboundRTPStreamStats::kType);
    stats_types.insert(RTCTransportStats::kType);
    return stats_types;
  }

  explicit RTCStatsReportVerifier(const RTCStatsReport* report)
      : report_(report) {
  }

  void VerifyReport() {
    std::set<const char*> missing_stats = StatsTypes();
    bool verify_successful = true;
    std::vector<const RTCTransportStats*> transport_stats =
        report_->GetStatsOfType<RTCTransportStats>();
    EXPECT_EQ(transport_stats.size(), 1);
    std::string selected_candidate_pair_id =
        *transport_stats[0]->selected_candidate_pair_id;
    for (const RTCStats& stats : *report_) {
      missing_stats.erase(stats.type());
      if (stats.type() == RTCCertificateStats::kType) {
        verify_successful &= VerifyRTCCertificateStats(
            stats.cast_to<RTCCertificateStats>());
      } else if (stats.type() == RTCCodecStats::kType) {
        verify_successful &= VerifyRTCCodecStats(
            stats.cast_to<RTCCodecStats>());
      } else if (stats.type() == RTCDataChannelStats::kType) {
        verify_successful &= VerifyRTCDataChannelStats(
            stats.cast_to<RTCDataChannelStats>());
      } else if (stats.type() == RTCIceCandidatePairStats::kType) {
        verify_successful &= VerifyRTCIceCandidatePairStats(
            stats.cast_to<RTCIceCandidatePairStats>(),
            stats.id() == selected_candidate_pair_id);
      } else if (stats.type() == RTCLocalIceCandidateStats::kType) {
        verify_successful &= VerifyRTCLocalIceCandidateStats(
            stats.cast_to<RTCLocalIceCandidateStats>());
      } else if (stats.type() == RTCRemoteIceCandidateStats::kType) {
        verify_successful &= VerifyRTCRemoteIceCandidateStats(
            stats.cast_to<RTCRemoteIceCandidateStats>());
      } else if (stats.type() == RTCMediaStreamStats::kType) {
        verify_successful &= VerifyRTCMediaStreamStats(
            stats.cast_to<RTCMediaStreamStats>());
      } else if (stats.type() == RTCMediaStreamTrackStats::kType) {
        verify_successful &= VerifyRTCMediaStreamTrackStats(
            stats.cast_to<RTCMediaStreamTrackStats>());
      } else if (stats.type() == RTCPeerConnectionStats::kType) {
        verify_successful &= VerifyRTCPeerConnectionStats(
            stats.cast_to<RTCPeerConnectionStats>());
      } else if (stats.type() == RTCInboundRTPStreamStats::kType) {
        verify_successful &= VerifyRTCInboundRTPStreamStats(
            stats.cast_to<RTCInboundRTPStreamStats>());
      } else if (stats.type() == RTCOutboundRTPStreamStats::kType) {
        verify_successful &= VerifyRTCOutboundRTPStreamStats(
            stats.cast_to<RTCOutboundRTPStreamStats>());
      } else if (stats.type() == RTCTransportStats::kType) {
        verify_successful &= VerifyRTCTransportStats(
            stats.cast_to<RTCTransportStats>());
      } else {
        EXPECT_TRUE(false) << "Unrecognized stats type: " << stats.type();
        verify_successful = false;
      }
    }
    if (!missing_stats.empty()) {
      verify_successful = false;
      for (const char* missing : missing_stats) {
        EXPECT_TRUE(false) << "Missing expected stats type: " << missing;
      }
    }
    EXPECT_TRUE(verify_successful) <<
        "One or more problems with the stats. This is the report:\n" <<
        report_->ToString();
  }

  bool VerifyRTCCertificateStats(
      const RTCCertificateStats& certificate) {
    RTCStatsVerifier verifier(report_, &certificate);
    verifier.TestMemberIsDefined(certificate.fingerprint);
    verifier.TestMemberIsDefined(certificate.fingerprint_algorithm);
    verifier.TestMemberIsDefined(certificate.base64_certificate);
    verifier.TestMemberIsOptionalIDReference(
        certificate.issuer_certificate_id, RTCCertificateStats::kType);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCCodecStats(
      const RTCCodecStats& codec) {
    RTCStatsVerifier verifier(report_, &codec);
    verifier.TestMemberIsDefined(codec.payload_type);
    verifier.TestMemberIsDefined(codec.mime_type);
    verifier.TestMemberIsPositive<uint32_t>(codec.clock_rate);
    verifier.TestMemberIsUndefined(codec.channels);
    verifier.TestMemberIsUndefined(codec.sdp_fmtp_line);
    verifier.TestMemberIsUndefined(codec.implementation);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCDataChannelStats(
      const RTCDataChannelStats& data_channel) {
    RTCStatsVerifier verifier(report_, &data_channel);
    verifier.TestMemberIsDefined(data_channel.label);
    verifier.TestMemberIsDefined(data_channel.protocol);
    verifier.TestMemberIsDefined(data_channel.datachannelid);
    verifier.TestMemberIsDefined(data_channel.state);
    verifier.TestMemberIsNonNegative<uint32_t>(data_channel.messages_sent);
    verifier.TestMemberIsNonNegative<uint64_t>(data_channel.bytes_sent);
    verifier.TestMemberIsNonNegative<uint32_t>(data_channel.messages_received);
    verifier.TestMemberIsNonNegative<uint64_t>(data_channel.bytes_received);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCIceCandidatePairStats(
      const RTCIceCandidatePairStats& candidate_pair, bool is_selected_pair) {
    RTCStatsVerifier verifier(report_, &candidate_pair);
    verifier.TestMemberIsIDReference(
        candidate_pair.transport_id, RTCTransportStats::kType);
    verifier.TestMemberIsIDReference(
        candidate_pair.local_candidate_id, RTCLocalIceCandidateStats::kType);
    verifier.TestMemberIsIDReference(
        candidate_pair.remote_candidate_id, RTCRemoteIceCandidateStats::kType);
    verifier.TestMemberIsDefined(candidate_pair.state);
    verifier.TestMemberIsNonNegative<uint64_t>(candidate_pair.priority);
    verifier.TestMemberIsDefined(candidate_pair.nominated);
    verifier.TestMemberIsDefined(candidate_pair.writable);
    verifier.TestMemberIsUndefined(candidate_pair.readable);
    verifier.TestMemberIsNonNegative<uint64_t>(candidate_pair.bytes_sent);
    verifier.TestMemberIsNonNegative<uint64_t>(candidate_pair.bytes_received);
    verifier.TestMemberIsNonNegative<double>(
        candidate_pair.total_round_trip_time);
    verifier.TestMemberIsNonNegative<double>(
        candidate_pair.current_round_trip_time);
    if (is_selected_pair) {
      verifier.TestMemberIsNonNegative<double>(
          candidate_pair.available_outgoing_bitrate);
    } else {
      verifier.TestMemberIsUndefined(candidate_pair.available_outgoing_bitrate);
    }
    verifier.TestMemberIsUndefined(candidate_pair.available_incoming_bitrate);
    verifier.TestMemberIsNonNegative<uint64_t>(
        candidate_pair.requests_received);
    verifier.TestMemberIsNonNegative<uint64_t>(candidate_pair.requests_sent);
    verifier.TestMemberIsNonNegative<uint64_t>(
        candidate_pair.responses_received);
    verifier.TestMemberIsNonNegative<uint64_t>(candidate_pair.responses_sent);
    verifier.TestMemberIsUndefined(candidate_pair.retransmissions_received);
    verifier.TestMemberIsUndefined(candidate_pair.retransmissions_sent);
    verifier.TestMemberIsUndefined(candidate_pair.consent_requests_received);
    verifier.TestMemberIsNonNegative<uint64_t>(
        candidate_pair.consent_requests_sent);
    verifier.TestMemberIsUndefined(candidate_pair.consent_responses_received);
    verifier.TestMemberIsUndefined(candidate_pair.consent_responses_sent);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCIceCandidateStats(
      const RTCIceCandidateStats& candidate) {
    RTCStatsVerifier verifier(report_, &candidate);
    verifier.TestMemberIsIDReference(
        candidate.transport_id, RTCTransportStats::kType);
    verifier.TestMemberIsDefined(candidate.is_remote);
    verifier.TestMemberIsDefined(candidate.ip);
    verifier.TestMemberIsNonNegative<int32_t>(candidate.port);
    verifier.TestMemberIsDefined(candidate.protocol);
    verifier.TestMemberIsDefined(candidate.candidate_type);
    verifier.TestMemberIsNonNegative<int32_t>(candidate.priority);
    verifier.TestMemberIsUndefined(candidate.url);
    verifier.TestMemberIsDefined(candidate.deleted);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCLocalIceCandidateStats(
      const RTCLocalIceCandidateStats& local_candidate) {
    return VerifyRTCIceCandidateStats(local_candidate);
  }

  bool VerifyRTCRemoteIceCandidateStats(
      const RTCRemoteIceCandidateStats& remote_candidate) {
    return VerifyRTCIceCandidateStats(remote_candidate);
  }

  bool VerifyRTCMediaStreamStats(
      const RTCMediaStreamStats& media_stream) {
    RTCStatsVerifier verifier(report_, &media_stream);
    verifier.TestMemberIsDefined(media_stream.stream_identifier);
    verifier.TestMemberIsIDReference(
        media_stream.track_ids, RTCMediaStreamTrackStats::kType);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCMediaStreamTrackStats(
      const RTCMediaStreamTrackStats& media_stream_track) {
    RTCStatsVerifier verifier(report_, &media_stream_track);
    verifier.TestMemberIsDefined(media_stream_track.track_identifier);
    verifier.TestMemberIsDefined(media_stream_track.remote_source);
    verifier.TestMemberIsDefined(media_stream_track.ended);
    verifier.TestMemberIsDefined(media_stream_track.detached);
    verifier.TestMemberIsDefined(media_stream_track.kind);
    // Video or audio media stream track?
    if (*media_stream_track.kind == RTCMediaStreamTrackKind::kVideo) {
      // Video-only members
      verifier.TestMemberIsNonNegative<uint32_t>(
          media_stream_track.frame_width);
      verifier.TestMemberIsNonNegative<uint32_t>(
          media_stream_track.frame_height);
      verifier.TestMemberIsUndefined(media_stream_track.frames_per_second);
      if (*media_stream_track.remote_source) {
        verifier.TestMemberIsUndefined(media_stream_track.frames_sent);
        verifier.TestMemberIsNonNegative<uint32_t>(
            media_stream_track.frames_received);
        verifier.TestMemberIsNonNegative<uint32_t>(
            media_stream_track.frames_decoded);
        verifier.TestMemberIsNonNegative<uint32_t>(
            media_stream_track.frames_dropped);
      } else {
        verifier.TestMemberIsNonNegative<uint32_t>(
            media_stream_track.frames_sent);
        verifier.TestMemberIsUndefined(media_stream_track.frames_received);
        verifier.TestMemberIsUndefined(media_stream_track.frames_decoded);
        verifier.TestMemberIsUndefined(media_stream_track.frames_dropped);
      }
      verifier.TestMemberIsUndefined(media_stream_track.frames_corrupted);
      verifier.TestMemberIsUndefined(media_stream_track.partial_frames_lost);
      verifier.TestMemberIsUndefined(media_stream_track.full_frames_lost);
      // Audio-only members should be undefined
      verifier.TestMemberIsUndefined(media_stream_track.audio_level);
      verifier.TestMemberIsUndefined(media_stream_track.echo_return_loss);
      verifier.TestMemberIsUndefined(
          media_stream_track.echo_return_loss_enhancement);
    } else {
      RTC_DCHECK_EQ(*media_stream_track.kind,
                    RTCMediaStreamTrackKind::kAudio);
      // Video-only members should be undefined
      verifier.TestMemberIsUndefined(media_stream_track.frame_width);
      verifier.TestMemberIsUndefined(media_stream_track.frame_height);
      verifier.TestMemberIsUndefined(media_stream_track.frames_per_second);
      verifier.TestMemberIsUndefined(media_stream_track.frames_sent);
      verifier.TestMemberIsUndefined(media_stream_track.frames_received);
      verifier.TestMemberIsUndefined(media_stream_track.frames_decoded);
      verifier.TestMemberIsUndefined(media_stream_track.frames_dropped);
      verifier.TestMemberIsUndefined(media_stream_track.frames_corrupted);
      verifier.TestMemberIsUndefined(media_stream_track.partial_frames_lost);
      verifier.TestMemberIsUndefined(media_stream_track.full_frames_lost);
      // Audio-only members
      verifier.TestMemberIsNonNegative<double>(media_stream_track.audio_level);
      // TODO(hbos): |echo_return_loss| and |echo_return_loss_enhancement| are
      // flaky on msan bot (sometimes defined, sometimes undefined). Should the
      // test run until available or is there a way to have it always be
      // defined? crbug.com/627816
      verifier.MarkMemberTested(media_stream_track.echo_return_loss, true);
      verifier.MarkMemberTested(
          media_stream_track.echo_return_loss_enhancement, true);
    }
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCPeerConnectionStats(
      const RTCPeerConnectionStats& peer_connection) {
    RTCStatsVerifier verifier(report_, &peer_connection);
    verifier.TestMemberIsNonNegative<uint32_t>(
        peer_connection.data_channels_opened);
    verifier.TestMemberIsNonNegative<uint32_t>(
        peer_connection.data_channels_closed);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  void VerifyRTCRTPStreamStats(
      const RTCRTPStreamStats& stream, RTCStatsVerifier* verifier) {
    verifier->TestMemberIsDefined(stream.ssrc);
    verifier->TestMemberIsUndefined(stream.associate_stats_id);
    verifier->TestMemberIsDefined(stream.is_remote);
    verifier->TestMemberIsDefined(stream.media_type);
    verifier->TestMemberIsIDReference(
        stream.track_id, RTCMediaStreamTrackStats::kType);
    verifier->TestMemberIsIDReference(
        stream.transport_id, RTCTransportStats::kType);
    verifier->TestMemberIsIDReference(stream.codec_id, RTCCodecStats::kType);
    if (stream.media_type.is_defined() && *stream.media_type == "video") {
      verifier->TestMemberIsNonNegative<uint32_t>(stream.fir_count);
      verifier->TestMemberIsNonNegative<uint32_t>(stream.pli_count);
      verifier->TestMemberIsNonNegative<uint32_t>(stream.nack_count);
    } else {
      verifier->TestMemberIsUndefined(stream.fir_count);
      verifier->TestMemberIsUndefined(stream.pli_count);
      verifier->TestMemberIsUndefined(stream.nack_count);
    }
    verifier->TestMemberIsUndefined(stream.sli_count);
  }

  bool VerifyRTCInboundRTPStreamStats(
      const RTCInboundRTPStreamStats& inbound_stream) {
    RTCStatsVerifier verifier(report_, &inbound_stream);
    VerifyRTCRTPStreamStats(inbound_stream, &verifier);
    if (inbound_stream.media_type.is_defined() &&
        *inbound_stream.media_type == "video") {
      verifier.TestMemberIsNonNegative<uint64_t>(inbound_stream.qp_sum);
    } else {
      verifier.TestMemberIsUndefined(inbound_stream.qp_sum);
    }
    verifier.TestMemberIsNonNegative<uint32_t>(inbound_stream.packets_received);
    verifier.TestMemberIsNonNegative<uint64_t>(inbound_stream.bytes_received);
    verifier.TestMemberIsNonNegative<uint32_t>(inbound_stream.packets_lost);
    if (inbound_stream.media_type.is_defined() &&
        *inbound_stream.media_type == "video") {
      verifier.TestMemberIsUndefined(inbound_stream.jitter);
    } else {
      verifier.TestMemberIsNonNegative<double>(inbound_stream.jitter);
    }
    verifier.TestMemberIsNonNegative<double>(inbound_stream.fraction_lost);
    verifier.TestMemberIsUndefined(inbound_stream.round_trip_time);
    verifier.TestMemberIsUndefined(inbound_stream.packets_discarded);
    verifier.TestMemberIsUndefined(inbound_stream.packets_repaired);
    verifier.TestMemberIsUndefined(inbound_stream.burst_packets_lost);
    verifier.TestMemberIsUndefined(inbound_stream.burst_packets_discarded);
    verifier.TestMemberIsUndefined(inbound_stream.burst_loss_count);
    verifier.TestMemberIsUndefined(inbound_stream.burst_discard_count);
    verifier.TestMemberIsUndefined(inbound_stream.burst_loss_rate);
    verifier.TestMemberIsUndefined(inbound_stream.burst_discard_rate);
    verifier.TestMemberIsUndefined(inbound_stream.gap_loss_rate);
    verifier.TestMemberIsUndefined(inbound_stream.gap_discard_rate);
    if (inbound_stream.media_type.is_defined() &&
        *inbound_stream.media_type == "video") {
      verifier.TestMemberIsDefined(inbound_stream.frames_decoded);
    } else {
      verifier.TestMemberIsUndefined(inbound_stream.frames_decoded);
    }
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCOutboundRTPStreamStats(
      const RTCOutboundRTPStreamStats& outbound_stream) {
    RTCStatsVerifier verifier(report_, &outbound_stream);
    VerifyRTCRTPStreamStats(outbound_stream, &verifier);
    if (outbound_stream.media_type.is_defined() &&
        *outbound_stream.media_type == "video") {
      verifier.TestMemberIsNonNegative<uint64_t>(outbound_stream.qp_sum);
    } else {
      verifier.TestMemberIsUndefined(outbound_stream.qp_sum);
    }
    verifier.TestMemberIsNonNegative<uint32_t>(outbound_stream.packets_sent);
    verifier.TestMemberIsNonNegative<uint64_t>(outbound_stream.bytes_sent);
    verifier.TestMemberIsUndefined(outbound_stream.target_bitrate);
    if (outbound_stream.media_type.is_defined() &&
        *outbound_stream.media_type == "video") {
      verifier.TestMemberIsDefined(outbound_stream.frames_encoded);
    } else {
      verifier.TestMemberIsUndefined(outbound_stream.frames_encoded);
    }
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

  bool VerifyRTCTransportStats(
      const RTCTransportStats& transport) {
    RTCStatsVerifier verifier(report_, &transport);
    verifier.TestMemberIsNonNegative<uint64_t>(transport.bytes_sent);
    verifier.TestMemberIsNonNegative<uint64_t>(transport.bytes_received);
    verifier.TestMemberIsOptionalIDReference(
        transport.rtcp_transport_stats_id, RTCTransportStats::kType);
    verifier.TestMemberIsDefined(transport.dtls_state);
    verifier.TestMemberIsIDReference(
        transport.selected_candidate_pair_id, RTCIceCandidatePairStats::kType);
    verifier.TestMemberIsIDReference(
        transport.local_certificate_id, RTCCertificateStats::kType);
    verifier.TestMemberIsIDReference(
        transport.remote_certificate_id, RTCCertificateStats::kType);
    return verifier.ExpectAllMembersSuccessfullyTested();
  }

 private:
  rtc::scoped_refptr<const RTCStatsReport> report_;
};

#ifdef HAVE_SCTP
TEST_F(RTCStatsIntegrationTest, GetStatsFromCaller) {
  StartCall();

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsFromCaller();
  RTCStatsReportVerifier(report.get()).VerifyReport();
}

TEST_F(RTCStatsIntegrationTest, GetStatsFromCallee) {
  StartCall();

  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsFromCallee();
  RTCStatsReportVerifier(report.get()).VerifyReport();
}

TEST_F(RTCStatsIntegrationTest, GetsStatsWhileDestroyingPeerConnections) {
  StartCall();

  rtc::scoped_refptr<RTCStatsObtainer> stats_obtainer =
      RTCStatsObtainer::Create();
  caller_->pc()->GetStats(stats_obtainer);
  // This will destroy the peer connection.
  caller_ = nullptr;
  // Any pending stats requests should have completed in the act of destroying
  // the peer connection.
  EXPECT_TRUE(stats_obtainer->report());
}
#endif  // HAVE_SCTP

}  // namespace

}  // namespace webrtc
