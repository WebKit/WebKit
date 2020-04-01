/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/datagram_rtp_transport.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_error.h"
#include "media/base/rtp_utils.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

// Field trials.
// Disable datagram to RTCP feedback translation and enable RTCP feedback loop
// on top of datagram feedback loop. Note that two
// feedback loops add unneccesary overhead, so it's preferable to use feedback
// loop provided by datagram transport and convert datagram ACKs to RTCP ACKs,
// but enabling RTCP feedback loop may be useful in tests and experiments.
const char kDisableDatagramToRtcpFeebackTranslationFieldTrial[] =
    "WebRTC-kDisableDatagramToRtcpFeebackTranslation";

}  // namespace

// Maximum packet size of RTCP feedback packet for allocation. We re-create RTCP
// feedback packets when we get ACK notifications from datagram transport. Our
// rtcp feedback packets contain only 1 ACK, so they are much smaller than 1250.
constexpr size_t kMaxRtcpFeedbackPacketSize = 1250;

DatagramRtpTransport::DatagramRtpTransport(
    const std::vector<RtpExtension>& rtp_header_extensions,
    cricket::IceTransportInternal* ice_transport,
    DatagramTransportInterface* datagram_transport)
    : ice_transport_(ice_transport),
      datagram_transport_(datagram_transport),
      disable_datagram_to_rtcp_feeback_translation_(field_trial::IsEnabled(
          kDisableDatagramToRtcpFeebackTranslationFieldTrial)) {
  // Save extension map for parsing RTP packets (we only need transport
  // sequence numbers).
  const RtpExtension* transport_sequence_number_extension =
      RtpExtension::FindHeaderExtensionByUri(rtp_header_extensions,
                                             TransportSequenceNumber::kUri);

  if (transport_sequence_number_extension != nullptr) {
    rtp_header_extension_map_.Register<TransportSequenceNumber>(
        transport_sequence_number_extension->id);
  } else {
    RTC_LOG(LS_ERROR) << "Transport sequence numbers are not supported in "
                         "datagram transport connection";
  }

  // TODO(sukhanov): Add CHECK to make sure that field trial
  // WebRTC-ExcludeTransportSequenceNumberFromFecFieldTrial is enabled.
  // If feedback loop is translation is enabled, FEC packets must exclude
  // transport sequence numbers, otherwise recovered packets will be corrupt.

  RTC_DCHECK(ice_transport_);
  RTC_DCHECK(datagram_transport_);

  ice_transport_->SignalNetworkRouteChanged.connect(
      this, &DatagramRtpTransport::OnNetworkRouteChanged);
  // Subscribe to DatagramTransport to read incoming packets.
  datagram_transport_->SetDatagramSink(this);
  datagram_transport_->SetTransportStateCallback(this);
}

DatagramRtpTransport::~DatagramRtpTransport() {
  // Unsubscribe from DatagramTransport sinks.
  datagram_transport_->SetDatagramSink(nullptr);
  datagram_transport_->SetTransportStateCallback(nullptr);
}

bool DatagramRtpTransport::SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                                         const rtc::PacketOptions& options,
                                         int flags) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  // Assign and increment datagram_id.
  const DatagramId datagram_id = current_datagram_id_++;

  // Send as is (without extracting transport sequence number) for
  // RTP packets if we are not doing datagram => RTCP feedback translation.
  if (disable_datagram_to_rtcp_feeback_translation_) {
    // Even if we are not extracting transport sequence number we need to
    // propagate "Sent" notification for both RTP and RTCP packets. For this
    // reason we need save options.packet_id in packet map.
    sent_rtp_packet_map_[datagram_id] = SentPacketInfo(options.packet_id);

    return SendDatagram(*packet, datagram_id);
  }

  // Parse RTP packet.
  RtpPacket rtp_packet(&rtp_header_extension_map_);
  // TODO(mellem): Verify that this doesn't mangle something (it shouldn't).
  if (!rtp_packet.Parse(*packet)) {
    RTC_NOTREACHED() << "Failed to parse outgoing RtpPacket, len="
                     << packet->size()
                     << ", options.packet_id=" << options.packet_id;
    return -1;
  }

  // Try to get transport sequence number.
  uint16_t transport_senquence_number;
  if (!rtp_packet.GetExtension<TransportSequenceNumber>(
          &transport_senquence_number)) {
    // Save packet info without transport sequence number.
    sent_rtp_packet_map_[datagram_id] = SentPacketInfo(options.packet_id);

    RTC_LOG(LS_VERBOSE)
        << "Sending rtp packet without transport sequence number, packet="
        << rtp_packet.ToString();

    return SendDatagram(*packet, datagram_id);
  }

  // Save packet info with sequence number and ssrc so we could reconstruct
  // RTCP feedback packet when we receive datagram ACK.
  sent_rtp_packet_map_[datagram_id] = SentPacketInfo(
      options.packet_id, rtp_packet.Ssrc(), transport_senquence_number);

  // Since datagram transport provides feedback and timestamps, we do not need
  // to send transport sequence number, so we remove it from RTP packet. Later
  // when we get Ack for sent datagram, we will re-create RTCP feedback packet.
  if (!rtp_packet.RemoveExtension(TransportSequenceNumber::kId)) {
    RTC_NOTREACHED() << "Failed to remove transport sequence number, packet="
                     << rtp_packet.ToString();
    return -1;
  }

  RTC_LOG(LS_VERBOSE) << "Removed transport_senquence_number="
                      << transport_senquence_number
                      << " from packet=" << rtp_packet.ToString()
                      << ", saved bytes=" << packet->size() - rtp_packet.size();

  return SendDatagram(
      rtc::ArrayView<const uint8_t>(rtp_packet.data(), rtp_packet.size()),
      datagram_id);
}

