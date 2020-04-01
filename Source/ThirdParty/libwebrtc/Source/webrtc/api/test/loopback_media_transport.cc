/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/loopback_media_transport.h"

#include <memory>

#include "absl/algorithm/container.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {

constexpr size_t kLoopbackMaxDatagramSize = 1200;

class WrapperDatagramTransport : public DatagramTransportInterface {
 public:
  explicit WrapperDatagramTransport(DatagramTransportInterface* wrapped)
      : wrapped_(wrapped) {}

  // Datagram transport overrides.
  void Connect(rtc::PacketTransportInternal* packet_transport) override {
    return wrapped_->Connect(packet_transport);
  }

  CongestionControlInterface* congestion_control() override {
    return wrapped_->congestion_control();
  }

  void SetTransportStateCallback(
      MediaTransportStateCallback* callback) override {
    return wrapped_->SetTransportStateCallback(callback);
  }

  RTCError SendDatagram(rtc::ArrayView<const uint8_t> data,
                        DatagramId datagram_id) override {
    return wrapped_->SendDatagram(data, datagram_id);
  }

  size_t GetLargestDatagramSize() const override {
    return wrapped_->GetLargestDatagramSize();
  }

  void SetDatagramSink(DatagramSinkInterface* sink) override {
    return wrapped_->SetDatagramSink(sink);
  }

  std::string GetTransportParameters() const override {
    return wrapped_->GetTransportParameters();
  }

  RTCError SetRemoteTransportParameters(absl::string_view parameters) override {
    return wrapped_->SetRemoteTransportParameters(parameters);
  }

  // Data channel overrides.
  RTCError OpenChannel(int channel_id) override {
    return wrapped_->OpenChannel(channel_id);
  }

  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override {
    return wrapped_->SendData(channel_id, params, buffer);
  }

  RTCError CloseChannel(int channel_id) override {
    return wrapped_->CloseChannel(channel_id);
  }

  void SetDataSink(DataChannelSink* sink) override {
    wrapped_->SetDataSink(sink);
  }

  bool IsReadyToSend() const override { return wrapped_->IsReadyToSend(); }

 private:
  DatagramTransportInterface* wrapped_;
};

}  // namespace

WrapperMediaTransportFactory::WrapperMediaTransportFactory(
    DatagramTransportInterface* wrapped_datagram_transport)
    : wrapped_datagram_transport_(wrapped_datagram_transport) {}

WrapperMediaTransportFactory::WrapperMediaTransportFactory(
    MediaTransportFactory* wrapped)
    : wrapped_factory_(wrapped) {}

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
WrapperMediaTransportFactory::CreateMediaTransport(
    rtc::PacketTransportInternal* packet_transport,
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

RTCErrorOr<std::unique_ptr<DatagramTransportInterface>>
WrapperMediaTransportFactory::CreateDatagramTransport(
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  created_transport_count_++;
  if (wrapped_factory_) {
    return wrapped_factory_->CreateDatagramTransport(network_thread, settings);
  }
  return {
      std::make_unique<WrapperDatagramTransport>(wrapped_datagram_transport_)};
}

std::string WrapperMediaTransportFactory::GetTransportName() const {
  if (wrapped_factory_) {
    return wrapped_factory_->GetTransportName();
  }
  return "wrapped-transport";
}

int WrapperMediaTransportFactory::created_transport_count() const {
  return created_transport_count_;
}

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
WrapperMediaTransportFactory::CreateMediaTransport(
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

MediaTransportPair::MediaTransportPair(rtc::Thread* thread)
    : first_datagram_transport_(thread),
      second_datagram_transport_(thread),
      first_factory_(&first_datagram_transport_),
      second_factory_(&second_datagram_transport_) {
  first_datagram_transport_.Connect(&second_datagram_transport_);
  second_datagram_transport_.Connect(&first_datagram_transport_);
}

MediaTransportPair::~MediaTransportPair() = default;

MediaTransportPair::LoopbackDataChannelTransport::LoopbackDataChannelTransport(
    rtc::Thread* thread)
    : thread_(thread) {}

MediaTransportPair::LoopbackDataChannelTransport::
    ~LoopbackDataChannelTransport() {
  RTC_CHECK(data_sink_ == nullptr);
}

void MediaTransportPair::LoopbackDataChannelTransport::Connect(
    LoopbackDataChannelTransport* other) {
  other_ = other;
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::OpenChannel(
    int channel_id) {
  // No-op.  No need to open channels for the loopback.
  return RTCError::OK();
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_,
                             [this, channel_id, params, buffer] {
                               other_->OnData(channel_id, params.type, buffer);
                             });
  return RTCError::OK();
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::CloseChannel(
    int channel_id) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, channel_id] {
    other_->OnRemoteCloseChannel(channel_id);
    rtc::CritScope lock(&sink_lock_);
    if (data_sink_) {
      data_sink_->OnChannelClosed(channel_id);
    }
  });
  return RTCError::OK();
}

void MediaTransportPair::LoopbackDataChannelTransport::SetDataSink(
    DataChannelSink* sink) {
  rtc::CritScope lock(&sink_lock_);
  data_sink_ = sink;
  if (data_sink_ && ready_to_send_) {
    data_sink_->OnReadyToSend();
  }
}

bool MediaTransportPair::LoopbackDataChannelTransport::IsReadyToSend() const {
  rtc::CritScope lock(&sink_lock_);
  return ready_to_send_;
}

