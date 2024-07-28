/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Scalable Encoder
// ==============
//
// This is an example of a scalable encoder loop. It takes two input files in
// YV12 format, passes it through the encoder, and writes the compressed
// frames to disk in OBU format.
//
// Getting The Default Configuration
// ---------------------------------
// Encoders have the notion of "usage profiles." For example, an encoder
// may want to publish default configurations for both a video
// conferencing application and a best quality offline encoder. These
// obviously have very different default settings. Consult the
// documentation for your codec to see if it provides any default
// configurations. All codecs provide a default configuration, number 0,
// which is valid for material in the vacinity of QCIF/QVGA.
//
// Updating The Configuration
// ---------------------------------
// Almost all applications will want to update the default configuration
// with settings specific to their usage. Here we set the width and height
// of the video file to that specified on the command line. We also scale
// the default bitrate based on the ratio between the default resolution
// and the resolution specified on the command line.
//
// Encoding A Frame
// ----------------
// The frame is read as a continuous block (size = width * height * 3 / 2)
// from the input file. If a frame was read (the input file has not hit
// EOF) then the frame is passed to the encoder. Otherwise, a NULL
// is passed, indicating the End-Of-Stream condition to the encoder. The
// `frame_cnt` is reused as the presentation time stamp (PTS) and each
// frame is shown for one frame-time in duration. The flags parameter is
// unused in this example.

// Forced Keyframes
// ----------------
// Keyframes can be forced by setting the AOM_EFLAG_FORCE_KF bit of the
// flags passed to `aom_codec_control()`. In this example, we force a
// keyframe every <keyframe-interval> frames. Note, the output stream can
// contain additional keyframes beyond those that have been forced using the
// AOM_EFLAG_FORCE_KF flag because of automatic keyframe placement by the
// encoder.
//
// Processing The Encoded Data
// ---------------------------
// Each packet of type `AOM_CODEC_CX_FRAME_PKT` contains the encoded data
// for this frame. We write a IVF frame header, followed by the raw data.
//
// Cleanup
// -------
// The `aom_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exeptions, aom_codec functions return an enumerated error status,
// with the value `0` indicating success.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "av1/common/enums.h"
#include "common/tools_common.h"
#include "common/video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <codec> <width> <height> <infile0> <infile1> "
          "<outfile> <frames to encode>\n"
          "See comments in scalable_encoder.c for more information.\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static int encode_frame(aom_codec_ctx_t *codec, aom_image_t *img,
                        int frame_index, int flags, FILE *outfile) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res =
      aom_codec_encode(codec, img, frame_index, 1, flags);
  if (res != AOM_CODEC_OK) die_codec(codec, "Failed to encode frame");

  while ((pkt = aom_codec_get_cx_data(codec, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;
      if (fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz, outfile) !=
          pkt->data.frame.sz) {
        die_codec(codec, "Failed to write compressed frame");
      }
      printf(keyframe ? "K" : ".");
      printf(" %6d\n", (int)pkt->data.frame.sz);
      fflush(stdout);
    }
  }

  return got_pkts;
}

