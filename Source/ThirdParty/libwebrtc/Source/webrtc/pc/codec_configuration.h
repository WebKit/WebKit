/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CODEC_CONFIGURATION_H_
#define PC_CODEC_CONFIGURATION_H_

#include "media/base/codec.h"

namespace webrtc {

// ResiliencyInfo encapsulates the redundancy requirements for a codec.
struct ResiliencyInfo {
  bool rtx = false;
  bool red = false;
  bool ulpfec = false;
  bool flexfec = false;

  bool operator==(const ResiliencyInfo& other) const = default;
};

// CodecConfiguration stores codec attributes and associated resiliency
// requirements. The payload type (id) in the 'codec' member should be ignored.
struct CodecConfiguration {
  Codec codec;
  ResiliencyInfo resiliency;

  bool operator==(const CodecConfiguration& other) const {
    return codec == other.codec && resiliency == other.resiliency;
  }
};

}  // namespace webrtc

#endif  // PC_CODEC_CONFIGURATION_H_
