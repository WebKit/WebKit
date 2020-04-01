/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
#define API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/transport/datagram_transport_interface.h"
#include "api/transport/media/media_transport_interface.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// Wrapper used to hand out unique_ptrs to loopback media
// transport without ownership changes to the underlying
// transport.
// It works in two modes:
// It can either wrap a factory, or it can wrap an existing interface.
// In the former mode, it delegates the work to the wrapped factory.
// In the latter mode, it always returns static instance of the transport
// interface.
//
// Example use:
// Factory wrap_static_interface = Wrapper(media_transport_interface);
// Factory wrap_factory = Wrapper(wrap_static_interface);
// The second factory may be created multiple times, and ownership may be passed
// to the client. The first factory counts the number of invocations of
// CreateMediaTransport();
class WrapperMediaTransportFactory : public MediaTransportFactory {
 public:
  explicit WrapperMediaTransportFactory(
      DatagramTransportInterface* wrapped_datagram_transport);
  explicit WrapperMediaTransportFactory(MediaTransportFactory* wrapped);

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override;

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override;

  RTCErrorOr<std::unique_ptr<DatagramTransportInterface>>
  CreateDatagramTransport(rtc::Thread* network_thread,
                          const MediaTransportSettings& settings) override;

  std::string GetTransportName() const override;

  int created_transport_count() const;

 private:
  DatagramTransportInterface* wrapped_datagram_transport_ = nullptr;
  MediaTransportFactory* wrapped_factory_ = nullptr;
  int created_transport_count_ = 0;
};

// Contains two MediaTransportsInterfaces that are connected to each other.
// Currently supports audio only.
class MediaTransportPair {
 public:
  struct Stats {
    int sent_audio_frames = 0;
    int received_audio_frames = 0;
    int sent_video_frames = 0;
    int received_video_frames = 0;
  };

  explicit MediaTransportPair(rtc::Thread* thread);
  ~MediaTransportPair();

  DatagramTransportInterface* first_datagram_transport() {
    return &first_datagram_transport_;
  }
  DatagramTransportInterface* second_datagram_transport() {
    return &second_datagram_transport_;
  }

  std::unique_ptr<MediaTransportFactory> first_factory() {
    return std::make_unique<WrapperMediaTransportFactory>(&first_factory_);
  }

  std::unique_ptr<MediaTransportFactory> second_factory() {
    return std::make_unique<WrapperMediaTransportFactory>(&second_factory_);
  }

  void SetState(MediaTransportState state) {
    first_datagram_transport_.SetState(state);
    second_datagram_transport_.SetState(state);
  }

  void SetFirstState(MediaTransportState state) {
    first_datagram_transport_.SetState(state);
  }

  void SetSecondStateAfterConnect(MediaTransportState state) {
    second_datagram_transport_.SetState(state);
  }

  void SetFirstDatagramTransportParameters(const std::string& params) {
    first_datagram_transport_.set_transport_parameters(params);
  }

  void SetSecondDatagramTransportParameters(const std::string& params) {
    second_datagram_transport_.set_transport_parameters(params);
  }

  void SetFirstDatagramTransportParametersComparison(
      std::function<bool(absl::string_view, absl::string_view)> comparison) {
    first_datagram_transport_.set_transport_parameters_comparison(
        std::move(comparison));
  }

  void SetSecondDatagramTransportParametersComparison(
      std::function<bool(absl::string_view, absl::string_view)> comparison) {
    second_datagram_transport_.set_transport_parameters_comparison(
        std::move(comparison));
  }

  void FlushAsyncInvokes() {
    first_datagram_transport_.FlushAsyncInvokes();
    second_datagram_transport_.FlushAsyncInvokes();
  }

  int first_factory_transport_count() const {
    return first_factory_.created_transport_count();
  }

  int second_factory_transport_count() const {
    return second_factory_.created_transport_count();
  }

 private:
  class LoopbackDataChannelTransport : public DataChannelTransportInterface {
   public:
    explicit LoopbackDataChannelTransport(rtc::Thread* thread);
    ~LoopbackDataChannelTransport() override;

    void Connect(LoopbackDataChannelTransport* other);

