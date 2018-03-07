/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_PAYLOAD_ROUTER_H_
#define VIDEO_PAYLOAD_ROUTER_H_

#include <map>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTPFragmentationHeader;
class RtpRtcp;
struct RTPVideoHeader;

// PayloadRouter routes outgoing data to the correct sending RTP module, based
// on the simulcast layer in RTPVideoHeader.
class PayloadRouter : public EncodedImageCallback {
 public:
  // Rtp modules are assumed to be sorted in simulcast index order.
  PayloadRouter(const std::vector<RtpRtcp*>& rtp_modules,
                const std::vector<uint32_t>& ssrcs,
                int payload_type,
                const std::map<uint32_t, RtpPayloadState>& states);
  ~PayloadRouter();

  // PayloadRouter will only route packets if being active, all packets will be
  // dropped otherwise.
  void SetActive(bool active);
  bool IsActive();

  std::map<uint32_t, RtpPayloadState> GetRtpPayloadStates() const;

  // Implements EncodedImageCallback.
  // Returns 0 if the packet was routed / sent, -1 otherwise.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void OnBitrateAllocationUpdated(const BitrateAllocation& bitrate);

 private:
  class RtpPayloadParams;

  void UpdateModuleSendingState() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::CriticalSection crit_;
  bool active_ RTC_GUARDED_BY(crit_);

  // Rtp modules are assumed to be sorted in simulcast index order. Not owned.
  const std::vector<RtpRtcp*> rtp_modules_;
  const int payload_type_;

  const bool forced_fallback_enabled_;
  std::vector<RtpPayloadParams> params_ RTC_GUARDED_BY(crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(PayloadRouter);
};

}  // namespace webrtc

#endif  // VIDEO_PAYLOAD_ROUTER_H_
