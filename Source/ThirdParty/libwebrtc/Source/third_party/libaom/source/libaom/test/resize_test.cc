/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <climits>
#include <vector>
#include "aom_dsp/aom_dsp_common.h"
#include "common/tools_common.h"
#include "av1/encoder/encoder.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

// Enable(1) or Disable(0) writing of the compressed bitstream.
#define WRITE_COMPRESSED_STREAM 0

namespace {

#if WRITE_COMPRESSED_STREAM
static void mem_put_le16(char *const mem, unsigned int val) {
  mem[0] = val;
  mem[1] = val >> 8;
}

static void mem_put_le32(char *const mem, unsigned int val) {
  mem[0] = val;
  mem[1] = val >> 8;
  mem[2] = val >> 16;
  mem[3] = val >> 24;
}

static void write_ivf_file_header(const aom_codec_enc_cfg_t *const cfg,
                                  int frame_cnt, FILE *const outfile) {
  char header[32];

  header[0] = 'D';
  header[1] = 'K';
  header[2] = 'I';
  header[3] = 'F';
  mem_put_le16(header + 4, 0);                    /* version */
  mem_put_le16(header + 6, 32);                   /* headersize */
  mem_put_le32(header + 8, AV1_FOURCC);           /* fourcc (av1) */
  mem_put_le16(header + 12, cfg->g_w);            /* width */
  mem_put_le16(header + 14, cfg->g_h);            /* height */
  mem_put_le32(header + 16, cfg->g_timebase.den); /* rate */
  mem_put_le32(header + 20, cfg->g_timebase.num); /* scale */
  mem_put_le32(header + 24, frame_cnt);           /* length */
  mem_put_le32(header + 28, 0);                   /* unused */

  (void)fwrite(header, 1, 32, outfile);
}

static void write_ivf_frame_size(FILE *const outfile, const size_t size) {
  char header[4];
  mem_put_le32(header, static_cast<unsigned int>(size));
  (void)fwrite(header, 1, 4, outfile);
}

static void write_ivf_frame_header(const aom_codec_cx_pkt_t *const pkt,
                                   FILE *const outfile) {
  char header[12];
  aom_codec_pts_t pts;

  if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) return;

  pts = pkt->data.frame.pts;
  mem_put_le32(header, static_cast<unsigned int>(pkt->data.frame.sz));
  mem_put_le32(header + 4, pts & 0xFFFFFFFF);
  mem_put_le32(header + 8, pts >> 32);

  (void)fwrite(header, 1, 12, outfile);
}
#endif  // WRITE_COMPRESSED_STREAM

const unsigned int kInitialWidth = 320;
const unsigned int kInitialHeight = 240;

struct FrameInfo {
  FrameInfo(aom_codec_pts_t _pts, unsigned int _w, unsigned int _h)
      : pts(_pts), w(_w), h(_h) {}

  aom_codec_pts_t pts;
  unsigned int w;
  unsigned int h;
};

void ScaleForFrameNumber(unsigned int frame, unsigned int initial_w,
                         unsigned int initial_h, unsigned int *w,
                         unsigned int *h, int flag_codec) {
  if (frame < 10) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  if (frame < 20) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 30) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 40) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  if (frame < 50) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 60) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 70) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  if (frame < 80) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 90) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 100) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 110) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  // Go down very low
  if (frame < 120) {
    *w = initial_w / 4;
    *h = initial_h / 4;
    return;
  }
  if (flag_codec == 1) {
    // Cases that only works for AV1.
    // For AV1: Swap width and height of original.
    if (frame < 140) {
      *w = initial_h;
      *h = initial_w;
      return;
    }
  }
  *w = initial_w;
  *h = initial_h;
}

class ResizingVideoSource : public ::libaom_test::DummyVideoSource {
 public:
  ResizingVideoSource() {
    SetSize(kInitialWidth, kInitialHeight);
    limit_ = 150;
  }
  int flag_codec_;
  virtual ~ResizingVideoSource() {}

 protected:
  virtual void Next() {
    ++frame_;
    unsigned int width;
    unsigned int height;
    ScaleForFrameNumber(frame_, kInitialWidth, kInitialHeight, &width, &height,
                        flag_codec_);
    SetSize(width, height);
    FillFrame();
  }
};

class ResizeTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  ResizeTest() : EncoderTest(GET_PARAM(0)) {}

  virtual ~ResizeTest() {}

  virtual void SetUp() { InitializeConfig(GET_PARAM(1)); }

  virtual void PreEncodeFrameHook(libaom_test::VideoSource *video,
                                  libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      if (GET_PARAM(1) == ::libaom_test::kRealTime) {
        encoder->Control(AV1E_SET_AQ_MODE, 3);
        encoder->Control(AOME_SET_CPUUSED, 5);
        encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      }
    }
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t pts) {
    frame_info_list_.push_back(FrameInfo(pts, img.d_w, img.d_h));
  }

  std::vector<FrameInfo> frame_info_list_;
};

TEST_P(ResizeTest, TestExternalResizeWorks) {
  ResizingVideoSource video;
  video.flag_codec_ = 0;
  cfg_.g_lag_in_frames = 0;
  // We use max(kInitialWidth, kInitialHeight) because during the test
  // the width and height of the frame are swapped
  cfg_.g_forced_max_frame_width = cfg_.g_forced_max_frame_height =
      AOMMAX(kInitialWidth, kInitialHeight);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // Check we decoded the same number of frames as we attempted to encode
  ASSERT_EQ(frame_info_list_.size(), video.limit());

  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const unsigned int frame = static_cast<unsigned>(info->pts);
    unsigned int expected_w;
    unsigned int expected_h;
    ScaleForFrameNumber(frame, kInitialWidth, kInitialHeight, &expected_w,
                        &expected_h, 0);
    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
  }
}

#if !CONFIG_REALTIME_ONLY
const unsigned int kStepDownFrame = 3;
const unsigned int kStepUpFrame = 6;

class ResizeInternalTestLarge : public ResizeTest {
 protected:
#if WRITE_COMPRESSED_STREAM
  ResizeInternalTestLarge()
      : ResizeTest(), frame0_psnr_(0.0), outfile_(nullptr), out_frames_(0) {}
#else
  ResizeInternalTestLarge() : ResizeTest(), frame0_psnr_(0.0) {}
#endif

  virtual ~ResizeInternalTestLarge() {}

  virtual void BeginPassHook(unsigned int /*pass*/) {
#if WRITE_COMPRESSED_STREAM
    outfile_ = fopen("av10-2-05-resize.ivf", "wb");
#endif
  }

  virtual void EndPassHook() {
#if WRITE_COMPRESSED_STREAM
    if (outfile_) {
      if (!fseek(outfile_, 0, SEEK_SET))
        write_ivf_file_header(&cfg_, out_frames_, outfile_);
      fclose(outfile_);
      outfile_ = nullptr;
    }
#endif
  }

  virtual void PreEncodeFrameHook(libaom_test::VideoSource *video,
                                  libaom_test::Encoder *encoder) {
    if (change_config_) {
      int new_q = 60;
      if (video->frame() == 0) {
        struct aom_scaling_mode mode = { AOME_ONETWO, AOME_ONETWO };
        encoder->Control(AOME_SET_SCALEMODE, &mode);
      } else if (video->frame() == 1) {
        struct aom_scaling_mode mode = { AOME_NORMAL, AOME_NORMAL };
        encoder->Control(AOME_SET_SCALEMODE, &mode);
        cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = new_q;
        encoder->Config(&cfg_);
      }
    } else {
      if (video->frame() >= kStepDownFrame && video->frame() < kStepUpFrame) {
        struct aom_scaling_mode mode = { AOME_FOURFIVE, AOME_THREEFIVE };
        encoder->Control(AOME_SET_SCALEMODE, &mode);
      }
      if (video->frame() >= kStepUpFrame) {
        struct aom_scaling_mode mode = { AOME_NORMAL, AOME_NORMAL };
        encoder->Control(AOME_SET_SCALEMODE, &mode);
      }
    }
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    if (frame0_psnr_ == 0.) frame0_psnr_ = pkt->data.psnr.psnr[0];
    EXPECT_NEAR(pkt->data.psnr.psnr[0], frame0_psnr_, 4.1);
  }

#if WRITE_COMPRESSED_STREAM
  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    ++out_frames_;

    // Write initial file header if first frame.
    if (pkt->data.frame.pts == 0) write_ivf_file_header(&cfg_, 0, outfile_);

    // Write frame header and data.
    write_ivf_frame_header(pkt, outfile_);
    (void)fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz, outfile_);
  }
