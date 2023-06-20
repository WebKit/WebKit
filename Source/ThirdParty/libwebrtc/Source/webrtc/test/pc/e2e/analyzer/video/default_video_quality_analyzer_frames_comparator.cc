/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frames_comparator.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_type.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"
#include "test/pc/e2e/metric_metadata_keys.h"

namespace webrtc {
namespace {

using ::webrtc::webrtc_pc_e2e::SampleMetadataKey;

constexpr TimeDelta kFreezeThreshold = TimeDelta::Millis(150);
constexpr int kMaxActiveComparisons = 10;
constexpr int kMillisInSecond = 1000;

SamplesStatsCounter::StatsSample StatsSample(
    double value,
    Timestamp sampling_time,
    std::map<std::string, std::string> metadata) {
  return SamplesStatsCounter::StatsSample{value, sampling_time,
                                          std::move(metadata)};
}

SamplesStatsCounter::StatsSample StatsSample(
    TimeDelta duration,
    Timestamp sampling_time,
    std::map<std::string, std::string> metadata) {
  return SamplesStatsCounter::StatsSample{duration.ms<double>(), sampling_time,
                                          std::move(metadata)};
}

FrameComparison ValidateFrameComparison(FrameComparison comparison) {
  RTC_DCHECK(comparison.frame_stats.captured_time.IsFinite())
      << "Any comparison has to have finite captured_time";
  switch (comparison.type) {
    case FrameComparisonType::kRegular:
      // Regular comparison has to have all FrameStats filled in.
      RTC_DCHECK(comparison.captured.has_value() ||
                 comparison.overload_reason != OverloadReason::kNone)
          << "Regular comparison has to have captured frame if it's not "
          << "overloaded comparison";
      RTC_DCHECK(comparison.rendered.has_value() ||
                 comparison.overload_reason != OverloadReason::kNone)
          << "rendered frame has to be presented if it's not overloaded "
          << "comparison";
      RTC_DCHECK(comparison.frame_stats.pre_encode_time.IsFinite())
          << "Regular comparison has to have finite pre_encode_time";
      RTC_DCHECK(comparison.frame_stats.encoded_time.IsFinite())
          << "Regular comparison has to have finite encoded_time";
      RTC_DCHECK(comparison.frame_stats.received_time.IsFinite())
          << "Regular comparison has to have finite received_time";
      RTC_DCHECK(comparison.frame_stats.decode_start_time.IsFinite())
          << "Regular comparison has to have finite decode_start_time";
      RTC_DCHECK(comparison.frame_stats.decode_end_time.IsFinite())
          << "Regular comparison has to have finite decode_end_time";
      RTC_DCHECK(comparison.frame_stats.rendered_time.IsFinite())
          << "Regular comparison has to have finite rendered_time";
      RTC_DCHECK(comparison.frame_stats.decoded_frame_width.has_value())
          << "Regular comparison has to have decoded_frame_width";
      RTC_DCHECK(comparison.frame_stats.decoded_frame_height.has_value())
          << "Regular comparison has to have decoded_frame_height";
      RTC_DCHECK(comparison.frame_stats.used_encoder.has_value())
          << "Regular comparison has to have used_encoder";
      RTC_DCHECK(comparison.frame_stats.used_decoder.has_value())
          << "Regular comparison has to have used_decoder";
      RTC_DCHECK(!comparison.frame_stats.decoder_failed)
          << "Regular comparison can't have decoder failure";
      break;
    case FrameComparisonType::kDroppedFrame:
      // Frame can be dropped before encoder, by encoder, inside network or
      // after decoder.
      RTC_DCHECK(!comparison.captured.has_value())
          << "Dropped frame comparison can't have captured frame";
      RTC_DCHECK(!comparison.rendered.has_value())
          << "Dropped frame comparison can't have rendered frame";

      if (comparison.frame_stats.encoded_time.IsFinite()) {
        RTC_DCHECK(comparison.frame_stats.used_encoder.has_value())
            << "Dropped frame comparison has to have used_encoder when "
            << "encoded_time is set";
        RTC_DCHECK(comparison.frame_stats.pre_encode_time.IsFinite())
            << "Dropped frame comparison has to have finite pre_encode_time "
            << "when encoded_time is finite.";
      }

      if (comparison.frame_stats.decode_end_time.IsFinite() ||
          comparison.frame_stats.decoder_failed) {
        RTC_DCHECK(comparison.frame_stats.received_time.IsFinite())
            << "Dropped frame comparison has to have received_time when "
            << "decode_end_time is set or decoder_failed is true";
        RTC_DCHECK(comparison.frame_stats.decode_start_time.IsFinite())
            << "Dropped frame comparison has to have decode_start_time when "
            << "decode_end_time is set or decoder_failed is true";
        RTC_DCHECK(comparison.frame_stats.used_decoder.has_value())
            << "Dropped frame comparison has to have used_decoder when "
            << "decode_end_time is set or decoder_failed is true";
      } else if (comparison.frame_stats.decode_end_time.IsFinite()) {
        RTC_DCHECK(comparison.frame_stats.decoded_frame_width.has_value())
            << "Dropped frame comparison has to have decoded_frame_width when "
            << "decode_end_time is set";
        RTC_DCHECK(comparison.frame_stats.decoded_frame_height.has_value())
            << "Dropped frame comparison has to have decoded_frame_height when "
            << "decode_end_time is set";
      }
      RTC_DCHECK(!comparison.frame_stats.rendered_time.IsFinite())
          << "Dropped frame comparison can't have rendered_time";
      break;
    case FrameComparisonType::kFrameInFlight:
      // Frame in flight comparison may miss almost any FrameStats, but if
      // stats for stage X are set, then stats for stage X - 1 also has to be
      // set. Also these frames were never rendered.
      RTC_DCHECK(!comparison.captured.has_value())
          << "Frame in flight comparison can't have captured frame";
      RTC_DCHECK(!comparison.rendered.has_value())
          << "Frame in flight comparison can't have rendered frame";
      RTC_DCHECK(!comparison.frame_stats.rendered_time.IsFinite())
          << "Frame in flight comparison can't have rendered_time";

      if (comparison.frame_stats.decode_end_time.IsFinite() ||
          comparison.frame_stats.decoder_failed) {
        RTC_DCHECK(comparison.frame_stats.used_decoder.has_value())
            << "Frame in flight comparison has to have used_decoder when "
            << "decode_end_time is set or decoder_failed is true.";
        RTC_DCHECK(comparison.frame_stats.decode_start_time.IsFinite())
            << "Frame in flight comparison has to have finite "
            << "decode_start_time when decode_end_time is finite or "
            << "decoder_failed is true.";
      }
      if (comparison.frame_stats.decode_end_time.IsFinite()) {
        RTC_DCHECK(comparison.frame_stats.decoded_frame_width.has_value())
            << "Frame in flight comparison has to have decoded_frame_width "
            << "when decode_end_time is set.";
        RTC_DCHECK(comparison.frame_stats.decoded_frame_height.has_value())
            << "Frame in flight comparison has to have decoded_frame_height "
            << "when decode_end_time is set.";
      }
      if (comparison.frame_stats.decode_start_time.IsFinite()) {
        RTC_DCHECK(comparison.frame_stats.received_time.IsFinite())
            << "Frame in flight comparison has to have finite received_time "
            << "when decode_start_time is finite.";
      }
      if (comparison.frame_stats.received_time.IsFinite()) {
        RTC_DCHECK(comparison.frame_stats.encoded_time.IsFinite())
            << "Frame in flight comparison has to have finite encoded_time "
            << "when received_time is finite.";
      }
      if (comparison.frame_stats.encoded_time.IsFinite()) {
        RTC_DCHECK(comparison.frame_stats.used_encoder.has_value())
            << "Frame in flight comparison has to have used_encoder when "
            << "encoded_time is set";
        RTC_DCHECK(comparison.frame_stats.pre_encode_time.IsFinite())
            << "Frame in flight comparison has to have finite pre_encode_time "
            << "when encoded_time is finite.";
      }
      break;
  }
  return comparison;
}

}  // namespace

void DefaultVideoQualityAnalyzerFramesComparator::Start(int max_threads_count) {
  for (int i = 0; i < max_threads_count; i++) {
    thread_pool_.push_back(rtc::PlatformThread::SpawnJoinable(
        [this] { ProcessComparisons(); },
        "DefaultVideoQualityAnalyzerFramesComparator-" + std::to_string(i)));
  }
  {
    MutexLock lock(&mutex_);
    RTC_CHECK_EQ(state_, State::kNew) << "Frames comparator is already started";
    state_ = State::kActive;
  }
  cpu_measurer_.StartMeasuringCpuProcessTime();
}

void DefaultVideoQualityAnalyzerFramesComparator::Stop(
    const std::map<InternalStatsKey, Timestamp>& last_rendered_frame_times) {
  {
    MutexLock lock(&mutex_);
    if (state_ == State::kStopped) {
      return;
    }
    RTC_CHECK_EQ(state_, State::kActive)
        << "Frames comparator has to be started before it will be used";
    state_ = State::kStopped;
  }
  cpu_measurer_.StopMeasuringCpuProcessTime();
  comparison_available_event_.Set();
  thread_pool_.clear();

  {
    MutexLock lock(&mutex_);
    // Perform final Metrics update. On this place analyzer is stopped and no
    // one holds any locks.

    // Time between freezes.
    // Count time since the last freeze to the end of the call as time
    // between freezes.
    for (auto& entry : last_rendered_frame_times) {
      const InternalStatsKey& stats_key = entry.first;
      const Timestamp& last_rendered_frame_time = entry.second;

      // If there are no freezes in the call we have to report
      // time_between_freezes_ms as call duration and in such case
      // `last_rendered_frame_time` for this stream will be stream start time.
      // If there is freeze, then we need add time from last rendered frame
      // to last freeze end as time between freezes.
      stream_stats_.at(stats_key).time_between_freezes_ms.AddSample(StatsSample(
          last_rendered_frame_time - stream_last_freeze_end_time_.at(stats_key),
          Now(), /*metadata=*/{}));
    }

    for (auto& [key, stream_stats] : stream_stats_) {
      // Freeze Time:
      // If there were no freezes on a video stream, add only one sample with
      // value 0 (0ms freezes time).
      if (stream_stats.freeze_time_ms.IsEmpty()) {
        stream_stats.freeze_time_ms.AddSample(0);
      }

      // Harmonic framerate (fps):
      // sum of interframe delays / squared sum of interframe delays.
      // The metric aims to reflect overall experience related to smoothness of
      // video playback and includes both freezes and pauses.
      double sum_squared_interframe_delays_secs = 0;
      double sum_interframe_delays_ms = 0;
      for (const SamplesStatsCounter::StatsSample& sample :
           stream_stats.time_between_rendered_frames_ms.GetTimedSamples()) {
        double interframe_delay_ms = sample.value;
        const double interframe_delays_secs =
            interframe_delay_ms / static_cast<double>(kMillisInSecond);
        sum_squared_interframe_delays_secs +=
            interframe_delays_secs * interframe_delays_secs;

        sum_interframe_delays_ms += interframe_delay_ms;
      }
      if (sum_squared_interframe_delays_secs > 0.0) {
        stream_stats.harmonic_framerate_fps =
            sum_interframe_delays_ms / static_cast<double>(kMillisInSecond) /
            sum_squared_interframe_delays_secs;
      }
    }
  }
}

void DefaultVideoQualityAnalyzerFramesComparator::EnsureStatsForStream(
    size_t stream_index,
    size_t sender_peer_index,
    size_t peers_count,
    Timestamp captured_time,
    Timestamp start_time) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "Frames comparator has to be started before it will be used";

