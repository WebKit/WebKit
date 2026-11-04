/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>
#include "gtest/gtest.h"
#include "test/acm_random.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

// Params: test mode, speed, aq_mode and screen_content mode.
class ActiveMapTest
    : public ::libaom_test::CodecTestWith4Params<libaom_test::TestMode, int,
                                                 int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  static const int kWidth = 208;
  static const int kHeight = 144;

  ActiveMapTest() : EncoderTest(GET_PARAM(0)) {}
  ~ActiveMapTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
    aq_mode_ = GET_PARAM(3);
    screen_mode_ = GET_PARAM(4);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_ALLOW_WARPED_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_TUNE_CONTENT, screen_mode_);
      if (screen_mode_) encoder->Control(AV1E_SET_ENABLE_PALETTE, 1);
    } else if (video->frame() == 3) {
      aom_active_map_t map = aom_active_map_t();
      /* clang-format off */
      uint8_t active_map[9 * 13] = {
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
        0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0,
      };
      /* clang-format on */
      map.cols = (kWidth + 15) / 16;
      map.rows = (kHeight + 15) / 16;
      ASSERT_EQ(map.cols, 13u);
      ASSERT_EQ(map.rows, 9u);
      map.active_map = active_map;
      encoder->Control(AOME_SET_ACTIVEMAP, &map);
    } else if (video->frame() == 15) {
      aom_active_map_t map = aom_active_map_t();
      map.cols = (kWidth + 15) / 16;
      map.rows = (kHeight + 15) / 16;
      map.active_map = nullptr;
      encoder->Control(AOME_SET_ACTIVEMAP, &map);
    }
  }

  void DoTest() {
    // Validate that this non multiple of 64 wide clip encodes
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_resize_mode = 0;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_max_dist = 90000;
    ::libaom_test::I420VideoSource video("hantro_odd.yuv", kWidth, kHeight, 100,
                                         1, 0, 100);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int cpu_used_;
  int aq_mode_;
  int screen_mode_;
};

TEST_P(ActiveMapTest, Test) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(ActiveMapTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(5, 12), ::testing::Values(0, 3),
                           ::testing::Values(0, 1));

// Encodes with every block inactive, with and without temporal layers. All
// frames after the key frame should then be coded as skip and stay tiny.
// Params: test mode, speed and number of temporal layers.
class ActiveMapSvcTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 int>,
      public ::libaom_test::EncoderTest {
 protected:
  static const int kWidth = 208;
  static const int kHeight = 144;
  static const int kCols = (kWidth + 15) / 16;
  static const int kRows = (kHeight + 15) / 16;
  static constexpr size_t kMaxSkipFrameSize = 200;

  ActiveMapSvcTest() : EncoderTest(GET_PARAM(0)) {}
  ~ActiveMapSvcTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
    number_temporal_layers_ = GET_PARAM(3);
  }

  void SetSvcParams(::libaom_test::Encoder *encoder) {
    aom_svc_params_t svc_params = {};
    svc_params.number_spatial_layers = 1;
    svc_params.number_temporal_layers = number_temporal_layers_;
    svc_params.scaling_factor_num[0] = 1;
    svc_params.scaling_factor_den[0] = 1;
    for (int i = 0; i < number_temporal_layers_; ++i) {
      svc_params.max_quantizers[i] = 56;
      svc_params.min_quantizers[i] = 2;
      svc_params.layer_target_bitrate[i] =
          cfg_.rc_target_bitrate * (i + 1) / number_temporal_layers_;
      svc_params.framerate_factor[i] = 1 << (number_temporal_layers_ - 1 - i);
    }
    encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    const unsigned int frame = video->frame();
    temporal_layer_id_ = frame % number_temporal_layers_;
    if (frame == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, 0);
      if (number_temporal_layers_ > 1) SetSvcParams(encoder);
    }
    if (number_temporal_layers_ > 1) {
      aom_svc_layer_id_t layer_id = {};
      layer_id.temporal_layer_id = temporal_layer_id_;
      encoder->Control(AV1E_SET_SVC_LAYER_ID, &layer_id);
    }
    if (frame > 0) {
      uint8_t active_map[kRows * kCols] = {};
      aom_active_map_t map = aom_active_map_t();
      map.rows = kRows;
      map.cols = kCols;
      map.active_map = active_map;
      encoder->Control(AOME_SET_ACTIVEMAP, &map);
    }
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    if (num_frames_++ == 0) return;
    EXPECT_LT(pkt->data.frame.sz, kMaxSkipFrameSize)
        << "frame " << num_frames_ - 1;
  }

  // The top temporal layer isn't used as a reference, so the encoder skips
  // loop filtering on it while the decoder doesn't.
  void MismatchHook(const aom_image_t *img_enc,
                    const aom_image_t *img_dec) override {
    if (number_temporal_layers_ > 1 &&
        temporal_layer_id_ == number_temporal_layers_ - 1) {
      return;
    }
    EncoderTest::MismatchHook(img_enc, img_dec);
  }

  void DoTest() {
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_resize_mode = 0;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_max_dist = 90000;
    ::libaom_test::I420VideoSource video("hantro_odd.yuv", kWidth, kHeight, 30,
                                         1, 0, 20);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    EXPECT_EQ(num_frames_, 20);
  }

  int cpu_used_;
  int number_temporal_layers_;
  int temporal_layer_id_ = 0;
  int num_frames_ = 0;
};

