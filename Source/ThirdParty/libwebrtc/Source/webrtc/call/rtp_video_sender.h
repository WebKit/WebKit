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
#include <unordered_set>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/fec_controller.h"
#include "api/fec_controller_override.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/video_codecs/video_encoder.h"
#include "call/rtp_config.h"
#include "call/rtp_payload_params.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/rtp_video_sender_interface.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class FrameEncryptorInterface;
class RTPFragmentationHeader;
class RtpRtcp;
class RtpTransportControllerSendInterface;

namespace webrtc_internal_rtp_video_sender {
// RTP state for a single simulcast stream. Internal to the implementation of
// RtpVideoSender.
struct RtpStreamSender {
  RtpStreamSender(std::unique_ptr<RtpRtcp> rtp_rtcp,
                  std::unique_ptr<RTPSenderVideo> sender_video,
                  std::unique_ptr<VideoFecGenerator> fec_generator);
  ~RtpStreamSender();

  RtpStreamSender(RtpStreamSender&&) = default;
  RtpStreamSender& operator=(RtpStreamSender&&) = default;

  // Note: Needs pointer stability.
  std::unique_ptr<RtpRtcp> rtp_rtcp;
  std::unique_ptr<RTPSenderVideo> sender_video;
  std::unique_ptr<VideoFecGenerator> fec_generator;
};

}  // namespace webrtc_internal_rtp_video_sender

// RtpVideoSender routes outgoing data to the correct sending RTP module, based
// on the simulcast layer in RTPVideoHeader.
class RtpVideoSender : public RtpVideoSenderInterface,
                       public OverheadObserver,
                       public VCMProtectionCallback,
                       public StreamFeedbackObserver {
 public:
  // Rtp modules are assumed to be sorted in simulcast index order.
  RtpVideoSender(
      Clock* clock,
      std::map<uint32_t, RtpState> suspended_ssrcs,
      const std::map<uint32_t, RtpPayloadState>& states,
      const RtpConfig& rtp_config,
      int rtcp_report_interval_ms,
      Transport* send_transport,
      const RtpSenderObservers& observers,
      RtpTransportControllerSendInterface* transport,
      RtcEventLog* event_log,
      RateLimiter* retransmission_limiter,  // move inside RtpTransport
      std::unique_ptr<FecController> fec_controller,
      FrameEncryptorInterface* frame_encryptor,
      const CryptoOptions& crypto_options,  // move inside RtpTransport
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer);
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

  void DeliverRtcp(const uint8_t* packet, size_t length) override;

  // Implements webrtc::VCMProtectionCallback.
  int ProtectionRequest(const FecProtectionParams* delta_params,
                        const FecProtectionParams* key_params,
                        uint32_t* sent_video_rate_bps,
                        uint32_t* sent_nack_rate_bps,
                        uint32_t* sent_fec_rate_bps) override;

  // Implements FecControllerOverride.
  void SetFecAllowed(bool fec_allowed) override;

  // Implements EncodedImageCallback.
  // Returns 0 if the packet was routed / sent, -1 otherwise.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void OnBitrateAllocationUpdated(
      const VideoBitrateAllocation& bitrate) override;

  void OnTransportOverheadChanged(
      size_t transport_overhead_bytes_per_packet) override;
  // Implements OverheadObserver.
  void OnOverheadChanged(size_t overhead_bytes_per_packet) override;
  void OnBitrateUpdated(BitrateAllocationUpdate update, int framerate) override;
  uint32_t GetPayloadBitrateBps() const override;
  uint32_t GetProtectionBitrateBps() const override;
  void SetEncodingData(size_t width,
                       size_t height,
                       size_t num_temporal_layers) override;

  std::vector<RtpSequenceNumberMap::Info> GetSentRtpPacketInfos(
      uint32_t ssrc,
      rtc::ArrayView<const uint16_t> sequence_numbers) const override;

  // From StreamFeedbackObserver.
  void OnPacketFeedbackVector(
      std::vector<StreamPacketInfo> packet_feedback_vector) override;

 private:
  void UpdateModuleSendingState() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void ConfigureProtection();
  void ConfigureSsrcs();
  void ConfigureRids();
  bool NackEnabled() const;
  uint32_t GetPacketizationOverheadRate() const;

  const bool send_side_bwe_with_overhead_;
  const bool account_for_packetization_overhead_;
  const bool use_early_loss_detection_;
  const bool has_packet_feedback_;

  // TODO(holmer): Remove crit_ once RtpVideoSender runs on the
  // transport task queue.
  rtc::CriticalSection crit_;
  bool active_ RTC_GUARDED_BY(crit_);

  ProcessThread* module_process_thread_;
  rtc::ThreadChecker module_process_thread_checker_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;

  const std::unique_ptr<FecController> fec_controller_;
  bool fec_allowed_ RTC_GUARDED_BY(crit_);

  // Rtp modules are assumed to be sorted in simulcast index order.
  const std::vector<webrtc_internal_rtp_video_sender::RtpStreamSender>
      rtp_streams_;
  const RtpConfig rtp_config_;
  const absl::optional<VideoCodecType> codec_type_;
  RtpTransportControllerSendInterface* const transport_;

  // When using the generic descriptor we want all simulcast streams to share
  // one frame id space (so that the SFU can switch stream without having to
  // rewrite the frame id), therefore |shared_frame_id| has to live in a place
  // where we are aware of all the different streams.
  int64_t shared_frame_id_ = 0;
  std::vector<RtpPayloadParams> params_ RTC_GUARDED_BY(crit_);

  size_t transport_overhead_bytes_per_packet_ RTC_GUARDED_BY(crit_);
  size_t overhead_bytes_per_packet_ RTC_GUARDED_BY(crit_);
  uint32_t protection_bitrate_bps_;
  uint32_t encoder_target_rate_bps_;

  std::vector<bool> loss_mask_vector_ RTC_GUARDED_BY(crit_);

  std::vector<FrameCounts> frame_counts_ RTC_GUARDED_BY(crit_);
  FrameCountObserver* const frame_count_observer_;

  // Effectively const map from SSRC to RtpRtcp, for all media SSRCs.
  // This map is set at construction time and never changed, but it's
  // non-trivial to make it properly const.
  std::map<uint32_t, RtpRtcp*> ssrc_to_rtp_module_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpVideoSender);
};

}  // namespace webrtc

#endif  // CALL_RTP_VIDEO_SENDER_H_
