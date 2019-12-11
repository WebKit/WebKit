/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>

#include <climits>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/video_source.h"
#include "test/util.h"

// Enable(1) or Disable(0) writing of the compressed bitstream.
#define WRITE_COMPRESSED_STREAM 0

namespace {

#if WRITE_COMPRESSED_STREAM
static void mem_put_le16(char *const mem, const unsigned int val) {
  mem[0] = val;
  mem[1] = val >> 8;
}

static void mem_put_le32(char *const mem, const unsigned int val) {
  mem[0] = val;
  mem[1] = val >> 8;
  mem[2] = val >> 16;
  mem[3] = val >> 24;
}

static void write_ivf_file_header(const vpx_codec_enc_cfg_t *const cfg,
                                  int frame_cnt, FILE *const outfile) {
  char header[32];

  header[0] = 'D';
  header[1] = 'K';
  header[2] = 'I';
  header[3] = 'F';
  mem_put_le16(header + 4, 0);                    /* version */
  mem_put_le16(header + 6, 32);                   /* headersize */
  mem_put_le32(header + 8, 0x30395056);           /* fourcc (vp9) */
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

static void write_ivf_frame_header(const vpx_codec_cx_pkt_t *const pkt,
                                   FILE *const outfile) {
  char header[12];
  vpx_codec_pts_t pts;

  if (pkt->kind != VPX_CODEC_CX_FRAME_PKT) return;

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
  FrameInfo(vpx_codec_pts_t _pts, unsigned int _w, unsigned int _h)
      : pts(_pts), w(_w), h(_h) {}

  vpx_codec_pts_t pts;
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
  if (frame < 120) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 130) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 140) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 150) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  if (frame < 160) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 170) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 180) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 190) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  if (frame < 200) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 210) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 220) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 230) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  if (frame < 240) {
    *w = initial_w * 3 / 4;
    *h = initial_h * 3 / 4;
    return;
  }
  if (frame < 250) {
    *w = initial_w / 2;
    *h = initial_h / 2;
    return;
  }
  if (frame < 260) {
    *w = initial_w;
    *h = initial_h;
    return;
  }
  // Go down very low.
  if (frame < 270) {
    *w = initial_w / 4;
    *h = initial_h / 4;
    return;
  }
  if (flag_codec == 1) {
    // Cases that only works for VP9.
    // For VP9: Swap width and height of original.
    if (frame < 320) {
      *w = initial_h;
      *h = initial_w;
      return;
    }
  }
  *w = initial_w;
  *h = initial_h;
}

class ResizingVideoSource : public ::libvpx_test::DummyVideoSource {
 public:
  ResizingVideoSource() {
    SetSize(kInitialWidth, kInitialHeight);
    limit_ = 350;
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
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWithParam<libvpx_test::TestMode> {
 protected:
  ResizeTest() : EncoderTest(GET_PARAM(0)) {}

  virtual ~ResizeTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    ASSERT_NE(static_cast<int>(pkt->data.frame.width[0]), 0);
    ASSERT_NE(static_cast<int>(pkt->data.frame.height[0]), 0);
    encode_frame_width_.push_back(pkt->data.frame.width[0]);
    encode_frame_height_.push_back(pkt->data.frame.height[0]);
  }

  unsigned int GetFrameWidth(size_t idx) const {
    return encode_frame_width_[idx];
  }

  unsigned int GetFrameHeight(size_t idx) const {
    return encode_frame_height_[idx];
  }

  virtual void DecompressedFrameHook(const vpx_image_t &img,
                                     vpx_codec_pts_t pts) {
    frame_info_list_.push_back(FrameInfo(pts, img.d_w, img.d_h));
  }

  std::vector<FrameInfo> frame_info_list_;
  std::vector<unsigned int> encode_frame_width_;
  std::vector<unsigned int> encode_frame_height_;
};

TEST_P(ResizeTest, TestExternalResizeWorks) {
  ResizingVideoSource video;
  video.flag_codec_ = 0;
  cfg_.g_lag_in_frames = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const unsigned int frame = static_cast<unsigned>(info->pts);
    unsigned int expected_w;
    unsigned int expected_h;
    const size_t idx = info - frame_info_list_.begin();
    ASSERT_EQ(info->w, GetFrameWidth(idx));
    ASSERT_EQ(info->h, GetFrameHeight(idx));
    ScaleForFrameNumber(frame, kInitialWidth, kInitialHeight, &expected_w,
                        &expected_h, 0);
    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
  }
}

