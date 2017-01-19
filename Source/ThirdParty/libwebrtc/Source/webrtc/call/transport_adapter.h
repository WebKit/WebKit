/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_TRANSPORT_ADAPTER_H_
#define WEBRTC_CALL_TRANSPORT_ADAPTER_H_

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/transport.h"

namespace webrtc {
namespace internal {

class TransportAdapter : public Transport {
 public:
  explicit TransportAdapter(Transport* transport);

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  void Enable();
  void Disable();

 private:
  Transport *transport_;
  Atomic32 enabled_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_CALL_TRANSPORT_ADAPTER_H_
