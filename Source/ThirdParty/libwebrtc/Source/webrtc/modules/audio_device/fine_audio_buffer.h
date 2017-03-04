/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_

#include <memory>

#include "webrtc/base/buffer.h"
#include "webrtc/typedefs.h"

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
class FineAudioBuffer {
 public:
  // |device_buffer| is a buffer that provides 10ms of audio data.
  // |desired_frame_size_bytes| is the number of bytes of audio data
  // GetPlayoutData() should return on success. It is also the required size of
  // each recorded buffer used in DeliverRecordedData() calls.
  // |sample_rate| is the sample rate of the audio data. This is needed because
  // |device_buffer| delivers 10ms of data. Given the sample rate the number
  // of samples can be calculated.
  FineAudioBuffer(AudioDeviceBuffer* device_buffer,
                  size_t desired_frame_size_bytes,
                  int sample_rate);
  ~FineAudioBuffer();

  // Clears buffers and counters dealing with playour and/or recording.
  void ResetPlayout();
  void ResetRecord();

  // |buffer| must be of equal or greater size than what is returned by
  // RequiredBufferSize(). This is to avoid unnecessary memcpy.
  void GetPlayoutData(int8_t* buffer);

  // Consumes the audio data in |buffer| and sends it to the WebRTC layer in
  // chunks of 10ms. The provided delay estimates in |playout_delay_ms| and
  // |record_delay_ms| are given to the AEC in the audio processing module.
  // They can be fixed values on most platforms and they are ignored if an
  // external (hardware/built-in) AEC is used.
  // The size of |buffer| is given by |size_in_bytes| and must be equal to
  // |desired_frame_size_bytes_|.
  // Example: buffer size is 5ms => call #1 stores 5ms of data, call #2 stores
  // 5ms of data and sends a total of 10ms to WebRTC and clears the intenal
  // cache. Call #3 restarts the scheme above.
  void DeliverRecordedData(const int8_t* buffer,
                           size_t size_in_bytes,
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
  // Number of bytes delivered by GetPlayoutData() call and provided to
  // DeliverRecordedData().
  const size_t desired_frame_size_bytes_;
  // Sample rate in Hertz.
  const int sample_rate_;
  // Number of audio samples per 10ms.
  const size_t samples_per_10_ms_;
  // Number of audio bytes per 10ms.
  const size_t bytes_per_10_ms_;
  rtc::BufferT<int8_t> playout_buffer_;
  // Storage for input samples that are about to be delivered to the WebRTC
  // ADB or remains from the last successful delivery of a 10ms audio buffer.
  rtc::BufferT<int8_t> record_buffer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_