TEST_P(ActiveMapSvcTest, AllInactive) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(ActiveMapSvcTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(7, 12), ::testing::Values(1, 2));

class MovingGradientSource : public ::libaom_test::DummyVideoSource {
 protected:
  void FillFrame() override {
    memset(img_->img_data, 128, raw_sz_);
    for (unsigned int y = 0; y < img_->d_h; ++y) {
      for (unsigned int x = 0; x < img_->d_w; ++x) {
        img_->planes[AOM_PLANE_Y][y * img_->stride[AOM_PLANE_Y] + x] =
            static_cast<uint8_t>(x * 3 + y + frame_ * 7);
      }
    }
  }
};

// Params: test mode, speed and aq_mode.
class GetActiveMapTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 int>,
      public ::libaom_test::EncoderTest {
 protected:
  // The last 16x16 column and row only cover 8 pixels.
  static const int kWidth = 200;
  static const int kHeight = 136;
  static const int kCols = 13;
  static const int kRows = 9;

  GetActiveMapTest() : EncoderTest(GET_PARAM(0)) {}
  ~GetActiveMapTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
    aq_mode_ = GET_PARAM(3);
  }

  // Mirrors the pattern on odd frames so a stale map does not match.
  void BuildMap(int frame, uint8_t *map) const {
    /* clang-format off */
    static const uint8_t kMap[kRows * kCols] = {
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    };
    /* clang-format on */
    for (int r = 0; r < kRows; ++r) {
      for (int c = 0; c < kCols; ++c) {
        const int src_c = (frame & 1) ? kCols - 1 - c : c;
        map[r * kCols + c] = kMap[r * kCols + src_c];
      }
    }
  }

  void ExpectMap(::libaom_test::Encoder *encoder, const uint8_t *expected,
                 int frame) {
    uint8_t map_out[kRows * kCols];
    memset(map_out, 2, sizeof(map_out));
    aom_active_map_t map = aom_active_map_t();
    map.rows = kRows;
    map.cols = kCols;
    map.active_map = map_out;
    encoder->Control(AV1E_GET_ACTIVEMAP, &map);
    for (int i = 0; i < kRows * kCols; ++i) {
      ASSERT_EQ(map_out[i], expected[i])
          << "frame " << frame << " row " << i / kCols << " col " << i % kCols;
    }
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    const int frame = static_cast<int>(video->frame());
    if (frame == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
    }
    uint8_t map_in[kRows * kCols];
    // The key frame turns the map off, so check from the second inter frame.
    if (frame >= 2) {
      BuildMap(frame - 1, map_in);
      ASSERT_NO_FATAL_FAILURE(ExpectMap(encoder, map_in, frame));
    }
    BuildMap(frame, map_in);
    aom_active_map_t map = aom_active_map_t();
    map.rows = kRows;
    map.cols = kCols;
    map.active_map = map_in;
    encoder->Control(AOME_SET_ACTIVEMAP, &map);
    ASSERT_NO_FATAL_FAILURE(ExpectMap(encoder, map_in, frame));
  }

  int cpu_used_;
  int aq_mode_;
};

