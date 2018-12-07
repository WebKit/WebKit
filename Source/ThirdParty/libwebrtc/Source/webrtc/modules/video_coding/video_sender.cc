/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <algorithm>  // std::max

#include "api/video/video_bitrate_allocator.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "modules/video_coding/video_coding_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace vcm {

namespace {

constexpr char kFrameDropperFieldTrial[] = "WebRTC-FrameDropper";

}  // namespace

VideoSender::VideoSender(Clock* clock,
                         EncodedImageCallback* post_encode_callback)
    : _encoder(nullptr),
      _mediaOpt(clock),
      _encodedFrameCallback(post_encode_callback, &_mediaOpt),
      post_encode_callback_(post_encode_callback),
      _codecDataBase(&_encodedFrameCallback),
      frame_dropper_requested_(true),
      force_disable_frame_dropper_(false),
      current_codec_(),
      encoder_params_({VideoBitrateAllocation(), 0, 0, 0}),
      encoder_has_internal_source_(false),
      next_frame_types_(1, kVideoFrameDelta) {
  // Allow VideoSender to be created on one thread but used on another, post
  // construction. This is currently how this class is being used by at least
  // one external project (diffractor).
  sequenced_checker_.Detach();
}

VideoSender::~VideoSender() {}

// Register the send codec to be used.
int32_t VideoSender::RegisterSendCodec(const VideoCodec* sendCodec,
                                       uint32_t numberOfCores,
                                       uint32_t maxPayloadSize) {
  RTC_DCHECK(sequenced_checker_.CalledSequentially());
  rtc::CritScope lock(&encoder_crit_);
  if (sendCodec == nullptr) {
    return VCM_PARAMETER_ERROR;
  }

  bool ret =
      _codecDataBase.SetSendCodec(sendCodec, numberOfCores, maxPayloadSize);

  // Update encoder regardless of result to make sure that we're not holding on
  // to a deleted instance.
  _encoder = _codecDataBase.GetEncoder();
  // Cache the current codec here so they can be fetched from this thread
  // without requiring the _sendCritSect lock.
  current_codec_ = *sendCodec;

  if (!ret) {
    RTC_LOG(LS_ERROR) << "Failed to initialize set encoder with codec type '"
                      << sendCodec->codecType << "'.";
    return VCM_CODEC_ERROR;
  }

  // SetSendCodec succeeded, _encoder should be set.
  RTC_DCHECK(_encoder);

  int numLayers;
  if (sendCodec->codecType == kVideoCodecVP8) {
    numLayers = sendCodec->VP8().numberOfTemporalLayers;
  } else if (sendCodec->codecType == kVideoCodecVP9) {
    numLayers = sendCodec->VP9().numberOfTemporalLayers;
  } else if (sendCodec->codecType == kVideoCodecGeneric &&
             sendCodec->numberOfSimulcastStreams > 0) {
    // This is mainly for unit testing, disabling frame dropping.
    // TODO(sprang): Add a better way to disable frame dropping.
    numLayers = sendCodec->simulcastStream[0].numberOfTemporalLayers;
  } else {
    numLayers = 1;
  }

  // Force-disable frame dropper if either:
  //  * We have screensharing with layers.
  //  * "WebRTC-FrameDropper" field trial is "Disabled".
  force_disable_frame_dropper_ =
      field_trial::IsDisabled(kFrameDropperFieldTrial) ||
      (numLayers > 1 && sendCodec->mode == VideoCodecMode::kScreensharing);

  {
    rtc::CritScope cs(&params_crit_);
    next_frame_types_.clear();
    next_frame_types_.resize(VCM_MAX(sendCodec->numberOfSimulcastStreams, 1),
                             kVideoFrameKey);
    // Cache InternalSource() to have this available from IntraFrameRequest()
    // without having to acquire encoder_crit_ (avoid blocking on encoder use).
    encoder_has_internal_source_ = _encoder->InternalSource();
  }

  RTC_LOG(LS_VERBOSE) << " max bitrate " << sendCodec->maxBitrate
                      << " start bitrate " << sendCodec->startBitrate
                      << " max frame rate " << sendCodec->maxFramerate
                      << " max payload size " << maxPayloadSize;
  _mediaOpt.SetEncodingData(sendCodec->maxBitrate * 1000,
                            sendCodec->startBitrate * 1000,
                            sendCodec->maxFramerate);
  return VCM_OK;
}

// Register an external decoder object.
// This can not be used together with external decoder callbacks.
void VideoSender::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                          bool internalSource /*= false*/) {
  RTC_DCHECK(sequenced_checker_.CalledSequentially());

  rtc::CritScope lock(&encoder_crit_);

  if (externalEncoder == nullptr) {
    _codecDataBase.DeregisterExternalEncoder();
    {
      // Make sure the VCM doesn't use the de-registered codec
      rtc::CritScope params_lock(&params_crit_);
      _encoder = nullptr;
      encoder_has_internal_source_ = false;
    }
    return;
  }
  _codecDataBase.RegisterExternalEncoder(externalEncoder,
                                         internalSource);
}

