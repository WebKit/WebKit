/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/test/video_quality_analyzer_interface.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// WebRTC will request a key frame after 3 seconds if no frames were received.
// We assume max frame rate ~60 fps, so 270 frames will cover max freeze without
// key frame request.
constexpr int kDefaultMaxFramesInFlightPerStream = 270;

class RateCounter {
 public:
  void AddEvent(Timestamp event_time);

  bool IsEmpty() const { return event_first_time_ == event_last_time_; }

  double GetEventsPerSecond() const;

 private:
  Timestamp event_first_time_ = Timestamp::MinusInfinity();
  Timestamp event_last_time_ = Timestamp::MinusInfinity();
  int64_t event_count_ = 0;
};

struct FrameCounters {
  // Count of frames, that were passed into WebRTC pipeline by video stream
  // source.
  int64_t captured = 0;
  // Count of frames that reached video encoder.
  int64_t pre_encoded = 0;
  // Count of encoded images that were produced by encoder for all requested
  // spatial layers and simulcast streams.
  int64_t encoded = 0;
  // Count of encoded images received in decoder for all requested spatial
  // layers and simulcast streams.
  int64_t received = 0;
  // Count of frames that were produced by decoder.
  int64_t decoded = 0;
  // Count of frames that went out from WebRTC pipeline to video sink.
  int64_t rendered = 0;
  // Count of frames that were dropped in any point between capturing and
  // rendering.
  int64_t dropped = 0;
};

struct StreamStats {
  SamplesStatsCounter psnr;
  SamplesStatsCounter ssim;
  // Time from frame encoded (time point on exit from encoder) to the
  // encoded image received in decoder (time point on entrance to decoder).
  SamplesStatsCounter transport_time_ms;
  // Time from frame was captured on device to time frame was displayed on
  // device.
  SamplesStatsCounter total_delay_incl_transport_ms;
  // Time between frames out from renderer.
  SamplesStatsCounter time_between_rendered_frames_ms;
  RateCounter encode_frame_rate;
  SamplesStatsCounter encode_time_ms;
  SamplesStatsCounter decode_time_ms;
  // Time from last packet of frame is received until it's sent to the renderer.
  SamplesStatsCounter receive_to_render_time_ms;
  // Max frames skipped between two nearest.
  SamplesStatsCounter skipped_between_rendered;
  // In the next 2 metrics freeze is a pause that is longer, than maximum:
  //  1. 150ms
  //  2. 3 * average time between two sequential frames.
  // Item 1 will cover high fps video and is a duration, that is noticeable by
  // human eye. Item 2 will cover low fps video like screen sharing.
  // Freeze duration.
  SamplesStatsCounter freeze_time_ms;
  // Mean time between one freeze end and next freeze start.
  SamplesStatsCounter time_between_freezes_ms;
  SamplesStatsCounter resolution_of_rendered_frame;

  int64_t dropped_by_encoder = 0;
  int64_t dropped_before_encoder = 0;
};

struct AnalyzerStats {
  // Size of analyzer internal comparisons queue, measured when new element
  // id added to the queue.
  SamplesStatsCounter comparisons_queue_size;
  // Number of performed comparisons of 2 video frames from captured and
  // rendered streams.
  int64_t comparisons_done = 0;
  // Number of cpu overloaded comparisons. Comparison is cpu overloaded if it is
  // queued when there are too many not processed comparisons in the queue.
  // Overloaded comparison doesn't include metrics like SSIM and PSNR that
  // require heavy computations.
  int64_t cpu_overloaded_comparisons_done = 0;
  // Number of memory overloaded comparisons. Comparison is memory overloaded if
  // it is queued when its captured frame was already removed due to high memory
  // usage for that video stream.
  int64_t memory_overloaded_comparisons_done = 0;
};

class DefaultVideoQualityAnalyzer : public VideoQualityAnalyzerInterface {
 public:
  explicit DefaultVideoQualityAnalyzer(
      bool heavy_metrics_computation_enabled = true,
      int max_frames_in_flight_per_stream_count =
          kDefaultMaxFramesInFlightPerStream);
  ~DefaultVideoQualityAnalyzer() override;

