/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_config.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "vpx/vpx_codec.h"

namespace {

class DatarateTestLarge
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 public:
  DatarateTestLarge() : EncoderTest(GET_PARAM(0)) {}

  virtual ~DatarateTestLarge() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    first_drop_ = 0;
    bits_total_ = 0;
    duration_ = 0.0;
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
    gf_boost_ = 0;
    use_roi_ = false;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(VP8E_SET_NOISE_SENSITIVITY, denoiser_on_);
      encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);
      encoder->Control(VP8E_SET_GF_CBR_BOOST_PCT, gf_boost_);
    }

    if (use_roi_) {
      encoder->Control(VP8E_SET_ROI_MAP, &roi_);
    }

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
      encoder->Control(VP8E_SET_NOISE_SENSITIVITY, denoiser_on_);
    }

    const vpx_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    vpx_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    // TODO(jimbankoski): Remove these lines when the issue:
    // http://code.google.com/p/webm/issues/detail?id=496 is fixed.
    // For now the codec assumes buffer starts at starting buffer rate
    // plus one frame's time.
    if (last_pts_ == 0) duration = 1;

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    /* Test the buffer model here before subtracting the frame. Do so because
     * the way the leaky bucket model works in libvpx is to allow the buffer to
     * empty - and then stop showing frames until we've got enough bits to
     * show one. As noted in comment below (issue 495), this does not currently
     * apply to key frames. For now exclude key frames in condition below. */
    const bool key_frame =
        (pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;
    if (!key_frame) {
      ASSERT_GE(bits_in_buffer_model_, 0)
          << "Buffer Underrun at frame " << pkt->data.frame.pts;
    }

    const int64_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Subtract from the buffer the bits associated with a played back frame.
    bits_in_buffer_model_ -= frame_size_in_bits;

    // Update the running total of bits for end of test datarate checks.
    bits_total_ += frame_size_in_bits;

    // If first drop not set and we have a drop set it to this time.
    if (!first_drop_ && duration > 1) first_drop_ = last_pts_ + 1;

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;

    // We update this so that we can calculate the datarate minus the last
    // frame encoded in the file.
    bits_in_last_frame_ = frame_size_in_bits;

    ++frame_number_;
  }

  virtual void EndPassHook(void) {
    if (bits_total_) {
      const double file_size_in_kb = bits_total_ / 1000.;  // bits per kilobit

      duration_ = (last_pts_ + 1) * timebase_;

      // Effective file datarate includes the time spent prebuffering.
      effective_datarate_ = (bits_total_ - bits_in_last_frame_) / 1000.0 /
                            (cfg_.rc_buf_initial_sz / 1000.0 + duration_);

      file_datarate_ = file_size_in_kb / duration_;
    }
  }

  vpx_codec_pts_t last_pts_;
  int64_t bits_in_buffer_model_;
  double timebase_;
  int frame_number_;
  vpx_codec_pts_t first_drop_;
  int64_t bits_total_;
  double duration_;
  double file_datarate_;
  double effective_datarate_;
  int64_t bits_in_last_frame_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
  int set_cpu_used_;
  int gf_boost_;
  bool use_roi_;
  vpx_roi_map_t roi_;
};

#if CONFIG_TEMPORAL_DENOISING
// Check basic datarate targeting, for a single bitrate, but loop over the
// various denoiser settings.
TEST_P(DatarateTestLarge, DenoiserLevels) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int j = 1; j < 5; ++j) {
    // Run over the denoiser levels.
    // For the temporal denoiser (#if CONFIG_TEMPORAL_DENOISING) the level j
    // refers to the 4 denoiser modes: denoiserYonly, denoiserOnYUV,
    // denoiserOnAggressive, and denoiserOnAdaptive.
    denoiser_on_ = j;
    cfg_.rc_target_bitrate = 300;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";

    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestLarge, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}
#endif  // CONFIG_TEMPORAL_DENOISING

TEST_P(DatarateTestLarge, BasicBufferModel) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // 2 pass cbr datarate control has a bug hidden by the small # of
  // frames selected in this encode. The problem is that even if the buffer is
  // negative we produce a keyframe on a cutscene. Ignoring datarate
  // constraints
  // TODO(jimbankoski): ( Fix when issue
  // http://code.google.com/p/webm/issues/detail?id=495 is addressed. )
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // There is an issue for low bitrates in real-time mode, where the
  // effective_datarate slightly overshoots the target bitrate.
  // This is same the issue as noted about (#495).
  // TODO(jimbankoski/marpan): Update test to run for lower bitrates (< 100),
  // when the issue is resolved.
  for (int i = 100; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

TEST_P(DatarateTestLarge, ChangingDropFrameThresh) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_max_quantizer = 36;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.kf_mode = VPX_KF_DISABLED;

  const int frame_count = 40;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, frame_count);

  // Here we check that the first dropped frame gets earlier and earlier
  // as the drop frame threshold is increased.

  const int kDropFrameThreshTestStep = 30;
  vpx_codec_pts_t last_drop = frame_count;
  for (int i = 1; i < 91; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
  }
}

TEST_P(DatarateTestLarge, DropFramesMultiThreads) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 30;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}

class DatarateTestRealTime : public DatarateTestLarge {
 public:
  virtual ~DatarateTestRealTime() {}
};

#if CONFIG_TEMPORAL_DENOISING
// Check basic datarate targeting, for a single bitrate, but loop over the
// various denoiser settings.
TEST_P(DatarateTestRealTime, DenoiserLevels) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int j = 1; j < 5; ++j) {
    // Run over the denoiser levels.
    // For the temporal denoiser (#if CONFIG_TEMPORAL_DENOISING) the level j
    // refers to the 4 denoiser modes: denoiserYonly, denoiserOnYUV,
    // denoiserOnAggressive, and denoiserOnAdaptive.
    denoiser_on_ = j;
    cfg_.rc_target_bitrate = 300;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestRealTime, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}
#endif  // CONFIG_TEMPORAL_DENOISING

