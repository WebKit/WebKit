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

// Lightfield Bitstream Parsing
// ============================
//
// This is a lightfield bitstream parsing example. It takes an input file
// containing the whole compressed lightfield bitstream(ivf file) and a text
// file containing a stream of tiles to decode and then constructs and outputs
// a new bitstream that can be decoded by an AV1 decoder. The output bitstream
// contains reference frames(i.e. anchor frames), camera frame header, and
// tile list OBUs. num_references is the number of anchor frames coded at the
// beginning of the light field file.  After running the lightfield encoder,
// run lightfield bitstream parsing:
// examples/lightfield_bitstream_parsing vase10x10.ivf vase_tile_list.ivf 4
//   tile_list.txt
//
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
#include "aom/aom_encoder.h"
#include "aom/aom_integer.h"
#include "aom/aomdx.h"
#include "aom_dsp/bitwriter_buffer.h"
#include "common/tools_common.h"
#include "common/video_reader.h"
#include "common/video_writer.h"

#define MAX_TILES 512

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile> <outfile> <num_references> <tile_list>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

const int output_frame_width = 512;
const int output_frame_height = 512;

// Spec:
// typedef struct {
//   uint8_t anchor_frame_idx;
//   uint8_t tile_row;
//   uint8_t tile_col;
//   uint16_t coded_tile_data_size_minus_1;
//   uint8_t *coded_tile_data;
// } TILE_LIST_ENTRY;

// Tile list entry provided by the application
typedef struct {
  int image_idx;
  int reference_idx;
  int tile_col;
  int tile_row;
} TILE_LIST_INFO;

static int get_image_bps(aom_img_fmt_t fmt) {
  switch (fmt) {
    case AOM_IMG_FMT_I420: return 12;
    case AOM_IMG_FMT_I422: return 16;
    case AOM_IMG_FMT_I444: return 24;
    case AOM_IMG_FMT_I42016: return 24;
    case AOM_IMG_FMT_I42216: return 32;
    case AOM_IMG_FMT_I44416: return 48;
    default: die("Invalid image format");
  }
}

static void process_tile_list(const TILE_LIST_INFO *tiles, int num_tiles,
                              aom_codec_pts_t tl_pts, unsigned char **frames,
                              const size_t *frame_sizes, aom_codec_ctx_t *codec,
                              unsigned char *tl_buf, AvxVideoWriter *writer,
                              uint8_t output_frame_width_in_tiles_minus_1,
                              uint8_t output_frame_height_in_tiles_minus_1) {
  unsigned char *tl = tl_buf;
  struct aom_write_bit_buffer wb = { tl, 0 };
  unsigned char *saved_obu_size_loc = NULL;
  uint32_t tile_list_obu_header_size = 0;
  uint32_t tile_list_obu_size = 0;
  int num_tiles_minus_1 = num_tiles - 1;
  int i;

  // Write the tile list OBU header that is 1 byte long.
  aom_wb_write_literal(&wb, 0, 1);  // forbidden bit.
  aom_wb_write_literal(&wb, 8, 4);  // tile list OBU: "1000"
  aom_wb_write_literal(&wb, 0, 1);  // obu_extension = 0
  aom_wb_write_literal(&wb, 1, 1);  // obu_has_size_field
  aom_wb_write_literal(&wb, 0, 1);  // reserved
  tl++;
  tile_list_obu_header_size++;

  // Write the OBU size using a fixed length_field_size of 4 bytes.
  saved_obu_size_loc = tl;
  // aom_wb_write_unsigned_literal(&wb, data, bits) requires that bits <= 32.
  aom_wb_write_unsigned_literal(&wb, 0, 32);
  tl += 4;
  tile_list_obu_header_size += 4;

  // write_tile_list_obu()
  aom_wb_write_literal(&wb, output_frame_width_in_tiles_minus_1, 8);
  aom_wb_write_literal(&wb, output_frame_height_in_tiles_minus_1, 8);
  aom_wb_write_literal(&wb, num_tiles_minus_1, 16);
  tl += 4;
  tile_list_obu_size += 4;

  // Write each tile's data
  for (i = 0; i <= num_tiles_minus_1; i++) {
    aom_tile_data tile_data = { 0, NULL, 0 };

    int image_idx = tiles[i].image_idx;
    int ref_idx = tiles[i].reference_idx;
    int tc = tiles[i].tile_col;
    int tr = tiles[i].tile_row;

    // Reset bit writer to the right location.
    wb.bit_buffer = tl;
    wb.bit_offset = 0;

    size_t frame_size = frame_sizes[image_idx];
    const unsigned char *frame = frames[image_idx];

    AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1_SET_DECODE_TILE_ROW, tr);
    AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1_SET_DECODE_TILE_COL, tc);

    aom_codec_err_t aom_status =
        aom_codec_decode(codec, frame, frame_size, NULL);
    if (aom_status) die_codec(codec, "Failed to decode tile.");

    AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1D_GET_TILE_DATA, &tile_data);

    // Copy over tile info.
    //  uint8_t anchor_frame_idx;
    //  uint8_t tile_row;
    //  uint8_t tile_col;
    //  uint16_t coded_tile_data_size_minus_1;
    //  uint8_t *coded_tile_data;
    uint32_t tile_info_bytes = 5;
    aom_wb_write_literal(&wb, ref_idx, 8);
    aom_wb_write_literal(&wb, tr, 8);
    aom_wb_write_literal(&wb, tc, 8);
    aom_wb_write_literal(&wb, (int)tile_data.coded_tile_data_size - 1, 16);
    tl += tile_info_bytes;

    memcpy(tl, (uint8_t *)tile_data.coded_tile_data,
           tile_data.coded_tile_data_size);
    tl += tile_data.coded_tile_data_size;

    tile_list_obu_size +=
        tile_info_bytes + (uint32_t)tile_data.coded_tile_data_size;
  }

  // Write tile list OBU size.
  size_t bytes_written = 0;
  if (aom_uleb_encode_fixed_size(tile_list_obu_size, 4, 4, saved_obu_size_loc,
                                 &bytes_written))
    die_codec(codec, "Failed to encode the tile list obu size.");

  // Copy the tile list.
  if (!aom_video_writer_write_frame(
          writer, tl_buf, tile_list_obu_header_size + tile_list_obu_size,
          tl_pts))
    die_codec(codec, "Failed to copy compressed tile list.");
}

