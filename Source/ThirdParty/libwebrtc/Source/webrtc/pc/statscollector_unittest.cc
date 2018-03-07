/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "pc/statscollector.h"

#include "api/mediastreaminterface.h"
#include "media/base/fakemediaengine.h"
#include "media/base/test/mock_mediachannel.h"
#include "pc/channelmanager.h"
#include "pc/mediastream.h"
#include "pc/mediastreamtrack.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/test/fakedatachannelprovider.h"
#include "pc/test/fakevideotracksource.h"
#include "pc/test/mock_peerconnection.h"
#include "pc/videotrack.h"
#include "rtc_base/base64.h"
#include "rtc_base/fakesslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/stringencode.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Field;
using testing::Invoke;
using testing::Return;
using testing::ReturnNull;
using testing::ReturnRef;
using testing::SetArgPointee;
using webrtc::PeerConnectionInterface;
using webrtc::StatsReport;
using webrtc::StatsReports;

namespace {
const bool kDefaultRtcpMuxRequired = true;
const bool kDefaultSrtpRequired = true;
}

namespace cricket {

class ChannelManager;

}  // namespace cricket

namespace webrtc {

namespace internal {
// This value comes from openssl/tls1.h
static const int TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA = 0xC014;
}  // namespace internal

// Error return values
const char kNotFound[] = "NOT FOUND";

// Constant names for track identification.
const char kLocalTrackId[] = "local_track_id";
const char kRemoteTrackId[] = "remote_track_id";
const uint32_t kSsrcOfTrack = 1234;

class FakeAudioProcessor : public webrtc::AudioProcessorInterface {
 public:
  FakeAudioProcessor() {}
  ~FakeAudioProcessor() {}

 private:
  void GetStats(AudioProcessorInterface::AudioProcessorStats* stats) override {
    stats->typing_noise_detected = true;
    stats->echo_return_loss = 2;
    stats->echo_return_loss_enhancement = 3;
    stats->echo_delay_median_ms = 4;
    stats->echo_delay_std_ms = 6;
  }

  AudioProcessorInterface::AudioProcessorStatistics GetStats(
      bool has_recv_streams) override {
    AudioProcessorStatistics stats;
    stats.typing_noise_detected = true;
    if (has_recv_streams) {
      stats.apm_statistics.echo_return_loss = 2.0;
      stats.apm_statistics.echo_return_loss_enhancement = 3.0;
      stats.apm_statistics.delay_median_ms = 4;
      stats.apm_statistics.delay_standard_deviation_ms = 5;
    }
    return stats;
  }
};

class FakeAudioTrack
    : public webrtc::MediaStreamTrack<webrtc::AudioTrackInterface> {
 public:
  explicit FakeAudioTrack(const std::string& id)
      : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(id),
        processor_(new rtc::RefCountedObject<FakeAudioProcessor>()) {}
  std::string kind() const override { return "audio"; }
  webrtc::AudioSourceInterface* GetSource() const override { return NULL; }
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {}
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {}
  bool GetSignalLevel(int* level) override {
    *level = 1;
    return true;
  }
  rtc::scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor()
      override {
    return processor_;
  }

 private:
  rtc::scoped_refptr<FakeAudioProcessor> processor_;
};

// This fake audio processor is used to verify that the undesired initial values
// (-1) will be filtered out.
class FakeAudioProcessorWithInitValue : public webrtc::AudioProcessorInterface {
 public:
  FakeAudioProcessorWithInitValue() {}
  ~FakeAudioProcessorWithInitValue() {}

 private:
  void GetStats(AudioProcessorInterface::AudioProcessorStats* stats) override {
    stats->typing_noise_detected = false;
    stats->echo_return_loss = -100;
    stats->echo_return_loss_enhancement = -100;
    stats->echo_delay_median_ms = -1;
    stats->echo_delay_std_ms = -1;
  }

  AudioProcessorInterface::AudioProcessorStatistics GetStats(
      bool /*has_recv_streams*/) override {
    AudioProcessorStatistics stats;
    stats.typing_noise_detected = false;
    return stats;
  }
};

class FakeAudioTrackWithInitValue
    : public webrtc::MediaStreamTrack<webrtc::AudioTrackInterface> {
 public:
  explicit FakeAudioTrackWithInitValue(const std::string& id)
      : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(id),
        processor_(
            new rtc::RefCountedObject<FakeAudioProcessorWithInitValue>()) {}
  std::string kind() const override { return "audio"; }
  webrtc::AudioSourceInterface* GetSource() const override { return NULL; }
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {}
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {}
  bool GetSignalLevel(int* level) override {
    *level = 1;
    return true;
  }
  rtc::scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor()
      override {
    return processor_;
  }

 private:
  rtc::scoped_refptr<FakeAudioProcessorWithInitValue> processor_;
};

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

// Finds the |n|-th report of type |type| in |reports|.
// |n| starts from 1 for finding the first report.
const StatsReport* FindNthReportByType(
    const StatsReports& reports, const StatsReport::StatsType& type, int n) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i]->type() == type) {
      n--;
      if (n == 0)
        return reports[i];
    }
  }
  return nullptr;
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
  return ExtractStatsValue(
      StatsReport::kStatsReportTypeBwe, reports, name);
}

std::string DerToPem(const std::string& der) {
  return rtc::SSLIdentity::DerToPem(
        rtc::kPemTypeCertificate,
        reinterpret_cast<const unsigned char*>(der.c_str()),
        der.length());
}

std::vector<std::string> DersToPems(
    const std::vector<std::string>& ders) {
  std::vector<std::string> pems(ders.size());
  std::transform(ders.begin(), ders.end(), pems.begin(), DerToPem);
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
    ASSERT_TRUE(report != NULL);

    std::string der_base64;
    EXPECT_TRUE(GetValue(
        report, StatsReport::kStatsValueNameDer, &der_base64));
    std::string der = rtc::Base64::Decode(der_base64, rtc::Base64::DO_STRICT);
    EXPECT_EQ(ders[i], der);

    std::string fingerprint_algorithm;
    EXPECT_TRUE(GetValue(
        report,
        StatsReport::kStatsValueNameFingerprintAlgorithm,
        &fingerprint_algorithm));
    // The digest algorithm for a FakeSSLCertificate is always SHA-1.
    std::string sha_1_str = rtc::DIGEST_SHA_1;
    EXPECT_EQ(sha_1_str, fingerprint_algorithm);

    std::string fingerprint;
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameFingerprint,
                         &fingerprint));
    EXPECT_FALSE(fingerprint.empty());

    ++i;
    std::string issuer_id;
    if (!GetValue(report, StatsReport::kStatsValueNameIssuerId,
                  &issuer_id)) {
      break;
    }

    cert_id = IdFromCertIdString(issuer_id);
    certificate_id = &cert_id;
  }
  EXPECT_EQ(ders.size(), i);
}

