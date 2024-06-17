/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <climits>

#include "aom/aom_image.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

TEST(AomImageTest, AomImgWrapInvalidAlign) {
  const int kWidth = 128;
  const int kHeight = 128;
  unsigned char buf[kWidth * kHeight * 3];

  aom_image_t img;
  // Set img_data and img_data_owner to junk values. aom_img_wrap() should
  // not read these values on failure.
  img.img_data = (unsigned char *)"";
  img.img_data_owner = 1;

  aom_img_fmt_t format = AOM_IMG_FMT_I444;
  // 'align' must be a power of 2 but is not. This causes the aom_img_wrap()
  // call to fail. The test verifies we do not read the junk values in 'img'.
  unsigned int align = 31;
  EXPECT_EQ(aom_img_wrap(&img, format, kWidth, kHeight, align, buf), nullptr);
}

TEST(AomImageTest, AomImgSetRectOverflow) {
  const int kWidth = 128;
  const int kHeight = 128;
  unsigned char buf[kWidth * kHeight * 3];

  aom_image_t img;
  aom_img_fmt_t format = AOM_IMG_FMT_I444;
  unsigned int align = 32;
  EXPECT_EQ(aom_img_wrap(&img, format, kWidth, kHeight, align, buf), &img);

  EXPECT_EQ(aom_img_set_rect(&img, 0, 0, kWidth, kHeight, 0), 0);
  // This would result in overflow because -1 is cast to UINT_MAX.
  EXPECT_NE(aom_img_set_rect(&img, static_cast<unsigned int>(-1),
                             static_cast<unsigned int>(-1), kWidth, kHeight, 0),
            0);
}

TEST(AomImageTest, AomImgAllocNone) {
  const int kWidth = 128;
  const int kHeight = 128;

  aom_image_t img;
  aom_img_fmt_t format = AOM_IMG_FMT_NONE;
  unsigned int align = 32;
  ASSERT_EQ(aom_img_alloc(&img, format, kWidth, kHeight, align), nullptr);
}

TEST(AomImageTest, AomImgAllocNv12) {
  const int kWidth = 128;
  const int kHeight = 128;

  aom_image_t img;
  aom_img_fmt_t format = AOM_IMG_FMT_NV12;
  unsigned int align = 32;
  EXPECT_EQ(aom_img_alloc(&img, format, kWidth, kHeight, align), &img);
  EXPECT_EQ(img.stride[AOM_PLANE_U], img.stride[AOM_PLANE_Y]);
  EXPECT_EQ(img.stride[AOM_PLANE_V], 0);
  EXPECT_EQ(img.planes[AOM_PLANE_V], nullptr);
  aom_img_free(&img);
}

TEST(AomImageTest, AomImgAllocHugeWidth) {
  // The stride (0x80000000 * 2) would overflow unsigned int.
  aom_image_t *image =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, 0x80000000, 1, 1);
  ASSERT_EQ(image, nullptr);

  // The stride (0x80000000) would overflow int.
  image = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 0x80000000, 1, 1);
  ASSERT_EQ(image, nullptr);

  // The aligned width (UINT_MAX + 1) would overflow unsigned int.
  image = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, UINT_MAX, 1, 1);
  ASSERT_EQ(image, nullptr);

  image = aom_img_alloc_with_border(nullptr, AOM_IMG_FMT_I422, 1, INT_MAX, 1,
                                    0x40000000, 0);
  if (image) {
    uint16_t *y_plane =
        reinterpret_cast<uint16_t *>(image->planes[AOM_PLANE_Y]);
    y_plane[0] = 0;
    y_plane[image->d_w - 1] = 0;
    aom_img_free(image);
  }

  image = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 0x7ffffffe, 1, 1);
  if (image) {
    aom_img_free(image);
  }

  image = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 285245883, 64, 1);
  if (image) {
    aom_img_free(image);
  }

  image = aom_img_alloc(nullptr, AOM_IMG_FMT_NV12, 285245883, 64, 1);
  if (image) {
    aom_img_free(image);
  }

  image = aom_img_alloc(nullptr, AOM_IMG_FMT_YV12, 285245883, 64, 1);
  if (image) {
    aom_img_free(image);
  }

  image = aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, 65536, 2, 1);
  if (image) {
    uint16_t *y_plane =
        reinterpret_cast<uint16_t *>(image->planes[AOM_PLANE_Y]);
    y_plane[0] = 0;
    y_plane[image->d_w - 1] = 0;
    aom_img_free(image);
  }

  image = aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, 285245883, 2, 1);
  if (image) {
    uint16_t *y_plane =
        reinterpret_cast<uint16_t *>(image->planes[AOM_PLANE_Y]);
    y_plane[0] = 0;
    y_plane[image->d_w - 1] = 0;
    aom_img_free(image);
  }
}
