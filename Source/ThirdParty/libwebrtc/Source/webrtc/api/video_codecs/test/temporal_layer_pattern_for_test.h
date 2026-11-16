/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_TEST_TEMPORAL_LAYER_PATTERN_FOR_TEST_H_
#define API_VIDEO_CODECS_TEST_TEMPORAL_LAYER_PATTERN_FOR_TEST_H_

#include <optional>
#include <vector>

namespace webrtc {

// Test only helper that generates the frame configurations of a generalized
// L1TN pattern; a single spatial layer with `num_temporal_layers` dyadic
// temporal layers. Each layer has twice as many frames per second as the layer
// below it, except for the base layer which has the same frame rate as the
// layer just above it. With three layers this gives the repeating pattern of
// temporal ids {0, 2, 1, 2}, where layers 0, 1 and 2 contribute 1, 1 and 2
// frames per group of pictures respectively.
//
// Each temporal layer is given a fixed fraction of the stream bitrate, see
// `FrameConfig::rate_factor`. Since the layers have different frame rates this
// also determines the relative size of the frames in each layer, see
// `frame_budget_factor`.
//
// This is not a rate allocator; it only produces the input a test needs to
// drive `VideoEncoderInterface` with a temporal layer structure.
class TemporalLayerPatternForTest {
 public:
  struct FrameConfig {
    // The temporal layer this frame belongs to.
    int temporal_id = 0;
    // The buffer this frame should reference, or nullopt if it references
    // nothing. Only the very first frame of the pattern, which has to be
    // encoded as a keyframe, references nothing.
    std::optional<int> reference_buffer;
    // The buffer this frame should be stored in, or nullopt if no later frame
    // will reference this frame.
    std::optional<int> update_buffer;
    // Multiplying the nominal stream bitrate with this factor gives the
    // bitrate of the temporal layer this frame belongs to, which is what the
    // rate controller is asked to hit. The factors of all the layers of a
    // pattern sum to one, so the stream targets the nominal bitrate in total.
    double rate_factor = 1.0;
  };

  // `num_reference_buffers` is the number of buffers the encoder makes
  // available. A dyadic pattern needs one buffer per temporal layer except the
  // topmost one, which is never referenced. If fewer buffers are available,
  // the highest layers that would have updated a buffer stop doing so, and
  // frames that would have referenced them reference the most recently stored
  // frame instead.
  //
  // `bitrate_fractions` holds the fraction of the stream bitrate given to each
  // temporal layer, one entry per layer. The entries need not be normalized,
  // only their relative sizes matter. See `GeometricDistribution` and
  // `LinearDistribution`.
  TemporalLayerPatternForTest(int num_temporal_layers,
                              int num_reference_buffers,
                              std::vector<double> bitrate_fractions);

  // The number of temporal layers this pattern produces frames for.
  int num_temporal_layers() const { return num_temporal_layers_; }

  // The number of frames in one group of pictures, i.e. the period of the
  // pattern. Doubles for every added temporal layer, so a test that wants to
  // observe a given number of base layer frames has to scale the length of
  // its sequence accordingly.
  static int FramesPerGroupOfPictures(int num_temporal_layers);

  // The share of the stream bitrate given to temporal layer `temporal_id`, see
  // `FrameConfig::rate_factor`.
  double rate_factor(int temporal_id) const {
    return rate_factors_[temporal_id];
  }

  // How many nominal per frame bit budgets a single frame of temporal layer
  // `temporal_id` gets, i.e. the share of the bitrate that layer holds divided
  // by the share of the frames it contributes. Lets a caller check up front
  // how large a single frame's bit budget will get; the base layer factor of a
  // strongly skewed distribution grows quickly with the number of layers.
  double frame_budget_factor(int temporal_id) const {
    return frame_budget_factors_[temporal_id];
  }

  // Returns the configuration of the next frame in the pattern.
  FrameConfig NextFrameConfig();

  // Bitrate fractions for a distribution where the per frame bit budget of
  // temporal layer `t` is `ratio` times that of layer `t - 1`. A ratio of one
  // gives every frame the same budget regardless of which layer it belongs to,
  // lower ratios favor the lower layers.
  //
  // The suggested ratio is 0.5, which makes the per frame budget proportional
  // to the distance to the frame being predicted from. Lower temporal layers
  // predict from further away and therefore need more bits to reach the same
  // quality, and they are referenced by more frames, so bits spent there also
  // improve the rest of the group of pictures and, for the base layer, all
  // later groups of pictures as well. Both effects scale with the prediction
  // distance, which halves for every temporal layer.
  static std::vector<double> GeometricDistribution(int num_temporal_layers,
                                                   double ratio);

  // Bitrate fractions for a distribution where the per frame bit budget
  // decreases linearly with the temporal layer id, from `num_temporal_layers`
  // units in the base layer down to a single unit in the topmost layer.
  static std::vector<double> LinearDistribution(int num_temporal_layers);

 private:
  int TemporalIdOf(int frame_index) const;

  const int num_temporal_layers_;
  // The number of temporal layers, counted from the base layer and up, whose
  // frames are stored in a reference buffer.
  const int num_stored_layers_;
  // The share of the stream bitrate held by each temporal layer, indexed by
  // temporal id.
  std::vector<double> rate_factors_;
  // The per frame bit budget of each temporal layer, in nominal per frame
  // budgets, indexed by temporal id.
  std::vector<double> frame_budget_factors_;
  int frame_index_ = 0;
  int last_updated_buffer_ = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_TEST_TEMPORAL_LAYER_PATTERN_FOR_TEST_H_
