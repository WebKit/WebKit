/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_

#include <math.h>

#include <memory>
#include <string>
#include <utility>

#if defined(WEBRTC_ANDROID)
#include "webrtc/modules/video_coding/codecs/test/android_test_initializer.h"
#include "webrtc/sdk/android/src/jni/androidmediadecoder_jni.h"
#include "webrtc/sdk/android/src/jni/androidmediaencoder_jni.h"
#elif defined(WEBRTC_IOS)
#include "webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_decoder.h"
#include "webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_encoder.h"
#endif

#include "webrtc/base/checks.h"
#include "webrtc/base/file.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/test/testsupport/packet_reader.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {
// Maximum number of rate updates (i.e., calls to encoder to change bitrate
// and/or frame rate) for the current tests.
const int kMaxNumRateUpdates = 3;

const int kPercTargetvsActualMismatch = 20;
const int kBaseKeyFrameInterval = 3000;

// Default sequence is foreman (CIF): may be better to use VGA for resize test.
const int kCifWidth = 352;
const int kCifHeight = 288;
const char kFilenameForemanCif[] = "foreman_cif";

// Codec and network settings.
struct CodecParams {
  VideoCodecType codec_type;
  bool hw_codec;
  bool use_single_core;

  int width;
  int height;

  int num_temporal_layers;
  int key_frame_interval;
  bool error_concealment_on;
  bool denoising_on;
  bool frame_dropper_on;
  bool spatial_resize_on;

  float packet_loss_probability;  // [0.0, 1.0].

  std::string filename;
  bool verbose_logging;
};

// Thresholds for the quality metrics.
struct QualityThresholds {
  double min_avg_psnr;
  double min_min_psnr;
  double min_avg_ssim;
  double min_min_ssim;
};

// The sequence of bit rate and frame rate changes for the encoder, the frame
// number where the changes are made, and the total number of frames for the
// test.
struct RateProfile {
  int target_bit_rate[kMaxNumRateUpdates];
  int input_frame_rate[kMaxNumRateUpdates];
  int frame_index_rate_update[kMaxNumRateUpdates + 1];
  int num_frames;
};

// Thresholds for the rate control metrics. The rate mismatch thresholds are
// defined as percentages. |max_time_hit_target| is defined as number of frames,
// after a rate update is made to the encoder, for the encoder to reach within
// |kPercTargetvsActualMismatch| of new target rate. The thresholds are defined
// for each rate update sequence.
struct RateControlThresholds {
  int max_num_dropped_frames;
  int max_key_frame_size_mismatch;
  int max_delta_frame_size_mismatch;
  int max_encoding_rate_mismatch;
  int max_time_hit_target;
  int num_spatial_resizes;
  int num_key_frames;
};

// Should video files be saved persistently to disk for post-run visualization?
struct VisualizationParams {
  bool save_source_y4m;
  bool save_encoded_ivf;
  bool save_decoded_y4m;
};

#if !defined(WEBRTC_IOS)
const int kNumFramesShort = 100;
#endif
const int kNumFramesLong = 299;

// Parameters from VP8 wrapper, which control target size of key frames.
const float kInitialBufferSize = 0.5f;
const float kOptimalBufferSize = 0.6f;
const float kScaleKeyFrameSize = 0.5f;

// Integration test for video processor. Encodes+decodes a clip and
// writes it to the output directory. After completion, quality metrics
// (PSNR and SSIM) and rate control metrics are computed and compared to given
// thresholds, to verify that the quality and encoder response is acceptable.
// The rate control tests allow us to verify the behavior for changing bit rate,
// changing frame rate, frame dropping/spatial resize, and temporal layers.
// The thresholds for the rate control metrics are set to be fairly
// conservative, so failure should only happen when some significant regression
// or breakdown occurs.
class VideoProcessorIntegrationTest : public testing::Test {
 protected:
  VideoProcessorIntegrationTest() {
#if defined(WEBRTC_VIDEOPROCESSOR_INTEGRATIONTEST_HW_CODECS_ENABLED) && \
    defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();

    external_encoder_factory_.reset(
        new webrtc_jni::MediaCodecVideoEncoderFactory());
    external_decoder_factory_.reset(
        new webrtc_jni::MediaCodecVideoDecoderFactory());
#endif
  }
  virtual ~VideoProcessorIntegrationTest() = default;