int main(int argc, char **argv) {
  FILE *infile0 = NULL;
  FILE *infile1 = NULL;
  aom_codec_enc_cfg_t cfg;
  int frame_count = 0;
  aom_image_t raw0, raw1;
  aom_codec_err_t res;
  AvxVideoInfo info;
  const int fps = 30;
  const int bitrate = 200;
  int keyframe_interval = 0;
  int max_frames = 0;
  int frames_encoded = 0;
  const char *codec_arg = NULL;
  const char *width_arg = NULL;
  const char *height_arg = NULL;
  const char *infile0_arg = NULL;
  const char *infile1_arg = NULL;
  const char *outfile_arg = NULL;
  //  const char *keyframe_interval_arg = NULL;
  FILE *outfile = NULL;

  exec_name = argv[0];

  // Clear explicitly, as simply assigning "{ 0 }" generates
  // "missing-field-initializers" warning in some compilers.
  memset(&info, 0, sizeof(info));

  if (argc != 8) die("Invalid number of arguments");

  codec_arg = argv[1];
  width_arg = argv[2];
  height_arg = argv[3];
  infile0_arg = argv[4];
  infile1_arg = argv[5];
  outfile_arg = argv[6];
  max_frames = (int)strtol(argv[7], NULL, 0);

  aom_codec_iface_t *encoder = get_aom_encoder_by_short_name(codec_arg);
  if (!encoder) die("Unsupported codec.");

  info.codec_fourcc = get_fourcc_by_aom_encoder(encoder);
  info.frame_width = (int)strtol(width_arg, NULL, 0);
  info.frame_height = (int)strtol(height_arg, NULL, 0);
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  if (info.frame_width <= 0 || info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }

  if (!aom_img_alloc(&raw0, AOM_IMG_FMT_I420, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image for layer 0.");
  }
  if (!aom_img_alloc(&raw1, AOM_IMG_FMT_I420, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image for layer 1.");
  }

  //  keyframe_interval = (int)strtol(keyframe_interval_arg, NULL, 0);
  keyframe_interval = 100;
  if (keyframe_interval < 0) die("Invalid keyframe interval value.");

  printf("Using %s\n", aom_codec_iface_name(encoder));

  aom_codec_ctx_t codec;
  res = aom_codec_enc_config_default(encoder, &cfg, 0);
  if (res) die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = 0;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_Q;
  cfg.save_as_annexb = 0;

  outfile = fopen(outfile_arg, "wb");
  if (!outfile) die("Failed to open %s for writing.", outfile_arg);

  if (!(infile0 = fopen(infile0_arg, "rb")))
    die("Failed to open %s for reading.", infile0_arg);
  if (!(infile1 = fopen(infile1_arg, "rb")))
    die("Failed to open %s for reading.", infile0_arg);

  if (aom_codec_enc_init(&codec, encoder, &cfg, 0))
    die("Failed to initialize encoder");
  if (aom_codec_control(&codec, AOME_SET_CPUUSED, 8))
    die_codec(&codec, "Failed to set cpu to 8");

  if (aom_codec_control(&codec, AV1E_SET_TILE_COLUMNS, 2))
    die_codec(&codec, "Failed to set tile columns to 2");
  if (aom_codec_control(&codec, AV1E_SET_NUM_TG, 3))
    die_codec(&codec, "Failed to set num of tile groups to 3");

  if (aom_codec_control(&codec, AOME_SET_NUMBER_SPATIAL_LAYERS, 2))
    die_codec(&codec, "Failed to set number of spatial layers to 2");

  // Encode frames.
  while (aom_img_read(&raw0, infile0)) {
    int flags = 0;

    // configure and encode base layer

    if (keyframe_interval > 0 && frames_encoded % keyframe_interval == 0)
      flags |= AOM_EFLAG_FORCE_KF;
    else
      // use previous base layer (LAST) as sole reference
      // save this frame as LAST to be used as reference by enhanmcent layer
      // and next base layer
      flags |= AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
               AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
               AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
               AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF |
               AOM_EFLAG_NO_UPD_ENTROPY;
    cfg.g_w = info.frame_width;
    cfg.g_h = info.frame_height;
    if (aom_codec_enc_config_set(&codec, &cfg))
      die_codec(&codec, "Failed to set enc cfg for layer 0");
    if (aom_codec_control(&codec, AOME_SET_SPATIAL_LAYER_ID, 0))
      die_codec(&codec, "Failed to set layer id to 0");
    if (aom_codec_control(&codec, AOME_SET_CQ_LEVEL, 62))
      die_codec(&codec, "Failed to set cq level");
    encode_frame(&codec, &raw0, frame_count++, flags, outfile);

    // configure and encode enhancement layer

    //  use LAST (base layer) as sole reference
    flags = AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
            AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
            AOM_EFLAG_NO_REF_ARF2 | AOM_EFLAG_NO_UPD_LAST |
            AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF |
            AOM_EFLAG_NO_UPD_ENTROPY;
    cfg.g_w = info.frame_width;
    cfg.g_h = info.frame_height;
    aom_img_read(&raw1, infile1);
    if (aom_codec_enc_config_set(&codec, &cfg))
      die_codec(&codec, "Failed to set enc cfg for layer 1");
    if (aom_codec_control(&codec, AOME_SET_SPATIAL_LAYER_ID, 1))
      die_codec(&codec, "Failed to set layer id to 1");
    if (aom_codec_control(&codec, AOME_SET_CQ_LEVEL, 10))
      die_codec(&codec, "Failed to set cq level");
    encode_frame(&codec, &raw1, frame_count++, flags, outfile);

    frames_encoded++;

    if (max_frames > 0 && frames_encoded >= max_frames) break;
  }

  // Flush encoder.
  while (encode_frame(&codec, NULL, -1, 0, outfile)) continue;

  printf("\n");
  fclose(infile0);
  fclose(infile1);
  printf("Processed %d frames.\n", frame_count / 2);

  aom_img_free(&raw0);
  aom_img_free(&raw1);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  fclose(outfile);

  return EXIT_SUCCESS;
}
