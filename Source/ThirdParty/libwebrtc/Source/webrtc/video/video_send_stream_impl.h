/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_VIDEO_SEND_STREAM_IMPL_H_
#define VIDEO_VIDEO_SEND_STREAM_IMPL_H_

#include <map>
#include <memory>
#include <unordered_set>
#include <vector>

#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_stream_encoder_interface.h"
#include "call/bitrate_allocator.h"
#include "call/rtp_video_sender_interface.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/weak_ptr.h"
#include "video/call_stats.h"
#include "video/encoder_rtcp_feedback.h"
#include "video/send_delay_stats.h"
#include "video/send_statistics_proxy.h"
#include "video/video_send_stream.h"

namespace webrtc {
namespace internal {

// VideoSendStreamImpl implements internal::VideoSendStream.
// It is created and destroyed on |worker_queue|. The intent is to decrease the
// need for locking and to ensure methods are called in sequence.
// Public methods except |DeliverRtcp| must be called on |worker_queue|.
// DeliverRtcp is called on the libjingle worker thread or a network thread.
// An encoder may deliver frames through the EncodedImageCallback on an
// arbitrary thread.
class VideoSendStreamImpl : public webrtc::BitrateAllocatorObserver,
                            public webrtc::OverheadObserver,
                            public webrtc::VCMProtectionCallback,
                            public VideoStreamEncoderInterface::EncoderSink,
                            public VideoBitrateAllocationObserver,
                            public webrtc::PacketFeedbackObserver {
 public:
  VideoSendStreamImpl(
      SendStatisticsProxy* stats_proxy,
      rtc::TaskQueue* worker_queue,
      CallStats* call_stats,
      RtpTransportControllerSendInterface* transport,
      BitrateAllocatorInterface* bitrate_allocator,
      SendDelayStats* send_delay_stats,
      VideoStreamEncoderInterface* video_stream_encoder,
      RtcEventLog* event_log,
      const VideoSendStream::Config* config,
      int initial_encoder_max_bitrate,
      double initial_encoder_bitrate_priority,
      std::map<uint32_t, RtpState> suspended_ssrcs,
      std::map<uint32_t, RtpPayloadState> suspended_payload_states,
      VideoEncoderConfig::ContentType content_type,
      std::unique_ptr<FecController> fec_controller);
  ~VideoSendStreamImpl() override;

  // RegisterProcessThread register |module_process_thread| with those objects
  // that use it. Registration has to happen on the thread were
  // |module_process_thread| was created (libjingle's worker thread).
  // TODO(perkj): Replace the use of |module_process_thread| with a TaskQueue,
  // maybe |worker_queue|.
  void RegisterProcessThread(ProcessThread* module_process_thread);
  void DeRegisterProcessThread();

  bool DeliverRtcp(const uint8_t* packet, size_t length);
  void UpdateActiveSimulcastLayers(const std::vector<bool> active_layers);
  void Start();
  void Stop();

  // TODO(holmer): Move these to RtpTransportControllerSend.
  std::map<uint32_t, RtpState> GetRtpStates() const;

  std::map<uint32_t, RtpPayloadState> GetRtpPayloadStates() const;

  void EnableEncodedFrameRecording(const std::vector<rtc::PlatformFile>& files,
                                   size_t byte_limit);

  void SetTransportOverhead(size_t transport_overhead_per_packet);

  absl::optional<float> configured_pacing_factor_;

  // From PacketFeedbackObserver.
  void OnPacketAdded(uint32_t ssrc, uint16_t seq_num) override;
  void OnPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector) override;

 private:
  class CheckEncoderActivityTask;

  // Implements BitrateAllocatorObserver.
  uint32_t OnBitrateUpdated(uint32_t bitrate_bps,
                            uint8_t fraction_loss,
                            int64_t rtt,
                            int64_t probing_interval_ms) override;

  // Implements webrtc::VCMProtectionCallback.
  int ProtectionRequest(const FecProtectionParams* delta_params,
                        const FecProtectionParams* key_params,
                        uint32_t* sent_video_rate_bps,
                        uint32_t* sent_nack_rate_bps,
                        uint32_t* sent_fec_rate_bps) override;

  // Implements OverheadObserver.
  void OnOverheadChanged(size_t overhead_bytes_per_packet) override;

  void OnEncoderConfigurationChanged(std::vector<VideoStream> streams,
                                     int min_transmit_bitrate_bps) override;

  // Implements EncodedImageCallback. The implementation routes encoded frames
  // to the |payload_router_| and |config.pre_encode_callback| if set.
  // Called on an arbitrary encoder callback thread.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  // Implements VideoBitrateAllocationObserver.
  void OnBitrateAllocationUpdated(
      const VideoBitrateAllocation& allocation) override;

  // Starts monitoring and sends a keyframe.
  void StartupVideoSendStream();
  // Removes the bitrate observer, stops monitoring and notifies the video
  // encoder of the bitrate update.
  void StopVideoSendStream();

  void ConfigureProtection();
  void ConfigureSsrcs();
  void SignalEncoderTimedOut();
  void SignalEncoderActive();

  const bool send_side_bwe_with_overhead_;

  SendStatisticsProxy* const stats_proxy_;
  const VideoSendStream::Config* const config_;

  std::unique_ptr<FecController> fec_controller_;
  rtc::TaskQueue* const worker_queue_;

  rtc::CriticalSection encoder_activity_crit_sect_;
  CheckEncoderActivityTask* check_encoder_activity_task_
      RTC_GUARDED_BY(encoder_activity_crit_sect_);

  CallStats* const call_stats_;
  RtpTransportControllerSendInterface* const transport_;
  BitrateAllocatorInterface* const bitrate_allocator_;

  rtc::CriticalSection ivf_writers_crit_;
  std::unique_ptr<IvfFileWriter>
      file_writers_[kMaxSimulcastStreams] RTC_GUARDED_BY(ivf_writers_crit_);

  int max_padding_bitrate_;
  int encoder_min_bitrate_bps_;
  uint32_t encoder_max_bitrate_bps_;
  uint32_t encoder_target_rate_bps_;
  double encoder_bitrate_priority_;
  bool has_packet_feedback_;

  VideoStreamEncoderInterface* const video_stream_encoder_;
  EncoderRtcpFeedback encoder_feedback_;

  RtcpBandwidthObserver* const bandwidth_observer_;
  RtpVideoSenderInterface* const rtp_video_sender_;

  // |weak_ptr_| to our self. This is used since we can not call
  // |weak_ptr_factory_.GetWeakPtr| from multiple sequences but it is ok to copy
  // an existing WeakPtr.
  rtc::WeakPtr<VideoSendStreamImpl> weak_ptr_;
  // |weak_ptr_factory_| must be declared last to make sure all WeakPtr's are
  // invalidated before any other members are destroyed.
  rtc::WeakPtrFactory<VideoSendStreamImpl> weak_ptr_factory_;

  rtc::CriticalSection overhead_bytes_per_packet_crit_;
  size_t overhead_bytes_per_packet_
      RTC_GUARDED_BY(overhead_bytes_per_packet_crit_);
  size_t transport_overhead_bytes_per_packet_;

  std::unordered_set<uint16_t> feedback_packet_seq_num_set_;
  std::vector<bool> loss_mask_vector_;
};
}  // namespace internal
}  // namespace webrtc
#endif  // VIDEO_VIDEO_SEND_STREAM_IMPL_H_
