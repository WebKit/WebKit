/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"

#include <string.h>  // memcmp

#include <limits>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace test {

namespace {

struct ExtensionPair {
  RTPExtensionType type;
  const char* name;
};

constexpr int kMaxCsrcs = 3;

// Maximum serialized size of a header extension, including 1 byte ID.
constexpr int kMaxExtensionSizeBytes = 4;
constexpr int kMaxNumExtensions = 5;

constexpr ExtensionPair kExtensions[kMaxNumExtensions] = {
    {RTPExtensionType::kRtpExtensionTransmissionTimeOffset,
     RtpExtension::kTimestampOffsetUri},
    {RTPExtensionType::kRtpExtensionAbsoluteSendTime,
     RtpExtension::kAbsSendTimeUri},
    {RTPExtensionType::kRtpExtensionTransportSequenceNumber,
     RtpExtension::kTransportSequenceNumberUri},
    {RTPExtensionType::kRtpExtensionAudioLevel, RtpExtension::kAudioLevelUri},
    {RTPExtensionType::kRtpExtensionVideoRotation,
     RtpExtension::kVideoRotationUri}};

template <typename T>
void ShuffleInPlace(Random* prng, rtc::ArrayView<T> array) {
  RTC_DCHECK_LE(array.size(), std::numeric_limits<uint32_t>::max());
  for (uint32_t i = 0; i + 1 < array.size(); i++) {
    uint32_t other = prng->Rand(i, static_cast<uint32_t>(array.size() - 1));
    std::swap(array[i], array[other]);
  }
}
}  // namespace

std::unique_ptr<RtcEventAlrState> EventGenerator::NewAlrState() {
  return absl::make_unique<RtcEventAlrState>(prng_.Rand<bool>());
}

std::unique_ptr<RtcEventAudioPlayout> EventGenerator::NewAudioPlayout(
    uint32_t ssrc) {
  return absl::make_unique<RtcEventAudioPlayout>(ssrc);
}

std::unique_ptr<RtcEventAudioNetworkAdaptation>
EventGenerator::NewAudioNetworkAdaptation() {
  std::unique_ptr<AudioEncoderRuntimeConfig> config =
      absl::make_unique<AudioEncoderRuntimeConfig>();

  config->bitrate_bps = prng_.Rand(0, 3000000);
  config->enable_fec = prng_.Rand<bool>();
  config->enable_dtx = prng_.Rand<bool>();
  config->frame_length_ms = prng_.Rand(10, 120);
  config->num_channels = prng_.Rand(1, 2);
  config->uplink_packet_loss_fraction = prng_.Rand<float>();

  return absl::make_unique<RtcEventAudioNetworkAdaptation>(std::move(config));
}

std::unique_ptr<RtcEventBweUpdateDelayBased>
EventGenerator::NewBweUpdateDelayBased() {
  constexpr int32_t kMaxBweBps = 20000000;
  int32_t bitrate_bps = prng_.Rand(0, kMaxBweBps);
  BandwidthUsage state = static_cast<BandwidthUsage>(
      prng_.Rand(static_cast<uint32_t>(BandwidthUsage::kLast) - 1));
  return absl::make_unique<RtcEventBweUpdateDelayBased>(bitrate_bps, state);
}

std::unique_ptr<RtcEventBweUpdateLossBased>
EventGenerator::NewBweUpdateLossBased() {
  constexpr int32_t kMaxBweBps = 20000000;
  constexpr int32_t kMaxPackets = 1000;
  int32_t bitrate_bps = prng_.Rand(0, kMaxBweBps);
  uint8_t fraction_lost = prng_.Rand<uint8_t>();
  int32_t total_packets = prng_.Rand(1, kMaxPackets);

  return absl::make_unique<RtcEventBweUpdateLossBased>(
      bitrate_bps, fraction_lost, total_packets);
}

