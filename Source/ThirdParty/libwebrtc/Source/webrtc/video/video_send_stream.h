/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_SEND_STREAM_H_
#define VIDEO_VIDEO_SEND_STREAM_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/fec_controller.h"
#include "api/field_trials_view.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "call/bitrate_allocator.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "rtc_base/event.h"
#include "rtc_base/system/no_unique_address.h"
#include "video/encoder_rtcp_feedback.h"
#include "video/send_delay_stats.h"
#include "video/send_statistics_proxy.h"
#include "video/video_send_stream_impl.h"
#include "video/video_stream_encoder_interface.h"

namespace webrtc {
namespace test {
class VideoSendStreamPeer;
}  // namespace test

class CallStats;
class IvfFileWriter;
class RateLimiter;
class RtpRtcp;
class RtpTransportControllerSendInterface;
class RtcEventLog;

namespace internal {

class VideoSendStreamImpl;

// VideoSendStream implements webrtc::VideoSendStream.
// Internally, it delegates all public methods to VideoSendStreamImpl and / or
// VideoStreamEncoder.
class VideoSendStream : public webrtc::VideoSendStream {
 public:
  using RtpStateMap = std::map<uint32_t, RtpState>;
  using RtpPayloadStateMap = std::map<uint32_t, RtpPayloadState>;

  VideoSendStream(
      Clock* clock,
      int num_cpu_cores,
      TaskQueueFactory* task_queue_factory,
      TaskQueueBase* network_queue,
      RtcpRttStats* call_stats,
      RtpTransportControllerSendInterface* transport,
      BitrateAllocatorInterface* bitrate_allocator,
      SendDelayStats* send_delay_stats,
      RtcEventLog* event_log,
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config,
      const std::map<uint32_t, RtpState>& suspended_ssrcs,
      const std::map<uint32_t, RtpPayloadState>& suspended_payload_states,
      std::unique_ptr<FecController> fec_controller,
      const FieldTrialsView& field_trials);

  ~VideoSendStream() override;

  void DeliverRtcp(const uint8_t* packet, size_t length);

  // webrtc::VideoSendStream implementation.
  void Start() override;
  void StartPerRtpStream(std::vector<bool> active_layers) override;
  void Stop() override;
  bool started() override;

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override;
  std::vector<rtc::scoped_refptr<Resource>> GetAdaptationResources() override;

  void SetSource(rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
                 const DegradationPreference& degradation_preference) override;

  void ReconfigureVideoEncoder(VideoEncoderConfig config) override;
  void ReconfigureVideoEncoder(VideoEncoderConfig config,
                               SetParametersCallback callback) override;
  Stats GetStats() override;

  void StopPermanentlyAndGetRtpStates(RtpStateMap* rtp_state_map,
                                      RtpPayloadStateMap* payload_state_map);
  void GenerateKeyFrame(const std::vector<std::string>& rids) override;

 private:
  friend class test::VideoSendStreamPeer;
  class OnSendPacketObserver : public SendPacketObserver {
   public:
    OnSendPacketObserver(SendStatisticsProxy* stats_proxy,
                         SendDelayStats* send_delay_stats)
        : stats_proxy_(*stats_proxy), send_delay_stats_(*send_delay_stats) {}

    void OnSendPacket(absl::optional<uint16_t> packet_id,
                      Timestamp capture_time,
                      uint32_t ssrc) override {
      stats_proxy_.OnSendPacket(ssrc, capture_time);
      if (packet_id.has_value()) {
        send_delay_stats_.OnSendPacket(*packet_id, capture_time, ssrc);
      }
    }

   private:
    SendStatisticsProxy& stats_proxy_;
    SendDelayStats& send_delay_stats_;
  };

  absl::optional<float> GetPacingFactorOverride() const;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker thread_checker_;
  RtpTransportControllerSendInterface* const transport_;

  SendStatisticsProxy stats_proxy_;
  OnSendPacketObserver send_packet_observer_;
  const VideoSendStream::Config config_;
  const VideoEncoderConfig::ContentType content_type_;
  std::unique_ptr<VideoStreamEncoderInterface> video_stream_encoder_;
  EncoderRtcpFeedback encoder_feedback_;
  RtpVideoSenderInterface* const rtp_video_sender_;
  VideoSendStreamImpl send_stream_;
  bool running_ RTC_GUARDED_BY(thread_checker_) = false;
};

}  // namespace internal
}  // namespace webrtc

#endif  // VIDEO_VIDEO_SEND_STREAM_H_
