/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/null_transport.h"

#include <cstdint>

#include "api/array_view.h"
#include "api/call/transport.h"

namespace webrtc {
namespace test {

bool NullTransport::SendRtp(ArrayView<const uint8_t> packet,
                            const PacketOptions& options) {
  return true;
}

bool NullTransport::SendRtcp(ArrayView<const uint8_t> packet,
                             const PacketOptions& options) {
  return true;
}

}  // namespace test
}  // namespace webrtc
