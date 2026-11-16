/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENCODED_AUDIO_FRAME_INJECTOR_INTERFACE_H_
#define API_ENCODED_AUDIO_FRAME_INJECTOR_INTERFACE_H_

#include <cstdint>
#include <memory>

#include "absl/functional/any_invocable.h"
#include "api/frame_transformer_interface.h"
#include "api/ref_count.h"

namespace webrtc {

// Callback invoked by webrtc to notify of an update to target bitrate.
using TargetBitrateCallback =
    absl::AnyInvocable<void(int32_t target_bitrate) const>;

// Interface that allows injecting encoded audio frames on a sender.
class EncodedAudioFrameInjectorInterface : public RefCountInterface {
 public:
  // Injects an encoded audio frame, can be called on any thread.
  virtual void InjectFrame(
      std::unique_ptr<TransformableAudioFrameInterface> encoded_frame) = 0;

 protected:
  ~EncodedAudioFrameInjectorInterface() override = default;
};

}  // namespace webrtc

#endif  // API_ENCODED_AUDIO_FRAME_INJECTOR_INTERFACE_H_
