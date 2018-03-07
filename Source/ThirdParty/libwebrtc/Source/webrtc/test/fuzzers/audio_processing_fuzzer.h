/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FUZZERS_AUDIO_PROCESSING_FUZZER_H_
#define TEST_FUZZERS_AUDIO_PROCESSING_FUZZER_H_

#include <memory>

#include "modules/audio_processing/include/audio_processing.h"
namespace webrtc {

rtc::Optional<bool> ParseBool(const uint8_t** data, size_t* remaining_size);
rtc::Optional<uint8_t> ParseByte(const uint8_t** data, size_t* remaining_size);

void FuzzAudioProcessing(const uint8_t* data,
                         size_t size,
                         std::unique_ptr<AudioProcessing> apm);
}  // namespace webrtc

#endif  // TEST_FUZZERS_AUDIO_PROCESSING_FUZZER_H_
