/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_CONVERSION_H_
#define MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_CONVERSION_H_

#include "api/audio_codecs/audio_format.h"
#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

SdpAudioFormat CodecInstToSdp(const CodecInst& codec_inst);
CodecInst SdpToCodecInst(int payload_type, const SdpAudioFormat& audio_format);

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_CONVERSION_H_
