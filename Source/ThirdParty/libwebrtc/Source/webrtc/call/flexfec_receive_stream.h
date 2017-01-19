/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_H_
#define WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_H_

#include <memory>
#include <string>

#include "webrtc/api/call/flexfec_receive_stream.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"

namespace webrtc {

namespace internal {

class FlexfecReceiveStream : public webrtc::FlexfecReceiveStream {
 public:
  FlexfecReceiveStream(Config configuration,
                       RecoveredPacketReceiver* recovered_packet_callback);
  ~FlexfecReceiveStream();

  const Config& config() const { return config_; }

  bool AddAndProcessReceivedPacket(const uint8_t* packet, size_t length);

  // Implements webrtc::FlexfecReceiveStream.
  void Start() override;
  void Stop() override;
  Stats GetStats() const override;

 private:
  rtc::CriticalSection crit_;
  bool started_ GUARDED_BY(crit_);

  const Config config_;
  const std::unique_ptr<FlexfecReceiver> receiver_;
};

}  // namespace internal

}  // namespace webrtc

#endif  // WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_H_
