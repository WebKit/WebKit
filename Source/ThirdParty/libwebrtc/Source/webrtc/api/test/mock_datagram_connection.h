/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_MOCK_DATAGRAM_CONNECTION_H_
#define API_TEST_MOCK_DATAGRAM_CONNECTION_H_

#include <cstddef>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/candidate.h"
#include "api/datagram_connection.h"
#include "p2p/base/transport_description.h"
#include "test/gmock.h"

namespace webrtc {

class MockDatagramConnection : public DatagramConnection {
 public:
  MOCK_METHOD(void,
              SetRemoteIceParameters,
              (const IceParameters& ice_parameters),
              (override));
  MOCK_METHOD(void,
              AddRemoteCandidate,
              (const Candidate& candidate),
              (override));
  MOCK_METHOD(bool, Writable, (), (override));
  MOCK_METHOD(void,
              SetRemoteDtlsParameters,
              (absl::string_view digestAlgorithm,
               const uint8_t* digest,
               size_t digest_len,
               DatagramConnection::SSLRole ssl_role),
              (override));
  MOCK_METHOD(bool, SendPacket, (ArrayView<const uint8_t> data), (override));
  MOCK_METHOD(void,
              SendPacket,
              (ArrayView<const uint8_t> data, PacketSendParameters params),
              (override));
  MOCK_METHOD(void,
              Terminate,
              (absl::AnyInvocable<void()> terminate_complete_callback),
              (override));
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_DATAGRAM_CONNECTION_H_