std::unique_ptr<RtcEventProbeClusterCreated>
EventGenerator::NewProbeClusterCreated() {
  constexpr int kMaxBweBps = 20000000;
  constexpr int kMaxNumProbes = 10000;
  int id = prng_.Rand(1, kMaxNumProbes);
  int bitrate_bps = prng_.Rand(0, kMaxBweBps);
  int min_probes = prng_.Rand(5, 50);
  int min_bytes = prng_.Rand(500, 50000);

  return absl::make_unique<RtcEventProbeClusterCreated>(id, bitrate_bps,
                                                        min_probes, min_bytes);
}

std::unique_ptr<RtcEventProbeResultFailure>
EventGenerator::NewProbeResultFailure() {
  constexpr int kMaxNumProbes = 10000;
  int id = prng_.Rand(1, kMaxNumProbes);
  ProbeFailureReason reason = static_cast<ProbeFailureReason>(
      prng_.Rand(static_cast<uint32_t>(ProbeFailureReason::kLast) - 1));

  return absl::make_unique<RtcEventProbeResultFailure>(id, reason);
}

std::unique_ptr<RtcEventProbeResultSuccess>
EventGenerator::NewProbeResultSuccess() {
  constexpr int kMaxBweBps = 20000000;
  constexpr int kMaxNumProbes = 10000;
  int id = prng_.Rand(1, kMaxNumProbes);
  int bitrate_bps = prng_.Rand(0, kMaxBweBps);

  return absl::make_unique<RtcEventProbeResultSuccess>(id, bitrate_bps);
}

std::unique_ptr<RtcEventIceCandidatePairConfig>
EventGenerator::NewIceCandidatePairConfig() {
  IceCandidateType local_candidate_type = static_cast<IceCandidateType>(
      prng_.Rand(static_cast<uint32_t>(IceCandidateType::kNumValues) - 1));
  IceCandidateNetworkType local_network_type =
      static_cast<IceCandidateNetworkType>(prng_.Rand(
          static_cast<uint32_t>(IceCandidateNetworkType::kNumValues) - 1));
  IceCandidatePairAddressFamily local_address_family =
      static_cast<IceCandidatePairAddressFamily>(prng_.Rand(
          static_cast<uint32_t>(IceCandidatePairAddressFamily::kNumValues) -
          1));
  IceCandidateType remote_candidate_type = static_cast<IceCandidateType>(
      prng_.Rand(static_cast<uint32_t>(IceCandidateType::kNumValues) - 1));
  IceCandidatePairAddressFamily remote_address_family =
      static_cast<IceCandidatePairAddressFamily>(prng_.Rand(
          static_cast<uint32_t>(IceCandidatePairAddressFamily::kNumValues) -
          1));
  IceCandidatePairProtocol protocol_type =
      static_cast<IceCandidatePairProtocol>(prng_.Rand(
          static_cast<uint32_t>(IceCandidatePairProtocol::kNumValues) - 1));

  IceCandidatePairDescription desc;
  desc.local_candidate_type = local_candidate_type;
  desc.local_relay_protocol = protocol_type;
  desc.local_network_type = local_network_type;
  desc.local_address_family = local_address_family;
  desc.remote_candidate_type = remote_candidate_type;
  desc.remote_address_family = remote_address_family;
  desc.candidate_pair_protocol = protocol_type;

  IceCandidatePairConfigType type =
      static_cast<IceCandidatePairConfigType>(prng_.Rand(
          static_cast<uint32_t>(IceCandidatePairConfigType::kNumValues) - 1));
  uint32_t pair_id = prng_.Rand<uint32_t>();
  return absl::make_unique<RtcEventIceCandidatePairConfig>(type, pair_id, desc);
}

std::unique_ptr<RtcEventIceCandidatePair>
EventGenerator::NewIceCandidatePair() {
  IceCandidatePairEventType type =
      static_cast<IceCandidatePairEventType>(prng_.Rand(
          static_cast<uint32_t>(IceCandidatePairEventType::kNumValues) - 1));
  uint32_t pair_id = prng_.Rand<uint32_t>();

  return absl::make_unique<RtcEventIceCandidatePair>(type, pair_id);
}

