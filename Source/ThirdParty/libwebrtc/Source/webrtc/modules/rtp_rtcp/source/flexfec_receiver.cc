/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"

#include <utility>

#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

namespace {

using Packet = ForwardErrorCorrection::Packet;
using ReceivedPacket = ForwardErrorCorrection::ReceivedPacket;

// Minimum header size (in bytes) of a well-formed non-singular FlexFEC packet.
constexpr size_t kMinFlexfecHeaderSize = 20;

// How often to log the recovered packets to the text log.
constexpr int kPacketLogIntervalMs = 10000;

}  // namespace

FlexfecReceiver::FlexfecReceiver(uint32_t ssrc,
                                 uint32_t protected_media_ssrc,
                                 RecoveredPacketReceiver* callback)
    : ssrc_(ssrc),
      protected_media_ssrc_(protected_media_ssrc),
      erasure_code_(ForwardErrorCorrection::CreateFlexfec()),
      callback_(callback),
      clock_(Clock::GetRealTimeClock()),
      last_recovered_packet_ms_(-1) {
  // It's OK to create this object on a different thread/task queue than
  // the one used during main operation.
  sequence_checker_.Detach();
}

FlexfecReceiver::~FlexfecReceiver() = default;

bool FlexfecReceiver::AddAndProcessReceivedPacket(const uint8_t* packet,
                                                  size_t packet_length) {
  RTC_DCHECK(sequence_checker_.CalledSequentially());

  if (!AddReceivedPacket(packet, packet_length)) {
    return false;
  }
  return ProcessReceivedPackets();
}

FecPacketCounter FlexfecReceiver::GetPacketCounter() const {
  RTC_DCHECK(sequence_checker_.CalledSequentially());
  return packet_counter_;
}

bool FlexfecReceiver::AddReceivedPacket(const uint8_t* packet,
                                        size_t packet_length) {
  RTC_DCHECK(sequence_checker_.CalledSequentially());

  // RTP packets with a full base header (12 bytes), but without payload,
  // could conceivably be useful in the decoding. Therefore we check
  // with a strict inequality here.
  if (packet_length < kRtpHeaderSize) {
    LOG(LS_WARNING) << "Truncated packet, discarding.";
    return false;
  }

  // TODO(brandtr): Consider how to handle received FlexFEC packets and
  // the bandwidth estimator.
  RtpPacketReceived parsed_packet;
  if (!parsed_packet.Parse(packet, packet_length)) {
    return false;
  }

  // Demultiplex based on SSRC, and insert into erasure code decoder.
  std::unique_ptr<ReceivedPacket> received_packet(new ReceivedPacket());
  received_packet->seq_num = parsed_packet.SequenceNumber();
  received_packet->ssrc = parsed_packet.Ssrc();
  if (received_packet->ssrc == ssrc_) {
    // This is a FEC packet belonging to this FlexFEC stream.
    if (parsed_packet.payload_size() < kMinFlexfecHeaderSize) {
      LOG(LS_WARNING) << "Truncated FlexFEC packet, discarding.";
      return false;
    }
    received_packet->is_fec = true;
    ++packet_counter_.num_fec_packets;
    // Insert packet payload into erasure code.
    // TODO(brandtr): Remove this memcpy when the FEC packet classes
    // are using COW buffers internally.
    received_packet->pkt = rtc::scoped_refptr<Packet>(new Packet());
    memcpy(received_packet->pkt->data, parsed_packet.payload(),
           parsed_packet.payload_size());
    received_packet->pkt->length = parsed_packet.payload_size();
  } else {
    // This is a media packet, or a FlexFEC packet belonging to some
    // other FlexFEC stream.
    if (received_packet->ssrc != protected_media_ssrc_) {
      return false;
    }
    received_packet->is_fec = false;
    // Insert entire packet into erasure code.
    // TODO(brandtr): Remove this memcpy too.
    received_packet->pkt = rtc::scoped_refptr<Packet>(new Packet());
    memcpy(received_packet->pkt->data, parsed_packet.data(),
           parsed_packet.size());
    received_packet->pkt->length = parsed_packet.size();
  }
  received_packets_.push_back(std::move(received_packet));
  ++packet_counter_.num_packets;

  return true;
}

// Note that the implementation of this member function and the implementation
// in UlpfecReceiver::ProcessReceivedFec() are slightly different.
// This implementation only returns _recovered_ media packets through the
// callback, whereas the implementation in UlpfecReceiver returns _all inserted_
// media packets through the callback. The latter behaviour makes sense
// for ULPFEC, since the ULPFEC receiver is owned by the RtpStreamReceiver.
// Here, however, the received media pipeline is more decoupled from the
// FlexFEC decoder, and we therefore do not interfere with the reception
// of non-recovered media packets.
bool FlexfecReceiver::ProcessReceivedPackets() {
  RTC_DCHECK(sequence_checker_.CalledSequentially());

  // Decode.
  if (!received_packets_.empty()) {
    if (erasure_code_->DecodeFec(&received_packets_, &recovered_packets_) !=
        0) {
      return false;
    }
  }
  // Return recovered packets through callback.
  for (const auto& recovered_packet : recovered_packets_) {
    if (recovered_packet->returned) {
      continue;
    }
    ++packet_counter_.num_recovered_packets;
    if (!callback_->OnRecoveredPacket(recovered_packet->pkt->data,
                                      recovered_packet->pkt->length)) {
      return false;
    }
    recovered_packet->returned = true;
    // Periodically log the incoming packets.
    int64_t now_ms = clock_->TimeInMilliseconds();
    if (now_ms - last_recovered_packet_ms_ > kPacketLogIntervalMs) {
      uint32_t media_ssrc =
          ForwardErrorCorrection::ParseSsrc(recovered_packet->pkt->data);
      LOG(LS_INFO) << "Recovered media packet with SSRC: " << media_ssrc
                   << " from FlexFEC stream with SSRC: " << ssrc_ << ".";
      last_recovered_packet_ms_ = now_ms;
    }
  }
  return true;
}

}  // namespace webrtc
