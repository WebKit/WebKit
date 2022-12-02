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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "aom/aom_codec.h"
#include "aom/aom_image.h"
#include "aom/internal/aom_image_internal.h"
#include "aom_scale/yv12config.h"
#include "av1/encoder/bitstream.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {
const size_t kMetadataPayloadSizeT35 = 24;
// 0xB5 stands for the itut t35 metadata country code for the Unites States
const uint8_t kMetadataPayloadT35[kMetadataPayloadSizeT35] = {
  0xB5, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
  0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
};

const size_t kMetadataPayloadSizeCll = 4;
const uint8_t kMetadataPayloadCll[kMetadataPayloadSizeCll] = { 0xB5, 0x01, 0x02,
                                                               0x03 };

const size_t kMetadataObuSizeT35 = 28;
const uint8_t kMetadataObuT35[kMetadataObuSizeT35] = {
  0x2A, 0x1A, 0x02, 0xB5, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x80
};
const size_t kMetadataObuSizeMdcv = 28;
const uint8_t kMetadataObuMdcv[kMetadataObuSizeMdcv] = {
  0x2A, 0x1A, 0x02, 0xB5, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x80
};
const size_t kMetadataObuSizeCll = 8;
const uint8_t kMetadataObuCll[kMetadataObuSizeCll] = { 0x2A, 0x06, 0x01, 0xB5,
                                                       0x01, 0x02, 0x03, 0x80 };

class MetadataEncodeTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  MetadataEncodeTest() : EncoderTest(GET_PARAM(0)) {}

  virtual ~MetadataEncodeTest() {}

  virtual void SetUp() { InitializeConfig(GET_PARAM(1)); }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder * /*encoder*/) {
    aom_image_t *current_frame = video->img();
    if (current_frame) {
      if (current_frame->metadata) aom_img_remove_metadata(current_frame);
      ASSERT_EQ(aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_ITUT_T35,
                                     kMetadataPayloadT35, 0, AOM_MIF_ANY_FRAME),
                -1);
      ASSERT_EQ(aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_ITUT_T35,
                                     nullptr, kMetadataPayloadSizeT35,
                                     AOM_MIF_ANY_FRAME),
                -1);
      ASSERT_EQ(aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_ITUT_T35,
                                     nullptr, 0, AOM_MIF_ANY_FRAME),
                -1);
      ASSERT_EQ(
          aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_ITUT_T35,
                               kMetadataPayloadT35, kMetadataPayloadSizeT35,
                               AOM_MIF_ANY_FRAME),
          0);

      ASSERT_EQ(
          aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_HDR_MDCV,
                               kMetadataPayloadT35, kMetadataPayloadSizeT35,
                               AOM_MIF_KEY_FRAME),
          0);

      ASSERT_EQ(
          aom_img_add_metadata(current_frame, OBU_METADATA_TYPE_HDR_CLL,
                               kMetadataPayloadCll, kMetadataPayloadSizeCll,
                               AOM_MIF_KEY_FRAME),
          0);
    }
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const size_t bitstream_size = pkt->data.frame.sz;
      const uint8_t *bitstream =
          static_cast<const uint8_t *>(pkt->data.frame.buf);
      // look for valid metadatas in bitstream
      bool itut_t35_metadata_found = false;
      if (bitstream_size >= kMetadataObuSizeT35) {
        for (size_t i = 0; i <= bitstream_size - kMetadataObuSizeT35; ++i) {
          if (memcmp(bitstream + i, kMetadataObuT35, kMetadataObuSizeT35) ==
              0) {
            itut_t35_metadata_found = true;
          }
        }
      }
      ASSERT_EQ(itut_t35_metadata_found, 1u);

      // Testing for HDR MDCV metadata
      bool hdr_mdcv_metadata_found = false;
      if (bitstream_size >= kMetadataObuSizeMdcv) {
        for (size_t i = 0; i <= bitstream_size - kMetadataObuSizeMdcv; ++i) {
          if (memcmp(bitstream + i, kMetadataObuMdcv, kMetadataObuSizeMdcv) ==
              0) {
            hdr_mdcv_metadata_found = true;
          }
        }
      }
      ASSERT_TRUE(hdr_mdcv_metadata_found);

      // Testing for HDR CLL metadata
      bool hdr_cll_metadata_found = false;
      if (bitstream_size >= kMetadataObuSizeCll) {
        for (size_t i = 0; i <= bitstream_size - kMetadataObuSizeCll; ++i) {
          if (memcmp(bitstream + i, kMetadataObuCll, kMetadataObuSizeCll) ==
              0) {
            hdr_cll_metadata_found = true;
          }
        }
      }
      ASSERT_TRUE(hdr_cll_metadata_found);
    }
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t /*pts*/) {
    ASSERT_NE(img.metadata, nullptr);

    ASSERT_EQ(img.metadata->sz, 3u);

    for (size_t i = 0; i < img.metadata->sz - 1; ++i) {
      ASSERT_EQ(kMetadataPayloadSizeT35, img.metadata->metadata_array[i]->sz);
      EXPECT_EQ(
          memcmp(kMetadataPayloadT35, img.metadata->metadata_array[i]->payload,
                 kMetadataPayloadSizeT35),
          0);
    }

    ASSERT_EQ(kMetadataPayloadSizeCll, img.metadata->metadata_array[2]->sz);
    EXPECT_EQ(
        memcmp(kMetadataPayloadCll, img.metadata->metadata_array[2]->payload,
               kMetadataPayloadSizeCll),
        0);
  }
};