#endif

  double frame0_psnr_;
  bool change_config_;
#if WRITE_COMPRESSED_STREAM
  FILE *outfile_;
  unsigned int out_frames_;
#endif
};

TEST_P(ResizeInternalTestLarge, TestInternalResizeWorks) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 10);
  init_flags_ = AOM_CODEC_USE_PSNR;
  change_config_ = false;

  // q picked such that initial keyframe on this clip is ~30dB PSNR
  cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = 48;

  // If the number of frames being encoded is smaller than g_lag_in_frames
  // the encoded frame is unavailable using the current API. Comparing
  // frames to detect mismatch would then not be possible. Set
  // g_lag_in_frames = 0 to get around this.
  cfg_.g_lag_in_frames = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
  }
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const aom_codec_pts_t pts = info->pts;
    if (pts >= kStepDownFrame && pts < kStepUpFrame) {
      ASSERT_EQ(282U, info->w) << "Frame " << pts << " had unexpected width";
      ASSERT_EQ(173U, info->h) << "Frame " << pts << " had unexpected height";
    } else {
      EXPECT_EQ(352U, info->w) << "Frame " << pts << " had unexpected width";
      EXPECT_EQ(288U, info->h) << "Frame " << pts << " had unexpected height";
    }
  }
}

TEST_P(ResizeInternalTestLarge, TestInternalResizeChangeConfig) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 10);
  cfg_.g_w = 352;
  cfg_.g_h = 288;
  change_config_ = true;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(ResizeInternalTestLarge,
                           ::testing::Values(::libaom_test::kOnePassGood));
#endif

class ResizeRealtimeTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  ResizeRealtimeTest()
      : EncoderTest(GET_PARAM(0)), set_scale_mode_(false),
        set_scale_mode2_(false) {}
  virtual ~ResizeRealtimeTest() {}

  virtual void PreEncodeFrameHook(libaom_test::VideoSource *video,
                                  libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_AQ_MODE, 3);
      encoder->Control(AV1E_SET_ALLOW_WARPED_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
    }
    if (set_scale_mode_) {
      struct aom_scaling_mode mode;
      if (video->frame() <= 20)
        mode = { AOME_ONETWO, AOME_ONETWO };
      else if (video->frame() <= 40)
        mode = { AOME_ONEFOUR, AOME_ONEFOUR };
      else if (video->frame() > 40)
        mode = { AOME_NORMAL, AOME_NORMAL };
      encoder->Control(AOME_SET_SCALEMODE, &mode);
    } else if (set_scale_mode2_) {
      struct aom_scaling_mode mode;
      if (video->frame() <= 20)
        mode = { AOME_ONEFOUR, AOME_ONEFOUR };
      else if (video->frame() <= 40)
        mode = { AOME_ONETWO, AOME_ONETWO };
      else if (video->frame() > 40)
        mode = { AOME_THREEFOUR, AOME_THREEFOUR };
      encoder->Control(AOME_SET_SCALEMODE, &mode);
    }

    if (change_bitrate_ && video->frame() == frame_change_bitrate_) {
      change_bitrate_ = false;
      cfg_.rc_target_bitrate = 500;
      encoder->Config(&cfg_);
    }
  }

  virtual void SetUp() {
    InitializeConfig(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t pts) {
    frame_info_list_.push_back(FrameInfo(pts, img.d_w, img.d_h));
  }

  virtual void MismatchHook(const aom_image_t *img1, const aom_image_t *img2) {
    double mismatch_psnr = compute_psnr(img1, img2);
    mismatch_psnr_ += mismatch_psnr;
    ++mismatch_nframes_;
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }

  void DefaultConfig() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_mode = AOM_KF_AUTO;
    cfg_.g_lag_in_frames = 0;
    cfg_.kf_min_dist = cfg_.kf_max_dist = 3000;
    // Enable dropped frames.
    cfg_.rc_dropframe_thresh = 1;
    // Disable error_resilience mode.
    cfg_.g_error_resilient = 0;
    // Run at low bitrate.
    cfg_.rc_target_bitrate = 200;
    // We use max(kInitialWidth, kInitialHeight) because during the test
    // the width and height of the frame are swapped
    cfg_.g_forced_max_frame_width = cfg_.g_forced_max_frame_height =
        AOMMAX(kInitialWidth, kInitialHeight);
    if (set_scale_mode_ || set_scale_mode2_) {
      cfg_.rc_dropframe_thresh = 0;
      cfg_.g_forced_max_frame_width = 1280;
      cfg_.g_forced_max_frame_height = 1280;
    }
  }

  std::vector<FrameInfo> frame_info_list_;
  int set_cpu_used_;
  bool change_bitrate_;
  unsigned int frame_change_bitrate_;
  double mismatch_psnr_;
  int mismatch_nframes_;
  bool set_scale_mode_;
  bool set_scale_mode2_;
};

