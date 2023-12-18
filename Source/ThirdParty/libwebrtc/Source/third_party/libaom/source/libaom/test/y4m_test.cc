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

#include <string>

#include "config/aom_config.h"

#include "common/y4menc.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

using std::string;

static const unsigned int kWidth = 160;
static const unsigned int kHeight = 90;
static const unsigned int kFrames = 10;

struct Y4mTestParam {
  const char *filename;
  unsigned int bit_depth;
  aom_img_fmt format;
  const char *md5raw;
};

const Y4mTestParam kY4mTestVectors[] = {
  { "park_joy_90p_8_420.y4m", 8, AOM_IMG_FMT_I420,
    "e5406275b9fc6bb3436c31d4a05c1cab" },
  { "park_joy_90p_8_420_monochrome.y4m", 8, AOM_IMG_FMT_I420,
    "95ef5bf6218580588be24a5271bb6a7f" },
  { "park_joy_90p_8_420_vertical_csp.y4m", 8, AOM_IMG_FMT_I420,
    "e5406275b9fc6bb3436c31d4a05c1cab" },
  { "park_joy_90p_8_422.y4m", 8, AOM_IMG_FMT_I422,
    "284a47a47133b12884ec3a14e959a0b6" },
  { "park_joy_90p_8_444.y4m", 8, AOM_IMG_FMT_I444,
    "90517ff33843d85de712fd4fe60dbed0" },
  { "park_joy_90p_10_420.y4m", 10, AOM_IMG_FMT_I42016,
    "63f21f9f717d8b8631bd2288ee87137b" },
  { "park_joy_90p_10_422.y4m", 10, AOM_IMG_FMT_I42216,
    "48ab51fb540aed07f7ff5af130c9b605" },
  { "park_joy_90p_10_444.y4m", 10, AOM_IMG_FMT_I44416,
    "067bfd75aa85ff9bae91fa3e0edd1e3e" },
  { "park_joy_90p_12_420.y4m", 12, AOM_IMG_FMT_I42016,
    "9e6d8f6508c6e55625f6b697bc461cef" },
  { "park_joy_90p_12_422.y4m", 12, AOM_IMG_FMT_I42216,
    "b239c6b301c0b835485be349ca83a7e3" },
  { "park_joy_90p_12_444.y4m", 12, AOM_IMG_FMT_I44416,
    "5a6481a550821dab6d0192f5c63845e9" },
};

static const int PLANES_YUV[] = { AOM_PLANE_Y, AOM_PLANE_U, AOM_PLANE_V };

class Y4mVideoSourceTest : public ::testing::TestWithParam<Y4mTestParam>,
                           public ::libaom_test::Y4mVideoSource {
 protected:
  Y4mVideoSourceTest() : Y4mVideoSource("", 0, 0) {}

  ~Y4mVideoSourceTest() override { CloseSource(); }

  virtual void Init(const std::string &file_name, int limit) {
    file_name_ = file_name;
    start_ = 0;
    limit_ = limit;
    frame_ = 0;
    Begin();
  }

  // Checks y4m header information
  void HeaderChecks(unsigned int bit_depth, aom_img_fmt_t fmt) {
    ASSERT_NE(input_file_, nullptr);
    ASSERT_EQ(y4m_.pic_w, (int)kWidth);
    ASSERT_EQ(y4m_.pic_h, (int)kHeight);
    ASSERT_EQ(img()->d_w, kWidth);
    ASSERT_EQ(img()->d_h, kHeight);
    ASSERT_EQ(y4m_.bit_depth, bit_depth);
    ASSERT_EQ(y4m_.aom_fmt, fmt);
    if (fmt == AOM_IMG_FMT_I420 || fmt == AOM_IMG_FMT_I42016) {
      ASSERT_EQ(y4m_.bps, (int)y4m_.bit_depth * 3 / 2);
      ASSERT_EQ(img()->x_chroma_shift, 1U);
      ASSERT_EQ(img()->y_chroma_shift, 1U);
    }
    if (fmt == AOM_IMG_FMT_I422 || fmt == AOM_IMG_FMT_I42216) {
      ASSERT_EQ(y4m_.bps, (int)y4m_.bit_depth * 2);
      ASSERT_EQ(img()->x_chroma_shift, 1U);
      ASSERT_EQ(img()->y_chroma_shift, 0U);
    }
    if (fmt == AOM_IMG_FMT_I444 || fmt == AOM_IMG_FMT_I44416) {
      ASSERT_EQ(y4m_.bps, (int)y4m_.bit_depth * 3);
      ASSERT_EQ(img()->x_chroma_shift, 0U);
      ASSERT_EQ(img()->y_chroma_shift, 0U);
    }
  }

  // Checks MD5 of the raw frame data
  void Md5Check(const string &expected_md5) {
    ASSERT_NE(input_file_, nullptr);
    libaom_test::MD5 md5;
    for (unsigned int i = start_; i < limit_; i++) {
      md5.Add(img());
      Next();
    }
    ASSERT_EQ(string(md5.Get()), expected_md5);
  }
};

