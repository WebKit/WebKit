/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
#define MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_

#include <memory>
#include <vector>

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/render_queue_item_verifier.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/swap_queue.h"

namespace webrtc {

class AudioBuffer;

// The acoustic echo control for mobile (AECM) component is a low complexity
// robust option intended for use on mobile devices.
class EchoControlMobileImpl {
 public:
  EchoControlMobileImpl(rtc::CriticalSection* crit_render,
                        rtc::CriticalSection* crit_capture);

  ~EchoControlMobileImpl();

  int Enable(bool enable);
  bool is_enabled() const;

  // Recommended settings for particular audio routes. In general, the louder
  // the echo is expected to be, the higher this value should be set. The
  // preferred setting may vary from device to device.
  enum RoutingMode {
    kQuietEarpieceOrHeadset,
    kEarpiece,
    kLoudEarpiece,
    kSpeakerphone,
    kLoudSpeakerphone
  };

  // Sets echo control appropriate for the audio routing |mode| on the device.
  // It can and should be updated during a call if the audio routing changes.
  int set_routing_mode(RoutingMode mode);
  RoutingMode routing_mode() const;

  // Comfort noise replaces suppressed background noise to maintain a
  // consistent signal level.
  int enable_comfort_noise(bool enable);
  bool is_comfort_noise_enabled() const;

  // A typical use case is to initialize the component with an echo path from a
  // previous call. The echo path is retrieved using |GetEchoPath()|, typically
  // at the end of a call. The data can then be stored for later use as an
  // initializer before the next call, using |SetEchoPath()|.
  //
  // Controlling the echo path this way requires the data |size_bytes| to match
  // the internal echo path size. This size can be acquired using
  // |echo_path_size_bytes()|. |SetEchoPath()| causes an entire reset, worth
  // noting if it is to be called during an ongoing call.
  //
  // It is possible that version incompatibilities may result in a stored echo
  // path of the incorrect size. In this case, the stored path should be
  // discarded.
  int SetEchoPath(const void* echo_path, size_t size_bytes);
  int GetEchoPath(void* echo_path, size_t size_bytes) const;

  // The returned path size is guaranteed not to change for the lifetime of
  // the application.
  static size_t echo_path_size_bytes();

  void ProcessRenderAudio(rtc::ArrayView<const int16_t> packed_render_audio);
  int ProcessCaptureAudio(AudioBuffer* audio, int stream_delay_ms);

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

  int Configure();

  rtc::CriticalSection* const crit_render_ RTC_ACQUIRED_BEFORE(crit_capture_);
  rtc::CriticalSection* const crit_capture_;

  bool enabled_ = false;

  RoutingMode routing_mode_ RTC_GUARDED_BY(crit_capture_);
  bool comfort_noise_enabled_ RTC_GUARDED_BY(crit_capture_);
  unsigned char* external_echo_path_ RTC_GUARDED_BY(crit_render_)
      RTC_GUARDED_BY(crit_capture_);

  std::vector<std::unique_ptr<Canceller>> cancellers_;
  std::unique_ptr<StreamProperties> stream_properties_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoControlMobileImpl);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
