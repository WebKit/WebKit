/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/composite_data_channel_transport.h"

#include <utility>

#include "absl/algorithm/container.h"

namespace webrtc {

CompositeDataChannelTransport::CompositeDataChannelTransport(
    std::vector<DataChannelTransportInterface*> transports)
    : transports_(std::move(transports)) {
  for (auto transport : transports_) {
    transport->SetDataSink(this);
  }
}

CompositeDataChannelTransport::~CompositeDataChannelTransport() {
  for (auto transport : transports_) {
    transport->SetDataSink(nullptr);
  }
}

void CompositeDataChannelTransport::SetSendTransport(
    DataChannelTransportInterface* send_transport) {
  if (!absl::c_linear_search(transports_, send_transport)) {
    return;
  }
  send_transport_ = send_transport;
  // NB:  OnReadyToSend() checks if we're actually ready to send, and signals
  // |sink_| if appropriate.  This signal is required upon setting the sink.
  OnReadyToSend();
}

void CompositeDataChannelTransport::RemoveTransport(
    DataChannelTransportInterface* transport) {
  RTC_DCHECK(transport != send_transport_) << "Cannot remove send transport";

  auto it = absl::c_find(transports_, transport);
  if (it == transports_.end()) {
    return;
  }

  transport->SetDataSink(nullptr);
  transports_.erase(it);
}

RTCError CompositeDataChannelTransport::OpenChannel(int channel_id) {
  RTCError error = RTCError::OK();
  for (auto transport : transports_) {
    RTCError e = transport->OpenChannel(channel_id);
    if (!e.ok()) {
      error = std::move(e);
    }
  }
  return error;
}

RTCError CompositeDataChannelTransport::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (send_transport_) {
    return send_transport_->SendData(channel_id, params, buffer);
  }
  return RTCError(RTCErrorType::NETWORK_ERROR, "Send transport is not ready");
}

RTCError CompositeDataChannelTransport::CloseChannel(int channel_id) {
  if (send_transport_) {
    return send_transport_->CloseChannel(channel_id);
  }
  return RTCError(RTCErrorType::NETWORK_ERROR, "Send transport is not ready");
}

void CompositeDataChannelTransport::SetDataSink(DataChannelSink* sink) {
  sink_ = sink;
  // NB:  OnReadyToSend() checks if we're actually ready to send, and signals
  // |sink_| if appropriate.  This signal is required upon setting the sink.
  OnReadyToSend();
}

bool CompositeDataChannelTransport::IsReadyToSend() const {
  return send_transport_ && send_transport_->IsReadyToSend();
}

void CompositeDataChannelTransport::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (sink_) {
    sink_->OnDataReceived(channel_id, type, buffer);
  }
}

void CompositeDataChannelTransport::OnChannelClosing(int channel_id) {
  if (sink_) {
    sink_->OnChannelClosing(channel_id);
  }
}

void CompositeDataChannelTransport::OnChannelClosed(int channel_id) {
  if (sink_) {
    sink_->OnChannelClosed(channel_id);
  }
}

void CompositeDataChannelTransport::OnReadyToSend() {
  if (sink_ && send_transport_ && send_transport_->IsReadyToSend()) {
    sink_->OnReadyToSend();
  }
}

}  // namespace webrtc