  void SetUpCodecConfig(const CodecParams& process,
                        const VisualizationParams* visualization_params) {
    if (process.hw_codec) {
#if defined(WEBRTC_VIDEOPROCESSOR_INTEGRATIONTEST_HW_CODECS_ENABLED)
#if defined(WEBRTC_ANDROID)
      // In general, external codecs should be destroyed by the factories that
      // allocated them. For the particular case of the Android
      // MediaCodecVideo{En,De}coderFactory's, however, it turns out that it is
      // fine for the std::unique_ptr to destroy the owned codec directly.
      switch (process.codec_type) {
        case kVideoCodecH264:
          encoder_.reset(external_encoder_factory_->CreateVideoEncoder(
              cricket::VideoCodec(cricket::kH264CodecName)));
          decoder_.reset(
              external_decoder_factory_->CreateVideoDecoder(kVideoCodecH264));
          break;
        case kVideoCodecVP8:
          encoder_.reset(external_encoder_factory_->CreateVideoEncoder(
              cricket::VideoCodec(cricket::kVp8CodecName)));
          decoder_.reset(
              external_decoder_factory_->CreateVideoDecoder(kVideoCodecVP8));
          break;
        case kVideoCodecVP9:
          encoder_.reset(external_encoder_factory_->CreateVideoEncoder(
              cricket::VideoCodec(cricket::kVp9CodecName)));
          decoder_.reset(
              external_decoder_factory_->CreateVideoDecoder(kVideoCodecVP9));
          break;
        default:
          RTC_NOTREACHED();
          break;
      }
#elif defined(WEBRTC_IOS)
      ASSERT_EQ(kVideoCodecH264, process.codec_type)
          << "iOS HW codecs only support H264.";
      encoder_.reset(new H264VideoToolboxEncoder(
          cricket::VideoCodec(cricket::kH264CodecName)));
      decoder_.reset(new H264VideoToolboxDecoder());
#else
      RTC_NOTREACHED() << "Only support HW codecs on Android and iOS.";
#endif
#endif  // WEBRTC_VIDEOPROCESSOR_INTEGRATIONTEST_HW_CODECS_ENABLED
      RTC_CHECK(encoder_) << "HW encoder not successfully created.";
      RTC_CHECK(decoder_) << "HW decoder not successfully created.";
    } else {
      // SW codecs.
      switch (process.codec_type) {
        case kVideoCodecH264:
          encoder_.reset(H264Encoder::Create(
              cricket::VideoCodec(cricket::kH264CodecName)));
          decoder_.reset(H264Decoder::Create());
          break;
        case kVideoCodecVP8:
          encoder_.reset(VP8Encoder::Create());
          decoder_.reset(VP8Decoder::Create());
          break;
        case kVideoCodecVP9:
          encoder_.reset(VP9Encoder::Create());
          decoder_.reset(VP9Decoder::Create());
          break;
        default:
          RTC_NOTREACHED();
          break;
      }
    }

    VideoCodingModule::Codec(process.codec_type, &codec_settings_);

    // Configure input filename.
    config_.input_filename = test::ResourcePath(process.filename, "yuv");
    if (process.verbose_logging)
      printf("Filename: %s\n", process.filename.c_str());
    // Generate an output filename in a safe way.
    config_.output_filename = test::TempFilename(
        test::OutputPath(), "videoprocessor_integrationtest");
    config_.frame_length_in_bytes =
        CalcBufferSize(kI420, process.width, process.height);
    config_.verbose = process.verbose_logging;
    config_.use_single_core = process.use_single_core;
    // Key frame interval and packet loss are set for each test.
    config_.keyframe_interval = process.key_frame_interval;
    config_.networking_config.packet_loss_probability =
        packet_loss_probability_;

    // Configure codec settings.
    config_.codec_settings = &codec_settings_;
    config_.codec_settings->startBitrate = start_bitrate_;
    config_.codec_settings->width = process.width;
    config_.codec_settings->height = process.height;

    // These features may be set depending on the test.
    switch (config_.codec_settings->codecType) {
      case kVideoCodecH264:
        config_.codec_settings->H264()->frameDroppingOn =
            process.frame_dropper_on;
        config_.codec_settings->H264()->keyFrameInterval =
            kBaseKeyFrameInterval;
        break;
      case kVideoCodecVP8:
        config_.codec_settings->VP8()->errorConcealmentOn =
            process.error_concealment_on;
        config_.codec_settings->VP8()->denoisingOn = process.denoising_on;
        config_.codec_settings->VP8()->numberOfTemporalLayers =
            num_temporal_layers_;
        config_.codec_settings->VP8()->frameDroppingOn =
            process.frame_dropper_on;
        config_.codec_settings->VP8()->automaticResizeOn =
            process.spatial_resize_on;
        config_.codec_settings->VP8()->keyFrameInterval = kBaseKeyFrameInterval;
        break;
      case kVideoCodecVP9:
        config_.codec_settings->VP9()->denoisingOn = process.denoising_on;
        config_.codec_settings->VP9()->numberOfTemporalLayers =
            num_temporal_layers_;
        config_.codec_settings->VP9()->frameDroppingOn =
            process.frame_dropper_on;
        config_.codec_settings->VP9()->automaticResizeOn =
            process.spatial_resize_on;
        config_.codec_settings->VP9()->keyFrameInterval = kBaseKeyFrameInterval;
        break;
      default:
        RTC_NOTREACHED();
        break;
    }

    // Create file objects for quality analysis.
    analysis_frame_reader_.reset(new test::YuvFrameReaderImpl(
        config_.input_filename, config_.codec_settings->width,
        config_.codec_settings->height));
    analysis_frame_writer_.reset(new test::YuvFrameWriterImpl(
        config_.output_filename, config_.codec_settings->width,
        config_.codec_settings->height));
    RTC_CHECK(analysis_frame_reader_->Init());
    RTC_CHECK(analysis_frame_writer_->Init());

    if (visualization_params) {
      // clang-format off
      const std::string output_filename_base =
          test::OutputPath() + process.filename +
          "_cd-" + CodecTypeToPayloadName(process.codec_type).value_or("") +
          "_hw-" + std::to_string(process.hw_codec) +
          "_fr-" + std::to_string(start_frame_rate_) +
          "_br-" + std::to_string(static_cast<int>(start_bitrate_));
      // clang-format on
      if (visualization_params->save_source_y4m) {
        source_frame_writer_.reset(new test::Y4mFrameWriterImpl(
            output_filename_base + "_source.y4m", config_.codec_settings->width,
            config_.codec_settings->height, start_frame_rate_));
        RTC_CHECK(source_frame_writer_->Init());
      }
      if (visualization_params->save_encoded_ivf) {
        rtc::File post_encode_file =
            rtc::File::Create(output_filename_base + "_encoded.ivf");
        encoded_frame_writer_ =
            IvfFileWriter::Wrap(std::move(post_encode_file), 0);
      }
      if (visualization_params->save_decoded_y4m) {
        decoded_frame_writer_.reset(new test::Y4mFrameWriterImpl(
            output_filename_base + "_decoded.y4m",
            config_.codec_settings->width, config_.codec_settings->height,
            start_frame_rate_));
        RTC_CHECK(decoded_frame_writer_->Init());
      }
    }

    packet_manipulator_.reset(new test::PacketManipulatorImpl(
        &packet_reader_, config_.networking_config, config_.verbose));
    processor_.reset(new test::VideoProcessorImpl(
        encoder_.get(), decoder_.get(), analysis_frame_reader_.get(),
        analysis_frame_writer_.get(), packet_manipulator_.get(), config_,
        &stats_, source_frame_writer_.get(), encoded_frame_writer_.get(),
        decoded_frame_writer_.get()));
    RTC_CHECK(processor_->Init());
  }