  void Start(std::string test_case_name, int max_threads_count) override;
  uint16_t OnFrameCaptured(const std::string& stream_label,
                           const VideoFrame& frame) override;
  void OnFramePreEncode(const VideoFrame& frame) override;
  void OnFrameEncoded(uint16_t frame_id,
                      const EncodedImage& encoded_image) override;
  void OnFrameDropped(EncodedImageCallback::DropReason reason) override;
  void OnFramePreDecode(uint16_t frame_id,
                        const EncodedImage& input_image) override;
  void OnFrameDecoded(const VideoFrame& frame,
                      absl::optional<int32_t> decode_time_ms,
                      absl::optional<uint8_t> qp) override;
  void OnFrameRendered(const VideoFrame& frame) override;
  void OnEncoderError(const VideoFrame& frame, int32_t error_code) override;
  void OnDecoderError(uint16_t frame_id, int32_t error_code) override;
  void Stop() override;
  std::string GetStreamLabel(uint16_t frame_id) override;
  void OnStatsReports(const std::string& pc_label,
                      const StatsReports& stats_reports) override {}

  // Returns set of stream labels, that were met during test call.
  std::set<std::string> GetKnownVideoStreams() const;
  const FrameCounters& GetGlobalCounters() const;
  // Returns frame counter per stream label. Valid stream labels can be obtained
  // by calling GetKnownVideoStreams()
  const std::map<std::string, FrameCounters>& GetPerStreamCounters() const;
  // Returns video quality stats per stream label. Valid stream labels can be
  // obtained by calling GetKnownVideoStreams()
  std::map<std::string, StreamStats> GetStats() const;
  AnalyzerStats GetAnalyzerStats() const;

 private:
  struct FrameStats {
    FrameStats(std::string stream_label, Timestamp captured_time);

    std::string stream_label;

    // Frame events timestamp.
    Timestamp captured_time;
    Timestamp pre_encode_time = Timestamp::MinusInfinity();
    Timestamp encoded_time = Timestamp::MinusInfinity();
    // Time when last packet of a frame was received.
    Timestamp received_time = Timestamp::MinusInfinity();
    Timestamp decode_start_time = Timestamp::MinusInfinity();
    Timestamp decode_end_time = Timestamp::MinusInfinity();
    Timestamp rendered_time = Timestamp::MinusInfinity();
    Timestamp prev_frame_rendered_time = Timestamp::MinusInfinity();

    absl::optional<int> rendered_frame_width = absl::nullopt;
    absl::optional<int> rendered_frame_height = absl::nullopt;
  };

  // Describes why comparison was done in overloaded mode (without calculating
  // PSNR and SSIM).
  enum class OverloadReason {
    kNone,
    // Not enough CPU to process all incoming comparisons.
    kCpu,
    // Not enough memory to store captured frames for all comparisons.
    kMemory
  };

  // Represents comparison between two VideoFrames. Contains video frames itself
  // and stats. Can be one of two types:
  //   1. Normal - in this case |captured| is presented and either |rendered| is
  //      presented and |dropped| is false, either |rendered| is omitted and
  //      |dropped| is true.
  //   2. Overloaded - in this case both |captured| and |rendered| are omitted
  //      because there were too many comparisons in the queue. |dropped| can be
  //      true or false showing was frame dropped or not.
  struct FrameComparison {
    FrameComparison(absl::optional<VideoFrame> captured,
                    absl::optional<VideoFrame> rendered,
                    bool dropped,
                    FrameStats frame_stats,
                    OverloadReason overload_reason);

    // Frames can be omitted if there too many computations waiting in the
    // queue.
    absl::optional<VideoFrame> captured;
    absl::optional<VideoFrame> rendered;
    // If true frame was dropped somewhere from capturing to rendering and
    // wasn't rendered on remote peer side. If |dropped| is true, |rendered|
    // will be |absl::nullopt|.
    bool dropped;
    FrameStats frame_stats;
    OverloadReason overload_reason;
  };

  // Represents a current state of video stream.
  class StreamState {
   public:
    void PushBack(uint16_t frame_id) { frame_ids_.emplace_back(frame_id); }

    uint16_t PopFront();

    bool Empty() { return frame_ids_.empty(); }

    uint16_t Front() { return frame_ids_.front(); }

    int GetAliveFramesCount() { return frame_ids_.size() - dead_frames_count_; }

    uint16_t MarkNextAliveFrameAsDead();