  for (size_t i = 0; i < peers_count; ++i) {
    if (i == sender_peer_index && !options_.enable_receive_own_stream) {
      continue;
    }
    InternalStatsKey stats_key(stream_index, sender_peer_index, i);
    if (stream_stats_.find(stats_key) == stream_stats_.end()) {
      stream_stats_.insert({stats_key, StreamStats(captured_time)});
      // Assume that the first freeze was before first stream frame captured.
      // This way time before the first freeze would be counted as time
      // between freezes.
      stream_last_freeze_end_time_.insert({stats_key, start_time});
    } else {
      // When we see some `stream_label` for the first time we need to create
      // stream stats object for it and set up some states, but we need to do
      // it only once and for all receivers, so on the next frame on the same
      // `stream_label` we can be sure, that it's already done and we needn't
      // to scan though all peers again.
      break;
    }
  }
}

void DefaultVideoQualityAnalyzerFramesComparator::RegisterParticipantInCall(
    rtc::ArrayView<std::pair<InternalStatsKey, Timestamp>> stream_started_time,
    Timestamp start_time) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "Frames comparator has to be started before it will be used";

  for (const std::pair<InternalStatsKey, Timestamp>& pair :
       stream_started_time) {
    stream_stats_.insert({pair.first, StreamStats(pair.second)});
    stream_last_freeze_end_time_.insert({pair.first, start_time});
  }
}

