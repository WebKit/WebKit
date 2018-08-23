/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_VOIP_METRIC_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_VOIP_METRIC_H_

#include "modules/include/module_common_types.h"

namespace webrtc {
namespace rtcp {

class VoipMetric {
 public:
  static const uint8_t kBlockType = 7;
  static const uint16_t kBlockLength = 8;
  static const size_t kLength = 4 * (kBlockLength + 1);  // 36
  VoipMetric();
  VoipMetric(const VoipMetric&) = default;
  ~VoipMetric() {}

  VoipMetric& operator=(const VoipMetric&) = default;

  void Parse(const uint8_t* buffer);

  // Fills buffer with the VoipMetric.
  // Consumes VoipMetric::kLength bytes.
  void Create(uint8_t* buffer) const;

  void SetMediaSsrc(uint32_t ssrc) { ssrc_ = ssrc; }
  void SetVoipMetric(const RTCPVoIPMetric& voip_metric) {
    voip_metric_ = voip_metric;
  }

  uint32_t ssrc() const { return ssrc_; }
  const RTCPVoIPMetric& voip_metric() const { return voip_metric_; }

 private:
  uint32_t ssrc_;
  RTCPVoIPMetric voip_metric_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_VOIP_METRIC_H_
