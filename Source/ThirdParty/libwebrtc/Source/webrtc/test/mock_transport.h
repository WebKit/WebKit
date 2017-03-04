/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_MOCK_TRANSPORT_H_
#define WEBRTC_TEST_MOCK_TRANSPORT_H_

#include "webrtc/api/call/transport.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockTransport : public Transport {
 public:
  MOCK_METHOD3(SendRtp,
               bool(const uint8_t* data,
                    size_t len,
                    const PacketOptions& options));
  MOCK_METHOD2(SendRtcp, bool(const uint8_t* data, size_t len));
};
}  // namespace webrtc
#endif  // WEBRTC_TEST_MOCK_TRANSPORT_H_
