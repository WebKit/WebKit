/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/ivf_frame_reader.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
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
#include "rtc_base/logging.h"
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
      return nullptr;
    case VideoCodecType::kVideoCodecGeneric:
      return nullptr;
  }
}

}  // namespace

IvfFrameReader::IvfFrameReader(const Environment& env,
                               absl::string_view filepath,
                               bool repeat)
    : file_reader_(IvfFileReader::Create(FileWrapper::OpenReadOnly(filepath))),
      callback_(this),
      frame_num_(0),
      repeat_(repeat) {
  RTC_CHECK(file_reader_) << "Failed to open IVF file: " << filepath;
  resolution_ = {.width = file_reader_->GetFrameWidth(),
                 .height = file_reader_->GetFrameHeight()};
  video_decoder_ = CreateDecoder(env, file_reader_->GetVideoCodecType());
  RTC_CHECK(video_decoder_) << "No decoder found for file's video codec type";

  VideoDecoder::Settings decoder_settings;
  decoder_settings.set_codec_type(file_reader_->GetVideoCodecType());
  decoder_settings.set_max_render_resolution(
      {file_reader_->GetFrameWidth(), file_reader_->GetFrameHeight()});
  decoder_settings.set_buffer_pool_size(std::numeric_limits<int>::max());

  RTC_CHECK_EQ(video_decoder_->RegisterDecodeCompleteCallback(&callback_),
               WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK(video_decoder_->Configure(decoder_settings));
}

IvfFrameReader::~IvfFrameReader() {
  MutexLock lock(&lock_);
  if (!file_reader_) {
    return;
  }
  file_reader_->Close();
  file_reader_.reset();
  video_decoder_.reset();
  {
    MutexLock frame_lock(&frame_decode_lock_);
    next_frame_ = std::nullopt;
    next_frame_decoded_.Set();
  }
}

int IvfFrameReader::RateScaler::Skip(Ratio framerate_scale) {
  ticks_ = ticks_.value_or(framerate_scale.num);
  int skip = 0;
  while (ticks_ <= 0) {
    *ticks_ += framerate_scale.num;
    ++skip;
  }
  *ticks_ -= framerate_scale.den;
  return skip;
}

scoped_refptr<I420Buffer> IvfFrameReader::PullFrame() {
  int frame_num;
  return PullFrame(&frame_num);
}

scoped_refptr<I420Buffer> IvfFrameReader::PullFrame(int* frame_num) {
  return PullFrame(frame_num, resolution_, Ratio());
}

scoped_refptr<I420Buffer> IvfFrameReader::DecodeNextFrameLocked() {
  next_frame_decoded_.Reset();
  RTC_CHECK(file_reader_);

  if (!file_reader_->HasMoreFrames() && repeat_) {
    file_reader_->Reset();
  }

  std::optional<EncodedImage> image = file_reader_->NextFrame();
  if (!image) {
    return nullptr;
  }

  RTC_CHECK_EQ(WEBRTC_VIDEO_CODEC_OK,
               video_decoder_->Decode(*image, /*render_time_ms=*/0));

  bool decoded = next_frame_decoded_.Wait(kMaxNextFrameWaitTimeout);
  RTC_CHECK(decoded) << "Failed to decode next frame in "
                     << kMaxNextFrameWaitTimeout;

  MutexLock frame_lock(&frame_decode_lock_);
  if (!next_frame_) {
    RTC_LOG(LS_WARNING) << "next_frame_ is null after decode event!";
    return nullptr;
  }

  auto i420_buffer = next_frame_->video_frame_buffer()->ToI420();
  RTC_CHECK(i420_buffer);

  // The FrameReader interface requires returning a concrete I420Buffer,
  // which owns its memory and is mutable. The decoder might return a
  // read-only buffer or a different implementation of I420BufferInterface,
  // so we must copy it.
  scoped_refptr<I420Buffer> output_buffer = I420Buffer::Copy(*i420_buffer);
  next_frame_ = std::nullopt;

  return output_buffer;
}

scoped_refptr<I420Buffer> IvfFrameReader::PullFrame(int* frame_num,
                                                    Resolution resolution,
                                                    Ratio framerate_scale) {
  MutexLock lock(&lock_);

  int skip = framerate_scaler_.Skip(framerate_scale);
  if (!last_decoded_buffer_) {
    skip = 1;  // Force reading the first frame.
  }

  scoped_refptr<I420Buffer> buffer;
  if (skip == 0) {
    RTC_CHECK(last_decoded_buffer_);
    buffer = last_decoded_buffer_;
  } else {
    for (int i = 0; i < skip; ++i) {
      buffer = DecodeNextFrameLocked();
      if (!buffer) {
        return nullptr;
      }
    }
    last_decoded_buffer_ = buffer;
  }

  if (frame_num) {
    *frame_num = frame_num_;
  }
  frame_num_++;

  if (buffer->width() != static_cast<int>(resolution.width) ||
      buffer->height() != static_cast<int>(resolution.height)) {
    scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(resolution.width, resolution.height);
    scaled_buffer->ScaleFrom(*buffer);
    return scaled_buffer;
  }

  return buffer;
}

scoped_refptr<I420Buffer> IvfFrameReader::ReadFrame(int frame_num) {
  RTC_CHECK(false) << "ReadFrame is not supported for IVF files.";
  return nullptr;
}

scoped_refptr<I420Buffer> IvfFrameReader::ReadFrame(int frame_num,
                                                    Resolution resolution) {
  RTC_CHECK(false) << "ReadFrame is not supported for IVF files.";
  return nullptr;
}

int IvfFrameReader::num_frames() const {
  MutexLock lock(&lock_);
  return static_cast<int>(file_reader_->GetFramesCount());
}

int32_t IvfFrameReader::DecodedCallback::Decoded(VideoFrame& decoded_image) {
  Decoded(decoded_image, 0, 0);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t IvfFrameReader::DecodedCallback::Decoded(VideoFrame& decoded_image,
                                                 int64_t decode_time_ms) {
  Decoded(decoded_image, decode_time_ms, 0);
  return WEBRTC_VIDEO_CODEC_OK;
}

void IvfFrameReader::DecodedCallback::Decoded(
    VideoFrame& decoded_image,
    std::optional<int32_t> decode_time_ms,
    std::optional<uint8_t> qp) {
  reader_->OnFrameDecoded(decoded_image);
}

void IvfFrameReader::OnFrameDecoded(const VideoFrame& decoded_frame) {
  MutexLock lock(&frame_decode_lock_);
  next_frame_ = decoded_frame;
  next_frame_decoded_.Set();
}

}  // namespace test
}  // namespace webrtc