TEST_P(GetActiveMapTest, GetMatchesSet) {
  cfg_.g_lag_in_frames = 0;
  cfg_.rc_target_bitrate = 400;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_pass = AOM_RC_ONE_PASS;
  cfg_.kf_max_dist = 90000;
  MovingGradientSource video;
  video.SetSize(kWidth, kHeight);
  video.set_limit(10);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(GetActiveMapTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(7, 12), ::testing::Values(0, 3));

constexpr int kSrcWidth = 384;
constexpr int kSrcHeight = 256;
constexpr int kMapCols = kSrcWidth / 16;
constexpr int kMapRows = kSrcHeight / 16;
constexpr int kWinSize = 128;
constexpr int kDocHeight = 1024;
constexpr int kNumFrames = 60;
constexpr int kEdgeMargin = 8;
constexpr int kResizeFrame = 20;

void WindowPos(int frame, int *x, int *y) {
  static const int kPos[4][2] = {
    { 64, 64 }, { 192, 0 }, { 0, 128 }, { 256, 128 }
  };
  const int i = (frame / 10) % 4;
  *x = kPos[i][0];
  *y = kPos[i][1];
}

int ScrollPos(int frame) {
  static const int kSteps[4] = { 1, 2, 4, 8 };
  int pos = 0;
  for (int f = 1; f <= frame; ++f) pos += kSteps[(f / 7) % 4];
  return pos % (kDocHeight - kWinSize);
}

void BuildActiveMap(int frame, uint8_t *map) {
  memset(map, 0, kMapRows * kMapCols);
  for (int f = frame - 1; f <= frame; ++f) {
    int win_x;
    int win_y;
    WindowPos(f, &win_x, &win_y);
    for (int r = win_y / 16; r < (win_y + kWinSize) / 16; ++r) {
      for (int c = win_x / 16; c < (win_x + kWinSize) / 16; ++c) {
        map[r * kMapCols + c] = 1;
      }
    }
  }
}

// Marks the top half of the window active, 32 of the 384 blocks. With this
// share cyclic refresh starts on and turns off a few frames later.
void BuildSmallActiveMap(uint8_t *map) {
  memset(map, 0, kMapRows * kMapCols);
  int win_x;
  int win_y;
  WindowPos(1, &win_x, &win_y);
  for (int r = win_y / 16; r < win_y / 16 + 4; ++r) {
    for (int c = win_x / 16; c < (win_x + kWinSize) / 16; ++c) {
      map[r * kMapCols + c] = 1;
    }
  }
}

// With fixed_window the window stays where it is on frame 1 and scrolls one
// line per frame.
class ScrollingWindowSource : public ::libaom_test::DummyVideoSource {
 public:
  explicit ScrollingWindowSource(bool fixed_window)
      : fixed_window_(fixed_window), doc_(kWinSize * kDocHeight) {
    SetSize(kSrcWidth, kSrcHeight);
    set_limit(kNumFrames);
    ::libaom_test::ACMRandom rnd(0x5eed);
    for (int y = 0; y < kDocHeight; ++y) {
      for (int x = 0; x < kWinSize; ++x) {
        const bool glyph_row = (y % 16) < 12;
        doc_[y * kWinSize + x] = (glyph_row && rnd.Rand8() < 77) ? 20 : 235;
      }
    }
  }

 protected:
  void FillFrame() override {
    const int frame = static_cast<int>(frame_);
    const int layout_frame = fixed_window_ ? 1 : frame;
    uint8_t *const y_plane = img_->planes[AOM_PLANE_Y];
    const int y_stride = img_->stride[AOM_PLANE_Y];
    for (int y = 0; y < kSrcHeight; ++y) {
      for (int x = 0; x < kSrcWidth; ++x) {
        y_plane[y * y_stride + x] =
            static_cast<uint8_t>(40 + x * 150 / kSrcWidth + (y * 37) % 23);
      }
    }
    for (int plane = AOM_PLANE_U; plane <= AOM_PLANE_V; ++plane) {
      for (int y = 0; y < kSrcHeight / 2; ++y) {
        memset(img_->planes[plane] + y * img_->stride[plane],
               plane == AOM_PLANE_U ? 110 : 140, kSrcWidth / 2);
      }
    }

    int win_x;
    int win_y;
    WindowPos(layout_frame, &win_x, &win_y);
    const int scroll =
        fixed_window_ ? frame % (kDocHeight - kWinSize) : ScrollPos(frame);
    for (int y = 0; y < kWinSize; ++y) {
      memcpy(y_plane + (win_y + y) * y_stride + win_x,
             &doc_[(scroll + y) * kWinSize], kWinSize);
    }

    if (frame == 0) return;

    uint8_t map[kMapRows * kMapCols];
    BuildActiveMap(layout_frame, map);
    ::libaom_test::ACMRandom rnd(frame);
    for (int y = 0; y < kSrcHeight; ++y) {
      for (int x = 0; x < kSrcWidth; ++x) {
        if (map[(y / 16) * kMapCols + x / 16]) continue;
        if (rnd.Rand8() < 16) y_plane[y * y_stride + x] ^= 0x10;
      }
    }
  }

 private:
  bool fixed_window_;
  std::vector<uint8_t> doc_;
};

int GetPixel(const aom_image_t &img, int plane, int x, int y) {
  const uint8_t *row = img.planes[plane] + y * img.stride[plane];
  if (img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
    return reinterpret_cast<const uint16_t *>(row)[x];
  }
  return row[x];
}

// Params: test mode, speed, aq_mode, number of threads and content type.
class InactiveAreaTest
    : public ::libaom_test::CodecTestWith5Params<libaom_test::TestMode, int,
                                                 int, int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  InactiveAreaTest() : EncoderTest(GET_PARAM(0)) {}
  ~InactiveAreaTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
    aq_mode_ = GET_PARAM(3);
    cfg_.g_threads = GET_PARAM(4);
    content_ = GET_PARAM(5);
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 300;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.kf_max_dist = 90000;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    const int frame = static_cast<int>(video->frame());
    if (frame == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_TUNE_CONTENT, content_);
      encoder->Control(AV1E_SET_ROW_MT, cfg_.g_threads > 1);
      return;
    }
    if (resize_ && frame == kResizeFrame) {
      aom_scaling_mode_t mode = { AOME_ONETWO, AOME_ONETWO };
      encoder->Control(AOME_SET_SCALEMODE, &mode);
    }
    if (map_once_ && frame > 1) return;

    uint8_t map[kMapRows * kMapCols];
    BuildMap(frame, map);
    if (use_roi_) {
      const int mi_cols = kSrcWidth / 4;
      const int mi_rows = kSrcHeight / 4;
      std::vector<uint8_t> roi_map(mi_rows * mi_cols);
      for (int r = 0; r < mi_rows; ++r) {
        for (int c = 0; c < mi_cols; ++c) {
          roi_map[r * mi_cols + c] = map[(r / 4) * kMapCols + c / 4] ? 0 : 3;
        }
      }
      aom_roi_map_t roi = {};
      roi.enabled = 1;
      roi.roi_map = roi_map.data();
      roi.rows = mi_rows;
      roi.cols = mi_cols;
      for (int i = 0; i < AOM_MAX_SEGMENTS; ++i) roi.ref_frame[i] = -1;
      roi.skip[3] = 1;
      if (skip_ref_golden_) roi.ref_frame[3] = 4;
      encoder->Control(AOME_SET_ROI_MAP, &roi);
    } else {
      aom_active_map_t active_map = {};
      active_map.rows = kMapRows;
      active_map.cols = kMapCols;
      active_map.active_map = map;
      encoder->Control(AOME_SET_ACTIVEMAP, &active_map);
    }
  }

  void DecompressedFrameHook(const aom_image_t &img,
                             aom_codec_pts_t pts) override {
    // Only check that the encoder and decoder match.
    if (resize_) return;
    const int frame = static_cast<int>(pts);
    if (frame > 0 && !skip_ref_golden_) {
      uint8_t map[kMapRows * kMapCols];
      BuildMap(frame, map);
      std::vector<uint8_t> near_active(kSrcWidth * kSrcHeight, 0);
      for (int r = 0; r < kMapRows; ++r) {
        for (int c = 0; c < kMapCols; ++c) {
          if (!map[r * kMapCols + c]) continue;
          const int y0 = std::max(r * 16 - kEdgeMargin, 0);
          const int y1 = std::min((r + 1) * 16 + kEdgeMargin, kSrcHeight);
          const int x0 = std::max(c * 16 - kEdgeMargin, 0);
          const int x1 = std::min((c + 1) * 16 + kEdgeMargin, kSrcWidth);
          for (int y = y0; y < y1; ++y) {
            memset(&near_active[y * kSrcWidth + x0], 1, x1 - x0);
          }
        }
      }
      int changed = 0;
      for (int plane = 0; plane < 3; ++plane) {
        const int shift = plane ? 1 : 0;
        const int width = kSrcWidth >> shift;
        const int height = kSrcHeight >> shift;
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            if (near_active[(y << shift) * kSrcWidth + (x << shift)]) continue;
            changed +=
                GetPixel(img, plane, x, y) != prev_[plane][y * width + x];
          }
        }
      }
      EXPECT_EQ(changed, 0) << "Inactive pixels changed in frame " << frame;
    }

    for (int plane = 0; plane < 3; ++plane) {
      const int shift = plane ? 1 : 0;
      const int width = kSrcWidth >> shift;
      const int height = kSrcHeight >> shift;
      prev_[plane].resize(width * height);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          prev_[plane][y * width + x] =
              static_cast<uint8_t>(GetPixel(img, plane, x, y));
        }
      }
    }
  }

  void BuildMap(int frame, uint8_t *map) const {
    if (map_once_) {
      BuildSmallActiveMap(map);
    } else {
      BuildActiveMap(frame, map);
    }
  }

  void DoTest(bool use_roi, bool skip_ref_golden = false, bool map_once = false,
              bool resize = false) {
    use_roi_ = use_roi;
    skip_ref_golden_ = skip_ref_golden;
    map_once_ = map_once;
    resize_ = resize;
    // More bits let q drop low enough while the map stays set.
    if (map_once) cfg_.rc_target_bitrate = 1000;
    ScrollingWindowSource video(map_once);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int cpu_used_;
  int aq_mode_;
  int content_;
  bool use_roi_ = false;
  bool skip_ref_golden_ = false;
  bool map_once_ = false;
  bool resize_ = false;
  std::vector<uint8_t> prev_[3];
};

TEST_P(InactiveAreaTest, ActiveMap) { DoTest(false); }

TEST_P(InactiveAreaTest, RoiSkip) { DoTest(true); }

TEST_P(InactiveAreaTest, RoiSkipGoldenRef) { DoTest(true, true); }

// The map stays set while cyclic refresh turns off and q drops.
TEST_P(InactiveAreaTest, ActiveMapSetOnce) { DoTest(false, false, true); }

// One frame has another size, so the frames around it can't use the segment
// map of the frame before them.
TEST_P(InactiveAreaTest, ActiveMapSetOnceResize) {
  DoTest(false, false, true, true);
}

AV1_INSTANTIATE_TEST_SUITE(
    InactiveAreaTest, ::testing::Values(::libaom_test::kRealTime),
    ::testing::Range(7, 12), ::testing::Values(0, 3), ::testing::Values(1, 4),
    ::testing::Values(AOM_CONTENT_DEFAULT, AOM_CONTENT_SCREEN));

}  // namespace
