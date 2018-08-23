/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_CREATE_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_CREATE_H_

#include <map>
#include <memory>
#include <utility>

#include "api/video/video_stream_encoder_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video/video_stream_encoder_settings.h"

namespace webrtc {

std::unique_ptr<VideoStreamEncoderInterface> CreateVideoStreamEncoder(
    uint32_t number_of_cores,
    VideoStreamEncoderObserver* encoder_stats_observer,
    const VideoStreamEncoderSettings& settings,
    // Deprecated, used for tests only.
    rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback);

inline std::unique_ptr<VideoStreamEncoderInterface> CreateVideoStreamEncoder(
    uint32_t number_of_cores,
    VideoStreamEncoderObserver* encoder_stats_observer,
    const VideoStreamEncoderSettings& settings) {
  return CreateVideoStreamEncoder(number_of_cores, encoder_stats_observer,
                                  settings, nullptr);
}

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_CREATE_H_
