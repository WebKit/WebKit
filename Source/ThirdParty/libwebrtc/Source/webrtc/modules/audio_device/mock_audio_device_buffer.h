/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_MOCK_AUDIO_DEVICE_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_MOCK_AUDIO_DEVICE_BUFFER_H_

#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockAudioDeviceBuffer : public AudioDeviceBuffer {
 public:
  MockAudioDeviceBuffer() {}
  virtual ~MockAudioDeviceBuffer() {}
  MOCK_METHOD1(RequestPlayoutData, int32_t(size_t nSamples));
  MOCK_METHOD1(GetPlayoutData, int32_t(void* audioBuffer));
  MOCK_METHOD2(SetRecordedBuffer,
               int32_t(const void* audioBuffer, size_t nSamples));
  MOCK_METHOD3(SetVQEData,
               void(int playDelayMS, int recDelayMS, int clockDrift));
  MOCK_METHOD0(DeliverRecordedData, int32_t());
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_MOCK_AUDIO_DEVICE_BUFFER_H_