TEST_P(DatarateTestRealTime, BasicBufferModel) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // 2 pass cbr datarate control has a bug hidden by the small # of
  // frames selected in this encode. The problem is that even if the buffer is
  // negative we produce a keyframe on a cutscene, ignoring datarate
  // constraints
  // TODO(jimbankoski): Fix when issue
  // http://bugs.chromium.org/p/webm/issues/detail?id=495 is addressed.
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // There is an issue for low bitrates in real-time mode, where the
  // effective_datarate slightly overshoots the target bitrate.
  // This is same the issue as noted above (#495).
  // TODO(jimbankoski/marpan): Update test to run for lower bitrates (< 100),
  // when the issue is resolved.
  for (int i = 100; i <= 700; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

TEST_P(DatarateTestRealTime, ChangingDropFrameThresh) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_max_quantizer = 36;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.kf_mode = VPX_KF_DISABLED;

  const int frame_count = 40;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, frame_count);

  // Check that the first dropped frame gets earlier and earlier
  // as the drop frame threshold is increased.

  const int kDropFrameThreshTestStep = 30;
  vpx_codec_pts_t last_drop = frame_count;
  for (int i = 1; i < 91; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
  }
}

TEST_P(DatarateTestRealTime, DropFramesMultiThreads) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 30;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // Encode using multiple threads.
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}

TEST_P(DatarateTestRealTime, RegionOfInterest) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // Encode using multiple threads.
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  cfg_.rc_target_bitrate = 450;
  cfg_.g_w = 352;
  cfg_.g_h = 288;

  ResetModel();

  // Set ROI parameters
  use_roi_ = true;
  memset(&roi_, 0, sizeof(roi_));

  roi_.rows = (cfg_.g_h + 15) / 16;
  roi_.cols = (cfg_.g_w + 15) / 16;

  roi_.delta_q[0] = 0;
  roi_.delta_q[1] = -20;
  roi_.delta_q[2] = 0;
  roi_.delta_q[3] = 0;

  roi_.delta_lf[0] = 0;
  roi_.delta_lf[1] = -20;
  roi_.delta_lf[2] = 0;
  roi_.delta_lf[3] = 0;

  roi_.static_threshold[0] = 0;
  roi_.static_threshold[1] = 1000;
  roi_.static_threshold[2] = 0;
  roi_.static_threshold[3] = 0;

  // Use 2 states: 1 is center square, 0 is the rest.
  roi_.roi_map =
      (uint8_t *)calloc(roi_.rows * roi_.cols, sizeof(*roi_.roi_map));
  for (unsigned int i = 0; i < roi_.rows; ++i) {
    for (unsigned int j = 0; j < roi_.cols; ++j) {
      if (i > (roi_.rows >> 2) && i < ((roi_.rows * 3) >> 2) &&
          j > (roi_.cols >> 2) && j < ((roi_.cols * 3) >> 2)) {
        roi_.roi_map[i * roi_.cols + j] = 1;
      }
    }
  }

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";

  free(roi_.roi_map);
}

TEST_P(DatarateTestRealTime, GFBoost) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_error_resilient = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // Apply a gf boost.
  gf_boost_ = 50;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}

class DatarateTestVP9Large
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 public:
  DatarateTestVP9Large() : EncoderTest(GET_PARAM(0)) {}

 protected:
  virtual ~DatarateTestVP9Large() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    tot_frame_number_ = 0;
    first_drop_ = 0;
    num_drops_ = 0;
    // Denoiser is off by default.
    denoiser_on_ = 0;
    // For testing up to 3 layers.
    for (int i = 0; i < 3; ++i) {
      bits_total_[i] = 0;
    }
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
    frame_parallel_decoding_mode_ = 1;
    use_roi_ = false;
  }

  //
  // Frame flags and layer id for temporal layers.
  //

  // For two layers, test pattern is:
  //   1     3
  // 0    2     .....
  // For three layers, test pattern is:
  //   1      3    5      7
  //      2           6
  // 0          4            ....
  // LAST is always update on base/layer 0, GOLDEN is updated on layer 1.
  // For this 3 layer example, the 2nd enhancement layer (layer 2) updates
  // the altref frame.
  int SetFrameFlags(int frame_num, int num_temp_layers) {
    int frame_flags = 0;
    if (num_temp_layers == 2) {
      if (frame_num % 2 == 0) {
        // Layer 0: predict from L and ARF, update L.
        frame_flags =
            VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      } else {
        // Layer 1: predict from L, G and ARF, and update G.
        frame_flags = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
                      VP8_EFLAG_NO_UPD_ENTROPY;
      }
    } else if (num_temp_layers == 3) {
      if (frame_num % 4 == 0) {
        // Layer 0: predict from L and ARF; update L.
        frame_flags =
            VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF;
      } else if ((frame_num - 2) % 4 == 0) {
        // Layer 1: predict from L, G, ARF; update G.
        frame_flags = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      } else if ((frame_num - 1) % 2 == 0) {
        // Layer 2: predict from L, G, ARF; update ARF.
        frame_flags = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST;
      }
    }
    return frame_flags;
  }

  int SetLayerId(int frame_num, int num_temp_layers) {
    int layer_id = 0;
    if (num_temp_layers == 2) {
      if (frame_num % 2 == 0) {
        layer_id = 0;
      } else {
        layer_id = 1;
      }
    } else if (num_temp_layers == 3) {
      if (frame_num % 4 == 0) {
        layer_id = 0;
      } else if ((frame_num - 2) % 4 == 0) {
        layer_id = 1;
      } else if ((frame_num - 1) % 2 == 0) {
        layer_id = 2;
      }
    }
    return layer_id;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
    }

    encoder->Control(VP9E_SET_NOISE_SENSITIVITY, denoiser_on_);
    encoder->Control(VP9E_SET_TILE_COLUMNS, (cfg_.g_threads >> 1));
    encoder->Control(VP9E_SET_FRAME_PARALLEL_DECODING,
                     frame_parallel_decoding_mode_);

    if (use_roi_) {
      encoder->Control(VP9E_SET_ROI_MAP, &roi_);
    }

    if (cfg_.ts_number_layers > 1) {
      if (video->frame() == 0) {
        encoder->Control(VP9E_SET_SVC, 1);
      }
      vpx_svc_layer_id_t layer_id;
      layer_id.spatial_layer_id = 0;
      frame_flags_ = SetFrameFlags(video->frame(), cfg_.ts_number_layers);
      layer_id.temporal_layer_id =
          SetLayerId(video->frame(), cfg_.ts_number_layers);
      encoder->Control(VP9E_SET_SVC_LAYER_ID, &layer_id);
    }
    const vpx_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    vpx_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    if (duration > 1) {
      // If first drop not set and we have a drop set it to this time.
      if (!first_drop_) first_drop_ = last_pts_ + 1;
      // Update the number of frame drops.
      num_drops_ += static_cast<int>(duration - 1);
      // Update counter for total number of frames (#frames input to encoder).
      // Needed for setting the proper layer_id below.
      tot_frame_number_ += static_cast<int>(duration - 1);
    }

    int layer = SetLayerId(tot_frame_number_, cfg_.ts_number_layers);

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    // Buffer should not go negative.
    ASSERT_GE(bits_in_buffer_model_, 0)
        << "Buffer Underrun at frame " << pkt->data.frame.pts;

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Update the total encoded bits. For temporal layers, update the cumulative
    // encoded bits per layer.
    for (int i = layer; i < static_cast<int>(cfg_.ts_number_layers); ++i) {
      bits_total_[i] += frame_size_in_bits;
    }

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++frame_number_;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) {
    for (int layer = 0; layer < static_cast<int>(cfg_.ts_number_layers);
         ++layer) {
      duration_ = (last_pts_ + 1) * timebase_;
      if (bits_total_[layer]) {
        // Effective file datarate:
        effective_datarate_[layer] = (bits_total_[layer] / 1000.0) / duration_;
      }
    }
  }

  vpx_codec_pts_t last_pts_;
  double timebase_;
  int frame_number_;      // Counter for number of non-dropped/encoded frames.
  int tot_frame_number_;  // Counter for total number of input frames.
  int64_t bits_total_[3];
  double duration_;
  double effective_datarate_[3];
  int set_cpu_used_;
  int64_t bits_in_buffer_model_;
  vpx_codec_pts_t first_drop_;
  int num_drops_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
  int frame_parallel_decoding_mode_;
  bool use_roi_;
  vpx_roi_map_t roi_;
};

