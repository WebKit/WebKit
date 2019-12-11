/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools_common.h"
#include "../vp9/encoder/vp9_resize.h"

static const char *exec_name = NULL;

static void usage() {
  printf("Usage:\n");
  printf("%s <input_yuv> <width>x<height> <target_width>x<target_height> ",
         exec_name);
  printf("<output_yuv> [<frames>]\n");
}

void usage_exit(void) {
  usage();
  exit(EXIT_FAILURE);
}

static int parse_dim(char *v, int *width, int *height) {
  char *x = strchr(v, 'x');
  if (x == NULL) x = strchr(v, 'X');
  if (x == NULL) return 0;
  *width = atoi(v);
  *height = atoi(&x[1]);
  if (*width <= 0 || *height <= 0)
    return 0;
  else
    return 1;
}

int main(int argc, char *argv[]) {
  char *fin, *fout;
  FILE *fpin, *fpout;
  uint8_t *inbuf, *outbuf;
  uint8_t *inbuf_u, *outbuf_u;
  uint8_t *inbuf_v, *outbuf_v;
  int f, frames;
  int width, height, target_width, target_height;

  exec_name = argv[0];

  if (argc < 5) {
    printf("Incorrect parameters:\n");
    usage();
    return 1;
  }

  fin = argv[1];
  fout = argv[4];
  if (!parse_dim(argv[2], &width, &height)) {
    printf("Incorrect parameters: %s\n", argv[2]);
    usage();
    return 1;
  }
  if (!parse_dim(argv[3], &target_width, &target_height)) {
    printf("Incorrect parameters: %s\n", argv[3]);
    usage();
    return 1;
  }

  fpin = fopen(fin, "rb");
  if (fpin == NULL) {
    printf("Can't open file %s to read\n", fin);
    usage();
    return 1;
  }
  fpout = fopen(fout, "wb");
  if (fpout == NULL) {
    printf("Can't open file %s to write\n", fout);
    usage();
    return 1;
  }
  if (argc >= 6)
    frames = atoi(argv[5]);
  else
    frames = INT_MAX;

  printf("Input size:  %dx%d\n", width, height);
  printf("Target size: %dx%d, Frames: ", target_width, target_height);
  if (frames == INT_MAX)
    printf("All\n");
  else
    printf("%d\n", frames);

  inbuf = (uint8_t *)malloc(width * height * 3 / 2);
  outbuf = (uint8_t *)malloc(target_width * target_height * 3 / 2);
  inbuf_u = inbuf + width * height;
  inbuf_v = inbuf_u + width * height / 4;
  outbuf_u = outbuf + target_width * target_height;
  outbuf_v = outbuf_u + target_width * target_height / 4;
  f = 0;
  while (f < frames) {
    if (fread(inbuf, width * height * 3 / 2, 1, fpin) != 1) break;
    vp9_resize_frame420(inbuf, width, inbuf_u, inbuf_v, width / 2, height,
                        width, outbuf, target_width, outbuf_u, outbuf_v,
                        target_width / 2, target_height, target_width);
    fwrite(outbuf, target_width * target_height * 3 / 2, 1, fpout);
    f++;
  }
  printf("%d frames processed\n", f);
  fclose(fpin);
  fclose(fpout);

  free(inbuf);
  free(outbuf);
  return 0;
}
