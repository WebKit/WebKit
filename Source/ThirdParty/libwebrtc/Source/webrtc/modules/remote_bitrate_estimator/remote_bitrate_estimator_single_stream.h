/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

class RemoteBitrateEstimatorSingleStream : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorSingleStream(RemoteBitrateObserver* observer,
                                     Clock* clock);
  virtual ~RemoteBitrateEstimatorSingleStream();

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override;
  void Process() override;
  int64_t TimeUntilNextProcess() override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;
  void RemoveStream(uint32_t ssrc) override;
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const override;
  void SetMinBitrate(int min_bitrate_bps) override;

 private:
  struct Detector;

  typedef std::map<uint32_t, Detector*> SsrcOveruseEstimatorMap;

  // Triggers a new estimate calculation.
  void UpdateEstimate(int64_t time_now)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  void GetSsrcs(std::vector<uint32_t>* ssrcs) const
      SHARED_LOCKS_REQUIRED(crit_sect_.get());

  // Returns |remote_rate_| if the pointed to object exists,
  // otherwise creates it.
  AimdRateControl* GetRemoteRate() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  Clock* clock_;
  SsrcOveruseEstimatorMap overuse_detectors_ GUARDED_BY(crit_sect_.get());
  RateStatistics incoming_bitrate_ GUARDED_BY(crit_sect_.get());
  uint32_t last_valid_incoming_bitrate_ GUARDED_BY(crit_sect_.get());
  std::unique_ptr<AimdRateControl> remote_rate_ GUARDED_BY(crit_sect_.get());
  RemoteBitrateObserver* observer_ GUARDED_BY(crit_sect_.get());
  std::unique_ptr<CriticalSectionWrapper> crit_sect_;
  int64_t last_process_time_;
  int64_t process_interval_ms_ GUARDED_BY(crit_sect_.get());
  bool uma_recorded_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RemoteBitrateEstimatorSingleStream);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