void VerifyVoiceReceiverInfoReport(
    const StatsReport* report,
    const cricket::VoiceReceiverInfo& info) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAudioOutputLevel, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameBytesReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int64_t>(info.bytes_rcvd), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.jitter_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterBufferMs, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.jitter_buffer_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePreferredJitterBufferMs,
      &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.jitter_buffer_preferred_ms),
      value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameCurrentDelayMs, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.delay_estimate_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameExpandRate, &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameSpeechExpandRate, &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.speech_expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAccelerateRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.accelerate_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNamePreemptiveExpandRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.preemptive_expand_rate), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameSecondaryDecodedRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.secondary_decoded_rate), value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameSecondaryDiscardedRate,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<float>(info.secondary_discarded_rate),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.packets_rcvd), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingCTSG, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_calls_to_silence_generator),
      value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingCTN, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_calls_to_neteq),
      value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingNormal, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_normal), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingPLC, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_plc), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingCNG, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_cng), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameDecodingPLCCNG, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_plc_cng), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameDecodingMutedOutput,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(info.decoding_muted_output), value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameCodecName,
                       &value_in_report));
}


void VerifyVoiceSenderInfoReport(const StatsReport* report,
                                 const cricket::VoiceSenderInfo& sinfo) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameCodecName, &value_in_report));
  EXPECT_EQ(sinfo.codec_name, value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameBytesSent, &value_in_report));
  EXPECT_EQ(rtc::ToString<int64_t>(sinfo.bytes_sent), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsSent, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.packets_sent), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsLost, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.packets_lost), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterReceived, &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.jitter_ms), value_in_report);
  if (sinfo.apm_statistics.delay_median_ms) {
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameEchoDelayMedian,
                         &value_in_report));
    EXPECT_EQ(rtc::ToString<int>(*sinfo.apm_statistics.delay_median_ms),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoDelayMedian,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.delay_standard_deviation_ms) {
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameEchoDelayStdDev,
                         &value_in_report));
    EXPECT_EQ(
        rtc::ToString<int>(*sinfo.apm_statistics.delay_standard_deviation_ms),
        value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoDelayStdDev,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.echo_return_loss) {
    EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameEchoReturnLoss,
                         &value_in_report));
    EXPECT_EQ(rtc::ToString<int>(*sinfo.apm_statistics.echo_return_loss),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoReturnLoss,
                          &value_in_report));
  }
  if (sinfo.apm_statistics.echo_return_loss_enhancement) {
    EXPECT_TRUE(GetValue(report,
                         StatsReport::kStatsValueNameEchoReturnLossEnhancement,
                         &value_in_report));
    EXPECT_EQ(
        rtc::ToString<int>(*sinfo.apm_statistics.echo_return_loss_enhancement),
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
    EXPECT_EQ(
        rtc::ToString<float>(*sinfo.apm_statistics.residual_echo_likelihood),
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
    EXPECT_EQ(rtc::ToString<float>(
                  *sinfo.apm_statistics.residual_echo_likelihood_recent_max),
              value_in_report);
  } else {
    EXPECT_FALSE(GetValue(
        report, StatsReport::kStatsValueNameResidualEchoLikelihoodRecentMax,
        &value_in_report));
  }
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAudioInputLevel,
                       &value_in_report));
  EXPECT_EQ(rtc::ToString<int>(sinfo.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameTypingNoiseState, &value_in_report));
  std::string typing_detected = sinfo.typing_noise_detected ? "true" : "false";
  EXPECT_EQ(typing_detected, value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameAnaBitrateActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.bitrate_action_counter);
  EXPECT_EQ(
      rtc::ToString<uint32_t>(*sinfo.ana_statistics.bitrate_action_counter),
      value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameAnaChannelActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.channel_action_counter);
  EXPECT_EQ(
      rtc::ToString<uint32_t>(*sinfo.ana_statistics.channel_action_counter),
      value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAnaDtxActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.dtx_action_counter);
  EXPECT_EQ(rtc::ToString<uint32_t>(*sinfo.ana_statistics.dtx_action_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(report, StatsReport::kStatsValueNameAnaFecActionCounter,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.fec_action_counter);
  EXPECT_EQ(rtc::ToString<uint32_t>(*sinfo.ana_statistics.fec_action_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAnaFrameLengthIncreaseCounter,
      &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.frame_length_increase_counter);
  EXPECT_EQ(rtc::ToString<uint32_t>(
                *sinfo.ana_statistics.frame_length_increase_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAnaFrameLengthDecreaseCounter,
      &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.frame_length_decrease_counter);
  EXPECT_EQ(rtc::ToString<uint32_t>(
                *sinfo.ana_statistics.frame_length_decrease_counter),
            value_in_report);
  EXPECT_TRUE(GetValue(report,
                       StatsReport::kStatsValueNameAnaUplinkPacketLossFraction,
                       &value_in_report));
  ASSERT_TRUE(sinfo.ana_statistics.uplink_packet_loss_fraction);
  EXPECT_EQ(
      rtc::ToString<float>(*sinfo.ana_statistics.uplink_packet_loss_fraction),
      value_in_report);
}

// Helper methods to avoid duplication of code.
void InitVoiceSenderInfo(cricket::VoiceSenderInfo* voice_sender_info) {
  voice_sender_info->add_ssrc(kSsrcOfTrack);
  voice_sender_info->codec_name = "fake_codec";
  voice_sender_info->bytes_sent = 100;
  voice_sender_info->packets_sent = 101;
  voice_sender_info->rtt_ms = 102;
  voice_sender_info->fraction_lost = 103;
  voice_sender_info->jitter_ms = 104;
  voice_sender_info->packets_lost = 105;
  voice_sender_info->ext_seqnum = 106;
  voice_sender_info->audio_level = 107;
  voice_sender_info->echo_return_loss = 108;
  voice_sender_info->echo_return_loss_enhancement = 109;
  voice_sender_info->echo_delay_median_ms = 110;
  voice_sender_info->echo_delay_std_ms = 111;
  voice_sender_info->typing_noise_detected = false;
  voice_sender_info->ana_statistics.bitrate_action_counter = 112;
  voice_sender_info->ana_statistics.channel_action_counter = 113;
  voice_sender_info->ana_statistics.dtx_action_counter = 114;
  voice_sender_info->ana_statistics.fec_action_counter = 115;
  voice_sender_info->ana_statistics.frame_length_increase_counter = 116;
  voice_sender_info->ana_statistics.frame_length_decrease_counter = 117;
  voice_sender_info->ana_statistics.uplink_packet_loss_fraction = 118.0;
}

void UpdateVoiceSenderInfoFromAudioTrack(
    AudioTrackInterface* audio_track,
    cricket::VoiceSenderInfo* voice_sender_info,
    bool has_remote_tracks) {
  audio_track->GetSignalLevel(&voice_sender_info->audio_level);
  webrtc::AudioProcessorInterface::AudioProcessorStatistics
      audio_processor_stats =
          audio_track->GetAudioProcessor()->GetStats(has_remote_tracks);
  voice_sender_info->typing_noise_detected =
      audio_processor_stats.typing_noise_detected;
  voice_sender_info->apm_statistics = audio_processor_stats.apm_statistics;
}

void InitVoiceReceiverInfo(cricket::VoiceReceiverInfo* voice_receiver_info) {
  voice_receiver_info->add_ssrc(kSsrcOfTrack);
  voice_receiver_info->bytes_rcvd = 110;
  voice_receiver_info->packets_rcvd = 111;
  voice_receiver_info->packets_lost = 112;
  voice_receiver_info->fraction_lost = 113;
  voice_receiver_info->packets_lost = 114;
  voice_receiver_info->ext_seqnum = 115;
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
}

class StatsCollectorForTest : public webrtc::StatsCollector {
 public:
  explicit StatsCollectorForTest(PeerConnection* pc)
      : StatsCollector(pc), time_now_(19477) {}

  double GetTimeNow() override {
    return time_now_;
  }

 private:
  double time_now_;
};

class StatsCollectorTest : public testing::Test {
 protected:
  StatsCollectorTest()
      : worker_thread_(rtc::Thread::Current()),
        network_thread_(rtc::Thread::Current()),
        media_engine_(new cricket::FakeMediaEngine()),
        pc_factory_(new FakePeerConnectionFactory(
            std::unique_ptr<cricket::MediaEngineInterface>(media_engine_))),
        pc_(pc_factory_) {
    // By default, we ignore session GetStats calls.
    EXPECT_CALL(pc_, GetSessionStats(_)).WillRepeatedly(ReturnNull());
    // Add default returns for mock classes.
    EXPECT_CALL(pc_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(pc_, sctp_data_channels())
        .WillRepeatedly(ReturnRef(data_channels_));
    EXPECT_CALL(pc_, GetSenders()).WillRepeatedly(Return(
        std::vector<rtc::scoped_refptr<RtpSenderInterface>>()));
    EXPECT_CALL(pc_, GetReceivers()).WillRepeatedly(Return(
        std::vector<rtc::scoped_refptr<RtpReceiverInterface>>()));
  }

  ~StatsCollectorTest() {}

  // This creates a standard setup with a transport called "trspname"
  // having one transport channel
  // and the specified virtual connection name.
  void InitSessionStats(const std::string& vc_name) {
    const std::string kTransportName("trspname");
    cricket::TransportStats transport_stats;
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;
    transport_stats.transport_name = kTransportName;
    transport_stats.channel_stats.push_back(channel_stats);

    session_stats_.transport_stats[kTransportName] = transport_stats;
    session_stats_.proxy_to_transport[vc_name] = kTransportName;
  }

  // Adds a outgoing video track with a given SSRC into the stats.
  void AddOutgoingVideoTrackStats() {
    stream_ = webrtc::MediaStream::Create("streamlabel");
    track_ = webrtc::VideoTrack::Create(kLocalTrackId,
                                        webrtc::FakeVideoTrackSource::Create(),
                                        rtc::Thread::Current());
    stream_->AddTrack(track_);
    EXPECT_CALL(pc_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kLocalTrackId), Return(true)));
  }

  // Adds a incoming video track with a given SSRC into the stats.
  void AddIncomingVideoTrackStats() {
    stream_ = webrtc::MediaStream::Create("streamlabel");
    track_ = webrtc::VideoTrack::Create(kRemoteTrackId,
                                        webrtc::FakeVideoTrackSource::Create(),
                                        rtc::Thread::Current());
    stream_->AddTrack(track_);
    EXPECT_CALL(pc_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
    }

  // Adds a outgoing audio track with a given SSRC into the stats.
  void AddOutgoingAudioTrackStats() {
    if (stream_ == NULL)
      stream_ = webrtc::MediaStream::Create("streamlabel");

    audio_track_ = new rtc::RefCountedObject<FakeAudioTrack>(
        kLocalTrackId);
    stream_->AddTrack(audio_track_);
    EXPECT_CALL(pc_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
        .WillOnce(DoAll(SetArgPointee<1>(kLocalTrackId), Return(true)));
  }

  // Adds a incoming audio track with a given SSRC into the stats.
  void AddIncomingAudioTrackStats() {
    if (stream_ == NULL)
      stream_ = webrtc::MediaStream::Create("streamlabel");

    audio_track_ = new rtc::RefCountedObject<FakeAudioTrack>(
        kRemoteTrackId);
    stream_->AddTrack(audio_track_);
    EXPECT_CALL(pc_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
        .WillOnce(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
  }

  void AddDataChannel(cricket::DataChannelType type,
                      const std::string& label,
                      int id) {
    InternalDataChannelInit config;
    config.id = id;

    data_channels_.push_back(DataChannel::Create(
        &data_channel_provider_, cricket::DCT_SCTP, label, config));
  }

  StatsReport* AddCandidateReport(StatsCollector* collector,
                                  const cricket::Candidate& candidate,
                                  bool local) {
    return collector->AddCandidateReport(candidate, local);
  }

  void SetupAndVerifyAudioTrackStats(
      FakeAudioTrack* audio_track,
      webrtc::MediaStream* stream,
      webrtc::StatsCollector* stats,
      cricket::VoiceChannel* voice_channel,
      const std::string& vc_name,
      MockVoiceMediaChannel* media_channel,
      cricket::VoiceSenderInfo* voice_sender_info,
      cricket::VoiceReceiverInfo* voice_receiver_info,
      cricket::VoiceMediaInfo* stats_read,
      StatsReports* reports) {
    // A track can't have both sender report and recv report at the same time
    // for now, this might change in the future though.
    EXPECT_TRUE((voice_sender_info == NULL) ^ (voice_receiver_info == NULL));

    // Instruct the session to return stats containing the transport channel.
    InitSessionStats(vc_name);
    EXPECT_CALL(pc_, GetSessionStats(_))
        .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
          return std::unique_ptr<SessionStats>(
              new SessionStats(session_stats_));
        }));

    // Constructs an ssrc stats update.
    if (voice_sender_info)
      stats_read->senders.push_back(*voice_sender_info);
    if (voice_receiver_info)
      stats_read->receivers.push_back(*voice_receiver_info);

    EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(Return(voice_channel));
    EXPECT_CALL(pc_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(*media_channel, GetStats(_))
        .WillOnce(DoAll(SetArgPointee<0>(*stats_read), Return(true)));

    stats->UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
    stats->ClearUpdateStatsCacheForTest();
    stats->GetStats(NULL, reports);

    // Verify the existence of the track report.
    const StatsReport* report = FindNthReportByType(
        *reports, StatsReport::kStatsReportTypeSsrc, 1);
    EXPECT_FALSE(report == NULL);
    EXPECT_EQ(stats->GetTimeNow(), report->timestamp());
    std::string track_id = ExtractSsrcStatsValue(
        *reports, StatsReport::kStatsValueNameTrackId);
    EXPECT_EQ(audio_track->id(), track_id);
    std::string ssrc_id = ExtractSsrcStatsValue(
        *reports, StatsReport::kStatsValueNameSsrc);
    EXPECT_EQ(rtc::ToString<uint32_t>(kSsrcOfTrack), ssrc_id);

    std::string media_type = ExtractSsrcStatsValue(*reports,
        StatsReport::kStatsValueNameMediaType);
    EXPECT_EQ("audio", media_type);

    // Verifies the values in the track report.
    if (voice_sender_info) {
      UpdateVoiceSenderInfoFromAudioTrack(audio_track, voice_sender_info,
                                          stats_read->receivers.size() > 0);
      VerifyVoiceSenderInfoReport(report, *voice_sender_info);
    }
    if (voice_receiver_info) {
      VerifyVoiceReceiverInfoReport(report, *voice_receiver_info);
    }

    // Verify we get the same result by passing a track to GetStats().
    StatsReports track_reports;  // returned values.
    stats->GetStats(audio_track, &track_reports);
    const StatsReport* track_report = FindNthReportByType(
        track_reports, StatsReport::kStatsReportTypeSsrc, 1);
    EXPECT_TRUE(track_report);
    EXPECT_EQ(stats->GetTimeNow(), track_report->timestamp());
    track_id = ExtractSsrcStatsValue(track_reports,
                                     StatsReport::kStatsValueNameTrackId);
    EXPECT_EQ(audio_track->id(), track_id);
    ssrc_id = ExtractSsrcStatsValue(track_reports,
                                    StatsReport::kStatsValueNameSsrc);
    EXPECT_EQ(rtc::ToString<uint32_t>(kSsrcOfTrack), ssrc_id);
    if (voice_sender_info)
      VerifyVoiceSenderInfoReport(track_report, *voice_sender_info);
    if (voice_receiver_info)
    VerifyVoiceReceiverInfoReport(track_report, *voice_receiver_info);
  }

  void TestCertificateReports(
      const rtc::FakeSSLCertificate& local_cert,
      const std::vector<std::string>& local_ders,
      std::unique_ptr<rtc::FakeSSLCertificate> remote_cert,
      const std::vector<std::string>& remote_ders) {
    StatsCollectorForTest stats(&pc_);

    StatsReports reports;  // returned values.

    // Fake stats to process.
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;
    channel_stats.srtp_crypto_suite = rtc::SRTP_AES128_CM_SHA1_80;
    channel_stats.ssl_cipher_suite =
        internal::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;

    cricket::TransportStats transport_stats;
    transport_stats.transport_name = "audio";
    transport_stats.channel_stats.push_back(channel_stats);

    SessionStats session_stats;
    session_stats.transport_stats[transport_stats.transport_name] =
        transport_stats;

    // Fake certificate to report
    rtc::scoped_refptr<rtc::RTCCertificate> local_certificate(
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::FakeSSLIdentity>(
            new rtc::FakeSSLIdentity(local_cert))));

    // Configure MockWebRtcSession
    EXPECT_CALL(pc_, GetLocalCertificate(transport_stats.transport_name, _))
        .WillOnce(DoAll(SetArgPointee<1>(local_certificate), Return(true)));
    EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(
                         transport_stats.transport_name))
        .WillOnce(Return(remote_cert.release()));
    EXPECT_CALL(pc_, GetSessionStats(_))
        .WillOnce(Invoke([&session_stats](const ChannelNamePairs&) {
          return std::unique_ptr<SessionStats>(
              new SessionStats(session_stats));
        }));

    stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

    stats.GetStats(NULL, &reports);

    const StatsReport* channel_report = FindNthReportByType(
        reports, StatsReport::kStatsReportTypeComponent, 1);
    EXPECT_TRUE(channel_report != NULL);

    // Check local certificate chain.
    std::string local_certificate_id = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameLocalCertificateId);
    if (local_ders.size() > 0) {
      EXPECT_NE(kNotFound, local_certificate_id);
      StatsReport::Id id(IdFromCertIdString(local_certificate_id));
      CheckCertChainReports(reports, local_ders, id);
    } else {
      EXPECT_EQ(kNotFound, local_certificate_id);
    }

    // Check remote certificate chain.
    std::string remote_certificate_id = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameRemoteCertificateId);
    if (remote_ders.size() > 0) {
      EXPECT_NE(kNotFound, remote_certificate_id);
      StatsReport::Id id(IdFromCertIdString(remote_certificate_id));
      CheckCertChainReports(reports, remote_ders, id);
    } else {
      EXPECT_EQ(kNotFound, remote_certificate_id);
    }

    // Check negotiated ciphers.
    std::string dtls_cipher_suite =
        ExtractStatsValue(StatsReport::kStatsReportTypeComponent, reports,
                          StatsReport::kStatsValueNameDtlsCipher);
    EXPECT_EQ(rtc::SSLStreamAdapter::SslCipherSuiteToName(
                  internal::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA),
              dtls_cipher_suite);
    std::string srtp_crypto_suite =
        ExtractStatsValue(StatsReport::kStatsReportTypeComponent, reports,
                          StatsReport::kStatsValueNameSrtpCipher);
    EXPECT_EQ(rtc::SrtpCryptoSuiteToName(rtc::SRTP_AES128_CM_SHA1_80),
              srtp_crypto_suite);
  }

  webrtc::RtcEventLogNullImpl event_log_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  // |media_engine_| is actually owned by |pc_factory_|.
  cricket::FakeMediaEngine* media_engine_;
  rtc::scoped_refptr<FakePeerConnectionFactory> pc_factory_;
  MockPeerConnection pc_;
  FakeDataChannelProvider data_channel_provider_;
  SessionStats session_stats_;
  rtc::scoped_refptr<webrtc::MediaStream> stream_;
  rtc::scoped_refptr<webrtc::VideoTrack> track_;
  rtc::scoped_refptr<FakeAudioTrack> audio_track_;
  std::vector<rtc::scoped_refptr<DataChannel>> data_channels_;
};

