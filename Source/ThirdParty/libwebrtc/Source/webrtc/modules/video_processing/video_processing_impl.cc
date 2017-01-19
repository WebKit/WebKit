/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/video_processing_impl.h"

#include <assert.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

VideoProcessing* VideoProcessing::Create() {
  return new VideoProcessingImpl();
}

VideoProcessingImpl::VideoProcessingImpl() {}
VideoProcessingImpl::~VideoProcessingImpl() {}

void VideoProcessingImpl::EnableTemporalDecimation(bool enable) {
  rtc::CritScope mutex(&mutex_);
  frame_pre_processor_.EnableTemporalDecimation(enable);
}

void VideoProcessingImpl::SetInputFrameResampleMode(
    VideoFrameResampling resampling_mode) {
  rtc::CritScope cs(&mutex_);
  frame_pre_processor_.SetInputFrameResampleMode(resampling_mode);
}

int32_t VideoProcessingImpl::SetTargetResolution(uint32_t width,
                                                 uint32_t height,
                                                 uint32_t frame_rate) {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.SetTargetResolution(width, height, frame_rate);
}

uint32_t VideoProcessingImpl::GetDecimatedFrameRate() {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.GetDecimatedFrameRate();
}

uint32_t VideoProcessingImpl::GetDecimatedWidth() const {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.GetDecimatedWidth();
}

uint32_t VideoProcessingImpl::GetDecimatedHeight() const {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.GetDecimatedHeight();
}

void VideoProcessingImpl::EnableDenoising(bool enable) {
  rtc::CritScope cs(&mutex_);
  frame_pre_processor_.EnableDenoising(enable);
}

const VideoFrame* VideoProcessingImpl::PreprocessFrame(
    const VideoFrame& frame) {
  rtc::CritScope mutex(&mutex_);
  return frame_pre_processor_.PreprocessFrame(frame);
}

}  // namespace webrtc
