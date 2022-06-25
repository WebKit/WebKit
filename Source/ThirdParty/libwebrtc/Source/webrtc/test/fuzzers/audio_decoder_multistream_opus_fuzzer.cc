/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_decoder_multi_channel_opus.h"
#include "api/audio_codecs/opus/audio_decoder_multi_channel_opus_config.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {

AudioDecoderMultiChannelOpusConfig MakeDecoderConfig(
    int num_channels,
    int num_streams,
    int coupled_streams,
    std::vector<unsigned char> channel_mapping) {
  AudioDecoderMultiChannelOpusConfig config;
  config.num_channels = num_channels;
  config.num_streams = num_streams;
  config.coupled_streams = coupled_streams;
  config.channel_mapping = channel_mapping;
  return config;
}

void FuzzOneInput(const uint8_t* data, size_t size) {
  const std::vector<AudioDecoderMultiChannelOpusConfig> surround_configs = {
      MakeDecoderConfig(1, 1, 0, {0}),  // Mono

      MakeDecoderConfig(2, 2, 0, {0, 0}),  // Copy the first (of
                                           // 2) decoded streams
                                           // into both output
                                           // channel 0 and output
                                           // channel 1. Ignore
                                           // the 2nd decoded
                                           // stream.

      MakeDecoderConfig(4, 2, 2, {0, 1, 2, 3}),             // Quad.
      MakeDecoderConfig(6, 4, 2, {0, 4, 1, 2, 3, 5}),       // 5.1
      MakeDecoderConfig(8, 5, 3, {0, 6, 1, 2, 3, 4, 5, 7})  // 7.1
  };

  const auto config = surround_configs[data[0] % surround_configs.size()];
  RTC_CHECK(config.IsOk());
  std::unique_ptr<AudioDecoder> dec =
      AudioDecoderMultiChannelOpus::MakeAudioDecoder(config);
  RTC_CHECK(dec);
  const int kSampleRateHz = 48000;
  const size_t kAllocatedOuputSizeSamples =
      4 * kSampleRateHz / 10;  // 4x100 ms, 4 times the size of the output array
                               // for the stereo Opus codec. It should be enough
                               // for 8 channels.
  int16_t output[kAllocatedOuputSizeSamples];
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, data, size, dec.get(),
                   kSampleRateHz, sizeof(output), output);
}
}  // namespace webrtc
