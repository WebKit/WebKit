/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/encoder_database.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
const size_t kDefaultPayloadSize = 1440;
}

VCMEncoderDataBase::VCMEncoderDataBase(
    VCMEncodedFrameCallback* encoded_frame_callback)
    : number_of_cores_(0),
      max_payload_size_(kDefaultPayloadSize),
      pending_encoder_reset_(true),
      send_codec_(),
      external_encoder_(nullptr),
      internal_source_(false),
      encoded_frame_callback_(encoded_frame_callback) {}

VCMEncoderDataBase::~VCMEncoderDataBase() {
  DeleteEncoder();
}

// Assuming only one registered encoder - since only one used, no need for more.
bool VCMEncoderDataBase::SetSendCodec(const VideoCodec* send_codec,
                                      int number_of_cores,
                                      size_t max_payload_size) {
  RTC_DCHECK(send_codec);
  if (max_payload_size == 0) {
    max_payload_size = kDefaultPayloadSize;
  }
  RTC_DCHECK_GE(number_of_cores, 1);
  // Make sure the start bit rate is sane...
  RTC_DCHECK_LE(send_codec->startBitrate, 1000000);
  bool reset_required = pending_encoder_reset_;
  if (number_of_cores_ != number_of_cores) {
    number_of_cores_ = number_of_cores;
    reset_required = true;
  }
  if (max_payload_size_ != max_payload_size) {
    max_payload_size_ = max_payload_size;
    reset_required = true;
  }

  VideoCodec new_send_codec;
  memcpy(&new_send_codec, send_codec, sizeof(new_send_codec));

  if (new_send_codec.maxBitrate == 0) {
    // max is one bit per pixel
    new_send_codec.maxBitrate = (static_cast<int>(send_codec->height) *
                                 static_cast<int>(send_codec->width) *
                                 static_cast<int>(send_codec->maxFramerate)) /
                                1000;
    if (send_codec->startBitrate > new_send_codec.maxBitrate) {
      // But if the user tries to set a higher start bit rate we will
      // increase the max accordingly.
      new_send_codec.maxBitrate = send_codec->startBitrate;
    }
  }

  if (new_send_codec.startBitrate > new_send_codec.maxBitrate)
    new_send_codec.startBitrate = new_send_codec.maxBitrate;

  if (!reset_required) {
    reset_required = RequiresEncoderReset(new_send_codec);
  }

  memcpy(&send_codec_, &new_send_codec, sizeof(send_codec_));

  if (!reset_required) {
    return true;
  }

  // If encoder exists, will destroy it and create new one.
  DeleteEncoder();
  ptr_encoder_.reset(new VCMGenericEncoder(
      external_encoder_, encoded_frame_callback_, internal_source_));
  encoded_frame_callback_->SetInternalSource(internal_source_);
  if (ptr_encoder_->InitEncode(&send_codec_, number_of_cores_,
                               max_payload_size_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to initialize video encoder.";
    DeleteEncoder();
    return false;
  }

  pending_encoder_reset_ = false;

  return true;
}

void VCMEncoderDataBase::DeregisterExternalEncoder() {
  DeleteEncoder();
  memset(&send_codec_, 0, sizeof(VideoCodec));
  external_encoder_ = nullptr;
  internal_source_ = false;
}

void VCMEncoderDataBase::RegisterExternalEncoder(VideoEncoder* external_encoder,
                                                 bool internal_source) {
  // Since only one encoder can be used at a given time, only one external
  // encoder can be registered/used.
  RTC_CHECK(external_encoder_ == nullptr);
  external_encoder_ = external_encoder;
  internal_source_ = internal_source;
  pending_encoder_reset_ = true;
}

bool VCMEncoderDataBase::RequiresEncoderReset(
    const VideoCodec& new_send_codec) {
  if (!ptr_encoder_)
    return true;

  // Does not check startBitrate, maxFramerate or plType
  if (new_send_codec.codecType != send_codec_.codecType ||
      new_send_codec.width != send_codec_.width ||
      new_send_codec.height != send_codec_.height ||
      new_send_codec.maxBitrate != send_codec_.maxBitrate ||
      new_send_codec.minBitrate != send_codec_.minBitrate ||
      new_send_codec.qpMax != send_codec_.qpMax ||
      new_send_codec.numberOfSimulcastStreams !=
          send_codec_.numberOfSimulcastStreams ||
      new_send_codec.mode != send_codec_.mode) {
    return true;
  }

  switch (new_send_codec.codecType) {
    case kVideoCodecVP8:
      if (new_send_codec.VP8() != *send_codec_.VP8()) {
        return true;
      }
      break;

    case kVideoCodecVP9:
      if (new_send_codec.VP9() != *send_codec_.VP9()) {
        return true;
      }
      break;

    case kVideoCodecH264:
      if (new_send_codec.H264() != *send_codec_.H264()) {
        return true;
      }
      break;

    default:
      break;
  }

  for (unsigned char i = 0; i < new_send_codec.numberOfSimulcastStreams; ++i) {
    if (new_send_codec.simulcastStream[i] != send_codec_.simulcastStream[i])
      return true;
  }
  return false;
}

VCMGenericEncoder* VCMEncoderDataBase::GetEncoder() {
  return ptr_encoder_.get();
}

void VCMEncoderDataBase::DeleteEncoder() {
  if (!ptr_encoder_)
    return;
  ptr_encoder_->Release();
  ptr_encoder_.reset();
}

bool VCMEncoderDataBase::MatchesCurrentResolution(int width, int height) const {
  return send_codec_.width == width && send_codec_.height == height;
}

}  // namespace webrtc
