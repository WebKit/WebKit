/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_AFTER_INIT_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_AFTER_INIT_H_

#include <deque>
#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/before_initialization_fixture.h"

class TestErrorObserver;

class LoopBackTransport : public webrtc::Transport {
 public:
  LoopBackTransport(webrtc::VoENetwork* voe_network, int channel)
      : packet_event_(webrtc::EventWrapper::Create()),
        thread_(NetworkProcess, this, "LoopBackTransport"),
        channel_(channel),
        voe_network_(voe_network),
        transmitted_packets_(0) {
    thread_.Start();
  }

  ~LoopBackTransport() { thread_.Stop(); }

  bool SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options) override {
    StorePacket(Packet::Rtp, data, len);
    return true;
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    StorePacket(Packet::Rtcp, data, len);
    return true;
  }

  void WaitForTransmittedPackets(int32_t packet_count) {
    enum {
      kSleepIntervalMs = 10
    };
    int32_t limit = transmitted_packets_.Value() + packet_count;
    while (transmitted_packets_.Value() < limit) {
      webrtc::SleepMs(kSleepIntervalMs);
    }
  }

  void AddChannel(uint32_t ssrc, int channel) {
    rtc::CritScope lock(&crit_);
    channels_[ssrc] = channel;
  }

 private:
  struct Packet {
    enum Type { Rtp, Rtcp, } type;

    Packet() : len(0) {}
    Packet(Type type, const void* data, size_t len)
        : type(type), len(len) {
      assert(len <= 1500);
      memcpy(this->data, data, len);
    }

    uint8_t data[1500];
    size_t len;
  };

  void StorePacket(Packet::Type type,
                   const void* data,
                   size_t len) {
    {
      rtc::CritScope lock(&crit_);
      packet_queue_.push_back(Packet(type, data, len));
    }
    packet_event_->Set();
  }

  static bool NetworkProcess(void* transport) {
    return static_cast<LoopBackTransport*>(transport)->SendPackets();
  }

  bool SendPackets() {
    switch (packet_event_->Wait(10)) {
      case webrtc::kEventSignaled:
        break;
      case webrtc::kEventTimeout:
        break;
      case webrtc::kEventError:
        // TODO(pbos): Log a warning here?
        return true;
    }

    while (true) {
      Packet p;
      int channel = channel_;
      {
        rtc::CritScope lock(&crit_);
        if (packet_queue_.empty())
          break;
        p = packet_queue_.front();
        packet_queue_.pop_front();

        if (p.type == Packet::Rtp) {
          uint32_t ssrc =
              webrtc::ByteReader<uint32_t>::ReadBigEndian(&p.data[8]);
          if (channels_[ssrc] != 0)
            channel = channels_[ssrc];
        }
        // TODO(pbos): Add RTCP SSRC muxing/demuxing if anything requires it.
      }

      // Minimum RTP header size.
      if (p.len < 12)
        continue;

      switch (p.type) {
        case Packet::Rtp:
          voe_network_->ReceivedRTPPacket(channel, p.data, p.len,
                                          webrtc::PacketTime());
          break;
        case Packet::Rtcp:
          voe_network_->ReceivedRTCPPacket(channel, p.data, p.len);
          break;
      }
      ++transmitted_packets_;
    }
    return true;
  }

  rtc::CriticalSection crit_;
  const std::unique_ptr<webrtc::EventWrapper> packet_event_;
  rtc::PlatformThread thread_;
  std::deque<Packet> packet_queue_ GUARDED_BY(crit_);
  const int channel_;
  std::map<uint32_t, int> channels_ GUARDED_BY(crit_);
  webrtc::VoENetwork* const voe_network_;
  webrtc::Atomic32 transmitted_packets_;
};

// This fixture initializes the voice engine in addition to the work
// done by the before-initialization fixture. It also registers an error
// observer which will fail tests on error callbacks. This fixture is
// useful to tests that want to run before we have started any form of
// streaming through the voice engine.
class AfterInitializationFixture : public BeforeInitializationFixture {
 public:
  AfterInitializationFixture();
  virtual ~AfterInitializationFixture();

 protected:
  std::unique_ptr<TestErrorObserver> error_observer_;
};

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_AFTER_INIT_H_