const unsigned int kStepDownFrame = 3;
const unsigned int kStepUpFrame = 6;

class ResizeInternalTest : public ResizeTest {
 protected:
#if WRITE_COMPRESSED_STREAM
  ResizeInternalTest()
      : ResizeTest(), frame0_psnr_(0.0), outfile_(NULL), out_frames_(0) {}
#else
  ResizeInternalTest() : ResizeTest(), frame0_psnr_(0.0) {}
#endif

  virtual ~ResizeInternalTest() {}

  virtual void BeginPassHook(unsigned int /*pass*/) {
#if WRITE_COMPRESSED_STREAM
    outfile_ = fopen("vp90-2-05-resize.ivf", "wb");
#endif
  }

  virtual void EndPassHook() {
#if WRITE_COMPRESSED_STREAM
    if (outfile_) {
      if (!fseek(outfile_, 0, SEEK_SET))
        write_ivf_file_header(&cfg_, out_frames_, outfile_);
      fclose(outfile_);
      outfile_ = NULL;
    }
#endif
  }

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  libvpx_test::Encoder *encoder) {
    if (change_config_) {
      int new_q = 60;
      if (video->frame() == 0) {
        struct vpx_scaling_mode mode = { VP8E_ONETWO, VP8E_ONETWO };
        encoder->Control(VP8E_SET_SCALEMODE, &mode);
      }
      if (video->frame() == 1) {
        struct vpx_scaling_mode mode = { VP8E_NORMAL, VP8E_NORMAL };
        encoder->Control(VP8E_SET_SCALEMODE, &mode);
        cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = new_q;
        encoder->Config(&cfg_);
      }
    } else {
      if (video->frame() == kStepDownFrame) {
        struct vpx_scaling_mode mode = { VP8E_FOURFIVE, VP8E_THREEFIVE };
        encoder->Control(VP8E_SET_SCALEMODE, &mode);
      }
      if (video->frame() == kStepUpFrame) {
        struct vpx_scaling_mode mode = { VP8E_NORMAL, VP8E_NORMAL };
        encoder->Control(VP8E_SET_SCALEMODE, &mode);
      }
    }
  }

  virtual void PSNRPktHook(const vpx_codec_cx_pkt_t *pkt) {
    if (frame0_psnr_ == 0.) frame0_psnr_ = pkt->data.psnr.psnr[0];
    EXPECT_NEAR(pkt->data.psnr.psnr[0], frame0_psnr_, 2.0);
  }

#if WRITE_COMPRESSED_STREAM
  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
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

TEST_P(ResizeInternalTest, TestInternalResizeWorks) {
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 10);
  init_flags_ = VPX_CODEC_USE_PSNR;
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
    const vpx_codec_pts_t pts = info->pts;
    if (pts >= kStepDownFrame && pts < kStepUpFrame) {
      ASSERT_EQ(282U, info->w) << "Frame " << pts << " had unexpected width";
      ASSERT_EQ(173U, info->h) << "Frame " << pts << " had unexpected height";
    } else {
      EXPECT_EQ(352U, info->w) << "Frame " << pts << " had unexpected width";
      EXPECT_EQ(288U, info->h) << "Frame " << pts << " had unexpected height";
    }
  }
}

TEST_P(ResizeInternalTest, TestInternalResizeChangeConfig) {
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 10);
  cfg_.g_w = 352;
  cfg_.g_h = 288;
  change_config_ = true;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