  // Reset quantities after each encoder update, update the target
  // per-frame bandwidth.
  void ResetRateControlMetrics(int num_frames) {
    for (int i = 0; i < num_temporal_layers_; i++) {
      num_frames_per_update_[i] = 0;
      sum_frame_size_mismatch_[i] = 0.0f;
      sum_encoded_frame_size_[i] = 0.0f;
      encoding_bitrate_[i] = 0.0f;
      // Update layer per-frame-bandwidth.
      per_frame_bandwidth_[i] = static_cast<float>(bit_rate_layer_[i]) /
                                static_cast<float>(frame_rate_layer_[i]);
    }
    // Set maximum size of key frames, following setting in the VP8 wrapper.
    float max_key_size = kScaleKeyFrameSize * kOptimalBufferSize * frame_rate_;
    // We don't know exact target size of the key frames (except for first one),
    // but the minimum in libvpx is ~|3 * per_frame_bandwidth| and maximum is
    // set by |max_key_size_  * per_frame_bandwidth|. Take middle point/average
    // as reference for mismatch. Note key frames always correspond to base
    // layer frame in this test.
    target_size_key_frame_ = 0.5 * (3 + max_key_size) * per_frame_bandwidth_[0];
    num_frames_total_ = 0;
    sum_encoded_frame_size_total_ = 0.0f;
    encoding_bitrate_total_ = 0.0f;
    perc_encoding_rate_mismatch_ = 0.0f;
    num_frames_to_hit_target_ = num_frames;
    encoding_rate_within_target_ = false;
    sum_key_frame_size_mismatch_ = 0.0;
    num_key_frames_ = 0;
  }

