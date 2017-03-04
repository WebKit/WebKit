/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_STREAM_DECODER_H_
#define WEBRTC_VIDEO_VIDEO_STREAM_DECODER_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CallStatsObserver;
class ChannelStatsObserver;
class EncodedImageCallback;
class ReceiveStatisticsProxy;
class VideoRenderCallback;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

enum StreamType {
  kViEStreamTypeNormal = 0,  // Normal media stream
  kViEStreamTypeRtx = 1      // Retransmission media stream
};

class VideoStreamDecoder : public VCMReceiveCallback,
                           public VCMReceiveStatisticsCallback,
                           public VCMDecoderTimingCallback,
                           public CallStatsObserver {
 public:
  friend class ChannelStatsObserver;

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
                        rtc::Optional<uint8_t> qp) override;
  int32_t ReceivedDecodedReferenceFrame(const uint64_t picture_id) override;
  void OnIncomingPayloadType(int payload_type) override;
  void OnDecoderImplementationName(const char* implementation_name) override;

  // Implements VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;
  void OnCompleteFrame(bool is_keyframe, size_t size_bytes) override;
  void OnFrameBufferTimingsUpdated(int decode_ms,
                                   int max_decode_ms,
                                   int current_delay_ms,
                                   int target_delay_ms,
                                   int jitter_buffer_ms,
                                   int min_playout_delay_ms,
                                   int render_delay_ms) override;

  // Implements VCMDecoderTimingCallback.
  void OnDecoderTiming(int decode_ms,
                       int max_decode_ms,
                       int current_delay_ms,
                       int target_delay_ms,
                       int jitter_buffer_ms,
                       int min_playout_delay_ms,
                       int render_delay_ms) override;

  void RegisterReceiveStatisticsProxy(
      ReceiveStatisticsProxy* receive_statistics_proxy);

  // Implements StatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

 private:
  // Used for all registered callbacks except rendering.
  rtc::CriticalSection crit_;

  vcm::VideoReceiver* const video_receiver_;

  ReceiveStatisticsProxy* const receive_stats_callback_;
  rtc::VideoSinkInterface<VideoFrame>* const incoming_video_stream_;

  int64_t last_rtt_ms_ GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_STREAM_DECODER_H_