bool DatagramRtpTransport::SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                                          const rtc::PacketOptions& options,
                                          int flags) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  // Assign and increment datagram_id.
  const DatagramId datagram_id = current_datagram_id_++;

  // Even if we are not extracting transport sequence number we need to
  // propagate "Sent" notification for both RTP and RTCP packets. For this
  // reason we need save options.packet_id in packet map.
  sent_rtp_packet_map_[datagram_id] = SentPacketInfo(options.packet_id);
  return SendDatagram(*packet, datagram_id);
}

bool DatagramRtpTransport::SendDatagram(rtc::ArrayView<const uint8_t> data,
                                        DatagramId datagram_id) {
  return datagram_transport_->SendDatagram(data, datagram_id).ok();
}

void DatagramRtpTransport::OnDatagramReceived(
    rtc::ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  rtc::ArrayView<const char> cdata(reinterpret_cast<const char*>(data.data()),
                                   data.size());
  if (cricket::InferRtpPacketType(cdata) == cricket::RtpPacketType::kRtcp) {
    rtc::CopyOnWriteBuffer buffer(data.data(), data.size());
    SignalRtcpPacketReceived(&buffer, /*packet_time_us=*/-1);
    return;
  }

  // TODO(sukhanov): I am not filling out time, but on my video quality
  // test in WebRTC the time was not set either and higher layers of the stack
  // overwrite -1 with current current rtc time. Leaveing comment for now to
  // make sure it works as expected.
  RtpPacketReceived parsed_packet(&rtp_header_extension_map_);
  if (!parsed_packet.Parse(data)) {
    RTC_LOG(LS_ERROR) << "Failed to parse incoming RTP packet";
    return;
  }
  if (!rtp_demuxer_.OnRtpPacket(parsed_packet)) {
    RTC_LOG(LS_WARNING) << "Failed to demux RTP packet: "
                        << RtpDemuxer::DescribePacket(parsed_packet);
  }
}

void DatagramRtpTransport::OnDatagramSent(DatagramId datagram_id) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  // Find packet_id and propagate OnPacketSent notification.
  const auto& it = sent_rtp_packet_map_.find(datagram_id);
  if (it == sent_rtp_packet_map_.end()) {
    RTC_NOTREACHED() << "Did not find sent packet info for sent datagram_id="
                     << datagram_id;
    return;
  }

  // Also see how DatagramRtpTransport::OnSentPacket handles OnSentPacket
  // notification from ICE in bypass mode.
  rtc::SentPacket sent_packet(/*packet_id=*/it->second.packet_id,
                              rtc::TimeMillis());

  SignalSentPacket(sent_packet);
}

bool DatagramRtpTransport::GetAndRemoveSentPacketInfo(
    DatagramId datagram_id,
    SentPacketInfo* sent_packet_info) {
  RTC_CHECK(sent_packet_info != nullptr);

  const auto& it = sent_rtp_packet_map_.find(datagram_id);
  if (it == sent_rtp_packet_map_.end()) {
    return false;
  }

  *sent_packet_info = it->second;
  sent_rtp_packet_map_.erase(it);
  return true;
}

