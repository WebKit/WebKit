/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/audio_multi_vector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/audio_view.h"
#include "modules/audio_coding/neteq/audio_vector.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
std::vector<std::unique_ptr<AudioVector>> InitializeChannelVector(
    size_t num_channels,
    size_t channel_size = 0u) {
  RTC_DCHECK_GT(num_channels, 0u);
  RTC_CHECK_LE(num_channels, kMaxNumberOfAudioChannels);
  std::vector<std::unique_ptr<AudioVector>> channels(num_channels);
  for (auto& c : channels) {
    c = channel_size ? std::make_unique<AudioVector>(channel_size)
                     : std::make_unique<AudioVector>();
  }
  return channels;
}
}  // namespace

AudioMultiVector::AudioMultiVector(size_t N)
    : channels_(InitializeChannelVector(N)) {}

AudioMultiVector::AudioMultiVector(size_t N, size_t initial_size)
    : channels_(InitializeChannelVector(N, initial_size)) {}

AudioMultiVector::~AudioMultiVector() = default;

void AudioMultiVector::Clear() {
  for (auto& c : channels_) {
    c->Clear();
  }
}

void AudioMultiVector::Zeros(size_t length) {
  for (auto& c : channels_) {
    c->Clear();
    c->Extend(length);
  }
}

void AudioMultiVector::CopyTo(AudioMultiVector* copy_to) const {
  if (copy_to) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->CopyTo(&(*copy_to)[i]);
    }
  }
}

void AudioMultiVector::PushBackInterleaved(
    ArrayView<const int16_t> append_this) {
  RTC_DCHECK_EQ(append_this.size() % Channels(), 0);
  if (append_this.empty()) {
    return;
  }
  if (Channels() == 1) {
    // Special case to avoid extra allocation and data shuffling.
    channels_[0]->PushBack(append_this.data(), append_this.size());
    return;
  }
  size_t length_per_channel = append_this.size() / Channels();
  int16_t* temp_array = new int16_t[length_per_channel];  // Temporary storage.
  for (size_t channel = 0; channel < Channels(); ++channel) {
    // Copy elements to `temp_array`.
    for (size_t i = 0; i < length_per_channel; ++i) {
      temp_array[i] = append_this[channel + i * Channels()];
    }
    channels_[channel]->PushBack(temp_array, length_per_channel);
  }
  delete[] temp_array;
}

void AudioMultiVector::PushBack(const AudioMultiVector& append_this) {
  RTC_DCHECK_EQ(Channels(), append_this.Channels());
  if (Channels() == append_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->PushBack(append_this[i]);
    }
  }
}

void AudioMultiVector::PushBackFromIndex(const AudioMultiVector& append_this,
                                         size_t index) {
  RTC_DCHECK_LT(index, append_this.Size());
  index = std::min(index, append_this.Size() - 1);
  size_t length = append_this.Size() - index;
  RTC_DCHECK_EQ(Channels(), append_this.Channels());
  if (Channels() == append_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->PushBack(append_this[i], length, index);
    }
  }
}

void AudioMultiVector::PopFront(size_t length) {
  for (auto& c : channels_) {
    c->PopFront(length);
  }
}

void AudioMultiVector::PopBack(size_t length) {
  for (auto& c : channels_) {
    c->PopBack(length);
  }
}

size_t AudioMultiVector::ReadInterleaved(size_t length,
                                         int16_t* destination) const {
  return ReadInterleavedFromIndex(0, length, destination);
}

size_t AudioMultiVector::ReadInterleavedFromIndex(size_t start_index,
                                                  size_t length,
                                                  int16_t* destination) const {
  RTC_DCHECK(destination);
  size_t index = 0;  // Number of elements written to `destination` so far.
  RTC_DCHECK_LE(start_index, Size());
  start_index = std::min(start_index, Size());
  if (length + start_index > Size()) {
    length = Size() - start_index;
  }
  if (Channels() == 1) {
    // Special case to avoid the nested for loop below.
    (*this)[0].CopyTo(length, start_index, destination);
    return length;
  }
  for (size_t i = 0; i < length; ++i) {
    for (size_t channel = 0; channel < Channels(); ++channel) {
      destination[index] = (*this)[channel][i + start_index];
      ++index;
    }
  }
  return index;
}

bool AudioMultiVector::ReadInterleavedFromIndex(
    const size_t start_index,
    InterleavedView<int16_t> dst) const {
  RTC_DCHECK_EQ(dst.num_channels(), Channels());
  if (start_index + dst.samples_per_channel() > Size()) {
    return false;
  }
  if (Channels() == 1) {
    // Special case to avoid the nested for loop below.
    return channels_[0]->CopyTo(start_index, dst.AsMono());
  }
  size_t index = 0;
  for (size_t i = 0; i < dst.samples_per_channel(); ++i) {
    for (const auto& ch : channels_) {
      dst[index] = (*ch)[i + start_index];
      ++index;
    }
  }
  return true;
}

size_t AudioMultiVector::ReadInterleavedFromEnd(size_t length,
                                                int16_t* destination) const {
  length = std::min(length, Size());  // Cannot read more than Size() elements.
  return ReadInterleavedFromIndex(Size() - length, length, destination);
}

void AudioMultiVector::OverwriteAt(const AudioMultiVector& insert_this,
                                   size_t length,
                                   size_t position) {
  RTC_DCHECK_EQ(Channels(), insert_this.Channels());
  // Cap `length` at the length of `insert_this`.
  RTC_DCHECK_LE(length, insert_this.Size());
  length = std::min(length, insert_this.Size());
  if (Channels() == insert_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->OverwriteAt(insert_this[i], length, position);
    }
  }
}

void AudioMultiVector::CrossFade(const AudioMultiVector& append_this,
                                 size_t fade_length) {
  RTC_DCHECK_EQ(Channels(), append_this.Channels());
  if (Channels() == append_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->CrossFade(append_this[i], fade_length);
    }
  }
}

size_t AudioMultiVector::Channels() const {
  return channels_.size();
}

size_t AudioMultiVector::Size() const {
  RTC_DCHECK(channels_[0]);
  return channels_[0]->Size();
}

void AudioMultiVector::AssertSize(size_t required_size) {
  if (Size() < required_size) {
    size_t extend_length = required_size - Size();
    for (auto& c : channels_) {
      c->Extend(extend_length);
    }
  }
}

bool AudioMultiVector::Empty() const {
  RTC_DCHECK(channels_[0]);
  return channels_[0]->Empty();
}

void AudioMultiVector::CopyChannel(size_t from_channel, size_t to_channel) {
  RTC_DCHECK_LT(from_channel, Channels());
  RTC_DCHECK_LT(to_channel, Channels());
  channels_[from_channel]->CopyTo(channels_[to_channel].get());
}

const AudioVector& AudioMultiVector::operator[](size_t index) const {
  return *(channels_[index]);
}

AudioVector& AudioMultiVector::operator[](size_t index) {
  return *(channels_[index]);
}

}  // namespace webrtc