  // For every encoded frame, update the rate control metrics.
  void UpdateRateControlMetrics(int frame_num, FrameType frame_type) {
    float encoded_size_kbits = processor_->EncodedFrameSize() * 8.0f / 1000.0f;
    // Update layer data.
    // Update rate mismatch relative to per-frame bandwidth for delta frames.
    if (frame_type == kVideoFrameDelta) {
      // TODO(marpan): Should we count dropped (zero size) frames in mismatch?
      sum_frame_size_mismatch_[layer_] +=
          fabs(encoded_size_kbits - per_frame_bandwidth_[layer_]) /
          per_frame_bandwidth_[layer_];
    } else {
      float target_size = (frame_num == 1) ? target_size_key_frame_initial_
                                           : target_size_key_frame_;
      sum_key_frame_size_mismatch_ +=
          fabs(encoded_size_kbits - target_size) / target_size;
      num_key_frames_ += 1;
    }
    sum_encoded_frame_size_[layer_] += encoded_size_kbits;
    // Encoding bitrate per layer: from the start of the update/run to the
    // current frame.
    encoding_bitrate_[layer_] = sum_encoded_frame_size_[layer_] *
                                frame_rate_layer_[layer_] /
                                num_frames_per_update_[layer_];
    // Total encoding rate: from the start of the update/run to current frame.
    sum_encoded_frame_size_total_ += encoded_size_kbits;
    encoding_bitrate_total_ =
        sum_encoded_frame_size_total_ * frame_rate_ / num_frames_total_;
    perc_encoding_rate_mismatch_ =
        100 * fabs(encoding_bitrate_total_ - bit_rate_) / bit_rate_;
    if (perc_encoding_rate_mismatch_ < kPercTargetvsActualMismatch &&
        !encoding_rate_within_target_) {
      num_frames_to_hit_target_ = num_frames_total_;
      encoding_rate_within_target_ = true;
    }
  }