rtcp::ReportBlock EventGenerator::NewReportBlock() {
  rtcp::ReportBlock report_block;
  report_block.SetMediaSsrc(prng_.Rand<uint32_t>());
  report_block.SetFractionLost(prng_.Rand<uint8_t>());
  // cumulative_lost is a 3-byte signed value.
  RTC_DCHECK(report_block.SetCumulativeLost(
      prng_.Rand(-(1 << 23) + 1, (1 << 23) - 1)));
  report_block.SetExtHighestSeqNum(prng_.Rand<uint32_t>());
  report_block.SetJitter(prng_.Rand<uint32_t>());
  report_block.SetLastSr(prng_.Rand<uint32_t>());
  report_block.SetDelayLastSr(prng_.Rand<uint32_t>());
  return report_block;
}

rtcp::SenderReport EventGenerator::NewSenderReport() {
  rtcp::SenderReport sender_report;
  sender_report.SetSenderSsrc(prng_.Rand<uint32_t>());
  sender_report.SetNtp(NtpTime(prng_.Rand<uint32_t>(), prng_.Rand<uint32_t>()));
  sender_report.SetPacketCount(prng_.Rand<uint32_t>());
  sender_report.AddReportBlock(NewReportBlock());
  return sender_report;
}

rtcp::ReceiverReport EventGenerator::NewReceiverReport() {
  rtcp::ReceiverReport receiver_report;
  receiver_report.SetSenderSsrc(prng_.Rand<uint32_t>());
  receiver_report.AddReportBlock(NewReportBlock());
  return receiver_report;
}

std::unique_ptr<RtcEventRtcpPacketIncoming>
EventGenerator::NewRtcpPacketIncoming() {
  // TODO(terelius): Test the other RTCP types too.
  switch (prng_.Rand(0, 1)) {
    case 0: {
      rtcp::SenderReport sender_report = NewSenderReport();
      rtc::Buffer buffer = sender_report.Build();
      return absl::make_unique<RtcEventRtcpPacketIncoming>(buffer);
    }
    case 1: {
      rtcp::ReceiverReport receiver_report = NewReceiverReport();
      rtc::Buffer buffer = receiver_report.Build();
      return absl::make_unique<RtcEventRtcpPacketIncoming>(buffer);
    }
    default:
      RTC_NOTREACHED();
      rtc::Buffer buffer;
      return absl::make_unique<RtcEventRtcpPacketIncoming>(buffer);
  }
}

std::unique_ptr<RtcEventRtcpPacketOutgoing>
EventGenerator::NewRtcpPacketOutgoing() {
  // TODO(terelius): Test the other RTCP types too.
  switch (prng_.Rand(0, 1)) {
    case 0: {
      rtcp::SenderReport sender_report = NewSenderReport();
      rtc::Buffer buffer = sender_report.Build();
      return absl::make_unique<RtcEventRtcpPacketOutgoing>(buffer);
    }
    case 1: {
      rtcp::ReceiverReport receiver_report = NewReceiverReport();
      rtc::Buffer buffer = receiver_report.Build();
      return absl::make_unique<RtcEventRtcpPacketOutgoing>(buffer);
    }
    default:
      RTC_NOTREACHED();
      rtc::Buffer buffer;
      return absl::make_unique<RtcEventRtcpPacketOutgoing>(buffer);
  }
}

