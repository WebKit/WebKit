/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_FAKEICETRANSPORT_H_
#define WEBRTC_P2P_BASE_FAKEICETRANSPORT_H_

#include <string>

#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/p2p/base/icetransportinternal.h"

namespace cricket {

class FakeIceTransport : public IceTransportInternal {
 public:
  explicit FakeIceTransport(const std::string& name, int component)
      : name_(name), component_(component) {}
  ~FakeIceTransport() override {
    if (dest_ && dest_->dest_ == this) {
      dest_->dest_ = nullptr;
    }
  }

  // If async, will send packets by "Post"-ing to message queue instead of
  // synchronously "Send"-ing.
  void SetAsync(bool async) { async_ = async; }
  void SetAsyncDelay(int delay_ms) { async_delay_ms_ = delay_ms; }

  // SetWritable, SetReceiving and SetDestination are the main methods that can
  // be used for testing, to simulate connectivity or lack thereof.
  void SetWritable(bool writable) { set_writable(writable); }
  void SetReceiving(bool receiving) { set_receiving(receiving); }

  // Simulates the two transports connecting to each other.
  // If |asymmetric| is true this method only affects this FakeIceTransport.
  // If false, it affects |dest| as well.
  void SetDestination(FakeIceTransport* dest, bool asymmetric = false) {
    if (dest == dest_) {
      return;
    }
    RTC_DCHECK(!dest || !dest_)
        << "Changing fake destination from one to another is not supported.";
    if (dest) {
      // This simulates the delivery of candidates.
      dest_ = dest;
      set_writable(true);
      if (!asymmetric) {
        dest->SetDestination(this, true);
      }
    } else {
      // Simulates loss of connectivity, by asymmetrically forgetting dest_.
      dest_ = nullptr;
      set_writable(false);
    }
  }

  void SetConnectionCount(size_t connection_count) {
    size_t old_connection_count = connection_count_;
    connection_count_ = connection_count;
    if (connection_count) {
      had_connection_ = true;
    }
    // In this fake transport channel, |connection_count_| determines the
    // transport state.
    if (connection_count_ < old_connection_count) {
      SignalStateChanged(this);
    }
  }

  void SetCandidatesGatheringComplete() {
    if (gathering_state_ != kIceGatheringComplete) {
      gathering_state_ = kIceGatheringComplete;
      SignalGatheringState(this);
    }
  }

  // Convenience functions for accessing ICE config and other things.
  int receiving_timeout() const { return ice_config_.receiving_timeout; }
  bool gather_continually() const { return ice_config_.gather_continually(); }
  const Candidates& remote_candidates() const { return remote_candidates_; }

  // Fake IceTransportInternal implementation.
  const std::string& transport_name() const override { return name_; }
  int component() const override { return component_; }
  uint64_t IceTiebreaker() const { return tiebreaker_; }
  IceMode remote_ice_mode() const { return remote_ice_mode_; }
  const std::string& ice_ufrag() const { return ice_ufrag_; }
  const std::string& ice_pwd() const { return ice_pwd_; }
  const std::string& remote_ice_ufrag() const { return remote_ice_ufrag_; }
  const std::string& remote_ice_pwd() const { return remote_ice_pwd_; }

  IceTransportState GetState() const override {
    if (connection_count_ == 0) {
      return had_connection_ ? IceTransportState::STATE_FAILED
                             : IceTransportState::STATE_INIT;
    }

    if (connection_count_ == 1) {
      return IceTransportState::STATE_COMPLETED;
    }

    return IceTransportState::STATE_CONNECTING;
  }

  void SetIceRole(IceRole role) override { role_ = role; }
  IceRole GetIceRole() const override { return role_; }
  void SetIceTiebreaker(uint64_t tiebreaker) override {
    tiebreaker_ = tiebreaker;
  }
  void SetIceParameters(const IceParameters& ice_params) override {
    ice_ufrag_ = ice_params.ufrag;
    ice_pwd_ = ice_params.pwd;
  }
  void SetRemoteIceParameters(const IceParameters& params) override {
    remote_ice_ufrag_ = params.ufrag;
    remote_ice_pwd_ = params.pwd;
  }

  void SetRemoteIceMode(IceMode mode) override { remote_ice_mode_ = mode; }

  void MaybeStartGathering() override {
    if (gathering_state_ == kIceGatheringNew) {
      gathering_state_ = kIceGatheringGathering;
      SignalGatheringState(this);
    }
  }

  IceGatheringState gathering_state() const override {
    return gathering_state_;
  }

  void SetIceConfig(const IceConfig& config) override { ice_config_ = config; }

  void AddRemoteCandidate(const Candidate& candidate) override {
    remote_candidates_.push_back(candidate);
  }
  void RemoveRemoteCandidate(const Candidate& candidate) override {}

  bool GetStats(ConnectionInfos* infos) override {
    ConnectionInfo info;
    infos->clear();
    infos->push_back(info);
    return true;
  }

  rtc::Optional<int> GetRttEstimate() override {
    return rtc::Optional<int>();
  }

  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override {
  }

  // Fake PacketTransportInternal implementation.
  bool writable() const override { return writable_; }
  bool receiving() const override { return receiving_; }
  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override {
    if (!dest_) {
      return -1;
    }
    rtc::CopyOnWriteBuffer packet(data, len);
    if (async_) {
      invoker_.AsyncInvokeDelayed<void>(
          RTC_FROM_HERE, rtc::Thread::Current(),
          rtc::Bind(&FakeIceTransport::SendPacketInternal, this, packet),
          async_delay_ms_);
    } else {
      SendPacketInternal(packet);
    }
    rtc::SentPacket sent_packet(options.packet_id, rtc::TimeMillis());
    SignalSentPacket(this, sent_packet);
    return static_cast<int>(len);
  }
  int SetOption(rtc::Socket::Option opt, int value) override { return true; }
  bool GetOption(rtc::Socket::Option opt, int* value) override { return true; }
  int GetError() override { return 0; }

 private:
  void set_writable(bool writable) {
    if (writable_ == writable) {
      return;
    }
    LOG(INFO) << "set_writable from:" << writable_ << " to " << writable;
    writable_ = writable;
    if (writable_) {
      SignalReadyToSend(this);
    }
    SignalWritableState(this);
  }

  void set_receiving(bool receiving) {
    if (receiving_ == receiving) {
      return;
    }
    receiving_ = receiving;
    SignalReceivingState(this);
  }

  void SendPacketInternal(const rtc::CopyOnWriteBuffer& packet) {
    if (dest_) {
      dest_->SignalReadPacket(dest_, packet.data<char>(), packet.size(),
                              rtc::CreatePacketTime(0), 0);
    }
  }

  rtc::AsyncInvoker invoker_;
  std::string name_;
  int component_;
  FakeIceTransport* dest_ = nullptr;
  bool async_ = false;
  int async_delay_ms_ = 0;
  Candidates remote_candidates_;
  IceConfig ice_config_;
  IceRole role_ = ICEROLE_UNKNOWN;
  uint64_t tiebreaker_ = 0;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceMode remote_ice_mode_ = ICEMODE_FULL;
  size_t connection_count_ = 0;
  IceGatheringState gathering_state_ = kIceGatheringNew;
  bool had_connection_ = false;
  bool writable_ = false;
  bool receiving_ = false;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_FAKEICETRANSPORT_H_
