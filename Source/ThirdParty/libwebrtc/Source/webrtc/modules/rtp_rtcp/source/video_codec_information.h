/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_VIDEO_CODEC_INFORMATION_H_
#define MODULES_RTP_RTCP_SOURCE_VIDEO_CODEC_INFORMATION_H_

#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
class VideoCodecInformation {
 public:
  virtual void Reset() = 0;

  virtual RtpVideoCodecTypes Type() = 0;
  virtual ~VideoCodecInformation() {}
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_VIDEO_CODEC_INFORMATION_H_
