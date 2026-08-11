/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Frame-by-frame MD5 Checksum
// ===========================
//
// This example builds upon the simple decoder loop to show how checksums
// of the decoded output can be generated. These are used for validating
// decoder implementations against the reference implementation, for example.
//
// MD5 algorithm
// -------------
// The Message-Digest 5 (MD5) is a well known hash function. We have provided
// an implementation derived from the RSA Data Security, Inc. MD5 Message-Digest
// Algorithm for your use. Our implmentation only changes the interface of this
// reference code. You must include the `md5_utils.h` header for access to these
// functions.
//
// Processing The Decoded Data
// ---------------------------
// Each row of the image is passed to the MD5 accumulator. First the Y plane
// is processed, then U, then V. It is important to honor the image's `stride`
// values.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "common/md5_utils.h"
#include "common/tools_common.h"
#include "common/video_reader.h"

static void get_image_md5(const aom_image_t *img, unsigned char digest[16]) {
  int plane, y;
  MD5Context md5;

  MD5Init(&md5);

  for (plane = 0; plane < 3; ++plane) {
    const unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = plane ? (img->d_w + 1) >> 1 : img->d_w;
    const int h = plane ? (img->d_h + 1) >> 1 : img->d_h;

    for (y = 0; y < h; ++y) {
      MD5Update(&md5, buf, w);
      buf += stride;
    }
  }

  MD5Final(digest, &md5);
}

static void print_md5(FILE *stream, unsigned char digest[16]) {
  int i;

  for (i = 0; i < 16; ++i) fprintf(stream, "%02x", digest[i]);
}

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
    die("Failed to initialize decoder");

  while (aom_video_reader_read_frame(reader)) {
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame");

    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
      unsigned char digest[16];

      get_image_md5(img, digest);
      print_md5(outfile, digest);
      fprintf(outfile, "  img-%ux%u-%04d.i420\n", img->d_w, img->d_h,
              ++frame_cnt);
    }
  }

  printf("Processed %d frames.\n", frame_cnt);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  aom_video_reader_close(reader);

  fclose(outfile);
  return EXIT_SUCCESS;
}
