/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/test/legacy_stats_collector_test_helpers.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "api/legacy_stats_types.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/scoped_refptr.h"
#include "media/base/media_channel.h"
#include "pc/test/fake_audio_track.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "rtc_base/base64.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/ssl_identity.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

scoped_refptr<AudioTrackInterface> CreateFakeAudioTrack(const std::string& id,
                                                        bool return_stats) {
  auto processor = make_ref_counted<FakeAudioProcessor>();
  processor->return_stats = return_stats;
  if (return_stats) {
    processor->stats.apm_statistics.echo_return_loss = 2.0;
    processor->stats.apm_statistics.echo_return_loss_enhancement = 3.0;
    processor->stats.apm_statistics.delay_median_ms = 4;
    processor->stats.apm_statistics.delay_standard_deviation_ms = 5;
  }
  return FakeAudioTrack::Create(id, MediaStreamTrackInterface::kLive,
                                processor);
}

bool GetValue(const StatsReport* report,
              StatsReport::StatsValueName name,
              std::string* value) {
  const StatsReport::Value* v = report->FindValue(name);
  if (!v)
    return false;
  *value = v->ToString();
  return true;
}

std::string ExtractStatsValue(const StatsReport::StatsType& type,
                              const StatsReports& reports,
                              StatsReport::StatsValueName name) {
  for (const auto* r : reports) {
    std::string ret;
    if (r->type() == type && GetValue(r, name, &ret))
      return ret;
  }

  return kNotFound;
}

StatsReport::Id TypedIdFromIdString(StatsReport::StatsType type,
                                    const std::string& value) {
  EXPECT_FALSE(value.empty());
  StatsReport::Id id;
  if (value.empty())
    return id;

  // This has assumptions about how the ID is constructed.  As is, this is
  // OK since this is for testing purposes only, but if we ever need this
  // in production, we should add a generic method that does this.
  size_t index = value.find('_');
  EXPECT_NE(index, std::string::npos);
  if (index == std::string::npos || index == (value.length() - 1))
    return id;

  id = StatsReport::NewTypedId(type, value.substr(index + 1));
  EXPECT_EQ(id->ToString(), value);
  return id;
}

StatsReport::Id IdFromCertIdString(const std::string& cert_id) {
  return TypedIdFromIdString(StatsReport::kStatsReportTypeCertificate, cert_id);
}

const StatsReport* FindNthReportByType(const StatsReports& reports,
                                       const StatsReport::StatsType& type,
                                       int n) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i]->type() == type) {
      n--;
      if (n == 0)
        return reports[i];
    }
  }
  return nullptr;
}

std::optional<std::string> GetValueInNthReportByType(
    const StatsReports& reports,
    StatsReport::StatsType type,
    StatsReport::StatsValueName name,
    int n) {
  const StatsReport* report = FindNthReportByType(reports, type, n);
  if (!report) {
    return std::nullopt;
  }
  std::string value;
  if (!GetValue(report, name, &value)) {
    return std::nullopt;
  }
  return value;
}

std::vector<const StatsReport*> GetReportsByType(const StatsReports& reports,
                                                 StatsReport::StatsType type) {
  std::vector<const StatsReport*> filtered_reports;
  for (const StatsReport* report : reports) {
    if (report->type() == type) {
      filtered_reports.push_back(report);
    }
  }
  return filtered_reports;
}

const StatsReport* FindReportById(const StatsReports& reports,
                                  const StatsReport::Id& id) {
  for (const auto* r : reports) {
    if (r->id()->Equals(id))
      return r;
  }
  return nullptr;
}

std::string ExtractSsrcStatsValue(const StatsReports& reports,
                                  StatsReport::StatsValueName name) {
  return ExtractStatsValue(StatsReport::kStatsReportTypeSsrc, reports, name);
}

std::string ExtractBweStatsValue(const StatsReports& reports,
                                 StatsReport::StatsValueName name) {
  return ExtractStatsValue(StatsReport::kStatsReportTypeBwe, reports, name);
}

std::string DerToPem(const std::string& der) {
  return SSLIdentity::DerToPem(
      kPemTypeCertificate, reinterpret_cast<const unsigned char*>(der.c_str()),
      der.length());
}

std::vector<std::string> DersToPems(const std::vector<std::string>& ders) {
  std::vector<std::string> pems(ders.size());
  absl::c_transform(ders, pems.begin(), DerToPem);
  return pems;
}

void CheckCertChainReports(const StatsReports& reports,
                           const std::vector<std::string>& ders,
                           const StatsReport::Id& start_id) {
  StatsReport::Id cert_id;
  const StatsReport::Id* certificate_id = &start_id;
  size_t i = 0;
  while (true) {
    const StatsReport* report = FindReportById(reports, *certificate_id);
    ASSERT_TRUE(report != nullptr);

    std::string der_base64;
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDer, &der_base64));
    EXPECT_THAT(ders[i], Eq(Base64Decode(der_base64)));

    std::string fingerprint_algorithm;
    EXPECT_TRUE(GetValue(report,
                         StatsReport::kStatsValueNameFingerprintAlgorithm,
                         &fingerprint_algorithm));
    // The digest algorithm for a FakeSSLCertificate is always SHA-1.
    std::string sha_1_str = DIGEST_SHA_1;
    EXPECT_EQ(sha_1_str, fingerprint_algorithm);

    std::string fingerprint;
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameFingerprint,
                         &fingerprint));
    EXPECT_FALSE(fingerprint.empty());

    ++i;
    std::string issuer_id;
    if (!GetValue(report, StatsReport::kStatsValueNameIssuerId, &issuer_id)) {
      break;
    }

    cert_id = IdFromCertIdString(issuer_id);
    certificate_id = &cert_id;
  }
  EXPECT_EQ(ders.size(), i);
}

