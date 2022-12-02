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

// AOM Set Active and ROI Maps
// ===========================
//
// This is an example demonstrating how to control the AOM encoder's
// ROI and Active maps.
//
// ROI (Reigon of Interest) maps are a way for the application to assign
// each macroblock in the image to a region, and then set quantizer and
// filtering parameters on that image.
//
// Active maps are a way for the application to specify on a
// macroblock-by-macroblock basis whether there is any activity in that
// macroblock.
//
//
// Configuration
// -------------
// An ROI map is set on frame 22. If the width of the image in macroblocks
// is evenly divisble by 4, then the output will appear to have distinct
// columns, where the quantizer, loopfilter, and static threshold differ
// from column to column.
//
// An active map is set on frame 33. If the width of the image in macroblocks
// is evenly divisble by 4, then the output will appear to have distinct
// columns, where one column will have motion and the next will not.
//
// The active map is cleared on frame 44.
//
// Observing The Effects
// ---------------------
// Use the `simple_decoder` example to decode this sample, and observe
// the change in the image at frames 22, 33, and 44.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "common/tools_common.h"
#include "common/video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <codec> <width> <height> <infile> <outfile>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static void set_active_map(const aom_codec_enc_cfg_t *cfg,
                           aom_codec_ctx_t *codec) {
  unsigned int i;
  aom_active_map_t map = { 0, 0, 0 };

  map.rows = (cfg->g_h + 15) / 16;
  map.cols = (cfg->g_w + 15) / 16;

  map.active_map = (uint8_t *)malloc(map.rows * map.cols);
  if (!map.active_map) die("Failed to allocate active map");
  for (i = 0; i < map.rows * map.cols; ++i) map.active_map[i] = i % 2;

  if (aom_codec_control(codec, AOME_SET_ACTIVEMAP, &map))
    die_codec(codec, "Failed to set active map");

  free(map.active_map);
}

static void unset_active_map(const aom_codec_enc_cfg_t *cfg,
                             aom_codec_ctx_t *codec) {
  aom_active_map_t map = { 0, 0, 0 };

  map.rows = (cfg->g_h + 15) / 16;
  map.cols = (cfg->g_w + 15) / 16;
  map.active_map = NULL;

  if (aom_codec_control(codec, AOME_SET_ACTIVEMAP, &map))
    die_codec(codec, "Failed to set active map");
}

static int encode_frame(aom_codec_ctx_t *codec, aom_image_t *img,
                        int frame_index, AvxVideoWriter *writer) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res = aom_codec_encode(codec, img, frame_index, 1, 0);
  if (res != AOM_CODEC_OK) die_codec(codec, "Failed to encode frame");

  while ((pkt = aom_codec_get_cx_data(codec, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;
      if (!aom_video_writer_write_frame(writer, pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts)) {
        die_codec(codec, "Failed to write compressed frame");
      }

      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }

  return got_pkts;
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  aom_codec_ctx_t codec;
  aom_codec_enc_cfg_t cfg;
  int frame_count = 0;
  const int limit = 10;
  aom_image_t raw;
  aom_codec_err_t res;
  AvxVideoInfo info;
  AvxVideoWriter *writer = NULL;
  const int fps = 2;  // TODO(dkovalev) add command line argument
  const double bits_per_pixel_per_frame = 0.067;

#if CONFIG_REALTIME_ONLY
  const int usage = 1;
  const int speed = 7;
#else
  const int usage = 0;
  const int speed = 2;
#endif

  exec_name = argv[0];
  if (argc != 6) die("Invalid number of arguments");

  memset(&info, 0, sizeof(info));

  aom_codec_iface_t *encoder = get_aom_encoder_by_short_name(argv[1]);
  if (encoder == NULL) {
    die("Unsupported codec.");
  }
  assert(encoder != NULL);
  info.codec_fourcc = get_fourcc_by_aom_encoder(encoder);
  info.frame_width = (int)strtol(argv[2], NULL, 0);
  info.frame_height = (int)strtol(argv[3], NULL, 0);
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  if (info.frame_width <= 0 || info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }

  if (!aom_img_alloc(&raw, AOM_IMG_FMT_I420, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image.");
  }

  printf("Using %s\n", aom_codec_iface_name(encoder));

  res = aom_codec_enc_config_default(encoder, &cfg, usage);
  if (res) die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate =
      (unsigned int)(bits_per_pixel_per_frame * cfg.g_w * cfg.g_h * fps / 1000);
  cfg.g_lag_in_frames = 0;

  writer = aom_video_writer_open(argv[5], kContainerIVF, &info);
  if (!writer) die("Failed to open %s for writing.", argv[5]);

  if (!(infile = fopen(argv[4], "rb")))
    die("Failed to open %s for reading.", argv[4]);

  if (aom_codec_enc_init(&codec, encoder, &cfg, 0))
    die("Failed to initialize encoder");

  if (aom_codec_control(&codec, AOME_SET_CPUUSED, speed))
    die_codec(&codec, "Failed to set cpu-used");

  // Encode frames.
  while (aom_img_read(&raw, infile) && frame_count < limit) {
    ++frame_count;

    if (frame_count == 5) {
      set_active_map(&cfg, &codec);
    } else if (frame_count == 9) {
      unset_active_map(&cfg, &codec);
    }

    encode_frame(&codec, &raw, frame_count, writer);
  }

  // Flush encoder.
  while (encode_frame(&codec, NULL, -1, writer)) {
  }

  printf("\n");
  fclose(infile);
  printf("Processed %d frames.\n", frame_count);

  aom_img_free(&raw);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  aom_video_writer_close(writer);

  return EXIT_SUCCESS;
}
