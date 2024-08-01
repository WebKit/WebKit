/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Lightfield Encoder
// ==================
//
// This is an example of a simple lightfield encoder.  It builds upon the
// twopass_encoder.c example. It takes an input file in YV12 format,
// treating it as a planar lightfield instead of a video. The img_width
// and img_height arguments are the dimensions of the lightfield images,
// while the lf_width and lf_height arguments are the number of
// lightfield images in each dimension. The lf_blocksize determines the
// number of reference images used for MCP. For example, 5 means that there
// is a reference image for every 5x5 lightfield image block. All images
// within a block will use the center image in that block as the reference
// image for MCP.
// Run "make test" to download lightfield test data: vase10x10.yuv.
// Run lightfield encoder to encode whole lightfield:
// examples/lightfield_encoder 1024 1024 vase10x10.yuv vase10x10.ivf 10 10 5

// Note: In bitstream.c and encoder.c, define EXT_TILE_DEBUG as 1 will print
// out the uncompressed header and the frame contexts, which can be used to
// test the bit exactness of the headers and the frame contexts for large scale
// tile coded frames.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "aom_scale/yv12config.h"
#include "av1/common/enums.h"
#include "av1/encoder/encoder_utils.h"
#include "common/tools_common.h"
#include "common/video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <img_width> <img_height> <infile> <outfile> "
          "<lf_width> <lf_height> <lf_blocksize>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static int img_size_bytes(aom_image_t *img) {
  int image_size_bytes = 0;
  int plane;
  for (plane = 0; plane < 3; ++plane) {
    const int w = aom_img_plane_width(img, plane) *
                  ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = aom_img_plane_height(img, plane);
    image_size_bytes += w * h;
  }
  return image_size_bytes;
}

static int get_frame_stats(aom_codec_ctx_t *ctx, const aom_image_t *img,
                           aom_codec_pts_t pts, unsigned int duration,
                           aom_enc_frame_flags_t flags,
                           aom_fixed_buf_t *stats) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res = aom_codec_encode(ctx, img, pts, duration, flags);
  if (res != AOM_CODEC_OK) die_codec(ctx, "Failed to get frame stats.");

  while ((pkt = aom_codec_get_cx_data(ctx, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == AOM_CODEC_STATS_PKT) {
      const uint8_t *const pkt_buf = pkt->data.twopass_stats.buf;
      const size_t pkt_size = pkt->data.twopass_stats.sz;
      stats->buf = realloc(stats->buf, stats->sz + pkt_size);
      if (!stats->buf) die("Failed to allocate frame stats buffer.");
      memcpy((uint8_t *)stats->buf + stats->sz, pkt_buf, pkt_size);
      stats->sz += pkt_size;
    }
  }

  return got_pkts;
}

static int encode_frame(aom_codec_ctx_t *ctx, const aom_image_t *img,
                        aom_codec_pts_t pts, unsigned int duration,
                        aom_enc_frame_flags_t flags, AvxVideoWriter *writer) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res = aom_codec_encode(ctx, img, pts, duration, flags);
  if (res != AOM_CODEC_OK) die_codec(ctx, "Failed to encode frame.");

  while ((pkt = aom_codec_get_cx_data(ctx, &iter)) != NULL) {
    got_pkts = 1;
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;

      if (!aom_video_writer_write_frame(writer, pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts))
        die_codec(ctx, "Failed to write compressed frame.");
      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }

  return got_pkts;
}

static void get_raw_image(aom_image_t **frame_to_encode, aom_image_t *raw,
                          aom_image_t *raw_shift) {
  if (FORCE_HIGHBITDEPTH_DECODING) {
    // Need to allocate larger buffer to use hbd internal.
    int input_shift = 0;
    aom_img_upshift(raw_shift, raw, input_shift);
    *frame_to_encode = raw_shift;
  } else {
    *frame_to_encode = raw;
  }
}