  // Verify expected behavior of rate control and print out data.
  void VerifyRateControlMetrics(int update_index,
                                int max_key_frame_size_mismatch,
                                int max_delta_frame_size_mismatch,
                                int max_encoding_rate_mismatch,
                                int max_time_hit_target,
                                int max_num_dropped_frames,
                                int num_spatial_resizes,
                                int num_key_frames) {
    int num_dropped_frames = processor_->NumberDroppedFrames();
    int num_resize_actions = processor_->NumberSpatialResizes();
    printf(
        "For update #: %d,\n"
        " Target Bitrate: %d,\n"
        " Encoding bitrate: %f,\n"
        " Frame rate: %d \n",
        update_index, bit_rate_, encoding_bitrate_total_, frame_rate_);
    printf(
        " Number of frames to approach target rate: %d, \n"
        " Number of dropped frames: %d, \n"
        " Number of spatial resizes: %d, \n",
        num_frames_to_hit_target_, num_dropped_frames, num_resize_actions);
    EXPECT_LE(perc_encoding_rate_mismatch_, max_encoding_rate_mismatch);
    if (num_key_frames_ > 0) {
      int perc_key_frame_size_mismatch =
          100 * sum_key_frame_size_mismatch_ / num_key_frames_;
      printf(
          " Number of Key frames: %d \n"
          " Key frame rate mismatch: %d \n",
          num_key_frames_, perc_key_frame_size_mismatch);
      EXPECT_LE(perc_key_frame_size_mismatch, max_key_frame_size_mismatch);
    }
    printf("\n");
    printf("Rates statistics for Layer data \n");
    for (int i = 0; i < num_temporal_layers_; i++) {
      printf("Temporal layer #%d \n", i);
      int perc_frame_size_mismatch =
          100 * sum_frame_size_mismatch_[i] / num_frames_per_update_[i];
      int perc_encoding_rate_mismatch =
          100 * fabs(encoding_bitrate_[i] - bit_rate_layer_[i]) /
          bit_rate_layer_[i];
      printf(
          " Target Layer Bit rate: %f \n"
          " Layer frame rate: %f, \n"
          " Layer per frame bandwidth: %f, \n"
          " Layer Encoding bit rate: %f, \n"
          " Layer Percent frame size mismatch: %d,  \n"
          " Layer Percent encoding rate mismatch: %d, \n"
          " Number of frame processed per layer: %d \n",
          bit_rate_layer_[i], frame_rate_layer_[i], per_frame_bandwidth_[i],
          encoding_bitrate_[i], perc_frame_size_mismatch,
          perc_encoding_rate_mismatch, num_frames_per_update_[i]);
      EXPECT_LE(perc_frame_size_mismatch, max_delta_frame_size_mismatch);
      EXPECT_LE(perc_encoding_rate_mismatch, max_encoding_rate_mismatch);
    }
    printf("\n");
    EXPECT_LE(num_frames_to_hit_target_, max_time_hit_target);
    EXPECT_LE(num_dropped_frames, max_num_dropped_frames);
    EXPECT_EQ(num_resize_actions, num_spatial_resizes);
    EXPECT_EQ(num_key_frames_, num_key_frames);
  }

  void VerifyQuality(const test::QualityMetricsResult& psnr_result,
                     const test::QualityMetricsResult& ssim_result,
                     const QualityThresholds& quality_thresholds) {
    EXPECT_GT(psnr_result.average, quality_thresholds.min_avg_psnr);
    EXPECT_GT(psnr_result.min, quality_thresholds.min_min_psnr);
    EXPECT_GT(ssim_result.average, quality_thresholds.min_avg_ssim);
    EXPECT_GT(ssim_result.min, quality_thresholds.min_min_ssim);
  }

  // Layer index corresponding to frame number, for up to 3 layers.
  void LayerIndexForFrame(int frame_number) {
    if (num_temporal_layers_ == 1) {
      layer_ = 0;
    } else if (num_temporal_layers_ == 2) {
      // layer 0:  0     2     4 ...
      // layer 1:     1     3
      if (frame_number % 2 == 0) {
        layer_ = 0;
      } else {
        layer_ = 1;
      }
    } else if (num_temporal_layers_ == 3) {
      // layer 0:  0            4            8 ...
      // layer 1:        2            6
      // layer 2:     1      3     5      7
      if (frame_number % 4 == 0) {
        layer_ = 0;
      } else if ((frame_number + 2) % 4 == 0) {
        layer_ = 1;
      } else if ((frame_number + 1) % 2 == 0) {
        layer_ = 2;
      }
    } else {
      RTC_NOTREACHED() << "Max 3 layers are supported.";
    }
  }

