/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENCODER_RTCP_FEEDBACK_H_
#define WEBRTC_VIDEO_ENCODER_RTCP_FEEDBACK_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ViEEncoder;

class EncoderRtcpFeedback : public RtcpIntraFrameObserver {
 public:
  EncoderRtcpFeedback(Clock* clock,
                       const std::vector<uint32_t>& ssrcs,
                       ViEEncoder* encoder);
  void OnReceivedIntraFrameRequest(uint32_t ssrc) override;

 private:
  bool HasSsrc(uint32_t ssrc);
  size_t GetStreamIndex(uint32_t ssrc);

  Clock* const clock_;
  const std::vector<uint32_t> ssrcs_;
  ViEEncoder* const vie_encoder_;

  rtc::CriticalSection crit_;
  std::vector<int64_t> time_last_intra_request_ms_ GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENCODER_RTCP_FEEDBACK_H_
