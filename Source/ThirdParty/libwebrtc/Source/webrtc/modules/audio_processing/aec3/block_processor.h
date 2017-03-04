/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_BLOCK_PROCESSOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_BLOCK_PROCESSOR_H_

#include <memory>
#include <vector>

#include "webrtc/modules/audio_processing/aec3/echo_remover.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_controller.h"

namespace webrtc {

// Class for performing echo cancellation on 64 sample blocks of audio data.
class BlockProcessor {
 public:
  static BlockProcessor* Create(int sample_rate_hz);
  // Only used for testing purposes.
  static BlockProcessor* Create(
      int sample_rate_hz,
      std::unique_ptr<RenderDelayBuffer> render_buffer);
  static BlockProcessor* Create(
      int sample_rate_hz,
      std::unique_ptr<RenderDelayBuffer> render_buffer,
      std::unique_ptr<RenderDelayController> delay_controller,
      std::unique_ptr<EchoRemover> echo_remover);

  virtual ~BlockProcessor() = default;

  // Processes a block of capture data.
  virtual void ProcessCapture(
      bool echo_path_gain_change,
      bool capture_signal_saturation,
      std::vector<std::vector<float>>* capture_block) = 0;

  // Buffers a block of render data supplied by a FrameBlocker object. Returns a
  // bool indicating the success of the buffering.
  virtual bool BufferRender(std::vector<std::vector<float>>* render_block) = 0;

  // Reports whether echo leakage has been detected in the echo canceller
  // output.
  virtual void UpdateEchoLeakageStatus(bool leakage_detected) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_BLOCK_PROCESSOR_H_
