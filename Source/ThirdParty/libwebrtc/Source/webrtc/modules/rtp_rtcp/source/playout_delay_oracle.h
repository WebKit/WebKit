/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_
#define MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_

#include <stdint.h>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This class tracks the application requests to limit minimum and maximum
// playout delay and makes a decision on whether the current RTP frame
// should include the playout out delay extension header.
//
//  Playout delay can be defined in terms of capture and render time as follows:
//
// Render time = Capture time in receiver time + playout delay
//
// The application specifies a minimum and maximum limit for the playout delay
// which are both communicated to the receiver and the receiver can adapt
// the playout delay within this range based on observed network jitter.
class PlayoutDelayOracle {
 public:
  PlayoutDelayOracle();
  ~PlayoutDelayOracle();

  // Returns true if the current frame should include the playout delay
  // extension
  bool send_playout_delay() const {
    rtc::CritScope lock(&crit_sect_);
    return send_playout_delay_;
  }

  // Returns current playout delay.
  PlayoutDelay playout_delay() const {
    rtc::CritScope lock(&crit_sect_);
    return playout_delay_;
  }

  // Updates the application requested playout delay, current ssrc
  // and the current sequence number.
  void UpdateRequest(uint32_t ssrc,
                     PlayoutDelay playout_delay,
                     uint16_t seq_num);

  void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks);

 private:
  // The playout delay information is updated from the encoder thread(s).
  // The sequence number feedback is updated from the worker thread.
  // Guards access to data across multiple threads.
  rtc::CriticalSection crit_sect_;
  // The current highest sequence number on which playout delay has been sent.
  int64_t high_sequence_number_ RTC_GUARDED_BY(crit_sect_);
  // Indicates whether the playout delay should go on the next frame.
  bool send_playout_delay_ RTC_GUARDED_BY(crit_sect_);
  // Sender ssrc.
  uint32_t ssrc_ RTC_GUARDED_BY(crit_sect_);
  // Sequence number unwrapper.
  SequenceNumberUnwrapper unwrapper_ RTC_GUARDED_BY(crit_sect_);
  // Playout delay values on the next frame if |send_playout_delay_| is set.
  PlayoutDelay playout_delay_ RTC_GUARDED_BY(crit_sect_);

  RTC_DISALLOW_COPY_AND_ASSIGN(PlayoutDelayOracle);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_
