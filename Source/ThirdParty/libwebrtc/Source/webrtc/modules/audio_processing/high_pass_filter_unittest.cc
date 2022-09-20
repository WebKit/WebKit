/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/high_pass_filter.h"

#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// Process one frame of data via the AudioBuffer interface and produce the
// output.
std::vector<float> ProcessOneFrameAsAudioBuffer(
    const std::vector<float>& frame_input,
    const StreamConfig& stream_config,
    HighPassFilter* high_pass_filter) {
  AudioBuffer audio_buffer(
      stream_config.sample_rate_hz(), stream_config.num_channels(),
      stream_config.sample_rate_hz(), stream_config.num_channels(),
      stream_config.sample_rate_hz(), stream_config.num_channels());

  test::CopyVectorToAudioBuffer(stream_config, frame_input, &audio_buffer);
  high_pass_filter->Process(&audio_buffer, /*use_split_band_data=*/false);
  std::vector<float> frame_output;
  test::ExtractVectorFromAudioBuffer(stream_config, &audio_buffer,
                                     &frame_output);
  return frame_output;
}

// Process one frame of data via the vector interface and produce the output.
std::vector<float> ProcessOneFrameAsVector(
    const std::vector<float>& frame_input,
    const StreamConfig& stream_config,
    HighPassFilter* high_pass_filter) {
  std::vector<std::vector<float>> process_vector(
      stream_config.num_channels(),
      std::vector<float>(stream_config.num_frames()));

  for (size_t k = 0; k < stream_config.num_frames(); ++k) {
    for (size_t channel = 0; channel < stream_config.num_channels();
         ++channel) {
      process_vector[channel][k] =
          frame_input[k * stream_config.num_channels() + channel];
    }
  }

  high_pass_filter->Process(&process_vector);

  std::vector<float> output;
  for (size_t k = 0; k < stream_config.num_frames(); ++k) {
    for (size_t channel = 0; channel < stream_config.num_channels();
         ++channel) {
      output.push_back(process_vector[channel][k]);
    }
  }

  return process_vector[0];
}

// Processes a specified amount of frames, verifies the results and reports
// any errors.
void RunBitexactnessTest(int num_channels,
                         bool use_audio_buffer_interface,
                         const std::vector<float>& input,
                         const std::vector<float>& reference) {
  const StreamConfig stream_config(16000, num_channels);
  HighPassFilter high_pass_filter(16000, num_channels);

  std::vector<float> output;
  const size_t num_frames_to_process =
      input.size() /
      (stream_config.num_frames() * stream_config.num_channels());
  for (size_t frame_no = 0; frame_no < num_frames_to_process; ++frame_no) {
    std::vector<float> frame_input(
        input.begin() + stream_config.num_frames() *
                            stream_config.num_channels() * frame_no,
        input.begin() + stream_config.num_frames() *
                            stream_config.num_channels() * (frame_no + 1));
    if (use_audio_buffer_interface) {
      output = ProcessOneFrameAsAudioBuffer(frame_input, stream_config,
                                            &high_pass_filter);
    } else {
      output = ProcessOneFrameAsVector(frame_input, stream_config,
                                       &high_pass_filter);
    }
  }

  // Form vector to compare the reference to. Only the last frame processed
  // is compared in order not having to specify all preceeding frames as
  // inputs. As the algorithm being tested has a memory, testing only
  // the last frame implicitly also tests the preceeding frames.
  const size_t reference_frame_length =
      reference.size() / stream_config.num_channels();
  std::vector<float> output_to_verify;
  for (size_t channel_no = 0; channel_no < stream_config.num_channels();
       ++channel_no) {
    output_to_verify.insert(
        output_to_verify.end(),
        output.begin() + channel_no * stream_config.num_frames(),
        output.begin() + channel_no * stream_config.num_frames() +
            reference_frame_length);
  }

  const float kElementErrorBound = 1.0f / 32768.0f;
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      reference_frame_length, num_channels, reference, output_to_verify,
      kElementErrorBound));
}

