/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "api/array_view.h"
#include "modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "rtc_base/buffer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace test {
namespace {

void FuzzOneInputTest(webrtc::ArrayView<const uint8_t> data) {
  FuzzDataHelper fuzz_data(data);
  ComfortNoiseDecoder cng_decoder;

  while (1) {
    if (!fuzz_data.CanReadBytes(1))
      break;
    const uint8_t sid_frame_len = fuzz_data.Read<uint8_t>();
    auto sid_frame = fuzz_data.ReadByteArray(sid_frame_len);
    if (sid_frame.empty())
      break;
    cng_decoder.UpdateSid(sid_frame);
    if (!fuzz_data.CanReadBytes(3))
      break;
    constexpr bool kTrueOrFalse[] = {true, false};
    const bool new_period = fuzz_data.SelectOneOf(kTrueOrFalse);
    constexpr size_t kOutputSizes[] = {80, 160, 320, 480};
    const size_t output_size = fuzz_data.SelectOneOf(kOutputSizes);
    const size_t num_generate_calls =
        std::min(fuzz_data.Read<uint8_t>(), static_cast<uint8_t>(17));
    BufferT<int16_t> output = BufferT<int16_t>::CreateWithCapacity(output_size);
    for (size_t i = 0; i < num_generate_calls; ++i) {
      output.SetSize(0);
      output.AppendData(output_size, [&](ArrayView<int16_t> out) {
        cng_decoder.Generate(out, new_period);
        return output_size;
      });
    }
  }
}

}  // namespace
}  // namespace test

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 5000) {
    return;
  }
  test::FuzzOneInputTest(webrtc::ArrayView<const uint8_t>(data, size));
}

}  // namespace webrtc