// Check the AOME_SET_SCALEMODE control by downsizing to
// 1/2, then 1/4, and then back up to originsal.
TEST_P(ResizeRealtimeTest, TestInternalResizeSetScaleMode1) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.g_w = 1280;
  cfg_.g_h = 720;
  set_scale_mode_ = true;
  set_scale_mode2_ = false;
  DefaultConfig();
  change_bitrate_ = false;
  mismatch_nframes_ = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Check we decoded the same number of frames as we attempted to encode
  ASSERT_EQ(frame_info_list_.size(), video.limit());
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const auto frame = static_cast<unsigned>(info->pts);
    unsigned int expected_w = 1280 >> 1;
    unsigned int expected_h = 720 >> 1;
    if (frame > 40) {
      expected_w = 1280;
      expected_h = 720;
    } else if (frame > 20 && frame <= 40) {
      expected_w = 1280 >> 2;
      expected_h = 720 >> 2;
    }
    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

// Check the AOME_SET_SCALEMODE control by downsizing to
// 1/2, then 1/4, and then back up to originsal.
TEST_P(ResizeRealtimeTest, TestInternalResizeSetScaleMode1QVGA) {
  ::libaom_test::I420VideoSource video("desktop1.320_180.yuv", 320, 180, 30, 1,
                                       0, 80);
  cfg_.g_w = 320;
  cfg_.g_h = 180;
  set_scale_mode_ = true;
  set_scale_mode2_ = false;
  DefaultConfig();
  change_bitrate_ = false;
  mismatch_nframes_ = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Check we decoded the same number of frames as we attempted to encode
  ASSERT_EQ(frame_info_list_.size(), video.limit());
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const auto frame = static_cast<unsigned>(info->pts);
    unsigned int expected_w = 320 >> 1;
    unsigned int expected_h = 180 >> 1;
    if (frame > 40) {
      expected_w = 320;
      expected_h = 180;
    } else if (frame > 20 && frame <= 40) {
      expected_w = 320 >> 2;
      expected_h = 180 >> 2;
    }
    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

// Check the AOME_SET_SCALEMODE control by downsizing to
// 1/4, then 1/2, and then up to 3/4.
TEST_P(ResizeRealtimeTest, TestInternalResizeSetScaleMode2) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.g_w = 1280;
  cfg_.g_h = 720;
  set_scale_mode_ = false;
  set_scale_mode2_ = true;
  DefaultConfig();
  change_bitrate_ = false;
  mismatch_nframes_ = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Check we decoded the same number of frames as we attempted to encode
  ASSERT_EQ(frame_info_list_.size(), video.limit());
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const auto frame = static_cast<unsigned>(info->pts);
    unsigned int expected_w = 1280 >> 2;
    unsigned int expected_h = 720 >> 2;
    if (frame > 40) {
      expected_w = (3 * 1280) >> 2;
      expected_h = (3 * 720) >> 2;
    } else if (frame > 20 && frame <= 40) {
      expected_w = 1280 >> 1;
      expected_h = 720 >> 1;
    }
    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

TEST_P(ResizeRealtimeTest, TestExternalResizeWorks) {
  ResizingVideoSource video;
  video.flag_codec_ = 1;
  change_bitrate_ = false;
  set_scale_mode_ = false;
  set_scale_mode2_ = false;
  mismatch_psnr_ = 0.0;
  mismatch_nframes_ = 0;
  DefaultConfig();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // Check we decoded the same number of frames as we attempted to encode
  ASSERT_EQ(frame_info_list_.size(), video.limit());

  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const unsigned int frame = static_cast<unsigned>(info->pts);
    unsigned int expected_w;
    unsigned int expected_h;
    ScaleForFrameNumber(frame, kInitialWidth, kInitialHeight, &expected_w,
                        &expected_h, 1);
    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

// Verify the dynamic resizer behavior for real time, 1 pass CBR mode.
// Run at low bitrate, with resize_allowed = 1, and verify that we get
// one resize down event.
TEST_P(ResizeRealtimeTest, TestInternalResizeDown) {
  ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  cfg_.g_w = 640;
  cfg_.g_h = 480;
  change_bitrate_ = false;
  set_scale_mode_ = false;
  set_scale_mode2_ = false;
  mismatch_psnr_ = 0.0;
  mismatch_nframes_ = 0;
  DefaultConfig();
  // Disable dropped frames.
  cfg_.rc_dropframe_thresh = 0;
  // Starting bitrate low.
  cfg_.rc_target_bitrate = 150;
  cfg_.rc_resize_mode = RESIZE_DYNAMIC;
  cfg_.g_forced_max_frame_width = 1280;
  cfg_.g_forced_max_frame_height = 1280;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  unsigned int last_w = cfg_.g_w;
  unsigned int last_h = cfg_.g_h;
  int resize_down_count = 0;
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    if (info->w != last_w || info->h != last_h) {
      // Verify that resize down occurs.
      if (info->w < last_w && info->h < last_h) {
        resize_down_count++;
      }
      last_w = info->w;
      last_h = info->h;
    }
  }

#if CONFIG_AV1_DECODER
  // Verify that we get at lease 1 resize down event in this test.
  ASSERT_GE(resize_down_count, 1) << "Resizing should occur.";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
#else
  printf("Warning: AV1 decoder unavailable, unable to check resize count!\n");
#endif
}

// Verify the dynamic resizer behavior for real time, 1 pass CBR mode.
// Start at low target bitrate, raise the bitrate in the middle of the clip
// (at frame# = frame_change_bitrate_), scaling-up should occur after bitrate
// is increased.
TEST_P(ResizeRealtimeTest, TestInternalResizeDownUpChangeBitRate) {
  ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30, 1,
                                       0, 400);
  cfg_.g_w = 640;
  cfg_.g_h = 480;
  change_bitrate_ = true;
  frame_change_bitrate_ = 120;
  set_scale_mode_ = false;
  set_scale_mode2_ = false;
  mismatch_psnr_ = 0.0;
  mismatch_nframes_ = 0;
  DefaultConfig();
  // Disable dropped frames.
  cfg_.rc_dropframe_thresh = 0;
  // Starting bitrate low.
  cfg_.rc_target_bitrate = 150;
  cfg_.rc_resize_mode = RESIZE_DYNAMIC;
  cfg_.g_forced_max_frame_width = 1280;
  cfg_.g_forced_max_frame_height = 1280;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  unsigned int last_w = cfg_.g_w;
  unsigned int last_h = cfg_.g_h;
  unsigned int frame_number = 0;
  int resize_down_count = 0;
  int resize_up_count = 0;
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    if (info->w != last_w || info->h != last_h) {
      if (frame_number < frame_change_bitrate_) {
        // Verify that resize down occurs, before bitrate is increased.
        ASSERT_LT(info->w, last_w);
        ASSERT_LT(info->h, last_h);
        resize_down_count++;
      } else {
        // Verify that resize up occurs, after bitrate is increased.
        ASSERT_GT(info->w, last_w);
        ASSERT_GT(info->h, last_h);
        resize_up_count++;
      }
      last_w = info->w;
      last_h = info->h;
    }
    frame_number++;
  }

