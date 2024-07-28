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

// Lightfield Decoder
// ==================
//
// This is an example of a simple lightfield decoder. It builds upon the
// simple_decoder.c example.  It takes an input file containing the compressed
// data (in ivf format), treating it as a lightfield instead of a video; and a
// text file with a list of tiles to decode. There is an optional parameter
// allowing to choose the output format, and the supported formats are
// YUV1D(default), YUV, and NV12.
// After running the lightfield encoder, run lightfield decoder to decode a
// batch of tiles:
// examples/lightfield_decoder vase10x10.ivf vase_reference.yuv 4 tile_list.txt
// 0(optional)
// The tile_list.txt is expected to be of the form:
// Frame <frame_index0>
// <image_index0> <anchor_index0> <tile_col0> <tile_row0>
// <image_index1> <anchor_index1> <tile_col1> <tile_row1>
// ...
// Frame <frame_index1)
// ...
//
// The "Frame" markers indicate a new render frame and thus a new tile list
// will be started and the old one flushed.  The image_indexN, anchor_indexN,
// tile_colN, and tile_rowN identify an individual tile to be decoded and
// to use anchor_indexN anchor image for MCP.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
          "Usage: %s <infile> <outfile> <num_references> <tile_list> <output "
          "format(optional)>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

// Output frame size
static const int output_frame_width = 512;
static const int output_frame_height = 512;

static void aom_img_copy_tile(const aom_image_t *src, const aom_image_t *dst,
                              int dst_row_offset, int dst_col_offset) {
  const int shift = (src->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 1 : 0;
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    const unsigned char *src_buf = src->planes[plane];
    const int src_stride = src->stride[plane];
    unsigned char *dst_buf = dst->planes[plane];
    const int dst_stride = dst->stride[plane];
    const int roffset =
        (plane > 0) ? dst_row_offset >> dst->y_chroma_shift : dst_row_offset;
    const int coffset =
        (plane > 0) ? dst_col_offset >> dst->x_chroma_shift : dst_col_offset;

    // col offset needs to be adjusted for HBD.
    dst_buf += roffset * dst_stride + (coffset << shift);

    const int w = (aom_img_plane_width(src, plane) << shift);
    const int h = aom_img_plane_height(src, plane);
    int y;

    for (y = 0; y < h; ++y) {
      memcpy(dst_buf, src_buf, w);
      src_buf += src_stride;
      dst_buf += dst_stride;
    }
  }
}

static void decode_tile(aom_codec_ctx_t *codec, const unsigned char *frame,
                        size_t frame_size, int tr, int tc, int ref_idx,
                        aom_image_t *reference_images, aom_image_t *output,
                        int *tile_idx, unsigned int *output_bit_depth,
                        aom_image_t **img_ptr, int output_format) {
  AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1_SET_TILE_MODE, 1);
  AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1D_EXT_TILE_DEBUG, 1);
  AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1_SET_DECODE_TILE_ROW, tr);
  AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1_SET_DECODE_TILE_COL, tc);

  av1_ref_frame_t ref;
  ref.idx = 0;
  ref.use_external_ref = 1;
  ref.img = reference_images[ref_idx];
  if (AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1_SET_REFERENCE, &ref)) {
    die_codec(codec, "Failed to set reference frame.");
  }

  aom_codec_err_t aom_status = aom_codec_decode(codec, frame, frame_size, NULL);
  if (aom_status) die_codec(codec, "Failed to decode tile.");

  aom_codec_iter_t iter = NULL;
  aom_image_t *img = aom_codec_get_frame(codec, &iter);
  if (!img) die_codec(codec, "Failed to get frame.");
  *img_ptr = img;

  // aom_img_alloc() sets bit_depth as follows:
  // output->bit_depth = (fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 16 : 8;
  // Use img->bit_depth(read from bitstream), so that aom_shift_img()
  // works as expected.
  output->bit_depth = img->bit_depth;
  *output_bit_depth = img->bit_depth;

  if (output_format != YUV1D) {
    // read out the tile size.
    unsigned int tile_size = 0;
    if (AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1D_GET_TILE_SIZE, &tile_size))
      die_codec(codec, "Failed to get the tile size");
    const unsigned int tile_width = tile_size >> 16;
    const unsigned int tile_height = tile_size & 65535;
    const uint32_t output_frame_width_in_tiles =
        output_frame_width / tile_width;

    // Copy the tile to the output frame.
    const int row_offset =
        (*tile_idx / output_frame_width_in_tiles) * tile_height;
    const int col_offset =
        (*tile_idx % output_frame_width_in_tiles) * tile_width;

    aom_img_copy_tile(img, output, row_offset, col_offset);
    (*tile_idx)++;
  }
}

static void img_write_to_file(const aom_image_t *img, FILE *file,
                              int output_format) {
  if (output_format == YUV)
    aom_img_write(img, file);
  else if (output_format == NV12)
    aom_img_write_nv12(img, file);
  else
    die("Invalid output format");
}