// Check basic rate targeting for VBR mode with 0 lag.
TEST_P(DatarateTestVP9Large, BasicRateTargetingVBRLagZero) {
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = VPX_VBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  for (int i = 400; i <= 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.75)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.30)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for VBR mode with non-zero lag.
TEST_P(DatarateTestVP9Large, BasicRateTargetingVBRLagNonZero) {
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = VPX_VBR;
  // For non-zero lag, rate control will work (be within bounds) for
  // real-time mode.
  if (deadline_ == VPX_DL_REALTIME) {
    cfg_.g_lag_in_frames = 15;
  } else {
    cfg_.g_lag_in_frames = 0;
  }

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  for (int i = 400; i <= 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.75)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.30)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for VBR mode with non-zero lag, with
// frame_parallel_decoding_mode off. This enables the adapt_coeff/mode/mv probs
// since error_resilience is off.
TEST_P(DatarateTestVP9Large, BasicRateTargetingVBRLagNonZeroFrameParDecOff) {
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = VPX_VBR;
  // For non-zero lag, rate control will work (be within bounds) for
  // real-time mode.
  if (deadline_ == VPX_DL_REALTIME) {
    cfg_.g_lag_in_frames = 15;
  } else {
    cfg_.g_lag_in_frames = 0;
  }

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  for (int i = 400; i <= 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    frame_parallel_decoding_mode_ = 0;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.75)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.30)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR mode.
TEST_P(DatarateTestVP9Large, BasicRateTargeting) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int i = 150; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR mode, with frame_parallel_decoding_mode
// off( and error_resilience off).
TEST_P(DatarateTestVP9Large, BasicRateTargetingFrameParDecOff) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.g_error_resilient = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int i = 150; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    frame_parallel_decoding_mode_ = 0;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR mode, with 2 threads and dropped frames.
TEST_P(DatarateTestVP9Large, BasicRateTargetingDropFramesMultiThreads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 30;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  // Encode using multiple threads.
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic rate targeting for CBR.
TEST_P(DatarateTestVP9Large, BasicRateTargeting444) {
  ::libvpx_test::Y4mVideoSource video("rush_hour_444.y4m", 0, 140);

  cfg_.g_profile = 1;
  cfg_.g_timebase = video.timebase();

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;

  for (int i = 250; i < 900; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_[0] * 0.80)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_[0] * 1.15)
        << " The datarate for the file missed the target!"
        << cfg_.rc_target_bitrate << " " << effective_datarate_;
  }
}

// Check that (1) the first dropped frame gets earlier and earlier
// as the drop frame threshold is increased, and (2) that the total number of
// frame drops does not decrease as we increase frame drop threshold.
// Use a lower qp-max to force some frame drops.
TEST_P(DatarateTestVP9Large, ChangingDropFrameThresh) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 50;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.g_lag_in_frames = 0;
  // TODO(marpan): Investigate datarate target failures with a smaller keyframe
  // interval (128).
  cfg_.kf_max_dist = 9999;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  const int kDropFrameThreshTestStep = 30;
  for (int j = 50; j <= 150; j += 100) {
    cfg_.rc_target_bitrate = j;
    vpx_codec_pts_t last_drop = 140;
    int last_num_drops = 0;
    for (int i = 10; i < 100; i += kDropFrameThreshTestStep) {
      cfg_.rc_dropframe_thresh = i;
      ResetModel();
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.25)
          << " The datarate for the file is greater than target by too much!";
      ASSERT_LE(first_drop_, last_drop)
          << " The first dropped frame for drop_thresh " << i
          << " > first dropped frame for drop_thresh "
          << i - kDropFrameThreshTestStep;
      ASSERT_GE(num_drops_, last_num_drops * 0.85)
          << " The number of dropped frames for drop_thresh " << i
          << " < number of dropped frames for drop_thresh "
          << i - kDropFrameThreshTestStep;
      last_drop = first_drop_;
      last_num_drops = num_drops_;
    }
  }
}

// Check basic rate targeting for 2 temporal layers.
TEST_P(DatarateTestVP9Large, BasicRateTargeting2TemporalLayers) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  // 2 Temporal layers, no spatial layers: Framerate decimation (2, 1).
  cfg_.ss_number_layers = 1;
  cfg_.ts_number_layers = 2;
  cfg_.ts_rate_decimator[0] = 2;
  cfg_.ts_rate_decimator[1] = 1;

  cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  if (deadline_ == VPX_DL_REALTIME) cfg_.g_error_resilient = 1;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  for (int i = 200; i <= 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    // 60-40 bitrate allocation for 2 temporal layers.
    cfg_.layer_target_bitrate[0] = 60 * cfg_.rc_target_bitrate / 100;
    cfg_.layer_target_bitrate[1] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int j = 0; j < static_cast<int>(cfg_.ts_number_layers); ++j) {
      ASSERT_GE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 0.85)
          << " The datarate for the file is lower than target by too much, "
             "for layer: "
          << j;
      ASSERT_LE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 1.15)
          << " The datarate for the file is greater than target by too much, "
             "for layer: "
          << j;
    }
  }
}