int main(int argc, char **argv) {
  AvxVideoReader *reader = NULL;
  AvxVideoWriter *writer = NULL;
  const AvxVideoInfo *info = NULL;
  int num_references;
  int i;
  aom_codec_pts_t pts;
  const char *tile_list_file = NULL;

  exec_name = argv[0];
  if (argc != 5) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  num_references = (int)strtol(argv[3], NULL, 0);
  info = aom_video_reader_get_info(reader);

  aom_video_reader_set_fourcc(reader, AV1_FOURCC);

  // The writer to write out ivf file in tile list OBU, which can be decoded by
  // AV1 decoder.
  writer = aom_video_writer_open(argv[2], kContainerIVF, info);
  if (!writer) die("Failed to open %s for writing", argv[2]);

  tile_list_file = argv[4];

  aom_codec_iface_t *decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  printf("Using %s\n", aom_codec_iface_name(decoder));

  aom_codec_ctx_t codec;
  if (aom_codec_dec_init(&codec, decoder, NULL, 0))
    die("Failed to initialize decoder.");

  // Decode anchor frames.
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_TILE_MODE, 0);

  printf("Reading %d reference images.\n", num_references);
  for (i = 0; i < num_references; ++i) {
    aom_video_reader_read_frame(reader);

    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    pts = (aom_codec_pts_t)aom_video_reader_get_frame_pts(reader);

    // Copy references bitstream directly.
    if (!aom_video_writer_write_frame(writer, frame, frame_size, pts))
      die_codec(&codec, "Failed to copy compressed anchor frame.");

    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");
  }

  // Decode camera frames.
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_TILE_MODE, 1);
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_EXT_TILE_DEBUG, 1);

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
    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    frames[f] = (unsigned char *)malloc(frame_size * sizeof(unsigned char));
    if (!frames[f]) die("Failed to allocate frame data.");
    memcpy(frames[f], frame, frame_size);
    frame_sizes[f] = frame_size;
  }
  printf("Read %d frames.\n", num_frames);

  // Copy first camera frame for getting camera frame header. This is done
  // only once.
  {
    size_t frame_size = frame_sizes[0];
    const unsigned char *frame = frames[0];
    pts = num_references;
    aom_tile_data frame_header_info = { 0, NULL, 0 };

    // Need to decode frame header to get camera frame header info. So, here
    // decoding 1 tile is enough.
    AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_DECODE_TILE_ROW, 0);
    AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_DECODE_TILE_COL, 0);

    aom_codec_err_t aom_status =
        aom_codec_decode(&codec, frame, frame_size, NULL);
    if (aom_status) die_codec(&codec, "Failed to decode tile.");

    AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_GET_FRAME_HEADER_INFO,
                                  &frame_header_info);

    size_t obu_size_offset =
        (uint8_t *)frame_header_info.coded_tile_data - frame;
    size_t length_field_size = frame_header_info.coded_tile_data_size;
    // Remove ext-tile tile info.
    uint32_t frame_header_size = (uint32_t)frame_header_info.extra_size - 1;
    size_t bytes_to_copy =
        obu_size_offset + length_field_size + frame_header_size;

    unsigned char *frame_hdr_buf = (unsigned char *)malloc(bytes_to_copy);
    if (frame_hdr_buf == NULL)
      die_codec(&codec, "Failed to allocate frame header buffer.");

    memcpy(frame_hdr_buf, frame, bytes_to_copy);

    // Update frame header OBU size.
    size_t bytes_written = 0;
    if (aom_uleb_encode_fixed_size(
            frame_header_size, length_field_size, length_field_size,
            frame_hdr_buf + obu_size_offset, &bytes_written))
      die_codec(&codec, "Failed to encode the tile list obu size.");

    // Copy camera frame header bitstream.
    if (!aom_video_writer_write_frame(writer, frame_hdr_buf, bytes_to_copy,
                                      pts))
      die_codec(&codec, "Failed to copy compressed camera frame header.");
    free(frame_hdr_buf);
  }

  // Read out the image format.
  aom_img_fmt_t ref_fmt = 0;
  if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_GET_IMG_FORMAT, &ref_fmt))
    die_codec(&codec, "Failed to get the image format");
  const int bps = get_image_bps(ref_fmt);
  if (!bps) die_codec(&codec, "Invalid image format.");
  // read out the tile size.
  unsigned int tile_size = 0;
  if (AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_GET_TILE_SIZE, &tile_size))
    die_codec(&codec, "Failed to get the tile size");
  const unsigned int tile_width = tile_size >> 16;
  const unsigned int tile_height = tile_size & 65535;
  // Allocate a buffer to store tile list bitstream.
  const size_t data_sz = MAX_TILES * ALIGN_POWER_OF_TWO(tile_width, 5) *
                         ALIGN_POWER_OF_TWO(tile_height, 5) * bps / 8;

  unsigned char *tl_buf = (unsigned char *)malloc(data_sz);
  if (tl_buf == NULL) die_codec(&codec, "Failed to allocate tile list buffer.");

  aom_codec_pts_t tl_pts = num_references;
  const uint8_t output_frame_width_in_tiles_minus_1 =
      output_frame_width / tile_width - 1;
  const uint8_t output_frame_height_in_tiles_minus_1 =
      output_frame_height / tile_height - 1;

  printf("Reading tile list from file.\n");
  char line[1024];
  FILE *tile_list_fptr = fopen(tile_list_file, "r");
  if (!tile_list_fptr) die_codec(&codec, "Failed to open tile list file.");
  int num_tiles = 0;
  TILE_LIST_INFO tiles[MAX_TILES];
  while ((fgets(line, 1024, tile_list_fptr)) != NULL) {
    if (line[0] == 'F' || num_tiles >= MAX_TILES) {
      // Flush existing tile list and start another, either because we hit a
      // new render frame or because we've hit our max number of tiles per list.
      if (num_tiles > 0) {
        process_tile_list(tiles, num_tiles, tl_pts, frames, frame_sizes, &codec,
                          tl_buf, writer, output_frame_width_in_tiles_minus_1,
                          output_frame_height_in_tiles_minus_1);
        ++tl_pts;
      }
      num_tiles = 0;
    }
    if (line[0] == 'F') {
      continue;
    }
    if (sscanf(line, "%d %d %d %d", &tiles[num_tiles].image_idx,
               &tiles[num_tiles].reference_idx, &tiles[num_tiles].tile_col,
               &tiles[num_tiles].tile_row) == 4) {
      if (tiles[num_tiles].image_idx >= num_frames) {
        die("Tile list image_idx out of bounds: %d >= %d.",
            tiles[num_tiles].image_idx, num_frames);
      }
      if (tiles[num_tiles].reference_idx >= num_references) {
        die("Tile list reference_idx out of bounds: %d >= %d.",
            tiles[num_tiles].reference_idx, num_references);
      }
      ++num_tiles;
    }
  }
  if (num_tiles > 0) {
    // Flush out the last tile list.
    process_tile_list(tiles, num_tiles, tl_pts, frames, frame_sizes, &codec,
                      tl_buf, writer, output_frame_width_in_tiles_minus_1,
                      output_frame_height_in_tiles_minus_1);
    ++tl_pts;
  }

  const int num_tile_lists = (int)(tl_pts - pts);
  printf("Finished processing tile lists.  Num tile lists: %d.\n",
         num_tile_lists);
  free(tl_buf);
  for (int f = 0; f < num_frames; ++f) {
    free(frames[f]);
  }
  free(frame_sizes);
  free(frames);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_writer_close(writer);
  aom_video_reader_close(reader);

  return EXIT_SUCCESS;
}
