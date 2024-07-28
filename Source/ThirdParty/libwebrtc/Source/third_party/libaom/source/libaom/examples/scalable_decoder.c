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

// Scalable Decoder
// ==============
//
// This is an example of a scalable decoder loop. It takes a 2-spatial-layer
// input file
// containing the compressed data (in OBU format), passes it through the
// decoder, and writes the decompressed frames to disk. The base layer and
// enhancement layers are stored as separate files, out_lyr0.yuv and
// out_lyr1.yuv, respectively.
//
// Standard Includes
// -----------------
// For decoders, you only have to include `aom_decoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// av1.
//
// Initializing The Codec
// ----------------------
// The libaom decoder is initialized by the call to aom_codec_dec_init().
// Determining the codec interface to use is handled by AvxVideoReader and the
// functions prefixed with aom_video_reader_. Discussion of those functions is
// beyond the scope of this example, but the main gist is to open the input file
// and parse just enough of it to determine if it's a AVx file and which AVx
// codec is contained within the file.
// Note the NULL pointer passed to aom_codec_dec_init(). We do that in this
// example because we want the algorithm to determine the stream configuration
// (width/height) and allocate memory automatically.
//
// Decoding A Frame
// ----------------
// Once the frame has been read into memory, it is decoded using the
// `aom_codec_decode` function. The call takes a pointer to the data
// (`frame`) and the length of the data (`frame_size`). No application data
// is associated with the frame in this example, so the `user_priv`
// parameter is NULL. The `deadline` parameter is left at zero for this
// example. This parameter is generally only used when doing adaptive post
// processing.
//
// Codecs may produce a variable number of output frames for every call to
// `aom_codec_decode`. These frames are retrieved by the
// `aom_codec_get_frame` iterator function. The iterator variable `iter` is
// initialized to NULL each time `aom_codec_decode` is called.
// `aom_codec_get_frame` is called in a loop, returning a pointer to a
// decoded image or NULL to indicate the end of list.
//
// Processing The Decoded Data
// ---------------------------
// In this example, we simply write the encoded data to disk. It is
// important to honor the image's `stride` values.
//
// Cleanup
// -------
// The `aom_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exceptions, aom_codec functions return an enumerated error status,
// with the value `0` indicating success.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "common/obudec.h"
#include "common/tools_common.h"
#include "common/video_reader.h"

static const char *exec_name;

#define MAX_LAYERS 5

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int frame_cnt = 0;
  FILE *outfile[MAX_LAYERS];
  char filename[80];
  FILE *inputfile = NULL;
  uint8_t *buf = NULL;
  size_t bytes_in_buffer = 0;
  size_t buffer_size = 0;
  struct AvxInputContext aom_input_ctx;
  struct ObuDecInputContext obu_ctx = { &aom_input_ctx, NULL, 0, 0, 0 };
  aom_codec_stream_info_t si;
  uint8_t tmpbuf[32];
  unsigned int i;

  exec_name = argv[0];

  if (argc != 2) die("Invalid number of arguments.");

  if (!(inputfile = fopen(argv[1], "rb")))
    die("Failed to open %s for read.", argv[1]);
  obu_ctx.avx_ctx->file = inputfile;
  obu_ctx.avx_ctx->filename = argv[1];

  aom_codec_iface_t *decoder = get_aom_decoder_by_index(0);
  printf("Using %s\n", aom_codec_iface_name(decoder));

  aom_codec_ctx_t codec;
  if (aom_codec_dec_init(&codec, decoder, NULL, 0))
    die("Failed to initialize decoder.");

  if (aom_codec_control(&codec, AV1D_SET_OUTPUT_ALL_LAYERS, 1)) {
    die_codec(&codec, "Failed to set output_all_layers control.");
  }

  // peak sequence header OBU to get number of spatial layers
  const size_t ret = fread(tmpbuf, 1, 32, inputfile);
  if (ret != 32) die_codec(&codec, "Input is not a valid obu file");
  si.is_annexb = 0;
  if (aom_codec_peek_stream_info(decoder, tmpbuf, 32, &si)) {
    die_codec(&codec, "Input is not a valid obu file");
  }
  fseek(inputfile, -32, SEEK_CUR);

  if (!file_is_obu(&obu_ctx))
    die_codec(&codec, "Input is not a valid obu file");

  // open base layer output yuv file
  snprintf(filename, sizeof(filename), "out_lyr%d.yuv", 0);
  if (!(outfile[0] = fopen(filename, "wb")))
    die("Failed top open output for writing.");

  // open any enhancement layer output yuv files
  for (i = 1; i < si.number_spatial_layers; i++) {
    snprintf(filename, sizeof(filename), "out_lyr%u.yuv", i);
    if (!(outfile[i] = fopen(filename, "wb")))
      die("Failed to open output for writing.");
  }

  while (!obudec_read_temporal_unit(&obu_ctx, &buf, &bytes_in_buffer,
                                    &buffer_size)) {
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    if (aom_codec_decode(&codec, buf, bytes_in_buffer, NULL))
      die_codec(&codec, "Failed to decode frame.");

    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
      aom_image_t *img_shifted =
          aom_img_alloc(NULL, AOM_IMG_FMT_I420, img->d_w, img->d_h, 16);
      img_shifted->bit_depth = 8;
      aom_img_downshift(img_shifted, img,
                        img->bit_depth - img_shifted->bit_depth);
      if (img->spatial_id == 0) {
        printf("Writing        base layer 0 %d\n", frame_cnt);
        aom_img_write(img_shifted, outfile[0]);
      } else if (img->spatial_id <= (int)(si.number_spatial_layers - 1)) {
        printf("Writing enhancement layer %d %d\n", img->spatial_id, frame_cnt);
        aom_img_write(img_shifted, outfile[img->spatial_id]);
      } else {
        die_codec(&codec, "Invalid bitstream. Layer id exceeds layer count");
      }
      if (img->spatial_id == (int)(si.number_spatial_layers - 1)) ++frame_cnt;
    }
  }

  printf("Processed %d frames.\n", frame_cnt);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");

  for (i = 0; i < si.number_spatial_layers; i++) fclose(outfile[i]);

  fclose(inputfile);

  return EXIT_SUCCESS;
}
