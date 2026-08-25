/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FRAME_TRANSFORMER_FACTORY_H_
#define API_FRAME_TRANSFORMER_FACTORY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/payload_type.h"
#include "rtc_base/system/rtc_export.h"

// This file contains EXPERIMENTAL functions to create video frames from
// either an old video frame or directly from parameters.
// These functions will be used in Chrome functionality to manipulate
// encoded frames from Javascript.
namespace webrtc {

// Creates a new frame with the same metadata as the original.
// The original can be a sender or receiver frame.
RTC_EXPORT std::unique_ptr<TransformableAudioFrameInterface> CloneAudioFrame(
    TransformableAudioFrameInterface* original);
RTC_EXPORT std::unique_ptr<TransformableVideoFrameInterface> CloneVideoFrame(
    TransformableVideoFrameInterface* original);
RTC_EXPORT std::unique_ptr<TransformableAudioFrameInterface>
CreateOutgoingAudioFrame(TransformableAudioFrameInterface::FrameType frame_type,
                         PayloadType payload_type,
                         uint32_t rtp_timestamp_without_offset,
                         const uint8_t* payload_data,
                         size_t payload_size,
                         std::optional<uint64_t> absolute_capture_timestamp_ms,
                         uint32_t ssrc,
                         const std::vector<uint32_t>& csrcs,
                         const std::string& codec_mime_type,
                         std::optional<uint16_t> sequence_number,
                         std::optional<uint8_t> audio_level_dbov);
}  // namespace webrtc

#endif  // API_FRAME_TRANSFORMER_FACTORY_H_
