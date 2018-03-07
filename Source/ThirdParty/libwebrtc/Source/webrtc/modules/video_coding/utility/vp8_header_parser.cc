/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/utility/vp8_header_parser.h"

#include "rtc_base/logging.h"

namespace webrtc {

namespace vp8 {
namespace {
const size_t kCommonPayloadHeaderLength = 3;
const size_t kKeyPayloadHeaderLength = 10;
}  // namespace

static uint32_t BSwap32(uint32_t x) {
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

static void VP8LoadFinalBytes(VP8BitReader* const br) {
  // Only read 8bits at a time.
  if (br->buf_ < br->buf_end_) {
    br->bits_ += 8;
    br->value_ = static_cast<uint32_t>(*br->buf_++) | (br->value_ << 8);
  } else if (!br->eof_) {
    br->value_ <<= 8;
    br->bits_ += 8;
    br->eof_ = 1;
  }
}

static void VP8LoadNewBytes(VP8BitReader* const br) {
  int BITS = 24;
  // Read 'BITS' bits at a time.
  if (br->buf_ + sizeof(uint32_t) <= br->buf_end_) {
    uint32_t bits;
    const uint32_t in_bits = *(const uint32_t*)(br->buf_);
    br->buf_ += BITS >> 3;
#if defined(WEBRTC_ARCH_BIG_ENDIAN)
    bits = static_cast<uint32_t>(in_bits);
    if (BITS != 8 * sizeof(uint32_t))
      bits >>= (8 * sizeof(uint32_t) - BITS);
#else
    bits = BSwap32(in_bits);
    bits >>= 32 - BITS;
#endif
    br->value_ = bits | (br->value_ << BITS);
    br->bits_ += BITS;
  } else {
    VP8LoadFinalBytes(br);
  }
}

static void VP8InitBitReader(VP8BitReader* const br,
                             const uint8_t* const start,
                             const uint8_t* const end) {
  br->range_ = 255 - 1;
  br->buf_ = start;
  br->buf_end_ = end;
  br->value_ = 0;
  br->bits_ = -8;  // To load the very first 8bits.
  br->eof_ = 0;
  VP8LoadNewBytes(br);
}

// Read a bit with proba 'prob'.
static int VP8GetBit(VP8BitReader* const br, int prob) {
  uint8_t range = br->range_;
  if (br->bits_ < 0) {
    VP8LoadNewBytes(br);
    if (br->eof_)
      return 0;
  }
  const int pos = br->bits_;
  const uint8_t split = (range * prob) >> 8;
  const uint8_t value = static_cast<uint8_t>(br->value_ >> pos);
  int bit;
  if (value > split) {
    range -= split + 1;
    br->value_ -= static_cast<uint32_t>(split + 1) << pos;
    bit = 1;
  } else {
    range = split;
    bit = 0;
  }
  if (range <= static_cast<uint8_t>(0x7e)) {
    const int shift = kVP8Log2Range[range];
    range = kVP8NewRange[range];
    br->bits_ -= shift;
  }
  br->range_ = range;
  return bit;
}

static uint32_t VP8GetValue(VP8BitReader* const br, int bits) {
  uint32_t v = 0;
  while (bits-- > 0) {
    v |= VP8GetBit(br, 0x80) << bits;
  }
  return v;
}

static uint32_t VP8Get(VP8BitReader* const br) {
  return VP8GetValue(br, 1);
}

static int32_t VP8GetSignedValue(VP8BitReader* const br, int bits) {
  const int value = VP8GetValue(br, bits);
  return VP8Get(br) ? -value : value;
}

static void ParseSegmentHeader(VP8BitReader* br) {
  int use_segment = VP8Get(br);
  if (use_segment) {
    int update_map = VP8Get(br);
    if (VP8Get(br)) {
      int s;
      VP8Get(br);
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        VP8Get(br) ? VP8GetSignedValue(br, 7) : 0;
      }
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        VP8Get(br) ? VP8GetSignedValue(br, 6) : 0;
      }
    }
    if (update_map) {
      int s;
      for (s = 0; s < MB_FEATURE_TREE_PROBS; ++s) {
        VP8Get(br) ? VP8GetValue(br, 8) : 255;
      }
    }
  }
}

static void ParseFilterHeader(VP8BitReader* br) {
  VP8Get(br);
  VP8GetValue(br, 6);
  VP8GetValue(br, 3);
  int use_lf_delta = VP8Get(br);
  if (use_lf_delta) {
    if (VP8Get(br)) {
      int i;
      for (i = 0; i < NUM_REF_LF_DELTAS; ++i) {
        if (VP8Get(br)) {
          VP8GetSignedValue(br, 6);
        }
      }
      for (i = 0; i < NUM_MODE_LF_DELTAS; ++i) {
        if (VP8Get(br)) {
          VP8GetSignedValue(br, 6);
        }
      }
    }
  }
}

bool GetQp(const uint8_t* buf, size_t length, int* qp) {
  if (length < kCommonPayloadHeaderLength) {
    RTC_LOG(LS_WARNING) << "Failed to get QP, invalid length.";
    return false;
  }
  VP8BitReader br;
  const uint32_t bits = buf[0] | (buf[1] << 8) | (buf[2] << 16);
  int key_frame = !(bits & 1);
  // Size of first partition in bytes.
  uint32_t partition_length = (bits >> 5);
  size_t header_length = kCommonPayloadHeaderLength;
  if (key_frame) {
    header_length = kKeyPayloadHeaderLength;
  }
  if (header_length + partition_length > length) {
    RTC_LOG(LS_WARNING) << "Failed to get QP, invalid length: " << length;
    return false;
  }
  buf += header_length;

  VP8InitBitReader(&br, buf, buf + partition_length);
  if (key_frame) {
    // Color space and pixel type.
    VP8Get(&br);
    VP8Get(&br);
  }
  ParseSegmentHeader(&br);
  ParseFilterHeader(&br);
  // Number of coefficient data partitions.
  VP8GetValue(&br, 2);
  // Base QP.
  const int base_q0 = VP8GetValue(&br, 7);
  if (br.eof_ == 1) {
    RTC_LOG(LS_WARNING) << "Failed to get QP, end of file reached.";
    return false;
  }
  *qp = base_q0;
  return true;
}

}  // namespace vp8

}  // namespace webrtc
