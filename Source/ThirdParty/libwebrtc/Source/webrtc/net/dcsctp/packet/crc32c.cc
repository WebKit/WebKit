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

#if !defined(WEBRTC_WEBKIT_BUILD)
#include "third_party/crc32c/src/include/crc32c/crc32c.h"
#else
extern "C" uint32_t calculate_crc32c(uint32_t, const unsigned char*, unsigned int);
extern "C" uint32_t sctp_finalize_crc32c(uint32_t);
#endif

namespace dcsctp {

uint32_t GenerateCrc32C(rtc::ArrayView<const uint8_t> data) {
#if !defined(WEBRTC_WEBKIT_BUILD)
  uint32_t crc32c = crc32c_value(data.data(), data.size());
#else
  uint32_t crc32c = calculate_crc32c(0xffffffff, const_cast<uint8_t*>(data.data()), (unsigned int)data.size());
  crc32c = sctp_finalize_crc32c(crc32c);
#endif

  // Byte swapping for little endian byte order:
  uint8_t byte0 = crc32c;
  uint8_t byte1 = crc32c >> 8;
  uint8_t byte2 = crc32c >> 16;
  uint8_t byte3 = crc32c >> 24;
  crc32c = ((byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3);
  return crc32c;
}
}  // namespace dcsctp