TEST_F(StatsCollectorTest, FilterOutNegativeDataChannelId) {
  const std::string label = "hacks";
  // The data channel id is from the Config which is -1 initially.
  const int id = -1;
  const std::string state = DataChannelInterface::DataStateString(
      DataChannelInterface::DataState::kConnecting);

  AddDataChannel(cricket::DCT_SCTP, label, id);
  StatsCollectorForTest stats(&pc_);

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

  StatsReports reports;
  stats.GetStats(NULL, &reports);

  const StatsReport* report =
      FindNthReportByType(reports, StatsReport::kStatsReportTypeDataChannel, 1);

  std::string value_in_report;
  EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameDataChannelId,
                        &value_in_report));
}

// Verify that ExtractDataInfo populates reports.
TEST_F(StatsCollectorTest, ExtractDataInfo) {
  const std::string label = "hacks";
  const int id = 31337;
  const std::string state = DataChannelInterface::DataStateString(
      DataChannelInterface::DataState::kConnecting);

  AddDataChannel(cricket::DCT_SCTP, label, id);
  StatsCollectorForTest stats(&pc_);

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

  StatsReports reports;
  stats.GetStats(NULL, &reports);

  const StatsReport* report =
      FindNthReportByType(reports, StatsReport::kStatsReportTypeDataChannel, 1);

  StatsReport::Id reportId = StatsReport::NewTypedIntId(
      StatsReport::kStatsReportTypeDataChannel, id);

  EXPECT_TRUE(reportId->Equals(report->id()));

  EXPECT_EQ(stats.GetTimeNow(), report->timestamp());
  EXPECT_EQ(label, ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                                     reports,
                                     StatsReport::kStatsValueNameLabel));
  EXPECT_EQ(rtc::ToString<int64_t>(id),
            ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel, reports,
                              StatsReport::kStatsValueNameDataChannelId));
  EXPECT_EQ(state, ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                                     reports,
                                     StatsReport::kStatsValueNameState));
  EXPECT_EQ("", ExtractStatsValue(StatsReport::kStatsReportTypeDataChannel,
                                  reports,
                                  StatsReport::kStatsValueNameProtocol));
}

