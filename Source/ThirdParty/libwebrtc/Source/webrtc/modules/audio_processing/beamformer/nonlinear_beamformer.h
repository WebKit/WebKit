/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_BEAMFORMER_NONLINEAR_BEAMFORMER_H_
#define MODULES_AUDIO_PROCESSING_BEAMFORMER_NONLINEAR_BEAMFORMER_H_

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include <math.h>

#include <memory>
#include <vector>

#include "common_audio/lapped_transform.h"
#include "common_audio/channel_buffer.h"
#include "modules/audio_processing/beamformer/array_util.h"
#include "modules/audio_processing/beamformer/complex_matrix.h"

namespace webrtc {

class PostFilterTransform : public LappedTransform::Callback {
 public:
  PostFilterTransform(size_t num_channels,
                      size_t chunk_length,
                      float* window,
                      size_t fft_size);

  void ProcessChunk(float* const* data, float* final_mask);

 protected:
  void ProcessAudioBlock(const complex<float>* const* input,
                         size_t num_input_channels,
                         size_t num_freq_bins,
                         size_t num_output_channels,
                         complex<float>* const* output) override;

 private:
  LappedTransform transform_;
  const size_t num_freq_bins_;
  float* final_mask_;
};

// Enhances sound sources coming directly in front of a uniform linear array
// and suppresses sound sources coming from all other directions. Operates on
// multichannel signals and produces single-channel output.
//
// The implemented nonlinear postfilter algorithm taken from "A Robust Nonlinear
// Beamforming Postprocessor" by Bastiaan Kleijn.
class NonlinearBeamformer : public LappedTransform::Callback {
 public:
  static const float kHalfBeamWidthRadians;

  explicit NonlinearBeamformer(
      const std::vector<Point>& array_geometry,
      size_t num_postfilter_channels = 1u,
      SphericalPointf target_direction =
          SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f));
  ~NonlinearBeamformer() override;

  // Sample rate corresponds to the lower band.
  // Needs to be called before the NonlinearBeamformer can be used.
  virtual void Initialize(int chunk_size_ms, int sample_rate_hz);

  // Analyzes one time-domain chunk of audio. The audio is expected to be split
  // into frequency bands inside the ChannelBuffer. The number of frames and
  // channels must correspond to the constructor parameters.
  virtual void AnalyzeChunk(const ChannelBuffer<float>& data);

  // Applies the postfilter mask to one chunk of audio. The audio is expected to
  // be split into frequency bands inside the ChannelBuffer. The number of
  // frames and channels must correspond to the constructor parameters.
  virtual void PostFilter(ChannelBuffer<float>* data);

  virtual void AimAt(const SphericalPointf& target_direction);

  virtual bool IsInBeam(const SphericalPointf& spherical_point);

  // After processing each block |is_target_present_| is set to true if the
  // target signal es present and to false otherwise. This methods can be called
  // to know if the data is target signal or interference and process it
  // accordingly.
  virtual bool is_target_present();