void DefaultVideoQualityAnalyzerFramesComparator::AddComparison(
    InternalStatsKey stats_key,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    FrameComparisonType type,
    FrameStats frame_stats) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "Frames comparator has to be started before it will be used";
  AddComparisonInternal(std::move(stats_key), std::move(captured),
                        std::move(rendered), type, std::move(frame_stats));
}

void DefaultVideoQualityAnalyzerFramesComparator::AddComparison(
    InternalStatsKey stats_key,
    int skipped_between_rendered,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    FrameComparisonType type,
    FrameStats frame_stats) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "Frames comparator has to be started before it will be used";
  stream_stats_.at(stats_key).skipped_between_rendered.AddSample(
      StatsSample(skipped_between_rendered, Now(),
                  /*metadata=*/
                  {{SampleMetadataKey::kFrameIdMetadataKey,
                    std::to_string(frame_stats.frame_id)}}));
  AddComparisonInternal(std::move(stats_key), std::move(captured),
                        std::move(rendered), type, std::move(frame_stats));
}

void DefaultVideoQualityAnalyzerFramesComparator::AddComparisonInternal(
    InternalStatsKey stats_key,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    FrameComparisonType type,
    FrameStats frame_stats) {
  cpu_measurer_.StartExcludingCpuThreadTime();
  frames_comparator_stats_.comparisons_queue_size.AddSample(
      StatsSample(comparisons_.size(), Now(), /*metadata=*/{}));
  // If there too many computations waiting in the queue, we won't provide
  // frames itself to make future computations lighter.
  if (comparisons_.size() >= kMaxActiveComparisons) {
    comparisons_.emplace_back(ValidateFrameComparison(
        FrameComparison(std::move(stats_key), /*captured=*/absl::nullopt,
                        /*rendered=*/absl::nullopt, type,
                        std::move(frame_stats), OverloadReason::kCpu)));
  } else {
    OverloadReason overload_reason = OverloadReason::kNone;
    if (!captured && type == FrameComparisonType::kRegular) {
      overload_reason = OverloadReason::kMemory;
    }
    comparisons_.emplace_back(ValidateFrameComparison(FrameComparison(
        std::move(stats_key), std::move(captured), std::move(rendered), type,
        std::move(frame_stats), overload_reason)));
  }
  comparison_available_event_.Set();
  cpu_measurer_.StopExcludingCpuThreadTime();
}