// Method for forming a vector out of an array.
// TODO(peah): Remove once braced initialization is allowed.
std::vector<float> CreateVector(const rtc::ArrayView<const float>& array_view) {
  std::vector<float> v;
  for (auto value : array_view) {
    v.push_back(value);
  }
  return v;
}
}  // namespace

TEST(HighPassFilterAccuracyTest, ResetWithAudioBufferInterface) {
  const StreamConfig stream_config_stereo(16000, 2);
  const StreamConfig stream_config_mono(16000, 1);
  std::vector<float> x_mono(160, 1.f);
  std::vector<float> x_stereo(320, 1.f);
  HighPassFilter hpf(16000, 1);
  std::vector<float> y =
      ProcessOneFrameAsAudioBuffer(x_mono, stream_config_mono, &hpf);
  hpf.Reset(2);
  y = ProcessOneFrameAsAudioBuffer(x_stereo, stream_config_stereo, &hpf);
  hpf.Reset(1);
  y = ProcessOneFrameAsAudioBuffer(x_mono, stream_config_mono, &hpf);
  hpf.Reset();
  y = ProcessOneFrameAsAudioBuffer(x_mono, stream_config_mono, &hpf);
}

TEST(HighPassFilterAccuracyTest, ResetWithVectorInterface) {
  const StreamConfig stream_config_stereo(16000, 2);
  const StreamConfig stream_config_mono(16000, 1);
  std::vector<float> x_mono(160, 1.f);
  std::vector<float> x_stereo(320, 1.f);
  HighPassFilter hpf(16000, 1);
  std::vector<float> y =
      ProcessOneFrameAsVector(x_mono, stream_config_mono, &hpf);
  hpf.Reset(2);
  y = ProcessOneFrameAsVector(x_stereo, stream_config_stereo, &hpf);
  hpf.Reset(1);
  y = ProcessOneFrameAsVector(x_mono, stream_config_mono, &hpf);
  hpf.Reset();
  y = ProcessOneFrameAsVector(x_mono, stream_config_mono, &hpf);
}