 protected:
  // Process one frequency-domain block of audio. This is where the fun
  // happens. Implements LappedTransform::Callback.
  void ProcessAudioBlock(const complex<float>* const* input,
                         size_t num_input_channels,
                         size_t num_freq_bins,
                         size_t num_output_channels,
                         complex<float>* const* output) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NonlinearBeamformerTest,
                           InterfAnglesTakeAmbiguityIntoAccount);

  typedef Matrix<float> MatrixF;
  typedef ComplexMatrix<float> ComplexMatrixF;
  typedef complex<float> complex_f;

  void InitLowFrequencyCorrectionRanges();
  void InitHighFrequencyCorrectionRanges();
  void InitInterfAngles();
  void InitDelaySumMasks();
  void InitTargetCovMats();
  void InitDiffuseCovMats();
  void InitInterfCovMats();
  void NormalizeCovMats();

  // Calculates postfilter masks that minimize the mean squared error of our
  // estimation of the desired signal.
  float CalculatePostfilterMask(const ComplexMatrixF& interf_cov_mat,
                                float rpsiw,
                                float ratio_rxiw_rxim,
                                float rmxi_r);

  // Prevents the postfilter masks from degenerating too quickly (a cause of
  // musical noise).
  void ApplyMaskTimeSmoothing();
  void ApplyMaskFrequencySmoothing();

  // The postfilter masks are unreliable at low frequencies. Calculates a better
  // mask by averaging mid-low frequency values.
  void ApplyLowFrequencyCorrection();

  // Postfilter masks are also unreliable at high frequencies. Average mid-high
  // frequency masks to calculate a single mask per block which can be applied
  // in the time-domain. Further, we average these block-masks over a chunk,
  // resulting in one postfilter mask per audio chunk. This allows us to skip
  // both transforming and blocking the high-frequency signal.
  void ApplyHighFrequencyCorrection();

  // Compute the means needed for the above frequency correction.
  float MaskRangeMean(size_t start_bin, size_t end_bin);

  // Applies post-filter mask to |input| and store in |output|.
  void ApplyPostFilter(const complex_f* input, complex_f* output);

  void EstimateTargetPresence();

  static const size_t kFftSize = 256;
  static const size_t kNumFreqBins = kFftSize / 2 + 1;

  // Deals with the fft transform and blocking.
  size_t chunk_length_;
  std::unique_ptr<LappedTransform> process_transform_;
  std::unique_ptr<PostFilterTransform> postfilter_transform_;
  float window_[kFftSize];

  // Parameters exposed to the user.
  const size_t num_input_channels_;
  const size_t num_postfilter_channels_;
  int sample_rate_hz_;

  const std::vector<Point> array_geometry_;
  // The normal direction of the array if it has one and it is in the xy-plane.
  const rtc::Optional<Point> array_normal_;

  // Minimum spacing between microphone pairs.
  const float min_mic_spacing_;

  // Calculated based on user-input and constants in the .cc file.
  size_t low_mean_start_bin_;
  size_t low_mean_end_bin_;
  size_t high_mean_start_bin_;
  size_t high_mean_end_bin_;

  // Quickly varying mask updated every block.
  float new_mask_[kNumFreqBins];
  // Time smoothed mask.
  float time_smooth_mask_[kNumFreqBins];
  // Time and frequency smoothed mask.
  float final_mask_[kNumFreqBins];

  float target_angle_radians_;
  // Angles of the interferer scenarios.
  std::vector<float> interf_angles_radians_;
  // The angle between the target and the interferer scenarios.
  const float away_radians_;

  // Array of length |kNumFreqBins|, Matrix of size |1| x |num_channels_|.
  ComplexMatrixF delay_sum_masks_[kNumFreqBins];

  // Arrays of length |kNumFreqBins|, Matrix of size |num_input_channels_| x
  // |num_input_channels_|.
  ComplexMatrixF target_cov_mats_[kNumFreqBins];
  ComplexMatrixF uniform_cov_mat_[kNumFreqBins];
  // Array of length |kNumFreqBins|, Matrix of size |num_input_channels_| x
  // |num_input_channels_|. The vector has a size equal to the number of
  // interferer scenarios.
  std::vector<std::unique_ptr<ComplexMatrixF>> interf_cov_mats_[kNumFreqBins];

  // Of length |kNumFreqBins|.
  float wave_numbers_[kNumFreqBins];

  // Preallocated for ProcessAudioBlock()
  // Of length |kNumFreqBins|.
  float rxiws_[kNumFreqBins];
  // The vector has a size equal to the number of interferer scenarios.
  std::vector<float> rpsiws_[kNumFreqBins];

  // The microphone normalization factor.
  ComplexMatrixF eig_m_;

  // For processing the high-frequency input signal.
  float high_pass_postfilter_mask_;
  float old_high_pass_mask_;

  // True when the target signal is present.
  bool is_target_present_;
  // Number of blocks after which the data is considered interference if the
  // mask does not pass |kMaskSignalThreshold|.
  size_t hold_target_blocks_;
  // Number of blocks since the last mask that passed |kMaskSignalThreshold|.
  size_t interference_blocks_count_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_BEAMFORMER_NONLINEAR_BEAMFORMER_H_
