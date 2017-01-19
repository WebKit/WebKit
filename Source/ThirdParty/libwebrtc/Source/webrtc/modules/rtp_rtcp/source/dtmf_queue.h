/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_DTMF_QUEUE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_DTMF_QUEUE_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class DTMFqueue {
 public:
  DTMFqueue();
  ~DTMFqueue();

  int32_t AddDTMF(uint8_t dtmf_key, uint16_t len, uint8_t level);
  int8_t NextDTMF(uint8_t* dtmf_key, uint16_t* len, uint8_t* level);
  bool PendingDTMF();

 private:
  rtc::CriticalSection dtmf_critsect_;
  uint8_t next_empty_index_;
  uint8_t dtmf_key_[DTMF_OUTBAND_MAX];
  uint16_t dtmf_length[DTMF_OUTBAND_MAX];
  uint8_t dtmf_level_[DTMF_OUTBAND_MAX];
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_DTMF_QUEUE_H_
