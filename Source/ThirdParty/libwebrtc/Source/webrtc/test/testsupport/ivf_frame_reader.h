/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_IVF_FRAME_READER_H_
#define TEST_TESTSUPPORT_IVF_FRAME_READER_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/utility/ivf_file_reader.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

class IvfFrameReader : public FrameReader {
 public:
  IvfFrameReader(const Environment& env,
                 absl::string_view filepath,
                 bool repeat);
  ~IvfFrameReader() override;

  scoped_refptr<I420Buffer> PullFrame() override;
  scoped_refptr<I420Buffer> PullFrame(int* frame_num) override;
  scoped_refptr<I420Buffer> PullFrame(int* frame_num,
                                      Resolution resolution,
                                      Ratio framerate_scale) override;

  // DO NOT USE! Random access seeking is not supported for compressed files.
  scoped_refptr<I420Buffer> ReadFrame(int frame_num) override;
  scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                      Resolution resolution) override;

  int num_frames() const override;

 private:
  class DecodedCallback : public DecodedImageCallback {
   public:
    explicit DecodedCallback(IvfFrameReader* reader) : reader_(reader) {}

    int32_t Decoded(VideoFrame& decoded_image) override;
    int32_t Decoded(VideoFrame& decoded_image, int64_t decode_time_ms) override;
    void Decoded(VideoFrame& decoded_image,
                 std::optional<int32_t> decode_time_ms,
                 std::optional<uint8_t> qp) override;

   private:
    IvfFrameReader* const reader_;
  };

  void OnFrameDecoded(const VideoFrame& decoded_frame);

  std::unique_ptr<IvfFileReader> file_reader_ RTC_GUARDED_BY(lock_);
  std::unique_ptr<VideoDecoder> video_decoder_ RTC_GUARDED_BY(lock_);
  DecodedCallback callback_;

  mutable Mutex lock_;
  Mutex frame_decode_lock_;

  Event next_frame_decoded_;
  std::optional<VideoFrame> next_frame_ RTC_GUARDED_BY(frame_decode_lock_);

  int frame_num_ RTC_GUARDED_BY(lock_);
  const bool repeat_;

  Resolution resolution_;

  struct RateScaler {
    int Skip(Ratio framerate_scale);
    std::optional<int> ticks_;
  };
  RateScaler framerate_scaler_ RTC_GUARDED_BY(lock_);

  scoped_refptr<I420Buffer> last_decoded_buffer_ RTC_GUARDED_BY(lock_);

  scoped_refptr<I420Buffer> DecodeNextFrameLocked()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_IVF_FRAME_READER_H_
