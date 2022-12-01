/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "common/tools_common.h"
#include "config/aom_config.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/md5_helper.h"
#include "test/test_vectors.h"
#include "test/util.h"
#if CONFIG_WEBM_IO
#include "test/webm_video_source.h"
#endif

namespace {

const int kThreads = 0;
const int kFileName = 1;
const int kRowMT = 2;

typedef std::tuple<int, const char *, int> DecodeParam;

class TestVectorTest : public ::libaom_test::DecoderTest,
                       public ::libaom_test::CodecTestWithParam<DecodeParam> {
 protected:
  TestVectorTest() : DecoderTest(GET_PARAM(0)), md5_file_(nullptr) {}

  virtual ~TestVectorTest() {
    if (md5_file_) fclose(md5_file_);
  }

  void OpenMD5File(const std::string &md5_file_name_) {
    md5_file_ = libaom_test::OpenTestDataFile(md5_file_name_);
    ASSERT_NE(md5_file_, nullptr)
        << "Md5 file open failed. Filename: " << md5_file_name_;
  }

  virtual void PreDecodeFrameHook(
      const libaom_test::CompressedVideoSource &video,
      libaom_test::Decoder *decoder) {
    if (video.frame_number() == 0) decoder->Control(AV1D_SET_ROW_MT, row_mt_);
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     const unsigned int frame_number) {
    ASSERT_NE(md5_file_, nullptr);
    char expected_md5[33];
    char junk[128];

    // Read correct md5 checksums.
    const int res = fscanf(md5_file_, "%s  %s", expected_md5, junk);
    ASSERT_NE(res, EOF) << "Read md5 data failed";
    expected_md5[32] = '\0';

    ::libaom_test::MD5 md5_res;
#if FORCE_HIGHBITDEPTH_DECODING
    const aom_img_fmt_t shifted_fmt =
        (aom_img_fmt)(img.fmt & ~AOM_IMG_FMT_HIGHBITDEPTH);
    if (img.bit_depth == 8 && shifted_fmt != img.fmt) {
      aom_image_t *img_shifted =
          aom_img_alloc(nullptr, shifted_fmt, img.d_w, img.d_h, 16);
      img_shifted->bit_depth = img.bit_depth;
      img_shifted->monochrome = img.monochrome;
      aom_img_downshift(img_shifted, &img, 0);
      md5_res.Add(img_shifted);
      aom_img_free(img_shifted);
    } else {
#endif
      md5_res.Add(&img);
#if FORCE_HIGHBITDEPTH_DECODING
    }
#endif

    const char *actual_md5 = md5_res.Get();
    // Check md5 match.
    ASSERT_STREQ(expected_md5, actual_md5)
        << "Md5 checksums don't match: frame number = " << frame_number;
  }

  unsigned int row_mt_;

 private:
  FILE *md5_file_;
};

// This test runs through the whole set of test vectors, and decodes them.
// The md5 checksums are computed for each frame in the video file. If md5
// checksums match the correct md5 data, then the test is passed. Otherwise,
// the test failed.
TEST_P(TestVectorTest, MD5Match) {
  const DecodeParam input = GET_PARAM(1);
  const std::string filename = std::get<kFileName>(input);
  aom_codec_flags_t flags = 0;
  aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
  char str[256];

  cfg.threads = std::get<kThreads>(input);
  row_mt_ = std::get<kRowMT>(input);

  snprintf(str, sizeof(str) / sizeof(str[0]) - 1, "file: %s threads: %d",
           filename.c_str(), cfg.threads);
  SCOPED_TRACE(str);

  // Open compressed video file.
  std::unique_ptr<libaom_test::CompressedVideoSource> video;
  if (filename.substr(filename.length() - 3, 3) == "ivf") {
    video.reset(new libaom_test::IVFVideoSource(filename));
  } else if (filename.substr(filename.length() - 4, 4) == "webm" ||
             filename.substr(filename.length() - 3, 3) == "mkv") {
#if CONFIG_WEBM_IO
    video.reset(new libaom_test::WebMVideoSource(filename));
#else
    fprintf(stderr, "WebM IO is disabled, skipping test vector %s\n",
            filename.c_str());
    return;
#endif
  }
  ASSERT_NE(video, nullptr);
  video->Init();

  // Construct md5 file name.
  const std::string md5_filename = filename + ".md5";
  OpenMD5File(md5_filename);

  // Set decode config and flags.
  cfg.allow_lowbitdepth = !FORCE_HIGHBITDEPTH_DECODING;
  set_cfg(cfg);
  set_flags(flags);

  // Decode frame, and check the md5 matching.
  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get(), cfg));
}

#if CONFIG_AV1_DECODER
AV1_INSTANTIATE_TEST_SUITE(
    TestVectorTest,
    ::testing::Combine(::testing::Values(1),  // Single thread.
                       ::testing::ValuesIn(libaom_test::kAV1TestVectors,
                                           libaom_test::kAV1TestVectors +
                                               libaom_test::kNumAV1TestVectors),
                       ::testing::Values(0)));

// Test AV1 decode in with different numbers of threads.
INSTANTIATE_TEST_SUITE_P(
    AV1MultiThreaded, TestVectorTest,
    ::testing::Combine(
        ::testing::Values(
            static_cast<const libaom_test::CodecFactory *>(&libaom_test::kAV1)),
        ::testing::Combine(
            ::testing::Range(2, 9),  // With 2 ~ 8 threads.
            ::testing::ValuesIn(libaom_test::kAV1TestVectors,
                                libaom_test::kAV1TestVectors +
                                    libaom_test::kNumAV1TestVectors),
            ::testing::Range(0, 2))));

#endif  // CONFIG_AV1_DECODER

}  // namespace
