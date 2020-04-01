/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_RTPRECEIVER_H_
#define API_TEST_MOCK_RTPRECEIVER_H_

#include <string>
#include <vector>

#include "api/rtp_receiver_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtpReceiver : public rtc::RefCountedObject<RtpReceiverInterface> {
 public:
  MOCK_METHOD1(SetTrack, void(MediaStreamTrackInterface*));
  MOCK_CONST_METHOD0(track, rtc::scoped_refptr<MediaStreamTrackInterface>());
  MOCK_CONST_METHOD0(streams,
                     std::vector<rtc::scoped_refptr<MediaStreamInterface>>());
  MOCK_CONST_METHOD0(media_type, cricket::MediaType());
  MOCK_CONST_METHOD0(id, std::string());
  MOCK_CONST_METHOD0(GetParameters, RtpParameters());
  MOCK_METHOD1(SetObserver, void(RtpReceiverObserverInterface*));
  MOCK_METHOD1(SetJitterBufferMinimumDelay, void(absl::optional<double>));
  MOCK_CONST_METHOD0(GetSources, std::vector<RtpSource>());
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_RTPRECEIVER_H_
