/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_OBJC_CODEC_H264_TEST_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_OBJC_CODEC_H264_TEST_H_

#include <memory>

#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"

namespace webrtc {

std::unique_ptr<cricket::WebRtcVideoEncoderFactory> CreateObjCEncoderFactory();
std::unique_ptr<cricket::WebRtcVideoDecoderFactory> CreateObjCDecoderFactory();

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_OBJC_CODEC_H264_TEST_H_
