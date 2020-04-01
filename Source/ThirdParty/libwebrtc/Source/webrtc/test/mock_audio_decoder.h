/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_MOCK_AUDIO_DECODER_H_
#define TEST_MOCK_AUDIO_DECODER_H_

#include "api/audio_codecs/audio_decoder.h"
#include "test/gmock.h"

namespace webrtc {

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();
  ~MockAudioDecoder();
  MOCK_METHOD0(Die, void());
  MOCK_METHOD5(DecodeInternal,
               int(const uint8_t*, size_t, int, int16_t*, SpeechType*));
  MOCK_CONST_METHOD0(HasDecodePlc, bool());
  MOCK_METHOD2(DecodePlc, size_t(size_t, int16_t*));
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD0(ErrorCode, int());
  MOCK_CONST_METHOD2(PacketDuration, int(const uint8_t*, size_t));
  MOCK_CONST_METHOD0(Channels, size_t());
  MOCK_CONST_METHOD0(SampleRateHz, int());
};

}  // namespace webrtc
#endif  // TEST_MOCK_AUDIO_DECODER_H_
