/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_DECODER_IMPL_H_
#define VIDEO_VIDEO_STREAM_DECODER_IMPL_H_

#include <map>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/video_stream_decoder.h"
#include "modules/video_coding/frame_buffer2.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class VideoStreamDecoderImpl : public VideoStreamDecoder,
                               private DecodedImageCallback {
 public:
  VideoStreamDecoderImpl(
      VideoStreamDecoder::Callbacks* callbacks,
      VideoDecoderFactory* decoder_factory,
      std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings);

  ~VideoStreamDecoderImpl() override;

  void OnFrame(std::unique_ptr<video_coding::EncodedFrame> frame) override;

 private:
  enum DecodeResult {
    kOk,
    kDecodeFailure,
    kNoFrame,
    kNoDecoder,
    kShutdown,
  };

  struct FrameTimestamps {
    int64_t timestamp;
    int64_t decode_start_time_ms;
    int64_t render_time_us;
  };

  VideoDecoder* GetDecoder(int payload_type);
  static void DecodeLoop(void* ptr);
  DecodeResult DecodeNextFrame(int max_wait_time_ms, bool keyframe_required);

  FrameTimestamps* GetFrameTimestamps(int64_t timestamp);

  // Implements DecodedImageCallback interface
  int32_t Decoded(VideoFrame& decodedImage) override;
  int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) override;
  void Decoded(VideoFrame& decodedImage,
               absl::optional<int32_t> decode_time_ms,
               absl::optional<uint8_t> qp) override;

  VideoStreamDecoder::Callbacks* const callbacks_
      RTC_PT_GUARDED_BY(bookkeeping_queue_);
  VideoDecoderFactory* const decoder_factory_;
  std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings_;

  // The |bookkeeping_queue_| is used to:
  //  - Make |callbacks_|.
  //  - Insert/extract frames from the |frame_buffer_|
  //  - Synchronize with whatever thread that makes the Decoded callback.
  rtc::TaskQueue bookkeeping_queue_;

  rtc::PlatformThread decode_thread_;
  VCMJitterEstimator jitter_estimator_;
  VCMTiming timing_;
  video_coding::FrameBuffer frame_buffer_;
  video_coding::VideoLayerFrameId last_continuous_id_;
  absl::optional<int> current_payload_type_;
  std::unique_ptr<VideoDecoder> decoder_;

  // Some decoders are pipelined so it is not sufficient to save frame info
  // for the last frame only.
  static constexpr int kFrameTimestampsMemory = 8;
  std::array<FrameTimestamps, kFrameTimestampsMemory> frame_timestamps_
      RTC_GUARDED_BY(bookkeeping_queue_);
  int next_frame_timestamps_index_ RTC_GUARDED_BY(bookkeeping_queue_);
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_DECODER_IMPL_H_
