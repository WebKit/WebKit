/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/dtmf_queue.h"

#include <string.h>

namespace webrtc {
DTMFqueue::DTMFqueue() : next_empty_index_(0) {
  memset(dtmf_key_, 0, sizeof(dtmf_key_));
  memset(dtmf_length, 0, sizeof(dtmf_length));
  memset(dtmf_level_, 0, sizeof(dtmf_level_));
}

DTMFqueue::~DTMFqueue() {}

int32_t DTMFqueue::AddDTMF(uint8_t key, uint16_t len, uint8_t level) {
  rtc::CritScope lock(&dtmf_critsect_);

  if (next_empty_index_ >= DTMF_OUTBAND_MAX) {
    return -1;
  }
  int32_t index = next_empty_index_;
  dtmf_key_[index] = key;
  dtmf_length[index] = len;
  dtmf_level_[index] = level;
  next_empty_index_++;
  return 0;
}

int8_t DTMFqueue::NextDTMF(uint8_t* dtmf_key, uint16_t* len, uint8_t* level) {
  rtc::CritScope lock(&dtmf_critsect_);
  if (next_empty_index_ == 0)
    return -1;

  *dtmf_key = dtmf_key_[0];
  *len = dtmf_length[0];
  *level = dtmf_level_[0];

  memmove(&(dtmf_key_[0]), &(dtmf_key_[1]),
          next_empty_index_ * sizeof(uint8_t));
  memmove(&(dtmf_length[0]), &(dtmf_length[1]),
          next_empty_index_ * sizeof(uint16_t));
  memmove(&(dtmf_level_[0]), &(dtmf_level_[1]),
          next_empty_index_ * sizeof(uint8_t));

  next_empty_index_--;
  return 0;
}

bool DTMFqueue::PendingDTMF() {
  rtc::CritScope lock(&dtmf_critsect_);
  return next_empty_index_ > 0;
}
}  // namespace webrtc
