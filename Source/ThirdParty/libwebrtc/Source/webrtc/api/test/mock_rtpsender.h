/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_RTPSENDER_H_
#define API_TEST_MOCK_RTPSENDER_H_

#include <string>
#include <vector>

#include "api/rtp_sender_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtpSender : public rtc::RefCountedObject<RtpSenderInterface> {
 public:
  MOCK_METHOD1(SetTrack, bool(MediaStreamTrackInterface*));
  MOCK_CONST_METHOD0(track, rtc::scoped_refptr<MediaStreamTrackInterface>());
  MOCK_CONST_METHOD0(ssrc, uint32_t());
  MOCK_CONST_METHOD0(media_type, cricket::MediaType());
  MOCK_CONST_METHOD0(id, std::string());
  MOCK_CONST_METHOD0(stream_ids, std::vector<std::string>());
  MOCK_CONST_METHOD0(init_send_encodings, std::vector<RtpEncodingParameters>());
  MOCK_CONST_METHOD0(GetParameters, RtpParameters());
  MOCK_METHOD1(SetParameters, RTCError(const RtpParameters&));
  MOCK_CONST_METHOD0(GetDtmfSender, rtc::scoped_refptr<DtmfSenderInterface>());
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_RTPSENDER_H_
