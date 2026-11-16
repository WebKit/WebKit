/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "api/units/time_delta.h"
#include "api/video/resolution.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {
using FrameType = VideoEncoderInterface::FrameType;

// NOTE: This class is still under development and may change without notice.
class VideoEncoderFactoryInterface {
 public:
  enum class RateControlMode { kCqp, kCbr };

  // Identifies the settings in `StaticEncoderSettings::Cbr` that an encoder
  // may or may not honor, see
  // `Capabilities::BitrateControl::supported_cbr_settings()`.
  enum class CbrSetting {
    // Covers both `max_buffer_size` and `target_buffer_size`, which are only
    // meaningful together.
    kBufferSizes,
    kMaxIntraBitrateFactor
  };

  class Capabilities {
   public:
    class PredictionConstraints {
     public:
      enum class BufferSpaceType {
        kMultiInstance,  // multiple independent sets of buffers
        kMultiKeyframe,  // single set of buffers, but can store multiple
                         // keyframes simultaneously.
        kSingleKeyframe  // single set of buffers, can only store one keyframe
                         // at a time.
      };

      PredictionConstraints() = default;

      int num_buffers() const { return num_buffers_; }
      void set_num_buffers(int val) { num_buffers_ = val; }

      int max_references() const { return max_references_; }
      void set_max_references(int val) { max_references_ = val; }

      int max_temporal_layers() const { return max_temporal_layers_; }
      void set_max_temporal_layers(int val) { max_temporal_layers_ = val; }

      BufferSpaceType buffer_space_type() const { return buffer_space_type_; }
      void set_buffer_space_type(BufferSpaceType val) {
        buffer_space_type_ = val;
      }

      int max_spatial_layers() const { return max_spatial_layers_; }
      void set_max_spatial_layers(int val) { max_spatial_layers_ = val; }

      const std::vector<Rational>& scaling_factors() const {
        return scaling_factors_;
      }
      void set_scaling_factors(std::vector<Rational> val) {
        scaling_factors_ = std::move(val);
      }

      const std::set<FrameType>& supported_frame_types() const {
        return supported_frame_types_;
      }
      void set_supported_frame_types(std::set<FrameType> val) {
        supported_frame_types_ = std::move(val);
      }

     private:
      int num_buffers_ = 0;
      int max_references_ = 0;
      int max_temporal_layers_ = 0;
      BufferSpaceType buffer_space_type_ = BufferSpaceType::kSingleKeyframe;
      int max_spatial_layers_ = 0;
      std::vector<Rational> scaling_factors_;
      std::set<FrameType> supported_frame_types_;
    };

    class InputConstraints {
     public:
      InputConstraints() = default;

      Resolution min() const { return min_; }
      void set_min(Resolution val) { min_ = val; }

      Resolution max() const { return max_; }
      void set_max(Resolution val) { max_ = val; }

      int pixel_alignment() const { return pixel_alignment_; }
      void set_pixel_alignment(int val) { pixel_alignment_ = val; }

      const std::vector<VideoFrameBuffer::Type>& input_formats() const {
        return input_formats_;
      }
      void set_input_formats(std::vector<VideoFrameBuffer::Type> val) {
        input_formats_ = std::move(val);
      }

     private:
      Resolution min_;
      Resolution max_;
      int pixel_alignment_ = 0;
      std::vector<VideoFrameBuffer::Type> input_formats_;
    };

    class BitrateControl {
     public:
      BitrateControl() = default;

      std::pair<int, int> qp_range() const { return qp_range_; }
      int min_qp() const { return qp_range_.first; }
      int max_qp() const { return qp_range_.second; }
      void set_qp_range(int min_qp, int max_qp) {
        qp_range_ = {min_qp, max_qp};
      }

      const std::set<RateControlMode>& rc_modes() const { return rc_modes_; }
      void set_rc_modes(std::set<RateControlMode> val) {
        rc_modes_ = std::move(val);
      }

      // The settings in `StaticEncoderSettings::Cbr` that this encoder honors.
      // Settings not listed here are ignored, in which case the encoder uses
      // an implementation defined behavior instead. Only meaningful if
      // `rc_modes()` contains `RateControlMode::kCbr`.
      const std::set<CbrSetting>& supported_cbr_settings() const {
        return supported_cbr_settings_;
      }
      void set_supported_cbr_settings(std::set<CbrSetting> val) {
        supported_cbr_settings_ = std::move(val);
      }

     private:
      std::pair<int, int> qp_range_ = {0, 0};
      std::set<RateControlMode> rc_modes_;
      std::set<CbrSetting> supported_cbr_settings_;
    };

    class Performance {
     public:
      Performance() = default;