// This test verifies that 64-bit counters are passed successfully.
TEST_F(StatsCollectorTest, BytesCounterHandles64Bits) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kVideoChannelName[] = "video";

  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVideoChannelName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // The number of bytes must be larger than 0xFFFFFFFF for this test.
  const int64_t kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                      Return(true)));
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractSsrcStatsValue(reports,
      StatsReport::kStatsValueNameBytesSent);
  EXPECT_EQ(kBytesSentString, result);
}

// Test that audio BWE information is reported via stats.
TEST_F(StatsCollectorTest, AudioBandwidthEstimationInfoIsReported) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kAudioChannelName[] = "audio";

  InitSessionStats(kAudioChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVoiceMediaChannel();
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, nullptr,
      rtc::WrapUnique(media_channel), kAudioChannelName,
      kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  StatsReports reports;  // returned values.
  cricket::VoiceSenderInfo voice_sender_info;
  cricket::VoiceMediaInfo stats_read;
  // Set up an SSRC just to test that we get both kinds of stats back: SSRC and
  // BWE.
  const int64_t kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  voice_sender_info.add_ssrc(1234);
  voice_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(voice_sender_info);

  webrtc::Call::Stats call_stats;
  const int kSendBandwidth = 1234567;
  const int kRecvBandwidth = 12345678;
  const int kPacerDelay = 123;
  call_stats.send_bandwidth_bps = kSendBandwidth;
  call_stats.recv_bandwidth_bps = kRecvBandwidth;
  call_stats.pacer_delay_ms = kPacerDelay;
  EXPECT_CALL(pc_, GetCallStats()).WillRepeatedly(Return(call_stats));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read), Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result =
      ExtractSsrcStatsValue(reports, StatsReport::kStatsValueNameBytesSent);
  EXPECT_EQ(kBytesSentString, result);
  result = ExtractBweStatsValue(
      reports, StatsReport::kStatsValueNameAvailableSendBandwidth);
  EXPECT_EQ(rtc::ToString(kSendBandwidth), result);
  result = ExtractBweStatsValue(
      reports, StatsReport::kStatsValueNameAvailableReceiveBandwidth);
  EXPECT_EQ(rtc::ToString(kRecvBandwidth), result);
  result =
      ExtractBweStatsValue(reports, StatsReport::kStatsValueNameBucketDelay);
  EXPECT_EQ(rtc::ToString(kPacerDelay), result);
}

