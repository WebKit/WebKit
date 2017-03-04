/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_AUDIO_CODECS_BUILTIN_AUDIO_DECODER_FACTORY_H_
#define WEBRTC_API_AUDIO_CODECS_BUILTIN_AUDIO_DECODER_FACTORY_H_

#include "webrtc/api/audio_codecs/audio_decoder_factory.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {

// Creates a new factory that can create the built-in types of audio decoders.
// NOTE: This function is still under development and may change without notice.
rtc::scoped_refptr<AudioDecoderFactory> CreateBuiltinAudioDecoderFactory();

}  // namespace webrtc

#endif  // WEBRTC_API_AUDIO_CODECS_BUILTIN_AUDIO_DECODER_FACTORY_H_
