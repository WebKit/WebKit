/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/quicdatachannel.h"

#include "webrtc/base/bind.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/logging.h"
#include "webrtc/p2p/quic/quictransportchannel.h"
#include "webrtc/p2p/quic/reliablequicstream.h"

namespace webrtc {

void WriteQuicDataChannelMessageHeader(int data_channel_id,
                                       uint64_t message_id,
                                       rtc::CopyOnWriteBuffer* header) {
  RTC_DCHECK(header);
  // 64-bit varints require at most 10 bytes (7*10 == 70), and 32-bit varints
  // require at most 5 bytes (7*5 == 35).
  size_t max_length = 15;
  rtc::ByteBufferWriter byte_buffer(nullptr, max_length,
                                    rtc::ByteBuffer::ByteOrder::ORDER_HOST);
  byte_buffer.WriteUVarint(data_channel_id);
  byte_buffer.WriteUVarint(message_id);
  header->SetData(byte_buffer.Data(), byte_buffer.Length());
}

bool ParseQuicDataMessageHeader(const char* data,
                                size_t len,
                                int* data_channel_id,
                                uint64_t* message_id,
                                size_t* bytes_read) {
  RTC_DCHECK(data_channel_id);
  RTC_DCHECK(message_id);
  RTC_DCHECK(bytes_read);

  rtc::ByteBufferReader byte_buffer(data, len, rtc::ByteBuffer::ORDER_HOST);
  uint64_t dcid;
  if (!byte_buffer.ReadUVarint(&dcid)) {
    LOG(LS_ERROR) << "Could not read the data channel ID";
    return false;
  }
  *data_channel_id = dcid;
  if (!byte_buffer.ReadUVarint(message_id)) {
    LOG(LS_ERROR) << "Could not read message ID for data channel "
                  << *data_channel_id;
    return false;
  }
  size_t remaining_bytes = byte_buffer.Length();
  *bytes_read = len - remaining_bytes;
  return true;
}

QuicDataChannel::QuicDataChannel(rtc::Thread* signaling_thread,
                                 rtc::Thread* worker_thread,
                                 rtc::Thread* network_thread,
                                 const std::string& label,
                                 const DataChannelInit& config)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      network_thread_(network_thread),
      id_(config.id),
      state_(kConnecting),
      buffered_amount_(0),
      next_message_id_(0),
      label_(label),
      protocol_(config.protocol) {}

QuicDataChannel::~QuicDataChannel() {}

void QuicDataChannel::RegisterObserver(DataChannelObserver* observer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  observer_ = observer;
}

void QuicDataChannel::UnregisterObserver() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  observer_ = nullptr;
}

bool QuicDataChannel::Send(const DataBuffer& buffer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (state_ != kOpen) {
    LOG(LS_ERROR) << "QUIC data channel " << id_
                  << " is not open so cannot send.";
    return false;
  }
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&QuicDataChannel::Send_n, this, buffer));
}

bool QuicDataChannel::Send_n(const DataBuffer& buffer) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Encode and send the header containing the data channel ID and message ID.
  rtc::CopyOnWriteBuffer header;
  WriteQuicDataChannelMessageHeader(id_, ++next_message_id_, &header);
  RTC_DCHECK(quic_transport_channel_);
  cricket::ReliableQuicStream* stream =
      quic_transport_channel_->CreateQuicStream();
  RTC_DCHECK(stream);

  // Send the header with a FIN if the message is empty.
  bool header_fin = (buffer.size() == 0);
  rtc::StreamResult header_result =
      stream->Write(header.data<char>(), header.size(), header_fin);

  if (header_result == rtc::SR_BLOCK) {
    // The header is write blocked but we should try sending the message. Since
    // the ReliableQuicStream queues data in order, if the header is write
    // blocked then the message will be write blocked. Otherwise if the message
    // is sent then the header is sent.
    LOG(LS_INFO) << "Stream " << stream->id()
                 << " header is write blocked for QUIC data channel " << id_;
  } else if (header_result != rtc::SR_SUCCESS) {
    LOG(LS_ERROR) << "Stream " << stream->id()
                  << " failed to write header for QUIC data channel " << id_
                  << ". Unexpected error " << header_result;
    return false;
  }

  // If the message is not empty, then send the message with a FIN.
  bool message_fin = true;
  rtc::StreamResult message_result =
      header_fin ? header_result : stream->Write(buffer.data.data<char>(),
                                                 buffer.size(), message_fin);

  if (message_result == rtc::SR_SUCCESS) {
    // The message is sent and we don't need this QUIC stream.
    LOG(LS_INFO) << "Stream " << stream->id()
                 << " successfully wrote message for QUIC data channel " << id_;
    stream->Close();
    return true;
  }
  // TODO(mikescarlett): Register the ReliableQuicStream's priority to the
  // QuicWriteBlockedList so that the QUIC session doesn't drop messages when
  // the QUIC transport channel becomes unwritable.
  if (message_result == rtc::SR_BLOCK) {
    // The QUIC stream is write blocked, so the message is queued by the QUIC
    // session. If this is due to the QUIC not being writable, it will be sent
    // once QUIC becomes writable again. Otherwise it may be due to exceeding
    // the QUIC flow control limit, in which case the remote peer's QUIC session
    // will tell the QUIC stream to send more data.
    LOG(LS_INFO) << "Stream " << stream->id()
                 << " message is write blocked for QUIC data channel " << id_;
    SetBufferedAmount_w(buffered_amount_ + stream->queued_data_bytes());
    stream->SignalQueuedBytesWritten.connect(
        this, &QuicDataChannel::OnQueuedBytesWritten);
    write_blocked_quic_streams_[stream->id()] = stream;
    // The QUIC stream will be removed from |write_blocked_quic_streams_| once
    // it closes.
    stream->SignalClosed.connect(this,
                                 &QuicDataChannel::OnWriteBlockedStreamClosed);
    return true;
  }
  LOG(LS_ERROR) << "Stream " << stream->id()
                << " failed to write message for QUIC data channel " << id_
                << ". Unexpected error: " << message_result;
  return false;
}

