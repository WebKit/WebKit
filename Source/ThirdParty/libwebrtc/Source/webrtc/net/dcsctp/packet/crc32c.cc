/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/crc32c.h"

#include <cstdint>
#include <span>

#include "absl/crc/crc32c.h"
#include "absl/numeric/bits.h"
#include "absl/strings/string_view.h"

namespace dcsctp {

uint32_t GenerateCrc32C(std::span<const uint8_t> data) {
  absl::crc32c_t crc32 = absl::ComputeCrc32c(absl::string_view(
      reinterpret_cast<const char*>(data.data()), data.size()));
  return absl::byteswap(static_cast<uint32_t>(crc32));
}
}  // namespace dcsctp
