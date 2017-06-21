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
#include "webrtc/video/quality_threshold.h"
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
                               public StreamDataCountersCallback,
                               public CallStatsObserver {
 public:
  ReceiveStatisticsProxy(const VideoReceiveStream::Config* config,
                         Clock* clock);
  virtual ~ReceiveStatisticsProxy();

  VideoReceiveStream::Stats GetStats() const;

  void OnDecodedFrame(rtc::Optional<uint8_t> qp, VideoContentType content_type);
  void OnSyncOffsetUpdated(int64_t sync_offset_ms, double estimated_freq_khz);
  void OnRenderedFrame(const VideoFrame& frame);
  void OnIncomingPayloadType(int payload_type);
  void OnDecoderImplementationName(const char* implementation_name);
  void OnIncomingRate(unsigned int framerate, unsigned int bitrate_bps);

  void OnPreDecode(const EncodedImage& encoded_image,
                   const CodecSpecificInfo* codec_specific_info);

  // Overrides VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bitRate, uint32_t frameRate) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnCompleteFrame(bool is_keyframe, size_t size_bytes) override;
  void OnFrameBufferTimingsUpdated(int decode_ms,
                                   int max_decode_ms,
                                   int current_delay_ms,
                                   int target_delay_ms,
                                   int jitter_buffer_ms,
                                   int min_playout_delay_ms,
                                   int render_delay_ms) override;

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

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

 private:
  struct SampleCounter {
    SampleCounter() : sum(0), num_samples(0) {}
    void Add(int sample);
    int Avg(int64_t min_required_samples) const;
    void Reset();

   private:
    int64_t sum;
    int64_t num_samples;
  };
  struct QpCounters {
    SampleCounter vp8;
  };

  void UpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void QualitySample() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Removes info about old frames and then updates the framerate.
  void UpdateFramerate(int64_t now_ms) const EXCLUSIVE_LOCKS_REQUIRED(crit_);

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
  int64_t last_sample_time_ GUARDED_BY(crit_);
  QualityThreshold fps_threshold_ GUARDED_BY(crit_);
  QualityThreshold qp_threshold_ GUARDED_BY(crit_);
  QualityThreshold variance_threshold_ GUARDED_BY(crit_);
  SampleCounter qp_sample_ GUARDED_BY(crit_);
  int num_bad_states_ GUARDED_BY(crit_);
  int num_certain_states_ GUARDED_BY(crit_);
  mutable VideoReceiveStream::Stats stats_ GUARDED_BY(crit_);
  RateStatistics decode_fps_estimator_ GUARDED_BY(crit_);
  RateStatistics renders_fps_estimator_ GUARDED_BY(crit_);
  rtc::RateTracker render_fps_tracker_ GUARDED_BY(crit_);
  rtc::RateTracker render_pixel_tracker_ GUARDED_BY(crit_);
  rtc::RateTracker total_byte_tracker_ GUARDED_BY(crit_);
  SampleCounter render_width_counter_ GUARDED_BY(crit_);
  SampleCounter render_height_counter_ GUARDED_BY(crit_);
  SampleCounter sync_offset_counter_ GUARDED_BY(crit_);
  SampleCounter decode_time_counter_ GUARDED_BY(crit_);
  SampleCounter jitter_buffer_delay_counter_ GUARDED_BY(crit_);
  SampleCounter target_delay_counter_ GUARDED_BY(crit_);
  SampleCounter current_delay_counter_ GUARDED_BY(crit_);
  SampleCounter delay_counter_ GUARDED_BY(crit_);
  SampleCounter e2e_delay_counter_video_ GUARDED_BY(crit_);
  SampleCounter e2e_delay_counter_screenshare_ GUARDED_BY(crit_);
  int64_t e2e_delay_max_ms_video_ GUARDED_BY(crit_);
  int64_t e2e_delay_max_ms_screenshare_ GUARDED_BY(crit_);
  MaxCounter freq_offset_counter_ GUARDED_BY(crit_);
  int64_t first_report_block_time_ms_ GUARDED_BY(crit_);
  ReportBlockStats report_block_stats_ GUARDED_BY(crit_);
  QpCounters qp_counters_;  // Only accessed on the decoding thread.
  std::map<uint32_t, StreamDataCounters> rtx_stats_ GUARDED_BY(crit_);
  int64_t avg_rtt_ms_ GUARDED_BY(crit_);
  mutable std::map<int64_t, size_t> frame_window_ GUARDED_BY(&crit_);
  VideoContentType last_content_type_ GUARDED_BY(&crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
