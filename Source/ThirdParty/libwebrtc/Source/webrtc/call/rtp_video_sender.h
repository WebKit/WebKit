/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_VIDEO_SENDER_H_
#define CALL_RTP_VIDEO_SENDER_H_

#include <map>
#include <memory>
#include <vector>

#include "api/call/transport.h"
#include "api/video_codecs/video_encoder.h"
#include "call/rtp_config.h"
#include "call/rtp_payload_params.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/rtp_video_sender_interface.h"
#include "common_types.h"  // NOLINT(build/include)
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class RTPFragmentationHeader;
class RtpRtcp;
class RtpTransportControllerSendInterface;

// RtpVideoSender routes outgoing data to the correct sending RTP module, based
// on the simulcast layer in RTPVideoHeader.
class RtpVideoSender : public RtpVideoSenderInterface {
 public:
  // Rtp modules are assumed to be sorted in simulcast index order.
  RtpVideoSender(
      const std::vector<uint32_t>& ssrcs,
      std::map<uint32_t, RtpState> suspended_ssrcs,
      const std::map<uint32_t, RtpPayloadState>& states,
      const RtpConfig& rtp_config,
      const RtcpConfig& rtcp_config,
      Transport* send_transport,
      const RtpSenderObservers& observers,
      RtpTransportControllerSendInterface* transport,
      RtcEventLog* event_log,
      RateLimiter* retransmission_limiter);  // move inside RtpTransport
  ~RtpVideoSender() override;

  // RegisterProcessThread register |module_process_thread| with those objects
  // that use it. Registration has to happen on the thread were
  // |module_process_thread| was created (libjingle's worker thread).
  // TODO(perkj): Replace the use of |module_process_thread| with a TaskQueue,
  // maybe |worker_queue|.
  void RegisterProcessThread(ProcessThread* module_process_thread) override;
  void DeRegisterProcessThread() override;

  // RtpVideoSender will only route packets if being active, all packets will be
  // dropped otherwise.
  void SetActive(bool active) override;
  // Sets the sending status of the rtp modules and appropriately sets the
  // payload router to active if any rtp modules are active.
  void SetActiveModules(const std::vector<bool> active_modules) override;
  bool IsActive() override;

  void OnNetworkAvailability(bool network_available) override;
  std::map<uint32_t, RtpState> GetRtpStates() const override;
  std::map<uint32_t, RtpPayloadState> GetRtpPayloadStates() const override;

  bool FecEnabled() const override;

  bool NackEnabled() const override;

  void DeliverRtcp(const uint8_t* packet, size_t length) override;

  void ProtectionRequest(const FecProtectionParams* delta_params,
                         const FecProtectionParams* key_params,
                         uint32_t* sent_video_rate_bps,
                         uint32_t* sent_nack_rate_bps,
                         uint32_t* sent_fec_rate_bps) override;

  void SetMaxRtpPacketSize(size_t max_rtp_packet_size) override;

  // Implements EncodedImageCallback.
  // Returns 0 if the packet was routed / sent, -1 otherwise.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void OnBitrateAllocationUpdated(
      const VideoBitrateAllocation& bitrate) override;

 private:
  void UpdateModuleSendingState() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void ConfigureProtection(const RtpConfig& rtp_config);
  void ConfigureSsrcs(const RtpConfig& rtp_config);

  rtc::CriticalSection crit_;
  bool active_ RTC_GUARDED_BY(crit_);

  ProcessThread* module_process_thread_;
  rtc::ThreadChecker module_process_thread_checker_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;

  std::unique_ptr<FlexfecSender> flexfec_sender_;
  // Rtp modules are assumed to be sorted in simulcast index order.
  const std::vector<std::unique_ptr<RtpRtcp>> rtp_modules_;
  const RtpConfig rtp_config_;
  RtpTransportControllerSendInterface* const transport_;

  // When using the generic descriptor we want all simulcast streams to share
  // one frame id space (so that the SFU can switch stream without having to
  // rewrite the frame id), therefore |shared_frame_id| has to live in a place
  // where we are aware of all the different streams.
  int64_t shared_frame_id_ = 0;
  std::vector<RtpPayloadParams> params_ RTC_GUARDED_BY(crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpVideoSender);
};

}  // namespace webrtc

#endif  // CALL_RTP_VIDEO_SENDER_H_
