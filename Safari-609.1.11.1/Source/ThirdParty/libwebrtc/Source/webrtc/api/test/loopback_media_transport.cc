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

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {

// Wrapper used to hand out unique_ptrs to loopback media transports without
// ownership changes.
class WrapperMediaTransport : public MediaTransportInterface {
 public:
  explicit WrapperMediaTransport(MediaTransportInterface* wrapped)
      : wrapped_(wrapped) {}

  RTCError SendAudioFrame(uint64_t channel_id,
                          MediaTransportEncodedAudioFrame frame) override {
    return wrapped_->SendAudioFrame(channel_id, std::move(frame));
  }

  RTCError SendVideoFrame(
      uint64_t channel_id,
      const MediaTransportEncodedVideoFrame& frame) override {
    return wrapped_->SendVideoFrame(channel_id, frame);
  }

  void SetKeyFrameRequestCallback(
      MediaTransportKeyFrameRequestCallback* callback) override {
    wrapped_->SetKeyFrameRequestCallback(callback);
  }

  RTCError RequestKeyFrame(uint64_t channel_id) override {
    return wrapped_->RequestKeyFrame(channel_id);
  }

  void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override {
    wrapped_->SetReceiveAudioSink(sink);
  }

  void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override {
    wrapped_->SetReceiveVideoSink(sink);
  }

  void AddTargetTransferRateObserver(
      TargetTransferRateObserver* observer) override {
    wrapped_->AddTargetTransferRateObserver(observer);
  }

  void RemoveTargetTransferRateObserver(
      TargetTransferRateObserver* observer) override {
    wrapped_->RemoveTargetTransferRateObserver(observer);
  }

  void SetMediaTransportStateCallback(
      MediaTransportStateCallback* callback) override {
    wrapped_->SetMediaTransportStateCallback(callback);
  }

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

  void SetAllocatedBitrateLimits(
      const MediaTransportAllocatedBitrateLimits& limits) override {}

  absl::optional<std::string> GetTransportParametersOffer() const override {
    return wrapped_->GetTransportParametersOffer();
  }

 private:
  MediaTransportInterface* wrapped_;
};

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
    MediaTransportInterface* wrapped_media_transport,
    DatagramTransportInterface* wrapped_datagram_transport)
    : wrapped_media_transport_(wrapped_media_transport),
      wrapped_datagram_transport_(wrapped_datagram_transport) {}

WrapperMediaTransportFactory::WrapperMediaTransportFactory(
    MediaTransportFactory* wrapped)
    : wrapped_factory_(wrapped) {}

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
WrapperMediaTransportFactory::CreateMediaTransport(
    rtc::PacketTransportInternal* packet_transport,
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  created_transport_count_++;
  if (wrapped_factory_) {
    return wrapped_factory_->CreateMediaTransport(packet_transport,
                                                  network_thread, settings);
  }
  return {absl::make_unique<WrapperMediaTransport>(wrapped_media_transport_)};
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
      absl::make_unique<WrapperDatagramTransport>(wrapped_datagram_transport_)};
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
  created_transport_count_++;
  if (wrapped_factory_) {
    return wrapped_factory_->CreateMediaTransport(network_thread, settings);
  }
  return {absl::make_unique<WrapperMediaTransport>(wrapped_media_transport_)};
}

MediaTransportPair::MediaTransportPair(rtc::Thread* thread)
    : first_(thread),
      second_(thread),
      first_datagram_transport_(thread),
      second_datagram_transport_(thread),
      first_factory_(&first_, &first_datagram_transport_),
      second_factory_(&second_, &second_datagram_transport_) {
  first_.Connect(&second_);
  second_.Connect(&first_);
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

MediaTransportPair::LoopbackMediaTransport::LoopbackMediaTransport(
    rtc::Thread* thread)
    : dc_transport_(thread), thread_(thread), other_(nullptr) {
  RTC_LOG(LS_INFO) << "LoopbackMediaTransport";
}

MediaTransportPair::LoopbackMediaTransport::~LoopbackMediaTransport() {
  RTC_LOG(LS_INFO) << "~LoopbackMediaTransport";
  rtc::CritScope lock(&sink_lock_);
  RTC_CHECK(audio_sink_ == nullptr);
  RTC_CHECK(video_sink_ == nullptr);
  RTC_CHECK(target_transfer_rate_observers_.empty());
  RTC_CHECK(rtt_observers_.empty());
}

void MediaTransportPair::LoopbackMediaTransport::Connect(
    LoopbackMediaTransport* other) {
  other_ = other;
  dc_transport_.Connect(&other->dc_transport_);
}

void MediaTransportPair::LoopbackMediaTransport::Connect(
    rtc::PacketTransportInternal* packet_transport) {}

absl::optional<std::string>
MediaTransportPair::LoopbackMediaTransport::GetTransportParametersOffer()
    const {
  return "loopback-media-transport-parameters";
}

RTCError MediaTransportPair::LoopbackMediaTransport::SendAudioFrame(
    uint64_t channel_id,
    MediaTransportEncodedAudioFrame frame) {
  {
    rtc::CritScope lock(&stats_lock_);
    ++stats_.sent_audio_frames;
  }
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, channel_id, frame] {
    other_->OnData(channel_id, frame);
  });
  return RTCError::OK();
}