#if CONFIG_AV1_DECODER
  // Verify that we get at least 2 resize events in this test.
  ASSERT_GE(resize_up_count, 1) << "Resizing up should occur at lease once.";
  ASSERT_GE(resize_down_count, 1)
      << "Resizing down should occur at lease once.";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
#else
  printf("Warning: AV1 decoder unavailable, unable to check resize count!\n");
#endif
}

class ResizeCspTest : public ResizeTest {
 protected:
#if WRITE_COMPRESSED_STREAM
  ResizeCspTest()
      : ResizeTest(), frame0_psnr_(0.0), outfile_(nullptr), out_frames_(0) {}
#else
  ResizeCspTest() : ResizeTest(), frame0_psnr_(0.0) {}
#endif

  virtual ~ResizeCspTest() {}

  virtual void BeginPassHook(unsigned int /*pass*/) {
#if WRITE_COMPRESSED_STREAM
    outfile_ = fopen("av11-2-05-cspchape.ivf", "wb");
#endif
  }

  virtual void EndPassHook() {
#if WRITE_COMPRESSED_STREAM
    if (outfile_) {
      if (!fseek(outfile_, 0, SEEK_SET))
        write_ivf_file_header(&cfg_, out_frames_, outfile_);
      fclose(outfile_);
      outfile_ = nullptr;
    }
#endif
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    if (frame0_psnr_ == 0.) frame0_psnr_ = pkt->data.psnr.psnr[0];
    EXPECT_NEAR(pkt->data.psnr.psnr[0], frame0_psnr_, 2.0);
  }

#if WRITE_COMPRESSED_STREAM
  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    ++out_frames_;