void EventGenerator::RandomizeRtpPacket(
    size_t payload_size,
    size_t padding_size,
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map,
    RtpPacket* rtp_packet) {
  constexpr int kMaxPayloadType = 127;
  rtp_packet->SetPayloadType(prng_.Rand(kMaxPayloadType));
  rtp_packet->SetMarker(prng_.Rand<bool>());
  rtp_packet->SetSequenceNumber(prng_.Rand<uint16_t>());
  rtp_packet->SetSsrc(ssrc);
  rtp_packet->SetTimestamp(prng_.Rand<uint32_t>());

  uint32_t csrcs_count = prng_.Rand(0, kMaxCsrcs);
  std::vector<uint32_t> csrcs;
  for (size_t i = 0; i < csrcs_count; i++) {
    csrcs.push_back(prng_.Rand<uint32_t>());
  }
  rtp_packet->SetCsrcs(csrcs);

  if (extension_map.IsRegistered(TransmissionOffset::kId))
    rtp_packet->SetExtension<TransmissionOffset>(prng_.Rand(0x00ffffff));
  if (extension_map.IsRegistered(AudioLevel::kId))
    rtp_packet->SetExtension<AudioLevel>(prng_.Rand<bool>(), prng_.Rand(127));
  if (extension_map.IsRegistered(AbsoluteSendTime::kId))
    rtp_packet->SetExtension<AbsoluteSendTime>(prng_.Rand(0x00ffffff));
  if (extension_map.IsRegistered(VideoOrientation::kId))
    rtp_packet->SetExtension<VideoOrientation>(prng_.Rand(3));
  if (extension_map.IsRegistered(TransportSequenceNumber::kId))
    rtp_packet->SetExtension<TransportSequenceNumber>(prng_.Rand<uint16_t>());

  RTC_CHECK_LE(rtp_packet->headers_size() + payload_size, IP_PACKET_SIZE);

  uint8_t* payload = rtp_packet->AllocatePayload(payload_size);
  RTC_DCHECK(payload != nullptr);
  for (size_t i = 0; i < payload_size; i++) {
    payload[i] = prng_.Rand<uint8_t>();
  }
  RTC_CHECK(rtp_packet->SetPadding(padding_size, &prng_));
}

std::unique_ptr<RtcEventRtpPacketIncoming> EventGenerator::NewRtpPacketIncoming(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map) {
  constexpr size_t kMaxPaddingLength = 224;
  const bool padding = prng_.Rand(0, 9) == 0;  // Let padding be 10% probable.
  const size_t padding_size = !padding ? 0u : prng_.Rand(0u, kMaxPaddingLength);

  // 12 bytes RTP header, 4 bytes for 0xBEDE + alignment, 4 bytes per CSRC.
  constexpr size_t kMaxHeaderSize =
      16 + 4 * kMaxCsrcs + kMaxExtensionSizeBytes * kMaxNumExtensions;

  // In principle, a packet can contain both padding and other payload.
  // Currently, RTC eventlog encoder-parser can only maintain padding length if
  // packet is full padding.
  // TODO(webrtc:9730): Remove the deterministic logic for padding_size > 0.
  size_t payload_size =
      padding_size > 0 ? 0
                       : prng_.Rand(0u, static_cast<uint32_t>(IP_PACKET_SIZE -
                                                              1 - padding_size -
                                                              kMaxHeaderSize));

  RtpPacketReceived rtp_packet(&extension_map);
  RandomizeRtpPacket(payload_size, padding_size, ssrc, extension_map,
                     &rtp_packet);

  return absl::make_unique<RtcEventRtpPacketIncoming>(rtp_packet);
}

std::unique_ptr<RtcEventRtpPacketOutgoing> EventGenerator::NewRtpPacketOutgoing(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map) {
  constexpr size_t kMaxPaddingLength = 224;
  const bool padding = prng_.Rand(0, 9) == 0;  // Let padding be 10% probable.
  const size_t padding_size = !padding ? 0u : prng_.Rand(0u, kMaxPaddingLength);

  // 12 bytes RTP header, 4 bytes for 0xBEDE + alignment, 4 bytes per CSRC.
  constexpr size_t kMaxHeaderSize =
      16 + 4 * kMaxCsrcs + kMaxExtensionSizeBytes * kMaxNumExtensions;

  // In principle,a packet can contain both padding and other payload.
  // Currently, RTC eventlog encoder-parser can only maintain padding length if
  // packet is full padding.
  // TODO(webrtc:9730): Remove the deterministic logic for padding_size > 0.
  size_t payload_size =
      padding_size > 0 ? 0
                       : prng_.Rand(0u, static_cast<uint32_t>(IP_PACKET_SIZE -
                                                              1 - padding_size -
                                                              kMaxHeaderSize));

  RtpPacketToSend rtp_packet(&extension_map,
                             kMaxHeaderSize + payload_size + padding_size);
  RandomizeRtpPacket(payload_size, padding_size, ssrc, extension_map,
                     &rtp_packet);

  int probe_cluster_id = prng_.Rand(0, 100000);
  return absl::make_unique<RtcEventRtpPacketOutgoing>(rtp_packet,
                                                      probe_cluster_id);
}