// Check basic rate targeting for 3 temporal layers.
TEST_P(DatarateTestVP9Large, BasicRateTargeting3TemporalLayers) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  // 3 Temporal layers, no spatial layers: Framerate decimation (4, 2, 1).
  cfg_.ss_number_layers = 1;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;

  cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  for (int i = 200; i <= 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    // 40-20-40 bitrate allocation for 3 temporal layers.
    cfg_.layer_target_bitrate[0] = 40 * cfg_.rc_target_bitrate / 100;
    cfg_.layer_target_bitrate[1] = 60 * cfg_.rc_target_bitrate / 100;
    cfg_.layer_target_bitrate[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int j = 0; j < static_cast<int>(cfg_.ts_number_layers); ++j) {
      // TODO(yaowu): Work out more stable rc control strategy and
      //              Adjust the thresholds to be tighter than .75.
      ASSERT_GE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 0.75)
          << " The datarate for the file is lower than target by too much, "
             "for layer: "
          << j;
      // TODO(yaowu): Work out more stable rc control strategy and
      //              Adjust the thresholds to be tighter than 1.25.
      ASSERT_LE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 1.25)
          << " The datarate for the file is greater than target by too much, "
             "for layer: "
          << j;
    }
  }
}

// Check basic rate targeting for 3 temporal layers, with frame dropping.
// Only for one (low) bitrate with lower max_quantizer, and somewhat higher
// frame drop threshold, to force frame dropping.
TEST_P(DatarateTestVP9Large, BasicRateTargeting3TemporalLayersFrameDropping) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  // Set frame drop threshold and rc_max_quantizer to force some frame drops.
  cfg_.rc_dropframe_thresh = 20;
  cfg_.rc_max_quantizer = 45;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  // 3 Temporal layers, no spatial layers: Framerate decimation (4, 2, 1).
  cfg_.ss_number_layers = 1;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;

  cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  // 40-20-40 bitrate allocation for 3 temporal layers.
  cfg_.layer_target_bitrate[0] = 40 * cfg_.rc_target_bitrate / 100;
  cfg_.layer_target_bitrate[1] = 60 * cfg_.rc_target_bitrate / 100;
  cfg_.layer_target_bitrate[2] = cfg_.rc_target_bitrate;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  for (int j = 0; j < static_cast<int>(cfg_.ts_number_layers); ++j) {
    ASSERT_GE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 0.85)
        << " The datarate for the file is lower than target by too much, "
           "for layer: "
        << j;
    ASSERT_LE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 1.15)
        << " The datarate for the file is greater than target by too much, "
           "for layer: "
        << j;
    // Expect some frame drops in this test: for this 200 frames test,
    // expect at least 10% and not more than 60% drops.
    ASSERT_GE(num_drops_, 20);
    ASSERT_LE(num_drops_, 130);
  }
}

class DatarateTestVP9RealTime : public DatarateTestVP9Large {
 public:
  virtual ~DatarateTestVP9RealTime() {}
};

// Check VP9 region of interest feature.
TEST_P(DatarateTestVP9RealTime, RegionOfInterest) {
  if (deadline_ != VPX_DL_REALTIME || set_cpu_used_ < 5) return;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);

  cfg_.rc_target_bitrate = 450;
  cfg_.g_w = 352;
  cfg_.g_h = 288;

  ResetModel();

  // Set ROI parameters
  use_roi_ = true;
  memset(&roi_, 0, sizeof(roi_));

  roi_.rows = (cfg_.g_h + 7) / 8;
  roi_.cols = (cfg_.g_w + 7) / 8;

  roi_.delta_q[1] = -20;
  roi_.delta_lf[1] = -20;
  memset(roi_.ref_frame, -1, sizeof(roi_.ref_frame));
  roi_.ref_frame[1] = 1;

  // Use 2 states: 1 is center square, 0 is the rest.
  roi_.roi_map = reinterpret_cast<uint8_t *>(
      calloc(roi_.rows * roi_.cols, sizeof(*roi_.roi_map)));
  ASSERT_TRUE(roi_.roi_map != NULL);

  for (unsigned int i = 0; i < roi_.rows; ++i) {
    for (unsigned int j = 0; j < roi_.cols; ++j) {
      if (i > (roi_.rows >> 2) && i < ((roi_.rows * 3) >> 2) &&
          j > (roi_.cols >> 2) && j < ((roi_.cols * 3) >> 2)) {
        roi_.roi_map[i * roi_.cols + j] = 1;
      }
    }
  }

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_[0] * 0.90)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, effective_datarate_[0] * 1.4)
      << " The datarate for the file missed the target!";

  free(roi_.roi_map);
}

#if CONFIG_VP9_TEMPORAL_DENOISING
class DatarateTestVP9LargeDenoiser : public DatarateTestVP9Large {
 public:
  virtual ~DatarateTestVP9LargeDenoiser() {}
};

// Check basic datarate targeting, for a single bitrate, when denoiser is on.
TEST_P(DatarateTestVP9LargeDenoiser, LowNoise) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: denoiserYonly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // Turn on the denoiser.
  denoiser_on_ = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic datarate targeting, for a single bitrate, when denoiser is on,
// for clip with high noise level. Use 2 threads.
TEST_P(DatarateTestVP9LargeDenoiser, HighNoise) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.g_threads = 2;

  ::libvpx_test::Y4mVideoSource video("noisy_clip_640_360.y4m", 0, 200);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: kDenoiserOnYOnly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 1000;
  ResetModel();
  // Turn on the denoiser.
  denoiser_on_ = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic datarate targeting, for a single bitrate, when denoiser is on,
// for 1280x720 clip with 4 threads.
TEST_P(DatarateTestVP9LargeDenoiser, 4threads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.g_threads = 4;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 300);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: denoiserYonly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 1000;
  ResetModel();
  // Turn on the denoiser.
  denoiser_on_ = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.29)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestVP9LargeDenoiser, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: denoiserYonly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}
#endif  // CONFIG_VP9_TEMPORAL_DENOISING