TEST(HighPassFilterAccuracyTest, MonoInitial) {
  const float kReferenceInput[] = {
      0.150254f,  0.512488f,  -0.631245f, 0.240938f,  0.089080f,  -0.365440f,
      -0.121169f, 0.095748f,  1.000000f,  0.773932f,  -0.377232f, 0.848124f,
      0.202718f,  -0.017621f, 0.199738f,  -0.057279f, -0.034693f, 0.416303f,
      0.393761f,  0.396041f,  0.187653f,  -0.337438f, 0.200436f,  0.455577f,
      0.136624f,  0.289150f,  0.203131f,  -0.084798f, 0.082124f,  -0.220010f,
      0.248266f,  -0.320554f, -0.298701f, -0.226218f, -0.822794f, 0.401962f,
      0.090876f,  -0.210968f, 0.382936f,  -0.478291f, -0.028572f, -0.067474f,
      0.089204f,  0.087430f,  -0.241695f, -0.008398f, -0.046076f, 0.175416f,
      0.305518f,  0.309992f,  -0.241352f, 0.021618f,  -0.339291f, -0.311173f,
      -0.001914f, 0.428301f,  -0.215087f, 0.103784f,  -0.063041f, 0.312250f,
      -0.304344f, 0.009098f,  0.154406f,  0.307571f,  0.431537f,  0.024014f,
      -0.416832f, -0.207440f, -0.296664f, 0.656846f,  -0.172033f, 0.209054f,
      -0.053772f, 0.248326f,  -0.213741f, -0.391871f, -0.397490f, 0.136428f,
      -0.049568f, -0.054788f, 0.396633f,  0.081485f,  0.055279f,  0.443690f,
      -0.224812f, 0.194675f,  0.233369f,  -0.068107f, 0.060270f,  -0.325801f,
      -0.320801f, 0.029308f,  0.201837f,  0.722528f,  -0.186366f, 0.052351f,
      -0.023053f, -0.540192f, -0.122671f, -0.501532f, 0.234847f,  -0.248165f,
      0.027971f,  -0.152171f, 0.084820f,  -0.167764f, 0.136923f,  0.206619f,
      0.478395f,  -0.054249f, -0.597574f, -0.234627f, 0.378548f,  -0.299619f,
      0.268543f,  0.034666f,  0.401492f,  -0.547983f, -0.055248f, -0.337538f,
      0.812657f,  0.230611f,  0.385360f,  -0.295713f, -0.130957f, -0.076143f,
      0.306960f,  -0.077653f, 0.196049f,  -0.573390f, -0.098885f, -0.230155f,
      -0.440716f, 0.141956f,  0.078802f,  0.009356f,  -0.372703f, 0.315083f,
      0.097859f,  -0.083575f, 0.006397f,  -0.073216f, -0.489105f, -0.079827f,
      -0.232329f, -0.273644f, -0.323162f, -0.149105f, -0.559646f, 0.269458f,
      0.145333f,  -0.005597f, -0.009717f, -0.223051f, 0.284676f,  -0.037228f,
      -0.199679f, 0.377651f,  -0.062813f, -0.164607f};
  const float kReference[] = {0.146139f, 0.490336f,  -0.649520f, 0.233881f,
                              0.073214f, -0.373256f, -0.115394f, 0.102109f,
                              0.976217f, 0.702270f,  -0.457697f, 0.757116f};

  for (bool use_audio_buffer_interface : {true, false}) {
    RunBitexactnessTest(
        1, use_audio_buffer_interface,
        CreateVector(rtc::ArrayView<const float>(kReferenceInput)),
        CreateVector(rtc::ArrayView<const float>(kReference)));
  }
}

