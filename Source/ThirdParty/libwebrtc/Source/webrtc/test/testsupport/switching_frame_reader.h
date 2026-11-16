/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_SWITCHING_FRAME_READER_H_
#define TEST_TESTSUPPORT_SWITCHING_FRAME_READER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

// A composite FrameReader that holds multiple file-based frame readers and
// periodically switches between them at a specified time interval (looping
// back to the beginning). Frames pulled or read are scaled to the target
// resolution if they differ.
class SwitchingFrameReader : public FrameReader {
 public:
  SwitchingFrameReader(std::vector<std::string> file_paths,
                       Resolution target_resolution,
                       int fps,
                       TimeDelta camera_switching_interval,
                       YuvFrameReaderImpl::RepeatMode repeat_mode =
                           YuvFrameReaderImpl::RepeatMode::kPingPong);

  SwitchingFrameReader(std::vector<std::unique_ptr<FrameReader>> readers,
                       Resolution target_resolution,
                       int fps,
                       TimeDelta camera_switching_interval);

  ~SwitchingFrameReader() override = default;

  scoped_refptr<I420Buffer> PullFrame() override;
  scoped_refptr<I420Buffer> PullFrame(int* frame_num) override;
  scoped_refptr<I420Buffer> PullFrame(int* frame_num,
                                      Resolution resolution,
                                      Ratio framerate_scale) override;
  scoped_refptr<I420Buffer> ReadFrame(int frame_num) override;
  scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                      Resolution resolution) override;

  int num_frames() const override;

  // Returns the index of the active reader for the next PullFrame() call.
  int current_reader_index() const;

 private:
  struct FrameLocation {
    int reader_index;
    int sub_frame_num;
  };

  class RateScaler {
   public:
    int Skip(Ratio framerate_scale);

   private:
    std::optional<int> ticks_;
  };

  FrameLocation GetFrameLocation(int frame_num) const;
  scoped_refptr<I420Buffer> Scale(scoped_refptr<I420Buffer> buffer,
                                  Resolution resolution) const;

  const std::vector<std::unique_ptr<FrameReader>> readers_;
  const Resolution target_resolution_;
  const int fps_;
  const TimeDelta camera_switching_interval_;
  int frame_num_ = 0;
  RateScaler framerate_scaler_;
  scoped_refptr<I420Buffer> last_frame_;
  int last_frame_num_ = 0;
  mutable std::vector<FrameLocation> frame_location_cache_;
  mutable std::vector<int> count_per_reader_;
};

std::unique_ptr<FrameReader> CreateSwitchingFrameReader(
    std::vector<std::string> file_paths,
    Resolution target_resolution,
    int fps,
    TimeDelta camera_switching_interval,
    YuvFrameReaderImpl::RepeatMode repeat_mode =
        YuvFrameReaderImpl::RepeatMode::kPingPong);

std::unique_ptr<FrameGeneratorInterface> CreateSwitchingFrameGenerator(
    std::vector<std::string> file_paths,
    Resolution target_resolution,
    int fps,
    TimeDelta camera_switching_interval,
    YuvFrameReaderImpl::RepeatMode repeat_mode =
        YuvFrameReaderImpl::RepeatMode::kRepeat);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_SWITCHING_FRAME_READER_H_