static void assign_layer_bitrates(vpx_codec_enc_cfg_t *const enc_cfg,
                                  const vpx_svc_extra_cfg_t *svc_params,
                                  int spatial_layers, int temporal_layers,
                                  int temporal_layering_mode,
                                  int *layer_target_avg_bandwidth,
                                  int64_t *bits_in_buffer_model) {
  int sl, spatial_layer_target;
  float total = 0;
  float alloc_ratio[VPX_MAX_LAYERS] = { 0 };
  float framerate = 30.0;
  for (sl = 0; sl < spatial_layers; ++sl) {
    if (svc_params->scaling_factor_den[sl] > 0) {
      alloc_ratio[sl] = (float)(svc_params->scaling_factor_num[sl] * 1.0 /
                                svc_params->scaling_factor_den[sl]);
      total += alloc_ratio[sl];
    }
  }
  for (sl = 0; sl < spatial_layers; ++sl) {
    enc_cfg->ss_target_bitrate[sl] = spatial_layer_target =
        (unsigned int)(enc_cfg->rc_target_bitrate * alloc_ratio[sl] / total);
    const int index = sl * temporal_layers;
    if (temporal_layering_mode == 3) {
      enc_cfg->layer_target_bitrate[index] = spatial_layer_target >> 1;
      enc_cfg->layer_target_bitrate[index + 1] =
          (spatial_layer_target >> 1) + (spatial_layer_target >> 2);
      enc_cfg->layer_target_bitrate[index + 2] = spatial_layer_target;
    } else if (temporal_layering_mode == 2) {
      enc_cfg->layer_target_bitrate[index] = spatial_layer_target * 2 / 3;
      enc_cfg->layer_target_bitrate[index + 1] = spatial_layer_target;
    } else if (temporal_layering_mode <= 1) {
      enc_cfg->layer_target_bitrate[index] = spatial_layer_target;
    }
  }
  for (sl = 0; sl < spatial_layers; ++sl) {
    for (int tl = 0; tl < temporal_layers; ++tl) {
      const int layer = sl * temporal_layers + tl;
      float layer_framerate = framerate;
      if (temporal_layers == 2 && tl == 0) layer_framerate = framerate / 2;
      if (temporal_layers == 3 && tl == 0) layer_framerate = framerate / 4;
      if (temporal_layers == 3 && tl == 1) layer_framerate = framerate / 2;
      layer_target_avg_bandwidth[layer] = static_cast<int>(
          enc_cfg->layer_target_bitrate[layer] * 1000.0 / layer_framerate);
      bits_in_buffer_model[layer] =
          enc_cfg->layer_target_bitrate[layer] * enc_cfg->rc_buf_initial_sz;
    }
  }
}

static void CheckLayerRateTargeting(vpx_codec_enc_cfg_t *const cfg,
                                    int number_spatial_layers,
                                    int number_temporal_layers,
                                    double *file_datarate,
                                    double thresh_overshoot,
                                    double thresh_undershoot) {
  for (int sl = 0; sl < number_spatial_layers; ++sl)
    for (int tl = 0; tl < number_temporal_layers; ++tl) {
      const int layer = sl * number_temporal_layers + tl;
      ASSERT_GE(cfg->layer_target_bitrate[layer],
                file_datarate[layer] * thresh_overshoot)
          << " The datarate for the file exceeds the target by too much!";
      ASSERT_LE(cfg->layer_target_bitrate[layer],
                file_datarate[layer] * thresh_undershoot)
          << " The datarate for the file is lower than the target by too much!";
    }
}

