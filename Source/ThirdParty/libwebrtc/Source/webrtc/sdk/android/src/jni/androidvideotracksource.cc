/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/androidvideotracksource.h"

#include <utility>

#include "webrtc/base/logging.h"

namespace {
// MediaCodec wants resolution to be divisible by 2.
const int kRequiredResolutionAlignment = 2;
}

namespace webrtc {

AndroidVideoTrackSource::AndroidVideoTrackSource(
    rtc::Thread* signaling_thread,
    JNIEnv* jni,
    jobject j_surface_texture_helper,
    bool is_screencast)
    : AdaptedVideoTrackSource(kRequiredResolutionAlignment),
      signaling_thread_(signaling_thread),
      surface_texture_helper_(
          new rtc::RefCountedObject<webrtc_jni::SurfaceTextureHelper>(
              jni,
              j_surface_texture_helper)),
      is_screencast_(is_screencast) {
  LOG(LS_INFO) << "AndroidVideoTrackSource ctor";
  camera_thread_checker_.DetachFromThread();
}

void AndroidVideoTrackSource::SetState(SourceState state) {
  if (rtc::Thread::Current() != signaling_thread_) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&AndroidVideoTrackSource::SetState, this, state));
    return;
  }

  if (state_ != state) {
    state_ = state;
    FireOnChanged();
  }
}

void AndroidVideoTrackSource::OnByteBufferFrameCaptured(const void* frame_data,
                                                        int length,
                                                        int width,
                                                        int height,
                                                        VideoRotation rotation,
                                                        int64_t timestamp_ns) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());

  int64_t camera_time_us = timestamp_ns / rtc::kNumNanosecsPerMicrosec;
  int64_t translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(camera_time_us, rtc::TimeMicros());

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  if (!AdaptFrame(width, height, camera_time_us, &adapted_width,
                  &adapted_height, &crop_width, &crop_height, &crop_x,
                  &crop_y)) {
    return;
  }

  const uint8_t* y_plane = static_cast<const uint8_t*>(frame_data);
  const uint8_t* uv_plane = y_plane + width * height;
  const int uv_width = (width + 1) / 2;

  RTC_CHECK_GE(length, width * height + 2 * uv_width * ((height + 1) / 2));

  // Can only crop at even pixels.
  crop_x &= ~1;
  crop_y &= ~1;
  // Crop just by modifying pointers.
  y_plane += width * crop_y + crop_x;
  uv_plane += uv_width * crop_y + crop_x;

  rtc::scoped_refptr<I420Buffer> buffer =
      buffer_pool_.CreateBuffer(adapted_width, adapted_height);

  nv12toi420_scaler_.NV12ToI420Scale(
      y_plane, width, uv_plane, uv_width * 2, crop_width, crop_height,
      buffer->MutableDataY(), buffer->StrideY(),
      // Swap U and V, since we have NV21, not NV12.
      buffer->MutableDataV(), buffer->StrideV(), buffer->MutableDataU(),
      buffer->StrideU(), buffer->width(), buffer->height());

  OnFrame(VideoFrame(buffer, rotation, translated_camera_time_us));
}

void AndroidVideoTrackSource::OnTextureFrameCaptured(
    int width,
    int height,
    VideoRotation rotation,
    int64_t timestamp_ns,
    const webrtc_jni::NativeHandleImpl& handle) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());

  int64_t camera_time_us = timestamp_ns / rtc::kNumNanosecsPerMicrosec;
  int64_t translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(camera_time_us, rtc::TimeMicros());

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  if (!AdaptFrame(width, height, camera_time_us, &adapted_width,
                  &adapted_height, &crop_width, &crop_height, &crop_x,
                  &crop_y)) {
    surface_texture_helper_->ReturnTextureFrame();
    return;
  }

  webrtc_jni::Matrix matrix = handle.sampling_matrix;

  matrix.Crop(crop_width / static_cast<float>(width),
              crop_height / static_cast<float>(height),
              crop_x / static_cast<float>(width),
              crop_y / static_cast<float>(height));

  // Note that apply_rotation() may change under our feet, so we should only
  // check once.
  if (apply_rotation()) {
    if (rotation == kVideoRotation_90 || rotation == kVideoRotation_270) {
      std::swap(adapted_width, adapted_height);
    }
    matrix.Rotate(rotation);
    rotation = kVideoRotation_0;
  }

  OnFrame(VideoFrame(
      surface_texture_helper_->CreateTextureFrame(
          adapted_width, adapted_height,
          webrtc_jni::NativeHandleImpl(handle.oes_texture_id, matrix)),
      rotation, translated_camera_time_us));
}

void AndroidVideoTrackSource::OnOutputFormatRequest(int width,
                                                    int height,
                                                    int fps) {
  cricket::VideoFormat format(width, height,
                              cricket::VideoFormat::FpsToInterval(fps), 0);
  video_adapter()->OnOutputFormatRequest(format);
}

}  // namespace webrtc