// Test that video BWE information is reported via stats.
TEST_F(StatsCollectorTest, VideoBandwidthEstimationInfoIsReported) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kVideoChannelName[] = "video";

  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVideoChannelName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);

  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // Set up an SSRC just to test that we get both kinds of stats back: SSRC and
  // BWE.
  const int64_t kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  webrtc::Call::Stats call_stats;
  const int kSendBandwidth = 1234567;
  const int kRecvBandwidth = 12345678;
  const int kPacerDelay = 123;
  call_stats.send_bandwidth_bps = kSendBandwidth;
  call_stats.recv_bandwidth_bps = kRecvBandwidth;
  call_stats.pacer_delay_ms = kPacerDelay;
  EXPECT_CALL(pc_, GetCallStats()).WillRepeatedly(Return(call_stats));
  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read), Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractSsrcStatsValue(reports,
      StatsReport::kStatsValueNameBytesSent);
  EXPECT_EQ(kBytesSentString, result);
  result = ExtractBweStatsValue(
      reports, StatsReport::kStatsValueNameAvailableSendBandwidth);
  EXPECT_EQ(rtc::ToString(kSendBandwidth), result);
  result = ExtractBweStatsValue(
      reports, StatsReport::kStatsValueNameAvailableReceiveBandwidth);
  EXPECT_EQ(rtc::ToString(kRecvBandwidth), result);
  result =
      ExtractBweStatsValue(reports, StatsReport::kStatsValueNameBucketDelay);
  EXPECT_EQ(rtc::ToString(kPacerDelay), result);
}

// This test verifies that an object of type "googSession" always
// exists in the returned stats.
TEST_F(StatsCollectorTest, SessionObjectExists) {
  StatsCollectorForTest stats(&pc_);

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
}

// This test verifies that only one object of type "googSession" exists
// in the returned stats.
TEST_F(StatsCollectorTest, OnlyOneSessionObjectExists) {
  StatsCollectorForTest stats(&pc_);

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
  session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 2);
  EXPECT_EQ(NULL, session_report);
}

// This test verifies that the empty track report exists in the returned stats
// without calling StatsCollector::UpdateStats.
TEST_F(StatsCollectorTest, TrackObjectExistsWithoutUpdateStats) {
  StatsCollectorForTest stats(&pc_);

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      "video", kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Verfies the existence of the track report.
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  EXPECT_EQ((size_t)1, reports.size());
  EXPECT_EQ(StatsReport::kStatsReportTypeTrack, reports[0]->type());
  EXPECT_EQ(0, reports[0]->timestamp());

  std::string trackValue =
      ExtractStatsValue(StatsReport::kStatsReportTypeTrack,
                        reports,
                        StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, trackValue);
}

// This test verifies that the empty track report exists in the returned stats
// when StatsCollector::UpdateStats is called with ssrc stats.
TEST_F(StatsCollectorTest, TrackAndSsrcObjectExistAfterUpdateSsrcStats) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kVideoChannelName[] = "video";
  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVideoChannelName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const int64_t kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                    Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE((size_t)3, reports.size());
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_TRUE(track_report);

  // Get report for the specific |track|.
  reports.clear();
  stats.GetStats(track_, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE((size_t)3, reports.size());
  track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());

  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(rtc::ToString<uint32_t>(kSsrcOfTrack), ssrc_id);

  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, track_id);

  std::string media_type = ExtractSsrcStatsValue(reports,
      StatsReport::kStatsValueNameMediaType);
  EXPECT_EQ("video", media_type);
}

// This test verifies that an SSRC object has the identifier of a Transport
// stats object, and that this transport stats object exists in stats.
TEST_F(StatsCollectorTest, TransportObjectLinkedFromSsrcObject) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVideoMediaChannel();
  // The transport_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVcName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const int64_t kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                          Return(true)));

  InitSessionStats(kVcName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  std::string transport_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeSsrc,
      reports,
      StatsReport::kStatsValueNameTransportId);
  ASSERT_NE(kNotFound, transport_id);
  // Transport id component ID will always be 1.
  // This has assumptions about how the ID is constructed.  As is, this is
  // OK since this is for testing purposes only, but if we ever need this
  // in production, we should add a generic method that does this.
  size_t index = transport_id.find('-');
  ASSERT_NE(std::string::npos, index);
  std::string content = transport_id.substr(index + 1);
  index = content.rfind('-');
  ASSERT_NE(std::string::npos, index);
  content = content.substr(0, index);
  StatsReport::Id id(StatsReport::NewComponentId(content, 1));
  ASSERT_EQ(transport_id, id->ToString());
  const StatsReport* transport_report = FindReportById(reports, id);
  ASSERT_FALSE(transport_report == NULL);
}

// This test verifies that a remote stats object will not be created for
// an outgoing SSRC where remote stats are not returned.
TEST_F(StatsCollectorTest, RemoteSsrcInfoIsAbsent) {
  StatsCollectorForTest stats(&pc_);

  auto* media_channel = new MockVideoMediaChannel();
  // The transport_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVcName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_TRUE(remote_report == NULL);
}

// This test verifies that a remote stats object will be created for
// an outgoing SSRC where stats are returned.
TEST_F(StatsCollectorTest, RemoteSsrcInfoIsPresent) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVideoMediaChannel();
  // The transport_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVcName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  // Constructs an ssrc stats update.
  cricket::VideoMediaInfo stats_read;

  cricket::SsrcReceiverInfo remote_ssrc_stats;
  remote_ssrc_stats.timestamp = 12345.678;
  remote_ssrc_stats.ssrc = kSsrcOfTrack;
  cricket::VideoSenderInfo video_sender_info;
  video_sender_info.add_ssrc(kSsrcOfTrack);
  video_sender_info.remote_stats.push_back(remote_ssrc_stats);
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                          Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);

  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_FALSE(remote_report == NULL);
  EXPECT_EQ(12345.678, remote_report->timestamp());
}

