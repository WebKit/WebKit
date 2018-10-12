/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_RTPRECEIVERINTERNAL_H_
#define PC_TEST_MOCK_RTPRECEIVERINTERNAL_H_

#include <string>
#include <vector>

#include "pc/rtpreceiver.h"
#include "test/gmock.h"

namespace webrtc {

// The definition of MockRtpReceiver is copied in to avoid multiple inheritance.
class MockRtpReceiverInternal : public RtpReceiverInternal {
 public:
  // RtpReceiverInterface methods.
  MOCK_METHOD1(SetTrack, void(MediaStreamTrackInterface*));
  MOCK_CONST_METHOD0(track, rtc::scoped_refptr<MediaStreamTrackInterface>());
  MOCK_CONST_METHOD0(stream_ids, std::vector<std::string>());
  MOCK_CONST_METHOD0(streams,
                     std::vector<rtc::scoped_refptr<MediaStreamInterface>>());
  MOCK_CONST_METHOD0(media_type, cricket::MediaType());
  MOCK_CONST_METHOD0(id, std::string());
  MOCK_CONST_METHOD0(GetParameters, RtpParameters());
  MOCK_METHOD1(SetParameters, bool(const RtpParameters&));
  MOCK_METHOD1(SetObserver, void(RtpReceiverObserverInterface*));
  MOCK_CONST_METHOD0(GetSources, std::vector<RtpSource>());
  MOCK_METHOD1(SetFrameDecryptor,
               void(rtc::scoped_refptr<FrameDecryptorInterface>));
  MOCK_CONST_METHOD0(GetFrameDecryptor,
                     rtc::scoped_refptr<FrameDecryptorInterface>());

  // RtpReceiverInternal methods.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVoiceMediaChannel, void(cricket::VoiceMediaChannel*));
  MOCK_METHOD1(SetVideoMediaChannel, void(cricket::VideoMediaChannel*));
  MOCK_METHOD1(SetupMediaChannel, void(uint32_t));
  MOCK_CONST_METHOD0(ssrc, uint32_t());
  MOCK_METHOD0(NotifyFirstPacketReceived, void());
  MOCK_METHOD1(set_stream_ids, void(std::vector<std::string>));
  MOCK_METHOD1(
      SetStreams,
      void(const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&));
  MOCK_CONST_METHOD0(AttachmentId, int());
};

}  // namespace webrtc

#endif  // PC_TEST_MOCK_RTPRECEIVERINTERNAL_H_