int main(int argc, char **argv) {
  FILE *outfile = NULL;
  AvxVideoReader *reader = NULL;
  const AvxVideoInfo *info = NULL;
  int num_references;
  aom_img_fmt_t ref_fmt = 0;
  aom_image_t reference_images[MAX_EXTERNAL_REFERENCES];
  aom_image_t output;
  aom_image_t *output_shifted = NULL;
  size_t frame_size = 0;
  const unsigned char *frame = NULL;
  int i, j;
  const char *tile_list_file = NULL;
  int output_format = YUV1D;
  exec_name = argv[0];

  if (argc < 5) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  num_references = (int)strtol(argv[3], NULL, 0);
  tile_list_file = argv[4];

  if (argc > 5) output_format = (int)strtol(argv[5], NULL, 0);
  if (output_format < YUV1D || output_format > NV12)
    die("Output format out of range [0, 2]");

  info = aom_video_reader_get_info(reader);

  aom_codec_iface_t *decoder;
  if (info->codec_fourcc == LST_FOURCC)
    decoder = get_aom_decoder_by_fourcc(AV1_FOURCC);
  else
    die("Unknown input codec.");
  printf("Using %s\n", aom_codec_iface_name(decoder));

  aom_codec_ctx_t codec;
  if (aom_codec_dec_init(&codec, decoder, NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_SET_IS_ANNEXB,
                                    info->is_annexb)) {
    die("Failed to set annex b status");
  }

  // Decode anchor frames.
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_TILE_MODE, 0);
  for (i = 0; i < num_references; ++i) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");

    if (i == 0) {
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
          die("Failed to allocate references.");
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

  FILE *infile = aom_video_reader_get_file(reader);
  // Record the offset of the first camera image.
  const FileOffset camera_frame_pos = ftello(infile);

  printf("Loading compressed frames into memory.\n");

  // Count the frames in the lightfield.
  int num_frames = 0;
  while (aom_video_reader_read_frame(reader)) {
    ++num_frames;
  }
  if (num_frames < 1) die("Input light field has no frames.");

  // Read all of the lightfield frames into memory.
  unsigned char **frames =
      (unsigned char **)malloc(num_frames * sizeof(unsigned char *));
  size_t *frame_sizes = (size_t *)malloc(num_frames * sizeof(size_t));
  if (!(frames && frame_sizes)) die("Failed to allocate frame data.");
  // Seek to the first camera image.
  fseeko(infile, camera_frame_pos, SEEK_SET);
  for (int f = 0; f < num_frames; ++f) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);
    frames[f] = (unsigned char *)malloc(frame_size * sizeof(unsigned char));
    if (!frames[f]) die("Failed to allocate frame data.");
    memcpy(frames[f], frame, frame_size);
    frame_sizes[f] = frame_size;
  }
  printf("Read %d frames.\n", num_frames);

  if (output_format != YUV1D) {
    // Allocate the output frame.
    aom_img_fmt_t out_fmt = ref_fmt;
    if (FORCE_HIGHBITDEPTH_DECODING) out_fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
    if (!aom_img_alloc(&output, out_fmt, output_frame_width,
                       output_frame_height, 32))
      die("Failed to allocate output image.");
  }

  printf("Decoding tile list from file.\n");
  char line[1024];
  FILE *tile_list_fptr = fopen(tile_list_file, "r");
  if (!tile_list_fptr) die_codec(&codec, "Failed to open tile list file.");
  int tile_list_cnt = 0;
  int tile_list_writes = 0;
  int tile_idx = 0;
  aom_image_t *out = NULL;
  unsigned int output_bit_depth = 0;

  while ((fgets(line, 1024, tile_list_fptr)) != NULL) {
    if (line[0] == 'F') {
      if (output_format != YUV1D) {
        // Write out the tile list.
        if (tile_list_cnt) {
          out = &output;
          if (output_bit_depth != 0) {
            if (!aom_shift_img(output_bit_depth, &out, &output_shifted)) {
              die("Error allocating image");
            }
          }
          img_write_to_file(out, outfile, output_format);
          tile_list_writes++;
        }

        tile_list_cnt++;
        tile_idx = 0;
        // Then memset the frame.
        memset(output.img_data, 0, output.sz);
      }
      continue;
    }

    int image_idx, ref_idx, tc, tr;
    sscanf(line, "%d %d %d %d", &image_idx, &ref_idx, &tc, &tr);
    if (image_idx >= num_frames) {
      die("Tile list image_idx out of bounds: %d >= %d.", image_idx,
          num_frames);
    }
    if (ref_idx >= num_references) {
      die("Tile list ref_idx out of bounds: %d >= %d.", ref_idx,
          num_references);
    }
    frame = frames[image_idx];
    frame_size = frame_sizes[image_idx];

    aom_image_t *img = NULL;
    decode_tile(&codec, frame, frame_size, tr, tc, ref_idx, reference_images,
                &output, &tile_idx, &output_bit_depth, &img, output_format);
    if (output_format == YUV1D) {
      out = img;
      if (output_bit_depth != 0) {
        if (!aom_shift_img(output_bit_depth, &out, &output_shifted)) {
          die("Error allocating image");
        }
      }
      aom_img_write(out, outfile);
    }
  }

  if (output_format != YUV1D) {
    // Write out the last tile list.
    if (tile_list_writes < tile_list_cnt) {
      out = &output;
      if (output_bit_depth != 0) {
        if (!aom_shift_img(output_bit_depth, &out, &output_shifted)) {
          die("Error allocating image");
        }
      }
      img_write_to_file(out, outfile, output_format);
    }
  }

  if (output_shifted) aom_img_free(output_shifted);
  if (output_format != YUV1D) aom_img_free(&output);
  for (i = 0; i < num_references; i++) aom_img_free(&reference_images[i]);
  for (int f = 0; f < num_frames; ++f) {
    free(frames[f]);
  }
  free(frame_sizes);
  free(frames);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
  fclose(outfile);

  return EXIT_SUCCESS;
}
