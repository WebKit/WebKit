/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP8_HEADER_PARSER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP8_HEADER_PARSER_H_

#include <stdint.h>
#include <stdio.h>

namespace webrtc {

namespace vp8 {

enum {
  MB_FEATURE_TREE_PROBS = 3,
  NUM_MB_SEGMENTS = 4,
  NUM_REF_LF_DELTAS = 4,
  NUM_MODE_LF_DELTAS = 4,
};

typedef struct VP8BitReader VP8BitReader;
struct VP8BitReader {
  // Boolean decoder.
  uint32_t value_;  // Current value.
  uint32_t range_;  // Current range minus 1. In [127, 254] interval.
  int bits_;        // Number of valid bits left.
  // Read buffer.
  const uint8_t* buf_;      // Next byte to be read.
  const uint8_t* buf_end_;  // End of read buffer.
  int eof_;                 // True if input is exhausted.
};

const uint8_t kVP8Log2Range[128] = {
    7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};

// range = ((range - 1) << kVP8Log2Range[range]) + 1
const uint8_t kVP8NewRange[128] = {
    127, 127, 191, 127, 159, 191, 223, 127, 143, 159, 175, 191, 207, 223, 239,
    127, 135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239,
    247, 127, 131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179,
    183, 187, 191, 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239,
    243, 247, 251, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149,
    151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179,
    181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209,
    211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 235, 237, 239,
    241, 243, 245, 247, 249, 251, 253, 127};

// Gets the QP, QP range: [0, 127].
// Returns true on success, false otherwise.
bool GetQp(const uint8_t* buf, size_t length, int* qp);

}  // namespace vp8

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP8_HEADER_PARSER_H_
