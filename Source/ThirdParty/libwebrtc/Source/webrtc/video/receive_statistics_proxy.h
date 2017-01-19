/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
#define WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_

#include <map>
#include <string>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/ratetracker.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/frame_callback.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/video/report_block_stats.h"
#include "webrtc/video/stats_counter.h"
#include "webrtc/video/video_stream_decoder.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {

class Clock;
class ViECodec;
class ViEDecoderObserver;
struct CodecSpecificInfo;

class ReceiveStatisticsProxy : public VCMReceiveStatisticsCallback,
                               public RtcpStatisticsCallback,
                               public RtcpPacketTypeCounterObserver,
                               public StreamDataCountersCallback {
 public:
  ReceiveStatisticsProxy(const VideoReceiveStream::Config* config,
                         Clock* clock);
  virtual ~ReceiveStatisticsProxy();

  VideoReceiveStream::Stats GetStats() const;

  void OnDecodedFrame();
  void OnSyncOffsetUpdated(int64_t sync_offset_ms, double estimated_freq_khz);
  void OnRenderedFrame(const VideoFrame& frame);
  void OnIncomingPayloadType(int payload_type);
  void OnDecoderImplementationName(const char* implementation_name);
  void OnIncomingRate(unsigned int framerate, unsigned int bitrate_bps);
  void OnDecoderTiming(int decode_ms,
                       int max_decode_ms,
                       int current_delay_ms,
                       int target_delay_ms,
                       int jitter_buffer_ms,
                       int min_playout_delay_ms,
                       int render_delay_ms,
                       int64_t rtt_ms);

  void OnPreDecode(const EncodedImage& encoded_image,
                   const CodecSpecificInfo* codec_specific_info);

  // Overrides VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bitRate, uint32_t frameRate) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;

  // Overrides RtcpStatisticsCallback.
  void StatisticsUpdated(const webrtc::RtcpStatistics& statistics,
                         uint32_t ssrc) override;
  void CNameChanged(const char* cname, uint32_t ssrc) override;

  // Overrides RtcpPacketTypeCounterObserver.
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override;
  // Overrides StreamDataCountersCallback.
  void DataCountersUpdated(const webrtc::StreamDataCounters& counters,
                           uint32_t ssrc) override;

 private:
  struct SampleCounter {
    SampleCounter() : sum(0), num_samples(0) {}
    void Add(int sample);
    int Avg(int min_required_samples) const;

   private:
    int sum;
    int num_samples;
  };
  struct QpCounters {
    SampleCounter vp8;
  };

  void UpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  // Ownership of this object lies with the owner of the ReceiveStatisticsProxy
  // instance.  Lifetime is guaranteed to outlive |this|.
  // TODO(tommi): In practice the config_ reference is only used for accessing
  // config_.rtp.ulpfec.ulpfec_payload_type.  Instead of holding a pointer back,
  // we could just store the value of ulpfec_payload_type and change the
  // ReceiveStatisticsProxy() ctor to accept a const& of Config (since we'll
  // then no longer store a pointer to the object).
  const VideoReceiveStream::Config& config_;
  const int64_t start_ms_;

  rtc::CriticalSection crit_;
  VideoReceiveStream::Stats stats_ GUARDED_BY(crit_);
  RateStatistics decode_fps_estimator_ GUARDED_BY(crit_);
  RateStatistics renders_fps_estimator_ GUARDED_BY(crit_);
  rtc::RateTracker render_fps_tracker_ GUARDED_BY(crit_);
  rtc::RateTracker render_pixel_tracker_ GUARDED_BY(crit_);
  SampleCounter render_width_counter_ GUARDED_BY(crit_);
  SampleCounter render_height_counter_ GUARDED_BY(crit_);
  SampleCounter sync_offset_counter_ GUARDED_BY(crit_);
  SampleCounter decode_time_counter_ GUARDED_BY(crit_);
  SampleCounter jitter_buffer_delay_counter_ GUARDED_BY(crit_);
  SampleCounter target_delay_counter_ GUARDED_BY(crit_);
  SampleCounter current_delay_counter_ GUARDED_BY(crit_);
  SampleCounter delay_counter_ GUARDED_BY(crit_);
  SampleCounter e2e_delay_counter_ GUARDED_BY(crit_);
  MaxCounter freq_offset_counter_ GUARDED_BY(crit_);
  ReportBlockStats report_block_stats_ GUARDED_BY(crit_);
  QpCounters qp_counters_;  // Only accessed on the decoding thread.
  std::map<uint32_t, StreamDataCounters> rtx_stats_ GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
