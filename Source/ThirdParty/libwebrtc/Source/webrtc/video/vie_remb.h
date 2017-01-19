/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIE_REMB_H_
#define WEBRTC_VIDEO_VIE_REMB_H_

#include <list>
#include <utility>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class ProcessThread;
class RtpRtcp;

class VieRemb : public RemoteBitrateObserver {
 public:
  explicit VieRemb(Clock* clock);
  ~VieRemb();

  // Called to add a receive channel to include in the REMB packet.
  void AddReceiveChannel(RtpRtcp* rtp_rtcp);

  // Removes the specified channel from REMB estimate.
  void RemoveReceiveChannel(RtpRtcp* rtp_rtcp);

  // Called to add a module that can generate and send REMB RTCP.
  void AddRembSender(RtpRtcp* rtp_rtcp);

  // Removes a REMB RTCP sender.
  void RemoveRembSender(RtpRtcp* rtp_rtcp);

  // Returns true if the instance is in use, false otherwise.
  bool InUse() const;

  // Called every time there is a new bitrate estimate for a receive channel
  // group. This call will trigger a new RTCP REMB packet if the bitrate
  // estimate has decreased or if no RTCP REMB packet has been sent for
  // a certain time interval.
  // Implements RtpReceiveBitrateUpdate.
  virtual void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                       uint32_t bitrate);

 private:
  typedef std::list<RtpRtcp*> RtpModules;

  Clock* const clock_;
  rtc::CriticalSection list_crit_;

  // The last time a REMB was sent.
  int64_t last_remb_time_;
  uint32_t last_send_bitrate_;

  // All RtpRtcp modules to include in the REMB packet.
  RtpModules receive_modules_;

  // All modules that can send REMB RTCP.
  RtpModules rtcp_sender_;

  // The last bitrate update.
  uint32_t bitrate_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIE_REMB_H_