class DatarateOnePassCbrSvc
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 public:
  DatarateOnePassCbrSvc() : EncoderTest(GET_PARAM(0)) {
    memset(&svc_params_, 0, sizeof(svc_params_));
  }
  virtual ~DatarateOnePassCbrSvc() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    speed_setting_ = GET_PARAM(2);
    ResetModel();
  }
  virtual void ResetModel() {
    last_pts_ = 0;
    duration_ = 0.0;
    mismatch_psnr_ = 0.0;
    mismatch_nframes_ = 0;
    denoiser_on_ = 0;
    tune_content_ = 0;
    base_speed_setting_ = 5;
    spatial_layer_id_ = 0;
    temporal_layer_id_ = 0;
    update_pattern_ = 0;
    memset(bits_in_buffer_model_, 0, sizeof(bits_in_buffer_model_));
    memset(bits_total_, 0, sizeof(bits_total_));
    memset(layer_target_avg_bandwidth_, 0, sizeof(layer_target_avg_bandwidth_));
    dynamic_drop_layer_ = false;
    change_bitrate_ = false;
    last_pts_ref_ = 0;
  }
  virtual void BeginPassHook(unsigned int /*pass*/) {}

  // Example pattern for spatial layers and 2 temporal layers used in the
  // bypass/flexible mode. The pattern corresponds to the pattern
  // VP9E_TEMPORAL_LAYERING_MODE_0101 (temporal_layering_mode == 2) used in
  // non-flexible mode, except that we disable inter-layer prediction.
  void set_frame_flags_bypass_mode(
      int tl, int num_spatial_layers, int is_key_frame,
      vpx_svc_ref_frame_config_t *ref_frame_config) {
    for (int sl = 0; sl < num_spatial_layers; ++sl) {
      if (!tl) {
        if (!sl) {
          ref_frame_config->frame_flags[sl] =
              VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF |
              VP8_EFLAG_NO_UPD_ARF;
        } else {
          if (is_key_frame) {
            ref_frame_config->frame_flags[sl] =
                VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
                VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
          } else {
            ref_frame_config->frame_flags[sl] =
                VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF |
                VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF;
          }
        }
      } else if (tl == 1) {
        if (!sl) {
          ref_frame_config->frame_flags[sl] =
              VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
              VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
        } else {
          ref_frame_config->frame_flags[sl] =
              VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_LAST |
              VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_GF;
        }
      }
      if (tl == 0) {
        ref_frame_config->lst_fb_idx[sl] = sl;
        if (sl) {
          if (is_key_frame) {
            ref_frame_config->lst_fb_idx[sl] = sl - 1;
            ref_frame_config->gld_fb_idx[sl] = sl;
          } else {
            ref_frame_config->gld_fb_idx[sl] = sl - 1;
          }
        } else {
          ref_frame_config->gld_fb_idx[sl] = 0;
        }
        ref_frame_config->alt_fb_idx[sl] = 0;
      } else if (tl == 1) {
        ref_frame_config->lst_fb_idx[sl] = sl;
        ref_frame_config->gld_fb_idx[sl] = num_spatial_layers + sl - 1;
        ref_frame_config->alt_fb_idx[sl] = num_spatial_layers + sl;
      }
    }
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      int i;
      for (i = 0; i < VPX_MAX_LAYERS; ++i) {
        svc_params_.max_quantizers[i] = 63;
        svc_params_.min_quantizers[i] = 0;
      }
      svc_params_.speed_per_layer[0] = base_speed_setting_;
      for (i = 1; i < VPX_SS_MAX_LAYERS; ++i) {
        svc_params_.speed_per_layer[i] = speed_setting_;
      }

      encoder->Control(VP9E_SET_NOISE_SENSITIVITY, denoiser_on_);
      encoder->Control(VP9E_SET_SVC, 1);
      encoder->Control(VP9E_SET_SVC_PARAMETERS, &svc_params_);
      encoder->Control(VP8E_SET_CPUUSED, speed_setting_);
      encoder->Control(VP9E_SET_TILE_COLUMNS, 0);
      encoder->Control(VP8E_SET_MAX_INTRA_BITRATE_PCT, 300);
      encoder->Control(VP9E_SET_TILE_COLUMNS, (cfg_.g_threads >> 1));
      encoder->Control(VP9E_SET_ROW_MT, 1);
      encoder->Control(VP8E_SET_STATIC_THRESHOLD, 1);
      encoder->Control(VP9E_SET_TUNE_CONTENT, tune_content_);
    }

    if (update_pattern_ && video->frame() >= 100) {
      vpx_svc_layer_id_t layer_id;
      if (video->frame() == 100) {
        cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;
        encoder->Config(&cfg_);
      }
      // Set layer id since the pattern changed.
      layer_id.spatial_layer_id = 0;
      layer_id.temporal_layer_id = (video->frame() % 2 != 0);
      encoder->Control(VP9E_SET_SVC_LAYER_ID, &layer_id);
      set_frame_flags_bypass_mode(layer_id.temporal_layer_id,
                                  number_spatial_layers_, 0, &ref_frame_config);
      encoder->Control(VP9E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config);
    }

    if (change_bitrate_ && video->frame() == 200) {
      duration_ = (last_pts_ + 1) * timebase_;
      for (int sl = 0; sl < number_spatial_layers_; ++sl) {
        for (int tl = 0; tl < number_temporal_layers_; ++tl) {
          const int layer = sl * number_temporal_layers_ + tl;
          const double file_size_in_kb = bits_total_[layer] / 1000.;
          file_datarate_[layer] = file_size_in_kb / duration_;
        }
      }

      CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                              number_temporal_layers_, file_datarate_, 0.78,
                              1.15);

      memset(file_datarate_, 0, sizeof(file_datarate_));
      memset(bits_total_, 0, sizeof(bits_total_));
      int64_t bits_in_buffer_model_tmp[VPX_MAX_LAYERS];
      last_pts_ref_ = last_pts_;
      // Set new target bitarate.
      cfg_.rc_target_bitrate = cfg_.rc_target_bitrate >> 1;
      // Buffer level should not reset on dynamic bitrate change.
      memcpy(bits_in_buffer_model_tmp, bits_in_buffer_model_,
             sizeof(bits_in_buffer_model_));
      assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                            cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                            layer_target_avg_bandwidth_, bits_in_buffer_model_);
      memcpy(bits_in_buffer_model_, bits_in_buffer_model_tmp,
             sizeof(bits_in_buffer_model_));

      // Change config to update encoder with new bitrate configuration.
      encoder->Config(&cfg_);
    }

    if (dynamic_drop_layer_) {
      if (video->frame() == 100) {
        // Change layer bitrates to set top layer to 0. This will trigger skip
        // encoding/dropping of top spatial layer.
        cfg_.rc_target_bitrate -= cfg_.layer_target_bitrate[2];
        cfg_.layer_target_bitrate[2] = 0;
        encoder->Config(&cfg_);
      } else if (video->frame() == 300) {
        // Change layer bitrate on top layer to non-zero to start encoding it
        // again.
        cfg_.layer_target_bitrate[2] = 500;
        cfg_.rc_target_bitrate += cfg_.layer_target_bitrate[2];
        encoder->Config(&cfg_);
      }
    }
    const vpx_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void PostEncodeFrameHook(::libvpx_test::Encoder *encoder) {
    vpx_svc_layer_id_t layer_id;
    encoder->Control(VP9E_GET_SVC_LAYER_ID, &layer_id);
    spatial_layer_id_ = layer_id.spatial_layer_id;
    temporal_layer_id_ = layer_id.temporal_layer_id;
    // Update buffer with per-layer target frame bandwidth, this is done
    // for every frame passed to the encoder (encoded or dropped).
    // For temporal layers, update the cumulative buffer level.
    for (int sl = 0; sl < number_spatial_layers_; ++sl) {
      for (int tl = temporal_layer_id_; tl < number_temporal_layers_; ++tl) {
        const int layer = sl * number_temporal_layers_ + tl;
        bits_in_buffer_model_[layer] +=
            static_cast<int64_t>(layer_target_avg_bandwidth_[layer]);
      }
    }
  }

  vpx_codec_err_t parse_superframe_index(const uint8_t *data, size_t data_sz,
                                         uint32_t sizes[8], int *count) {
    uint8_t marker;
    marker = *(data + data_sz - 1);
    *count = 0;
    if ((marker & 0xe0) == 0xc0) {
      const uint32_t frames = (marker & 0x7) + 1;
      const uint32_t mag = ((marker >> 3) & 0x3) + 1;
      const size_t index_sz = 2 + mag * frames;
      // This chunk is marked as having a superframe index but doesn't have
      // enough data for it, thus it's an invalid superframe index.
      if (data_sz < index_sz) return VPX_CODEC_CORRUPT_FRAME;
      {
        const uint8_t marker2 = *(data + data_sz - index_sz);
        // This chunk is marked as having a superframe index but doesn't have
        // the matching marker byte at the front of the index therefore it's an
        // invalid chunk.
        if (marker != marker2) return VPX_CODEC_CORRUPT_FRAME;
      }
      {
        uint32_t i, j;
        const uint8_t *x = &data[data_sz - index_sz + 1];
        for (i = 0; i < frames; ++i) {
          uint32_t this_sz = 0;

          for (j = 0; j < mag; ++j) this_sz |= (*x++) << (j * 8);
          sizes[i] = this_sz;
        }
        *count = frames;
      }
    }
    return VPX_CODEC_OK;
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    uint32_t sizes[8] = { 0 };
    int count = 0;
    last_pts_ = pkt->data.frame.pts;
    const bool key_frame =
        (pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;
    parse_superframe_index(static_cast<const uint8_t *>(pkt->data.frame.buf),
                           pkt->data.frame.sz, sizes, &count);
    if (!dynamic_drop_layer_) ASSERT_EQ(count, number_spatial_layers_);
    for (int sl = 0; sl < number_spatial_layers_; ++sl) {
      sizes[sl] = sizes[sl] << 3;
      // Update the total encoded bits per layer.
      // For temporal layers, update the cumulative encoded bits per layer.
      for (int tl = temporal_layer_id_; tl < number_temporal_layers_; ++tl) {
        const int layer = sl * number_temporal_layers_ + tl;
        bits_total_[layer] += static_cast<int64_t>(sizes[sl]);
        // Update the per-layer buffer level with the encoded frame size.
        bits_in_buffer_model_[layer] -= static_cast<int64_t>(sizes[sl]);
        // There should be no buffer underrun, except on the base
        // temporal layer, since there may be key frames there.
        if (!key_frame && tl > 0) {
          ASSERT_GE(bits_in_buffer_model_[layer], 0)
              << "Buffer Underrun at frame " << pkt->data.frame.pts;
        }
      }

      ASSERT_EQ(pkt->data.frame.width[sl],
                top_sl_width_ * svc_params_.scaling_factor_num[sl] /
                    svc_params_.scaling_factor_den[sl]);

      ASSERT_EQ(pkt->data.frame.height[sl],
                top_sl_height_ * svc_params_.scaling_factor_num[sl] /
                    svc_params_.scaling_factor_den[sl]);
    }
  }

  virtual void EndPassHook(void) {
    if (change_bitrate_) last_pts_ = last_pts_ - last_pts_ref_;
    duration_ = (last_pts_ + 1) * timebase_;
    for (int sl = 0; sl < number_spatial_layers_; ++sl) {
      for (int tl = 0; tl < number_temporal_layers_; ++tl) {
        const int layer = sl * number_temporal_layers_ + tl;
        const double file_size_in_kb = bits_total_[layer] / 1000.;
        file_datarate_[layer] = file_size_in_kb / duration_;
      }
    }
  }

  virtual void MismatchHook(const vpx_image_t *img1, const vpx_image_t *img2) {
    double mismatch_psnr = compute_psnr(img1, img2);
    mismatch_psnr_ += mismatch_psnr;
    ++mismatch_nframes_;
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }

  vpx_codec_pts_t last_pts_;
  int64_t bits_in_buffer_model_[VPX_MAX_LAYERS];
  double timebase_;
  int64_t bits_total_[VPX_MAX_LAYERS];
  double duration_;
  double file_datarate_[VPX_MAX_LAYERS];
  size_t bits_in_last_frame_;
  vpx_svc_extra_cfg_t svc_params_;
  int speed_setting_;
  double mismatch_psnr_;
  int mismatch_nframes_;
  int denoiser_on_;
  int tune_content_;
  int base_speed_setting_;
  int spatial_layer_id_;
  int temporal_layer_id_;
  int number_spatial_layers_;
  int number_temporal_layers_;
  int layer_target_avg_bandwidth_[VPX_MAX_LAYERS];
  bool dynamic_drop_layer_;
  unsigned int top_sl_width_;
  unsigned int top_sl_height_;
  vpx_svc_ref_frame_config_t ref_frame_config;
  int update_pattern_;
  bool change_bitrate_;
  vpx_codec_pts_t last_pts_ref_;
};

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and 1
// temporal layer, with screen content mode on and same speed setting for all
// layers.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SL1TLScreenContent1) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 1;
  cfg_.ts_rate_decimator[0] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 0;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  top_sl_width_ = 1280;
  top_sl_height_ = 720;
  cfg_.rc_target_bitrate = 500;
  ResetModel();
  tune_content_ = 1;
  base_speed_setting_ = speed_setting_;
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
}

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and
// 3 temporal layers. Run CIF clip with 1 thread.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SL3TL) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  // TODO(marpan): Check that effective_datarate for each layer hits the
  // layer target_bitrate.
  for (int i = 200; i <= 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                          cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                          layer_target_avg_bandwidth_, bits_in_buffer_model_);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                            number_temporal_layers_, file_datarate_, 0.78,
                            1.15);
