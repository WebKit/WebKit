/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_CONVERSION_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_CONVERSION_H_

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/audio_format.h"

namespace webrtc {

SdpAudioFormat CodecInstToSdp(const CodecInst& codec_inst);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_CONVERSION_H_
