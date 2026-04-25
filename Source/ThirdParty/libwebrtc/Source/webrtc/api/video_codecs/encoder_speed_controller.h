/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_ENCODER_SPEED_CONTROLLER_H_
#define API_VIDEO_CODECS_ENCODER_SPEED_CONTROLLER_H_

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

// Utility class intended to help dynamically find the optimal speed settings to
// use for a video encoder. An instance of this class is intended to handle a
// single session at a single resolution. I.e. and new instance should be
// created if the resolution is updated. That also provides the opportunity to
// configure a new set of available speeds, more appropriate for the new
// resolution. If spatial SVC and/or simulcast is used, the caller of this class
// must make sure the frame interval is adjusted if the encodings of a temporal
// unit is serialized.
class EncoderSpeedController {
 public:
  // The `ReferenceClass` allows the controller to pick a separate speed level
  // based on the importance of the frame. Frames that act as references for
  // many subsequent frames typically warrant a higher effort level.
  enum class ReferenceClass : int {
    kKey = 0,       // Key-frames, or long-term references.
    kMain,          // "Normal" delta frames or a temporal base layer
    kIntermediate,  // Reference for a short-live frame tree (e.g T1 in L1T3)
    kNoneReference  // A frame not used as reference sub subsequent frames.
  };
  struct Config {
    struct PsnrProbingSettings {
      enum class Mode {
        // Sample one base layer frame every `sampling_interval`, and sample
        // both alternatives when doing PSNR probing.
        kRegularBaseLayerSampling,
        // Only perform sampling of a base-layer frame when a PSNR probe is
        // needed.
        kOnlyWhenProbing,
      };
      Mode mode;

      // Detfault time between frames that should be sampled for PSNR.
      TimeDelta sampling_interval;
      // The expected ratio of base-layer to non-base-layer frames. E.g. for
      // L1T3 this will be 0.25;
      double average_base_layer_ratio = 1.0;
    };
    // The PSNR settings to used. If not set, PSNR gain levels must not be
    // present in the speed levels. Do not populate if the encoder does not
    // support calculating PSNR.
    std::optional<PsnrProbingSettings> psnr_probing_settings;

    // Represents an assignable speed level, with specific speeds for one or
    // more temporal layers.
    struct SpeedLevel {
      // The actual speed levels (values of the integers below) are
      // implementation specific. It is up to the user to make mappings
      // between these and what the API surface of the encoder looks like,
      // if it is not using integers.

      // Array of speeds, indexed by ReferenceClass.
      std::array<int, 4> speeds;

      // Don't use this speed level if the average QP is lower than `min_qp`.
      std::optional<int> min_qp;
      // Minimum PSNR gain required to go from the previous speed level to this
      // one, or nullopt if no PSNR calculation is required. This value must
      // not be set unless the encoder is capable of encoding a frame twice.
      struct PsnrComparison {
        // The baseline (faster) speed to compare the new `base_layer_speed`
        // speed with.
        int baseline_speed;
        // The min PSNR gain required to move to this speed level, where the
        // PSNR for `alternate_base_layer_speed` is expected to be lower than
        // the PSNR for `base_layer_speed`.
        double psnr_threshold;
      };
      std::optional<PsnrComparison> min_psnr_gain;
    };
    // Ordered vector of speed levels, start with the slowest speed (lower
    // effort) and the increasing the average speed for each entry.
    std::vector<SpeedLevel> speed_levels;

    // An index into `speed_levels` at which the controller should start.
    int start_speed_index;
  };

  // Input data to the controller about the frame that is about the be encoded.
  struct FrameEncodingInfo {
    // The reference class of the frame to be encoded.
    ReferenceClass reference_type;
    // True iff the frame is a repeat of the previous frame (e.g. the frames
    // used during quality convergence of a variable fps screenshare feed).
    bool is_repeat_frame;
    // The capture time of the frame.
    // TODO: webrtc:443906251 - Remove default value once downstream usage
    // is updated.
    Timestamp timestamp = Timestamp::MinusInfinity();
  };

  // Output from the controller, indicates which speed the encoder should be
  // configured with given the frame info that was submitted.
  struct EncodeSettings {
    // Speed the encoder should use for this frame.
    int speed;
    // If set, the encoder should encode this frame twice. FIRST with a speed of
    // `baseline_comparison_speed` and SECONDLY at speed `speed`. The two
    // results should then both be provided in `OnEncodedFrame()`.
    std::optional<int> baseline_comparison_speed;
    // If true, the encoder should calculate the PSNR for this frame - including
    // the second encoding if `baseline_comparison_speed` is set.
    bool calculate_psnr;
  };

  // Data the controller should be fed with after a frame has been encoded,
  // providing info about the resulting encoding.
  struct EncodeResults {
    // The speed setting used for this encoded frame.
    int speed;
    // The time it took to encode the frame.
    TimeDelta encode_time;
    // The _average_ frame QP of the encoded frame.
    int qp;
    // If set, the PSNR of the reconstructed frame vs the original raw frame.
    std::optional<double> psnr;
    // The frame encoding info - same as what was originally given as argument
    // to `GetEncodingSettings()`.
    FrameEncodingInfo frame_info;
  };

  // Creates an instance of the speed controller. This should be called any
  // time the encoder has been recreated e.g. due to a resolution change.
  static std::unique_ptr<EncoderSpeedController> Create(
      const Config& config,
      TimeDelta start_frame_interval);

  virtual ~EncoderSpeedController() = default;

  // Should be called any time the rate targets of the encoder changed.
  // The frame interval (1s/fps) effectively sets the time limit for an encoding
  // operation.
  virtual void SetFrameInterval(TimeDelta frame_interval) = 0;

  // Should be called before each frame to be encoded, and the encoder should
  // thereafter be configured with requested settings.
  virtual EncodeSettings GetEncodeSettings(FrameEncodingInfo frame_info) = 0;

  // TODO: webrtc:443906251 - Remove once downstream usage is gone.
  [[deprecated(
      "Use OnEncodedFrame(EncodeResults, std::optional<EncodeResults>)")]]
  virtual void OnEncodedFrame(EncodeResults results) {
    return OnEncodedFrame(results, std::nullopt);
  }

  // Should be called after each frame has completed encoding. If a baseline
  // comparison speed was set in the `EncodeSettings`, the `baseline_results`
  // parameter should be set with the results corresponding to those settings.
  virtual void OnEncodedFrame(
      EncodeResults results,
      std::optional<EncodeResults> baseline_results) = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_ENCODER_SPEED_CONTROLLER_H_