TEST_P(Y4mVideoSourceTest, SourceTest) {
  const Y4mTestParam t = GetParam();
  Init(t.filename, kFrames);
  HeaderChecks(t.bit_depth, t.format);
  Md5Check(t.md5raw);
}

INSTANTIATE_TEST_SUITE_P(C, Y4mVideoSourceTest,
                         ::testing::ValuesIn(kY4mTestVectors));

class Y4mVideoWriteTest : public Y4mVideoSourceTest {
 protected:
  Y4mVideoWriteTest() : tmpfile_(nullptr) {}

  ~Y4mVideoWriteTest() override {
    delete tmpfile_;
    input_file_ = nullptr;
  }

  void ReplaceInputFile(FILE *input_file) {
    CloseSource();
    frame_ = 0;
    input_file_ = input_file;
    rewind(input_file_);
    ReadSourceToStart();
  }

  // Writes out a y4m file and then reads it back
  void WriteY4mAndReadBack() {
    ASSERT_NE(input_file_, nullptr);
    char buf[Y4M_BUFFER_SIZE] = { 0 };
    const struct AvxRational framerate = { y4m_.fps_n, y4m_.fps_d };
    tmpfile_ = new libaom_test::TempOutFile;
    ASSERT_NE(tmpfile_, nullptr);
    ASSERT_NE(tmpfile_->file(), nullptr);
    y4m_write_file_header(buf, sizeof(buf), kWidth, kHeight, &framerate,
                          img()->monochrome, img()->csp, y4m_.aom_fmt,
                          y4m_.bit_depth, AOM_CR_STUDIO_RANGE);
    fputs(buf, tmpfile_->file());
    for (unsigned int i = start_; i < limit_; i++) {
      y4m_write_frame_header(buf, sizeof(buf));
      fputs(buf, tmpfile_->file());
      y4m_write_image_file(img(), PLANES_YUV, tmpfile_->file());
      Next();
    }
    ReplaceInputFile(tmpfile_->file());
  }

  void Init(const std::string &file_name, int limit) override {
    Y4mVideoSourceTest::Init(file_name, limit);
    WriteY4mAndReadBack();
  }
  libaom_test::TempOutFile *tmpfile_;
};

TEST_P(Y4mVideoWriteTest, WriteTest) {
  const Y4mTestParam t = GetParam();
  Init(t.filename, kFrames);
  HeaderChecks(t.bit_depth, t.format);
  Md5Check(t.md5raw);
}

INSTANTIATE_TEST_SUITE_P(C, Y4mVideoWriteTest,
                         ::testing::ValuesIn(kY4mTestVectors));

static const char kY4MRegularHeader[] =
    "YUV4MPEG2 W4 H4 F30:1 Ip A0:0 C420jpeg XYSCSS=420JPEG\n"
    "FRAME\n"
    "012345678912345601230123";

TEST(Y4MHeaderTest, RegularHeader) {
  libaom_test::TempOutFile f;
  ASSERT_NE(f.file(), nullptr);
  fwrite(kY4MRegularHeader, 1, sizeof(kY4MRegularHeader), f.file());
  fflush(f.file());
  EXPECT_EQ(0, fseek(f.file(), 0, 0));

  y4m_input y4m;
  EXPECT_EQ(y4m_input_open(&y4m, f.file(), nullptr, 0, AOM_CSP_UNKNOWN,
                           /*only_420=*/0),
            0);
  EXPECT_EQ(y4m.pic_w, 4);
  EXPECT_EQ(y4m.pic_h, 4);
  EXPECT_EQ(y4m.fps_n, 30);
  EXPECT_EQ(y4m.fps_d, 1);
  EXPECT_EQ(y4m.interlace, 'p');
  EXPECT_EQ(y4m.color_range, AOM_CR_STUDIO_RANGE);
  EXPECT_EQ(strcmp("420jpeg", y4m.chroma_type), 0);
  y4m_input_close(&y4m);
}

