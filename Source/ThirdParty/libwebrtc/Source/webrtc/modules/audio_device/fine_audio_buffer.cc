/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/fine_audio_buffer.h"

#include <memory.h>
#include <stdio.h>
#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_device/audio_device_buffer.h"

namespace webrtc {

FineAudioBuffer::FineAudioBuffer(AudioDeviceBuffer* device_buffer,
                                 size_t desired_frame_size_bytes,
                                 int sample_rate)
    : device_buffer_(device_buffer),
      desired_frame_size_bytes_(desired_frame_size_bytes),
      sample_rate_(sample_rate),
      samples_per_10_ms_(static_cast<size_t>(sample_rate_ * 10 / 1000)),
      bytes_per_10_ms_(samples_per_10_ms_ * sizeof(int16_t)),
      playout_cached_buffer_start_(0),
      playout_cached_bytes_(0),
      // Allocate extra space on the recording side to reduce the number of
      // memmove() calls.
      required_record_buffer_size_bytes_(
          5 * (desired_frame_size_bytes + bytes_per_10_ms_)),
      record_cached_bytes_(0),
      record_read_pos_(0),
      record_write_pos_(0) {
  playout_cache_buffer_.reset(new int8_t[bytes_per_10_ms_]);
  record_cache_buffer_.reset(new int8_t[required_record_buffer_size_bytes_]);
  memset(record_cache_buffer_.get(), 0, required_record_buffer_size_bytes_);
}

FineAudioBuffer::~FineAudioBuffer() {}

size_t FineAudioBuffer::RequiredPlayoutBufferSizeBytes() {
  // It is possible that we store the desired frame size - 1 samples. Since new
  // audio frames are pulled in chunks of 10ms we will need a buffer that can
  // hold desired_frame_size - 1 + 10ms of data. We omit the - 1.
  return desired_frame_size_bytes_ + bytes_per_10_ms_;
}

void FineAudioBuffer::ResetPlayout() {
  playout_cached_buffer_start_ = 0;
  playout_cached_bytes_ = 0;
  memset(playout_cache_buffer_.get(), 0, bytes_per_10_ms_);
}

void FineAudioBuffer::ResetRecord() {
  record_cached_bytes_ = 0;
  record_read_pos_ = 0;
  record_write_pos_ = 0;
  memset(record_cache_buffer_.get(), 0, required_record_buffer_size_bytes_);
}

void FineAudioBuffer::GetPlayoutData(int8_t* buffer) {
  if (desired_frame_size_bytes_ <= playout_cached_bytes_) {
    memcpy(buffer, &playout_cache_buffer_.get()[playout_cached_buffer_start_],
           desired_frame_size_bytes_);
    playout_cached_buffer_start_ += desired_frame_size_bytes_;
    playout_cached_bytes_ -= desired_frame_size_bytes_;
    RTC_CHECK_LT(playout_cached_buffer_start_ + playout_cached_bytes_,
                 bytes_per_10_ms_);
    return;
  }
  memcpy(buffer, &playout_cache_buffer_.get()[playout_cached_buffer_start_],
         playout_cached_bytes_);
  // Push another n*10ms of audio to |buffer|. n > 1 if
  // |desired_frame_size_bytes_| is greater than 10ms of audio. Note that we
  // write the audio after the cached bytes copied earlier.
  int8_t* unwritten_buffer = &buffer[playout_cached_bytes_];
  int bytes_left =
      static_cast<int>(desired_frame_size_bytes_ - playout_cached_bytes_);
  // Ceiling of integer division: 1 + ((x - 1) / y)
  size_t number_of_requests = 1 + (bytes_left - 1) / (bytes_per_10_ms_);
  for (size_t i = 0; i < number_of_requests; ++i) {
    device_buffer_->RequestPlayoutData(samples_per_10_ms_);
    int num_out = device_buffer_->GetPlayoutData(unwritten_buffer);
    if (static_cast<size_t>(num_out) != samples_per_10_ms_) {
      RTC_CHECK_EQ(num_out, 0);
      playout_cached_bytes_ = 0;
      return;
    }
    unwritten_buffer += bytes_per_10_ms_;
    RTC_CHECK_GE(bytes_left, 0);
    bytes_left -= static_cast<int>(bytes_per_10_ms_);
  }
  RTC_CHECK_LE(bytes_left, 0);
  // Put the samples that were written to |buffer| but are not used in the
  // cache.
  size_t cache_location = desired_frame_size_bytes_;
  int8_t* cache_ptr = &buffer[cache_location];
  playout_cached_bytes_ = number_of_requests * bytes_per_10_ms_ -
                          (desired_frame_size_bytes_ - playout_cached_bytes_);
  // If playout_cached_bytes_ is larger than the cache buffer, uninitialized
  // memory will be read.
  RTC_CHECK_LE(playout_cached_bytes_, bytes_per_10_ms_);
  RTC_CHECK_EQ(static_cast<size_t>(-bytes_left), playout_cached_bytes_);
  playout_cached_buffer_start_ = 0;
  memcpy(playout_cache_buffer_.get(), cache_ptr, playout_cached_bytes_);
}

void FineAudioBuffer::DeliverRecordedData(const int8_t* buffer,
                                          size_t size_in_bytes,
                                          int playout_delay_ms,
                                          int record_delay_ms) {
  // Check if the temporary buffer can store the incoming buffer. If not,
  // move the remaining (old) bytes to the beginning of the temporary buffer
  // and start adding new samples after the old samples.
  if (record_write_pos_ + size_in_bytes > required_record_buffer_size_bytes_) {
    if (record_cached_bytes_ > 0) {
      memmove(record_cache_buffer_.get(),
              record_cache_buffer_.get() + record_read_pos_,
              record_cached_bytes_);
    }
    record_write_pos_ = record_cached_bytes_;
    record_read_pos_ = 0;
  }
  // Add recorded samples to a temporary buffer.
  memcpy(record_cache_buffer_.get() + record_write_pos_, buffer, size_in_bytes);
  record_write_pos_ += size_in_bytes;
  record_cached_bytes_ += size_in_bytes;
  // Consume samples in temporary buffer in chunks of 10ms until there is not
  // enough data left. The number of remaining bytes in the cache is given by
  // |record_cached_bytes_| after this while loop is done.
  while (record_cached_bytes_ >= bytes_per_10_ms_) {
    device_buffer_->SetRecordedBuffer(
        record_cache_buffer_.get() + record_read_pos_, samples_per_10_ms_);
    device_buffer_->SetVQEData(playout_delay_ms, record_delay_ms, 0);
    device_buffer_->DeliverRecordedData();
    // Read next chunk of 10ms data.
    record_read_pos_ += bytes_per_10_ms_;
    // Reduce number of cached bytes with the consumed amount.
    record_cached_bytes_ -= bytes_per_10_ms_;
  }
}

}  // namespace webrtc
