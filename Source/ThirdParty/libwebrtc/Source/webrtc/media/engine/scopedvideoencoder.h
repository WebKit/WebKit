/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_SCOPEDVIDEOENCODER_H_
#define MEDIA_ENGINE_SCOPEDVIDEOENCODER_H_

#include <memory>

#include "media/engine/webrtcvideoencoderfactory.h"

namespace cricket {

// Helper function that creates a webrtc::VideoEncoder held by an
// std::unique_ptr instead of having to be deleted through
// WebRtcVideoEncoderFactory::DestroyVideoEncoder. The factory passed in must
// outlive the returned encoder.
// TODO(magjed): This helper function will be deleted once
// cricket::WebRtcVideoEncoderFactory is deprecated, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=7925 for more info.
std::unique_ptr<webrtc::VideoEncoder> CreateScopedVideoEncoder(
    cricket::WebRtcVideoEncoderFactory* factory,
    const VideoCodec& codec);

}  // namespace cricket

#endif  // MEDIA_ENGINE_SCOPEDVIDEOENCODER_H_
