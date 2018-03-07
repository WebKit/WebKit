/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_NACK_FEC_TABLES_H_
#define MODULES_VIDEO_CODING_NACK_FEC_TABLES_H_

namespace webrtc {

// Table for adjusting FEC rate for NACK/FEC protection method
// Table values are built as a sigmoid function, ranging from 0 to 100, based on
// the HybridNackTH values defined in media_opt_util.h.
const uint16_t VCMNackFecTable[100] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
    1,   2,   2,   2,   3,   3,   4,   5,   6,   7,   9,   10,  12,  15,  18,
    21,  24,  28,  32,  37,  41,  46,  51,  56,  61,  66,  70,  74,  78,  81,
    84,  86,  89,  90,  92,  93,  95,  95,  96,  97,  97,  98,  98,  99,  99,
    99,  99,  99,  99,  100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_NACK_FEC_TABLES_H_
