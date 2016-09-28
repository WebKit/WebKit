// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Helpers common across multiple parts of woff2

#include <algorithm>

#include "./woff2_common.h"

namespace woff2 {


uint32_t ComputeULongSum(const uint8_t* buf, size_t size) {
  uint32_t checksum = 0;
  size_t aligned_size = size & ~3;
  for (size_t i = 0; i < aligned_size; i += 4) {
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
    uint32_t v = *reinterpret_cast<const uint32_t*>(buf + i);
    checksum += (((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
      ((v & 0xFF0000) >> 8) | ((v & 0xFF000000) >> 24));
#elif (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    checksum += *reinterpret_cast<const uint32_t*>(buf + i);
#else
    checksum += (buf[i] << 24) | (buf[i + 1] << 16) |
      (buf[i + 2] << 8) | buf[i + 3];
#endif
  }

  // treat size not aligned on 4 as if it were padded to 4 with 0's
  if (size != aligned_size) {
    uint32_t v = 0;
    for (size_t i = aligned_size; i < size; ++i) {
      v |= buf[i] << (24 - 8 * (i & 3));
    }
    checksum += v;
  }

  return checksum;
}

size_t CollectionHeaderSize(uint32_t header_version, uint32_t num_fonts) {
  size_t size = 0;
  if (header_version == 0x00020000) {
    size += 12;  // ulDsig{Tag,Length,Offset}
  }
  if (header_version == 0x00010000 || header_version == 0x00020000) {
    size += 12   // TTCTag, Version, numFonts
      + 4 * num_fonts;  // OffsetTable[numFonts]
  }
  return size;
}

} // namespace woff2