// Testing that headers over 100 characters can be parsed.
static const char kY4MLongHeader[] =
    "YUV4MPEG2 W4 H4 F30:1 Ip A0:0 C420jpeg XYSCSS=420JPEG "
    "XCOLORRANGE=LIMITED XSOME_UNKNOWN_METADATA XOTHER_UNKNOWN_METADATA\n"
    "FRAME\n"
    "012345678912345601230123";

TEST(Y4MHeaderTest, LongHeader) {
  libaom_test::TempOutFile tmpfile;
  FILE *f = tmpfile.file();
  ASSERT_NE(f, nullptr);
  fwrite(kY4MLongHeader, 1, sizeof(kY4MLongHeader), f);
  fflush(f);
  EXPECT_EQ(fseek(f, 0, 0), 0);

  y4m_input y4m;
  EXPECT_EQ(y4m_input_open(&y4m, f, nullptr, 0, AOM_CSP_UNKNOWN,
                           /*only_420=*/0),
            0);
  EXPECT_EQ(y4m.pic_w, 4);
  EXPECT_EQ(y4m.pic_h, 4);
  EXPECT_EQ(y4m.fps_n, 30);
  EXPECT_EQ(y4m.fps_d, 1);
  EXPECT_EQ(y4m.interlace, 'p');
  EXPECT_EQ(y4m.color_range, AOM_CR_STUDIO_RANGE);
  EXPECT_EQ(strcmp("420jpeg", y4m.chroma_type), 0);
  y4m_input_close(&y4m);
}

static const char kY4MFullRangeHeader[] =
    "YUV4MPEG2 W4 H4 F30:1 Ip A0:0 C420jpeg XYSCSS=420JPEG XCOLORRANGE=FULL\n"
    "FRAME\n"
    "012345678912345601230123";

TEST(Y4MHeaderTest, FullRangeHeader) {
  libaom_test::TempOutFile tmpfile;
  FILE *f = tmpfile.file();
  ASSERT_NE(f, nullptr);
  fwrite(kY4MFullRangeHeader, 1, sizeof(kY4MFullRangeHeader), f);
  fflush(f);
  EXPECT_EQ(fseek(f, 0, 0), 0);

  y4m_input y4m;
  EXPECT_EQ(y4m_input_open(&y4m, f, nullptr, 0, AOM_CSP_UNKNOWN,
                           /*only_420=*/0),
            0);
  EXPECT_EQ(y4m.pic_w, 4);
  EXPECT_EQ(y4m.pic_h, 4);
  EXPECT_EQ(y4m.fps_n, 30);
  EXPECT_EQ(y4m.fps_d, 1);
  EXPECT_EQ(y4m.interlace, 'p');
  EXPECT_EQ(strcmp("420jpeg", y4m.chroma_type), 0);
  EXPECT_EQ(y4m.color_range, AOM_CR_FULL_RANGE);
  y4m_input_close(&y4m);
}

TEST(Y4MHeaderTest, WriteStudioColorRange) {
  char buf[128];
  struct AvxRational framerate = { /*numerator=*/30, /*denominator=*/1 };
  EXPECT_GE(y4m_write_file_header(
                buf, /*len=*/128, /*width=*/4, /*height=*/5, &framerate,
                /*monochrome=*/0, AOM_CSP_UNKNOWN, AOM_IMG_FMT_I420,
                /*bit_depth=*/8, AOM_CR_STUDIO_RANGE),
            0);
  EXPECT_EQ(strcmp("YUV4MPEG2 W4 H5 F30:1 Ip C420jpeg\n", buf), 0);
}

TEST(Y4MHeaderTest, WriteFullColorRange) {
  char buf[128];
  struct AvxRational framerate = { /*numerator=*/30, /*denominator=*/1 };
  EXPECT_GE(y4m_write_file_header(
                buf, /*len=*/128, /*width=*/4, /*height=*/5, &framerate,
                /*monochrome=*/0, AOM_CSP_UNKNOWN, AOM_IMG_FMT_I420,
                /*bit_depth=*/8, AOM_CR_FULL_RANGE),
            0);
  EXPECT_EQ(strcmp("YUV4MPEG2 W4 H5 F30:1 Ip C420jpeg XCOLORRANGE=FULL\n", buf),
            0);
}

}  // namespace