RTCError MediaTransportPair::LoopbackMediaTransport::SendVideoFrame(
    uint64_t channel_id,
    const MediaTransportEncodedVideoFrame& frame) {
  {
    rtc::CritScope lock(&stats_lock_);
    ++stats_.sent_video_frames;
  }
  // Ensure that we own the referenced data.
  MediaTransportEncodedVideoFrame frame_copy = frame;
  frame_copy.Retain();
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, thread_, [this, channel_id, frame_copy]() mutable {
        other_->OnData(channel_id, std::move(frame_copy));
      });
  return RTCError::OK();
}

void MediaTransportPair::LoopbackMediaTransport::SetKeyFrameRequestCallback(
    MediaTransportKeyFrameRequestCallback* callback) {
  rtc::CritScope lock(&sink_lock_);
  if (callback) {
    RTC_CHECK(key_frame_callback_ == nullptr);
  }
  key_frame_callback_ = callback;
}

RTCError MediaTransportPair::LoopbackMediaTransport::RequestKeyFrame(
    uint64_t channel_id) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, channel_id] {
    other_->OnKeyFrameRequested(channel_id);
  });
  return RTCError::OK();
}

void MediaTransportPair::LoopbackMediaTransport::SetReceiveAudioSink(
    MediaTransportAudioSinkInterface* sink) {
  rtc::CritScope lock(&sink_lock_);
  if (sink) {
    RTC_CHECK(audio_sink_ == nullptr);
  }
  audio_sink_ = sink;
}

void MediaTransportPair::LoopbackMediaTransport::SetReceiveVideoSink(
    MediaTransportVideoSinkInterface* sink) {
  rtc::CritScope lock(&sink_lock_);
  if (sink) {
    RTC_CHECK(video_sink_ == nullptr);
  }
  video_sink_ = sink;
}

void MediaTransportPair::LoopbackMediaTransport::AddTargetTransferRateObserver(
    TargetTransferRateObserver* observer) {
  RTC_CHECK(observer);
  {
    rtc::CritScope cs(&sink_lock_);
    RTC_CHECK(
        !absl::c_linear_search(target_transfer_rate_observers_, observer));
    target_transfer_rate_observers_.push_back(observer);
  }
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this] {
    RTC_DCHECK_RUN_ON(thread_);
    const DataRate kBitrate = DataRate::kbps(300);
    const Timestamp now = Timestamp::us(rtc::TimeMicros());

    TargetTransferRate transfer_rate;
    transfer_rate.at_time = now;
    transfer_rate.target_rate = kBitrate;
    transfer_rate.network_estimate.at_time = now;
    transfer_rate.network_estimate.round_trip_time = TimeDelta::ms(20);
    transfer_rate.network_estimate.bwe_period = TimeDelta::seconds(3);
    transfer_rate.network_estimate.bandwidth = kBitrate;

    rtc::CritScope cs(&sink_lock_);

    for (auto* o : target_transfer_rate_observers_) {
      o->OnTargetTransferRate(transfer_rate);
    }
  });
}

void MediaTransportPair::LoopbackMediaTransport::
    RemoveTargetTransferRateObserver(TargetTransferRateObserver* observer) {
  rtc::CritScope cs(&sink_lock_);
  auto it = absl::c_find(target_transfer_rate_observers_, observer);
  if (it == target_transfer_rate_observers_.end()) {
    RTC_LOG(LS_WARNING)
        << "Attempt to remove an unknown TargetTransferRate observer";
    return;
  }
  target_transfer_rate_observers_.erase(it);
}

void MediaTransportPair::LoopbackMediaTransport::AddRttObserver(
    MediaTransportRttObserver* observer) {
  RTC_CHECK(observer);
  {
    rtc::CritScope cs(&sink_lock_);
    RTC_CHECK(!absl::c_linear_search(rtt_observers_, observer));
    rtt_observers_.push_back(observer);
  }
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this] {
    RTC_DCHECK_RUN_ON(thread_);

    rtc::CritScope cs(&sink_lock_);
    for (auto* o : rtt_observers_) {
      o->OnRttUpdated(20);
    }
  });
}

void MediaTransportPair::LoopbackMediaTransport::RemoveRttObserver(
    MediaTransportRttObserver* observer) {
  rtc::CritScope cs(&sink_lock_);
  auto it = absl::c_find(rtt_observers_, observer);
  if (it == rtt_observers_.end()) {
    RTC_LOG(LS_WARNING) << "Attempt to remove an unknown RTT observer";
    return;
  }
  rtt_observers_.erase(it);
}

void MediaTransportPair::LoopbackMediaTransport::SetMediaTransportStateCallback(
    MediaTransportStateCallback* callback) {
  rtc::CritScope lock(&sink_lock_);
  state_callback_ = callback;
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this] {
    RTC_DCHECK_RUN_ON(thread_);
    OnStateChanged();
  });
}