RtpHeaderExtensionMap EventGenerator::NewRtpHeaderExtensionMap() {
  RtpHeaderExtensionMap extension_map;
  std::vector<int> id(RtpExtension::kOneByteHeaderExtensionMaxId -
                      RtpExtension::kMinId + 1);
  std::iota(id.begin(), id.end(), RtpExtension::kMinId);
  ShuffleInPlace(&prng_, rtc::ArrayView<int>(id));

  if (prng_.Rand<bool>()) {
    extension_map.Register<AudioLevel>(id[0]);
  }
  if (prng_.Rand<bool>()) {
    extension_map.Register<TransmissionOffset>(id[1]);
  }
  if (prng_.Rand<bool>()) {
    extension_map.Register<AbsoluteSendTime>(id[2]);
  }
  if (prng_.Rand<bool>()) {
    extension_map.Register<VideoOrientation>(id[3]);
  }
  if (prng_.Rand<bool>()) {
    extension_map.Register<TransportSequenceNumber>(id[4]);
  }

  return extension_map;
}

std::unique_ptr<RtcEventAudioReceiveStreamConfig>
EventGenerator::NewAudioReceiveStreamConfig(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extensions) {
  auto config = absl::make_unique<rtclog::StreamConfig>();
  // Add SSRCs for the stream.
  config->remote_ssrc = ssrc;
  config->local_ssrc = prng_.Rand<uint32_t>();
  // Add header extensions.
  for (size_t i = 0; i < kMaxNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensions[i].type);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensions[i].name, id);
    }
  }

  return absl::make_unique<RtcEventAudioReceiveStreamConfig>(std::move(config));
}

std::unique_ptr<RtcEventAudioSendStreamConfig>
EventGenerator::NewAudioSendStreamConfig(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extensions) {
  auto config = absl::make_unique<rtclog::StreamConfig>();
  // Add SSRC to the stream.
  config->local_ssrc = ssrc;
  // Add header extensions.
  for (size_t i = 0; i < kMaxNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensions[i].type);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensions[i].name, id);
    }
  }
  return absl::make_unique<RtcEventAudioSendStreamConfig>(std::move(config));
}

std::unique_ptr<RtcEventVideoReceiveStreamConfig>
EventGenerator::NewVideoReceiveStreamConfig(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extensions) {
  auto config = absl::make_unique<rtclog::StreamConfig>();

  // Add SSRCs for the stream.
  config->remote_ssrc = ssrc;
  config->local_ssrc = prng_.Rand<uint32_t>();
  // Add extensions and settings for RTCP.
  config->rtcp_mode =
      prng_.Rand<bool>() ? RtcpMode::kCompound : RtcpMode::kReducedSize;
  config->remb = prng_.Rand<bool>();
  config->rtx_ssrc = prng_.Rand<uint32_t>();
  config->codecs.emplace_back(prng_.Rand<bool>() ? "VP8" : "H264",
                              prng_.Rand(127), prng_.Rand(127));
  // Add header extensions.
  for (size_t i = 0; i < kMaxNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensions[i].type);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensions[i].name, id);
    }
  }
  return absl::make_unique<RtcEventVideoReceiveStreamConfig>(std::move(config));
}

std::unique_ptr<RtcEventVideoSendStreamConfig>
EventGenerator::NewVideoSendStreamConfig(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extensions) {
  auto config = absl::make_unique<rtclog::StreamConfig>();

  config->codecs.emplace_back(prng_.Rand<bool>() ? "VP8" : "H264",
                              prng_.Rand(127), prng_.Rand(127));
  config->local_ssrc = ssrc;
  config->rtx_ssrc = prng_.Rand<uint32_t>();
  // Add header extensions.
  for (size_t i = 0; i < kMaxNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensions[i].type);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensions[i].name, id);
    }
  }
  return absl::make_unique<RtcEventVideoSendStreamConfig>(std::move(config));
}

