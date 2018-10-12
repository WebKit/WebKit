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
 *  and push the result to the encoder via VideoEncoderCallback.
 */

#ifndef MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_
#define MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_

#include "modules/bitrate_controller/include/bitrate_controller.h"

#include <list>
#include <map>
#include <utility>
#include <vector>

#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class BitrateControllerImpl : public BitrateController {
 public:
  // TODO(perkj): BitrateObserver has been deprecated and is not used in WebRTC.
  // |observer| is left for project that is not yet updated.
  BitrateControllerImpl(const Clock* clock,
                        BitrateObserver* observer,
                        RtcEventLog* event_log);
  virtual ~BitrateControllerImpl() {}

  bool AvailableBandwidth(uint32_t* bandwidth) const override;

  RTC_DEPRECATED RtcpBandwidthObserver* CreateRtcpBandwidthObserver() override;

  // Deprecated
  void SetStartBitrate(int start_bitrate_bps) override;
  // Deprecated
  void SetMinMaxBitrate(int min_bitrate_bps, int max_bitrate_bps) override;

  void SetBitrates(int start_bitrate_bps,
                   int min_bitrate_bps,
                   int max_bitrate_bps) override;

  void ResetBitrates(int bitrate_bps,
                     int min_bitrate_bps,
                     int max_bitrate_bps) override;

  // Returns true if the parameters have changed since the last call.
  bool GetNetworkParameters(uint32_t* bitrate,
                            uint8_t* fraction_loss,
                            int64_t* rtt) override;

  void OnDelayBasedBweResult(const DelayBasedBwe::Result& result) override;

  int64_t TimeUntilNextProcess() override;
  void Process() override;

 private:
  class RtcpBandwidthObserverImpl;

  // Called by BitrateObserver's direct from the RTCP module.
  // Implements RtcpBandwidthObserver.
  void OnReceivedEstimatedBitrate(uint32_t bitrate) override;

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override;

  // Deprecated
  void MaybeTriggerOnNetworkChanged();

  void OnNetworkChanged(uint32_t bitrate,
                        uint8_t fraction_loss,  // 0 - 255.
                        int64_t rtt) RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  // Used by process thread.
  const Clock* const clock_;
  BitrateObserver* const observer_;
  int64_t last_bitrate_update_ms_;
  RtcEventLog* const event_log_;

  rtc::CriticalSection critsect_;
  std::map<uint32_t, uint32_t> ssrc_to_last_received_extended_high_seq_num_
      RTC_GUARDED_BY(critsect_);
  SendSideBandwidthEstimation bandwidth_estimation_ RTC_GUARDED_BY(critsect_);

  uint32_t last_bitrate_bps_ RTC_GUARDED_BY(critsect_);
  uint8_t last_fraction_loss_ RTC_GUARDED_BY(critsect_);
  int64_t last_rtt_ms_ RTC_GUARDED_BY(critsect_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(BitrateControllerImpl);
};
}  // namespace webrtc
#endif  // MODULES_BITRATE_CONTROLLER_BITRATE_CONTROLLER_IMPL_H_
