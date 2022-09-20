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
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
<<<<<<< HEAD
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frame_in_flight.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frames_comparator.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_stream_state.h"
#include "test/pc/e2e/analyzer/video/names_collection.h"
=======
#include "test/pc/e2e/analyzer/video/multi_head_queue.h"
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// WebRTC will request a key frame after 3 seconds if no frames were received.
// We assume max frame rate ~60 fps, so 270 frames will cover max freeze without
// key frame request.
constexpr size_t kDefaultMaxFramesInFlightPerStream = 270;

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
  SamplesStatsCounter target_encode_bitrate;

  int64_t total_encoded_images_payload = 0;
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
  // Count of frames in flight in analyzer measured when new comparison is added
  // and after analyzer was stopped.
  SamplesStatsCounter frames_in_flight_left_count;
};

struct StatsKey {
  StatsKey(std::string stream_label, std::string sender, std::string receiver)
      : stream_label(std::move(stream_label)),
        sender(std::move(sender)),
        receiver(std::move(receiver)) {}

  std::string ToString() const;

  // Label of video stream to which stats belongs to.
  std::string stream_label;
  // Name of the peer which send this stream.
  std::string sender;
  // Name of the peer on which stream was received.
  std::string receiver;
};

// Required to use StatsKey as std::map key.
bool operator<(const StatsKey& a, const StatsKey& b);
bool operator==(const StatsKey& a, const StatsKey& b);

struct InternalStatsKey {
  InternalStatsKey(size_t stream, size_t sender, size_t receiver)
      : stream(stream), sender(sender), receiver(receiver) {}

  std::string ToString() const;

  size_t stream;
  size_t sender;
  size_t receiver;
};

// Required to use InternalStatsKey as std::map key.
bool operator<(const InternalStatsKey& a, const InternalStatsKey& b);
bool operator==(const InternalStatsKey& a, const InternalStatsKey& b);

struct DefaultVideoQualityAnalyzerOptions {
  // Tells DefaultVideoQualityAnalyzer if heavy metrics like PSNR and SSIM have
  // to be computed or not.
  bool heavy_metrics_computation_enabled = true;
  // If true DefaultVideoQualityAnalyzer will try to adjust frames before
  // computing PSNR and SSIM for them. In some cases picture may be shifted by
  // a few pixels after the encode/decode step. Those difference is invisible
  // for a human eye, but it affects the metrics. So the adjustment is used to
  // get metrics that are closer to how human persepts the video. This feature
  // significantly slows down the comparison, so turn it on only when it is
  // needed.
  bool adjust_cropping_before_comparing_frames = false;
  // Amount of frames that are queued in the DefaultVideoQualityAnalyzer from
  // the point they were captured to the point they were rendered on all
  // receivers per stream.
  size_t max_frames_in_flight_per_stream_count =
      kDefaultMaxFramesInFlightPerStream;
};

class DefaultVideoQualityAnalyzer : public VideoQualityAnalyzerInterface {
 public:
  explicit DefaultVideoQualityAnalyzer(
      webrtc::Clock* clock,
      DefaultVideoQualityAnalyzerOptions options =
          DefaultVideoQualityAnalyzerOptions());
  ~DefaultVideoQualityAnalyzer() override;

  void Start(std::string test_case_name,
             rtc::ArrayView<const std::string> peer_names,
             int max_threads_count) override;
  uint16_t OnFrameCaptured(absl::string_view peer_name,
                           const std::string& stream_label,
                           const VideoFrame& frame) override;
  void OnFramePreEncode(absl::string_view peer_name,
                        const VideoFrame& frame) override;
  void OnFrameEncoded(absl::string_view peer_name,
                      uint16_t frame_id,
                      const EncodedImage& encoded_image,
                      const EncoderStats& stats) override;
  void OnFrameDropped(absl::string_view peer_name,
                      EncodedImageCallback::DropReason reason) override;
  void OnFramePreDecode(absl::string_view peer_name,
                        uint16_t frame_id,
                        const EncodedImage& input_image) override;
  void OnFrameDecoded(absl::string_view peer_name,
                      const VideoFrame& frame,
                      const DecoderStats& stats) override;
  void OnFrameRendered(absl::string_view peer_name,
                       const VideoFrame& frame) override;
  void OnEncoderError(absl::string_view peer_name,
                      const VideoFrame& frame,
                      int32_t error_code) override;
  void OnDecoderError(absl::string_view peer_name,
                      uint16_t frame_id,
                      int32_t error_code) override;