void MediaTransportPair::LoopbackDataChannelTransport::FlushAsyncInvokes() {
  invoker_.Flush(thread_);
}

void MediaTransportPair::LoopbackDataChannelTransport::OnData(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  rtc::CritScope lock(&sink_lock_);
  if (data_sink_) {
    data_sink_->OnDataReceived(channel_id, type, buffer);
  }
}

void MediaTransportPair::LoopbackDataChannelTransport::OnRemoteCloseChannel(
    int channel_id) {
  rtc::CritScope lock(&sink_lock_);
  if (data_sink_) {
    data_sink_->OnChannelClosing(channel_id);
    data_sink_->OnChannelClosed(channel_id);
  }
}

void MediaTransportPair::LoopbackDataChannelTransport::OnReadyToSend(
    bool ready_to_send) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, ready_to_send] {
    rtc::CritScope lock(&sink_lock_);
    ready_to_send_ = ready_to_send;
    // Propagate state to data channel sink, if present.
    if (data_sink_ && ready_to_send_) {
      data_sink_->OnReadyToSend();
    }
  });
}

MediaTransportPair::LoopbackDatagramTransport::LoopbackDatagramTransport(
    rtc::Thread* thread)
    : thread_(thread), dc_transport_(thread) {}

void MediaTransportPair::LoopbackDatagramTransport::Connect(
    LoopbackDatagramTransport* other) {
  other_ = other;
  dc_transport_.Connect(&other->dc_transport_);
}

void MediaTransportPair::LoopbackDatagramTransport::Connect(
    rtc::PacketTransportInternal* packet_transport) {
  if (state_after_connect_) {
    SetState(*state_after_connect_);
  }
}

CongestionControlInterface*
MediaTransportPair::LoopbackDatagramTransport::congestion_control() {
  return nullptr;
}

void MediaTransportPair::LoopbackDatagramTransport::SetTransportStateCallback(
    MediaTransportStateCallback* callback) {
  RTC_DCHECK_RUN_ON(thread_);
  state_callback_ = callback;
  if (state_callback_) {
    state_callback_->OnStateChanged(state_);
  }
}

RTCError MediaTransportPair::LoopbackDatagramTransport::SendDatagram(
    rtc::ArrayView<const uint8_t> data,
    DatagramId datagram_id) {
  rtc::CopyOnWriteBuffer buffer;
  buffer.SetData(data.data(), data.size());
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, thread_, [this, datagram_id, buffer = std::move(buffer)] {
        RTC_DCHECK_RUN_ON(thread_);
        other_->DeliverDatagram(std::move(buffer));
        if (sink_) {
          DatagramAck ack;
          ack.datagram_id = datagram_id;
          ack.receive_timestamp = Timestamp::Micros(rtc::TimeMicros());
          sink_->OnDatagramAcked(ack);
        }
      });
  return RTCError::OK();
}

size_t MediaTransportPair::LoopbackDatagramTransport::GetLargestDatagramSize()
    const {
  return kLoopbackMaxDatagramSize;
}

void MediaTransportPair::LoopbackDatagramTransport::SetDatagramSink(
    DatagramSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(thread_);
  sink_ = sink;
}

std::string
MediaTransportPair::LoopbackDatagramTransport::GetTransportParameters() const {
  return transport_parameters_;
}

RTCError
MediaTransportPair::LoopbackDatagramTransport::SetRemoteTransportParameters(
    absl::string_view remote_parameters) {
  RTC_DCHECK_RUN_ON(thread_);
  if (transport_parameters_comparison_(GetTransportParameters(),
                                       remote_parameters)) {
    return RTCError::OK();
  }
  return RTCError(RTCErrorType::UNSUPPORTED_PARAMETER,
                  "Incompatible remote transport parameters");
}

RTCError MediaTransportPair::LoopbackDatagramTransport::OpenChannel(
    int channel_id) {
  return dc_transport_.OpenChannel(channel_id);
}

RTCError MediaTransportPair::LoopbackDatagramTransport::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  return dc_transport_.SendData(channel_id, params, buffer);
}

RTCError MediaTransportPair::LoopbackDatagramTransport::CloseChannel(
    int channel_id) {
  return dc_transport_.CloseChannel(channel_id);
}

void MediaTransportPair::LoopbackDatagramTransport::SetDataSink(
    DataChannelSink* sink) {
  dc_transport_.SetDataSink(sink);
}

bool MediaTransportPair::LoopbackDatagramTransport::IsReadyToSend() const {
  return dc_transport_.IsReadyToSend();
}

void MediaTransportPair::LoopbackDatagramTransport::SetState(
    MediaTransportState state) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, state] {
    RTC_DCHECK_RUN_ON(thread_);
    state_ = state;
    if (state_callback_) {
      state_callback_->OnStateChanged(state_);
    }
  });
  dc_transport_.OnReadyToSend(state == MediaTransportState::kWritable);
}

void MediaTransportPair::LoopbackDatagramTransport::SetStateAfterConnect(
    MediaTransportState state) {
  state_after_connect_ = state;
}

void MediaTransportPair::LoopbackDatagramTransport::FlushAsyncInvokes() {
  dc_transport_.FlushAsyncInvokes();
}

void MediaTransportPair::LoopbackDatagramTransport::DeliverDatagram(
    rtc::CopyOnWriteBuffer buffer) {
  RTC_DCHECK_RUN_ON(thread_);
  if (sink_) {
    sink_->OnDatagramReceived(buffer);
  }
}

}  // namespace webrtc
