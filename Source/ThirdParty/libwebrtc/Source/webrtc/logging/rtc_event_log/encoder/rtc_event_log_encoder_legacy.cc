/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"

#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_logging_started.h"
#include "logging/rtc_event_log/events/rtc_event_logging_stopped.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "rtc_base/checks.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/logging.h"

#ifdef ENABLE_RTC_EVENT_LOG

// *.pb.h files are generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

namespace {
rtclog::DelayBasedBweUpdate::DetectorState ConvertDetectorState(
    BandwidthUsage state) {
  switch (state) {
    case BandwidthUsage::kBwNormal:
      return rtclog::DelayBasedBweUpdate::BWE_NORMAL;
    case BandwidthUsage::kBwUnderusing:
      return rtclog::DelayBasedBweUpdate::BWE_UNDERUSING;
    case BandwidthUsage::kBwOverusing:
      return rtclog::DelayBasedBweUpdate::BWE_OVERUSING;
    case BandwidthUsage::kLast:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog::DelayBasedBweUpdate::BWE_NORMAL;
}

rtclog::BweProbeResult::ResultType ConvertProbeResultType(
    ProbeFailureReason failure_reason) {
  switch (failure_reason) {
    case ProbeFailureReason::kInvalidSendReceiveInterval:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_INTERVAL;
    case ProbeFailureReason::kInvalidSendReceiveRatio:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_RATIO;
    case ProbeFailureReason::kTimeout:
      return rtclog::BweProbeResult::TIMEOUT;
    case ProbeFailureReason::kLast:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog::BweProbeResult::SUCCESS;
}

rtclog::VideoReceiveConfig_RtcpMode ConvertRtcpMode(RtcpMode rtcp_mode) {
  switch (rtcp_mode) {
    case RtcpMode::kCompound:
      return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
    case RtcpMode::kReducedSize:
      return rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE;
    case RtcpMode::kOff:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
}
}  // namespace

std::string RtcEventLogEncoderLegacy::Encode(const RtcEvent& event) {
  switch (event.GetType()) {
    case RtcEvent::Type::AudioNetworkAdaptation: {
      auto& rtc_event =
          static_cast<const RtcEventAudioNetworkAdaptation&>(event);
      return EncodeAudioNetworkAdaptation(rtc_event);
    }

    case RtcEvent::Type::AudioPlayout: {
      auto& rtc_event = static_cast<const RtcEventAudioPlayout&>(event);
      return EncodeAudioPlayout(rtc_event);
    }

    case RtcEvent::Type::AudioReceiveStreamConfig: {
      auto& rtc_event =
          static_cast<const RtcEventAudioReceiveStreamConfig&>(event);
      return EncodeAudioReceiveStreamConfig(rtc_event);
    }

    case RtcEvent::Type::AudioSendStreamConfig: {
      auto& rtc_event =
          static_cast<const RtcEventAudioSendStreamConfig&>(event);
      return EncodeAudioSendStreamConfig(rtc_event);
    }

    case RtcEvent::Type::BweUpdateDelayBased: {
      auto& rtc_event = static_cast<const RtcEventBweUpdateDelayBased&>(event);
      return EncodeBweUpdateDelayBased(rtc_event);
    }

    case RtcEvent::Type::BweUpdateLossBased: {
      auto& rtc_event = static_cast<const RtcEventBweUpdateLossBased&>(event);
      return EncodeBweUpdateLossBased(rtc_event);
    }

    case RtcEvent::Type::LoggingStarted: {
      auto& rtc_event = static_cast<const RtcEventLoggingStarted&>(event);
      return EncodeLoggingStarted(rtc_event);
    }

    case RtcEvent::Type::LoggingStopped: {
      auto& rtc_event = static_cast<const RtcEventLoggingStopped&>(event);
      return EncodeLoggingStopped(rtc_event);
    }

    case RtcEvent::Type::ProbeClusterCreated: {
      auto& rtc_event = static_cast<const RtcEventProbeClusterCreated&>(event);
      return EncodeProbeClusterCreated(rtc_event);
    }

    case RtcEvent::Type::ProbeResultFailure: {
      auto& rtc_event = static_cast<const RtcEventProbeResultFailure&>(event);
      return EncodeProbeResultFailure(rtc_event);
    }

    case RtcEvent::Type::ProbeResultSuccess: {
      auto& rtc_event = static_cast<const RtcEventProbeResultSuccess&>(event);
      return EncodeProbeResultSuccess(rtc_event);
    }

    case RtcEvent::Type::RtcpPacketIncoming: {
      auto& rtc_event = static_cast<const RtcEventRtcpPacketIncoming&>(event);
      return EncodeRtcpPacketIncoming(rtc_event);
    }

    case RtcEvent::Type::RtcpPacketOutgoing: {
      auto& rtc_event = static_cast<const RtcEventRtcpPacketOutgoing&>(event);
      return EncodeRtcpPacketOutgoing(rtc_event);
    }

    case RtcEvent::Type::RtpPacketIncoming: {
      auto& rtc_event = static_cast<const RtcEventRtpPacketIncoming&>(event);
      return EncodeRtpPacketIncoming(rtc_event);
    }

    case RtcEvent::Type::RtpPacketOutgoing: {
      auto& rtc_event = static_cast<const RtcEventRtpPacketOutgoing&>(event);
      return EncodeRtpPacketOutgoing(rtc_event);
    }

    case RtcEvent::Type::VideoReceiveStreamConfig: {
      auto& rtc_event =
          static_cast<const RtcEventVideoReceiveStreamConfig&>(event);
      return EncodeVideoReceiveStreamConfig(rtc_event);
    }

    case RtcEvent::Type::VideoSendStreamConfig: {
      auto& rtc_event =
          static_cast<const RtcEventVideoSendStreamConfig&>(event);
      return EncodeVideoSendStreamConfig(rtc_event);
    }
  }

  int event_type = static_cast<int>(event.GetType());
  RTC_NOTREACHED() << "Unknown event type (" << event_type << ")";
  return "";
}

std::string RtcEventLogEncoderLegacy::EncodeAudioNetworkAdaptation(
    const RtcEventAudioNetworkAdaptation& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT);

  auto audio_network_adaptation =
      rtclog_event.mutable_audio_network_adaptation();
  if (event.config_->bitrate_bps)
    audio_network_adaptation->set_bitrate_bps(*event.config_->bitrate_bps);
  if (event.config_->frame_length_ms)
    audio_network_adaptation->set_frame_length_ms(
        *event.config_->frame_length_ms);
  if (event.config_->uplink_packet_loss_fraction) {
    audio_network_adaptation->set_uplink_packet_loss_fraction(
        *event.config_->uplink_packet_loss_fraction);
  }
  if (event.config_->enable_fec)
    audio_network_adaptation->set_enable_fec(*event.config_->enable_fec);
  if (event.config_->enable_dtx)
    audio_network_adaptation->set_enable_dtx(*event.config_->enable_dtx);
  if (event.config_->num_channels)
    audio_network_adaptation->set_num_channels(*event.config_->num_channels);

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeAudioPlayout(
    const RtcEventAudioPlayout& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::AUDIO_PLAYOUT_EVENT);

  auto playout_event = rtclog_event.mutable_audio_playout_event();
  playout_event->set_local_ssrc(event.ssrc_);

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeAudioReceiveStreamConfig(
    const RtcEventAudioReceiveStreamConfig& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT);

  rtclog::AudioReceiveConfig* receiver_config =
      rtclog_event.mutable_audio_receiver_config();
  receiver_config->set_remote_ssrc(event.config_->remote_ssrc);
  receiver_config->set_local_ssrc(event.config_->local_ssrc);

  for (const auto& e : event.config_->rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeAudioSendStreamConfig(
    const RtcEventAudioSendStreamConfig& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::AUDIO_SENDER_CONFIG_EVENT);

  rtclog::AudioSendConfig* sender_config =
      rtclog_event.mutable_audio_sender_config();

  sender_config->set_ssrc(event.config_->local_ssrc);

  for (const auto& e : event.config_->rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeBweUpdateDelayBased(
    const RtcEventBweUpdateDelayBased& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::DELAY_BASED_BWE_UPDATE);

  auto bwe_event = rtclog_event.mutable_delay_based_bwe_update();
  bwe_event->set_bitrate_bps(event.bitrate_bps_);
  bwe_event->set_detector_state(ConvertDetectorState(event.detector_state_));

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeBweUpdateLossBased(
    const RtcEventBweUpdateLossBased& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::LOSS_BASED_BWE_UPDATE);

  auto bwe_event = rtclog_event.mutable_loss_based_bwe_update();
  bwe_event->set_bitrate_bps(event.bitrate_bps_);
  bwe_event->set_fraction_loss(event.fraction_loss_);
  bwe_event->set_total_packets(event.total_packets_);

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeLoggingStarted(
    const RtcEventLoggingStarted& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::LOG_START);
  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeLoggingStopped(
    const RtcEventLoggingStopped& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::LOG_END);
  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeProbeClusterCreated(
    const RtcEventProbeClusterCreated& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT);

  auto probe_cluster = rtclog_event.mutable_probe_cluster();
  probe_cluster->set_id(event.id_);
  probe_cluster->set_bitrate_bps(event.bitrate_bps_);
  probe_cluster->set_min_packets(event.min_probes_);
  probe_cluster->set_min_bytes(event.min_bytes_);

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeProbeResultFailure(
    const RtcEventProbeResultFailure& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::BWE_PROBE_RESULT_EVENT);

  auto probe_result = rtclog_event.mutable_probe_result();
  probe_result->set_id(event.id_);
  probe_result->set_result(ConvertProbeResultType(event.failure_reason_));

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeProbeResultSuccess(
    const RtcEventProbeResultSuccess& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::BWE_PROBE_RESULT_EVENT);

  auto probe_result = rtclog_event.mutable_probe_result();
  probe_result->set_id(event.id_);
  probe_result->set_result(rtclog::BweProbeResult::SUCCESS);
  probe_result->set_bitrate_bps(event.bitrate_bps_);

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeRtcpPacketIncoming(
    const RtcEventRtcpPacketIncoming& event) {
  return EncodeRtcpPacket(event.timestamp_us_, event.packet_, true);
}

std::string RtcEventLogEncoderLegacy::EncodeRtcpPacketOutgoing(
    const RtcEventRtcpPacketOutgoing& event) {
  return EncodeRtcpPacket(event.timestamp_us_, event.packet_, false);
}

std::string RtcEventLogEncoderLegacy::EncodeRtpPacketIncoming(
    const RtcEventRtpPacketIncoming& event) {
  return EncodeRtpPacket(event.timestamp_us_, event.header_,
                         event.packet_length_, PacedPacketInfo::kNotAProbe,
                         true);
}

std::string RtcEventLogEncoderLegacy::EncodeRtpPacketOutgoing(
    const RtcEventRtpPacketOutgoing& event) {
  return EncodeRtpPacket(event.timestamp_us_, event.header_,
                         event.packet_length_, event.probe_cluster_id_, false);
}

std::string RtcEventLogEncoderLegacy::EncodeVideoReceiveStreamConfig(
    const RtcEventVideoReceiveStreamConfig& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);

  rtclog::VideoReceiveConfig* receiver_config =
      rtclog_event.mutable_video_receiver_config();
  receiver_config->set_remote_ssrc(event.config_->remote_ssrc);
  receiver_config->set_local_ssrc(event.config_->local_ssrc);

  // TODO(perkj): Add field for rsid.
  receiver_config->set_rtcp_mode(ConvertRtcpMode(event.config_->rtcp_mode));
  receiver_config->set_remb(event.config_->remb);

  for (const auto& e : event.config_->rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  for (const auto& d : event.config_->codecs) {
    rtclog::DecoderConfig* decoder = receiver_config->add_decoders();
    decoder->set_name(d.payload_name);
    decoder->set_payload_type(d.payload_type);
    if (d.rtx_payload_type != 0) {
      rtclog::RtxMap* rtx = receiver_config->add_rtx_map();
      rtx->set_payload_type(d.payload_type);
      rtx->mutable_config()->set_rtx_ssrc(event.config_->rtx_ssrc);
      rtx->mutable_config()->set_rtx_payload_type(d.rtx_payload_type);
    }
  }

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeVideoSendStreamConfig(
    const RtcEventVideoSendStreamConfig& event) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(event.timestamp_us_);
  rtclog_event.set_type(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);

  rtclog::VideoSendConfig* sender_config =
      rtclog_event.mutable_video_sender_config();

  // TODO(perkj): rtclog::VideoSendConfig should only contain one SSRC.
  sender_config->add_ssrcs(event.config_->local_ssrc);
  if (event.config_->rtx_ssrc != 0) {
    sender_config->add_rtx_ssrcs(event.config_->rtx_ssrc);
  }

  for (const auto& e : event.config_->rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  // TODO(perkj): rtclog::VideoSendConfig should contain many possible codec
  // configurations.
  for (const auto& codec : event.config_->codecs) {
    sender_config->set_rtx_payload_type(codec.rtx_payload_type);
    rtclog::EncoderConfig* encoder = sender_config->mutable_encoder();
    encoder->set_name(codec.payload_name);
    encoder->set_payload_type(codec.payload_type);

    if (event.config_->codecs.size() > 1) {
      RTC_LOG(WARNING)
          << "LogVideoSendStreamConfig currently only supports one "
          << "codec. Logging codec :" << codec.payload_name;
      break;
    }
  }

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeRtcpPacket(
    int64_t timestamp_us,
    const rtc::Buffer& packet,
    bool is_incoming) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(timestamp_us);
  rtclog_event.set_type(rtclog::Event::RTCP_EVENT);
  rtclog_event.mutable_rtcp_packet()->set_incoming(is_incoming);

  rtcp::CommonHeader header;
  const uint8_t* block_begin = packet.data();
  const uint8_t* packet_end = packet.data() + packet.size();
  RTC_DCHECK(packet.size() <= IP_PACKET_SIZE);
  uint8_t buffer[IP_PACKET_SIZE];
  uint32_t buffer_length = 0;
  while (block_begin < packet_end) {
    if (!header.Parse(block_begin, packet_end - block_begin)) {
      break;  // Incorrect message header.
    }
    const uint8_t* next_block = header.NextPacket();
    uint32_t block_size = next_block - block_begin;
    switch (header.type()) {
      case rtcp::Bye::kPacketType:
      case rtcp::ExtendedJitterReport::kPacketType:
      case rtcp::ExtendedReports::kPacketType:
      case rtcp::Psfb::kPacketType:
      case rtcp::ReceiverReport::kPacketType:
      case rtcp::Rtpfb::kPacketType:
      case rtcp::SenderReport::kPacketType:
        // We log sender reports, receiver reports, bye messages
        // inter-arrival jitter, third-party loss reports, payload-specific
        // feedback and extended reports.
        memcpy(buffer + buffer_length, block_begin, block_size);
        buffer_length += block_size;
        break;
      case rtcp::App::kPacketType:
      case rtcp::Sdes::kPacketType:
      default:
        // We don't log sender descriptions, application defined messages
        // or message blocks of unknown type.
        break;
    }

    block_begin += block_size;
  }
  rtclog_event.mutable_rtcp_packet()->set_packet_data(buffer, buffer_length);

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::EncodeRtpPacket(
    int64_t timestamp_us,
    const webrtc::RtpPacket& header,
    size_t packet_length,
    int probe_cluster_id,
    bool is_incoming) {
  rtclog::Event rtclog_event;
  rtclog_event.set_timestamp_us(timestamp_us);
  rtclog_event.set_type(rtclog::Event::RTP_EVENT);

  rtclog_event.mutable_rtp_packet()->set_incoming(is_incoming);
  rtclog_event.mutable_rtp_packet()->set_packet_length(packet_length);
  rtclog_event.mutable_rtp_packet()->set_header(header.data(), header.size());
  if (probe_cluster_id != PacedPacketInfo::kNotAProbe) {
    RTC_DCHECK(!is_incoming);
    rtclog_event.mutable_rtp_packet()->set_probe_cluster_id(probe_cluster_id);
  }

  return Serialize(&rtclog_event);
}

std::string RtcEventLogEncoderLegacy::Serialize(rtclog::Event* event) {
  // Even though we're only serializing a single event during this call, what
  // we intend to get is a list of events, with a tag and length preceding
  // each actual event. To produce that, we serialize a list of a single event.
  // If we later concatenate several results from this function, the result will
  // be a proper concatenation of all those events.

  rtclog::EventStream event_stream;
  event_stream.add_stream();

  // As a tweak, we swap the new event into the event-stream, write that to
  // file, then swap back. This saves on some copying, while making sure that
  // the caller wouldn't be surprised by Serialize() modifying the object.
  rtclog::Event* output_event = event_stream.mutable_stream(0);
  output_event->Swap(event);

  std::string output_string = event_stream.SerializeAsString();
  RTC_DCHECK(!output_string.empty());

  // When the function returns, the original Event will be unchanged.
  output_event->Swap(event);

  return output_string;
}

}  // namespace webrtc

#endif  // ENABLE_RTC_EVENT_LOG