EncoderParameters VideoSender::UpdateEncoderParameters(
    const EncoderParameters& params,
    VideoBitrateAllocator* bitrate_allocator,
    uint32_t target_bitrate_bps) {
  uint32_t video_target_rate_bps = _mediaOpt.SetTargetRates(target_bitrate_bps);
  uint32_t input_frame_rate = _mediaOpt.InputFrameRate();
  if (input_frame_rate == 0)
    input_frame_rate = current_codec_.maxFramerate;

  VideoBitrateAllocation bitrate_allocation;
  // Only call allocators if bitrate > 0 (ie, not suspended), otherwise they
  // might cap the bitrate to the min bitrate configured.
  if (target_bitrate_bps > 0) {
    if (bitrate_allocator) {
      bitrate_allocation = bitrate_allocator->GetAllocation(
          video_target_rate_bps, input_frame_rate);
    } else {
      DefaultVideoBitrateAllocator default_allocator(current_codec_);
      bitrate_allocation = default_allocator.GetAllocation(
          video_target_rate_bps, input_frame_rate);
    }
  }
  EncoderParameters new_encoder_params = {bitrate_allocation, params.loss_rate,
                                          params.rtt, input_frame_rate};
  return new_encoder_params;
}

void VideoSender::UpdateChannelParameters(
    VideoBitrateAllocator* bitrate_allocator,
    VideoBitrateAllocationObserver* bitrate_updated_callback) {
  VideoBitrateAllocation target_rate;
  {
    rtc::CritScope cs(&params_crit_);
    encoder_params_ =
        UpdateEncoderParameters(encoder_params_, bitrate_allocator,
                                encoder_params_.target_bitrate.get_sum_bps());
    target_rate = encoder_params_.target_bitrate;
  }
  if (bitrate_updated_callback && target_rate.get_sum_bps() > 0)
    bitrate_updated_callback->OnBitrateAllocationUpdated(target_rate);
}

int32_t VideoSender::SetChannelParameters(
    uint32_t target_bitrate_bps,
    uint8_t loss_rate,
    int64_t rtt,
    VideoBitrateAllocator* bitrate_allocator,
    VideoBitrateAllocationObserver* bitrate_updated_callback) {
  EncoderParameters encoder_params;
  encoder_params.loss_rate = loss_rate;
  encoder_params.rtt = rtt;
  encoder_params = UpdateEncoderParameters(encoder_params, bitrate_allocator,
                                           target_bitrate_bps);
  if (bitrate_updated_callback && target_bitrate_bps > 0) {
    bitrate_updated_callback->OnBitrateAllocationUpdated(
        encoder_params.target_bitrate);
  }

  bool encoder_has_internal_source;
  {
    rtc::CritScope cs(&params_crit_);
    encoder_params_ = encoder_params;
    encoder_has_internal_source = encoder_has_internal_source_;
  }

  // For encoders with internal sources, we need to tell the encoder directly,
  // instead of waiting for an AddVideoFrame that will never come (internal
  // source encoders don't get input frames).
  if (encoder_has_internal_source) {
    rtc::CritScope cs(&encoder_crit_);
    if (_encoder) {
      SetEncoderParameters(encoder_params, encoder_has_internal_source);
    }
  }

  return VCM_OK;
}

void VideoSender::SetEncoderParameters(EncoderParameters params,
                                       bool has_internal_source) {
  // |target_bitrate == 0 | means that the network is down or the send pacer is
  // full. We currently only report this if the encoder has an internal source.
  // If the encoder does not have an internal source, higher levels are expected
  // to not call AddVideoFrame. We do this since its unclear how current
  // encoder implementations behave when given a zero target bitrate.
  // TODO(perkj): Make sure all known encoder implementations handle zero
  // target bitrate and remove this check.
  if (!has_internal_source && params.target_bitrate.get_sum_bps() == 0)
    return;

  if (params.input_frame_rate == 0) {
    // No frame rate estimate available, use default.
    params.input_frame_rate = current_codec_.maxFramerate;
  }
  if (_encoder != nullptr)
    _encoder->SetEncoderParameters(params);
}