TEST_P(MetadataEncodeTest, TestMetadataEncoding) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 5);
  init_flags_ = AOM_CODEC_USE_PSNR;

  cfg_.g_w = 352;
  cfg_.g_h = 288;

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 600;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_undershoot_pct = 50;
  cfg_.rc_overshoot_pct = 50;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.kf_mode = AOM_KF_AUTO;
  cfg_.g_lag_in_frames = 1;
  cfg_.kf_min_dist = cfg_.kf_max_dist = 3000;
  // Enable dropped frames.
  cfg_.rc_dropframe_thresh = 1;
  // Disable error_resilience mode.
  cfg_.g_error_resilient = 0;
  // Run at low bitrate.
  cfg_.rc_target_bitrate = 40;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(MetadataEncodeTest,
                           ::testing::Values(::libaom_test::kOnePassGood));

}  // namespace

TEST(MetadataTest, MetadataAllocation) {
  aom_metadata_t *metadata =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, kMetadataPayloadT35,
                             kMetadataPayloadSizeT35, AOM_MIF_ANY_FRAME);
  ASSERT_NE(metadata, nullptr);
  aom_img_metadata_free(metadata);
}

TEST(MetadataTest, MetadataArrayAllocation) {
  aom_metadata_array_t *metadata_array = aom_img_metadata_array_alloc(2);
  ASSERT_NE(metadata_array, nullptr);

  metadata_array->metadata_array[0] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, kMetadataPayloadT35,
                             kMetadataPayloadSizeT35, AOM_MIF_ANY_FRAME);
  metadata_array->metadata_array[1] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, kMetadataPayloadT35,
                             kMetadataPayloadSizeT35, AOM_MIF_ANY_FRAME);

  aom_img_metadata_array_free(metadata_array);
}

TEST(MetadataTest, AddMetadataToImage) {
  aom_image_t image;
  image.metadata = nullptr;

  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35,
                                 kMetadataPayloadT35, kMetadataPayloadSizeT35,
                                 AOM_MIF_ANY_FRAME),
            0);
  aom_img_metadata_array_free(image.metadata);
  EXPECT_EQ(aom_img_add_metadata(nullptr, OBU_METADATA_TYPE_ITUT_T35,
                                 kMetadataPayloadT35, kMetadataPayloadSizeT35,
                                 AOM_MIF_ANY_FRAME),
            -1);
}

TEST(MetadataTest, RemoveMetadataFromImage) {
  aom_image_t image;
  image.metadata = nullptr;

  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35,
                                 kMetadataPayloadT35, kMetadataPayloadSizeT35,
                                 AOM_MIF_ANY_FRAME),
            0);
  aom_img_remove_metadata(&image);
  aom_img_remove_metadata(nullptr);
}

