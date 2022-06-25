/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/ivf_video_frame_generator.h"

#include <limits>

#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/video_codec.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kMaxNextFrameWaitTemeoutMs = 1000;

}  // namespace

IvfVideoFrameGenerator::IvfVideoFrameGenerator(const std::string& file_name)
    : callback_(this),
      file_reader_(IvfFileReader::Create(FileWrapper::OpenReadOnly(file_name))),
      video_decoder_(CreateVideoDecoder(file_reader_->GetVideoCodecType())),
      width_(file_reader_->GetFrameWidth()),
      height_(file_reader_->GetFrameHeight()) {
  RTC_CHECK(video_decoder_) << "No decoder found for file's video codec type";
  VideoCodec codec_settings;
  codec_settings.codecType = file_reader_->GetVideoCodecType();
  codec_settings.width = file_reader_->GetFrameWidth();
  codec_settings.height = file_reader_->GetFrameHeight();
  // Set buffer pool size to max value to ensure that if users of generator,
  // ex. test frameworks, will retain frames for quite a long time, decoder
  // won't crash with buffers pool overflow error.
  codec_settings.buffer_pool_size = std::numeric_limits<int>::max();
  RTC_CHECK_EQ(video_decoder_->RegisterDecodeCompleteCallback(&callback_),
               WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK_EQ(
      video_decoder_->InitDecode(&codec_settings, /*number_of_cores=*/1),
      WEBRTC_VIDEO_CODEC_OK);
}
IvfVideoFrameGenerator::~IvfVideoFrameGenerator() {
  MutexLock lock(&lock_);
  if (!file_reader_) {
    return;
  }
  file_reader_->Close();
  file_reader_.reset();
  // Reset decoder to prevent it from async access to |this|.
  video_decoder_.reset();
  {
    MutexLock frame_lock(&frame_decode_lock_);
    next_frame_ = absl::nullopt;
    // Set event in case another thread is waiting on it.
    next_frame_decoded_.Set();
  }
}

FrameGeneratorInterface::VideoFrameData IvfVideoFrameGenerator::NextFrame() {
  MutexLock lock(&lock_);
  next_frame_decoded_.Reset();
  RTC_CHECK(file_reader_);
  if (!file_reader_->HasMoreFrames()) {
    file_reader_->Reset();
  }
  absl::optional<EncodedImage> image = file_reader_->NextFrame();
  RTC_CHECK(image);
  // Last parameter is undocumented and there is no usage of it found.
  RTC_CHECK_EQ(WEBRTC_VIDEO_CODEC_OK,
               video_decoder_->Decode(*image, /*missing_frames=*/false,
                                      /*render_time_ms=*/0));
  bool decoded = next_frame_decoded_.Wait(kMaxNextFrameWaitTemeoutMs);
  RTC_CHECK(decoded) << "Failed to decode next frame in "
                     << kMaxNextFrameWaitTemeoutMs << "ms. Can't continue";

  MutexLock frame_lock(&frame_decode_lock_);
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      next_frame_->video_frame_buffer();
  if (width_ != static_cast<size_t>(buffer->width()) ||
      height_ != static_cast<size_t>(buffer->height())) {
    // Video adapter has requested a down-scale. Allocate a new buffer and
    // return scaled version.
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(width_, height_);
    scaled_buffer->ScaleFrom(*buffer->ToI420());
    buffer = scaled_buffer;
  }
  return VideoFrameData(buffer, next_frame_->update_rect());
}

void IvfVideoFrameGenerator::ChangeResolution(size_t width, size_t height) {
  MutexLock lock(&lock_);
  width_ = width;
  height_ = height;
}

int32_t IvfVideoFrameGenerator::DecodedCallback::Decoded(
    VideoFrame& decoded_image) {
  Decoded(decoded_image, 0, 0);
  return WEBRTC_VIDEO_CODEC_OK;
}
int32_t IvfVideoFrameGenerator::DecodedCallback::Decoded(
    VideoFrame& decoded_image,
    int64_t decode_time_ms) {
  Decoded(decoded_image, decode_time_ms, 0);
  return WEBRTC_VIDEO_CODEC_OK;
}
void IvfVideoFrameGenerator::DecodedCallback::Decoded(
    VideoFrame& decoded_image,
    absl::optional<int32_t> decode_time_ms,
    absl::optional<uint8_t> qp) {
  reader_->OnFrameDecoded(decoded_image);
}

void IvfVideoFrameGenerator::OnFrameDecoded(const VideoFrame& decoded_frame) {
  MutexLock lock(&frame_decode_lock_);
  next_frame_ = decoded_frame;
  next_frame_decoded_.Set();
}

std::unique_ptr<VideoDecoder> IvfVideoFrameGenerator::CreateVideoDecoder(
    VideoCodecType codec_type) {
  if (codec_type == VideoCodecType::kVideoCodecVP8) {
    return VP8Decoder::Create();
  }
  if (codec_type == VideoCodecType::kVideoCodecVP9) {
    return VP9Decoder::Create();
  }
  if (codec_type == VideoCodecType::kVideoCodecH264) {
    return H264Decoder::Create();
  }
  return nullptr;
}

}  // namespace test
}  // namespace webrtc
