/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_DECODER_CREATE_H_
#define API_VIDEO_VIDEO_STREAM_DECODER_CREATE_H_

#include <map>
#include <memory>
#include <utility>

#include "api/video/video_stream_decoder.h"

namespace webrtc {
// The |decoder_settings| parameter is a map between:
// <payload type> -->  <<video format>, <number of cores>>.
// The video format is used when instantiating a decoder, and
// the number of cores is used when initializing the decoder.
std::unique_ptr<VideoStreamDecoder> CreateVideoStreamDecoder(
    VideoStreamDecoder::Callbacks* callbacks,
    VideoDecoderFactory* decoder_factory,
    std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings);

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_DECODER_CREATE_H_
