/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_SHARED_OBJECTS_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_SHARED_OBJECTS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/timestamp.h"

namespace webrtc {

// WebRTC will request a key frame after 3 seconds if no frames were received.
// We assume max frame rate ~60 fps, so 270 frames will cover max freeze without
// key frame request.
constexpr size_t kDefaultMaxFramesInFlightPerStream = 270;

class SamplesRateCounter {
 public:
  void AddEvent(Timestamp event_time);

  bool IsEmpty() const { return event_first_time_ == event_last_time_; }

  double GetEventsPerSecond() const;

 private:
  Timestamp event_first_time_ = Timestamp::MinusInfinity();
  Timestamp event_last_time_ = Timestamp::MinusInfinity();
  int64_t events_count_ = 0;
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

// Contains information about the codec that was used for encoding or decoding
// the stream.
struct StreamCodecInfo {
  // Codec implementation name.
  std::string codec_name;
  // Id of the first frame for which this codec was used.
  uint16_t first_frame_id;
  // Id of the last frame for which this codec was used.
  uint16_t last_frame_id;
  // Timestamp when the first frame was handled by the encode/decoder.
  Timestamp switched_on_at = Timestamp::PlusInfinity();
  // Timestamp when this codec was used last time.
  Timestamp switched_from_at = Timestamp::PlusInfinity();
};

// Represents phases where video frame can be dropped and such drop will be
// detected by analyzer.
enum class FrameDropPhase : int {
  kBeforeEncoder,
  kByEncoder,
  kTransport,
  kAfterDecoder,
  // kLastValue must be the last value in this enumeration.
  kLastValue
};

struct StreamStats {
  explicit StreamStats(Timestamp stream_started_time);

  // The time when the first frame of this stream was captured.
  Timestamp stream_started_time;

  // Spatial quality metrics.
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
  SamplesRateCounter encode_frame_rate;
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
  // Counters on which phase how many frames were dropped.
  std::map<FrameDropPhase, int64_t> dropped_by_phase;

  // Frame count metrics.
  int64_t num_send_key_frames = 0;
  int64_t num_recv_key_frames = 0;

  // Encoded frame size (in bytes) metrics.
  SamplesStatsCounter recv_key_frame_size_bytes;
  SamplesStatsCounter recv_delta_frame_size_bytes;

  // Vector of encoders used for this stream by sending client.
  std::vector<StreamCodecInfo> encoders;
  // Vectors of decoders used for this stream by receiving client.
  std::vector<StreamCodecInfo> decoders;
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
  StatsKey(std::string stream_label, std::string receiver)
      : stream_label(std::move(stream_label)), receiver(std::move(receiver)) {}

  std::string ToString() const;

  // Label of video stream to which stats belongs to.
  std::string stream_label;
  // Name of the peer on which stream was received.
  std::string receiver;
};

// Required to use StatsKey as std::map key.
bool operator<(const StatsKey& a, const StatsKey& b);
bool operator==(const StatsKey& a, const StatsKey& b);

// Contains all metadata related to the video streams that were seen by the
// video analyzer.
class VideoStreamsInfo {
 public:
  std::set<StatsKey> GetStatsKeys() const;

  // Returns all stream labels that are known to the video analyzer.
  std::set<std::string> GetStreams() const;

  // Returns set of the stream for specified `sender_name`. If sender didn't
  // send any streams or `sender_name` isn't known to the video analyzer
  // empty set will be returned.
  std::set<std::string> GetStreams(absl::string_view sender_name) const;

  // Returns sender name for specified `stream_label`. Returns `absl::nullopt`
  // if provided `stream_label` isn't known to the video analyzer.
  absl::optional<std::string> GetSender(absl::string_view stream_label) const;

  // Returns set of the receivers for specified `stream_label`. If stream wasn't
  // received by any peer or `stream_label` isn't known to the video analyzer
  // empty set will be returned.
  std::set<std::string> GetReceivers(absl::string_view stream_label) const;

 protected:
  friend class DefaultVideoQualityAnalyzer;
  VideoStreamsInfo(
      std::map<std::string, std::string> stream_to_sender,
      std::map<std::string, std::set<std::string>> sender_to_streams,
      std::map<std::string, std::set<std::string>> stream_to_receivers);

 private:
  std::map<std::string, std::string> stream_to_sender_;
  std::map<std::string, std::set<std::string>> sender_to_streams_;
  std::map<std::string, std::set<std::string>> stream_to_receivers_;
};

struct DefaultVideoQualityAnalyzerOptions {
  // Tells DefaultVideoQualityAnalyzer if heavy metrics have to be computed.
  bool compute_psnr = true;
  bool compute_ssim = true;
  // If true, weights the luma plane more than the chroma planes in the PSNR.
  bool use_weighted_psnr = false;
  // Tells DefaultVideoQualityAnalyzer if detailed frame stats should be
  // reported.
  bool report_detailed_frame_stats = false;
  // If true DefaultVideoQualityAnalyzer will try to adjust frames before
  // computing PSNR and SSIM for them. In some cases picture may be shifted by
  // a few pixels after the encode/decode step. Those difference is invisible
  // for a human eye, but it affects the metrics. So the adjustment is used to
  // get metrics that are closer to how human perceive the video. This feature
  // significantly slows down the comparison, so turn it on only when it is
  // needed.
  bool adjust_cropping_before_comparing_frames = false;
  // Amount of frames that are queued in the DefaultVideoQualityAnalyzer from
  // the point they were captured to the point they were rendered on all
  // receivers per stream.
  size_t max_frames_in_flight_per_stream_count =
      kDefaultMaxFramesInFlightPerStream;
  // If true, the analyzer will expect peers to receive their own video streams.
  bool enable_receive_own_stream = false;
};

}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_SHARED_OBJECTS_H_
