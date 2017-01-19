/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/ssrc_database.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/checks.h"

namespace webrtc {

SSRCDatabase* SSRCDatabase::GetSSRCDatabase() {
  return GetStaticInstance<SSRCDatabase>(kAddRef);
}

void SSRCDatabase::ReturnSSRCDatabase() {
  GetStaticInstance<SSRCDatabase>(kRelease);
}

uint32_t SSRCDatabase::CreateSSRC() {
  rtc::CritScope lock(&crit_);

  while (true) {  // Try until get a new ssrc.
    // 0 and 0xffffffff are invalid values for SSRC.
    uint32_t ssrc = random_.Rand(1u, 0xfffffffe);
    if (ssrcs_.insert(ssrc).second) {
      return ssrc;
    }
  }
}

void SSRCDatabase::RegisterSSRC(uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  ssrcs_.insert(ssrc);
}

void SSRCDatabase::ReturnSSRC(uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  ssrcs_.erase(ssrc);
}

SSRCDatabase::SSRCDatabase() : random_(rtc::TimeMicros()) {}

SSRCDatabase::~SSRCDatabase() {}

}  // namespace webrtc