void VerifyVoiceReceiverInfoReport(const StatsReport* report,
                                   const VoiceReceiverInfo& info) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAudioOutputLevel,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameBytesReceived,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.payload_bytes_received +
                         info.header_and_padding_bytes_received),
            value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameJitterReceived,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.jitter_ms), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameJitterBufferMs,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.jitter_buffer_ms), value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNamePreferredJitterBufferMs,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.jitter_buffer_preferred_ms), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameCurrentDelayMs,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.delay_estimate_ms), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameExpandRate,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameSpeechExpandRate,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.speech_expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAccelerateRate,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.accelerate_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNamePreemptiveExpandRate,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.preemptive_expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameSecondaryDecodedRate,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.secondary_decoded_rate), value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameSecondaryDiscardedRate,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.secondary_discarded_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNamePacketsReceived,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.packets_received), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingCTSG,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_calls_to_silence_generator),
            value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingCTN,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_calls_to_neteq), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingNormal,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_normal), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingPLC,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_plc), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingCodecPLC,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_codec_plc), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingCNG,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_cng), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingPLCCNG,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_plc_cng), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingMutedOutput,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(info.decoding_muted_output), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameCodecName,
                       &value_in_report));
}

void VerifyVoiceSenderInfoReport(const StatsReport* report,
                                 const VoiceSenderInfo& sinfo) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameCodecName,
                       &value_in_report));
  EXPECT_EQ(sinfo.codec_name, value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameBytesSent,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.payload_bytes_sent +
                         sinfo.header_and_padding_bytes_sent),
            value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNamePacketsSent,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.packets_sent), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNamePacketsLost,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.packets_lost), value_in_report);
  EXPECT_TRUE(
      GetValue(report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(
      GetValue(report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameJitterReceived,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.jitter_ms), value_in_report);
  if (sinfo.apm_statistics.delay_median_ms) {
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameEchoDelayMedian,
                         &value_in_report));
    EXPECT_EQ(absl::StrCat(*sinfo.apm_statistics.delay_median_ms),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoDelayMedian,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.delay_standard_deviation_ms) {
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameEchoDelayStdDev,
                         &value_in_report));
    EXPECT_EQ(absl::StrCat(*sinfo.apm_statistics.delay_standard_deviation_ms),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoDelayStdDev,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.echo_return_loss) {
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameEchoReturnLoss,
                         &value_in_report));
    EXPECT_EQ(absl::StrCat(*sinfo.apm_statistics.echo_return_loss),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoReturnLoss,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.echo_return_loss_enhancement) {
    EXPECT_TRUE(GetValue(report,
                         StatsReport::kStatsValueNameEchoReturnLossEnhancement,
                         &value_in_report));
    EXPECT_EQ(absl::StrCat(*sinfo.apm_statistics.echo_return_loss_enhancement),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report,
                          StatsReport::kStatsValueNameEchoReturnLossEnhancement,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.residual_echo_likelihood) {
    EXPECT_TRUE(GetValue(report,
                         StatsReport::kStatsValueNameResidualEchoLikelihood,
                         &value_in_report));
    EXPECT_EQ(absl::StrCat(*sinfo.apm_statistics.residual_echo_likelihood),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report,
                          StatsReport::kStatsValueNameResidualEchoLikelihood,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.residual_echo_likelihood_recent_max) {
    EXPECT_TRUE(GetValue(
        report, StatsReport::kStatsValueNameResidualEchoLikelihoodRecentMax,
        &value_in_report));
    EXPECT_EQ(
        absl::StrCat(*sinfo.apm_statistics.residual_echo_likelihood_recent_max),
        value_in_report);
  } else {
    EXPECT_FALSE(GetValue(
        report, StatsReport::kStatsValueNameResidualEchoLikelihoodRecentMax,
        &value_in_report));
  }
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAudioInputLevel,
                       &value_in_report));
  EXPECT_EQ(absl::StrCat(sinfo.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameAnaBitrateActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.bitrate_action_counter);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.bitrate_action_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameAnaChannelActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.channel_action_counter);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.channel_action_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAnaDtxActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.dtx_action_counter);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.dtx_action_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAnaFecActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.fec_action_counter);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.fec_action_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAnaFrameLengthIncreaseCounter,
      &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.frame_length_increase_counter);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.frame_length_increase_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAnaFrameLengthDecreaseCounter,
      &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.frame_length_decrease_counter);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.frame_length_decrease_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameAnaUplinkPacketLossFraction,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.uplink_packet_loss_fraction);
  EXPECT_EQ(absl::StrCat(*sinfo.ana_statistics.uplink_packet_loss_fraction),
            value_in_report);
}

