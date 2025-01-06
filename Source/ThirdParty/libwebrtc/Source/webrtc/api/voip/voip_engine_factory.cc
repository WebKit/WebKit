/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/voip/voip_engine_factory.h"

#include <memory>
#include <utility>

#include "api/audio/audio_processing.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/scoped_refptr.h"
#include "api/voip/voip_engine.h"
#include "audio/voip/voip_core.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::unique_ptr<VoipEngine> CreateVoipEngine(VoipEngineConfig config) {
  RTC_CHECK(config.encoder_factory);
  RTC_CHECK(config.decoder_factory);
  RTC_CHECK(config.task_queue_factory);
  RTC_CHECK(config.audio_device_module);

  Environment env = CreateEnvironment(std::move(config.task_queue_factory));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  RTC_CHECK(config.audio_processing == nullptr ||
            config.audio_processing_builder == nullptr);
  scoped_refptr<AudioProcessing> audio_processing =
      std::move(config.audio_processing);
#pragma clang diagnostic pop
  if (config.audio_processing_builder != nullptr) {
    audio_processing = std::move(config.audio_processing_builder)->Build(env);
  }

  if (audio_processing == nullptr) {
    RTC_DLOG(LS_INFO) << "No audio processing functionality provided.";
  }

  return std::make_unique<VoipCore>(
      env, std::move(config.encoder_factory), std::move(config.decoder_factory),
      std::move(config.audio_device_module), std::move(audio_processing));
}

}  // namespace webrtc