void QuicDataChannel::OnQueuedBytesWritten(net::QuicStreamId stream_id,
                                           uint64_t queued_bytes_written) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  SetBufferedAmount_w(buffered_amount_ - queued_bytes_written);
  const auto& kv = write_blocked_quic_streams_.find(stream_id);
  if (kv == write_blocked_quic_streams_.end()) {
    RTC_NOTREACHED();
    return;
  }
  cricket::ReliableQuicStream* stream = kv->second;
  // True if the QUIC stream is done sending data.
  if (stream->fin_sent()) {
    LOG(LS_INFO) << "Stream " << stream->id()
                 << " successfully wrote data for QUIC data channel " << id_;
    stream->Close();
  }
}

void QuicDataChannel::SetBufferedAmount_w(uint64_t buffered_amount) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  buffered_amount_ = buffered_amount;
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread_,
      rtc::Bind(&QuicDataChannel::OnBufferedAmountChange_s, this,
                buffered_amount));
}

void QuicDataChannel::Close() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (state_ == kClosed || state_ == kClosing) {
    return;
  }
  LOG(LS_INFO) << "Closing QUIC data channel.";
  SetState_s(kClosing);
  worker_thread_->Invoke<void>(RTC_FROM_HERE,
                               rtc::Bind(&QuicDataChannel::Close_w, this));
  SetState_s(kClosed);
}

void QuicDataChannel::Close_w() {
  RTC_DCHECK(worker_thread_->IsCurrent());
  for (auto& kv : incoming_quic_messages_) {
    Message& message = kv.second;
    cricket::ReliableQuicStream* stream = message.stream;
    stream->Close();
  }

  for (auto& kv : write_blocked_quic_streams_) {
    cricket::ReliableQuicStream* stream = kv.second;
    stream->Close();
  }
}

bool QuicDataChannel::SetTransportChannel(
    cricket::QuicTransportChannel* channel) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  if (!channel) {
    LOG(LS_ERROR) << "|channel| is NULL. Cannot set transport channel.";
    return false;
  }
  if (quic_transport_channel_) {
    if (channel == quic_transport_channel_) {
      LOG(LS_WARNING) << "Ignoring duplicate transport channel.";
      return true;
    }
    LOG(LS_ERROR) << "|channel| does not match existing transport channel.";
    return false;
  }

  quic_transport_channel_ = channel;
  LOG(LS_INFO) << "Setting QuicTransportChannel for QUIC data channel " << id_;
  DataState data_channel_state = worker_thread_->Invoke<DataState>(
      RTC_FROM_HERE, rtc::Bind(&QuicDataChannel::SetTransportChannel_w, this));
  SetState_s(data_channel_state);
  return true;
}

DataChannelInterface::DataState QuicDataChannel::SetTransportChannel_w() {
  RTC_DCHECK(worker_thread_->IsCurrent());
  quic_transport_channel_->SignalReadyToSend.connect(
      this, &QuicDataChannel::OnReadyToSend);
  quic_transport_channel_->SignalClosed.connect(
      this, &QuicDataChannel::OnConnectionClosed);
  if (quic_transport_channel_->writable()) {
    return kOpen;
  }
  return kConnecting;
}