static aom_fixed_buf_t pass0(aom_image_t *raw, FILE *infile,
                             aom_codec_iface_t *encoder,
                             const aom_codec_enc_cfg_t *cfg, int lf_width,
                             int lf_height, int lf_blocksize, int flags,
                             aom_image_t *raw_shift) {
  aom_codec_ctx_t codec;
  int frame_count = 0;
  int image_size_bytes = img_size_bytes(raw);
  int u_blocks, v_blocks;
  int bu, bv;
  aom_fixed_buf_t stats = { NULL, 0 };
  aom_image_t *frame_to_encode;

  if (aom_codec_enc_init(&codec, encoder, cfg, flags))
    die("Failed to initialize encoder");
  if (aom_codec_control(&codec, AOME_SET_ENABLEAUTOALTREF, 0))
    die_codec(&codec, "Failed to turn off auto altref");
  if (aom_codec_control(&codec, AV1E_SET_FRAME_PARALLEL_DECODING, 0))
    die_codec(&codec, "Failed to set frame parallel decoding");

  // How many reference images we need to encode.
  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;

  printf("\n First pass: ");

  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u_block_size, v_block_size;
      int block_ref_u, block_ref_v;

      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      u_block_size = block_u_end - block_u_min;
      v_block_size = block_v_end - block_v_min;
      block_ref_u = block_u_min + u_block_size / 2;
      block_ref_v = block_v_min + v_block_size / 2;

      printf("A%d, ", (block_ref_u + block_ref_v * lf_width));
      fseek(infile, (block_ref_u + block_ref_v * lf_width) * image_size_bytes,
            SEEK_SET);
      aom_img_read(raw, infile);
      get_raw_image(&frame_to_encode, raw, raw_shift);

      // Reference frames can be encoded encoded without tiles.
      ++frame_count;
      get_frame_stats(&codec, frame_to_encode, frame_count, 1,
                      AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                          AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                          AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                          AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                          AOM_EFLAG_NO_UPD_ARF,
                      &stats);
    }
  }

  if (aom_codec_control(&codec, AV1E_SET_FRAME_PARALLEL_DECODING, 1))
    die_codec(&codec, "Failed to set frame parallel decoding");

  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u, v;
      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      for (v = block_v_min; v < block_v_end; ++v) {
        for (u = block_u_min; u < block_u_end; ++u) {
          printf("C%d, ", (u + v * lf_width));
          fseek(infile, (u + v * lf_width) * image_size_bytes, SEEK_SET);
          aom_img_read(raw, infile);
          get_raw_image(&frame_to_encode, raw, raw_shift);

          ++frame_count;
          get_frame_stats(&codec, frame_to_encode, frame_count, 1,
                          AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                              AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                              AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                              AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                              AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_ENTROPY,
                          &stats);
        }
      }
    }
  }
  // Flush encoder.
  // No ARF, this should not be needed.
  while (get_frame_stats(&codec, NULL, frame_count, 1, 0, &stats)) {
  }

  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  printf("\nFirst pass complete. Processed %d frames.\n", frame_count);

  return stats;
}

