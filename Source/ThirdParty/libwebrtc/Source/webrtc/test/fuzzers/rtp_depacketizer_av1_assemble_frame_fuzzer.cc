/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_av1.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "api/array_view.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  std::vector<rtc::ArrayView<const uint8_t>> rtp_payloads;

  // Convert plain array of bytes into array of array bytes.
  test::FuzzDataHelper fuzz_input(rtc::MakeArrayView(data, size));
  while (fuzz_input.CanReadBytes(sizeof(uint16_t))) {
    // In practice one rtp payload can be up to ~1200 - 1500 bytes. Majority
    // of the payload is just copied. To make fuzzing more efficient limit the
    // size of rtp payload to realistic value.
    uint16_t next_size = fuzz_input.Read<uint16_t>() % 1200;
    if (next_size > fuzz_input.BytesLeft()) {
      next_size = fuzz_input.BytesLeft();
    }
    rtp_payloads.push_back(fuzz_input.ReadByteArray(next_size));
  }
  // Run code under test.
  VideoRtpDepacketizerAv1().AssembleFrame(rtp_payloads);
}
}  // namespace webrtc
