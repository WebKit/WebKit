/*
 *  Copyright 2021 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This utility computes values needed to generate yuvconstants based on
// white point values.
// The yuv formulas are tuned for 8 bit YUV channels.

// See Also
// https://mymusing.co/bt601-yuv-to-rgb-conversion-color/

// BT.709 full range YUV to RGB reference
//  R = Y               + V * 1.5748
//  G = Y - U * 0.18732 - V * 0.46812
//  B = Y + U * 1.8556
//  KR = 0.2126
//  KB = 0.0722

// // Y contribution to R,G,B.  Scale and bias.
// #define YG 16320 /* round(1.000 * 64 * 256 * 256 / 257) */
// #define YB 32    /* 64 / 2 */
//
// // U and V contributions to R,G,B.
// #define UB 113 /* round(1.77200 * 64) */
// #define UG 22  /* round(0.34414 * 64) */
// #define VG 46  /* round(0.71414 * 64) */
// #define VR 90  /* round(1.40200 * 64) */
//
// // Bias values to round, and subtract 128 from U and V.
// #define BB (-UB * 128 + YB)
// #define BG (UG * 128 + VG * 128 + YB)
// #define BR (-VR * 128 + YB)

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    printf("yuvconstants [KR] [KB]\n");
    printf("  e.g. yuvconstants 0.2126 0.0722\n");
    printf("  MC BT          KR           KB\n");
    printf("  1  BT.709      KR = 0.2126; KB = 0.0722\n");
    printf("  4  FCC         KR = 0.30;   KB = 0.11\n");
    printf("  6  BT.601      KR = 0.299;  KB = 0.114\n");
    printf("  7  SMPTE 240M  KR = 0.212;  KB = 0.087\n");
    printf("  9  BT.2020     KR = 0.2627; KB = 0.0593\n");
    return -1;
  }
  float kr = (float)atof(argv[1]);
  float kb = (float)atof(argv[2]);
  float kg = 1 - kr - kb;

  float vr = 2 * (1 - kr);
  float ug = 2 * ((1 - kb) * kb / kg);
  float vg = 2 * ((1 - kr) * kr / kg);
  float ub = 2 * (1 - kb);

  printf("Full range\n");
  printf("R = Y                + V * %5f\n", vr);
  printf("G = Y - U * %6f - V * %6f\n", ug, vg);
  printf("B = Y + U * %5f\n", ub);

  printf("KR = %4f; ", kr);
  printf("KB = %4f\n", kb);
  // printf("KG = %4f\n", kg);
  // #define YG 16320 /* round(1.000 * 64 * 256 * 256 / 257) */
  // #define YB 32    /* 64 / 2 */
  //
  // // U and V contributions to R,G,B.

  printf("UB %-3.0f /* round(%f * 64 = %8.4f) */\n", round(ub * 64), ub, ub * 64);
  printf("UG %-3.0f /* round(%f * 64 = %8.4f) */\n", round(ug * 64), ug, ug * 64);
  printf("VG %-3.0f /* round(%f * 64 = %8.4f) */\n", round(vg * 64), vg, vg * 64);
  printf("VR %-3.0f /* round(%f * 64 = %8.4f) */\n", round(vr * 64), vr, vr * 64);

  vr = 255.f / 224.f * 2 * (1 - kr);
  ug = 255.f / 224.f * 2 * ((1 - kb) * kb / kg);
  vg = 255.f / 224.f * 2 * ((1 - kr) * kr / kg);
  ub = 255.f / 224.f * 2 * (1 - kb);

  printf("\nLimited range\n");
  printf("R = (Y - 16) * 1.164                + V * %5f\n", vr);
  printf("G = (Y - 16) * 1.164 - U * %6f - V * %6f\n", ug, vg);
  printf("B = (Y - 16) * 1.164 + U * %5f\n", ub);

  // printf("KG = %4f\n", kg);
  // #define YG 16320 /* round(1.000 * 64 * 256 * 256 / 257) */
  // #define YB 32    /* 64 / 2 */
  //
  // // U and V contributions to R,G,B.

  printf("UB %-3.0f /* round(%f * 64 = %8.4f) */\n", round(ub * 64), ub, ub * 64);
  printf("UG %-3.0f /* round(%f * 64 = %8.4f) */\n", round(ug * 64), ug, ug * 64);
  printf("VG %-3.0f /* round(%f * 64 = %8.4f) */\n", round(vg * 64), vg, vg * 64);
  printf("VR %-3.0f /* round(%f * 64 = %8.4f) */\n", round(vr * 64), vr, vr * 64);

  return 0;
}
