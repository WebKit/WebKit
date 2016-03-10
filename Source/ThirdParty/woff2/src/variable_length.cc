// Copyright 2015 Google Inc. All Rights Reserved.
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
// Helper functions for woff2 variable length types: 255UInt16 and UIntBase128

#include "./variable_length.h"

namespace woff2 {

size_t Size255UShort(uint16_t value) {
  size_t result = 3;
  if (value < 253) {
    result = 1;
  } else if (value < 762) {
    result = 2;
  } else {
    result = 3;
  }
  return result;
}

void Write255UShort(std::vector<uint8_t>* out, int value) {
  if (value < 253) {
    out->push_back(value);
  } else if (value < 506) {
    out->push_back(255);
    out->push_back(value - 253);
  } else if (value < 762) {
    out->push_back(254);
    out->push_back(value - 506);
  } else {
    out->push_back(253);
    out->push_back(value >> 8);
    out->push_back(value & 0xff);
  }
}

void Store255UShort(int val, size_t* offset, uint8_t* dst) {
  std::vector<uint8_t> packed;
  Write255UShort(&packed, val);
  for (uint8_t val : packed) {
    dst[(*offset)++] = val;
  }
}

// Based on section 6.1.1 of MicroType Express draft spec
bool Read255UShort(Buffer* buf, unsigned int* value) {
  static const int kWordCode = 253;
  static const int kOneMoreByteCode2 = 254;
  static const int kOneMoreByteCode1 = 255;
  static const int kLowestUCode = 253;
  uint8_t code = 0;
  if (!buf->ReadU8(&code)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (code == kWordCode) {
    uint16_t result = 0;
    if (!buf->ReadU16(&result)) {
      return FONT_COMPRESSION_FAILURE();
    }
    *value = result;
    return true;
  } else if (code == kOneMoreByteCode1) {
    uint8_t result = 0;
    if (!buf->ReadU8(&result)) {
      return FONT_COMPRESSION_FAILURE();
    }
    *value = result + kLowestUCode;
    return true;
  } else if (code == kOneMoreByteCode2) {
    uint8_t result = 0;
    if (!buf->ReadU8(&result)) {
      return FONT_COMPRESSION_FAILURE();
    }
    *value = result + kLowestUCode * 2;
    return true;
  } else {
    *value = code;
    return true;
  }
}

bool ReadBase128(Buffer* buf, uint32_t* value) {
  uint32_t result = 0;
  for (size_t i = 0; i < 5; ++i) {
    uint8_t code = 0;
    if (!buf->ReadU8(&code)) {
      return FONT_COMPRESSION_FAILURE();
    }
    // Leading zeros are invalid.
    if (i == 0 && code == 0x80) {
      return FONT_COMPRESSION_FAILURE();
    }
    // If any of the top seven bits are set then we're about to overflow.
    if (result & 0xfe000000) {
      return FONT_COMPRESSION_FAILURE();
    }
    result = (result << 7) | (code & 0x7f);
    if ((code & 0x80) == 0) {
      *value = result;
      return true;
    }
  }
  // Make sure not to exceed the size bound
  return FONT_COMPRESSION_FAILURE();
}

size_t Base128Size(size_t n) {
  size_t size = 1;
  for (; n >= 128; n >>= 7) ++size;
  return size;
}

void StoreBase128(size_t len, size_t* offset, uint8_t* dst) {
  size_t size = Base128Size(len);
  for (size_t i = 0; i < size; ++i) {
    int b = static_cast<int>((len >> (7 * (size - i - 1))) & 0x7f);
    if (i < size - 1) {
      b |= 0x80;
    }
    dst[(*offset)++] = b;
  }
}

} // namespace woff2

