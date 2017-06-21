/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Usage: this class will register multiple RtcpBitrateObserver's one at each
 *  RTCP module. It will aggregate the results and run one bandwidth estimation
 *  and push the result to the encoders via BitrateObserver(s).
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_

#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtcEventLog;

// Deprecated
// TODO(perkj): Remove BitrateObserver when no implementations use it.
class BitrateObserver {
  // Observer class for bitrate changes announced due to change in bandwidth
  // estimate or due to bitrate allocation changes. Fraction loss and rtt is
  // also part of this callback to allow the obsevrer to optimize its settings
  // for different types of network environments. The bitrate does not include
  // packet headers and is measured in bits per second.
 public:
  virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                uint8_t fraction_loss,  // 0 - 255.
                                int64_t rtt_ms) = 0;

  virtual ~BitrateObserver() {}
};

class BitrateController : public Module,
                          public RtcpBandwidthObserver {
  // This class collects feedback from all streams sent to a peer (via
  // RTCPBandwidthObservers). It does one  aggregated send side bandwidth
  // estimation and divide the available bitrate between all its registered
  // BitrateObservers.
 public:
  static const int kDefaultStartBitratebps = 300000;

  // Deprecated:
  // TODO(perkj): BitrateObserver has been deprecated and is not used in WebRTC.
  // Remove this method once other other projects does not use it.
  static BitrateController* CreateBitrateController(const Clock* clock,
                                                    BitrateObserver* observer,
                                                    RtcEventLog* event_log);

  static BitrateController* CreateBitrateController(const Clock* clock,
                                                    RtcEventLog* event_log);

  virtual ~BitrateController() {}

  // Creates RtcpBandwidthObserver caller responsible to delete.
  virtual RtcpBandwidthObserver* CreateRtcpBandwidthObserver() = 0;

  // Deprecated
  virtual void SetStartBitrate(int start_bitrate_bps) = 0;
  // Deprecated
  virtual void SetMinMaxBitrate(int min_bitrate_bps, int max_bitrate_bps) = 0;
  virtual void SetBitrates(int start_bitrate_bps,
                           int min_bitrate_bps,
                           int max_bitrate_bps) = 0;

  virtual void ResetBitrates(int bitrate_bps,
                             int min_bitrate_bps,
                             int max_bitrate_bps) = 0;

  virtual void OnDelayBasedBweResult(const DelayBasedBwe::Result& result) = 0;

  // Gets the available payload bandwidth in bits per second. Note that
  // this bandwidth excludes packet headers.
  virtual bool AvailableBandwidth(uint32_t* bandwidth) const = 0;

  virtual void SetReservedBitrate(uint32_t reserved_bitrate_bps) = 0;

  virtual bool GetNetworkParameters(uint32_t* bitrate,
                                    uint8_t* fraction_loss,
                                    int64_t* rtt) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_
