/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_

#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/swap_queue.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/render_queue_item_verifier.h"

namespace webrtc {

class AudioBuffer;

class EchoControlMobileImpl : public EchoControlMobile {
 public:
  EchoControlMobileImpl(rtc::CriticalSection* crit_render,
                        rtc::CriticalSection* crit_capture);

  ~EchoControlMobileImpl() override;

  void ProcessRenderAudio(rtc::ArrayView<const int16_t> packed_render_audio);
  int ProcessCaptureAudio(AudioBuffer* audio, int stream_delay_ms);

  // EchoControlMobile implementation.
  bool is_enabled() const override;
  RoutingMode routing_mode() const override;
  bool is_comfort_noise_enabled() const override;

  void Initialize(int sample_rate_hz,
                  size_t num_reverse_channels,
                  size_t num_output_channels);

  static void PackRenderAudioBuffer(const AudioBuffer* audio,
                                    size_t num_output_channels,
                                    size_t num_channels,
                                    std::vector<int16_t>* packed_buffer);

  static size_t NumCancellersRequired(size_t num_output_channels,
                                      size_t num_reverse_channels);

 private:
  class Canceller;
  struct StreamProperties;

  // EchoControlMobile implementation.
  int Enable(bool enable) override;
  int set_routing_mode(RoutingMode mode) override;
  int enable_comfort_noise(bool enable) override;
  int SetEchoPath(const void* echo_path, size_t size_bytes) override;
  int GetEchoPath(void* echo_path, size_t size_bytes) const override;

  int Configure();

  rtc::CriticalSection* const crit_render_ ACQUIRED_BEFORE(crit_capture_);
  rtc::CriticalSection* const crit_capture_;

  bool enabled_ = false;

  RoutingMode routing_mode_ GUARDED_BY(crit_capture_);
  bool comfort_noise_enabled_ GUARDED_BY(crit_capture_);
  unsigned char* external_echo_path_ GUARDED_BY(crit_render_)
      GUARDED_BY(crit_capture_);

  std::vector<std::unique_ptr<Canceller>> cancellers_;
  std::unique_ptr<StreamProperties> stream_properties_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoControlMobileImpl);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
