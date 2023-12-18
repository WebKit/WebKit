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

// Lightfield Tile List Decoder
// ============================
//
// This is a lightfield tile list decoder example. It takes an input file that
// contains the anchor frames that are references of the coded tiles, the camera
// frame header, and tile list OBUs that include the tile information and the
// compressed tile data. This input file is reconstructed from the encoded
// lightfield ivf file, and is decodable by AV1 decoder. num_references is
// the number of anchor frames coded at the beginning of the light field file.
// num_tile_lists is the number of tile lists need to be decoded. There is an
// optional parameter allowing to choose the output format, and the supported
// formats are YUV1D(default), YUV, and NV12.
// Run lightfield tile list decoder to decode an AV1 tile list file:
// examples/lightfield_tile_list_decoder vase_tile_list.ivf vase_tile_list.yuv
// 4 2 0(optional)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "aom_scale/yv12config.h"
#include "av1/common/enums.h"
#include "common/tools_common.h"
#include "common/video_reader.h"

enum {
  YUV1D,  // 1D tile output for conformance test.
  YUV,    // Tile output in YUV format.
  NV12,   // Tile output in NV12 format.
} UENUM1BYTE(OUTPUT_FORMAT);

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <infile> <outfile> <num_references> <num_tile_lists> "
          "<output format(optional)>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static void write_tile_yuv1d(aom_codec_ctx_t *codec, const aom_image_t *img,
                             FILE *file) {
  // read out the tile size.
  unsigned int tile_size = 0;
  if (AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1D_GET_TILE_SIZE, &tile_size))
    die_codec(codec, "Failed to get the tile size");
  const unsigned int tile_width = tile_size >> 16;
  const unsigned int tile_height = tile_size & 65535;
  const uint32_t output_frame_width_in_tiles = img->d_w / tile_width;

  unsigned int tile_count = 0;
  if (AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1D_GET_TILE_COUNT, &tile_count))
    die_codec(codec, "Failed to get the tile size");

  // Write tile to file.
  const int shift = (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 1 : 0;
  unsigned int tile_idx;

  for (tile_idx = 0; tile_idx < tile_count; ++tile_idx) {
    const int row_offset =
        (tile_idx / output_frame_width_in_tiles) * tile_height;
    const int col_offset =
        (tile_idx % output_frame_width_in_tiles) * tile_width;
    int plane;

    for (plane = 0; plane < 3; ++plane) {
      const unsigned char *buf = img->planes[plane];
      const int stride = img->stride[plane];
      const int roffset =
          (plane > 0) ? row_offset >> img->y_chroma_shift : row_offset;
      const int coffset =
          (plane > 0) ? col_offset >> img->x_chroma_shift : col_offset;
      const int w = (plane > 0) ? ((tile_width >> img->x_chroma_shift) << shift)
                                : (tile_width << shift);
      const int h =
          (plane > 0) ? (tile_height >> img->y_chroma_shift) : tile_height;
      int y;

      // col offset needs to be adjusted for HBD.
      buf += roffset * stride + (coffset << shift);

      for (y = 0; y < h; ++y) {
        fwrite(buf, 1, w, file);
        buf += stride;
      }
    }
  }
}

int main(int argc, char **argv) {
  FILE *outfile = NULL;
  AvxVideoReader *reader = NULL;
  const AvxVideoInfo *info = NULL;
  int num_references;
  int num_tile_lists;
  aom_image_t reference_images[MAX_EXTERNAL_REFERENCES];
  size_t frame_size = 0;
  const unsigned char *frame = NULL;
  int output_format = YUV1D;
  int i, j, n;

  exec_name = argv[0];

  if (argc < 5) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  num_references = (int)strtol(argv[3], NULL, 0);
  num_tile_lists = (int)strtol(argv[4], NULL, 0);

  if (argc > 5) output_format = (int)strtol(argv[5], NULL, 0);
  if (output_format < YUV1D || output_format > NV12)
    die("Output format out of range [0, 2]");

  info = aom_video_reader_get_info(reader);

  aom_codec_iface_t *decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  printf("Using %s\n", aom_codec_iface_name(decoder));

  aom_codec_ctx_t codec;
  if (aom_codec_dec_init(&codec, decoder, NULL, 0))
    die("Failed to initialize decoder.");

  if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_SET_IS_ANNEXB,
                                    info->is_annexb)) {
    die_codec(&codec, "Failed to set annex b status");
  }

  // Decode anchor frames.
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_TILE_MODE, 0);
  for (i = 0; i < num_references; ++i) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");

    if (i == 0) {
      aom_img_fmt_t ref_fmt = 0;
      if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_GET_IMG_FORMAT, &ref_fmt))
        die_codec(&codec, "Failed to get the image format");

      int frame_res[2];
      if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_GET_FRAME_SIZE, frame_res))
        die_codec(&codec, "Failed to get the image frame size");

      // Allocate memory to store decoded references. Allocate memory with the
      // border so that it can be used as a reference.
      for (j = 0; j < num_references; j++) {
        unsigned int border = AOM_DEC_BORDER_IN_PIXELS;
        if (!aom_img_alloc_with_border(&reference_images[j], ref_fmt,
                                       frame_res[0], frame_res[1], 32, 8,
                                       border)) {
          fatal("Failed to allocate references.");
        }
      }
    }

    if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_COPY_NEW_FRAME_IMAGE,
                                      &reference_images[i]))
      die_codec(&codec, "Failed to copy decoded reference frame");

    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
      char name[1024];
      snprintf(name, sizeof(name), "ref_%d.yuv", i);
      printf("writing ref image to %s, %u, %u\n", name, img->d_w, img->d_h);
      FILE *ref_file = fopen(name, "wb");
      aom_img_write(img, ref_file);
      fclose(ref_file);
    }
  }

  // Decode the lightfield.
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_TILE_MODE, 1);

  // Set external references.
  av1_ext_ref_frame_t set_ext_ref = { &reference_images[0], num_references };
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_SET_EXT_REF_PTR, &set_ext_ref);
  // Must decode the camera frame header first.
  aom_video_reader_read_frame(reader);
  frame = aom_video_reader_get_frame(reader, &frame_size);
  if (aom_codec_decode(&codec, frame, frame_size, NULL))
    die_codec(&codec, "Failed to decode the frame.");
  // Decode tile lists one by one.
  for (n = 0; n < num_tile_lists; n++) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);

    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode the tile list.");
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = aom_codec_get_frame(&codec, &iter);
    if (!img) die_codec(&codec, "Failed to get frame.");

    if (output_format == YUV1D)
      // write the tile to the output file in 1D format.
      write_tile_yuv1d(&codec, img, outfile);
    else if (output_format == YUV)
      aom_img_write(img, outfile);
    else
      // NV12 output format
      aom_img_write_nv12(img, outfile);
  }

  for (i = 0; i < num_references; i++) aom_img_free(&reference_images[i]);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
  fclose(outfile);

  return EXIT_SUCCESS;
}