  // Set the bitrate and frame rate per layer, for up to 3 layers.
  void SetLayerRates() {
    RTC_DCHECK_LE(num_temporal_layers_, 3);
    for (int i = 0; i < num_temporal_layers_; i++) {
      float bit_rate_ratio =
          kVp8LayerRateAlloction[num_temporal_layers_ - 1][i];
      if (i > 0) {
        float bit_rate_delta_ratio =
            kVp8LayerRateAlloction[num_temporal_layers_ - 1][i] -
            kVp8LayerRateAlloction[num_temporal_layers_ - 1][i - 1];
        bit_rate_layer_[i] = bit_rate_ * bit_rate_delta_ratio;
      } else {
        bit_rate_layer_[i] = bit_rate_ * bit_rate_ratio;
      }
      frame_rate_layer_[i] =
          frame_rate_ / static_cast<float>(1 << (num_temporal_layers_ - 1));
    }
    if (num_temporal_layers_ == 3) {
      frame_rate_layer_[2] = frame_rate_ / 2.0f;
    }
  }

  // Processes all frames in the clip and verifies the result.
  void ProcessFramesAndVerify(QualityThresholds quality_thresholds,
                              RateProfile rate_profile,
                              CodecParams process,
                              RateControlThresholds* rc_thresholds,
                              const VisualizationParams* visualization_params) {
    // Codec/config settings.
    start_bitrate_ = rate_profile.target_bit_rate[0];
    start_frame_rate_ = rate_profile.input_frame_rate[0];
    packet_loss_probability_ = process.packet_loss_probability;
    num_temporal_layers_ = process.num_temporal_layers;
    SetUpCodecConfig(process, visualization_params);
    // Update the layers and the codec with the initial rates.
    bit_rate_ = rate_profile.target_bit_rate[0];
    frame_rate_ = rate_profile.input_frame_rate[0];
    SetLayerRates();
    // Set the initial target size for key frame.
    target_size_key_frame_initial_ =
        0.5 * kInitialBufferSize * bit_rate_layer_[0];
    processor_->SetRates(bit_rate_, frame_rate_);

    // Process each frame, up to |num_frames|.
    int num_frames = rate_profile.num_frames;
    int update_index = 0;
    ResetRateControlMetrics(
        rate_profile.frame_index_rate_update[update_index + 1]);
    int frame_number = 0;
    FrameType frame_type = kVideoFrameDelta;
    while (processor_->ProcessFrame(frame_number) &&
           frame_number < num_frames) {
      // Get the layer index for the frame |frame_number|.
      LayerIndexForFrame(frame_number);
      // Get the frame_type.
      frame_type = processor_->EncodedFrameType();
      // Counter for whole sequence run.
      ++frame_number;
      // Counters for each rate update.
      ++num_frames_per_update_[layer_];
      ++num_frames_total_;
      UpdateRateControlMetrics(frame_number, frame_type);
      // If we hit another/next update, verify stats for current state and
      // update layers and codec with new rates.
      if (frame_number ==
          rate_profile.frame_index_rate_update[update_index + 1]) {
        VerifyRateControlMetrics(
            update_index,
            rc_thresholds[update_index].max_key_frame_size_mismatch,
            rc_thresholds[update_index].max_delta_frame_size_mismatch,
            rc_thresholds[update_index].max_encoding_rate_mismatch,
            rc_thresholds[update_index].max_time_hit_target,
            rc_thresholds[update_index].max_num_dropped_frames,
            rc_thresholds[update_index].num_spatial_resizes,
            rc_thresholds[update_index].num_key_frames);
        // Update layer rates and the codec with new rates.
        ++update_index;
        bit_rate_ = rate_profile.target_bit_rate[update_index];
        frame_rate_ = rate_profile.input_frame_rate[update_index];
        SetLayerRates();
        ResetRateControlMetrics(
            rate_profile.frame_index_rate_update[update_index + 1]);
        processor_->SetRates(bit_rate_, frame_rate_);
      }
    }
    VerifyRateControlMetrics(
        update_index, rc_thresholds[update_index].max_key_frame_size_mismatch,
        rc_thresholds[update_index].max_delta_frame_size_mismatch,
        rc_thresholds[update_index].max_encoding_rate_mismatch,
        rc_thresholds[update_index].max_time_hit_target,
        rc_thresholds[update_index].max_num_dropped_frames,
        rc_thresholds[update_index].num_spatial_resizes,
        rc_thresholds[update_index].num_key_frames);
    EXPECT_EQ(num_frames, frame_number);
    EXPECT_EQ(num_frames + 1, static_cast<int>(stats_.stats_.size()));

    // Release encoder and decoder to make sure they have finished processing:
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());

