/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/analyzing_video_sink.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/video/video_frame_writer.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"
#include "test/pc/e2e/analyzer/video/video_dumping.h"
#include "test/pc/e2e/metric_metadata_keys.h"
#include "test/testsupport/fixed_fps_video_frame_writer_adapter.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

AnalyzingVideoSink::AnalyzingVideoSink(absl::string_view peer_name,
                                       Clock* clock,
                                       VideoQualityAnalyzerInterface& analyzer,
                                       AnalyzingVideoSinksHelper& sinks_helper,
                                       const VideoSubscription& subscription,
                                       bool report_infra_stats)
    : peer_name_(peer_name),
      report_infra_stats_(report_infra_stats),
      clock_(clock),
      analyzer_(&analyzer),
      sinks_helper_(&sinks_helper),
      subscription_(subscription) {}

void AnalyzingVideoSink::UpdateSubscription(
    const VideoSubscription& subscription) {
  // For peers with changed resolutions we need to close current writers and
  // open new ones. This is done by removing existing sinks, which will force
  // creation of the new sinks when next frame will be received.
  std::set<test::VideoFrameWriter*> writers_to_close;
  {
    MutexLock lock(&mutex_);
    subscription_ = subscription;
    for (auto it = stream_sinks_.cbegin(); it != stream_sinks_.cend();) {
      std::optional<VideoResolution> new_requested_resolution =
          subscription_.GetResolutionForPeer(it->second.sender_peer_name);
      if (!new_requested_resolution.has_value() ||
          (*new_requested_resolution != it->second.resolution)) {
        RTC_LOG(LS_INFO) << peer_name_ << ": Subscribed resolution for stream "
                         << it->first << " from " << it->second.sender_peer_name
                         << " was updated from "
                         << it->second.resolution.ToString() << " to "
                         << new_requested_resolution->ToString()
                         << ". Repopulating all video sinks and recreating "
                         << "requested video writers";
        writers_to_close.insert(it->second.video_frame_writer);
        it = stream_sinks_.erase(it);
      } else {
        ++it;
      }
    }
  }
  sinks_helper_->CloseAndRemoveVideoWriters(writers_to_close);
}

void AnalyzingVideoSink::OnFrame(const VideoFrame& frame) {
  if (IsDummyFrame(frame)) {
    // This is dummy frame, so we  don't need to process it further.
    return;
  }

  if (frame.id() == VideoFrame::kNotSetId) {
    // If frame ID is unknown we can't get required render resolution, so pass
    // to the analyzer in the actual resolution of the frame.
    AnalyzeFrame(frame);
  } else {
    std::string stream_label = analyzer_->GetStreamLabel(frame.id());
    MutexLock lock(&mutex_);
    Timestamp processing_started = clock_->CurrentTime();
    SinksDescriptor* sinks_descriptor = PopulateSinks(stream_label);
    RTC_CHECK(sinks_descriptor != nullptr);

    VideoFrame scaled_frame =
        ScaleVideoFrame(frame, sinks_descriptor->resolution);
    AnalyzeFrame(scaled_frame);
    for (auto& sink : sinks_descriptor->sinks) {
      sink->OnFrame(scaled_frame);
    }
    Timestamp processing_finished = clock_->CurrentTime();

    if (report_infra_stats_) {
      stats_.analyzing_sink_processing_time_ms.AddSample(
          (processing_finished - processing_started).ms<double>());
    }
  }
}

void AnalyzingVideoSink::LogMetrics(webrtc::test::MetricsLogger& metrics_logger,
                                    absl::string_view test_case_name) const {
  if (report_infra_stats_) {
    MutexLock lock(&mutex_);
    const std::string test_case(test_case_name);
    // TODO(bugs.webrtc.org/14757): Remove kExperimentalTestNameMetadataKey.
    std::map<std::string, std::string> metadata = {
        {MetricMetadataKey::kPeerMetadataKey, peer_name_},
        {MetricMetadataKey::kExperimentalTestNameMetadataKey, test_case}};
    metrics_logger.LogMetric(
        "analyzing_sink_processing_time_ms", test_case + "/" + peer_name_,
        stats_.analyzing_sink_processing_time_ms, test::Unit::kMilliseconds,
        test::ImprovementDirection::kSmallerIsBetter, metadata);
    metrics_logger.LogMetric("scaling_tims_ms", test_case + "/" + peer_name_,
                             stats_.scaling_tims_ms, test::Unit::kMilliseconds,
                             test::ImprovementDirection::kSmallerIsBetter,
                             metadata);
  }
}