// This test verifies that the empty track report exists in the returned stats
// when StatsCollector::UpdateStats is called with ssrc stats.
TEST_F(StatsCollectorTest, ReportsFromRemoteTrack) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kVideoChannelName[] = "video";
  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVideoChannelName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  AddIncomingVideoTrackStats();
  stats.AddStream(stream_);

  // Constructs an ssrc stats update.
  cricket::VideoReceiverInfo video_receiver_info;
  cricket::VideoMediaInfo stats_read;
  const int64_t kNumOfPacketsConcealed = 54321;

  // Construct a stats value to read.
  video_receiver_info.add_ssrc(1234);
  video_receiver_info.packets_concealed = kNumOfPacketsConcealed;
  stats_read.receivers.push_back(video_receiver_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                      Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE(static_cast<size_t>(3), reports.size());
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());

  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(rtc::ToString<uint32_t>(kSsrcOfTrack), ssrc_id);

  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kRemoteTrackId, track_id);
}

// This test verifies the Ice Candidate report should contain the correct
// information from local/remote candidates.
TEST_F(StatsCollectorTest, IceCandidateReport) {
  StatsCollectorForTest stats(&pc_);

  StatsReports reports;                     // returned values.

  const int local_port = 2000;
  const char local_ip[] = "192.168.0.1";
  const int remote_port = 2001;
  const char remote_ip[] = "192.168.0.2";

  rtc::SocketAddress local_address(local_ip, local_port);
  rtc::SocketAddress remote_address(remote_ip, remote_port);
  rtc::AdapterType network_type = rtc::ADAPTER_TYPE_ETHERNET;
  uint32_t priority = 1000;

  cricket::Candidate c;
  EXPECT_GT(c.id().length(), 0u);
  c.set_type(cricket::LOCAL_PORT_TYPE);
  c.set_protocol(cricket::UDP_PROTOCOL_NAME);
  c.set_address(local_address);
  c.set_priority(priority);
  c.set_network_type(network_type);
  std::string report_id = AddCandidateReport(&stats, c, true)->id()->ToString();
  EXPECT_EQ("Cand-" + c.id(), report_id);

  c = cricket::Candidate();
  EXPECT_GT(c.id().length(), 0u);
  c.set_type(cricket::PRFLX_PORT_TYPE);
  c.set_protocol(cricket::UDP_PROTOCOL_NAME);
  c.set_address(remote_address);
  c.set_priority(priority);
  c.set_network_type(network_type);
  report_id = AddCandidateReport(&stats, c, false)->id()->ToString();
  EXPECT_EQ("Cand-" + c.id(), report_id);

  stats.GetStats(NULL, &reports);

  // Verify the local candidate report is populated correctly.
  EXPECT_EQ(
      local_ip,
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateIPAddress));
  EXPECT_EQ(
      rtc::ToString<int>(local_port),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidatePortNumber));
  EXPECT_EQ(
      cricket::UDP_PROTOCOL_NAME,
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateTransportType));
  EXPECT_EQ(
      rtc::ToString<int>(priority),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidatePriority));
  EXPECT_EQ(
      IceCandidateTypeToStatsType(cricket::LOCAL_PORT_TYPE),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateType));
  EXPECT_EQ(
      AdapterTypeToStatsType(network_type),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceLocalCandidate, reports,
                        StatsReport::kStatsValueNameCandidateNetworkType));

  // Verify the remote candidate report is populated correctly.
  EXPECT_EQ(remote_ip,
            ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                              reports,
                              StatsReport::kStatsValueNameCandidateIPAddress));
  EXPECT_EQ(rtc::ToString<int>(remote_port),
            ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                              reports,
                              StatsReport::kStatsValueNameCandidatePortNumber));
  EXPECT_EQ(cricket::UDP_PROTOCOL_NAME,
            ExtractStatsValue(
                StatsReport::kStatsReportTypeIceRemoteCandidate, reports,
                StatsReport::kStatsValueNameCandidateTransportType));
  EXPECT_EQ(rtc::ToString<int>(priority),
            ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                              reports,
                              StatsReport::kStatsValueNameCandidatePriority));
  EXPECT_EQ(
      IceCandidateTypeToStatsType(cricket::PRFLX_PORT_TYPE),
      ExtractStatsValue(StatsReport::kStatsReportTypeIceRemoteCandidate,
                        reports, StatsReport::kStatsValueNameCandidateType));
  EXPECT_EQ(kNotFound,
            ExtractStatsValue(
                StatsReport::kStatsReportTypeIceRemoteCandidate, reports,
                StatsReport::kStatsValueNameCandidateNetworkType));
}

// This test verifies that all chained certificates are correctly
// reported
TEST_F(StatsCollectorTest, ChainedCertificateReportsCreated) {
  // Build local certificate chain.
  std::vector<std::string> local_ders(5);
  local_ders[0] = "These";
  local_ders[1] = "are";
  local_ders[2] = "some";
  local_ders[3] = "der";
  local_ders[4] = "values";
  rtc::FakeSSLCertificate local_cert(DersToPems(local_ders));

  // Build remote certificate chain
  std::vector<std::string> remote_ders(4);
  remote_ders[0] = "A";
  remote_ders[1] = "non-";
  remote_ders[2] = "intersecting";
  remote_ders[3] = "set";
  std::unique_ptr<rtc::FakeSSLCertificate> remote_cert(
      new rtc::FakeSSLCertificate(DersToPems(remote_ders)));

  TestCertificateReports(local_cert, local_ders, std::move(remote_cert),
                         remote_ders);
}

// This test verifies that all certificates without chains are correctly
// reported.
TEST_F(StatsCollectorTest, ChainlessCertificateReportsCreated) {
  // Build local certificate.
  std::string local_der = "This is the local der.";
  rtc::FakeSSLCertificate local_cert(DerToPem(local_der));

  // Build remote certificate.
  std::string remote_der = "This is somebody else's der.";
  std::unique_ptr<rtc::FakeSSLCertificate> remote_cert(
      new rtc::FakeSSLCertificate(DerToPem(remote_der)));

  TestCertificateReports(local_cert, std::vector<std::string>(1, local_der),
                         std::move(remote_cert),
                         std::vector<std::string>(1, remote_der));
}

// This test verifies that the stats are generated correctly when no
// transport is present.
TEST_F(StatsCollectorTest, NoTransport) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  StatsReports reports;  // returned values.

  // Fake stats to process.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;

  cricket::TransportStats transport_stats;
  transport_stats.transport_name = "audio";
  transport_stats.channel_stats.push_back(channel_stats);

  SessionStats session_stats;
  session_stats.transport_stats[transport_stats.transport_name] =
      transport_stats;

  // Configure MockWebRtcSession
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats));
      }));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);

  // Check that the negotiated ciphers are absent.
  std::string dtls_cipher_suite =
      ExtractStatsValue(StatsReport::kStatsReportTypeComponent, reports,
                        StatsReport::kStatsValueNameDtlsCipher);
  ASSERT_EQ(kNotFound, dtls_cipher_suite);
  std::string srtp_crypto_suite =
      ExtractStatsValue(StatsReport::kStatsReportTypeComponent, reports,
                        StatsReport::kStatsValueNameSrtpCipher);
  ASSERT_EQ(kNotFound, srtp_crypto_suite);
}

