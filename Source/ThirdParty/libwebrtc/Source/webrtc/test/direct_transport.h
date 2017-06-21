/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_DIRECT_TRANSPORT_H_
#define WEBRTC_TEST_DIRECT_TRANSPORT_H_

#include <assert.h>

#include <deque>

#include "webrtc/api/call/transport.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/call/call.h"
#include "webrtc/test/fake_network_pipe.h"

namespace webrtc {

class Clock;
class PacketReceiver;

namespace test {

class DirectTransport : public Transport {
 public:
  DirectTransport(Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);
  DirectTransport(const FakeNetworkPipe::Config& config,
                  Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);
  DirectTransport(const FakeNetworkPipe::Config& config,
                  Call* send_call,
                  std::unique_ptr<Demuxer> demuxer);

  // These deprecated variants always use ForceDemuxer.
  RTC_DEPRECATED DirectTransport(Call* send_call, MediaType media_type)
      : DirectTransport(
            FakeNetworkPipe::Config(),
            send_call,
            std::unique_ptr<Demuxer>(new ForceDemuxer(media_type))) {}
  RTC_DEPRECATED DirectTransport(const FakeNetworkPipe::Config& config,
                                 Call* send_call,
                                 MediaType media_type)
      : DirectTransport(
            config,
            send_call,
            std::unique_ptr<Demuxer>(new ForceDemuxer(media_type))) {}

  // These deprecated variants always use MediaType::VIDEO.
  RTC_DEPRECATED explicit DirectTransport(Call* send_call)
      : DirectTransport(
            FakeNetworkPipe::Config(),
            send_call,
            std::unique_ptr<Demuxer>(new ForceDemuxer(MediaType::VIDEO))) {}

  RTC_DEPRECATED DirectTransport(const FakeNetworkPipe::Config& config,
                                 Call* send_call)
      : DirectTransport(
            config,
            send_call,
            std::unique_ptr<Demuxer>(new ForceDemuxer(MediaType::VIDEO))) {}

  ~DirectTransport();

  void SetConfig(const FakeNetworkPipe::Config& config);

  virtual void StopSending();
  // TODO(holmer): Look into moving this to the constructor.
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(const uint8_t* data,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* data, size_t length) override;

  int GetAverageDelayMs();

 private:
  // TODO(minyue): remove when the deprecated ctors of DirectTransport that
  // create ForceDemuxer are removed.
  class ForceDemuxer : public Demuxer {
   public:
    explicit ForceDemuxer(MediaType media_type);
    void SetReceiver(PacketReceiver* receiver) override;
    void DeliverPacket(const NetworkPacket* packet,
                       const PacketTime& packet_time) override;

   private:
    const MediaType media_type_;
    PacketReceiver* packet_receiver_;
    RTC_DISALLOW_COPY_AND_ASSIGN(ForceDemuxer);
  };

  static bool NetworkProcess(void* transport);
  bool SendPackets();

  rtc::CriticalSection lock_;
  Call* const send_call_;
  rtc::Event packet_event_;
  rtc::PlatformThread thread_;
  Clock* const clock_;

  bool shutting_down_;

  FakeNetworkPipe fake_network_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_DIRECT_TRANSPORT_H_