TEST(HighPassFilterAccuracyTest, MonoConverged) {
  const float kReferenceInput[] = {
      0.150254f,  0.512488f,  -0.631245f, 0.240938f,  0.089080f,  -0.365440f,
      -0.121169f, 0.095748f,  1.000000f,  0.773932f,  -0.377232f, 0.848124f,
      0.202718f,  -0.017621f, 0.199738f,  -0.057279f, -0.034693f, 0.416303f,
      0.393761f,  0.396041f,  0.187653f,  -0.337438f, 0.200436f,  0.455577f,
      0.136624f,  0.289150f,  0.203131f,  -0.084798f, 0.082124f,  -0.220010f,
      0.248266f,  -0.320554f, -0.298701f, -0.226218f, -0.822794f, 0.401962f,
      0.090876f,  -0.210968f, 0.382936f,  -0.478291f, -0.028572f, -0.067474f,
      0.089204f,  0.087430f,  -0.241695f, -0.008398f, -0.046076f, 0.175416f,
      0.305518f,  0.309992f,  -0.241352f, 0.021618f,  -0.339291f, -0.311173f,
      -0.001914f, 0.428301f,  -0.215087f, 0.103784f,  -0.063041f, 0.312250f,
      -0.304344f, 0.009098f,  0.154406f,  0.307571f,  0.431537f,  0.024014f,
      -0.416832f, -0.207440f, -0.296664f, 0.656846f,  -0.172033f, 0.209054f,
      -0.053772f, 0.248326f,  -0.213741f, -0.391871f, -0.397490f, 0.136428f,
      -0.049568f, -0.054788f, 0.396633f,  0.081485f,  0.055279f,  0.443690f,
      -0.224812f, 0.194675f,  0.233369f,  -0.068107f, 0.060270f,  -0.325801f,
      -0.320801f, 0.029308f,  0.201837f,  0.722528f,  -0.186366f, 0.052351f,
      -0.023053f, -0.540192f, -0.122671f, -0.501532f, 0.234847f,  -0.248165f,
      0.027971f,  -0.152171f, 0.084820f,  -0.167764f, 0.136923f,  0.206619f,
      0.478395f,  -0.054249f, -0.597574f, -0.234627f, 0.378548f,  -0.299619f,
      0.268543f,  0.034666f,  0.401492f,  -0.547983f, -0.055248f, -0.337538f,
      0.812657f,  0.230611f,  0.385360f,  -0.295713f, -0.130957f, -0.076143f,
      0.306960f,  -0.077653f, 0.196049f,  -0.573390f, -0.098885f, -0.230155f,
      -0.440716f, 0.141956f,  0.078802f,  0.009356f,  -0.372703f, 0.315083f,
      0.097859f,  -0.083575f, 0.006397f,  -0.073216f, -0.489105f, -0.079827f,
      -0.232329f, -0.273644f, -0.323162f, -0.149105f, -0.559646f, 0.269458f,
      0.145333f,  -0.005597f, -0.009717f, -0.223051f, 0.284676f,  -0.037228f,
      -0.199679f, 0.377651f,  -0.062813f, -0.164607f, -0.082091f, -0.236957f,
      -0.313025f, 0.705903f,  0.462637f,  0.085942f,  -0.351308f, -0.241859f,
      -0.049333f, 0.221165f,  -0.372235f, -0.651092f, -0.404957f, 0.093201f,
      0.109366f,  0.126224f,  -0.036409f, 0.051333f,  -0.133063f, 0.240896f,
      -0.380532f, 0.127160f,  -0.237176f, -0.093586f, 0.154478f,  0.290379f,
      -0.312329f, 0.352297f,  0.184480f,  -0.018965f, -0.054555f, -0.060811f,
      -0.084705f, 0.006440f,  0.014333f,  0.230847f,  0.426721f,  0.130481f,
      -0.058605f, 0.174712f,  0.051204f,  -0.287773f, 0.265265f,  0.085810f,
      0.037775f,  0.143988f,  0.073051f,  -0.263103f, -0.045366f, -0.040816f,
      -0.148673f, 0.470072f,  -0.244727f, -0.135204f, -0.198973f, -0.328139f,
      -0.053722f, -0.076590f, 0.427586f,  -0.069591f, -0.297399f, 0.448094f,
      0.345037f,  -0.064170f, -0.420903f, -0.124253f, -0.043578f, 0.077149f,
      -0.072983f, 0.123916f,  0.109517f,  -0.349508f, -0.264912f, -0.207106f,
      -0.141912f, -0.089586f, 0.003485f,  -0.846518f, -0.127715f, 0.347208f,
      -0.298095f, 0.260935f,  0.097899f,  -0.008106f, 0.050987f,  -0.437362f,
      -0.023625f, 0.448230f,  0.027484f,  0.011562f,  -0.205167f, -0.008611f,
      0.064930f,  0.119156f,  -0.104183f, -0.066078f, 0.565530f,  -0.631108f,
      0.623029f,  0.094334f,  0.279472f,  -0.465059f, -0.164888f, -0.077706f,
      0.118130f,  -0.466746f, 0.131800f,  -0.338936f, 0.018497f,  0.182304f,
      0.091398f,  0.302547f,  0.281153f,  -0.181899f, 0.071836f,  -0.263911f,
      -0.369380f, 0.258447f,  0.000014f,  -0.015347f, 0.254619f,  0.166159f,
      0.097865f,  0.349389f,  0.259834f,  0.067003f,  -0.192925f, -0.182080f,
      0.333139f,  -0.450434f, -0.006836f, -0.544615f, 0.285183f,  0.240811f,
      0.000325f,  -0.019796f, -0.694804f, 0.162411f,  -0.612686f, -0.648134f,
      0.022338f,  -0.265058f, 0.114993f,  0.189185f,  0.239697f,  -0.193148f,
      0.125581f,  0.028122f,  0.230849f,  0.149832f,  0.250919f,  -0.036871f,
      -0.041136f, 0.281627f,  -0.593466f, -0.141009f, -0.355074f, -0.106915f,
      0.181276f,  0.230753f,  -0.283631f, -0.131643f, 0.038292f,  -0.081563f,
      0.084345f,  0.111763f,  -0.259882f, -0.049416f, -0.595824f, 0.320077f,
      -0.175802f, -0.336422f, -0.070966f, -0.399242f, -0.005829f, -0.156680f,
      0.608591f,  0.318150f,  -0.697767f, 0.123331f,  -0.390716f, -0.071276f,
      0.045943f,  0.208958f,  -0.076304f, 0.440505f,  -0.134400f, 0.091525f,
      0.185763f,  0.023806f,  0.246186f,  0.090323f,  -0.219133f, -0.504520f,
      0.519393f,  -0.168939f, 0.028884f,  0.157380f,  0.031745f,  -0.252830f,
      -0.130705f, -0.034901f, 0.413302f,  -0.240559f, 0.219279f,  0.086246f,
      -0.065353f, -0.295376f, -0.079405f, -0.024226f, -0.410629f, 0.053706f,
      -0.229794f, -0.026336f, 0.093956f,  -0.252810f, -0.080555f, 0.097827f,
      -0.513040f, 0.289508f,  0.677527f,  0.268109f,  -0.088244f, 0.119781f,
      -0.289511f, 0.524778f,  0.262884f,  0.220028f,  -0.244767f, 0.089411f,
      -0.156018f, -0.087030f, -0.159292f, -0.286646f, -0.253953f, -0.058657f,
      -0.474756f, 0.169797f,  -0.032919f, 0.195384f,  0.075355f,  0.138131f,
      -0.414465f, -0.285118f, -0.124915f, 0.030645f,  0.315431f,  -0.081032f,
      0.352546f,  0.132860f,  0.328112f,  0.035476f,  -0.183550f, -0.413984f,
      0.043452f,  0.228748f,  -0.081765f, -0.151125f, -0.086251f, -0.306448f,
      -0.137774f, -0.050508f, 0.012811f,  -0.017824f, 0.170841f,  0.030549f,
      0.506935f,  0.087197f,  0.504274f,  -0.202080f, 0.147146f,  -0.072728f,
      0.167713f,  0.165977f,  -0.610894f, -0.370849f, -0.402698f, 0.112297f,
      0.410855f,  -0.091330f, 0.227008f,  0.152454f,  -0.293884f, 0.111074f,
      -0.210121f, 0.423728f,  -0.009101f, 0.457188f,  -0.118785f, 0.164720f,
      -0.017547f, -0.565046f, -0.274461f, 0.171169f,  -0.015338f, -0.312635f,
      -0.175044f, 0.069729f,  -0.277504f, 0.272454f,  -0.179049f, 0.505495f,
      -0.301774f, 0.055664f,  -0.425058f, -0.202222f, -0.165787f, 0.112155f,
      0.263284f,  0.083972f,  -0.104256f, 0.227892f,  0.223253f,  0.033592f,
      0.159638f,  0.115358f,  -0.275811f, 0.212265f,  -0.183658f, -0.168768f};

  const float kReference[] = {-0.248836f, -0.086982f, 0.083715f,  -0.036787f,
                              0.127212f,  0.147464f,  -0.221733f, -0.004484f,
                              -0.535107f, 0.385999f,  -0.116346f, -0.265302f};

  for (bool use_audio_buffer_interface : {true, false}) {
    RunBitexactnessTest(
        1, use_audio_buffer_interface,
        CreateVector(rtc::ArrayView<const float>(kReferenceInput)),
        CreateVector(rtc::ArrayView<const float>(kReference)));
  }
}

}  // namespace webrtc
