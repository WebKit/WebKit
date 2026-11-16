/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_ENCODED_AUDIO_FRAME_INJECTOR_H_
#define PC_ENCODED_AUDIO_FRAME_INJECTOR_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>

#include "absl/base/nullability.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/encoded_audio_frame_injector_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class ProxyAudioTrack;
class ProxyAudioEncoder;
class TaskQueueBase;

class RTC_EXPORT EncodedAudioFrameInjector
    : public EncodedAudioFrameInjectorInterface {
 public:
  static scoped_refptr<EncodedAudioFrameInjector> Create(
      TargetBitrateCallback bitrate_callback,
      TaskQueueBase* absl_nonnull worker_thread);

  // EncodedAudioFrameInjectorInterface implementation
  void InjectFrame(
      std::unique_ptr<TransformableAudioFrameInterface> encoded_frame) override;

  // Public methods to expose track and factory to RtpSenderBase
  scoped_refptr<AudioTrackInterface> GetAudioTrack();
  absl_nonnull scoped_refptr<AudioEncoderFactory> CreateEncoderFactory();

  // Methods called by ProxyAudioEncoder
  void InvokeBitrateInfoCallback(int32_t allocated_bitrate);
  std::unique_ptr<TransformableAudioFrameInterface> GetNextFrame();

 protected:
  explicit EncodedAudioFrameInjector(TargetBitrateCallback bitrate_callback,
                                     TaskQueueBase* absl_nonnull worker_thread);
  ~EncodedAudioFrameInjector() override;

 private:
  const scoped_refptr<ProxyAudioTrack> audio_track_;

  Mutex buffered_frames_lock_;
  std::deque<std::unique_ptr<TransformableAudioFrameInterface>> buffered_frames_
      RTC_GUARDED_BY(buffered_frames_lock_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker signaling_sequence_checker_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker encoder_sequence_checker_{
      SequenceChecker::kDetached};

  const TargetBitrateCallback bitrate_callback_;
};

}  // namespace webrtc

#endif  // PC_ENCODED_AUDIO_FRAME_INJECTOR_H_
