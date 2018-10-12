/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_RTPSENDERINTERNAL_H_
#define PC_TEST_MOCK_RTPSENDERINTERNAL_H_

#include <string>
#include <vector>

#include "pc/rtpsender.h"
#include "test/gmock.h"

namespace webrtc {

// The definition of MockRtpSender is copied in to avoid multiple inheritance.
class MockRtpSenderInternal : public RtpSenderInternal {
 public:
  // RtpSenderInterface methods.
  MOCK_METHOD1(SetTrack, bool(MediaStreamTrackInterface*));
  MOCK_CONST_METHOD0(track, rtc::scoped_refptr<MediaStreamTrackInterface>());
  MOCK_CONST_METHOD0(ssrc, uint32_t());
  MOCK_CONST_METHOD0(media_type, cricket::MediaType());
  MOCK_CONST_METHOD0(id, std::string());
  MOCK_CONST_METHOD0(stream_ids, std::vector<std::string>());
  MOCK_CONST_METHOD0(init_send_encodings, std::vector<RtpEncodingParameters>());
  MOCK_METHOD0(GetParameters, RtpParameters());
  MOCK_METHOD1(SetParameters, RTCError(const RtpParameters&));
  MOCK_CONST_METHOD0(GetDtmfSender, rtc::scoped_refptr<DtmfSenderInterface>());
  MOCK_METHOD1(SetFrameEncryptor,
               void(rtc::scoped_refptr<FrameEncryptorInterface>));
  MOCK_CONST_METHOD0(GetFrameEncryptor,
                     rtc::scoped_refptr<FrameEncryptorInterface>());

  // RtpSenderInternal methods.
  MOCK_METHOD1(SetVoiceMediaChannel, void(cricket::VoiceMediaChannel*));
  MOCK_METHOD1(SetVideoMediaChannel, void(cricket::VideoMediaChannel*));
  MOCK_METHOD1(SetSsrc, void(uint32_t));
  MOCK_METHOD1(set_stream_ids, void(const std::vector<std::string>&));
  MOCK_METHOD1(set_init_send_encodings,
               void(const std::vector<RtpEncodingParameters>&));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(AttachmentId, int());
};

}  // namespace webrtc

#endif  // PC_TEST_MOCK_RTPSENDERINTERNAL_H_
