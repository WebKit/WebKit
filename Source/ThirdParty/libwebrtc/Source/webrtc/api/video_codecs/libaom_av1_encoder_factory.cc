/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/libaom_av1_encoder_factory.h"

#include <map>
#include <memory>
#include <string>

#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder_v2.h"

namespace webrtc {

std::string LibaomAv1EncoderFactory::CodecName() const {
  return "AV1";
}

std::string LibaomAv1EncoderFactory::ImplementationName() const {
  return "Libaom";
}

std::map<std::string, std::string> LibaomAv1EncoderFactory::CodecSpecifics()
    const {
  return {};
}

VideoEncoderFactoryInterface::Capabilities
LibaomAv1EncoderFactory::GetEncoderCapabilities() const {
  return LibaomAv1EncoderV2::GetCapabilities();
}

std::unique_ptr<VideoEncoderInterface> LibaomAv1EncoderFactory::CreateEncoder(
    const StaticEncoderSettings& settings,
    const std::map<std::string, std::string>& encoder_specific_settings) {
  auto encoder = std::make_unique<LibaomAv1EncoderV2>();
  if (!encoder->InitEncode(settings, encoder_specific_settings)) {
    return nullptr;
  }
  return encoder;
}

}  // namespace webrtc