void QuicDataChannel::OnIncomingMessage(Message&& message) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_DCHECK(message.stream);
  if (!observer_) {
    LOG(LS_WARNING) << "QUIC data channel " << id_
                    << " received a message but has no observer.";
    message.stream->Close();
    return;
  }
  // A FIN is received if the message fits into a single QUIC stream frame and
  // the remote peer is done sending.
  if (message.stream->fin_received()) {
    LOG(LS_INFO) << "Stream " << message.stream->id()
                 << " has finished receiving data for QUIC data channel "
                 << id_;
    DataBuffer final_message(message.buffer, false);
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
                               rtc::Bind(&QuicDataChannel::OnMessage_s, this,
                                         std::move(final_message)));
    message.stream->Close();
    return;
  }
  // Otherwise the message is divided across multiple QUIC stream frames, so
  // queue the data. OnDataReceived() will be called each time the remaining
  // QUIC stream frames arrive.
  LOG(LS_INFO) << "QUIC data channel " << id_
               << " is queuing incoming data for stream "
               << message.stream->id();
  incoming_quic_messages_[message.stream->id()] = std::move(message);
  message.stream->SignalDataReceived.connect(this,
                                             &QuicDataChannel::OnDataReceived);
  // The QUIC stream will be removed from |incoming_quic_messages_| once it
  // closes.
  message.stream->SignalClosed.connect(
      this, &QuicDataChannel::OnIncomingQueuedStreamClosed);
}

void QuicDataChannel::OnDataReceived(net::QuicStreamId stream_id,
                                     const char* data,
                                     size_t len) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_DCHECK(data);
  const auto& kv = incoming_quic_messages_.find(stream_id);
  if (kv == incoming_quic_messages_.end()) {
    RTC_NOTREACHED();
    return;
  }
  Message& message = kv->second;
  cricket::ReliableQuicStream* stream = message.stream;
  rtc::CopyOnWriteBuffer& received_data = message.buffer;
  // If the QUIC stream has not received a FIN, then the remote peer is not
  // finished sending data.
  if (!stream->fin_received()) {
    received_data.AppendData(data, len);
    return;
  }
  // Otherwise we are done receiving and can provide the data channel observer
  // with the message.
  LOG(LS_INFO) << "Stream " << stream_id
               << " has finished receiving data for QUIC data channel " << id_;
  received_data.AppendData(data, len);
  DataBuffer final_message(std::move(received_data), false);
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread_,
      rtc::Bind(&QuicDataChannel::OnMessage_s, this, std::move(final_message)));
  // Once the stream is closed, OnDataReceived will not fire for the stream.
  stream->Close();
}

void QuicDataChannel::OnReadyToSend(cricket::TransportChannel* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_DCHECK(channel == quic_transport_channel_);
  LOG(LS_INFO) << "QuicTransportChannel is ready to send";
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread_,
      rtc::Bind(&QuicDataChannel::SetState_s, this, kOpen));
}

void QuicDataChannel::OnWriteBlockedStreamClosed(net::QuicStreamId stream_id,
                                                 int error) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  LOG(LS_VERBOSE) << "Write blocked stream " << stream_id << " is closed.";
  write_blocked_quic_streams_.erase(stream_id);
}

void QuicDataChannel::OnIncomingQueuedStreamClosed(net::QuicStreamId stream_id,
                                                   int error) {
  RTC_DCHECK(network_thread_->IsCurrent());
  LOG(LS_VERBOSE) << "Incoming queued stream " << stream_id << " is closed.";
  incoming_quic_messages_.erase(stream_id);
}

void QuicDataChannel::OnConnectionClosed() {
  RTC_DCHECK(worker_thread_->IsCurrent());
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
                             rtc::Bind(&QuicDataChannel::Close, this));
}

void QuicDataChannel::OnMessage_s(const DataBuffer& received_data) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (observer_) {
    observer_->OnMessage(received_data);
  }
}

void QuicDataChannel::SetState_s(DataState state) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (state_ == state || state_ == kClosed) {
    return;
  }
  if (state_ == kClosing && state != kClosed) {
    return;
  }
  LOG(LS_INFO) << "Setting state to " << state << " for QUIC data channel "
               << id_;
  state_ = state;
  if (observer_) {
    observer_->OnStateChange();
  }
}

void QuicDataChannel::OnBufferedAmountChange_s(uint64_t buffered_amount) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (observer_) {
    observer_->OnBufferedAmountChange(buffered_amount);
  }
}

size_t QuicDataChannel::GetNumWriteBlockedStreams() const {
  return write_blocked_quic_streams_.size();
}

size_t QuicDataChannel::GetNumIncomingStreams() const {
  return incoming_quic_messages_.size();
}

}  // namespace webrtc
