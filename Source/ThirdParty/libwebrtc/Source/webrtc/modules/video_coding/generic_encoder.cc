/*
*  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_coding/generic_encoder.h"

#include <vector>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/media_optimization.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

VCMGenericEncoder::VCMGenericEncoder(
    VideoEncoder* encoder,
    VCMEncodedFrameCallback* encoded_frame_callback,
    bool internal_source)
    : encoder_(encoder),
      vcm_encoded_frame_callback_(encoded_frame_callback),
      internal_source_(internal_source),
      encoder_params_({BitrateAllocation(), 0, 0, 0}),
      is_screenshare_(false) {}

VCMGenericEncoder::~VCMGenericEncoder() {}

int32_t VCMGenericEncoder::Release() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::Release");
  return encoder_->Release();
}

int32_t VCMGenericEncoder::InitEncode(const VideoCodec* settings,
                                      int32_t number_of_cores,
                                      size_t max_payload_size) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::InitEncode");
  is_screenshare_ = settings->mode == VideoCodecMode::kScreensharing;
  if (encoder_->InitEncode(settings, number_of_cores, max_payload_size) != 0) {
    LOG(LS_ERROR) << "Failed to initialize the encoder associated with "
                     "payload name: "
                  << settings->plName;
    return -1;
  }
  encoder_->RegisterEncodeCompleteCallback(vcm_encoded_frame_callback_);
  return 0;
}

int32_t VCMGenericEncoder::Encode(const VideoFrame& frame,
                                  const CodecSpecificInfo* codec_specific,
                                  const std::vector<FrameType>& frame_types) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT1("webrtc", "VCMGenericEncoder::Encode", "timestamp",
               frame.timestamp());

  for (FrameType frame_type : frame_types)
    RTC_DCHECK(frame_type == kVideoFrameKey || frame_type == kVideoFrameDelta);

  int32_t result = encoder_->Encode(frame, codec_specific, &frame_types);

  if (is_screenshare_ &&
      result == WEBRTC_VIDEO_CODEC_TARGET_BITRATE_OVERSHOOT) {
    // Target bitrate exceeded, encoder state has been reset - try again.
    return encoder_->Encode(frame, codec_specific, &frame_types);
  }

  return result;
}

void VCMGenericEncoder::SetEncoderParameters(const EncoderParameters& params) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  bool channel_parameters_have_changed;
  bool rates_have_changed;
  {
    rtc::CritScope lock(&params_lock_);
    channel_parameters_have_changed =
        params.loss_rate != encoder_params_.loss_rate ||
        params.rtt != encoder_params_.rtt;
    rates_have_changed =
        params.target_bitrate != encoder_params_.target_bitrate ||
        params.input_frame_rate != encoder_params_.input_frame_rate;
    encoder_params_ = params;
  }
  if (channel_parameters_have_changed) {
    int res = encoder_->SetChannelParameters(params.loss_rate, params.rtt);
    if (res != 0) {
      LOG(LS_WARNING) << "Error set encoder parameters (loss = "
                      << params.loss_rate << ", rtt = " << params.rtt
                      << "): " << res;
    }
  }
  if (rates_have_changed) {
    int res = encoder_->SetRateAllocation(params.target_bitrate,
                                          params.input_frame_rate);
    if (res != 0) {
      LOG(LS_WARNING) << "Error set encoder rate (total bitrate bps = "
                      << params.target_bitrate.get_sum_bps()
                      << ", framerate = " << params.input_frame_rate
                      << "): " << res;
    }
  }
}

EncoderParameters VCMGenericEncoder::GetEncoderParameters() const {
  rtc::CritScope lock(&params_lock_);
  return encoder_params_;
}

int32_t VCMGenericEncoder::SetPeriodicKeyFrames(bool enable) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return encoder_->SetPeriodicKeyFrames(enable);
}

int32_t VCMGenericEncoder::RequestFrame(
    const std::vector<FrameType>& frame_types) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  // TODO(nisse): Used only with internal source. Delete as soon as
  // that feature is removed. The only implementation I've been able
  // to find ignores what's in the frame.
  return encoder_->Encode(VideoFrame(I420Buffer::Create(1, 1),
                                     kVideoRotation_0, 0),
                          NULL, &frame_types);
  return 0;
}

bool VCMGenericEncoder::InternalSource() const {
  return internal_source_;
}

bool VCMGenericEncoder::SupportsNativeHandle() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return encoder_->SupportsNativeHandle();
}

VCMEncodedFrameCallback::VCMEncodedFrameCallback(
    EncodedImageCallback* post_encode_callback,
    media_optimization::MediaOptimization* media_opt)
    : internal_source_(false),
      post_encode_callback_(post_encode_callback),
      media_opt_(media_opt) {}

VCMEncodedFrameCallback::~VCMEncodedFrameCallback() {}

EncodedImageCallback::Result VCMEncodedFrameCallback::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific,
    const RTPFragmentationHeader* fragmentation_header) {
  TRACE_EVENT_INSTANT1("webrtc", "VCMEncodedFrameCallback::Encoded",
                       "timestamp", encoded_image._timeStamp);
  Result result = post_encode_callback_->OnEncodedImage(
      encoded_image, codec_specific, fragmentation_header);
  if (result.error != Result::OK)
    return result;

  if (media_opt_) {
    media_opt_->UpdateWithEncodedData(encoded_image);
    if (internal_source_) {
      // Signal to encoder to drop next frame.
      result.drop_next_frame = media_opt_->DropFrame();
    }
  }
  return result;
}

}  // namespace webrtc