class ResizeRealtimeTest
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 protected:
  ResizeRealtimeTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~ResizeRealtimeTest() {}

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(VP9E_SET_AQ_MODE, 3);
      encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);
    }

    if (change_bitrate_ && video->frame() == 120) {
      change_bitrate_ = false;
      cfg_.rc_target_bitrate = 500;
      encoder->Config(&cfg_);
    }
  }

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
  }

  virtual void DecompressedFrameHook(const vpx_image_t &img,
                                     vpx_codec_pts_t pts) {
    frame_info_list_.push_back(FrameInfo(pts, img.d_w, img.d_h));
  }

  virtual void MismatchHook(const vpx_image_t *img1, const vpx_image_t *img2) {
    double mismatch_psnr = compute_psnr(img1, img2);
    mismatch_psnr_ += mismatch_psnr;
    ++mismatch_nframes_;
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    ASSERT_NE(static_cast<int>(pkt->data.frame.width[0]), 0);
    ASSERT_NE(static_cast<int>(pkt->data.frame.height[0]), 0);
    encode_frame_width_.push_back(pkt->data.frame.width[0]);
    encode_frame_height_.push_back(pkt->data.frame.height[0]);
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }

  unsigned int GetFrameWidth(size_t idx) const {
    return encode_frame_width_[idx];
  }

  unsigned int GetFrameHeight(size_t idx) const {
    return encode_frame_height_[idx];
  }

  void DefaultConfig() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_end_usage = VPX_CBR;
    cfg_.kf_mode = VPX_KF_AUTO;
    cfg_.g_lag_in_frames = 0;
    cfg_.kf_min_dist = cfg_.kf_max_dist = 3000;
    // Enable dropped frames.
    cfg_.rc_dropframe_thresh = 1;
    // Enable error_resilience mode.
    cfg_.g_error_resilient = 1;
    // Enable dynamic resizing.
    cfg_.rc_resize_allowed = 1;
    // Run at low bitrate.
    cfg_.rc_target_bitrate = 200;
  }

  std::vector<FrameInfo> frame_info_list_;
  int set_cpu_used_;
  bool change_bitrate_;
  double mismatch_psnr_;
  int mismatch_nframes_;
  std::vector<unsigned int> encode_frame_width_;
  std::vector<unsigned int> encode_frame_height_;
};

TEST_P(ResizeRealtimeTest, TestExternalResizeWorks) {
  ResizingVideoSource video;
  video.flag_codec_ = 1;
  DefaultConfig();
  // Disable internal resize for this test.
  cfg_.rc_resize_allowed = 0;
  change_bitrate_ = false;
  mismatch_psnr_ = 0.0;
  mismatch_nframes_ = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

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
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);
  DefaultConfig();
  cfg_.g_w = 352;
  cfg_.g_h = 288;
  change_bitrate_ = false;
  mismatch_psnr_ = 0.0;
  mismatch_nframes_ = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  unsigned int last_w = cfg_.g_w;
  unsigned int last_h = cfg_.g_h;
  int resize_count = 0;
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    if (info->w != last_w || info->h != last_h) {
      // Verify that resize down occurs.
      ASSERT_LT(info->w, last_w);
      ASSERT_LT(info->h, last_h);
      last_w = info->w;
      last_h = info->h;
      resize_count++;
    }
  }

#if CONFIG_VP9_DECODER
  // Verify that we get 1 resize down event in this test.
  ASSERT_EQ(1, resize_count) << "Resizing should occur.";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
#else
  printf("Warning: VP9 decoder unavailable, unable to check resize count!\n");
#endif
}

// Verify the dynamic resizer behavior for real time, 1 pass CBR mode.
// Start at low target bitrate, raise the bitrate in the middle of the clip,
// scaling-up should occur after bitrate changed.
TEST_P(ResizeRealtimeTest, TestInternalResizeDownUpChangeBitRate) {
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 359);
  DefaultConfig();
  cfg_.g_w = 352;
  cfg_.g_h = 288;
  change_bitrate_ = true;
  mismatch_psnr_ = 0.0;
  mismatch_nframes_ = 0;
  // Disable dropped frames.
  cfg_.rc_dropframe_thresh = 0;
  // Starting bitrate low.
  cfg_.rc_target_bitrate = 80;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  unsigned int last_w = cfg_.g_w;
  unsigned int last_h = cfg_.g_h;
  int resize_count = 0;
  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const size_t idx = info - frame_info_list_.begin();
    ASSERT_EQ(info->w, GetFrameWidth(idx));
    ASSERT_EQ(info->h, GetFrameHeight(idx));
    if (info->w != last_w || info->h != last_h) {
      resize_count++;
      if (resize_count == 1) {
        // Verify that resize down occurs.
        ASSERT_LT(info->w, last_w);
        ASSERT_LT(info->h, last_h);
      } else if (resize_count == 2) {
        // Verify that resize up occurs.
        ASSERT_GT(info->w, last_w);
        ASSERT_GT(info->h, last_h);
      }
      last_w = info->w;
      last_h = info->h;
    }
  }