      bool encode_on_calling_thread() const {
        return encode_on_calling_thread_;
      }
      void set_encode_on_calling_thread(bool val) {
        encode_on_calling_thread_ = val;
      }

      std::pair<int, int> min_max_effort_level() const {
        return min_max_effort_level_;
      }
      void set_min_max_effort_level(int min_effort, int max_effort) {
        min_max_effort_level_ = {min_effort, max_effort};
      }

     private:
      bool encode_on_calling_thread_ = false;
      std::pair<int, int> min_max_effort_level_ = {0, 0};
    };

    Capabilities() = default;

    const PredictionConstraints& prediction_constraints() const {
      return prediction_constraints_;
    }
    void set_prediction_constraints(PredictionConstraints val) {
      prediction_constraints_ = std::move(val);
    }

    const InputConstraints& input_constraints() const {
      return input_constraints_;
    }
    void set_input_constraints(InputConstraints val) {
      input_constraints_ = std::move(val);
    }

    const std::vector<EncodingFormat>& encoding_formats() const {
      return encoding_formats_;
    }
    void set_encoding_formats(std::vector<EncodingFormat> val) {
      encoding_formats_ = std::move(val);
    }

    const BitrateControl& bitrate_control() const { return bitrate_control_; }
    void set_bitrate_control(BitrateControl val) {
      bitrate_control_ = std::move(val);
    }

    const Performance& performance() const { return performance_; }
    void set_performance(Performance val) { performance_ = std::move(val); }

   private:
    PredictionConstraints prediction_constraints_;
    InputConstraints input_constraints_;
    std::vector<EncodingFormat> encoding_formats_;
    BitrateControl bitrate_control_;
    Performance performance_;
  };

  class StaticEncoderSettings {
   public:
    struct Cqp {};

    // Settings for constant bitrate mode, in which the encoder picks the QP
    // needed to meet the per frame bit budget implied by the target bitrate
    // and frame duration given in `VideoEncoderInterface::FrameEncodeSettings`.
    // Individual frames will be larger or smaller than that budget, and these
    // settings say how much of that variation the caller can absorb, expressed
    // as the transmission delay it adds when the stream is sent over a channel
    // with a capacity equal to the target bitrate.
    // An encoder is not required to honor all of these settings, see
    // `Capabilities::BitrateControl::supported_cbr_settings()`. Settings that
    // are out of range are rejected, i.e. `CreateEncoder` returns nullptr.
    struct Cbr {
      // The largest transmission delay the encoder may cause. Producing data
      // faster than the target bitrate allows adds delay, and the encoder is
      // expected to stay within this bound even at the cost of quality. Must
      // be finite and greater than zero.
      TimeDelta max_buffer_size;
      // The transmission delay the encoder should operate at, leaving the
      // difference up to `max_buffer_size` as headroom for bursts, such as
      // intra frames or sudden changes in the content. Must be greater than
      // zero and at most `max_buffer_size`.
      TimeDelta target_buffer_size;
      // How much data an intra frame may use, expressed as a multiple of the
      // bit budget of a frame on the same temporal layer. Intra frames are
      // typically much larger than inter frames, and this setting controls the
      // trade off between the quality of such frames and the delay they add.
      // Must be finite and at least 1.0. The default matches the lower bound
      // used by the VP8, VP9 and AV1 encoders in WebRTC.
      double max_intra_bitrate_factor = 3.0;
    };

    StaticEncoderSettings() = default;

    Resolution max_encode_dimensions() const { return max_encode_dimensions_; }
    void set_max_encode_dimensions(Resolution val) {
      max_encode_dimensions_ = val;
    }

    EncodingFormat encoding_format() const { return encoding_format_; }
    void set_encoding_format(EncodingFormat val) { encoding_format_ = val; }

    const std::variant<Cqp, Cbr>& rc_mode() const { return rc_mode_; }
    void set_rc_mode(std::variant<Cqp, Cbr> val) { rc_mode_ = std::move(val); }

    int max_number_of_threads() const { return max_number_of_threads_; }
    void set_max_number_of_threads(int val) { max_number_of_threads_ = val; }

   private:
    Resolution max_encode_dimensions_;
    EncodingFormat encoding_format_;
    std::variant<Cqp, Cbr> rc_mode_;
    int max_number_of_threads_ = 1;
  };

  virtual ~VideoEncoderFactoryInterface() = default;

  virtual std::string CodecName() const = 0;
  virtual std::string ImplementationName() const = 0;
  virtual std::map<std::string, std::string> CodecSpecifics() const = 0;

  virtual Capabilities GetEncoderCapabilities() const = 0;
  virtual std::unique_ptr<VideoEncoderInterface> CreateEncoder(
      const StaticEncoderSettings& settings,
      const std::map<std::string, std::string>& encoder_specific_settings) = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_
