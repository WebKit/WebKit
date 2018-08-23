/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_DECODER_H_
#define VIDEO_VIDEO_STREAM_DECODER_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "api/video/video_sink_interface.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class ReceiveStatisticsProxy;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

enum StreamType {
  kViEStreamTypeNormal = 0,  // Normal media stream
  kViEStreamTypeRtx = 1      // Retransmission media stream
};

class VideoStreamDecoder : public VCMReceiveCallback,
                           public VCMReceiveStatisticsCallback {
 public:
  VideoStreamDecoder(
      vcm::VideoReceiver* video_receiver,
      VCMFrameTypeCallback* vcm_frame_type_callback,
      VCMPacketRequestCallback* vcm_packet_request_callback,
      bool enable_nack,
      bool enable_fec,
      ReceiveStatisticsProxy* receive_statistics_proxy,
      rtc::VideoSinkInterface<VideoFrame>* incoming_video_stream);
  ~VideoStreamDecoder();

  // Implements VCMReceiveCallback.
  int32_t FrameToRender(VideoFrame& video_frame,
                        absl::optional<uint8_t> qp,
                        VideoContentType content_type) override;
  int32_t ReceivedDecodedReferenceFrame(const uint64_t picture_id) override;
  void OnIncomingPayloadType(int payload_type) override;
  void OnDecoderImplementationName(const char* implementation_name) override;

  // Implements VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;
  void OnCompleteFrame(bool is_keyframe,
                       size_t size_bytes,
                       VideoContentType content_type) override;
  void OnFrameBufferTimingsUpdated(int decode_ms,
                                   int max_decode_ms,
                                   int current_delay_ms,
                                   int target_delay_ms,
                                   int jitter_buffer_ms,
                                   int min_playout_delay_ms,
                                   int render_delay_ms) override;

  void OnTimingFrameInfoUpdated(const TimingFrameInfo& info) override;

  void RegisterReceiveStatisticsProxy(
      ReceiveStatisticsProxy* receive_statistics_proxy);

  // Called by VideoReceiveStream when stats are updated.
  void UpdateRtt(int64_t max_rtt_ms);

 private:
  // Used for all registered callbacks except rendering.
  rtc::CriticalSection crit_;

  vcm::VideoReceiver* const video_receiver_;

  ReceiveStatisticsProxy* const receive_stats_callback_;
  rtc::VideoSinkInterface<VideoFrame>* const incoming_video_stream_;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_DECODER_H_
