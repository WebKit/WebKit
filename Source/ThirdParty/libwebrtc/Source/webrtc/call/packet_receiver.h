/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_PACKET_RECEIVER_H_
#define CALL_PACKET_RECEIVER_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "api/mediatypes.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/copyonwritebuffer.h"

namespace webrtc {

class PacketReceiver {
 public:
  enum DeliveryStatus {
    DELIVERY_OK,
    DELIVERY_UNKNOWN_SSRC,
    DELIVERY_PACKET_ERROR,
  };

  virtual DeliveryStatus DeliverPacket(MediaType media_type,
                                       rtc::CopyOnWriteBuffer packet,
                                       int64_t packet_time_us);

  // TODO(bugs.webrtc.org/9584): Deprecated. Over the transition, default
  // implementations are used, and subclasses must override one or the other.
  virtual DeliveryStatus DeliverPacket(MediaType media_type,
                                       rtc::CopyOnWriteBuffer packet,
                                       const PacketTime& packet_time);

 protected:
  virtual ~PacketReceiver() {}
};

}  // namespace webrtc

#endif  // CALL_PACKET_RECEIVER_H_
