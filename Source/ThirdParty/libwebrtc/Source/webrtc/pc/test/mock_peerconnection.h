/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_TEST_MOCK_PEERCONNECTION_H_
#define WEBRTC_PC_TEST_MOCK_PEERCONNECTION_H_

#include <vector>

#include "webrtc/pc/peerconnection.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

// The factory isn't really used; it just satisfies the base PeerConnection.
class FakePeerConnectionFactory
    : public rtc::RefCountedObject<webrtc::PeerConnectionFactory> {
 public:
  FakePeerConnectionFactory()
      : rtc::RefCountedObject<webrtc::PeerConnectionFactory>(nullptr, nullptr) {
  }
};

class MockPeerConnection
    : public rtc::RefCountedObject<webrtc::PeerConnection> {
 public:
  MockPeerConnection()
      : rtc::RefCountedObject<webrtc::PeerConnection>(
            new FakePeerConnectionFactory()) {}
  MOCK_METHOD0(local_streams,
               rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD0(remote_streams,
               rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD0(session, WebRtcSession*());
  MOCK_CONST_METHOD0(GetSenders,
                     std::vector<rtc::scoped_refptr<RtpSenderInterface>>());
  MOCK_CONST_METHOD0(GetReceivers,
                     std::vector<rtc::scoped_refptr<RtpReceiverInterface>>());
  MOCK_CONST_METHOD0(sctp_data_channels,
                     const std::vector<rtc::scoped_refptr<DataChannel>>&());
};

}  // namespace webrtc

#endif  // WEBRTC_PC_TEST_MOCK_PEERCONNECTION_H_
