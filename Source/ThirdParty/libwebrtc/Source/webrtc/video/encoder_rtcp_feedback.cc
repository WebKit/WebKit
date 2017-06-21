/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/encoder_rtcp_feedback.h"

#include "webrtc/base/checks.h"
#include "webrtc/video/vie_encoder.h"

static const int kMinKeyFrameRequestIntervalMs = 300;

namespace webrtc {

EncoderRtcpFeedback::EncoderRtcpFeedback(Clock* clock,
                                         const std::vector<uint32_t>& ssrcs,
                                         ViEEncoder* encoder)
    : clock_(clock),
      ssrcs_(ssrcs),
      vie_encoder_(encoder),
      time_last_intra_request_ms_(ssrcs.size(), -1) {
  RTC_DCHECK(!ssrcs.empty());
}

bool EncoderRtcpFeedback::HasSsrc(uint32_t ssrc) {
  for (uint32_t registered_ssrc : ssrcs_) {
    if (registered_ssrc == ssrc) {
      return true;
    }
  }
  return false;
}

size_t EncoderRtcpFeedback::GetStreamIndex(uint32_t ssrc) {
  for (size_t i = 0; i < ssrcs_.size(); ++i) {
    if (ssrcs_[i] == ssrc)
      return i;
  }
  RTC_NOTREACHED() << "Unknown ssrc " << ssrc;
  return 0;
}

void EncoderRtcpFeedback::OnReceivedIntraFrameRequest(uint32_t ssrc) {
  RTC_DCHECK(HasSsrc(ssrc));
  size_t index = GetStreamIndex(ssrc);
  {
    // TODO(mflodman): Move to ViEEncoder after some more changes making it
    // easier to test there.
    int64_t now_ms = clock_->TimeInMilliseconds();
    rtc::CritScope lock(&crit_);
    if (time_last_intra_request_ms_[index] + kMinKeyFrameRequestIntervalMs >
        now_ms) {
      return;
    }
    time_last_intra_request_ms_[index] = now_ms;
  }

  vie_encoder_->OnReceivedIntraFrameRequest(index);
}

}  // namespace webrtc