// Add one raw video frame to the encoder, blocking.
int32_t VideoSender::AddVideoFrame(
    const VideoFrame& videoFrame,
    const CodecSpecificInfo* codecSpecificInfo,
    absl::optional<VideoEncoder::EncoderInfo> encoder_info) {
  EncoderParameters encoder_params;
  std::vector<FrameType> next_frame_types;
  bool encoder_has_internal_source = false;
  {
    rtc::CritScope lock(&params_crit_);
    encoder_params = encoder_params_;
    next_frame_types = next_frame_types_;
    encoder_has_internal_source = encoder_has_internal_source_;
  }
  rtc::CritScope lock(&encoder_crit_);
  if (_encoder == nullptr)
    return VCM_UNINITIALIZED;
  SetEncoderParameters(encoder_params, encoder_has_internal_source);
  if (!encoder_info) {
    encoder_info = _encoder->GetEncoderInfo();
  }

  // Frame dropping is enabled iff frame dropping has been requested, and
  // frame dropping is not force-disabled, and rate controller is not trusted.
  const bool frame_dropping_enabled =
      frame_dropper_requested_ && !force_disable_frame_dropper_ &&
      !encoder_info->has_trusted_rate_controller;
  _mediaOpt.EnableFrameDropper(frame_dropping_enabled);

  if (_mediaOpt.DropFrame()) {
    RTC_LOG(LS_VERBOSE) << "Drop Frame "
                        << "target bitrate "
                        << encoder_params.target_bitrate.get_sum_bps()
                        << " loss rate " << encoder_params.loss_rate << " rtt "
                        << encoder_params.rtt << " input frame rate "
                        << encoder_params.input_frame_rate;
    post_encode_callback_->OnDroppedFrame(
        EncodedImageCallback::DropReason::kDroppedByMediaOptimizations);
    return VCM_OK;
  }
  // TODO(pbos): Make sure setting send codec is synchronized with video
  // processing so frame size always matches.
  if (!_codecDataBase.MatchesCurrentResolution(videoFrame.width(),
                                               videoFrame.height())) {
    RTC_LOG(LS_ERROR)
        << "Incoming frame doesn't match set resolution. Dropping.";
    return VCM_PARAMETER_ERROR;
  }
  VideoFrame converted_frame = videoFrame;
  const VideoFrameBuffer::Type buffer_type =
      converted_frame.video_frame_buffer()->type();
  const bool is_buffer_type_supported =
      buffer_type == VideoFrameBuffer::Type::kI420 ||
      (buffer_type == VideoFrameBuffer::Type::kNative &&
       encoder_info->supports_native_handle);
  if (!is_buffer_type_supported) {
    // This module only supports software encoding.
    // TODO(pbos): Offload conversion from the encoder thread.
    rtc::scoped_refptr<I420BufferInterface> converted_buffer(
        converted_frame.video_frame_buffer()->ToI420());

    if (!converted_buffer) {
      RTC_LOG(LS_ERROR) << "Frame conversion failed, dropping frame.";
      return VCM_PARAMETER_ERROR;
    }
    converted_frame = VideoFrame(converted_buffer,
                                 converted_frame.timestamp(),
                                 converted_frame.render_time_ms(),
                                 converted_frame.rotation());
  }
  int32_t ret =
      _encoder->Encode(converted_frame, codecSpecificInfo, next_frame_types);
  if (ret < 0) {
    RTC_LOG(LS_ERROR) << "Failed to encode frame. Error code: " << ret;
    return ret;
  }

  {
    rtc::CritScope lock(&params_crit_);
    // Change all keyframe requests to encode delta frames the next time.
    for (size_t i = 0; i < next_frame_types_.size(); ++i) {
      // Check for equality (same requested as before encoding) to not
      // accidentally drop a keyframe request while encoding.
      if (next_frame_types[i] == next_frame_types_[i])
        next_frame_types_[i] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

int32_t VideoSender::IntraFrameRequest(size_t stream_index) {
  {
    rtc::CritScope lock(&params_crit_);
    if (stream_index >= next_frame_types_.size()) {
      return -1;
    }
    next_frame_types_[stream_index] = kVideoFrameKey;
    if (!encoder_has_internal_source_)
      return VCM_OK;
  }
  // TODO(pbos): Remove when InternalSource() is gone. Both locks have to be
  // held here for internal consistency, since _encoder could be removed while
  // not holding encoder_crit_. Checks have to be performed again since
  // params_crit_ was dropped to not cause lock-order inversions with
  // encoder_crit_.
  rtc::CritScope lock(&encoder_crit_);
  rtc::CritScope params_lock(&params_crit_);
  if (stream_index >= next_frame_types_.size())
    return -1;
  if (_encoder != nullptr && _encoder->InternalSource()) {
    // Try to request the frame if we have an external encoder with
    // internal source since AddVideoFrame never will be called.
    if (_encoder->RequestFrame(next_frame_types_) == WEBRTC_VIDEO_CODEC_OK) {
      // Try to remove just-performed keyframe request, if stream still exists.
      next_frame_types_[stream_index] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

int32_t VideoSender::EnableFrameDropper(bool enable) {
  rtc::CritScope lock(&encoder_crit_);
  frame_dropper_requested_ = enable;
  _mediaOpt.EnableFrameDropper(enable);
  return VCM_OK;
}
}  // namespace vcm
}  // namespace webrtc