// This test verifies that the stats are generated correctly when the transport
// does not have any certificates.
TEST_F(StatsCollectorTest, NoCertificates) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  StatsReports reports;  // returned values.

  // Fake stats to process.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;

  cricket::TransportStats transport_stats;
  transport_stats.transport_name = "audio";
  transport_stats.channel_stats.push_back(channel_stats);

  SessionStats session_stats;
  session_stats.transport_stats[transport_stats.transport_name] =
      transport_stats;

  // Configure MockWebRtcSession
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([&session_stats](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats));
      }));
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);
}

// This test verifies that a remote certificate with an unsupported digest
// algorithm is correctly ignored.
TEST_F(StatsCollectorTest, UnsupportedDigestIgnored) {
  // Build a local certificate.
  std::string local_der = "This is the local der.";
  rtc::FakeSSLCertificate local_cert(DerToPem(local_der));

  // Build a remote certificate with an unsupported digest algorithm.
  std::string remote_der = "This is somebody else's der.";
  std::unique_ptr<rtc::FakeSSLCertificate> remote_cert(
      new rtc::FakeSSLCertificate(DerToPem(remote_der)));
  remote_cert->set_digest_algorithm("foobar");

  TestCertificateReports(local_cert, std::vector<std::string>(1, local_der),
                         std::move(remote_cert), std::vector<std::string>());
}

// This test verifies that the audio/video related stats which are -1 initially
// will be filtered out.
TEST_F(StatsCollectorTest, FilterOutNegativeInitialValues) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVoiceMediaChannel();
  // The transport_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, media_engine_,
      rtc::WrapUnique(media_channel), kVcName, kDefaultRtcpMuxRequired,
      kDefaultSrtpRequired);

  // Create a local stream with a local audio track and adds it to the stats.
  if (stream_ == NULL)
    stream_ = webrtc::MediaStream::Create("streamlabel");

  rtc::scoped_refptr<FakeAudioTrackWithInitValue> local_track(
      new rtc::RefCountedObject<FakeAudioTrackWithInitValue>(kLocalTrackId));
  stream_->AddTrack(local_track);
  EXPECT_CALL(pc_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
      .WillOnce(DoAll(SetArgPointee<1>(kLocalTrackId), Return(true)));
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(local_track.get(), kSsrcOfTrack);

  // Create a remote stream with a remote audio track and adds it to the stats.
  rtc::scoped_refptr<webrtc::MediaStream> remote_stream(
      webrtc::MediaStream::Create("remotestreamlabel"));
  rtc::scoped_refptr<FakeAudioTrackWithInitValue> remote_track(
      new rtc::RefCountedObject<FakeAudioTrackWithInitValue>(kRemoteTrackId));
  EXPECT_CALL(pc_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
      .WillOnce(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
  remote_stream->AddTrack(remote_track);
  stats.AddStream(remote_stream);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  cricket::VoiceSenderInfo voice_sender_info;
  voice_sender_info.add_ssrc(kSsrcOfTrack);
  // These values are set to -1 initially in audio_send_stream.
  // The voice_sender_info will read the values from audio_send_stream.
  voice_sender_info.rtt_ms = -1;
  voice_sender_info.packets_lost = -1;
  voice_sender_info.jitter_ms = -1;

  // Some of the contents in |voice_sender_info| needs to be updated from the
  // |audio_track_|.
  UpdateVoiceSenderInfoFromAudioTrack(local_track.get(), &voice_sender_info,
                                      true);

  cricket::VoiceReceiverInfo voice_receiver_info;
  voice_receiver_info.add_ssrc(kSsrcOfTrack);
  voice_receiver_info.capture_start_ntp_time_ms = -1;
  voice_receiver_info.audio_level = -1;

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);
  stats_read.receivers.push_back(voice_receiver_info);

  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read), Return(true)));

  StatsReports reports;
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

  // Get stats for the local track.
  stats.GetStats(local_track.get(), &reports);
  const StatsReport* report =
      FindNthReportByType(reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_TRUE(report);
  // The -1 will not be added to the stats report.
  std::string value_in_report;
  EXPECT_FALSE(
      GetValue(report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNamePacketsLost,
                        &value_in_report));
  EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameJitterReceived,
                        &value_in_report));
  EXPECT_FALSE(GetValue(report,
                        StatsReport::kStatsValueNameEchoCancellationQualityMin,
                        &value_in_report));
  EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoDelayMedian,
                        &value_in_report));
  EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameEchoDelayStdDev,
                        &value_in_report));

  // Get stats for the remote track.
  reports.clear();
  stats.GetStats(remote_track.get(), &reports);
  report = FindNthReportByType(reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_TRUE(report);
  EXPECT_FALSE(GetValue(report,
                        StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                        &value_in_report));
  EXPECT_FALSE(GetValue(report, StatsReport::kStatsValueNameAudioInputLevel,
                        &value_in_report));
}

// This test verifies that a local stats object can get statistics via
// AudioTrackInterface::GetStats() method.
TEST_F(StatsCollectorTest, GetStatsFromLocalAudioTrack) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVoiceMediaChannel();
  // The transport_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, media_engine_,
      rtc::WrapUnique(media_channel), kVcName, kDefaultRtcpMuxRequired,
      kDefaultSrtpRequired);
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_, kSsrcOfTrack);

  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);

  cricket::VoiceMediaInfo stats_read;
  StatsReports reports;  // returned values.
  SetupAndVerifyAudioTrackStats(
      audio_track_.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, &voice_sender_info, NULL, &stats_read, &reports);

  // Verify that there is no remote report for the local audio track because
  // we did not set it up.
  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_TRUE(remote_report == NULL);
}

// This test verifies that audio receive streams populate stats reports
// correctly.
TEST_F(StatsCollectorTest, GetStatsFromRemoteStream) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVoiceMediaChannel();
  // The transport_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, media_engine_,
      rtc::WrapUnique(media_channel), kVcName, kDefaultRtcpMuxRequired,
      kDefaultSrtpRequired);
  AddIncomingAudioTrackStats();
  stats.AddStream(stream_);

  cricket::VoiceReceiverInfo voice_receiver_info;
  InitVoiceReceiverInfo(&voice_receiver_info);
  voice_receiver_info.codec_name = "fake_codec";

  cricket::VoiceMediaInfo stats_read;
  StatsReports reports;  // returned values.
  SetupAndVerifyAudioTrackStats(
      audio_track_.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, NULL, &voice_receiver_info, &stats_read, &reports);
}

// This test verifies that a local stats object won't update its statistics
// after a RemoveLocalAudioTrack() call.
TEST_F(StatsCollectorTest, GetStatsAfterRemoveAudioStream) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVoiceMediaChannel();
  // The transport_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, media_engine_,
      rtc::WrapUnique(media_channel), kVcName, kDefaultRtcpMuxRequired,
      kDefaultSrtpRequired);
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  stats.RemoveLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);
  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);

  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                            Return(true)));

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // The report will exist since we don't remove them in RemoveStream().
  const StatsReport* report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_FALSE(report == NULL);
  EXPECT_EQ(stats.GetTimeNow(), report->timestamp());
  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, track_id);
  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(rtc::ToString<uint32_t>(kSsrcOfTrack), ssrc_id);

  // Verifies the values in the track report, no value will be changed by the
  // AudioTrackInterface::GetSignalValue() and
  // AudioProcessorInterface::AudioProcessorStats::GetStats();
  VerifyVoiceSenderInfoReport(report, voice_sender_info);
}

