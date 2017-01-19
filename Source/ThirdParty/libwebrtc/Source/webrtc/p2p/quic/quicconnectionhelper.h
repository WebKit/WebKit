/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_QUIC_QUICCONNECTIONHELPER_H_
#define WEBRTC_P2P_QUIC_QUICCONNECTIONHELPER_H_

#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_alarm.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_simple_buffer_allocator.h"
#include "webrtc/base/thread.h"

namespace cricket {

// An alarm which will go off at a scheduled time, and execute the |OnAlarm|
// method of the delegate.
class QuicAlarm : public net::QuicAlarm, public rtc::MessageHandler {
 public:
  QuicAlarm(const net::QuicClock* clock,
            rtc::Thread* thread,
            net::QuicArenaScopedPtr<net::QuicAlarm::Delegate> delegate);

  ~QuicAlarm() override;

  // rtc::MessageHandler override.
  void OnMessage(rtc::Message* msg) override;

  // Helper method to get the delay in ms for posting task.
  int64_t GetDelay() const;

 protected:
  // net::QuicAlarm overrides.
  void SetImpl() override;
  void CancelImpl() override;

 private:
  const net::QuicClock* clock_;
  rtc::Thread* thread_;
};

// Helper methods for QuicConnection timing and random number generation.
class QuicConnectionHelper : public net::QuicConnectionHelperInterface {
 public:
  explicit QuicConnectionHelper(rtc::Thread* thread);
  ~QuicConnectionHelper() override;

  // QuicConnectionHelperInterface overrides.
  const net::QuicClock* GetClock() const override;
  net::QuicRandom* GetRandomGenerator() override;
  QuicAlarm* CreateAlarm(net::QuicAlarm::Delegate* delegate) override;
  net::QuicArenaScopedPtr<net::QuicAlarm> CreateAlarm(
      net::QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      net::QuicConnectionArena* arena) override;
  net::QuicBufferAllocator* GetBufferAllocator() override;

 private:
  net::QuicClock clock_;
  net::SimpleBufferAllocator buffer_allocator_;
  rtc::Thread* thread_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_QUIC_QUICCONNECTIONHELPER_H_