  void RegisterParticipantInCall(absl::string_view peer_name) override;
  void UnregisterParticipantInCall(absl::string_view peer_name) override;

  void Stop() override;
  std::string GetStreamLabel(uint16_t frame_id) override;
  void OnStatsReports(
      absl::string_view pc_label,
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {}

  // Returns set of stream labels, that were met during test call.
  std::set<StatsKey> GetKnownVideoStreams() const;
  VideoStreamsInfo GetKnownStreams() const;
  FrameCounters GetGlobalCounters() const;
  // Returns frame counter for frames received without frame id set.
  std::map<std::string, FrameCounters> GetUnknownSenderFrameCounters() const;
  // Returns frame counter per stream label. Valid stream labels can be obtained
  // by calling GetKnownVideoStreams()
  std::map<StatsKey, FrameCounters> GetPerStreamCounters() const;
  // Returns video quality stats per stream label. Valid stream labels can be
  // obtained by calling GetKnownVideoStreams()
  std::map<StatsKey, StreamStats> GetStats() const;
  AnalyzerStats GetAnalyzerStats() const;
  double GetCpuUsagePercent();

<<<<<<< HEAD
  // Returns mapping from the stream label to the history of frames that were
  // met in this stream in the order as they were captured.
  std::map<std::string, std::vector<uint16_t>> GetStreamFrames() const;
=======
 private:
  struct FrameStats {
    FrameStats(Timestamp captured_time) : captured_time(captured_time) {}

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

    int64_t encoded_image_size = 0;
    uint32_t target_encode_bitrate = 0;

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
    FrameComparison(InternalStatsKey stats_key,
                    absl::optional<VideoFrame> captured,
                    absl::optional<VideoFrame> rendered,
                    bool dropped,
                    FrameStats frame_stats,
                    OverloadReason overload_reason);

    InternalStatsKey stats_key;
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
    StreamState(size_t owner, size_t peers_count)
        : owner_(owner), frame_ids_(peers_count) {}

    size_t owner() const { return owner_; }

    void PushBack(uint16_t frame_id) { frame_ids_.PushBack(frame_id); }
    // Crash if state is empty. Guarantees that there can be no alive frames
    // that are not in the owner queue
    uint16_t PopFront(size_t peer);
    bool IsEmpty(size_t peer) const { return frame_ids_.IsEmpty(peer); }
    // Crash if state is empty.
    uint16_t Front(size_t peer) const { return frame_ids_.Front(peer).value(); }

    // When new peer is added - all current alive frames will be sent to it as
    // well. So we need to register them as expected by copying owner_ head to
    // the new head.
    void AddPeer() { frame_ids_.AddHead(owner_); }

    size_t GetAliveFramesCount() { return frame_ids_.size(owner_); }
    uint16_t MarkNextAliveFrameAsDead();

    void SetLastRenderedFrameTime(size_t peer, Timestamp time);
    absl::optional<Timestamp> last_rendered_frame_time(size_t peer) const;

   private:
    // Index of the owner. Owner's queue in |frame_ids_| will keep alive frames.
    const size_t owner_;
    // To correctly determine dropped frames we have to know sequence of frames
    // in each stream so we will keep a list of frame ids inside the stream.
    // This list is represented by multi head queue of frame ids with separate
    // head for each receiver. When the frame is rendered, we will pop ids from
    // the corresponding head until id will match with rendered one. All ids
    // before matched one can be considered as dropped:
    //
    // | frame_id1 |->| frame_id2 |->| frame_id3 |->| frame_id4 |
    //
    // If we received frame with id frame_id3, then we will pop frame_id1 and
    // frame_id2 and consider that frames as dropped and then compare received
    // frame with the one from |captured_frames_in_flight_| with id frame_id3.
    //
    // To track alive frames (frames that contains frame's payload in
    // |captured_frames_in_flight_|) the head which corresponds to |owner_| will
    // be used. So that head will point to the first alive frame in frames list.
    MultiHeadQueue<uint16_t> frame_ids_;
    std::map<size_t, Timestamp> last_rendered_frame_time_;
  };
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)

 private:
  enum State { kNew, kActive, kStopped };

<<<<<<< HEAD
  // Returns next frame id to use. Frame ID can't be `VideoFrame::kNotSetId`,
  // because this value is reserved by `VideoFrame` as "ID not set".
  uint16_t GetNextFrameId() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
=======
  struct ReceiverFrameStats {
    // Time when last packet of a frame was received.
    Timestamp received_time = Timestamp::MinusInfinity();
    Timestamp decode_start_time = Timestamp::MinusInfinity();
    Timestamp decode_end_time = Timestamp::MinusInfinity();
    Timestamp rendered_time = Timestamp::MinusInfinity();
    Timestamp prev_frame_rendered_time = Timestamp::MinusInfinity();