#if CONFIG_VP9_DECODER
    // Number of temporal layers > 1, so half of the frames in this SVC pattern
    // will be non-reference frame and hence encoder will avoid loopfilter.
    // Since frame dropper is off, we can expect 200 (half of the sequence)
    // mismatched frames.
    EXPECT_EQ(static_cast<unsigned int>(200), GetMismatchFrames());
#endif
  }
}

// Check basic rate targeting for 1 pass CBR SVC with denoising.
// 2 spatial layers and 3 temporal layer. Run HD clip with 2 threads.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SL3TLDenoiserOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 2;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  // TODO(marpan): Check that effective_datarate for each layer hits the
  // layer target_bitrate.
  // For SVC, noise_sen = 1 means denoising only the top spatial layer
  // noise_sen = 2 means denoising the two top spatial layers.
  for (int noise_sen = 1; noise_sen <= 2; noise_sen++) {
    for (int i = 600; i <= 1000; i += 200) {
      cfg_.rc_target_bitrate = i;
      ResetModel();
      denoiser_on_ = noise_sen;
      assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                            cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                            layer_target_avg_bandwidth_, bits_in_buffer_model_);
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                              number_temporal_layers_, file_datarate_, 0.78,
                              1.15);
#if CONFIG_VP9_DECODER
      // Number of temporal layers > 1, so half of the frames in this SVC
      // pattern
      // will be non-reference frame and hence encoder will avoid loopfilter.
      // Since frame dropper is off, we can expect 200 (half of the sequence)
      // mismatched frames.
      EXPECT_EQ(static_cast<unsigned int>(200), GetMismatchFrames());
#endif
    }
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and 3
// temporal layers. Run CIF clip with 1 thread, and few short key frame periods.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SL3TLSmallKf) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_target_bitrate = 400;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  // For this 3 temporal layer case, pattern repeats every 4 frames, so choose
  // 4 key neighboring key frame periods (so key frame will land on 0-2-1-2).
  for (int j = 64; j <= 67; j++) {
    cfg_.kf_max_dist = j;
    ResetModel();
    assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                          cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                          layer_target_avg_bandwidth_, bits_in_buffer_model_);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                            number_temporal_layers_, file_datarate_, 0.78,
                            1.15);
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and
// 3 temporal layers. Run HD clip with 4 threads.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SL3TL4Threads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 4;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  top_sl_width_ = 1280;
  top_sl_height_ = 720;
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
#if CONFIG_VP9_DECODER
  // Number of temporal layers > 1, so half of the frames in this SVC pattern
  // will be non-reference frame and hence encoder will avoid loopfilter.
  // Since frame dropper is off, we can expect 30 (half of the sequence)
  // mismatched frames.
  EXPECT_EQ(static_cast<unsigned int>(30), GetMismatchFrames());
