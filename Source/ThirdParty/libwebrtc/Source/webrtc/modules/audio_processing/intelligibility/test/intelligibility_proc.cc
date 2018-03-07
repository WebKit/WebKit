/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/channel_buffer.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/wav_file.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/intelligibility/intelligibility_enhancer.h"
#include "modules/audio_processing/noise_suppression_impl.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/flags.h"

using std::complex;

namespace webrtc {
namespace {

DEFINE_string(clear_file, "speech.wav", "Input file with clear speech.");
DEFINE_string(noise_file, "noise.wav", "Input file with noise data.");
DEFINE_string(out_file, "proc_enhanced.wav", "Enhanced output file.");
DEFINE_bool(help, false, "Print this message.");

int int_main(int argc, char* argv[]) {
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    return 1;
  }
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }
  if (argc != 1) {
    printf("\n\nInput files must be little-endian 16-bit signed raw PCM.\n");
    return 0;
  }

  WavReader in_file(FLAG_clear_file);
  WavReader noise_file(FLAG_noise_file);
  WavWriter out_file(FLAG_out_file, in_file.sample_rate(),
                     in_file.num_channels());
  rtc::CriticalSection crit;
  NoiseSuppressionImpl ns(&crit);
  IntelligibilityEnhancer enh(in_file.sample_rate(), in_file.num_channels(), 1u,
                              NoiseSuppressionImpl::num_noise_bins());
  ns.Initialize(noise_file.num_channels(), noise_file.sample_rate());
  ns.Enable(true);
  const size_t in_samples = noise_file.sample_rate() / 100;
  const size_t noise_samples = noise_file.sample_rate() / 100;
  std::vector<float> in(in_samples * in_file.num_channels());
  std::vector<float> noise(noise_samples * noise_file.num_channels());
  ChannelBuffer<float> in_buf(in_samples, in_file.num_channels());
  ChannelBuffer<float> noise_buf(noise_samples, noise_file.num_channels());
  AudioBuffer capture_audio(noise_samples, noise_file.num_channels(),
                            noise_samples, noise_file.num_channels(),
                            noise_samples);
  AudioBuffer render_audio(in_samples, in_file.num_channels(), in_samples,
                           in_file.num_channels(), in_samples);
  StreamConfig noise_config(noise_file.sample_rate(),
                            noise_file.num_channels());
  StreamConfig in_config(in_file.sample_rate(), in_file.num_channels());
  while (in_file.ReadSamples(in.size(), in.data()) == in.size() &&
         noise_file.ReadSamples(noise.size(), noise.data()) == noise.size()) {
    FloatS16ToFloat(noise.data(), noise.size(), noise.data());
    FloatS16ToFloat(in.data(), in.size(), in.data());
    Deinterleave(in.data(), in_buf.num_frames(), in_buf.num_channels(),
                 in_buf.channels());
    Deinterleave(noise.data(), noise_buf.num_frames(), noise_buf.num_channels(),
                 noise_buf.channels());
    capture_audio.CopyFrom(noise_buf.channels(), noise_config);
    render_audio.CopyFrom(in_buf.channels(), in_config);
    ns.AnalyzeCaptureAudio(&capture_audio);
    ns.ProcessCaptureAudio(&capture_audio);
    enh.SetCaptureNoiseEstimate(ns.NoiseEstimate(), 1);
    enh.ProcessRenderAudio(&render_audio);
    render_audio.CopyTo(in_config, in_buf.channels());
    Interleave(in_buf.channels(), in_buf.num_frames(), in_buf.num_channels(),
               in.data());
    FloatToFloatS16(in.data(), in.size(), in.data());
    out_file.WriteSamples(in.data(), in.size());
  }

  return 0;
}

}  // namespace
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::int_main(argc, argv);
}