void InitVoiceSenderInfo(VoiceSenderInfo* voice_sender_info, uint32_t ssrc) {
  voice_sender_info->add_ssrc(ssrc);
  voice_sender_info->codec_name = "fake_codec";
  voice_sender_info->payload_bytes_sent = 88;
  voice_sender_info->header_and_padding_bytes_sent = 12;
  voice_sender_info->packets_sent = 101;
  voice_sender_info->rtt_ms = 102;
  voice_sender_info->fraction_lost = 103;
  voice_sender_info->jitter_ms = 104;
  voice_sender_info->packets_lost = 105;
  voice_sender_info->audio_level = 107;
  voice_sender_info->apm_statistics.echo_return_loss = 108;
  voice_sender_info->apm_statistics.echo_return_loss_enhancement = 109;
  voice_sender_info->apm_statistics.delay_median_ms = 110;
  voice_sender_info->apm_statistics.delay_standard_deviation_ms = 111;
  voice_sender_info->ana_statistics.bitrate_action_counter = 112;
  voice_sender_info->ana_statistics.channel_action_counter = 113;
  voice_sender_info->ana_statistics.dtx_action_counter = 114;
  voice_sender_info->ana_statistics.fec_action_counter = 115;
  voice_sender_info->ana_statistics.frame_length_increase_counter = 116;
  voice_sender_info->ana_statistics.frame_length_decrease_counter = 117;
  voice_sender_info->ana_statistics.uplink_packet_loss_fraction = 118.0;
}

void UpdateVoiceSenderInfoFromAudioTrack(AudioTrackInterface* audio_track,
                                         VoiceSenderInfo* voice_sender_info,
                                         bool has_remote_tracks) {
  audio_track->GetSignalLevel(&voice_sender_info->audio_level);
  AudioProcessorInterface::AudioProcessorStatistics audio_processor_stats =
      audio_track->GetAudioProcessor()->GetStats(has_remote_tracks);
  voice_sender_info->apm_statistics = audio_processor_stats.apm_statistics;
}

void InitVoiceReceiverInfo(VoiceReceiverInfo* voice_receiver_info) {
  voice_receiver_info->add_ssrc(kSsrcOfTrack);
  voice_receiver_info->payload_bytes_received = 98;
  voice_receiver_info->header_and_padding_bytes_received = 12;
  voice_receiver_info->packets_received = 111;
  voice_receiver_info->packets_lost = 114;
  voice_receiver_info->jitter_ms = 116;
  voice_receiver_info->jitter_buffer_ms = 117;
  voice_receiver_info->jitter_buffer_preferred_ms = 118;
  voice_receiver_info->delay_estimate_ms = 119;
  voice_receiver_info->audio_level = 120;
  voice_receiver_info->expand_rate = 121;
  voice_receiver_info->speech_expand_rate = 122;
  voice_receiver_info->secondary_decoded_rate = 123;
  voice_receiver_info->accelerate_rate = 124;
  voice_receiver_info->preemptive_expand_rate = 125;
  voice_receiver_info->secondary_discarded_rate = 126;
  voice_receiver_info->decoding_codec_plc = 127;
}

scoped_refptr<MockRtpSenderInternal> CreateMockSender(
    scoped_refptr<MediaStreamTrackInterface> track,
    uint32_t ssrc) {
  auto sender = make_ref_counted<MockRtpSenderInternal>();
  EXPECT_CALL(*sender, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*sender, ssrc()).WillRepeatedly(Return(ssrc));
  EXPECT_CALL(*sender, media_type())
      .WillRepeatedly(
          Return(track->kind() == MediaStreamTrackInterface::kAudioKind
                     ? MediaType::AUDIO
                     : MediaType::VIDEO));
  EXPECT_CALL(*sender, SetMediaChannel(NotNull())).Times(AtMost(1));
  EXPECT_CALL(*sender, SetMediaChannel(IsNull())).WillRepeatedly(Return());
  EXPECT_CALL(*sender, DetachTrackAndGetStopTask()).WillRepeatedly([] {
    return nullptr;
  });
  return sender;
}

scoped_refptr<MockRtpReceiverInternal> CreateMockReceiver(
    scoped_refptr<MediaStreamTrackInterface> track,
    uint32_t ssrc) {
  auto receiver = make_ref_counted<MockRtpReceiverInternal>();
  EXPECT_CALL(*receiver, track()).WillRepeatedly(Return(track));
  EXPECT_CALL(*receiver, ssrc_s()).WillRepeatedly(Return(ssrc));
  EXPECT_CALL(*receiver, media_type())
      .WillRepeatedly(
          Return(track->kind() == MediaStreamTrackInterface::kAudioKind
                     ? MediaType::AUDIO
                     : MediaType::VIDEO));
  EXPECT_CALL(*receiver, SetMediaChannel(_)).WillRepeatedly(Return());
  EXPECT_CALL(*receiver, Stop()).WillRepeatedly(Return());
  return receiver;
}

}  // namespace webrtc
