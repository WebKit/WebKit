/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/create_frame_generator_capturer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

std::string TransformFilePath(std::string path) {
  static const std::string resource_prefix = "res://";
  int ext_pos = path.rfind('.');
  if (ext_pos < 0) {
    return test::ResourcePath(path, "yuv");
  } else if (absl::StartsWith(path, resource_prefix)) {
    std::string name = path.substr(resource_prefix.length(), ext_pos);
    std::string ext = path.substr(ext_pos, path.size());
    return test::ResourcePath(name, ext);
  }
  return path;
}

}  // namespace

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::SquaresVideo config) {
  return std::make_unique<FrameGeneratorCapturer>(
      clock,
      CreateSquareFrameGenerator(config.width, config.height,
                                 config.pixel_format, config.num_squares),
      config.framerate, task_queue_factory);
}
std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::SquareSlides config) {
  return std::make_unique<FrameGeneratorCapturer>(
      clock,
      CreateSlideFrameGenerator(
          config.width, config.height,
          /*frame_repeat_count*/ config.change_interval.seconds<double>() *
              config.framerate),
      config.framerate, task_queue_factory);
}
std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::VideoFile config) {
  RTC_CHECK(config.width && config.height);
  return std::make_unique<FrameGeneratorCapturer>(
      clock,
      CreateFromYuvFileFrameGenerator({TransformFilePath(config.name)},
                                      config.width, config.height,
                                      /*frame_repeat_count*/ 1),
      config.framerate, task_queue_factory);
}

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::ImageSlides config) {
  std::unique_ptr<FrameGeneratorInterface> slides_generator;
  std::vector<std::string> paths = config.paths;
  for (std::string& path : paths)
    path = TransformFilePath(path);

  if (config.crop.width || config.crop.height) {
    TimeDelta pause_duration =
        config.change_interval - config.crop.scroll_duration;
    RTC_CHECK_GE(pause_duration, TimeDelta::Zero());
    int crop_width = config.crop.width.value_or(config.width);
    int crop_height = config.crop.height.value_or(config.height);
    RTC_CHECK_LE(crop_width, config.width);
    RTC_CHECK_LE(crop_height, config.height);
    slides_generator = CreateScrollingInputFromYuvFilesFrameGenerator(
        clock, paths, config.width, config.height, crop_width, crop_height,
        config.crop.scroll_duration.ms(), pause_duration.ms());
  } else {
    slides_generator = CreateFromYuvFileFrameGenerator(
        paths, config.width, config.height,
        /*frame_repeat_count*/ config.change_interval.seconds<double>() *
            config.framerate);
  }
  return std::make_unique<FrameGeneratorCapturer>(
      clock, std::move(slides_generator), config.framerate, task_queue_factory);
}

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    const FrameGeneratorCapturerConfig& config) {
  if (config.video_file) {
    return CreateFrameGeneratorCapturer(clock, task_queue_factory,
                                        *config.video_file);
  } else if (config.image_slides) {
    return CreateFrameGeneratorCapturer(clock, task_queue_factory,
                                        *config.image_slides);
  } else if (config.squares_slides) {
    return CreateFrameGeneratorCapturer(clock, task_queue_factory,
                                        *config.squares_slides);
  } else {
    return CreateFrameGeneratorCapturer(
        clock, task_queue_factory,
        config.squares_video.value_or(
            FrameGeneratorCapturerConfig::SquaresVideo()));
  }
}

}  // namespace test
}  // namespace webrtc