void DefaultVideoQualityAnalyzerFramesComparator::ProcessComparisons() {
  while (true) {
    // Try to pick next comparison to perform from the queue.
    absl::optional<FrameComparison> comparison = absl::nullopt;
    bool more_new_comparisons_expected;
    {
      MutexLock lock(&mutex_);
      if (!comparisons_.empty()) {
        comparison = comparisons_.front();
        comparisons_.pop_front();
        if (!comparisons_.empty()) {
          comparison_available_event_.Set();
        }
      }
      // If state is stopped => no new frame comparisons are expected.
      more_new_comparisons_expected = state_ != State::kStopped;
    }
    if (!comparison) {
      if (!more_new_comparisons_expected) {
        comparison_available_event_.Set();
        return;
      }
      comparison_available_event_.Wait(TimeDelta::Seconds(1));
      continue;
    }

    cpu_measurer_.StartExcludingCpuThreadTime();
    ProcessComparison(comparison.value());
    cpu_measurer_.StopExcludingCpuThreadTime();
  }
}

void DefaultVideoQualityAnalyzerFramesComparator::ProcessComparison(
    const FrameComparison& comparison) {
  // Comparison is checked to be valid before adding, so we can use this
  // assumptions during computations.

  // Perform expensive psnr and ssim calculations while not holding lock.
  double psnr = -1.0;
  double ssim = -1.0;
  if ((options_.compute_psnr || options_.compute_ssim) &&
      comparison.captured.has_value() && comparison.rendered.has_value()) {
    rtc::scoped_refptr<I420BufferInterface> reference_buffer =
        comparison.captured->video_frame_buffer()->ToI420();
    rtc::scoped_refptr<I420BufferInterface> test_buffer =
        comparison.rendered->video_frame_buffer()->ToI420();
    if (options_.adjust_cropping_before_comparing_frames) {
      test_buffer = ScaleVideoFrameBuffer(
          *test_buffer, reference_buffer->width(), reference_buffer->height());
      reference_buffer = test::AdjustCropping(reference_buffer, test_buffer);
    }
    if (options_.compute_psnr) {
      psnr = options_.use_weighted_psnr
                 ? I420WeightedPSNR(*reference_buffer, *test_buffer)
                 : I420PSNR(*reference_buffer, *test_buffer);
    }
    if (options_.compute_ssim) {
      ssim = I420SSIM(*reference_buffer, *test_buffer);
    }
  }

  const FrameStats& frame_stats = comparison.frame_stats;

  MutexLock lock(&mutex_);
  auto stats_it = stream_stats_.find(comparison.stats_key);
  RTC_CHECK(stats_it != stream_stats_.end()) << comparison.stats_key.ToString();
  StreamStats* stats = &stats_it->second;

  frames_comparator_stats_.comparisons_done++;
  if (comparison.overload_reason == OverloadReason::kCpu) {
    frames_comparator_stats_.cpu_overloaded_comparisons_done++;
  } else if (comparison.overload_reason == OverloadReason::kMemory) {
    frames_comparator_stats_.memory_overloaded_comparisons_done++;
  }

  std::map<std::string, std::string> metadata;
  metadata.emplace(SampleMetadataKey::kFrameIdMetadataKey,
                   std::to_string(frame_stats.frame_id));

  if (psnr > 0) {
    stats->psnr.AddSample(
        StatsSample(psnr, frame_stats.rendered_time, metadata));
  }
  if (ssim > 0) {
    stats->ssim.AddSample(
        StatsSample(ssim, frame_stats.received_time, metadata));
  }
  stats->capture_frame_rate.AddEvent(frame_stats.captured_time);
  if (frame_stats.time_between_captured_frames.has_value()) {
    stats->time_between_captured_frames_ms.AddSample(
        StatsSample(*frame_stats.time_between_captured_frames,
                    frame_stats.captured_time, metadata));
  }

  // Compute dropped phase for dropped frame
  if (comparison.type == FrameComparisonType::kDroppedFrame) {
    FrameDropPhase dropped_phase;
    if (frame_stats.decode_end_time.IsFinite()) {
      dropped_phase = FrameDropPhase::kAfterDecoder;
    } else if (frame_stats.decode_start_time.IsFinite()) {
      dropped_phase = FrameDropPhase::kByDecoder;
    } else if (frame_stats.encoded_time.IsFinite()) {
      dropped_phase = FrameDropPhase::kTransport;
    } else if (frame_stats.pre_encode_time.IsFinite()) {
      dropped_phase = FrameDropPhase::kByEncoder;
    } else {
      dropped_phase = FrameDropPhase::kBeforeEncoder;
    }
    stats->dropped_by_phase[dropped_phase]++;
  }

  if (frame_stats.encoded_time.IsFinite()) {
    stats->encode_time_ms.AddSample(
        StatsSample(frame_stats.encoded_time - frame_stats.pre_encode_time,
                    frame_stats.encoded_time, metadata));
    stats->encode_frame_rate.AddEvent(frame_stats.encoded_time);
    if (frame_stats.time_between_encoded_frames.has_value()) {
      stats->time_between_encoded_frames_ms.AddSample(
          StatsSample(*frame_stats.time_between_encoded_frames,
                      frame_stats.encoded_time, metadata));
    }
    stats->total_encoded_images_payload +=
        frame_stats.encoded_image_size.bytes();
    stats->target_encode_bitrate.AddSample(StatsSample(
        frame_stats.target_encode_bitrate, frame_stats.encoded_time, metadata));
    for (const auto& [spatial_layer, qp_values] :
         frame_stats.spatial_layers_qp) {
      for (SamplesStatsCounter::StatsSample qp : qp_values.GetTimedSamples()) {
        qp.metadata = metadata;
        stats->spatial_layers_qp[spatial_layer].AddSample(std::move(qp));
      }
    }

    // Stats sliced on encoded frame type.
    if (frame_stats.encoded_frame_type == VideoFrameType::kVideoFrameKey) {
      ++stats->num_send_key_frames;
    }
  }
  // Next stats can be calculated only if frame was received on remote side.
  if (comparison.type != FrameComparisonType::kDroppedFrame ||
      comparison.frame_stats.decoder_failed) {
    if (frame_stats.rendered_time.IsFinite()) {
      stats->total_delay_incl_transport_ms.AddSample(
          StatsSample(frame_stats.rendered_time - frame_stats.captured_time,
                      frame_stats.received_time, metadata));
      stats->receive_to_render_time_ms.AddSample(
          StatsSample(frame_stats.rendered_time - frame_stats.received_time,
                      frame_stats.rendered_time, metadata));
    }
    if (frame_stats.decode_start_time.IsFinite()) {
      stats->transport_time_ms.AddSample(
          StatsSample(frame_stats.decode_start_time - frame_stats.encoded_time,
                      frame_stats.decode_start_time, metadata));

      // Stats sliced on decoded frame type.
      if (frame_stats.pre_decoded_frame_type ==
          VideoFrameType::kVideoFrameKey) {
        ++stats->num_recv_key_frames;
        stats->recv_key_frame_size_bytes.AddSample(
            StatsSample(frame_stats.pre_decoded_image_size.bytes(),
                        frame_stats.decode_start_time, metadata));
      } else if (frame_stats.pre_decoded_frame_type ==
                 VideoFrameType::kVideoFrameDelta) {
        stats->recv_delta_frame_size_bytes.AddSample(
            StatsSample(frame_stats.pre_decoded_image_size.bytes(),
                        frame_stats.decode_start_time, metadata));
      }
    }
    if (frame_stats.decode_end_time.IsFinite()) {
      stats->decode_time_ms.AddSample(StatsSample(
          frame_stats.decode_end_time - frame_stats.decode_start_time,
          frame_stats.decode_end_time, metadata));
      stats->resolution_of_decoded_frame.AddSample(
          StatsSample(*comparison.frame_stats.decoded_frame_width *
                          *comparison.frame_stats.decoded_frame_height,
                      frame_stats.decode_end_time, metadata));
    }

    if (frame_stats.prev_frame_rendered_time.has_value() &&
        frame_stats.rendered_time.IsFinite()) {
      RTC_DCHECK(frame_stats.time_between_rendered_frames.has_value());
      stats->time_between_rendered_frames_ms.AddSample(
          StatsSample(*frame_stats.time_between_rendered_frames,
                      frame_stats.rendered_time, metadata));
      TimeDelta average_time_between_rendered_frames = TimeDelta::Millis(
          stats->time_between_rendered_frames_ms.GetAverage());
      if (*frame_stats.time_between_rendered_frames >
          std::max(kFreezeThreshold + average_time_between_rendered_frames,
                   3 * average_time_between_rendered_frames)) {
        stats->freeze_time_ms.AddSample(
            StatsSample(*frame_stats.time_between_rendered_frames,
                        frame_stats.rendered_time, metadata));
        auto freeze_end_it =
            stream_last_freeze_end_time_.find(comparison.stats_key);
        RTC_DCHECK(freeze_end_it != stream_last_freeze_end_time_.end());
        // TODO(bugs.webrtc.org/14995): rethink this metric for paused stream.
        stats->time_between_freezes_ms.AddSample(StatsSample(
            *frame_stats.prev_frame_rendered_time - freeze_end_it->second,
            frame_stats.rendered_time, metadata));
        freeze_end_it->second = frame_stats.rendered_time;
      }
    }
  }
  // Compute stream codec info.
  if (frame_stats.used_encoder.has_value()) {
    if (stats->encoders.empty() || stats->encoders.back().codec_name !=
                                       frame_stats.used_encoder->codec_name) {
      stats->encoders.push_back(*frame_stats.used_encoder);
    }
    stats->encoders.back().last_frame_id =
        frame_stats.used_encoder->last_frame_id;
    stats->encoders.back().switched_from_at =
        frame_stats.used_encoder->switched_from_at;
  }

  if (frame_stats.used_decoder.has_value()) {
    if (stats->decoders.empty() || stats->decoders.back().codec_name !=
                                       frame_stats.used_decoder->codec_name) {
      stats->decoders.push_back(*frame_stats.used_decoder);
    }
    stats->decoders.back().last_frame_id =
        frame_stats.used_decoder->last_frame_id;
    stats->decoders.back().switched_from_at =
        frame_stats.used_decoder->switched_from_at;
  }
}

Timestamp DefaultVideoQualityAnalyzerFramesComparator::Now() {
  return clock_->CurrentTime();
}

}  // namespace webrtc