bool VerifyLoggedAlrStateEvent(const RtcEventAlrState& original_event,
                               const LoggedAlrStateEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.in_alr_ != logged_event.in_alr)
    return false;
  return true;
}

bool VerifyLoggedAudioPlayoutEvent(
    const RtcEventAudioPlayout& original_event,
    const LoggedAudioPlayoutEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.ssrc_ != logged_event.ssrc)
    return false;
  return true;
}

bool VerifyLoggedAudioNetworkAdaptationEvent(
    const RtcEventAudioNetworkAdaptation& original_event,
    const LoggedAudioNetworkAdaptationEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.config_->bitrate_bps != logged_event.config.bitrate_bps)
    return false;
  if (original_event.config_->enable_dtx != logged_event.config.enable_dtx)
    return false;
  if (original_event.config_->enable_fec != logged_event.config.enable_fec)
    return false;
  if (original_event.config_->frame_length_ms !=
      logged_event.config.frame_length_ms)
    return false;
  if (original_event.config_->num_channels != logged_event.config.num_channels)
    return false;
  if (original_event.config_->uplink_packet_loss_fraction !=
      logged_event.config.uplink_packet_loss_fraction)
    return false;

  return true;
}

bool VerifyLoggedBweDelayBasedUpdate(
    const RtcEventBweUpdateDelayBased& original_event,
    const LoggedBweDelayBasedUpdate& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  if (original_event.detector_state_ != logged_event.detector_state)
    return false;
  return true;
}

bool VerifyLoggedBweLossBasedUpdate(
    const RtcEventBweUpdateLossBased& original_event,
    const LoggedBweLossBasedUpdate& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  if (original_event.fraction_loss_ != logged_event.fraction_lost)
    return false;
  if (original_event.total_packets_ != logged_event.expected_packets)
    return false;
  return true;
}

bool VerifyLoggedBweProbeClusterCreatedEvent(
    const RtcEventProbeClusterCreated& original_event,
    const LoggedBweProbeClusterCreatedEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.id_ != logged_event.id)
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  if (original_event.min_probes_ != logged_event.min_packets)
    return false;
  if (original_event.min_bytes_ != logged_event.min_bytes)
    return false;

  return true;
}

bool VerifyLoggedBweProbeFailureEvent(
    const RtcEventProbeResultFailure& original_event,
    const LoggedBweProbeFailureEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.id_ != logged_event.id)
    return false;
  if (original_event.failure_reason_ != logged_event.failure_reason)
    return false;
  return true;
}

bool VerifyLoggedBweProbeSuccessEvent(
    const RtcEventProbeResultSuccess& original_event,
    const LoggedBweProbeSuccessEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.id_ != logged_event.id)
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  return true;
}

bool VerifyLoggedIceCandidatePairConfig(
    const RtcEventIceCandidatePairConfig& original_event,
    const LoggedIceCandidatePairConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.type_ != logged_event.type)
    return false;
  if (original_event.candidate_pair_id_ != logged_event.candidate_pair_id)
    return false;
  if (original_event.candidate_pair_desc_.local_candidate_type !=
      logged_event.local_candidate_type)
    return false;
  if (original_event.candidate_pair_desc_.local_relay_protocol !=
      logged_event.local_relay_protocol)
    return false;
  if (original_event.candidate_pair_desc_.local_network_type !=
      logged_event.local_network_type)
    return false;
  if (original_event.candidate_pair_desc_.local_address_family !=
      logged_event.local_address_family)
    return false;
  if (original_event.candidate_pair_desc_.remote_candidate_type !=
      logged_event.remote_candidate_type)
    return false;
  if (original_event.candidate_pair_desc_.remote_address_family !=
      logged_event.remote_address_family)
    return false;
  if (original_event.candidate_pair_desc_.candidate_pair_protocol !=
      logged_event.candidate_pair_protocol)
    return false;

  return true;
}

