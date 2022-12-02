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

// Simple Decoder
// ==============
//
// This is an example of a simple decoder loop. It takes an input file
// containing the compressed data (in IVF format), passes it through the
// decoder, and writes the decompressed frames to disk. Other decoder
// examples build upon this one.
//
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
//
// Standard Includes
// -----------------
// For decoders, you only have to include `aom_decoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// aom.
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
// parameter is NULL.
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
#include "common/tools_common.h"
#include "common/video_reader.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile> <outfile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int frame_cnt = 0;
  FILE *outfile = NULL;
  AvxVideoReader *reader = NULL;
  const AvxVideoInfo *info = NULL;

  exec_name = argv[0];

  if (argc != 3) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  info = aom_video_reader_get_info(reader);

  aom_codec_iface_t *decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");

  printf("Using %s\n", aom_codec_iface_name(decoder));

  aom_codec_ctx_t codec;
  if (aom_codec_dec_init(&codec, decoder, NULL, 0))
    die("Failed to initialize decoder.");

  while (aom_video_reader_read_frame(reader)) {
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");

    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
      aom_img_write(img, outfile);
      ++frame_cnt;
    }
  }

  printf("Processed %d frames.\n", frame_cnt);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");

  printf("Play: ffplay -f rawvideo -pix_fmt yuv420p -s %dx%d %s\n",
         info->frame_width, info->frame_height, argv[2]);

  aom_video_reader_close(reader);

  fclose(outfile);

  return EXIT_SUCCESS;
}
