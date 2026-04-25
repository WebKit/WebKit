/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/async_stun_tcp_socket.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/array_view.h"
#include "api/environment/environment.h"
#include "api/transport/stun.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

static const size_t kMaxPacketSize = 64 * 1024;

typedef uint16_t PacketLength;
static const size_t kPacketLenSize = sizeof(PacketLength);
static const size_t kPacketLenOffset = 2;
static const size_t kBufSize = kMaxPacketSize + kStunHeaderSize;
static const size_t kTurnChannelDataHdrSize = 4;

inline bool IsStunMessage(uint16_t msg_type) {
  // The first two bits of a channel data message are 0b01.
  return (msg_type & 0xC000) ? false : true;
}

AsyncStunTCPSocket::AsyncStunTCPSocket(
    const Environment& env,
    absl_nonnull std::unique_ptr<Socket> socket)
    : AsyncTCPSocketBase(std::move(socket), kBufSize), env_(env) {}

int AsyncStunTCPSocket::Send(const void* pv,
                             size_t cb,
                             const AsyncSocketPacketOptions& options) {
  if (cb > kBufSize || cb < kPacketLenSize + kPacketLenOffset) {
    SetError(EMSGSIZE);
    return -1;
  }

  // If we are blocking on send, then silently drop this packet
  if (!IsOutBufferEmpty())
    return static_cast<int>(cb);

  int pad_bytes;
  size_t expected_pkt_len = GetExpectedLength(pv, cb, &pad_bytes);

  // Accepts only complete STUN/ChannelData packets.
  if (cb != expected_pkt_len)
    return -1;

  AppendToOutBuffer(pv, cb);

  RTC_DCHECK(pad_bytes < 4);
  char padding[4] = {0};
  AppendToOutBuffer(padding, pad_bytes);

  int res = FlushOutBuffer();
  if (res <= 0) {
    // drop packet if we made no progress
    ClearOutBuffer();
    return res;
  }

  SentPacketInfo sent_packet(options.packet_id,
                             env_.clock().TimeInMilliseconds());
  NotifySentPacket(this, sent_packet);

  // We claim to have sent the whole thing, even if we only sent partial
  return static_cast<int>(cb);
}

size_t AsyncStunTCPSocket::ProcessInput(ArrayView<const uint8_t> data) {
  SocketAddress remote_addr(GetRemoteAddress());
  // STUN packet - First 4 bytes. Total header size is 20 bytes.
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |0 0|     STUN Message Type     |         Message Length        |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // TURN ChannelData
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |         Channel Number        |            Length             |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  size_t processed_bytes = 0;
  while (true) {
    size_t bytes_left = data.size() - processed_bytes;
    // We need at least 4 bytes to read the STUN or ChannelData packet length.
    if (bytes_left < kPacketLenOffset + kPacketLenSize)
      return processed_bytes;

    int pad_bytes;
    size_t expected_pkt_len = GetExpectedLength(data.data() + processed_bytes,
                                                bytes_left, &pad_bytes);
    size_t actual_length = expected_pkt_len + pad_bytes;

    if (bytes_left < actual_length) {
      return processed_bytes;
    }

    ReceivedIpPacket received_packet(
        data.subview(processed_bytes, expected_pkt_len), remote_addr,
        env_.clock().CurrentTime());
    NotifyPacketReceived(received_packet);
    processed_bytes += actual_length;
  }
}

size_t AsyncStunTCPSocket::GetExpectedLength(const void* data,
                                             size_t /* len */,
                                             int* pad_bytes) {
  *pad_bytes = 0;
  PacketLength pkt_len =
      GetBE16(static_cast<const char*>(data) + kPacketLenOffset);
  size_t expected_pkt_len;
  uint16_t msg_type = GetBE16(data);
  if (IsStunMessage(msg_type)) {
    // STUN message.
    expected_pkt_len = kStunHeaderSize + pkt_len;
  } else {
    // TURN ChannelData message.
    expected_pkt_len = kTurnChannelDataHdrSize + pkt_len;
    // From RFC 5766 section 11.5
    // Over TCP and TLS-over-TCP, the ChannelData message MUST be padded to
    // a multiple of four bytes in order to ensure the alignment of
    // subsequent messages.  The padding is not reflected in the length
    // field of the ChannelData message, so the actual size of a ChannelData
    // message (including padding) is (4 + Length) rounded up to the nearest
    // multiple of 4.  Over UDP, the padding is not required but MAY be
    // included.
    if (expected_pkt_len % 4)
      *pad_bytes = 4 - (expected_pkt_len % 4);
  }
  return expected_pkt_len;
}

}  // namespace webrtc
