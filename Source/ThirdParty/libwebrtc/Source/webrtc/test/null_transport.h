/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_NULL_TRANSPORT_H_
#define WEBRTC_TEST_NULL_TRANSPORT_H_

#include "webrtc/api/call/transport.h"

namespace webrtc {

class PacketReceiver;

namespace test {
class NullTransport : public Transport {
 public:
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_NULL_TRANSPORT_H_