// This test verifies that when ongoing and incoming audio tracks are using
// the same ssrc, they populate stats reports correctly.
TEST_F(StatsCollectorTest, LocalAndRemoteTracksWithSameSsrc) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVoiceMediaChannel();
  // The transport_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, media_engine_,
      rtc::WrapUnique(media_channel), kVcName, kDefaultRtcpMuxRequired,
      kDefaultSrtpRequired);

  // Create a local stream with a local audio track and adds it to the stats.
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  // Create a remote stream with a remote audio track and adds it to the stats.
  rtc::scoped_refptr<webrtc::MediaStream> remote_stream(
      webrtc::MediaStream::Create("remotestreamlabel"));
  rtc::scoped_refptr<FakeAudioTrack> remote_track(
      new rtc::RefCountedObject<FakeAudioTrack>(kRemoteTrackId));
  EXPECT_CALL(pc_, GetRemoteTrackIdBySsrc(kSsrcOfTrack, _))
      .WillOnce(DoAll(SetArgPointee<1>(kRemoteTrackId), Return(true)));
  remote_stream->AddTrack(remote_track);
  stats.AddStream(remote_stream);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);

  // Some of the contents in |voice_sender_info| needs to be updated from the
  // |audio_track_|.
  UpdateVoiceSenderInfoFromAudioTrack(audio_track_.get(), &voice_sender_info,
                                      true);

  cricket::VoiceReceiverInfo voice_receiver_info;
  InitVoiceReceiverInfo(&voice_receiver_info);

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);
  stats_read.receivers.push_back(voice_receiver_info);

  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                            Return(true)));

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

  // Get stats for the local track.
  stats.GetStats(audio_track_.get(), &reports);
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());
  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kLocalTrackId, track_id);
  VerifyVoiceSenderInfoReport(track_report, voice_sender_info);

  // Get stats for the remote track.
  reports.clear();
  stats.GetStats(remote_track.get(), &reports);
  track_report = FindNthReportByType(reports,
                                     StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_TRUE(track_report);
  EXPECT_EQ(stats.GetTimeNow(), track_report->timestamp());
  track_id = ExtractSsrcStatsValue(reports,
                                   StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kRemoteTrackId, track_id);
  VerifyVoiceReceiverInfoReport(track_report, voice_receiver_info);
}

// This test verifies that when two outgoing audio tracks are using the same
// ssrc at different times, they populate stats reports correctly.
// TODO(xians): Figure out if it is possible to encapsulate the setup and
// avoid duplication of code in test cases.
TEST_F(StatsCollectorTest, TwoLocalTracksWithSameSsrc) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  auto* media_channel = new MockVoiceMediaChannel();
  // The transport_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(
      worker_thread_, network_thread_, nullptr, media_engine_,
      rtc::WrapUnique(media_channel), kVcName, kDefaultRtcpMuxRequired,
      kDefaultSrtpRequired);

  // Create a local stream with a local audio track and adds it to the stats.
  AddOutgoingAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_, kSsrcOfTrack);

  cricket::VoiceSenderInfo voice_sender_info;
  InitVoiceSenderInfo(&voice_sender_info);
  voice_sender_info.add_ssrc(kSsrcOfTrack);

  cricket::VoiceMediaInfo stats_read;
  StatsReports reports;  // returned values.
  SetupAndVerifyAudioTrackStats(
      audio_track_.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, &voice_sender_info, NULL, &stats_read, &reports);

  // Remove the previous audio track from the stream.
  stream_->RemoveTrack(audio_track_.get());
  stats.RemoveLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  // Create a new audio track and adds it to the stream and stats.
  static const std::string kNewTrackId = "new_track_id";
  rtc::scoped_refptr<FakeAudioTrack> new_audio_track(
      new rtc::RefCountedObject<FakeAudioTrack>(kNewTrackId));
  EXPECT_CALL(pc_, GetLocalTrackIdBySsrc(kSsrcOfTrack, _))
      .WillOnce(DoAll(SetArgPointee<1>(kNewTrackId), Return(true)));
  stream_->AddTrack(new_audio_track);

  stats.AddLocalAudioTrack(new_audio_track, kSsrcOfTrack);
  stats.ClearUpdateStatsCacheForTest();
  cricket::VoiceSenderInfo new_voice_sender_info;
  InitVoiceSenderInfo(&new_voice_sender_info);
  cricket::VoiceMediaInfo new_stats_read;
  reports.clear();
  SetupAndVerifyAudioTrackStats(
      new_audio_track.get(), stream_.get(), &stats, &voice_channel, kVcName,
      media_channel, &new_voice_sender_info, NULL, &new_stats_read, &reports);
}

// This test verifies that stats are correctly set in video send ssrc stats.
TEST_F(StatsCollectorTest, VerifyVideoSendSsrcStats) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kVideoChannelName[] = "video";

  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVideoChannelName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;

  AddOutgoingVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.frames_encoded = 10;
  video_sender_info.qp_sum = 11;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read), Return(true)));
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  EXPECT_EQ(rtc::ToString(video_sender_info.frames_encoded),
            ExtractSsrcStatsValue(reports,
                                  StatsReport::kStatsValueNameFramesEncoded));
  EXPECT_EQ(rtc::ToString(*video_sender_info.qp_sum),
            ExtractSsrcStatsValue(reports, StatsReport::kStatsValueNameQpSum));
}

// This test verifies that stats are correctly set in video receive ssrc stats.
TEST_F(StatsCollectorTest, VerifyVideoReceiveSsrcStats) {
  StatsCollectorForTest stats(&pc_);

  EXPECT_CALL(pc_, GetLocalCertificate(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(pc_, GetRemoteSSLCertificate_ReturnsRawPointer(_))
      .WillRepeatedly(Return(nullptr));

  const char kVideoChannelName[] = "video";

  InitSessionStats(kVideoChannelName);
  EXPECT_CALL(pc_, GetSessionStats(_))
      .WillRepeatedly(Invoke([this](const ChannelNamePairs&) {
        return std::unique_ptr<SessionStats>(
            new SessionStats(session_stats_));
      }));

  auto* media_channel = new MockVideoMediaChannel();
  cricket::VideoChannel video_channel(
      worker_thread_, network_thread_, nullptr, rtc::WrapUnique(media_channel),
      kVideoChannelName, kDefaultRtcpMuxRequired, kDefaultSrtpRequired);
  StatsReports reports;  // returned values.
  cricket::VideoReceiverInfo video_receiver_info;
  cricket::VideoMediaInfo stats_read;

  AddIncomingVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_receiver_info.add_ssrc(1234);
  video_receiver_info.frames_decoded = 10;
  video_receiver_info.qp_sum = 11;
  stats_read.receivers.push_back(video_receiver_info);

  EXPECT_CALL(pc_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(pc_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(stats_read), Return(true)));
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  EXPECT_EQ(rtc::ToString(video_receiver_info.frames_decoded),
            ExtractSsrcStatsValue(reports,
                                  StatsReport::kStatsValueNameFramesDecoded));
  EXPECT_EQ(rtc::ToString(*video_receiver_info.qp_sum),
            ExtractSsrcStatsValue(reports, StatsReport::kStatsValueNameQpSum));
}

}  // namespace webrtc