static void pass1(aom_image_t *raw, FILE *infile, const char *outfile_name,
                  aom_codec_iface_t *encoder, aom_codec_enc_cfg_t *cfg,
                  int lf_width, int lf_height, int lf_blocksize, int flags,
                  aom_image_t *raw_shift) {
  AvxVideoInfo info = { get_fourcc_by_aom_encoder(encoder),
                        cfg->g_w,
                        cfg->g_h,
                        { cfg->g_timebase.num, cfg->g_timebase.den },
                        0 };
  AvxVideoWriter *writer = NULL;
  aom_codec_ctx_t codec;
  int frame_count = 0;
  int image_size_bytes = img_size_bytes(raw);
  int bu, bv;
  int u_blocks, v_blocks;
  aom_image_t *frame_to_encode;
  aom_image_t reference_images[MAX_EXTERNAL_REFERENCES];
  int reference_image_num = 0;
  int i;

  writer = aom_video_writer_open(outfile_name, kContainerIVF, &info);
  if (!writer) die("Failed to open %s for writing", outfile_name);

  if (aom_codec_enc_init(&codec, encoder, cfg, flags))
    die("Failed to initialize encoder");
  if (aom_codec_control(&codec, AOME_SET_ENABLEAUTOALTREF, 0))
    die_codec(&codec, "Failed to turn off auto altref");
  if (aom_codec_control(&codec, AV1E_SET_FRAME_PARALLEL_DECODING, 0))
    die_codec(&codec, "Failed to set frame parallel decoding");
  if (aom_codec_control(&codec, AV1E_ENABLE_EXT_TILE_DEBUG, 1))
    die_codec(&codec, "Failed to enable encoder ext_tile debug");
  if (aom_codec_control(&codec, AOME_SET_CPUUSED, 3))
    die_codec(&codec, "Failed to set cpu-used");

  // Note: The superblock is a sequence parameter and has to be the same for 1
  // sequence. In lightfield application, must choose the superblock size(either
  // 64x64 or 128x128) before the encoding starts. Otherwise, the default is
  // AOM_SUPERBLOCK_SIZE_DYNAMIC, and the superblock size will be set to 64x64
  // internally.
  if (aom_codec_control(&codec, AV1E_SET_SUPERBLOCK_SIZE,
                        AOM_SUPERBLOCK_SIZE_64X64))
    die_codec(&codec, "Failed to set SB size");

  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;

  reference_image_num = u_blocks * v_blocks;
  // Set the max gf group length so the references are guaranteed to be in
  // a different gf group than any of the regular frames. This avoids using
  // both vbr and constant quality mode in a single group. The number of
  // references now cannot surpass 17 because of the enforced MAX_GF_INTERVAL of
  // 16. If it is necessary to exceed this reference frame limit, one will have
  // to do some additional handling to ensure references are in separate gf
  // groups from the regular frames.
  if (aom_codec_control(&codec, AV1E_SET_MAX_GF_INTERVAL,
                        reference_image_num - 1))
    die_codec(&codec, "Failed to set max gf interval");
  aom_img_fmt_t ref_fmt = AOM_IMG_FMT_I420;
  if (FORCE_HIGHBITDEPTH_DECODING) ref_fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
  // Allocate memory with the border so that it can be used as a reference.
  const bool resize =
      codec.config.enc->rc_resize_mode || codec.config.enc->rc_superres_mode;
  const bool all_intra = reference_image_num - 1 == 0;
  int border_in_pixels =
      av1_get_enc_border_size(resize, all_intra, BLOCK_64X64);

  for (i = 0; i < reference_image_num; i++) {
    if (!aom_img_alloc_with_border(&reference_images[i], ref_fmt, cfg->g_w,
                                   cfg->g_h, 32, 8, border_in_pixels)) {
      die("Failed to allocate image.");
    }
  }

  printf("\n Second pass: ");

  // Encode reference images first.
  printf("Encoding Reference Images\n");
  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u_block_size, v_block_size;
      int block_ref_u, block_ref_v;

      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      u_block_size = block_u_end - block_u_min;
      v_block_size = block_v_end - block_v_min;
      block_ref_u = block_u_min + u_block_size / 2;
      block_ref_v = block_v_min + v_block_size / 2;

      printf("A%d, ", (block_ref_u + block_ref_v * lf_width));
      fseek(infile, (block_ref_u + block_ref_v * lf_width) * image_size_bytes,
            SEEK_SET);
      aom_img_read(raw, infile);

      get_raw_image(&frame_to_encode, raw, raw_shift);

      // Reference frames may be encoded without tiles.
      ++frame_count;
      printf("Encoding reference image %d of %d\n", bv * u_blocks + bu,
             u_blocks * v_blocks);
      encode_frame(&codec, frame_to_encode, frame_count, 1,
                   AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                       AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                       AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                       AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                       AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_ENTROPY,
                   writer);

      if (aom_codec_control(&codec, AV1_COPY_NEW_FRAME_IMAGE,
                            &reference_images[frame_count - 1]))
        die_codec(&codec, "Failed to copy decoder reference frame");
    }
  }

  cfg->large_scale_tile = 1;
  // Fixed q encoding for camera frames.
  cfg->rc_end_usage = AOM_Q;
  if (aom_codec_enc_config_set(&codec, cfg))
    die_codec(&codec, "Failed to configure encoder");

  // The fixed q value used in encoding.
  if (aom_codec_control(&codec, AOME_SET_CQ_LEVEL, 36))
    die_codec(&codec, "Failed to set cq level");
  if (aom_codec_control(&codec, AV1E_SET_FRAME_PARALLEL_DECODING, 1))
    die_codec(&codec, "Failed to set frame parallel decoding");
  if (aom_codec_control(&codec, AV1E_SET_SINGLE_TILE_DECODING, 1))
    die_codec(&codec, "Failed to turn on single tile decoding");
  // Set tile_columns and tile_rows to MAX values, which guarantees the tile
  // size of 64 x 64 pixels(i.e. 1 SB) for <= 4k resolution.
  if (aom_codec_control(&codec, AV1E_SET_TILE_COLUMNS, 6))
    die_codec(&codec, "Failed to set tile width");
  if (aom_codec_control(&codec, AV1E_SET_TILE_ROWS, 6))
    die_codec(&codec, "Failed to set tile height");

  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u, v;
      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      for (v = block_v_min; v < block_v_end; ++v) {
        for (u = block_u_min; u < block_u_end; ++u) {
          av1_ref_frame_t ref;
          ref.idx = 0;
          ref.use_external_ref = 1;
          ref.img = reference_images[bv * u_blocks + bu];
          if (aom_codec_control(&codec, AV1_SET_REFERENCE, &ref))
            die_codec(&codec, "Failed to set reference frame");

          printf("C%d, ", (u + v * lf_width));
          fseek(infile, (u + v * lf_width) * image_size_bytes, SEEK_SET);
          aom_img_read(raw, infile);
          get_raw_image(&frame_to_encode, raw, raw_shift);

          ++frame_count;
          printf("Encoding image %d of %d\n",
                 frame_count - (u_blocks * v_blocks), lf_width * lf_height);
          encode_frame(&codec, frame_to_encode, frame_count, 1,
                       AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                           AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                           AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                           AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                           AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_ENTROPY,
                       writer);
        }
      }
    }
  }

  // Flush encoder.
  // No ARF, this should not be needed.
  while (encode_frame(&codec, NULL, -1, 1, 0, writer)) {
  }

  for (i = 0; i < reference_image_num; i++) aom_img_free(&reference_images[i]);

  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  // Modify large_scale_file fourcc.
  if (cfg->large_scale_tile == 1)
    aom_video_writer_set_fourcc(writer, LST_FOURCC);
  aom_video_writer_close(writer);

  printf("\nSecond pass complete. Processed %d frames.\n", frame_count);
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  int w, h;
  // The number of lightfield images in the u and v dimensions.
  int lf_width, lf_height;
  // Defines how many images refer to the same reference image for MCP.
  // lf_blocksize X lf_blocksize images will all use the reference image
  // in the middle of the block of images.
  int lf_blocksize;
  aom_codec_ctx_t codec;
  aom_codec_enc_cfg_t cfg;
  aom_image_t raw;
  aom_image_t raw_shift;
  aom_codec_err_t res;
  aom_fixed_buf_t stats;
  int flags = 0;

  const int fps = 30;
  const int bitrate = 200;  // kbit/s
  const char *const width_arg = argv[1];
  const char *const height_arg = argv[2];
  const char *const infile_arg = argv[3];
  const char *const outfile_arg = argv[4];
  const char *const lf_width_arg = argv[5];
  const char *const lf_height_arg = argv[6];
  const char *lf_blocksize_arg = argv[7];
  exec_name = argv[0];

  if (argc < 8) die("Invalid number of arguments");

  aom_codec_iface_t *encoder = get_aom_encoder_by_short_name("av1");
  if (!encoder) die("Unsupported codec.");

  w = (int)strtol(width_arg, NULL, 0);
  h = (int)strtol(height_arg, NULL, 0);
  lf_width = (int)strtol(lf_width_arg, NULL, 0);
  lf_height = (int)strtol(lf_height_arg, NULL, 0);
  lf_blocksize = (int)strtol(lf_blocksize_arg, NULL, 0);
  lf_blocksize = lf_blocksize < lf_width ? lf_blocksize : lf_width;
  lf_blocksize = lf_blocksize < lf_height ? lf_blocksize : lf_height;

  if (w <= 0 || h <= 0 || (w % 2) != 0 || (h % 2) != 0)
    die("Invalid frame size: %dx%d", w, h);
  if (lf_width <= 0 || lf_height <= 0)
    die("Invalid lf_width and/or lf_height: %dx%d", lf_width, lf_height);
  if (lf_blocksize <= 0) die("Invalid lf_blocksize: %d", lf_blocksize);

  if (!aom_img_alloc(&raw, AOM_IMG_FMT_I420, w, h, 32)) {
    die("Failed to allocate image.");
  }
  if (FORCE_HIGHBITDEPTH_DECODING) {
    // Need to allocate larger buffer to use hbd internal.
    aom_img_alloc(&raw_shift, AOM_IMG_FMT_I420 | AOM_IMG_FMT_HIGHBITDEPTH, w, h,
                  32);
  }

  printf("Using %s\n", aom_codec_iface_name(encoder));

  // Configuration
  res = aom_codec_enc_config_default(encoder, &cfg, 0);
  if (res) die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = w;
  cfg.g_h = h;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = fps;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = 0;  // This is required.
  cfg.g_lag_in_frames = 0;    // need to set this since default is 19.
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.large_scale_tile = 0;  // Only set it to 1 for camera frame encoding.
  cfg.g_bit_depth = AOM_BITS_8;
  flags |= (cfg.g_bit_depth > AOM_BITS_8 || FORCE_HIGHBITDEPTH_DECODING)
               ? AOM_CODEC_USE_HIGHBITDEPTH
               : 0;

  if (!(infile = fopen(infile_arg, "rb")))
    die("Failed to open %s for reading", infile_arg);

  // Pass 0
  cfg.g_pass = AOM_RC_FIRST_PASS;
  stats = pass0(&raw, infile, encoder, &cfg, lf_width, lf_height, lf_blocksize,
                flags, &raw_shift);

  // Pass 1
  rewind(infile);
  cfg.g_pass = AOM_RC_LAST_PASS;
  cfg.rc_twopass_stats_in = stats;
  pass1(&raw, infile, outfile_arg, encoder, &cfg, lf_width, lf_height,
        lf_blocksize, flags, &raw_shift);
  free(stats.buf);

  if (FORCE_HIGHBITDEPTH_DECODING) aom_img_free(&raw_shift);
  aom_img_free(&raw);
  fclose(infile);

  return EXIT_SUCCESS;
}
