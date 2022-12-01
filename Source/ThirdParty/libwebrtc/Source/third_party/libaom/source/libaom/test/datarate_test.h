/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_config.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom/aom_codec.h"

namespace datarate_test {
namespace {
class DatarateTest : public ::libaom_test::EncoderTest {
 public:
  explicit DatarateTest(const ::libaom_test::CodecFactory *codec)
      : EncoderTest(codec), set_cpu_used_(0), aq_mode_(0),
        speed_change_test_(false) {}

 protected:
  virtual ~DatarateTest() {}

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    tot_frame_number_ = 0;
    first_drop_ = 0;
    num_drops_ = 0;
    // Denoiser is off by default.
    denoiser_on_ = 0;
    bits_total_ = 0;
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
    tile_column_ = 0;
    screen_mode_ = false;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_TILE_COLUMNS, tile_column_);
      encoder->Control(AV1E_SET_ROW_MT, 1);
      if (cfg_.g_usage == AOM_USAGE_REALTIME) {
        encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
        encoder->Control(AV1E_SET_ENABLE_WARPED_MOTION, 0);
        encoder->Control(AV1E_SET_ENABLE_RESTORATION, 0);
        encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
        encoder->Control(AV1E_SET_DELTAQ_MODE, 0);
        encoder->Control(AV1E_SET_ENABLE_TPL_MODEL, 0);
        encoder->Control(AV1E_SET_ENABLE_CDEF, 1);
        encoder->Control(AV1E_SET_COEFF_COST_UPD_FREQ, 2);
        encoder->Control(AV1E_SET_MODE_COST_UPD_FREQ, 2);
        encoder->Control(AV1E_SET_MV_COST_UPD_FREQ, 2);
        encoder->Control(AV1E_SET_DV_COST_UPD_FREQ, 2);
      }
      if (screen_mode_) {
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
        encoder->Control(AV1E_SET_ENABLE_PALETTE, 1);
        encoder->Control(AV1E_SET_ENABLE_INTRABC, 0);
      }
    }

    if (speed_change_test_) {
      if (video->frame() == 0) {
        encoder->Control(AOME_SET_CPUUSED, 8);
      } else if (video->frame() == 30) {
        encoder->Control(AOME_SET_CPUUSED, 7);
      } else if (video->frame() == 60) {
        encoder->Control(AOME_SET_CPUUSED, 6);
      } else if (video->frame() == 90) {
        encoder->Control(AOME_SET_CPUUSED, 7);
      }
    }

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
    }

    encoder->Control(AV1E_SET_NOISE_SENSITIVITY, denoiser_on_);

    const aom_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    aom_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    if (duration > 1) {
      // If first drop not set and we have a drop set it to this time.
      if (!first_drop_) first_drop_ = last_pts_ + 1;
      // Update the number of frame drops.
      num_drops_ += static_cast<int>(duration - 1);
      // Update counter for total number of frames (#frames input to encoder).
      // Needed for setting the proper layer_id below.
      tot_frame_number_ += static_cast<int>(duration - 1);
    }

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    // Buffer should not go negative.
    ASSERT_GE(bits_in_buffer_model_, 0)
        << "Buffer Underrun at frame " << pkt->data.frame.pts;

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Update the total encoded bits.
    bits_total_ += frame_size_in_bits;

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++frame_number_;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) {
    duration_ = (last_pts_ + 1) * timebase_;
    // Effective file datarate:
    effective_datarate_ = (bits_total_ / 1000.0) / duration_;
  }

  aom_codec_pts_t last_pts_;
  double timebase_;
  int frame_number_;      // Counter for number of non-dropped/encoded frames.
  int tot_frame_number_;  // Counter for total number of input frames.
  int64_t bits_total_;
  double duration_;
  double effective_datarate_;
  int set_cpu_used_;
  int64_t bits_in_buffer_model_;
  aom_codec_pts_t first_drop_;
  int num_drops_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
  unsigned int aq_mode_;
  bool speed_change_test_;
  int tile_column_;
  bool screen_mode_;
};

}  // namespace
}  // namespace datarate_test
