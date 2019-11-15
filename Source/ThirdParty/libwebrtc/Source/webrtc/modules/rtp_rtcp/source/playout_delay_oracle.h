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

#include "absl/types/optional.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/critical_section.h"
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
class PlayoutDelayOracle : public RtcpAckObserver {
 public:
  PlayoutDelayOracle();
  ~PlayoutDelayOracle() override;

  // The playout delay to be added to a packet. The input delays are provided by
  // the application, with -1 meaning unchanged/unspecified. The output delay
  // are the values to be attached to packets on the wire. Presence and value
  // depends on the current input, previous inputs, and received acks from the
  // remote end.
  absl::optional<PlayoutDelay> PlayoutDelayToSend(
      PlayoutDelay requested_delay) const;

  void OnSentPacket(uint16_t sequence_number,
                    absl::optional<PlayoutDelay> playout_delay);

  void OnReceivedAck(int64_t extended_highest_sequence_number) override;

 private:
  // The playout delay information is updated from the encoder thread(s).
  // The sequence number feedback is updated from the worker thread.
  // Guards access to data across multiple threads.
  rtc::CriticalSection crit_sect_;
  // The oldest sequence number on which the current playout delay values have
  // been sent. When set, it means we need to attach extension to sent packets.
  absl::optional<int64_t> unacked_sequence_number_ RTC_GUARDED_BY(crit_sect_);
  // Sequence number unwrapper for sent packets.

  // TODO(nisse): Could potentially get out of sync with the unwrapper used by
  // the caller of OnReceivedAck.
  SequenceNumberUnwrapper unwrapper_ RTC_GUARDED_BY(crit_sect_);
  // Playout delay values on the next frame if |send_playout_delay_| is set.
  PlayoutDelay latest_delay_ RTC_GUARDED_BY(crit_sect_) = {-1, -1};

  RTC_DISALLOW_COPY_AND_ASSIGN(PlayoutDelayOracle);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_
