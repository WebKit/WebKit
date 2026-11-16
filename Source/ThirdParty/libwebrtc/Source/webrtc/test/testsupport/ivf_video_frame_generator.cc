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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/codecs/av1/dav1d_decoder.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/ivf_file_reader.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {
namespace test {
namespace {

constexpr TimeDelta kMaxNextFrameWaitTimeout = TimeDelta::Seconds(1);

std::unique_ptr<VideoDecoder> CreateDecoder(const Environment& env,
                                            VideoCodecType codec_type) {
  switch (codec_type) {
    case VideoCodecType::kVideoCodecVP8:
      return CreateVp8Decoder(env);
    case VideoCodecType::kVideoCodecVP9:
      return VP9Decoder::Create();
    case VideoCodecType::kVideoCodecH264:
      return H264Decoder::Create();
    case VideoCodecType::kVideoCodecAV1:
      return CreateDav1dDecoder(env);
    case VideoCodecType::kVideoCodecH265:
      // No H.265 SW decoder implementation will be provided.
      return nullptr;
    case VideoCodecType::kVideoCodecGeneric:
      return nullptr;
  }
}

}  // namespace

IvfVideoFrameGenerator::IvfVideoFrameGenerator(const Environment& env,
                                               absl::string_view file_name,
                                               std::optional<int> fps_hint)
    : callback_(this),
      file_reader_(IvfFileReader::Create(FileWrapper::OpenReadOnly(file_name))),
      video_decoder_(CreateDecoder(env, file_reader_->GetVideoCodecType())),
      original_resolution_({.width = file_reader_->GetFrameWidth(),
                            .height = file_reader_->GetFrameHeight()}),
      fps_hint_(fps_hint) {
  RTC_CHECK(video_decoder_) << "No decoder found for file's video codec type";
  VideoDecoder::Settings decoder_settings;
  decoder_settings.set_codec_type(file_reader_->GetVideoCodecType());
  decoder_settings.set_max_render_resolution(
      {file_reader_->GetFrameWidth(), file_reader_->GetFrameHeight()});
  // Set buffer pool size to max value to ensure that if users of generator,
  // ex. test frameworks, will retain frames for quite a long time, decoder
  // won't crash with buffers pool overflow error.
  decoder_settings.set_buffer_pool_size(std::numeric_limits<int>::max());
  RTC_CHECK_EQ(video_decoder_->RegisterDecodeCompleteCallback(&callback_),
               WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK(video_decoder_->Configure(decoder_settings));
}
IvfVideoFrameGenerator::~IvfVideoFrameGenerator() {
  MutexLock lock(&lock_);
  if (!file_reader_) {
    return;
  }
  file_reader_->Close();
  file_reader_.reset();
  // Reset decoder to prevent it from async access to `this`.
  video_decoder_.reset();
  {
    MutexLock frame_lock(&frame_decode_lock_);
    next_frame_ = std::nullopt;
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
  std::optional<EncodedImage> image = file_reader_->NextFrame();
  RTC_CHECK(image);
  // Last parameter is undocumented and there is no usage of it found.
  RTC_CHECK_EQ(WEBRTC_VIDEO_CODEC_OK,
               video_decoder_->Decode(*image, /*render_time_ms=*/0));
  bool decoded = next_frame_decoded_.Wait(kMaxNextFrameWaitTimeout);
  RTC_CHECK(decoded) << "Failed to decode next frame in "
                     << kMaxNextFrameWaitTimeout << ". Can't continue";

  MutexLock frame_lock(&frame_decode_lock_);
  scoped_refptr<VideoFrameBuffer> buffer = next_frame_->video_frame_buffer();

  // Set original resolution to resolution of decoded frame.
  original_resolution_ = {.width = static_cast<size_t>(buffer->width()),
                          .height = static_cast<size_t>(buffer->width())};

  if (output_resolution_.has_value() &&
      (output_resolution_->width != original_resolution_.width ||
       output_resolution_->height != original_resolution_.height)) {
    // Video adapter has requested a down-scale. Allocate a new buffer and
    // return scaled version.
    scoped_refptr<I420Buffer> scaled_buffer = I420Buffer::Create(
        output_resolution_->width, output_resolution_->height);
    scaled_buffer->ScaleFrom(*buffer->ToI420());
    buffer = scaled_buffer;
  }
  return VideoFrameData(buffer, next_frame_->update_rect());
}

void IvfVideoFrameGenerator::SkipNextFrame() {
  MutexLock lock(&lock_);
  next_frame_decoded_.Reset();
  RTC_CHECK(file_reader_);
  if (!file_reader_->HasMoreFrames()) {
    file_reader_->Reset();
  }
  std::optional<EncodedImage> image = file_reader_->NextFrame();
  RTC_CHECK(image);
  // Last parameter is undocumented and there is no usage of it found.
  // Frame has to be decoded in case it is a key frame.
  RTC_CHECK_EQ(WEBRTC_VIDEO_CODEC_OK,
               video_decoder_->Decode(*image, /*render_time_ms=*/0));
}

void IvfVideoFrameGenerator::ChangeResolution(size_t width, size_t height) {
  MutexLock lock(&lock_);
  output_resolution_ = {.width = width, .height = height};
}

FrameGeneratorInterface::Resolution IvfVideoFrameGenerator::GetResolution()
    const {
  return output_resolution_.value_or(original_resolution_);
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
    std::optional<int32_t> decode_time_ms,
    std::optional<uint8_t> qp) {
  reader_->OnFrameDecoded(decoded_image);
}

void IvfVideoFrameGenerator::OnFrameDecoded(const VideoFrame& decoded_frame) {
  MutexLock lock(&frame_decode_lock_);
  next_frame_ = decoded_frame;
  next_frame_decoded_.Set();
}

}  // namespace test
}  // namespace webrtc