#endif
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Run CIF clip with 1 thread.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SL3TL) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
#if CONFIG_VP9_DECODER
  // Number of temporal layers > 1, so half of the frames in this SVC pattern
  // will be non-reference frame and hence encoder will avoid loopfilter.
  // Since frame dropper is off, we can expect 200 (half of the sequence)
  // mismatched frames.
  EXPECT_EQ(static_cast<unsigned int>(200), GetMismatchFrames());
#endif
}

// Check rate targeting for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers, changing the target bitrate at the middle of encoding.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SL3TLDynamicBitrateChange) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  change_bitrate_ = true;
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
#if CONFIG_VP9_DECODER
  // Number of temporal layers > 1, so half of the frames in this SVC pattern
  // will be non-reference frame and hence encoder will avoid loopfilter.
  // Since frame dropper is off, we can expect 200 (half of the sequence)
  // mismatched frames.
  EXPECT_EQ(static_cast<unsigned int>(200), GetMismatchFrames());
#endif
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and
// 2 temporal layers, with a change on the fly from the fixed SVC pattern to one
// generate via SVC_SET_REF_FRAME_CONFIG. The new pattern also disables
// inter-layer prediction.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SL2TLDynamicPatternChange) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 2;
  cfg_.ts_rate_decimator[0] = 2;
  cfg_.ts_rate_decimator[1] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 2;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  // Change SVC pattern on the fly.
  update_pattern_ = 1;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
#if CONFIG_VP9_DECODER
  // Number of temporal layers > 1, so half of the frames in this SVC pattern
  // will be non-reference frame and hence encoder will avoid loopfilter.
  // Since frame dropper is off, we can expect 200 (half of the sequence)
  // mismatched frames.
  EXPECT_EQ(static_cast<unsigned int>(200), GetMismatchFrames());
#endif
}

// Check basic rate targeting for 1 pass CBR SVC with 3 spatial layers and on
// the fly switching to 2 spatial layers and then back to 3. This switch is done
// by setting top spatial layer bitrate to 0, and then back to non-zero, during
// the sequence.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SL_to_2SL_dynamic) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 1;
  cfg_.ts_rate_decimator[0] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 0;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  dynamic_drop_layer_ = true;
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Don't check rate targeting on top spatial layer since it will be skipped
  // for part of the sequence.
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_ - 1,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and 3
// temporal layers. Run CIF clip with 1 thread, and few short key frame periods.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SL3TLSmallKf) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_target_bitrate = 800;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  top_sl_width_ = 640;
  top_sl_height_ = 480;
  // For this 3 temporal layer case, pattern repeats every 4 frames, so choose
  // 4 key neighboring key frame periods (so key frame will land on 0-2-1-2).
  for (int j = 32; j <= 35; j++) {
    cfg_.kf_max_dist = j;
    ResetModel();
    assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                          cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                          layer_target_avg_bandwidth_, bits_in_buffer_model_);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                            number_temporal_layers_, file_datarate_, 0.78,
                            1.15);
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Run HD clip with 4 threads.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SL3TL4threads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 4;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 9999;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  top_sl_width_ = 1280;
  top_sl_height_ = 720;
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode,
                        layer_target_avg_bandwidth_, bits_in_buffer_model_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
#if CONFIG_VP9_DECODER
  // Number of temporal layers > 1, so half of the frames in this SVC pattern
  // will be non-reference frame and hence encoder will avoid loopfilter.
  // Since frame dropper is off, we can expect 30 (half of the sequence)
  // mismatched frames.
  EXPECT_EQ(static_cast<unsigned int>(30), GetMismatchFrames());
#endif
}

// Run SVC encoder for 1 temporal layer, 2 spatial layers, with spatial
// downscale 5x5.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SL1TL5x5MultipleRuns) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 1;
  cfg_.ts_rate_decimator[0] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 3;
  cfg_.temporal_layering_mode = 0;
  svc_params_.scaling_factor_num[0] = 256;
  svc_params_.scaling_factor_den[0] = 1280;
  svc_params_.scaling_factor_num[1] = 1280;
  svc_params_.scaling_factor_den[1] = 1280;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.kf_max_dist = 999999;
  cfg_.kf_min_dist = 0;
  cfg_.ss_target_bitrate[0] = 300;
  cfg_.ss_target_bitrate[1] = 1400;
  cfg_.layer_target_bitrate[0] = 300;
  cfg_.layer_target_bitrate[1] = 1400;
  cfg_.rc_target_bitrate = 1700;
  number_spatial_layers_ = cfg_.ss_number_layers;
  number_temporal_layers_ = cfg_.ts_number_layers;
  ResetModel();
  layer_target_avg_bandwidth_[0] = cfg_.layer_target_bitrate[0] * 1000 / 30;
  bits_in_buffer_model_[0] =
      cfg_.layer_target_bitrate[0] * cfg_.rc_buf_initial_sz;
  layer_target_avg_bandwidth_[1] = cfg_.layer_target_bitrate[1] * 1000 / 30;
  bits_in_buffer_model_[1] =
      cfg_.layer_target_bitrate[1] * cfg_.rc_buf_initial_sz;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  top_sl_width_ = 1280;
  top_sl_height_ = 720;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  CheckLayerRateTargeting(&cfg_, number_spatial_layers_,
                          number_temporal_layers_, file_datarate_, 0.78, 1.15);
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
}

VP8_INSTANTIATE_TEST_CASE(DatarateTestLarge, ALL_TEST_MODES,
                          ::testing::Values(0));
VP8_INSTANTIATE_TEST_CASE(DatarateTestRealTime,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Values(-6, -12));
VP9_INSTANTIATE_TEST_CASE(DatarateTestVP9Large,
                          ::testing::Values(::libvpx_test::kOnePassGood,
                                            ::libvpx_test::kRealTime),
                          ::testing::Range(2, 9));
VP9_INSTANTIATE_TEST_CASE(DatarateTestVP9RealTime,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Range(5, 9));
#if CONFIG_VP9_TEMPORAL_DENOISING
VP9_INSTANTIATE_TEST_CASE(DatarateTestVP9LargeDenoiser,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Range(5, 9));
#endif
VP9_INSTANTIATE_TEST_CASE(DatarateOnePassCbrSvc,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Range(5, 9));
}  // namespace