    void set_last_rendered_frame_time(Timestamp time) {
      last_rendered_frame_time_ = time;
    }
    absl::optional<Timestamp> last_rendered_frame_time() const {
      return last_rendered_frame_time_;
    }

   private:
    // To correctly determine dropped frames we have to know sequence of frames
    // in each stream so we will keep a list of frame ids inside the stream.
    // When the frame is rendered, we will pop ids from the list for until id
    // will match with rendered one. All ids before matched one can be
    // considered as dropped:
    //
    // | frame_id1 |->| frame_id2 |->| frame_id3 |->| frame_id4 |
    //
    // If we received frame with id frame_id3, then we will pop frame_id1 and
    // frame_id2 and consider that frames as dropped and then compare received
    // frame with the one from |captured_frames_in_flight_| with id frame_id3.
    std::deque<uint16_t> frame_ids_;
    // Count of dead frames in the beginning of the deque.
    int dead_frames_count_;
    absl::optional<Timestamp> last_rendered_frame_time_ = absl::nullopt;
  };

  enum State { kNew, kActive, kStopped };

  void AddComparison(absl::optional<VideoFrame> captured,
                     absl::optional<VideoFrame> rendered,
                     bool dropped,
                     FrameStats frame_stats);
  static void ProcessComparisonsThread(void* obj);
  void ProcessComparisons();
  void ProcessComparison(const FrameComparison& comparison);
  // Report results for all metrics for all streams.
  void ReportResults();
  void ReportResults(const std::string& test_case_name,
                     const StreamStats& stats,
                     const FrameCounters& frame_counters)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Report result for single metric for specified stream.
  static void ReportResult(const std::string& metric_name,
                           const std::string& test_case_name,
                           const SamplesStatsCounter& counter,
                           const std::string& unit,
                           webrtc::test::ImproveDirection improve_direction =
                               webrtc::test::ImproveDirection::kNone);
  // Returns name of current test case for reporting.
  std::string GetTestCaseName(const std::string& stream_label) const;
  Timestamp Now();

  const bool heavy_metrics_computation_enabled_;
  const int max_frames_in_flight_per_stream_count_;
  webrtc::Clock* const clock_;
  std::atomic<uint16_t> next_frame_id_{0};

  std::string test_label_;

  rtc::CriticalSection lock_;
  State state_ RTC_GUARDED_BY(lock_) = State::kNew;
  Timestamp start_time_ RTC_GUARDED_BY(lock_) = Timestamp::MinusInfinity();
  // Frames that were captured by all streams and still aren't rendered by any
  // stream or deemed dropped. Frame with id X can be removed from this map if:
  // 1. The frame with id X was received in OnFrameRendered
  // 2. The frame with id Y > X was received in OnFrameRendered
  // 3. Next available frame id for newly captured frame is X
  // 4. There too many frames in flight for current video stream and X is the
  //    oldest frame id in this stream.
  std::map<uint16_t, VideoFrame> captured_frames_in_flight_
      RTC_GUARDED_BY(lock_);
  // Global frames count for all video streams.
  FrameCounters frame_counters_ RTC_GUARDED_BY(lock_);
  // Frame counters per each stream.
  std::map<std::string, FrameCounters> stream_frame_counters_
      RTC_GUARDED_BY(lock_);
  std::map<uint16_t, FrameStats> frame_stats_ RTC_GUARDED_BY(lock_);
  std::map<std::string, StreamState> stream_states_ RTC_GUARDED_BY(lock_);

  // Stores history mapping between stream labels and frame ids. Updated when
  // frame id overlap. It required to properly return stream label after 1st
  // frame from simulcast streams was already rendered and last is still
  // encoding.
  std::map<std::string, std::set<uint16_t>> stream_to_frame_id_history_
      RTC_GUARDED_BY(lock_);

  rtc::CriticalSection comparison_lock_;
  std::map<std::string, StreamStats> stream_stats_
      RTC_GUARDED_BY(comparison_lock_);
  std::map<std::string, Timestamp> stream_last_freeze_end_time_
      RTC_GUARDED_BY(comparison_lock_);
  std::deque<FrameComparison> comparisons_ RTC_GUARDED_BY(comparison_lock_);
  AnalyzerStats analyzer_stats_ RTC_GUARDED_BY(comparison_lock_);

  std::vector<std::unique_ptr<rtc::PlatformThread>> thread_pool_;
  rtc::Event comparison_available_event_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
