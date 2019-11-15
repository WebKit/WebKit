/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_FAKE_MEDIA_TRANSPORT_H_
#define API_TEST_FAKE_MEDIA_TRANSPORT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "api/media_transport_interface.h"
#include "api/test/fake_datagram_transport.h"

namespace webrtc {

// TODO(sukhanov): For now fake media transport does nothing and is used only
// in jsepcontroller unittests. In the future we should implement fake media
// transport, which forwards frames to another fake media transport, so we
// could unit test audio / video integration.
class FakeMediaTransport : public MediaTransportInterface {
 public:
  explicit FakeMediaTransport(
      const MediaTransportSettings& settings,
      const absl::optional<std::string>& transport_offer = "",
      const absl::optional<std::string>& remote_transport_parameters = "")
      : settings_(settings),
        transport_offer_(transport_offer),
        remote_transport_parameters_(remote_transport_parameters) {}
  ~FakeMediaTransport() = default;

  RTCError SendAudioFrame(uint64_t channel_id,
                          MediaTransportEncodedAudioFrame frame) override {
    return RTCError::OK();
  }

  RTCError SendVideoFrame(
      uint64_t channel_id,
      const MediaTransportEncodedVideoFrame& frame) override {
    return RTCError::OK();
  }

  RTCError RequestKeyFrame(uint64_t channel_id) override {
    return RTCError::OK();
  }

  void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override {}
  void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override {}

  // Returns true if fake media transport was created as a caller.
  bool is_caller() const { return settings_.is_caller; }
  absl::optional<std::string> pre_shared_key() const {
    return settings_.pre_shared_key;
  }

  RTCError OpenChannel(int channel_id) override { return RTCError::OK(); }

  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override {
    return RTCError::OK();
  }

  RTCError CloseChannel(int channel_id) override { return RTCError::OK(); }

  void SetDataSink(DataChannelSink* sink) override {}

  bool IsReadyToSend() const override { return false; }

  void SetMediaTransportStateCallback(
      MediaTransportStateCallback* callback) override {
    state_callback_ = callback;
  }

  void SetState(webrtc::MediaTransportState state) {
    if (state_callback_) {
      state_callback_->OnStateChanged(state);
    }
  }

  void AddTargetTransferRateObserver(
      webrtc::TargetTransferRateObserver* observer) override {
    RTC_CHECK(!absl::c_linear_search(target_rate_observers_, observer));
    target_rate_observers_.push_back(observer);
  }

  void RemoveTargetTransferRateObserver(
      webrtc::TargetTransferRateObserver* observer) override {
    auto it = absl::c_find(target_rate_observers_, observer);
    if (it != target_rate_observers_.end()) {
      target_rate_observers_.erase(it);
    }
  }

  void SetAllocatedBitrateLimits(
      const MediaTransportAllocatedBitrateLimits& limits) override {}

  void SetTargetBitrateLimits(const MediaTransportTargetRateConstraints&
                                  target_rate_constraints) override {
    target_rate_constraints_in_order_.push_back(target_rate_constraints);
  }

  const std::vector<MediaTransportTargetRateConstraints>&
  target_rate_constraints_in_order() {
    return target_rate_constraints_in_order_;
  }

  int target_rate_observers_size() { return target_rate_observers_.size(); }

  // Settings that were passed down to fake media transport.
  const MediaTransportSettings& settings() { return settings_; }

  absl::optional<std::string> GetTransportParametersOffer() const override {
    // At least right now, we intend to use GetTransportParametersOffer before
    // the transport is connected. This may change in the future.
    RTC_CHECK(!is_connected_);
    return transport_offer_;
  }

  const absl::optional<std::string>& remote_transport_parameters() {
    return remote_transport_parameters_;
  }

  void Connect(rtc::PacketTransportInternal* packet_transport) {
    RTC_CHECK(!is_connected_) << "::Connect was called twice";
    is_connected_ = true;
  }

  bool is_connected() { return is_connected_; }

 private:
  const MediaTransportSettings settings_;
  MediaTransportStateCallback* state_callback_ = nullptr;
  std::vector<webrtc::TargetTransferRateObserver*> target_rate_observers_;
  const absl::optional<std::string> transport_offer_;
  const absl::optional<std::string> remote_transport_parameters_;
  bool is_connected_ = false;
  std::vector<MediaTransportTargetRateConstraints>
      target_rate_constraints_in_order_;
};

// Fake media transport factory creates fake media transport.
// Also creates fake datagram transport, since both media and datagram
// transports are created by |MediaTransportFactory|.
class FakeMediaTransportFactory : public MediaTransportFactory {
 public:
  explicit FakeMediaTransportFactory(
      const absl::optional<std::string>& transport_offer = "")
      : transport_offer_(transport_offer) {}
  ~FakeMediaTransportFactory() = default;

  std::string GetTransportName() const override { return "fake"; }

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override {
    std::unique_ptr<MediaTransportInterface> media_transport =
        absl::make_unique<FakeMediaTransport>(settings, transport_offer_);
    media_transport->Connect(packet_transport);
    return std::move(media_transport);
  }

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override {
    std::unique_ptr<MediaTransportInterface> media_transport =
        absl::make_unique<FakeMediaTransport>(
            settings, transport_offer_, settings.remote_transport_parameters);
    return std::move(media_transport);
  }

  RTCErrorOr<std::unique_ptr<DatagramTransportInterface>>
  CreateDatagramTransport(rtc::Thread* network_thread,
                          const MediaTransportSettings& settings) override {
    return std::unique_ptr<DatagramTransportInterface>(
        new FakeDatagramTransport(settings, transport_offer_.value_or("")));
  }

 private:
  const absl::optional<std::string> transport_offer_;
};

}  // namespace webrtc

#endif  // API_TEST_FAKE_MEDIA_TRANSPORT_H_