    RTCError OpenChannel(int channel_id) override;

    RTCError SendData(int channel_id,
                      const SendDataParams& params,
                      const rtc::CopyOnWriteBuffer& buffer) override;

    RTCError CloseChannel(int channel_id) override;

    bool IsReadyToSend() const override;

    void SetDataSink(DataChannelSink* sink) override;

    void OnReadyToSend(bool ready_to_send);

    void FlushAsyncInvokes();

   private:
    void OnData(int channel_id,
                DataMessageType type,
                const rtc::CopyOnWriteBuffer& buffer);

    void OnRemoteCloseChannel(int channel_id);

    rtc::Thread* const thread_;
    rtc::CriticalSection sink_lock_;
    DataChannelSink* data_sink_ RTC_GUARDED_BY(sink_lock_) = nullptr;

    bool ready_to_send_ RTC_GUARDED_BY(sink_lock_) = false;

    LoopbackDataChannelTransport* other_;

    rtc::AsyncInvoker invoker_;
  };

  class LoopbackDatagramTransport : public DatagramTransportInterface {
   public:
    explicit LoopbackDatagramTransport(rtc::Thread* thread);

    void Connect(LoopbackDatagramTransport* other);

    // Datagram transport overrides.
    void Connect(rtc::PacketTransportInternal* packet_transport) override;
    CongestionControlInterface* congestion_control() override;
    void SetTransportStateCallback(
        MediaTransportStateCallback* callback) override;
    RTCError SendDatagram(rtc::ArrayView<const uint8_t> data,
                          DatagramId datagram_id) override;
    size_t GetLargestDatagramSize() const override;
    void SetDatagramSink(DatagramSinkInterface* sink) override;
    std::string GetTransportParameters() const override;
    RTCError SetRemoteTransportParameters(
        absl::string_view remote_parameters) override;

    // Data channel overrides.
    RTCError OpenChannel(int channel_id) override;
    RTCError SendData(int channel_id,
                      const SendDataParams& params,
                      const rtc::CopyOnWriteBuffer& buffer) override;
    RTCError CloseChannel(int channel_id) override;
    void SetDataSink(DataChannelSink* sink) override;
    bool IsReadyToSend() const override;

    // Loopback-specific functionality.
    void SetState(MediaTransportState state);

    // When Connect() is called, the datagram transport will enter this state.
    // This is useful for mimicking zero-RTT connectivity, for example.
    void SetStateAfterConnect(MediaTransportState state);
    void FlushAsyncInvokes();

    void set_transport_parameters(const std::string& value) {
      transport_parameters_ = value;
    }

    void set_transport_parameters_comparison(
        std::function<bool(absl::string_view, absl::string_view)> comparison) {
      thread_->Invoke<void>(
          RTC_FROM_HERE, [this, comparison = std::move(comparison)] {
            RTC_DCHECK_RUN_ON(thread_);
            transport_parameters_comparison_ = std::move(comparison);
          });
    }

   private:
    void DeliverDatagram(rtc::CopyOnWriteBuffer buffer);

    rtc::Thread* thread_;
    LoopbackDataChannelTransport dc_transport_;

    MediaTransportState state_ RTC_GUARDED_BY(thread_) =
        MediaTransportState::kPending;
    DatagramSinkInterface* sink_ RTC_GUARDED_BY(thread_) = nullptr;
    MediaTransportStateCallback* state_callback_ RTC_GUARDED_BY(thread_) =
        nullptr;
    LoopbackDatagramTransport* other_;

    std::string transport_parameters_;
    std::function<bool(absl::string_view, absl::string_view)>
        transport_parameters_comparison_ RTC_GUARDED_BY(thread_) =
            [](absl::string_view a, absl::string_view b) { return a == b; };

    absl::optional<MediaTransportState> state_after_connect_;

    rtc::AsyncInvoker invoker_;
  };

  LoopbackDatagramTransport first_datagram_transport_;
  LoopbackDatagramTransport second_datagram_transport_;
  WrapperMediaTransportFactory first_factory_;
  WrapperMediaTransportFactory second_factory_;
};

}  // namespace webrtc

#endif  // API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
