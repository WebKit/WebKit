/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_PEERCONNECTION_H_
#define PC_TEST_MOCK_PEERCONNECTION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "pc/peerconnection.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"

namespace webrtc {

// The factory isn't really used; it just satisfies the base PeerConnection.
class FakePeerConnectionFactory
    : public rtc::RefCountedObject<webrtc::PeerConnectionFactory> {
 public:
  explicit FakePeerConnectionFactory(
      std::unique_ptr<cricket::MediaEngineInterface> media_engine)
      : rtc::RefCountedObject<webrtc::PeerConnectionFactory>(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            std::move(media_engine),
            std::unique_ptr<webrtc::CallFactoryInterface>(),
            std::unique_ptr<RtcEventLogFactoryInterface>()) {}
};

class MockPeerConnection
    : public rtc::RefCountedObject<webrtc::PeerConnection> {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  explicit MockPeerConnection(PeerConnectionFactory* factory)
      : rtc::RefCountedObject<webrtc::PeerConnection>(
            factory,
            std::unique_ptr<RtcEventLog>(),
            std::unique_ptr<Call>()) {}
  MOCK_METHOD0(local_streams,
               rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD0(remote_streams,
               rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_CONST_METHOD0(GetSenders,
                     std::vector<rtc::scoped_refptr<RtpSenderInterface>>());
  MOCK_CONST_METHOD0(GetReceivers,
                     std::vector<rtc::scoped_refptr<RtpReceiverInterface>>());
  MOCK_CONST_METHOD0(sctp_data_channels,
                     const std::vector<rtc::scoped_refptr<DataChannel>>&());
  MOCK_CONST_METHOD0(voice_channel, cricket::VoiceChannel*());
  MOCK_CONST_METHOD0(video_channel, cricket::VideoChannel*());
  // Libjingle uses "local" for a outgoing track, and "remote" for a incoming
  // track.
  MOCK_METHOD2(GetLocalTrackIdBySsrc, bool(uint32_t, std::string*));
  MOCK_METHOD2(GetRemoteTrackIdBySsrc, bool(uint32_t, std::string*));
  MOCK_METHOD0(GetCallStats, Call::Stats());
  MOCK_METHOD1(GetSessionStats,
               std::unique_ptr<SessionStats>(const ChannelNamePairs&));
  MOCK_METHOD2(GetLocalCertificate,
               bool(const std::string& transport_name,
                    rtc::scoped_refptr<rtc::RTCCertificate>* certificate));

  // Workaround for gmock's inability to cope with move-only return values.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name) /* override */ {
    return std::unique_ptr<rtc::SSLCertificate>(
        GetRemoteSSLCertificate_ReturnsRawPointer(transport_name));
  }
  MOCK_METHOD1(GetRemoteSSLCertificate_ReturnsRawPointer,
               rtc::SSLCertificate*(const std::string& transport_name));
};

}  // namespace webrtc

#endif  // PC_TEST_MOCK_PEERCONNECTION_H_