TEST(MetadataTest, CopyMetadataToFrameBuffer) {
  YV12_BUFFER_CONFIG yvBuf;
  yvBuf.metadata = nullptr;

  aom_metadata_array_t *metadata_array = aom_img_metadata_array_alloc(1);
  ASSERT_NE(metadata_array, nullptr);

  metadata_array->metadata_array[0] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, kMetadataPayloadT35,
                             kMetadataPayloadSizeT35, AOM_MIF_ANY_FRAME);

  // Metadata_array
  int status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array);
  EXPECT_EQ(status, 0);
  status = aom_copy_metadata_to_frame_buffer(nullptr, metadata_array);
  EXPECT_EQ(status, -1);
  aom_img_metadata_array_free(metadata_array);

  // Metadata_array_2
  aom_metadata_array_t *metadata_array_2 = aom_img_metadata_array_alloc(0);
  ASSERT_NE(metadata_array_2, nullptr);
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array_2);
  EXPECT_EQ(status, -1);
  aom_img_metadata_array_free(metadata_array_2);

  // YV12_BUFFER_CONFIG
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, nullptr);
  EXPECT_EQ(status, -1);
  aom_remove_metadata_from_frame_buffer(&yvBuf);
  aom_remove_metadata_from_frame_buffer(nullptr);
}

TEST(MetadataTest, GetMetadataFromImage) {
  aom_image_t image;
  image.metadata = nullptr;

  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35,
                                 kMetadataPayloadT35, kMetadataPayloadSizeT35,
                                 AOM_MIF_ANY_FRAME),
            0);

  EXPECT_EQ(aom_img_get_metadata(nullptr, 0), nullptr);
  EXPECT_EQ(aom_img_get_metadata(&image, 1u), nullptr);
  EXPECT_EQ(aom_img_get_metadata(&image, 10u), nullptr);

  const aom_metadata_t *metadata = aom_img_get_metadata(&image, 0);
  ASSERT_NE(metadata, nullptr);
  ASSERT_EQ(metadata->sz, kMetadataPayloadSizeT35);
  EXPECT_EQ(
      memcmp(kMetadataPayloadT35, metadata->payload, kMetadataPayloadSizeT35),
      0);

  aom_img_metadata_array_free(image.metadata);
}

TEST(MetadataTest, ReadMetadatasFromImage) {
  aom_image_t image;
  image.metadata = nullptr;

  uint32_t types[3];
  types[0] = OBU_METADATA_TYPE_ITUT_T35;
  types[1] = OBU_METADATA_TYPE_HDR_CLL;
  types[2] = OBU_METADATA_TYPE_HDR_MDCV;

  ASSERT_EQ(aom_img_add_metadata(&image, types[0], kMetadataPayloadT35,
                                 kMetadataPayloadSizeT35, AOM_MIF_ANY_FRAME),
            0);
  ASSERT_EQ(aom_img_add_metadata(&image, types[1], kMetadataPayloadT35,
                                 kMetadataPayloadSizeT35, AOM_MIF_KEY_FRAME),
            0);
  ASSERT_EQ(aom_img_add_metadata(&image, types[2], kMetadataPayloadT35,
                                 kMetadataPayloadSizeT35, AOM_MIF_KEY_FRAME),
            0);

  size_t number_metadata = aom_img_num_metadata(&image);
  ASSERT_EQ(number_metadata, 3u);
  for (size_t i = 0; i < number_metadata; ++i) {
    const aom_metadata_t *metadata = aom_img_get_metadata(&image, i);
    ASSERT_NE(metadata, nullptr);
    ASSERT_EQ(metadata->type, types[i]);
    ASSERT_EQ(metadata->sz, kMetadataPayloadSizeT35);
    EXPECT_EQ(
        memcmp(kMetadataPayloadT35, metadata->payload, kMetadataPayloadSizeT35),
        0);
  }
  aom_img_metadata_array_free(image.metadata);
}