bool VerifyLoggedIceCandidatePairEvent(
    const RtcEventIceCandidatePair& original_event,
    const LoggedIceCandidatePairEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.type_ != logged_event.type)
    return false;
  if (original_event.candidate_pair_id_ != logged_event.candidate_pair_id)
    return false;

  return true;
}

bool VerifyLoggedRtpHeader(const RtpPacket& original_header,
                           const RTPHeader& logged_header) {
  // Standard RTP header.
  if (original_header.Marker() != logged_header.markerBit)
    return false;
  if (original_header.PayloadType() != logged_header.payloadType)
    return false;
  if (original_header.SequenceNumber() != logged_header.sequenceNumber)
    return false;
  if (original_header.Timestamp() != logged_header.timestamp)
    return false;
  if (original_header.Ssrc() != logged_header.ssrc)
    return false;
  if (original_header.Csrcs().size() != logged_header.numCSRCs)
    return false;
  for (size_t i = 0; i < logged_header.numCSRCs; i++) {
    if (original_header.Csrcs()[i] != logged_header.arrOfCSRCs[i])
      return false;
  }

  if (original_header.headers_size() != logged_header.headerLength)
    return false;

  // TransmissionOffset header extension.
  if (original_header.HasExtension<TransmissionOffset>() !=
      logged_header.extension.hasTransmissionTimeOffset)
    return false;
  if (logged_header.extension.hasTransmissionTimeOffset) {
    int32_t offset;
    original_header.GetExtension<TransmissionOffset>(&offset);
    if (offset != logged_header.extension.transmissionTimeOffset)
      return false;
  }

  // AbsoluteSendTime header extension.
  if (original_header.HasExtension<AbsoluteSendTime>() !=
      logged_header.extension.hasAbsoluteSendTime)
    return false;
  if (logged_header.extension.hasAbsoluteSendTime) {
    uint32_t sendtime;
    original_header.GetExtension<AbsoluteSendTime>(&sendtime);
    if (sendtime != logged_header.extension.absoluteSendTime)
      return false;
  }

  // TransportSequenceNumber header extension.
  if (original_header.HasExtension<TransportSequenceNumber>() !=
      logged_header.extension.hasTransportSequenceNumber)
    return false;
  if (logged_header.extension.hasTransportSequenceNumber) {
    uint16_t seqnum;
    original_header.GetExtension<TransportSequenceNumber>(&seqnum);
    if (seqnum != logged_header.extension.transportSequenceNumber)
      return false;
  }

  // AudioLevel header extension.
  if (original_header.HasExtension<AudioLevel>() !=
      logged_header.extension.hasAudioLevel)
    return false;
  if (logged_header.extension.hasAudioLevel) {
    bool voice_activity;
    uint8_t audio_level;
    original_header.GetExtension<AudioLevel>(&voice_activity, &audio_level);
    if (voice_activity != logged_header.extension.voiceActivity)
      return false;
    if (audio_level != logged_header.extension.audioLevel)
      return false;
  }

  // VideoOrientation header extension.
  if (original_header.HasExtension<VideoOrientation>() !=
      logged_header.extension.hasVideoRotation)
    return false;
  if (logged_header.extension.hasVideoRotation) {
    uint8_t rotation;
    original_header.GetExtension<VideoOrientation>(&rotation);
    if (ConvertCVOByteToVideoRotation(rotation) !=
        logged_header.extension.videoRotation)
      return false;
  }

  return true;
}

