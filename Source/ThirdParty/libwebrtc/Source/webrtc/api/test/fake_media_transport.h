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

#include "absl/memory/memory.h"
#include "api/media_transport_interface.h"

namespace webrtc {

// TODO(sukhanov): For now fake media transport does nothing and is used only
// in jsepcontroller unittests. In the future we should implement fake media
// transport, which forwards frames to another fake media transport, so we
// could unit test audio / video integration.
class FakeMediaTransport : public MediaTransportInterface {
 public:
  explicit FakeMediaTransport(const MediaTransportSettings& settings)
      : settings_(settings) {}
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
  };

  void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override {}
  void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override {}

  // Returns true if fake media transport was created as a caller.
  bool is_caller() const { return settings_.is_caller; }
  absl::optional<std::string> pre_shared_key() const {
    return settings_.pre_shared_key;
  }

  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override {
    return RTCError::OK();
  }

  RTCError CloseChannel(int channel_id) override { return RTCError::OK(); }

  void SetDataSink(DataChannelSink* sink) override {}

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
    RTC_CHECK(std::find(target_rate_observers_.begin(),
                        target_rate_observers_.end(),
                        observer) == target_rate_observers_.end());
    target_rate_observers_.push_back(observer);
  }

  void RemoveTargetTransferRateObserver(
      webrtc::TargetTransferRateObserver* observer) override {
    auto it = std::find(target_rate_observers_.begin(),
                        target_rate_observers_.end(), observer);
    if (it != target_rate_observers_.end()) {
      target_rate_observers_.erase(it);
    }
  }

  int target_rate_observers_size() { return target_rate_observers_.size(); }

 private:
  const MediaTransportSettings settings_;
  MediaTransportStateCallback* state_callback_;
  std::vector<webrtc::TargetTransferRateObserver*> target_rate_observers_;
};

// Fake media transport factory creates fake media transport.
class FakeMediaTransportFactory : public MediaTransportFactory {
 public:
  FakeMediaTransportFactory() = default;
  ~FakeMediaTransportFactory() = default;

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      bool is_caller) override {
    MediaTransportSettings settings;
    settings.is_caller = is_caller;
    return CreateMediaTransport(packet_transport, network_thread, settings);
  }

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override {
    std::unique_ptr<MediaTransportInterface> media_transport =
        absl::make_unique<FakeMediaTransport>(settings);
    return std::move(media_transport);
  }
};

}  // namespace webrtc

#endif  // API_TEST_FAKE_MEDIA_TRANSPORT_H_