    // Write initial file header if first frame.
    if (pkt->data.frame.pts == 0) write_ivf_file_header(&cfg_, 0, outfile_);

    // Write frame header and data.
    write_ivf_frame_header(pkt, outfile_);
    (void)fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz, outfile_);
  }
#endif

  double frame0_psnr_;
#if WRITE_COMPRESSED_STREAM
  FILE *outfile_;
  unsigned int out_frames_;
#endif
};

class ResizingCspVideoSource : public ::libaom_test::DummyVideoSource {
 public:
  explicit ResizingCspVideoSource(aom_img_fmt_t image_format) {
    SetSize(kInitialWidth, kInitialHeight);
    SetImageFormat(image_format);
    limit_ = 30;
  }

  virtual ~ResizingCspVideoSource() {}
};

#if (defined(DISABLE_TRELLISQ_SEARCH) && DISABLE_TRELLISQ_SEARCH)
TEST_P(ResizeCspTest, DISABLED_TestResizeCspWorks) {
#else
TEST_P(ResizeCspTest, TestResizeCspWorks) {
#endif
  const aom_img_fmt_t image_formats[] = { AOM_IMG_FMT_I420, AOM_IMG_FMT_I444 };
  for (const aom_img_fmt_t &img_format : image_formats) {
    ResizingCspVideoSource video(img_format);
    init_flags_ = AOM_CODEC_USE_PSNR;
    cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = 48;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_profile = (img_format == AOM_IMG_FMT_I420) ? 0 : 1;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

    // Check we decoded the same number of frames as we attempted to encode
    ASSERT_EQ(frame_info_list_.size(), video.limit());
    frame_info_list_.clear();
  }
}

#if !CONFIG_REALTIME_ONLY
// This class is used to check if there are any fatal
// failures while encoding with resize-mode > 0
class ResizeModeTestLarge
    : public ::libaom_test::CodecTestWith5Params<libaom_test::TestMode, int,
                                                 int, int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  ResizeModeTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        resize_mode_(GET_PARAM(2)), resize_denominator_(GET_PARAM(3)),
        resize_kf_denominator_(GET_PARAM(4)), cpu_used_(GET_PARAM(5)) {}
  virtual ~ResizeModeTestLarge() {}

  virtual void SetUp() {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.g_threads = 1;
    cfg_.g_lag_in_frames = 35;
    cfg_.rc_target_bitrate = 1000;
    cfg_.rc_resize_mode = resize_mode_;
    cfg_.rc_resize_denominator = resize_denominator_;
    cfg_.rc_resize_kf_denominator = resize_kf_denominator_;
    init_flags_ = AOM_CODEC_USE_PSNR;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
    }
  }

  ::libaom_test::TestMode encoding_mode_;
  int resize_mode_;
  int resize_denominator_;
  int resize_kf_denominator_;
  int cpu_used_;
};

TEST_P(ResizeModeTestLarge, ResizeModeTest) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 30);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ResizeModeTestLarge);
AV1_INSTANTIATE_TEST_SUITE(ResizeModeTestLarge,
                           ::testing::Values(::libaom_test::kOnePassGood,
                                             ::libaom_test::kTwoPassGood),
                           ::testing::Values(1, 2), ::testing::Values(8, 12),
                           ::testing::Values(10, 14), ::testing::Values(3, 6));
#endif  // !CONFIG_REALTIME_ONLY

AV1_INSTANTIATE_TEST_SUITE(ResizeTest,
                           ::testing::Values(::libaom_test::kRealTime));
AV1_INSTANTIATE_TEST_SUITE(ResizeRealtimeTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(6, 10));
AV1_INSTANTIATE_TEST_SUITE(ResizeCspTest,
                           ::testing::Values(::libaom_test::kRealTime));

}  // namespace
