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

#include <memory>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"

#include "aom_ports/mem.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/encode_test_driver.h"
#include "test/register_state_check.h"
#include "test/video_source.h"

namespace libaom_test {
void Encoder::InitEncoder(VideoSource *video) {
  aom_codec_err_t res;
  const aom_image_t *img = video->img();

  if (video->img() && !encoder_.priv) {
    cfg_.g_w = img->d_w;
    cfg_.g_h = img->d_h;
    cfg_.g_timebase = video->timebase();
    cfg_.rc_twopass_stats_in = stats_->buf();

    res = aom_codec_enc_init(&encoder_, CodecInterface(), &cfg_, init_flags_);
    ASSERT_EQ(AOM_CODEC_OK, res) << EncoderError();
  }
}

void Encoder::EncodeFrame(VideoSource *video,
                          const aom_enc_frame_flags_t frame_flags) {
  if (video->img())
    EncodeFrameInternal(*video, frame_flags);
  else
    Flush();

  // Handle twopass stats
  CxDataIterator iter = GetCxData();

  while (const aom_codec_cx_pkt_t *pkt = iter.Next()) {
    if (pkt->kind != AOM_CODEC_STATS_PKT) continue;

    stats_->Append(*pkt);
  }
}

void Encoder::EncodeFrameInternal(const VideoSource &video,
                                  const aom_enc_frame_flags_t frame_flags) {
  aom_codec_err_t res;
  const aom_image_t *img = video.img();

  // Handle frame resizing
  if (cfg_.g_w != img->d_w || cfg_.g_h != img->d_h) {
    cfg_.g_w = img->d_w;
    cfg_.g_h = img->d_h;
    res = aom_codec_enc_config_set(&encoder_, &cfg_);
    ASSERT_EQ(AOM_CODEC_OK, res) << EncoderError();
  }

  // Encode the frame
  API_REGISTER_STATE_CHECK(res =
                               aom_codec_encode(&encoder_, img, video.pts(),
                                                video.duration(), frame_flags));
  ASSERT_EQ(AOM_CODEC_OK, res) << EncoderError();
}

void Encoder::Flush() {
  const aom_codec_err_t res = aom_codec_encode(&encoder_, nullptr, 0, 0, 0);
  if (!encoder_.priv)
    ASSERT_EQ(AOM_CODEC_ERROR, res) << EncoderError();
  else
    ASSERT_EQ(AOM_CODEC_OK, res) << EncoderError();
}

void EncoderTest::InitializeConfig(TestMode mode) {
  int usage = AOM_USAGE_GOOD_QUALITY;
  switch (mode) {
    case kOnePassGood:
    case kTwoPassGood: break;
    case kRealTime: usage = AOM_USAGE_REALTIME; break;
    case kAllIntra: usage = AOM_USAGE_ALL_INTRA; break;
    default: ASSERT_TRUE(false) << "Unexpected mode " << mode;
  }
  mode_ = mode;
  passes_ = (mode == kTwoPassGood) ? 2 : 1;

  const aom_codec_err_t res = codec_->DefaultEncoderConfig(&cfg_, usage);
  ASSERT_EQ(AOM_CODEC_OK, res);
}

static bool compare_plane(const uint8_t *const buf1, int stride1,
                          const uint8_t *const buf2, int stride2, int w, int h,
                          int *const mismatch_row, int *const mismatch_col,
                          int *const mismatch_pix1, int *const mismatch_pix2) {
  int r, c;

  for (r = 0; r < h; ++r) {
    for (c = 0; c < w; ++c) {
      const int pix1 = buf1[r * stride1 + c];
      const int pix2 = buf2[r * stride2 + c];

      if (pix1 != pix2) {
        if (mismatch_row != nullptr) *mismatch_row = r;
        if (mismatch_col != nullptr) *mismatch_col = c;
        if (mismatch_pix1 != nullptr) *mismatch_pix1 = pix1;
        if (mismatch_pix2 != nullptr) *mismatch_pix2 = pix2;
        return false;
      }
    }
  }

  return true;
}

// The function should return "true" most of the time, therefore no early
// break-out is implemented within the match checking process.
static bool compare_img(const aom_image_t *img1, const aom_image_t *img2,
                        int *const mismatch_row, int *const mismatch_col,
                        int *const mismatch_plane, int *const mismatch_pix1,
                        int *const mismatch_pix2) {
  if (img1->fmt != img2->fmt || img1->cp != img2->cp || img1->tc != img2->tc ||
      img1->mc != img2->mc || img1->d_w != img2->d_w ||
      img1->d_h != img2->d_h || img1->monochrome != img2->monochrome) {
    if (mismatch_row != nullptr) *mismatch_row = -1;
    if (mismatch_col != nullptr) *mismatch_col = -1;
    return false;
  }

  const int num_planes = img1->monochrome ? 1 : 3;
  for (int plane = 0; plane < num_planes; plane++) {
    if (!compare_plane(img1->planes[plane], img1->stride[plane],
                       img2->planes[plane], img2->stride[plane],
                       aom_img_plane_width(img1, plane),
                       aom_img_plane_height(img1, plane), mismatch_row,
                       mismatch_col, mismatch_pix1, mismatch_pix2)) {
      if (mismatch_plane != nullptr) *mismatch_plane = plane;
      return false;
    }
  }

  return true;
}

void EncoderTest::MismatchHook(const aom_image_t *img_enc,
                               const aom_image_t *img_dec) {
  int mismatch_row = 0;
  int mismatch_col = 0;
  int mismatch_plane = 0;
  int mismatch_pix_enc = 0;
  int mismatch_pix_dec = 0;

  ASSERT_FALSE(compare_img(img_enc, img_dec, &mismatch_row, &mismatch_col,
                           &mismatch_plane, &mismatch_pix_enc,
                           &mismatch_pix_dec));

  GTEST_FAIL() << "Encode/Decode mismatch found:" << std::endl
               << "  pixel value enc/dec: " << mismatch_pix_enc << "/"
               << mismatch_pix_dec << std::endl
               << "                plane: " << mismatch_plane << std::endl
               << "              row/col: " << mismatch_row << "/"
               << mismatch_col << std::endl;
}

void EncoderTest::RunLoop(VideoSource *video) {
  stats_.Reset();

  ASSERT_TRUE(passes_ == 1 || passes_ == 2);
  for (unsigned int pass = 0; pass < passes_; pass++) {
    aom_codec_pts_t last_pts = 0;

    if (passes_ == 1)
      cfg_.g_pass = AOM_RC_ONE_PASS;
    else if (pass == 0)
      cfg_.g_pass = AOM_RC_FIRST_PASS;
    else
      cfg_.g_pass = AOM_RC_LAST_PASS;

    BeginPassHook(pass);
    std::unique_ptr<Encoder> encoder(
        codec_->CreateEncoder(cfg_, init_flags_, &stats_));
    ASSERT_NE(encoder, nullptr);

    ASSERT_NO_FATAL_FAILURE(video->Begin());
    encoder->InitEncoder(video);

    if (mode_ == kRealTime) {
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 0);
    }

    ASSERT_FALSE(::testing::Test::HasFatalFailure());
#if CONFIG_AV1_DECODER
    aom_codec_dec_cfg_t dec_cfg = aom_codec_dec_cfg_t();
    dec_cfg.allow_lowbitdepth = 1;
    std::unique_ptr<Decoder> decoder(
        codec_->CreateDecoder(dec_cfg, 0 /* flags */));
    if (decoder->IsAV1()) {
      // Set dec_cfg.tile_row = -1 and dec_cfg.tile_col = -1 so that the whole
      // frame is decoded.
      decoder->Control(AV1_SET_TILE_MODE, cfg_.large_scale_tile);
      decoder->Control(AV1D_EXT_TILE_DEBUG, 1);
      decoder->Control(AV1_SET_DECODE_TILE_ROW, -1);
      decoder->Control(AV1_SET_DECODE_TILE_COL, -1);
    }
#endif

    int number_spatial_layers = GetNumSpatialLayers();

    bool again;
    for (again = true; again; video->Next()) {
      again = (video->img() != nullptr);

      for (int sl = 0; sl < number_spatial_layers; sl++) {
        PreEncodeFrameHook(video, encoder.get());
        encoder->EncodeFrame(video, frame_flags_);
        PostEncodeFrameHook(encoder.get());
        CxDataIterator iter = encoder->GetCxData();
        bool has_cxdata = false;

#if CONFIG_AV1_DECODER
        bool has_dxdata = false;
#endif
        while (const aom_codec_cx_pkt_t *pkt = iter.Next()) {
          pkt = MutateEncoderOutputHook(pkt);
          again = true;
          switch (pkt->kind) {
            case AOM_CODEC_CX_FRAME_PKT:  //
              has_cxdata = true;
#if CONFIG_AV1_DECODER
              if (decoder.get() != nullptr && DoDecode()) {
                aom_codec_err_t res_dec;
                if (DoDecodeInvisible()) {
                  res_dec = decoder->DecodeFrame(
                      (const uint8_t *)pkt->data.frame.buf, pkt->data.frame.sz);
                } else {
                  res_dec = decoder->DecodeFrame(
                      (const uint8_t *)pkt->data.frame.buf +
                          (pkt->data.frame.sz - pkt->data.frame.vis_frame_size),
                      pkt->data.frame.vis_frame_size);
                }

                if (!HandleDecodeResult(res_dec, decoder.get())) break;

                has_dxdata = true;
              }
#endif
              ASSERT_GE(pkt->data.frame.pts, last_pts);
              if (sl == number_spatial_layers - 1)
                last_pts = pkt->data.frame.pts;
              FramePktHook(pkt);
              break;

            case AOM_CODEC_PSNR_PKT: PSNRPktHook(pkt); break;

            case AOM_CODEC_STATS_PKT: StatsPktHook(pkt); break;

            default: break;
          }
        }
        if (has_cxdata) {
          const aom_image_t *img_enc = encoder->GetPreviewFrame();
          if (img_enc) {
            CalculateFrameLevelSSIM(video->img(), img_enc, cfg_.g_bit_depth,
                                    cfg_.g_input_bit_depth);
          }
#if CONFIG_AV1_DECODER
          if (has_dxdata) {
            DxDataIterator dec_iter = decoder->GetDxData();
            const aom_image_t *img_dec = dec_iter.Next();
            if (img_enc && img_dec) {
              const bool res = compare_img(img_enc, img_dec, nullptr, nullptr,
                                           nullptr, nullptr, nullptr);
              if (!res) {  // Mismatch
                MismatchHook(img_enc, img_dec);
              }
            }
            if (img_dec) DecompressedFrameHook(*img_dec, video->pts());
          }
#endif
        }
        if (!Continue()) break;
      }  // Loop over spatial layers
    }

    EndPassHook();

    if (!Continue()) break;
  }
}

}  // namespace libaom_test