AnalyzingVideoSink::Stats AnalyzingVideoSink::stats() const {
  MutexLock lock(&mutex_);
  return stats_;
}

VideoFrame AnalyzingVideoSink::ScaleVideoFrame(
    const VideoFrame& frame,
    const VideoResolution& required_resolution) {
  Timestamp processing_started = clock_->CurrentTime();
  if (required_resolution.width() == static_cast<size_t>(frame.width()) &&
      required_resolution.height() == static_cast<size_t>(frame.height())) {
    if (report_infra_stats_) {
      stats_.scaling_tims_ms.AddSample(
          (clock_->CurrentTime() - processing_started).ms<double>());
    }
    return frame;
  }

  // We allow some difference in the aspect ration because when decoder
  // downscales video stream it may round up some dimensions to make them even,
  // ex: 960x540 -> 480x270 -> 240x136 instead of 240x135.
  RTC_CHECK_LE(std::abs(static_cast<double>(required_resolution.width()) /
                            required_resolution.height() -
                        static_cast<double>(frame.width()) / frame.height()),
               0.1)
      << peer_name_
      << ": Received frame has too different aspect ratio compared to "
      << "requested video resolution: required resolution="
      << required_resolution.ToString()
      << "; actual resolution=" << frame.width() << "x" << frame.height();

  rtc::scoped_refptr<I420Buffer> scaled_buffer(I420Buffer::Create(
      required_resolution.width(), required_resolution.height()));
  scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());

  VideoFrame scaled_frame = frame;
  scaled_frame.set_video_frame_buffer(scaled_buffer);
  if (report_infra_stats_) {
    stats_.scaling_tims_ms.AddSample(
        (clock_->CurrentTime() - processing_started).ms<double>());
  }
  return scaled_frame;
}

void AnalyzingVideoSink::AnalyzeFrame(const VideoFrame& frame) {
  VideoFrame frame_copy = frame;
  frame_copy.set_video_frame_buffer(
      I420Buffer::Copy(*frame.video_frame_buffer()->ToI420()));
  analyzer_->OnFrameRendered(peer_name_, frame_copy);
}

AnalyzingVideoSink::SinksDescriptor* AnalyzingVideoSink::PopulateSinks(
    absl::string_view stream_label) {
  // Fast pass: sinks already exists.
  auto sinks_it = stream_sinks_.find(std::string(stream_label));
  if (sinks_it != stream_sinks_.end()) {
    return &sinks_it->second;
  }

  // Slow pass: we need to create and save sinks
  std::optional<std::pair<std::string, VideoConfig>> peer_and_config =
      sinks_helper_->GetPeerAndConfig(stream_label);
  RTC_CHECK(peer_and_config.has_value())
      << "No video config for stream " << stream_label;
  const std::string& sender_peer_name = peer_and_config->first;
  const VideoConfig& config = peer_and_config->second;

  std::optional<VideoResolution> resolution =
      subscription_.GetResolutionForPeer(sender_peer_name);
  if (!resolution.has_value()) {
    RTC_LOG(LS_ERROR) << peer_name_ << " received stream " << stream_label
                      << " from " << sender_peer_name
                      << " for which they were not subscribed";
    resolution = config.GetResolution();
  }
  if (!resolution->IsRegular()) {
    RTC_LOG(LS_ERROR) << peer_name_ << " received stream " << stream_label
                      << " from " << sender_peer_name
                      << " for which resolution wasn't resolved";
    resolution = config.GetResolution();
  }

  RTC_CHECK(resolution.has_value());

  SinksDescriptor sinks_descriptor(sender_peer_name, *resolution);
  if (config.output_dump_options.has_value()) {
    std::unique_ptr<test::VideoFrameWriter> writer =
        config.output_dump_options->CreateOutputDumpVideoFrameWriter(
            stream_label, peer_name_, *resolution);
    if (config.output_dump_use_fixed_framerate) {
      writer = std::make_unique<test::FixedFpsVideoFrameWriterAdapter>(
          resolution->fps(), clock_, std::move(writer));
    }
    sinks_descriptor.sinks.push_back(std::make_unique<VideoWriter>(
        writer.get(), config.output_dump_options->sampling_modulo()));
    sinks_descriptor.video_frame_writer =
        sinks_helper_->AddVideoWriter(std::move(writer));
  }
  if (config.show_on_screen) {
    sinks_descriptor.sinks.push_back(
        absl::WrapUnique(test::VideoRenderer::Create(
            (*config.stream_label + "-render").c_str(), resolution->width(),
            resolution->height())));
  }
  return &stream_sinks_.emplace(stream_label, std::move(sinks_descriptor))
              .first->second;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