bool VerifyLoggedRtpPacketIncoming(
    const RtcEventRtpPacketIncoming& original_event,
    const LoggedRtpPacketIncoming& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.header_.headers_size() != logged_event.rtp.header_length)
    return false;

  if (original_event.packet_length_ != logged_event.rtp.total_length)
    return false;

  if ((original_event.header_.data()[0] & 0x20) != 0 &&  // has padding
      original_event.packet_length_ - original_event.header_.headers_size() !=
          logged_event.rtp.header.paddingLength) {
    // Currently, RTC eventlog encoder-parser can only maintain padding length
    // if packet is full padding.
    // TODO(webrtc:9730): Change the condition to something like
    // original_event.padding_length_ != logged_event.rtp.header.paddingLength.
    return false;
  }

  if (!VerifyLoggedRtpHeader(original_event.header_, logged_event.rtp.header))
    return false;

  return true;
}

bool VerifyLoggedRtpPacketOutgoing(
    const RtcEventRtpPacketOutgoing& original_event,
    const LoggedRtpPacketOutgoing& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.header_.headers_size() != logged_event.rtp.header_length)
    return false;

  if (original_event.packet_length_ != logged_event.rtp.total_length)
    return false;

  if ((original_event.header_.data()[0] & 0x20) != 0 &&  // has padding
      original_event.packet_length_ - original_event.header_.headers_size() !=
          logged_event.rtp.header.paddingLength) {
    // Currently, RTC eventlog encoder-parser can only maintain padding length
    // if packet is full padding.
    // TODO(webrtc:9730): Change the condition to something like
    // original_event.padding_length_ != logged_event.rtp.header.paddingLength.
    return false;
  }

  // TODO(terelius): Probe cluster ID isn't parsed, used or tested. Unless
  // someone has a strong reason to keep it, it'll be removed.

  if (!VerifyLoggedRtpHeader(original_event.header_, logged_event.rtp.header))
    return false;

  return true;
}

bool VerifyLoggedRtcpPacketIncoming(
    const RtcEventRtcpPacketIncoming& original_event,
    const LoggedRtcpPacketIncoming& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.packet_.size() != logged_event.rtcp.raw_data.size())
    return false;
  if (memcmp(original_event.packet_.data(), logged_event.rtcp.raw_data.data(),
             original_event.packet_.size()) != 0) {
    return false;
  }
  return true;
}

bool VerifyLoggedRtcpPacketOutgoing(
    const RtcEventRtcpPacketOutgoing& original_event,
    const LoggedRtcpPacketOutgoing& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.packet_.size() != logged_event.rtcp.raw_data.size())
    return false;
  if (memcmp(original_event.packet_.data(), logged_event.rtcp.raw_data.data(),
             original_event.packet_.size()) != 0) {
    return false;
  }
  return true;
}

bool VerifyLoggedStartEvent(int64_t start_time_us,
                            const LoggedStartEvent& logged_event) {
  if (start_time_us != logged_event.log_time_us())
    return false;
  return true;
}

bool VerifyLoggedStopEvent(int64_t stop_time_us,
                           const LoggedStopEvent& logged_event) {
  if (stop_time_us != logged_event.log_time_us())
    return false;
  return true;
}

bool VerifyLoggedAudioRecvConfig(
    const RtcEventAudioReceiveStreamConfig& original_event,
    const LoggedAudioRecvConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (*original_event.config_ != logged_event.config)
    return false;
  return true;
}

bool VerifyLoggedAudioSendConfig(
    const RtcEventAudioSendStreamConfig& original_event,
    const LoggedAudioSendConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (*original_event.config_ != logged_event.config)
    return false;
  return true;
}

bool VerifyLoggedVideoRecvConfig(
    const RtcEventVideoReceiveStreamConfig& original_event,
    const LoggedVideoRecvConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (*original_event.config_ != logged_event.config)
    return false;
  return true;
}

bool VerifyLoggedVideoSendConfig(
    const RtcEventVideoSendStreamConfig& original_event,
    const LoggedVideoSendConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  // TODO(terelius): In the past, we allowed storing multiple RtcStreamConfigs
  // in the same RtcEventVideoSendStreamConfig. Look into whether we should drop
  // backwards compatibility in the parser.
  if (logged_event.configs.size() != 1)
    return false;
  if (*original_event.config_ != logged_event.configs[0])
    return false;
  return true;
}

}  // namespace test
}  // namespace webrtc