#if CONFIG_VP9_DECODER
  // Verify that we get 2 resize events in this test.
  ASSERT_EQ(resize_count, 2) << "Resizing should occur twice.";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
#else
  printf("Warning: VP9 decoder unavailable, unable to check resize count!\n");
#endif
}

vpx_img_fmt_t CspForFrameNumber(int frame) {
  if (frame < 10) return VPX_IMG_FMT_I420;
  if (frame < 20) return VPX_IMG_FMT_I444;
  return VPX_IMG_FMT_I420;
}

class ResizeCspTest : public ResizeTest {
 protected:
#if WRITE_COMPRESSED_STREAM
  ResizeCspTest()
      : ResizeTest(), frame0_psnr_(0.0), outfile_(NULL), out_frames_(0) {}
#else
  ResizeCspTest() : ResizeTest(), frame0_psnr_(0.0) {}
#endif

  virtual ~ResizeCspTest() {}

  virtual void BeginPassHook(unsigned int /*pass*/) {
#if WRITE_COMPRESSED_STREAM
    outfile_ = fopen("vp91-2-05-cspchape.ivf", "wb");
#endif
  }

  virtual void EndPassHook() {
#if WRITE_COMPRESSED_STREAM
    if (outfile_) {
      if (!fseek(outfile_, 0, SEEK_SET))
        write_ivf_file_header(&cfg_, out_frames_, outfile_);
      fclose(outfile_);
      outfile_ = NULL;
    }
#endif
  }

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  libvpx_test::Encoder *encoder) {
    if (CspForFrameNumber(video->frame()) != VPX_IMG_FMT_I420 &&
        cfg_.g_profile != 1) {
      cfg_.g_profile = 1;
      encoder->Config(&cfg_);
    }
    if (CspForFrameNumber(video->frame()) == VPX_IMG_FMT_I420 &&
        cfg_.g_profile != 0) {
      cfg_.g_profile = 0;
      encoder->Config(&cfg_);
    }
  }

  virtual void PSNRPktHook(const vpx_codec_cx_pkt_t *pkt) {
    if (frame0_psnr_ == 0.) frame0_psnr_ = pkt->data.psnr.psnr[0];
    EXPECT_NEAR(pkt->data.psnr.psnr[0], frame0_psnr_, 2.0);
  }

#if WRITE_COMPRESSED_STREAM
  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
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

class ResizingCspVideoSource : public ::libvpx_test::DummyVideoSource {
 public:
  ResizingCspVideoSource() {
    SetSize(kInitialWidth, kInitialHeight);
    limit_ = 30;
  }

  virtual ~ResizingCspVideoSource() {}

 protected:
  virtual void Next() {
    ++frame_;
    SetImageFormat(CspForFrameNumber(frame_));
    FillFrame();
  }
};

TEST_P(ResizeCspTest, TestResizeCspWorks) {
  ResizingCspVideoSource video;
  init_flags_ = VPX_CODEC_USE_PSNR;
  cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = 48;
  cfg_.g_lag_in_frames = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

VP8_INSTANTIATE_TEST_CASE(ResizeTest, ONE_PASS_TEST_MODES);
VP9_INSTANTIATE_TEST_CASE(ResizeTest,
                          ::testing::Values(::libvpx_test::kRealTime));
VP9_INSTANTIATE_TEST_CASE(ResizeInternalTest,
                          ::testing::Values(::libvpx_test::kOnePassBest));
VP9_INSTANTIATE_TEST_CASE(ResizeRealtimeTest,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Range(5, 9));
VP9_INSTANTIATE_TEST_CASE(ResizeCspTest,
                          ::testing::Values(::libvpx_test::kRealTime));
}  // namespace
