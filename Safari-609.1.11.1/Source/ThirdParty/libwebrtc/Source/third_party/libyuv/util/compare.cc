/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libyuv/basic_types.h"
#include "libyuv/compare.h"
#include "libyuv/version.h"

int main(int argc, char** argv) {
  if (argc < 1) {
    printf("libyuv compare v%d\n", LIBYUV_VERSION);
    printf("compare file1.yuv file2.yuv\n");
    return -1;
  }
  char* name1 = argv[1];
  char* name2 = (argc > 2) ? argv[2] : NULL;
  FILE* fin1 = fopen(name1, "rb");
  FILE* fin2 = name2 ? fopen(name2, "rb") : NULL;

  const int kBlockSize = 32768;
  uint8_t buf1[kBlockSize];
  uint8_t buf2[kBlockSize];
  uint32_t hash1 = 5381;
  uint32_t hash2 = 5381;
  uint64_t sum_square_err = 0;
  uint64_t size_min = 0;
  int amt1 = 0;
  int amt2 = 0;
  do {
    amt1 = static_cast<int>(fread(buf1, 1, kBlockSize, fin1));
    if (amt1 > 0) {
      hash1 = libyuv::HashDjb2(buf1, amt1, hash1);
    }
    if (fin2) {
      amt2 = static_cast<int>(fread(buf2, 1, kBlockSize, fin2));
      if (amt2 > 0) {
        hash2 = libyuv::HashDjb2(buf2, amt2, hash2);
      }
      int amt_min = (amt1 < amt2) ? amt1 : amt2;
      size_min += amt_min;
      sum_square_err += libyuv::ComputeSumSquareError(buf1, buf2, amt_min);
    }
  } while (amt1 > 0 || amt2 > 0);

  printf("hash1 %x", hash1);
  if (fin2) {
    printf(", hash2 %x", hash2);
    double mse =
        static_cast<double>(sum_square_err) / static_cast<double>(size_min);
    printf(", mse %.2f", mse);
    double psnr = libyuv::SumSquareErrorToPsnr(sum_square_err, size_min);
    printf(", psnr %.2f\n", psnr);
    fclose(fin2);
  }
  fclose(fin1);
}
