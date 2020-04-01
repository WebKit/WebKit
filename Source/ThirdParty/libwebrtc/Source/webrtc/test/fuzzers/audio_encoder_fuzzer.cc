/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/audio_encoder_fuzzer.h"

#include <cstring>

#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

// This function reads bytes from |data_view|, interprets them as RTP timestamp
// and input samples, and sends them for encoding. The process continues until
// no more data is available.
void FuzzAudioEncoder(rtc::ArrayView<const uint8_t> data_view,
                      std::unique_ptr<AudioEncoder> encoder) {
  test::FuzzDataHelper data(data_view);
  const size_t block_size_samples =
      encoder->SampleRateHz() / 100 * encoder->NumChannels();
  const size_t block_size_bytes = block_size_samples * sizeof(int16_t);
  if (data_view.size() / block_size_bytes > 1000) {
    // If the size of the fuzzer data is more than 1000 input blocks (i.e., more
    // than 10 seconds), then don't fuzz at all for the fear of timing out.
    return;
  }

  rtc::BufferT<int16_t> input_aligned(block_size_samples);
  rtc::Buffer encoded;

  // Each round in the loop below will need one block of samples + a 32-bit
  // timestamp from the fuzzer input.
  const size_t bytes_to_read = block_size_bytes + sizeof(uint32_t);
  while (data.CanReadBytes(bytes_to_read)) {
    const uint32_t timestamp = data.Read<uint32_t>();
    auto byte_array = data.ReadByteArray(block_size_bytes);
    // Align the data by copying to another array.
    RTC_DCHECK_EQ(input_aligned.size() * sizeof(int16_t),
                  byte_array.size() * sizeof(uint8_t));
    memcpy(input_aligned.data(), byte_array.data(), byte_array.size());
    auto info = encoder->Encode(timestamp, input_aligned, &encoded);
  }
}

}  // namespace webrtc