void DatagramRtpTransport::OnDatagramAcked(const DatagramAck& ack) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  SentPacketInfo sent_packet_info;
  if (!GetAndRemoveSentPacketInfo(ack.datagram_id, &sent_packet_info)) {
    // TODO(sukhanov): If OnDatagramAck() can come after OnDatagramLost(),
    // datagram_id is already deleted and we may need to relax the CHECK below.
    // It's probably OK to ignore such datagrams, because it's been a few RTTs
    // anyway since they were sent.
    RTC_NOTREACHED() << "Did not find sent packet info for datagram_id="
                     << ack.datagram_id;
    return;
  }

  RTC_LOG(LS_VERBOSE) << "Datagram acked, ack.datagram_id=" << ack.datagram_id
                      << ", sent_packet_info.packet_id="
                      << sent_packet_info.packet_id
                      << ", sent_packet_info.transport_sequence_number="
                      << sent_packet_info.transport_sequence_number.value_or(-1)
                      << ", sent_packet_info.ssrc="
                      << sent_packet_info.ssrc.value_or(-1)
                      << ", receive_timestamp_ms="
                      << ack.receive_timestamp.ms();

  // If transport sequence number was not present in RTP packet, we do not need
  // to propagate RTCP feedback.
  if (!sent_packet_info.transport_sequence_number) {
    return;
  }

  // TODO(sukhanov): We noticed that datagram transport implementations can
  // return zero timestamps in the middle of the call. This is workaround to
  // avoid propagating zero timestamps, but we need to understand why we have
  // them in the first place.
  int64_t receive_timestamp_us = ack.receive_timestamp.us();

  if (receive_timestamp_us == 0) {
    receive_timestamp_us = previous_nonzero_timestamp_us_;
  } else {
    previous_nonzero_timestamp_us_ = receive_timestamp_us;
  }

  // Ssrc must be provided in packet info if transport sequence number is set,
  // which is guaranteed by SentPacketInfo constructor.
  RTC_CHECK(sent_packet_info.ssrc);

  // Recreate RTCP feedback packet.
  rtcp::TransportFeedback feedback_packet;
  feedback_packet.SetMediaSsrc(*sent_packet_info.ssrc);

  const uint16_t transport_sequence_number =
      sent_packet_info.transport_sequence_number.value();

  feedback_packet.SetBase(transport_sequence_number, receive_timestamp_us);
  feedback_packet.AddReceivedPacket(transport_sequence_number,
                                    receive_timestamp_us);

  rtc::CopyOnWriteBuffer buffer(kMaxRtcpFeedbackPacketSize);
  size_t index = 0;
  if (!feedback_packet.Create(buffer.data(), &index, buffer.capacity(),
                              nullptr)) {
    RTC_NOTREACHED() << "Failed to create RTCP feedback packet";
    return;
  }

  RTC_CHECK_GT(index, 0);
  RTC_CHECK_LE(index, kMaxRtcpFeedbackPacketSize);

  // Propagage created RTCP packet as normal incoming packet.
  buffer.SetSize(index);
  SignalRtcpPacketReceived(&buffer, /*packet_time_us=*/-1);
}

void DatagramRtpTransport::OnDatagramLost(DatagramId datagram_id) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  RTC_LOG(LS_INFO) << "Datagram lost, datagram_id=" << datagram_id;

  SentPacketInfo sent_packet_info;
  if (!GetAndRemoveSentPacketInfo(datagram_id, &sent_packet_info)) {
    RTC_NOTREACHED() << "Did not find sent packet info for lost datagram_id="
                     << datagram_id;
  }
}

void DatagramRtpTransport::OnStateChanged(MediaTransportState state) {
  state_ = state;
  SignalWritableState(state_ == MediaTransportState::kWritable);
  if (state_ == MediaTransportState::kWritable) {
    SignalReadyToSend(true);
  }
}

const std::string& DatagramRtpTransport::transport_name() const {
  return ice_transport_->transport_name();
}

int DatagramRtpTransport::SetRtpOption(rtc::Socket::Option opt, int value) {
  return ice_transport_->SetOption(opt, value);
}

int DatagramRtpTransport::SetRtcpOption(rtc::Socket::Option opt, int value) {
  return -1;
}

bool DatagramRtpTransport::IsReadyToSend() const {
  return state_ == MediaTransportState::kWritable;
}

bool DatagramRtpTransport::IsWritable(bool /*rtcp*/) const {
  return state_ == MediaTransportState::kWritable;
}

void DatagramRtpTransport::UpdateRtpHeaderExtensionMap(
    const cricket::RtpHeaderExtensions& header_extensions) {
  rtp_header_extension_map_ = RtpHeaderExtensionMap(header_extensions);
}

bool DatagramRtpTransport::RegisterRtpDemuxerSink(
    const RtpDemuxerCriteria& criteria,
    RtpPacketSinkInterface* sink) {
  rtp_demuxer_.RemoveSink(sink);
  return rtp_demuxer_.AddSink(criteria, sink);
}

bool DatagramRtpTransport::UnregisterRtpDemuxerSink(
    RtpPacketSinkInterface* sink) {
  return rtp_demuxer_.RemoveSink(sink);
}

void DatagramRtpTransport::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalNetworkRouteChanged(network_route);
}

}  // namespace webrtc
