/*
 *  Copyright 2021 The LibYuv Project Authors. All rights reserved.
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

// This utility computes values needed to generate yuvconstants based on
// white point values.
// The yuv formulas are tuned for 8 bit YUV channels.

// For those MCs that can be represented as kr and kb:
// Full range
// float M[3][3]
// {{1,0,2*(1-kr)},{1,-((2*kb)/((2-kb)*(1-kb-kr))),-((2*kr)/((2-kr)*(1-kb-kr)))},{1,2*(1-kb),0}};
// float B[3]
// {1+(256*(1-kr))/255,1-(256*kb)/(255*(2-kb)*(1-kb-kr))-(256*kr)/(255*(2-kr)*(1-kb-kr)),1+(256*(1-kb))/255};
// Limited range
// float M[3][3]
// {{85/73,0,255/112-(255*kr)/112},{85/73,-((255*kb)/(112*(2-kb)*(1-kb-kr))),-((255*kr)/(112*(2-kr)*(1-kb-kr)))},{85/73,255/112-(255*kb)/112,0}};
// float B[3]
// {77662/43435-(1537*kr)/1785,203/219-(1537*kb)/(1785*(2-kb)*(1-kb-kr))-(1537*kr)/(1785*(2-kr)*(1-kb-kr)),77662/43435-(1537*kb)/1785};

// mc bt
// 1 bt.709      KR = 0.2126; KB = 0.0722
// 4 fcc         KR = 0.30;   KB = 0.11
// 6 bt.601      KR = 0.299;  KB = 0.114
// 7 SMPTE 240M  KR = 0.212;  KB = 0.087
// 10 bt2020     KR = 0.2627; KB = 0.0593

// BT.709 full range YUV to RGB reference
//  R = Y               + V * 1.5748
//  G = Y - U * 0.18732 - V * 0.46812
//  B = Y + U * 1.8556
//  KR = 0.2126
//  KB = 0.0722

// https://mymusing.co/bt601-yuv-to-rgb-conversion-color/

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

int round(float v) {
  return (int)(v + 0.5);
}

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    printf("color kr kb\n");
    return -1;
  }
  float kr = atof(argv[1]);
  float kb = atof(argv[2]);
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
  //  printf("KG = %4f\n", kg);
  // #define YG 16320 /* round(1.000 * 64 * 256 * 256 / 257) */
  // #define YB 32    /* 64 / 2 */
  //
  // // U and V contributions to R,G,B.

  printf("UB %-3d /* round(%f * 64) */\n", round(ub * 64), ub);
  printf("UG %-3d /* round(%f * 64) */\n", round(ug * 64), ug);
  printf("VG %-3d /* round(%f * 64) */\n", round(vg * 64), vg);
  printf("VR %-3d /* round(%f * 64) */\n", round(vr * 64), vr);

  vr = 255.f / 224.f * 2 * (1 - kr);
  ug = 255.f / 224.f * 2 * ((1 - kb) * kb / kg);
  vg = 255.f / 224.f * 2 * ((1 - kr) * kr / kg);
  ub = 255.f / 224.f * 2 * (1 - kb);

  printf("Limited range\n");
  printf("R = (Y - 16) * 1.164                + V * %5f\n", vr);
  printf("G = (Y - 16) * 1.164 - U * %6f - V * %6f\n", ug, vg);
  printf("B = (Y - 16) * 1.164 + U * %5f\n", ub);

  //  printf("KG = %4f\n", kg);
  // #define YG 16320 /* round(1.000 * 64 * 256 * 256 / 257) */
  // #define YB 32    /* 64 / 2 */
  //
  // // U and V contributions to R,G,B.

  printf("UB %-3d /* round(%f * 64) */\n", round(ub * 64), ub);
  printf("UG %-3d /* round(%f * 64) */\n", round(ug * 64), ug);
  printf("VG %-3d /* round(%f * 64) */\n", round(vg * 64), vg);
  printf("VR %-3d /* round(%f * 64) */\n", round(vr * 64), vr);

  return 0;
}