RTCError MediaTransportPair::LoopbackMediaTransport::OpenChannel(
    int channel_id) {
  // No-op.  No need to open channels for the loopback.
  return dc_transport_.OpenChannel(channel_id);
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::OpenChannel(
    int channel_id) {
  // No-op.  No need to open channels for the loopback.
  return RTCError::OK();
}

RTCError MediaTransportPair::LoopbackMediaTransport::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  return dc_transport_.SendData(channel_id, params, buffer);
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

RTCError MediaTransportPair::LoopbackMediaTransport::CloseChannel(
    int channel_id) {
  return dc_transport_.CloseChannel(channel_id);
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

void MediaTransportPair::LoopbackMediaTransport::SetDataSink(
    DataChannelSink* sink) {
  dc_transport_.SetDataSink(sink);
}

bool MediaTransportPair::LoopbackMediaTransport::IsReadyToSend() const {
  return dc_transport_.IsReadyToSend();
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

void MediaTransportPair::LoopbackMediaTransport::SetState(
    MediaTransportState state) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, state] {
    RTC_DCHECK_RUN_ON(thread_);
    state_ = state;
    OnStateChanged();
  });
}

void MediaTransportPair::LoopbackMediaTransport::FlushAsyncInvokes() {
  invoker_.Flush(thread_);
  dc_transport_.FlushAsyncInvokes();
}

void MediaTransportPair::LoopbackDataChannelTransport::FlushAsyncInvokes() {
  invoker_.Flush(thread_);
}

MediaTransportPair::Stats
MediaTransportPair::LoopbackMediaTransport::GetStats() {
  rtc::CritScope lock(&stats_lock_);
  return stats_;
}

void MediaTransportPair::LoopbackMediaTransport::OnData(
    uint64_t channel_id,
    MediaTransportEncodedAudioFrame frame) {
  {
    rtc::CritScope lock(&sink_lock_);
    if (audio_sink_) {
      audio_sink_->OnData(channel_id, frame);
    }
  }
  {
    rtc::CritScope lock(&stats_lock_);
    ++stats_.received_audio_frames;
  }
}

void MediaTransportPair::LoopbackMediaTransport::OnData(
    uint64_t channel_id,
    MediaTransportEncodedVideoFrame frame) {
  {
    rtc::CritScope lock(&sink_lock_);
    if (video_sink_) {
      video_sink_->OnData(channel_id, frame);
    }
  }
  {
    rtc::CritScope lock(&stats_lock_);
    ++stats_.received_video_frames;
  }
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

void MediaTransportPair::LoopbackMediaTransport::OnKeyFrameRequested(
    int channel_id) {
  rtc::CritScope lock(&sink_lock_);
  if (key_frame_callback_) {
    key_frame_callback_->OnKeyFrameRequested(channel_id);
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

void MediaTransportPair::LoopbackMediaTransport::OnStateChanged() {
  rtc::CritScope lock(&sink_lock_);
  if (state_callback_) {
    state_callback_->OnStateChanged(state_);
  }

  dc_transport_.OnReadyToSend(state_ == MediaTransportState::kWritable);
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

void MediaTransportPair::LoopbackMediaTransport::SetAllocatedBitrateLimits(
    const MediaTransportAllocatedBitrateLimits& limits) {}

MediaTransportPair::LoopbackDatagramTransport::LoopbackDatagramTransport(
    rtc::Thread* thread)
    : dc_transport_(thread) {}

void MediaTransportPair::LoopbackDatagramTransport::Connect(
    LoopbackDatagramTransport* other) {
  dc_transport_.Connect(&other->dc_transport_);
}

void MediaTransportPair::LoopbackDatagramTransport::Connect(
    rtc::PacketTransportInternal* packet_transport) {}

CongestionControlInterface*
MediaTransportPair::LoopbackDatagramTransport::congestion_control() {
  return nullptr;
}

void MediaTransportPair::LoopbackDatagramTransport::SetTransportStateCallback(
    MediaTransportStateCallback* callback) {}

RTCError MediaTransportPair::LoopbackDatagramTransport::SendDatagram(
    rtc::ArrayView<const uint8_t> data,
    DatagramId datagram_id) {
  return RTCError::OK();
}

size_t MediaTransportPair::LoopbackDatagramTransport::GetLargestDatagramSize()
    const {
  return 0;
}

void MediaTransportPair::LoopbackDatagramTransport::SetDatagramSink(
    DatagramSinkInterface* sink) {}

std::string
MediaTransportPair::LoopbackDatagramTransport::GetTransportParameters() const {
  return transport_parameters_;
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
  dc_transport_.OnReadyToSend(state == MediaTransportState::kWritable);
}

void MediaTransportPair::LoopbackDatagramTransport::FlushAsyncInvokes() {
  dc_transport_.FlushAsyncInvokes();
}

}  // namespace webrtc
