/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"

#include <utility>

#include "absl/memory/memory.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_decoder.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_encoder.h"
#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"

namespace webrtc {
namespace webrtc_pc_e2e {

namespace {

class VideoWriter final : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  VideoWriter(test::VideoFrameWriter* video_writer)
      : video_writer_(video_writer) {}
  ~VideoWriter() override = default;

  void OnFrame(const VideoFrame& frame) override {
    bool result = video_writer_->WriteFrame(frame);
    RTC_CHECK(result) << "Failed to write frame";
  }

 private:
  test::VideoFrameWriter* video_writer_;
};

// Intercepts generated frames and passes them also to video quality analyzer
// and to provided sinks.
class AnalyzingFrameGenerator final : public test::FrameGenerator {
 public:
  AnalyzingFrameGenerator(
      std::string stream_label,
      std::unique_ptr<test::FrameGenerator> delegate,
      VideoQualityAnalyzerInterface* analyzer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : stream_label_(std::move(stream_label)),
        delegate_(std::move(delegate)),
        analyzer_(analyzer),
        sinks_(std::move(sinks)) {}
  ~AnalyzingFrameGenerator() override = default;

  VideoFrame* NextFrame() override {
    VideoFrame* frame = delegate_->NextFrame();

    uint16_t frame_id = analyzer_->OnFrameCaptured(stream_label_, *frame);
    frame->set_id(frame_id);

    for (auto& listener : sinks_) {
      listener->OnFrame(*frame);
    }
    return frame;
  }

  void ChangeResolution(size_t width, size_t height) override {
    delegate_->ChangeResolution(width, height);
  }

 private:
  const std::string stream_label_;
  std::unique_ptr<test::FrameGenerator> delegate_;
  VideoQualityAnalyzerInterface* const analyzer_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

// Implements the video sink, that forwards rendered frames to the video quality
// analyzer and provided sinks.
class AnalyzingVideoSink final : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  AnalyzingVideoSink(
      VideoQualityAnalyzerInterface* analyzer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : analyzer_(analyzer), sinks_(std::move(sinks)) {
    RTC_DCHECK(analyzer_);
  }
  ~AnalyzingVideoSink() override = default;

  void OnFrame(const VideoFrame& frame) override {
    if (IsDummyFrameBuffer(frame.video_frame_buffer()->ToI420())) {
      // This is dummy frame, so we  don't need to process it further.
      return;
    }
    analyzer_->OnFrameRendered(frame);
    for (auto& listener : sinks_) {
      listener->OnFrame(frame);
    }
  }

 private:
  VideoQualityAnalyzerInterface* const analyzer_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

}  // namespace

VideoQualityAnalyzerInjectionHelper::VideoQualityAnalyzerInjectionHelper(
    std::unique_ptr<VideoQualityAnalyzerInterface> analyzer,
    EncodedImageDataInjector* injector,
    EncodedImageDataExtractor* extractor)
    : analyzer_(std::move(analyzer)),
      injector_(injector),
      extractor_(extractor),
      encoding_entities_id_generator_(absl::make_unique<IntIdGenerator>(1)) {
  RTC_DCHECK(injector_);
  RTC_DCHECK(extractor_);
}
VideoQualityAnalyzerInjectionHelper::~VideoQualityAnalyzerInjectionHelper() =
    default;

std::unique_ptr<VideoEncoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> delegate,
    double bitrate_multiplier,
    std::map<std::string, absl::optional<int>> stream_required_spatial_index)
    const {
  return absl::make_unique<QualityAnalyzingVideoEncoderFactory>(
      std::move(delegate), bitrate_multiplier,
      std::move(stream_required_spatial_index),
      encoding_entities_id_generator_.get(), injector_, analyzer_.get());
}

std::unique_ptr<VideoDecoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> delegate) const {
  return absl::make_unique<QualityAnalyzingVideoDecoderFactory>(
      std::move(delegate), encoding_entities_id_generator_.get(), extractor_,
      analyzer_.get());
}

std::unique_ptr<test::FrameGenerator>
VideoQualityAnalyzerInjectionHelper::WrapFrameGenerator(
    std::string stream_label,
    std::unique_ptr<test::FrameGenerator> delegate,
    test::VideoFrameWriter* writer) const {
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  if (writer) {
    sinks.push_back(absl::make_unique<VideoWriter>(writer));
  }
  return absl::make_unique<AnalyzingFrameGenerator>(
      std::move(stream_label), std::move(delegate), analyzer_.get(),
      std::move(sinks));
}

std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>
VideoQualityAnalyzerInjectionHelper::CreateVideoSink(
    test::VideoFrameWriter* writer) const {
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  if (writer) {
    sinks.push_back(absl::make_unique<VideoWriter>(writer));
  }
  return absl::make_unique<AnalyzingVideoSink>(analyzer_.get(),
                                               std::move(sinks));
}

void VideoQualityAnalyzerInjectionHelper::Start(std::string test_case_name,
                                                int max_threads_count) {
  analyzer_->Start(std::move(test_case_name), max_threads_count);
}

void VideoQualityAnalyzerInjectionHelper::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& stats_reports) {
  analyzer_->OnStatsReports(pc_label, stats_reports);
}

void VideoQualityAnalyzerInjectionHelper::Stop() {
  analyzer_->Stop();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
