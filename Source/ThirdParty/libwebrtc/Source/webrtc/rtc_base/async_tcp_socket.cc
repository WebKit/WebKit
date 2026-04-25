/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_tcp_socket.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/environment/environment.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"

#if defined(WEBRTC_POSIX)
#include <cerrno>
#endif  // WEBRTC_POSIX

namespace webrtc {

static const size_t kMaxPacketSize = 64 * 1024;

typedef uint16_t PacketLength;
static const size_t kPacketLenSize = sizeof(PacketLength);

static const size_t kBufSize = kMaxPacketSize + kPacketLenSize;

// The input buffer will be resized so that at least kMinimumRecvSize bytes can
// be received (but it will not grow above the maximum size passed to the
// constructor).
static const size_t kMinimumRecvSize = 128;

static const int kListenBacklog = 5;

AsyncTCPSocketBase::AsyncTCPSocketBase(
    absl_nonnull std::unique_ptr<Socket> socket,
    size_t max_packet_size)
    : socket_(std::move(socket)),
      max_insize_(max_packet_size),
      max_outsize_(max_packet_size) {
  inbuf_.EnsureCapacity(kMinimumRecvSize);

  socket_->SubscribeConnectEvent(
      this, [this](Socket* socket) { OnConnectEvent(socket); });
  socket_->SubscribeReadEvent(this,
                              [this](Socket* socket) { OnReadEvent(socket); });
  socket_->SubscribeWriteEvent(
      this, [this](Socket* socket) { OnWriteEvent(socket); });
  socket_->SubscribeCloseEvent(
      this, [this](Socket* socket, int error) { OnCloseEvent(socket, error); });
}

AsyncTCPSocketBase::~AsyncTCPSocketBase() {}

SocketAddress AsyncTCPSocketBase::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

SocketAddress AsyncTCPSocketBase::GetRemoteAddress() const {
  return socket_->GetRemoteAddress();
}

int AsyncTCPSocketBase::Close() {
  return socket_->Close();
}

AsyncTCPSocket::State AsyncTCPSocketBase::GetState() const {
  switch (socket_->GetState()) {
    case Socket::CS_CLOSED:
      return STATE_CLOSED;
    case Socket::CS_CONNECTING:
      return STATE_CONNECTING;
    case Socket::CS_CONNECTED:
      return STATE_CONNECTED;
    default:
      RTC_DCHECK_NOTREACHED();
      return STATE_CLOSED;
  }
}

int AsyncTCPSocketBase::GetOption(Socket::Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int AsyncTCPSocketBase::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int AsyncTCPSocketBase::GetError() const {
  return socket_->GetError();
}

void AsyncTCPSocketBase::SetError(int error) {
  return socket_->SetError(error);
}

int AsyncTCPSocketBase::SendTo(const void* pv,
                               size_t cb,
                               const SocketAddress& addr,
                               const AsyncSocketPacketOptions& options) {
  const SocketAddress& remote_address = GetRemoteAddress();
  if (addr == remote_address)
    return Send(pv, cb, options);
  // Remote address may be empty if there is a sudden network change.
  RTC_DCHECK(remote_address.IsNil());
  socket_->SetError(ENOTCONN);
  return -1;
}

int AsyncTCPSocketBase::FlushOutBuffer() {
  RTC_DCHECK_GT(outbuf_.size(), 0);
  ArrayView<uint8_t> view = outbuf_;
  int res;
  while (!view.empty()) {
    res = socket_->Send(view.data(), view.size());
    if (res <= 0) {
      break;
    }
    if (static_cast<size_t>(res) > view.size()) {
      RTC_DCHECK_NOTREACHED();
      res = -1;
      break;
    }
    view = view.subview(res);
  }
  if (res > 0) {
    // The output buffer may have been written out over multiple partial Send(),
    // so reconstruct the total written length.
    RTC_DCHECK_EQ(view.size(), 0);
    res = outbuf_.size();
    outbuf_.Clear();
  } else {
    // There was an error when calling Send(), so there will still be data left
    // to send at a later point.
    RTC_DCHECK_GT(view.size(), 0);
    // In the special case of EWOULDBLOCK, signal that we had a partial write.
    if (socket_->GetError() == EWOULDBLOCK) {
      res = outbuf_.size() - view.size();
    }
    if (view.size() < outbuf_.size()) {
      memmove(outbuf_.data(), view.data(), view.size());
      outbuf_.SetSize(view.size());
    }
  }
  return res;
}

void AsyncTCPSocketBase::AppendToOutBuffer(const void* pv, size_t cb) {
  RTC_DCHECK(outbuf_.size() + cb <= max_outsize_);
  outbuf_.AppendData(static_cast<const uint8_t*>(pv), cb);
}

void AsyncTCPSocketBase::OnConnectEvent(Socket* socket) {
  NotifyConnect(this);
}

void AsyncTCPSocketBase::OnReadEvent(Socket* socket) {
  RTC_DCHECK(socket_.get() == socket);

  size_t total_recv = 0;
  while (true) {
    size_t free_size = inbuf_.capacity() - inbuf_.size();
    if (free_size < kMinimumRecvSize && inbuf_.capacity() < max_insize_) {
      inbuf_.EnsureCapacity(std::min(max_insize_, inbuf_.capacity() * 2));
      free_size = inbuf_.capacity() - inbuf_.size();
    }

    int len = socket_->Recv(inbuf_.data() + inbuf_.size(), free_size, nullptr);
    if (len < 0) {
      // TODO(stefan): Do something better like forwarding the error to the
      // user.
      if (!socket_->IsBlocking()) {
        RTC_LOG(LS_ERROR) << "Recv() returned error: " << socket_->GetError();
      }
      break;
    }

    total_recv += len;
    inbuf_.SetSize(inbuf_.size() + len);
    if (!len || static_cast<size_t>(len) < free_size) {
      break;
    }
  }

  if (!total_recv) {
    return;
  }

  size_t processed = ProcessInput(inbuf_);
  size_t bytes_remaining = inbuf_.size() - processed;
  if (processed > inbuf_.size()) {
    RTC_LOG(LS_ERROR) << "input buffer overflow";
    RTC_DCHECK_NOTREACHED();
    inbuf_.Clear();
  } else {
    if (bytes_remaining > 0) {
      memmove(inbuf_.data(), inbuf_.data() + processed, bytes_remaining);
    }
    inbuf_.SetSize(bytes_remaining);
  }
}

void AsyncTCPSocketBase::OnWriteEvent(Socket* socket) {
  RTC_DCHECK(socket_.get() == socket);

  if (!outbuf_.empty()) {
    FlushOutBuffer();
  }

  if (outbuf_.empty()) {
    NotifyReadyToSend(this);
  }
}

void AsyncTCPSocketBase::OnCloseEvent(Socket* socket, int error) {
  NotifyClosed(error);
}

AsyncTCPSocket::AsyncTCPSocket(const Environment& env,
                               absl_nonnull std::unique_ptr<Socket> socket)
    : AsyncTCPSocketBase(std::move(socket), kBufSize), env_(env) {}

int AsyncTCPSocket::Send(const void* pv,
                         size_t cb,
                         const AsyncSocketPacketOptions& options) {
  if (cb > kBufSize) {
    SetError(EMSGSIZE);
    return -1;
  }

  // If we are blocking on send, then silently drop this packet
  if (!IsOutBufferEmpty())
    return static_cast<int>(cb);

  PacketLength pkt_len = HostToNetwork16(static_cast<PacketLength>(cb));
  AppendToOutBuffer(&pkt_len, kPacketLenSize);
  AppendToOutBuffer(pv, cb);

  int res = FlushOutBuffer();
  if (res <= 0) {
    // drop packet if we made no progress
    ClearOutBuffer();
    return res;
  }

  SentPacketInfo sent_packet(options.packet_id,
                             env_.clock().TimeInMilliseconds(),
                             options.info_signaled_after_sent);
  CopySocketInformationToPacketInfo(cb, *this, &sent_packet.info);
  NotifySentPacket(this, sent_packet);

  // We claim to have sent the whole thing, even if we only sent partial
  return static_cast<int>(cb);
}

size_t AsyncTCPSocket::ProcessInput(ArrayView<const uint8_t> data) {
  SocketAddress remote_addr(GetRemoteAddress());

  size_t processed_bytes = 0;
  while (true) {
    size_t bytes_left = data.size() - processed_bytes;
    if (bytes_left < kPacketLenSize)
      return processed_bytes;

    PacketLength pkt_len = GetBE16(data.data() + processed_bytes);
    if (bytes_left < kPacketLenSize + pkt_len)
      return processed_bytes;

    ReceivedIpPacket received_packet(
        data.subview(processed_bytes + kPacketLenSize, pkt_len), remote_addr,
        env_.clock().CurrentTime());
    NotifyPacketReceived(received_packet);
    processed_bytes += kPacketLenSize + pkt_len;
  }
}

AsyncTcpListenSocket::AsyncTcpListenSocket(const Environment& env,
                                           std::unique_ptr<Socket> socket)
    : env_(env), socket_(std::move(socket)) {
  RTC_DCHECK(socket_.get() != nullptr);
  socket_->SubscribeReadEvent(this,
                              [this](Socket* socket) { OnReadEvent(socket); });
  if (socket_->Listen(kListenBacklog) < 0) {
    RTC_LOG(LS_ERROR) << "Listen() failed with error " << socket_->GetError();
  }
}

AsyncTcpListenSocket::State AsyncTcpListenSocket::GetState() const {
  switch (socket_->GetState()) {
    case Socket::CS_CLOSED:
      return State::kClosed;
    case Socket::CS_CONNECTING:
      return State::kBound;
    default:
      RTC_DCHECK_NOTREACHED();
      return State::kClosed;
  }
}

SocketAddress AsyncTcpListenSocket::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

void AsyncTcpListenSocket::OnReadEvent(Socket* socket) {
  RTC_DCHECK(socket_.get() == socket);

  SocketAddress address;
  Socket* new_socket = socket->Accept(&address);
  if (!new_socket) {
    // TODO(stefan): Do something better like forwarding the error
    // to the user.
    RTC_LOG(LS_ERROR) << "TCP accept failed with error " << socket_->GetError();
    return;
  }

  HandleIncomingConnection(absl::WrapUnique(new_socket));

  // Prime a read event in case data is waiting.
  new_socket->NotifyReadEvent(new_socket);
}

void AsyncTcpListenSocket::HandleIncomingConnection(
    std::unique_ptr<Socket> socket) {
  NotifyNewConnection(this, new AsyncTCPSocket(env_, std::move(socket)));
}

}  // namespace webrtc
