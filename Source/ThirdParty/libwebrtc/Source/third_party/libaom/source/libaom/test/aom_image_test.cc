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
  EXPECT_NE(aom_img_set_rect(&img, -1, -1, kWidth, kHeight, 0), 0);
}

TEST(AomImageTest, AomImgAllocNv12) {
  const int kWidth = 128;
  const int kHeight = 128;

  aom_image_t img;
  aom_img_fmt_t format = AOM_IMG_FMT_NV12;
  unsigned int align = 32;
  EXPECT_NE(aom_img_alloc(&img, format, kWidth, kHeight, align), nullptr);
  EXPECT_EQ(img.stride[AOM_PLANE_U], img.stride[AOM_PLANE_Y]);
  EXPECT_EQ(img.stride[AOM_PLANE_V], 0);
  EXPECT_EQ(img.planes[AOM_PLANE_V], nullptr);
  aom_img_free(&img);
}