    absl::optional<int> rendered_frame_width = absl::nullopt;
    absl::optional<int> rendered_frame_height = absl::nullopt;

    bool dropped = false;
  };

  class FrameInFlight {
   public:
    FrameInFlight(size_t stream,
                  VideoFrame frame,
                  Timestamp captured_time,
                  size_t owner,
                  size_t peers_count)
        : stream_(stream),
          owner_(owner),
          peers_count_(peers_count),
          frame_(std::move(frame)),
          captured_time_(captured_time) {}

    size_t stream() const { return stream_; }
    const absl::optional<VideoFrame>& frame() const { return frame_; }
    // Returns was frame removed or not.
    bool RemoveFrame();
    void SetFrameId(uint16_t id);

    void AddPeer() { ++peers_count_; }

    std::vector<size_t> GetPeersWhichDidntReceive() const;
    bool HaveAllPeersReceived() const;

    void SetPreEncodeTime(webrtc::Timestamp time) { pre_encode_time_ = time; }

    void OnFrameEncoded(webrtc::Timestamp time,
                        int64_t encoded_image_size,
                        uint32_t target_encode_bitrate);

    bool HasEncodedTime() const { return encoded_time_.IsFinite(); }

    void OnFramePreDecode(size_t peer,
                          webrtc::Timestamp received_time,
                          webrtc::Timestamp decode_start_time);

    bool HasReceivedTime(size_t peer) const;

    void SetDecodeEndTime(size_t peer, webrtc::Timestamp time) {
      receiver_stats_[peer].decode_end_time = time;
    }

    bool HasDecodeEndTime(size_t peer) const;

    void OnFrameRendered(size_t peer,
                         webrtc::Timestamp time,
                         int width,
                         int height);

    bool HasRenderedTime(size_t peer) const;

    // Crash if rendered time is not set for specified |peer|.
    webrtc::Timestamp rendered_time(size_t peer) const {
      return receiver_stats_.at(peer).rendered_time;
    }

    void MarkDropped(size_t peer) { receiver_stats_[peer].dropped = true; }

    void SetPrevFrameRenderedTime(size_t peer, webrtc::Timestamp time) {
      receiver_stats_[peer].prev_frame_rendered_time = time;
    }

    FrameStats GetStatsForPeer(size_t peer) const;

   private:
    const size_t stream_;
    const size_t owner_;
    size_t peers_count_;
    absl::optional<VideoFrame> frame_;

    // Frame events timestamp.
    Timestamp captured_time_;
    Timestamp pre_encode_time_ = Timestamp::MinusInfinity();
    Timestamp encoded_time_ = Timestamp::MinusInfinity();
    int64_t encoded_image_size_ = 0;
    uint32_t target_encode_bitrate_ = 0;
    std::map<size_t, ReceiverFrameStats> receiver_stats_;
  };

  class NamesCollection {
   public:
    NamesCollection() = default;
    explicit NamesCollection(rtc::ArrayView<const std::string> names) {
      names_ = std::vector<std::string>(names.begin(), names.end());
      for (size_t i = 0; i < names_.size(); ++i) {
        index_.emplace(names_[i], i);
      }
    }

    size_t size() const { return names_.size(); }

    size_t index(absl::string_view name) const { return index_.at(name); }

    const std::string& name(size_t index) const { return names_[index]; }

    bool HasName(absl::string_view name) const {
      return index_.find(name) != index_.end();
    }

    // Add specified |name| to the collection if it isn't presented.
    // Returns index which corresponds to specified |name|.
    size_t AddIfAbsent(absl::string_view name);

   private:
    std::vector<std::string> names_;
    std::map<absl::string_view, size_t> index_;
  };
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)

  void AddComparison(InternalStatsKey stats_key,
                     absl::optional<VideoFrame> captured,
                     absl::optional<VideoFrame> rendered,
                     bool dropped,
                     FrameStats frame_stats)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(comparison_lock_);
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
  StatsKey ToStatsKey(const InternalStatsKey& key) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Returns string representation of stats key for metrics naming. Used for
  // backward compatibility by metrics naming for 2 peers cases.
<<<<<<< HEAD
  std::string ToMetricName(const InternalStatsKey& key) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  static const uint16_t kStartingFrameId = 1;

  const DefaultVideoQualityAnalyzerOptions options_;
