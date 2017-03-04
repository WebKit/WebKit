/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/quicdatatransport.h"

#include "webrtc/base/bind.h"
#include "webrtc/base/logging.h"
#include "webrtc/p2p/quic/quictransportchannel.h"
#include "webrtc/p2p/quic/reliablequicstream.h"

namespace webrtc {

QuicDataTransport::QuicDataTransport(
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* network_thread,
    cricket::TransportController* transport_controller)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      network_thread_(network_thread),
      transport_controller_(transport_controller) {
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(network_thread_);
}

QuicDataTransport::~QuicDataTransport() {
  DestroyTransportChannel(quic_transport_channel_);
  LOG(LS_INFO) << "Destroyed the QUIC data transport.";
}

bool QuicDataTransport::SetTransport(const std::string& transport_name) {
  if (transport_name_ == transport_name) {
    // Nothing to do if transport name isn't changing
    return true;
  }

  cricket::QuicTransportChannel* transport_channel =
      CreateTransportChannel(transport_name);
  if (!SetTransportChannel(transport_channel)) {
    DestroyTransportChannel(transport_channel);
    return false;
  }

  transport_name_ = transport_name;
  return true;
}

bool QuicDataTransport::SetTransportChannel(
    cricket::QuicTransportChannel* channel) {
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

  LOG(LS_INFO) << "Setting QuicTransportChannel for QuicDataTransport";
  quic_transport_channel_ = channel;
  quic_transport_channel_->SignalIncomingStream.connect(
      this, &QuicDataTransport::OnIncomingStream);
  bool success = true;
  for (const auto& kv : data_channel_by_id_) {
    rtc::scoped_refptr<QuicDataChannel> data_channel = kv.second;
    if (!data_channel->SetTransportChannel(quic_transport_channel_)) {
      LOG(LS_ERROR)
          << "Cannot set QUIC transport channel for QUIC data channel "
          << kv.first;
      success = false;
    }
  }
  return success;
}

rtc::scoped_refptr<DataChannelInterface> QuicDataTransport::CreateDataChannel(
    const std::string& label,
    const DataChannelInit* config) {
  if (config == nullptr) {
    return nullptr;
  }
  if (data_channel_by_id_.find(config->id) != data_channel_by_id_.end()) {
    LOG(LS_ERROR) << "QUIC data channel already exists with id " << config->id;
    return nullptr;
  }
  rtc::scoped_refptr<QuicDataChannel> data_channel(new QuicDataChannel(
      signaling_thread_, worker_thread_, network_thread_, label, *config));
  if (quic_transport_channel_) {
    if (!data_channel->SetTransportChannel(quic_transport_channel_)) {
      LOG(LS_ERROR)
          << "Cannot set QUIC transport channel for QUIC data channel "
          << config->id;
    }
  }

  data_channel_by_id_[data_channel->id()] = data_channel;
  return data_channel;
}

void QuicDataTransport::DestroyDataChannel(int id) {
  data_channel_by_id_.erase(id);
}

bool QuicDataTransport::HasDataChannel(int id) const {
  return data_channel_by_id_.find(id) != data_channel_by_id_.end();
}

bool QuicDataTransport::HasDataChannels() const {
  return !data_channel_by_id_.empty();
}

// Called when a QUIC stream is created for incoming data.
void QuicDataTransport::OnIncomingStream(cricket::ReliableQuicStream* stream) {
  RTC_DCHECK(stream != nullptr);
  quic_stream_by_id_[stream->id()] = stream;
  stream->SignalDataReceived.connect(this, &QuicDataTransport::OnDataReceived);
}

// Called when the first QUIC stream frame is received for incoming data.
void QuicDataTransport::OnDataReceived(net::QuicStreamId id,
                                       const char* data,
                                       size_t len) {
  const auto& quic_stream_kv = quic_stream_by_id_.find(id);
  if (quic_stream_kv == quic_stream_by_id_.end()) {
    RTC_NOTREACHED();
    return;
  }
  cricket::ReliableQuicStream* stream = quic_stream_kv->second;
  stream->SignalDataReceived.disconnect(this);
  quic_stream_by_id_.erase(id);
  // Read the data channel ID and message ID.
  int data_channel_id;
  uint64_t message_id;
  size_t bytes_read;
  if (!ParseQuicDataMessageHeader(data, len, &data_channel_id, &message_id,
                                  &bytes_read)) {
    LOG(LS_ERROR) << "Could not read QUIC message header from QUIC stream "
                  << id;
    return;
  }
  data += bytes_read;
  len -= bytes_read;
  // Retrieve the data channel which will handle the message.
  const auto& data_channel_kv = data_channel_by_id_.find(data_channel_id);
  if (data_channel_kv == data_channel_by_id_.end()) {
    // TODO(mikescarlett): Implement OPEN message to create a new
    // QuicDataChannel when messages are received for a nonexistent ID.
    LOG(LS_ERROR) << "Data was received for QUIC data channel "
                  << data_channel_id
                  << " but it is not registered to the QuicDataTransport.";
    return;
  }
  QuicDataChannel* data_channel = data_channel_kv->second;
  QuicDataChannel::Message message;
  message.id = message_id;
  message.buffer = rtc::CopyOnWriteBuffer(data, len);
  message.stream = stream;
  data_channel->OnIncomingMessage(std::move(message));
}

cricket::QuicTransportChannel* QuicDataTransport::CreateTransportChannel(
    const std::string& transport_name) {
  DCHECK(transport_controller_->quic());

  cricket::TransportChannel* transport_channel =
      network_thread_->Invoke<cricket::TransportChannel*>(
          RTC_FROM_HERE,
          rtc::Bind(&cricket::TransportController::CreateTransportChannel_n,
                    transport_controller_, transport_name,
                    cricket::ICE_CANDIDATE_COMPONENT_DEFAULT));
  return static_cast<cricket::QuicTransportChannel*>(transport_channel);
}

void QuicDataTransport::DestroyTransportChannel(
    cricket::TransportChannel* transport_channel) {
  if (transport_channel) {
    network_thread_->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&cricket::TransportController::DestroyTransportChannel_n,
                  transport_controller_, transport_channel->transport_name(),
                  cricket::ICE_CANDIDATE_COMPONENT_DEFAULT));
  }
}

}  // namespace webrtc
