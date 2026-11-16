/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_ENCODED_VIDEO_FRAME_INJECTOR_H_
#define PC_ENCODED_VIDEO_FRAME_INJECTOR_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>

#include "absl/base/nullability.h"
#include "api/encoded_video_frame_injector_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class ProxyVideoTrack;
class ProxyVideoEncoder;
class ProxyVideoEncoderFactory;
class TaskQueueBase;

class RTC_EXPORT EncodedVideoFrameInjector
    : public EncodedVideoFrameInjectorInterface {
 public:
  static scoped_refptr<EncodedVideoFrameInjector> Create(
      KeyFrameCallback keyframe_callback,
      BitrateInfoCallback bitrate_callback,
      TaskQueueBase* absl_nonnull worker_thread);

  // VideoFrameInjectorInterface implementation
  void InjectFrame(
      std::unique_ptr<TransformableVideoFrameInterface> encoded_frame) override;

  // Public methods to expose the track and factory to RtpSenderBase
  scoped_refptr<VideoTrackInterface> GetVideoTrack();
  absl_nonnull std::unique_ptr<VideoEncoderFactory> CreateEncoderFactory();

  // Methods called by ProxyVideoEncoder
  void RegisterEncoder(ProxyVideoEncoder* encoder);
  void InvokeKeyFrameCallback();
  void InvokeBitrateInfoCallback(int32_t allocated_bitrate,
                                 int32_t available_outgoing_bitrate);
  void UnregisterEncoder(ProxyVideoEncoder* encoder);

 protected:
  EncodedVideoFrameInjector(KeyFrameCallback keyframe_callback,
                            BitrateInfoCallback bitrate_callback,
                            TaskQueueBase* absl_nonnull worker_thread);
  ~EncodedVideoFrameInjector() override;

 private:
  const scoped_refptr<ProxyVideoTrack> video_track_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker signaling_sequence_checker_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker encoder_sequence_checker_{
      SequenceChecker::kDetached};

  KeyFrameCallback keyframe_callback_;
  BitrateInfoCallback bitrate_callback_;

  Mutex encoder_lock_;
  // Safe raw pointer, set to nullptr when the encoder is destroyed
  // via UnregisterEncoder in ProxyVideoEncoder destructor.
  ProxyVideoEncoder* encoder_ RTC_GUARDED_BY(encoder_lock_) = nullptr;
  std::deque<std::unique_ptr<TransformableVideoFrameInterface>> buffered_frames_
      RTC_GUARDED_BY(encoder_lock_);
  static constexpr size_t kMaxBufferedFrames = 20;
};

}  // namespace webrtc

#endif  // PC_ENCODED_VIDEO_FRAME_INJECTOR_H_
