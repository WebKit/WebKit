/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_MOCK_DATAGRAM_CONNECTION_OBSERVER_H_
#define API_TEST_MOCK_DATAGRAM_CONNECTION_OBSERVER_H_

#include <cstdint>

#include "api/array_view.h"
#include "api/candidate.h"
#include "api/datagram_connection.h"
#include "test/gmock.h"

namespace webrtc {

class MockDatagramConnectionObserver : public DatagramConnection::Observer {
 public:
  MOCK_METHOD(void,
              OnCandidateGathered,
              (const Candidate& candidate),
              (override));
  MOCK_METHOD(void,
              OnPacketReceived,
              (ArrayView<const uint8_t> data, PacketMetadata metadata),
              (override));
  MOCK_METHOD(void, OnSendOutcome, (SendOutcome send_outcome), (override));
  MOCK_METHOD(void, OnConnectionError, (), (override));
  MOCK_METHOD(void, OnWritableChange, (), (override));
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_DATAGRAM_CONNECTION_OBSERVER_H_
