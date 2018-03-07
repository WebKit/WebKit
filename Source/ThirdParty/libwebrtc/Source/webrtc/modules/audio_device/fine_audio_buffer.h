/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_
#define MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_

#include <memory>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class AudioDeviceBuffer;

// FineAudioBuffer takes an AudioDeviceBuffer (ADB) which deals with audio data
// corresponding to 10ms of data. It then allows for this data to be pulled in
// a finer or coarser granularity. I.e. interacting with this class instead of
// directly with the AudioDeviceBuffer one can ask for any number of audio data
// samples. This class also ensures that audio data can be delivered to the ADB
// in 10ms chunks when the size of the provided audio buffers differs from 10ms.
// As an example: calling DeliverRecordedData() with 5ms buffers will deliver
// accumulated 10ms worth of data to the ADB every second call.
// TODO(henrika): add support for stereo when mobile platforms need it.
class FineAudioBuffer {
 public:
  // |device_buffer| is a buffer that provides 10ms of audio data.
  // |sample_rate| is the sample rate of the audio data. This is needed because
  // |device_buffer| delivers 10ms of data. Given the sample rate the number
  // of samples can be calculated. The |capacity| ensures that the buffer size
  // can be increased to at least capacity without further reallocation.
  FineAudioBuffer(AudioDeviceBuffer* device_buffer,
                  int sample_rate,
                  size_t capacity);
  ~FineAudioBuffer();

  // Clears buffers and counters dealing with playour and/or recording.
  void ResetPlayout();
  void ResetRecord();

  // Copies audio samples into |audio_buffer| where number of requested
  // elements is specified by |audio_buffer.size()|. The producer will always
  // fill up the audio buffer and if no audio exists, the buffer will contain
  // silence instead.
  void GetPlayoutData(rtc::ArrayView<int8_t> audio_buffer);

  // Consumes the audio data in |audio_buffer| and sends it to the WebRTC layer
  // in chunks of 10ms. The provided delay estimates in |playout_delay_ms| and
  // |record_delay_ms| are given to the AEC in the audio processing module.
  // They can be fixed values on most platforms and they are ignored if an
  // external (hardware/built-in) AEC is used.
  // Example: buffer size is 5ms => call #1 stores 5ms of data, call #2 stores
  // 5ms of data and sends a total of 10ms to WebRTC and clears the intenal
  // cache. Call #3 restarts the scheme above.
  void DeliverRecordedData(rtc::ArrayView<const int8_t> audio_buffer,
                           int playout_delay_ms,
                           int record_delay_ms);

 private:
  // Device buffer that works with 10ms chunks of data both for playout and
  // for recording. I.e., the WebRTC side will always be asked for audio to be
  // played out in 10ms chunks and recorded audio will be sent to WebRTC in
  // 10ms chunks as well. This pointer is owned by the constructor of this
  // class and the owner must ensure that the pointer is valid during the life-
  // time of this object.
  AudioDeviceBuffer* const device_buffer_;
  // Sample rate in Hertz.
  const int sample_rate_;
  // Number of audio samples per 10ms.
  const size_t samples_per_10_ms_;
  // Number of audio bytes per 10ms.
  const size_t bytes_per_10_ms_;
  // Storage for output samples from which a consumer can read audio buffers
  // in any size using GetPlayoutData().
  rtc::BufferT<int8_t> playout_buffer_;
  // Storage for input samples that are about to be delivered to the WebRTC
  // ADB or remains from the last successful delivery of a 10ms audio buffer.
  rtc::BufferT<int8_t> record_buffer_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_
