/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_FRAME_GENERATOR_H_
#define WEBRTC_TEST_FRAME_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/media/base/videosourceinterface.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class Clock;
namespace test {

// FrameForwarder can be used as an implementation
// of rtc::VideoSourceInterface<VideoFrame> where the caller controls when
// a frame should be forwarded to its sink.
// Currently this implementation only support one sink.
class FrameForwarder : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  FrameForwarder();
  virtual ~FrameForwarder();
  // Forwards |video_frame| to the registered |sink_|.
  virtual void IncomingCapturedFrame(const VideoFrame& video_frame);
  rtc::VideoSinkWants sink_wants() const;
  bool has_sinks() const;

 protected:
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

  rtc::CriticalSection crit_;
  rtc::VideoSinkInterface<VideoFrame>* sink_ GUARDED_BY(crit_);
  rtc::VideoSinkWants sink_wants_ GUARDED_BY(crit_);
};

class FrameGenerator {
 public:
  virtual ~FrameGenerator() = default;

  // Returns video frame that remains valid until next call.
  virtual VideoFrame* NextFrame() = 0;

  // Change the capture resolution.
  virtual void ChangeResolution(size_t width, size_t height) {
    RTC_NOTREACHED();
  }

  // Creates a frame generator that produces frames with small squares that
  // move randomly towards the lower right corner.
  static std::unique_ptr<FrameGenerator> CreateSquareGenerator(int width,
                                                               int height);

  // Creates a frame generator that repeatedly plays a set of yuv files.
  // The frame_repeat_count determines how many times each frame is shown,
  // with 1 = show each frame once, etc.
  static std::unique_ptr<FrameGenerator> CreateFromYuvFile(
      std::vector<std::string> files,
      size_t width,
      size_t height,
      int frame_repeat_count);

  // Creates a frame generator which takes a set of yuv files (wrapping a
  // frame generator created by CreateFromYuvFile() above), but outputs frames
  // that have been cropped to specified resolution: source_width/source_height
  // is the size of the source images, target_width/target_height is the size of
  // the cropped output. For each source image read, the cropped viewport will
  // be scrolled top to bottom/left to right for scroll_tim_ms milliseconds.
  // After that the image will stay in place for pause_time_ms milliseconds,
  // and then this will be repeated with the next file from the input set.
  static std::unique_ptr<FrameGenerator> CreateScrollingInputFromYuvFiles(
      Clock* clock,
      std::vector<std::string> filenames,
      size_t source_width,
      size_t source_height,
      size_t target_width,
      size_t target_height,
      int64_t scroll_time_ms,
      int64_t pause_time_ms);
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FRAME_GENERATOR_H_
