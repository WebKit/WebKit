/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/fine_audio_buffer.h"

#include <limits.h>

#include <memory>

#include "api/array_view.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_device/mock_audio_device_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Return;

namespace webrtc {

const int kSampleRate = 44100;
const int kChannels = 2;
const int kSamplesPer10Ms = kSampleRate * 10 / 1000;

// The fake audio data is 0,1,..SCHAR_MAX-1,0,1,... This is to make it easy
// to detect errors. This function verifies that the buffers contain such data.
// E.g. if there are two buffers of size 3, buffer 1 would contain 0,1,2 and
// buffer 2 would contain 3,4,5. Note that SCHAR_MAX is 127 so wrap-around
// will happen.
// |buffer| is the audio buffer to verify.
bool VerifyBuffer(const int16_t* buffer, int buffer_number, int size) {
  int start_value = (buffer_number * size) % SCHAR_MAX;
  for (int i = 0; i < size; ++i) {
    if (buffer[i] != (i + start_value) % SCHAR_MAX) {
      return false;
    }
  }
  return true;
}

// This function replaces the real AudioDeviceBuffer::GetPlayoutData when it's
// called (which is done implicitly when calling GetBufferData). It writes the
// sequence 0,1,..SCHAR_MAX-1,0,1,... to the buffer. Note that this is likely a
// buffer of different size than the one VerifyBuffer verifies.
// |iteration| is the number of calls made to UpdateBuffer prior to this call.
// |samples_per_10_ms| is the number of samples that should be written to the
// buffer (|arg0|).
ACTION_P2(UpdateBuffer, iteration, samples_per_10_ms) {
  int16_t* buffer = static_cast<int16_t*>(arg0);
  int start_value = (iteration * samples_per_10_ms) % SCHAR_MAX;
  for (int i = 0; i < samples_per_10_ms; ++i) {
    buffer[i] = (i + start_value) % SCHAR_MAX;
  }
  // Should return samples per channel.
  return samples_per_10_ms / kChannels;
}

// Writes a periodic ramp pattern to the supplied |buffer|. See UpdateBuffer()
// for details.
void UpdateInputBuffer(int16_t* buffer, int iteration, int size) {
  int start_value = (iteration * size) % SCHAR_MAX;
  for (int i = 0; i < size; ++i) {
    buffer[i] = (i + start_value) % SCHAR_MAX;
  }
}

// Action macro which verifies that the recorded 10ms chunk of audio data
// (in |arg0|) contains the correct reference values even if they have been
// supplied using a buffer size that is smaller or larger than 10ms.
// See VerifyBuffer() for details.
ACTION_P2(VerifyInputBuffer, iteration, samples_per_10_ms) {
  const int16_t* buffer = static_cast<const int16_t*>(arg0);
  int start_value = (iteration * samples_per_10_ms) % SCHAR_MAX;
  for (int i = 0; i < samples_per_10_ms; ++i) {
    EXPECT_EQ(buffer[i], (i + start_value) % SCHAR_MAX);
  }
  return 0;
}

void RunFineBufferTest(int frame_size_in_samples) {
  const int kFrameSizeSamples = frame_size_in_samples;
  const int kNumberOfFrames = 5;
  // Ceiling of integer division: 1 + ((x - 1) / y)
  const int kNumberOfUpdateBufferCalls =
      1 + ((kNumberOfFrames * frame_size_in_samples - 1) / kSamplesPer10Ms);

  auto task_queue_factory = CreateDefaultTaskQueueFactory();
  MockAudioDeviceBuffer audio_device_buffer(task_queue_factory.get());
  audio_device_buffer.SetPlayoutSampleRate(kSampleRate);
  audio_device_buffer.SetPlayoutChannels(kChannels);
  audio_device_buffer.SetRecordingSampleRate(kSampleRate);
  audio_device_buffer.SetRecordingChannels(kChannels);

  EXPECT_CALL(audio_device_buffer, RequestPlayoutData(_))
      .WillRepeatedly(Return(kSamplesPer10Ms));
  {
    InSequence s;
    for (int i = 0; i < kNumberOfUpdateBufferCalls; ++i) {
      EXPECT_CALL(audio_device_buffer, GetPlayoutData(_))
          .WillOnce(UpdateBuffer(i, kChannels * kSamplesPer10Ms))
          .RetiresOnSaturation();
    }
  }
  {
    InSequence s;
    for (int j = 0; j < kNumberOfUpdateBufferCalls - 1; ++j) {
      EXPECT_CALL(audio_device_buffer, SetRecordedBuffer(_, kSamplesPer10Ms))
          .WillOnce(VerifyInputBuffer(j, kChannels * kSamplesPer10Ms))
          .RetiresOnSaturation();
    }
  }
  EXPECT_CALL(audio_device_buffer, SetVQEData(_, _))
      .Times(kNumberOfUpdateBufferCalls - 1);
  EXPECT_CALL(audio_device_buffer, DeliverRecordedData())
      .Times(kNumberOfUpdateBufferCalls - 1)
      .WillRepeatedly(Return(0));

  FineAudioBuffer fine_buffer(&audio_device_buffer);
  std::unique_ptr<int16_t[]> out_buffer(
      new int16_t[kChannels * kFrameSizeSamples]);
  std::unique_ptr<int16_t[]> in_buffer(
      new int16_t[kChannels * kFrameSizeSamples]);

  for (int i = 0; i < kNumberOfFrames; ++i) {
    fine_buffer.GetPlayoutData(
        rtc::ArrayView<int16_t>(out_buffer.get(),
                                kChannels * kFrameSizeSamples),
        0);
    EXPECT_TRUE(
        VerifyBuffer(out_buffer.get(), i, kChannels * kFrameSizeSamples));
    UpdateInputBuffer(in_buffer.get(), i, kChannels * kFrameSizeSamples);
    fine_buffer.DeliverRecordedData(
        rtc::ArrayView<const int16_t>(in_buffer.get(),
                                      kChannels * kFrameSizeSamples),
        0);
  }
}

TEST(FineBufferTest, BufferLessThan10ms) {
  const int kFrameSizeSamples = kSamplesPer10Ms - 50;
  RunFineBufferTest(kFrameSizeSamples);
}

TEST(FineBufferTest, GreaterThan10ms) {
  const int kFrameSizeSamples = kSamplesPer10Ms + 50;
  RunFineBufferTest(kFrameSizeSamples);
}

}  // namespace webrtc
