/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/frame_transformer_interface.h"

namespace webrtc {

TransformableFrameInterface::TransformableFrameInterface(
    TransformableFrameInterface::Passkey) {}

TransformableVideoFrameInterface::TransformableVideoFrameInterface(
    TransformableFrameInterface::Passkey passkey)
    : TransformableFrameInterface(passkey) {}

TransformableAudioFrameInterface::TransformableAudioFrameInterface(
    TransformableFrameInterface::Passkey passkey)
    : TransformableFrameInterface(passkey) {}

}  // namespace webrtc