=======
  std::string StatsKeyToMetricName(const StatsKey& key) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void StartMeasuringCpuProcessTime();
  void StopMeasuringCpuProcessTime();
  void StartExcludingCpuThreadTime();
  void StopExcludingCpuThreadTime();

  // TODO(titovartem) restore const when old constructor will be removed.
  DefaultVideoQualityAnalyzerOptions options_;
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
  webrtc::Clock* const clock_;

  std::string test_label_;

<<<<<<< HEAD
  mutable Mutex mutex_;
  uint16_t next_frame_id_ RTC_GUARDED_BY(mutex_) = kStartingFrameId;
  std::unique_ptr<NamesCollection> peers_ RTC_GUARDED_BY(mutex_);
  State state_ RTC_GUARDED_BY(mutex_) = State::kNew;
  Timestamp start_time_ RTC_GUARDED_BY(mutex_) = Timestamp::MinusInfinity();
=======
  mutable Mutex lock_;
  std::unique_ptr<NamesCollection> peers_ RTC_GUARDED_BY(lock_);
  State state_ RTC_GUARDED_BY(lock_) = State::kNew;
  Timestamp start_time_ RTC_GUARDED_BY(lock_) = Timestamp::MinusInfinity();
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
  // Mapping from stream label to unique size_t value to use in stats and avoid
  // extra string copying.
  NamesCollection streams_ RTC_GUARDED_BY(lock_);
  // Frames that were captured by all streams and still aren't rendered by any
  // stream or deemed dropped. Frame with id X can be removed from this map if:
  // 1. The frame with id X was received in OnFrameRendered
  // 2. The frame with id Y > X was received in OnFrameRendered
  // 3. Next available frame id for newly captured frame is X
  // 4. There too many frames in flight for current video stream and X is the
  //    oldest frame id in this stream.
  std::map<uint16_t, FrameInFlight> captured_frames_in_flight_
      RTC_GUARDED_BY(lock_);
  // Global frames count for all video streams.
<<<<<<< HEAD
  FrameCounters frame_counters_ RTC_GUARDED_BY(mutex_);
  // Frame counters for received frames without video frame id set.
  // Map from peer name to the frame counters.
  std::map<std::string, FrameCounters> unknown_sender_frame_counters_
      RTC_GUARDED_BY(mutex_);
=======
  FrameCounters frame_counters_ RTC_GUARDED_BY(lock_);
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
  // Frame counters per each stream per each receiver.
  std::map<InternalStatsKey, FrameCounters> stream_frame_counters_
      RTC_GUARDED_BY(lock_);
  // Map from stream index in |streams_| to its StreamState.
  std::map<size_t, StreamState> stream_states_ RTC_GUARDED_BY(lock_);
  // Map from stream index in |streams_| to sender peer index in |peers_|.
  std::map<size_t, size_t> stream_to_sender_ RTC_GUARDED_BY(lock_);

  // Stores history mapping between stream index in |streams_| and frame ids.
  // Updated when frame id overlap. It required to properly return stream label
  // after 1st frame from simulcast streams was already rendered and last is
  // still encoding.
  std::map<size_t, std::set<uint16_t>> stream_to_frame_id_history_
<<<<<<< HEAD
      RTC_GUARDED_BY(mutex_);
  // Map from stream index to the list of frames as they were met in the stream.
  std::map<size_t, std::vector<uint16_t>> stream_to_frame_id_full_history_
      RTC_GUARDED_BY(mutex_);
  AnalyzerStats analyzer_stats_ RTC_GUARDED_BY(mutex_);

  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer_;
  DefaultVideoQualityAnalyzerFramesComparator frames_comparator_;
=======
      RTC_GUARDED_BY(lock_);

  mutable Mutex comparison_lock_;
  std::map<InternalStatsKey, StreamStats> stream_stats_
      RTC_GUARDED_BY(comparison_lock_);
  std::map<InternalStatsKey, Timestamp> stream_last_freeze_end_time_
      RTC_GUARDED_BY(comparison_lock_);
  std::deque<FrameComparison> comparisons_ RTC_GUARDED_BY(comparison_lock_);
  AnalyzerStats analyzer_stats_ RTC_GUARDED_BY(comparison_lock_);

  std::vector<rtc::PlatformThread> thread_pool_;
  rtc::Event comparison_available_event_;

  Mutex cpu_measurement_lock_;
  int64_t cpu_time_ RTC_GUARDED_BY(cpu_measurement_lock_) = 0;
  int64_t wallclock_time_ RTC_GUARDED_BY(cpu_measurement_lock_) = 0;
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