    // Close the analysis files before we use them for SSIM/PSNR calculations.
    analysis_frame_reader_->Close();
    analysis_frame_writer_->Close();

    // Close visualization files.
    if (source_frame_writer_) {
      source_frame_writer_->Close();
    }
    if (encoded_frame_writer_) {
      encoded_frame_writer_->Close();
    }
    if (decoded_frame_writer_) {
      decoded_frame_writer_->Close();
    }

    // TODO(marpan): Should compute these quality metrics per SetRates update.
    test::QualityMetricsResult psnr_result, ssim_result;
    EXPECT_EQ(0, test::I420MetricsFromFiles(config_.input_filename.c_str(),
                                            config_.output_filename.c_str(),
                                            config_.codec_settings->width,
                                            config_.codec_settings->height,
                                            &psnr_result, &ssim_result));
    printf("PSNR avg: %f, min: %f\nSSIM avg: %f, min: %f\n",
           psnr_result.average, psnr_result.min, ssim_result.average,
           ssim_result.min);
    stats_.PrintSummary();
    VerifyQuality(psnr_result, ssim_result, quality_thresholds);

    // Remove analysis file.
    if (remove(config_.output_filename.c_str()) < 0) {
      fprintf(stderr, "Failed to remove temporary file!\n");
    }
  }

  static void SetCodecParams(CodecParams* process_settings,
                             VideoCodecType codec_type,
                             bool hw_codec,
                             bool use_single_core,
                             float packet_loss_probability,
                             int key_frame_interval,
                             int num_temporal_layers,
                             bool error_concealment_on,
                             bool denoising_on,
                             bool frame_dropper_on,
                             bool spatial_resize_on,
                             int width,
                             int height,
                             const std::string& filename,
                             bool verbose_logging) {
    process_settings->codec_type = codec_type;
    process_settings->hw_codec = hw_codec;
    process_settings->use_single_core = use_single_core;
    process_settings->packet_loss_probability = packet_loss_probability;
    process_settings->key_frame_interval = key_frame_interval;
    process_settings->num_temporal_layers = num_temporal_layers,
    process_settings->error_concealment_on = error_concealment_on;
    process_settings->denoising_on = denoising_on;
    process_settings->frame_dropper_on = frame_dropper_on;
    process_settings->spatial_resize_on = spatial_resize_on;
    process_settings->width = width;
    process_settings->height = height;
    process_settings->filename = filename;
    process_settings->verbose_logging = verbose_logging;
  }

  static void SetCodecParams(CodecParams* process_settings,
                             VideoCodecType codec_type,
                             bool hw_codec,
                             bool use_single_core,
                             float packet_loss_probability,
                             int key_frame_interval,
                             int num_temporal_layers,
                             bool error_concealment_on,
                             bool denoising_on,
                             bool frame_dropper_on,
                             bool spatial_resize_on) {
    SetCodecParams(process_settings, codec_type, hw_codec, use_single_core,
                   packet_loss_probability, key_frame_interval,
                   num_temporal_layers, error_concealment_on, denoising_on,
                   frame_dropper_on, spatial_resize_on, kCifWidth, kCifHeight,
                   kFilenameForemanCif, false /* verbose_logging */);
  }

  static void SetQualityThresholds(QualityThresholds* quality_thresholds,
                                   double min_avg_psnr,
                                   double min_min_psnr,
                                   double min_avg_ssim,
                                   double min_min_ssim) {
    quality_thresholds->min_avg_psnr = min_avg_psnr;
    quality_thresholds->min_min_psnr = min_min_psnr;
    quality_thresholds->min_avg_ssim = min_avg_ssim;
    quality_thresholds->min_min_ssim = min_min_ssim;
  }

  static void SetRateProfile(RateProfile* rate_profile,
                             int update_index,
                             int bit_rate,
                             int frame_rate,
                             int frame_index_rate_update) {
    rate_profile->target_bit_rate[update_index] = bit_rate;
    rate_profile->input_frame_rate[update_index] = frame_rate;
    rate_profile->frame_index_rate_update[update_index] =
        frame_index_rate_update;
  }

  static void SetRateControlThresholds(RateControlThresholds* rc_thresholds,
                                       int update_index,
                                       int max_num_dropped_frames,
                                       int max_key_frame_size_mismatch,
                                       int max_delta_frame_size_mismatch,
                                       int max_encoding_rate_mismatch,
                                       int max_time_hit_target,
                                       int num_spatial_resizes,
                                       int num_key_frames) {
    rc_thresholds[update_index].max_num_dropped_frames = max_num_dropped_frames;
    rc_thresholds[update_index].max_key_frame_size_mismatch =
        max_key_frame_size_mismatch;
    rc_thresholds[update_index].max_delta_frame_size_mismatch =
        max_delta_frame_size_mismatch;
    rc_thresholds[update_index].max_encoding_rate_mismatch =
        max_encoding_rate_mismatch;
    rc_thresholds[update_index].max_time_hit_target = max_time_hit_target;
    rc_thresholds[update_index].num_spatial_resizes = num_spatial_resizes;
    rc_thresholds[update_index].num_key_frames = num_key_frames;
  }

  // Codecs.
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<cricket::WebRtcVideoEncoderFactory> external_encoder_factory_;
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<cricket::WebRtcVideoDecoderFactory> external_decoder_factory_;
  VideoCodec codec_settings_;

  // Helper objects.
  std::unique_ptr<test::FrameReader> analysis_frame_reader_;
  std::unique_ptr<test::FrameWriter> analysis_frame_writer_;
  test::PacketReader packet_reader_;
  std::unique_ptr<test::PacketManipulator> packet_manipulator_;
  test::Stats stats_;
  test::TestConfig config_;
  // Must be destroyed before |encoder_| and |decoder_|.
  std::unique_ptr<test::VideoProcessor> processor_;

  // Visualization objects.
  std::unique_ptr<test::FrameWriter> source_frame_writer_;
  std::unique_ptr<IvfFileWriter> encoded_frame_writer_;
  std::unique_ptr<test::FrameWriter> decoded_frame_writer_;

  // Quantities defined/updated for every encoder rate update.
  // Some quantities defined per temporal layer (at most 3 layers in this test).
  int num_frames_per_update_[3];
  float sum_frame_size_mismatch_[3];
  float sum_encoded_frame_size_[3];
  float encoding_bitrate_[3];
  float per_frame_bandwidth_[3];
  float bit_rate_layer_[3];
  float frame_rate_layer_[3];
  int num_frames_total_;
  float sum_encoded_frame_size_total_;
  float encoding_bitrate_total_;
  float perc_encoding_rate_mismatch_;
  int num_frames_to_hit_target_;
  bool encoding_rate_within_target_;
  int bit_rate_;
  int frame_rate_;
  int layer_;
  float target_size_key_frame_initial_;
  float target_size_key_frame_;
  float sum_key_frame_size_mismatch_;
  int num_key_frames_;
  float start_bitrate_;
  int start_frame_rate_;

  // Codec and network settings.
  float packet_loss_probability_;
  int num_temporal_layers_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
