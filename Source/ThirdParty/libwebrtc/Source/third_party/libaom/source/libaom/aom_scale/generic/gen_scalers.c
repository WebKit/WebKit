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

#include "config/aom_scale_rtcd.h"

#include "aom_scale/aom_scale.h"
#include "aom_mem/aom_mem.h"
/****************************************************************************
 *  Imports
 ****************************************************************************/

/****************************************************************************
 *
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width   : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width     : Stride of destination
 *                                                (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 4 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void aom_horizontal_line_5_4_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  const unsigned char *const source_end = source + source_width;
  (void)dest_width;

  while (source < source_end) {
    const unsigned int a = source[0];
    const unsigned int b = source[1];
    const unsigned int c = source[2];
    const unsigned int d = source[3];
    const unsigned int e = source[4];

    dest[0] = (unsigned char)a;
    dest[1] = (unsigned char)((b * 192 + c * 64 + 128) >> 8);
    dest[2] = (unsigned char)((c * 128 + d * 128 + 128) >> 8);
    dest[3] = (unsigned char)((d * 64 + e * 192 + 128) >> 8);

    source += 5;
    dest += 4;
  }
}

void aom_vertical_band_5_4_scale_c(unsigned char *source, int src_pitch,
                                   unsigned char *dest, int dest_pitch,
                                   unsigned int dest_width) {
  const unsigned char *const dest_end = dest + dest_width;
  while (dest < dest_end) {
    const unsigned int a = source[0 * src_pitch];
    const unsigned int b = source[1 * src_pitch];
    const unsigned int c = source[2 * src_pitch];
    const unsigned int d = source[3 * src_pitch];
    const unsigned int e = source[4 * src_pitch];

    dest[0 * dest_pitch] = (unsigned char)a;
    dest[1 * dest_pitch] = (unsigned char)((b * 192 + c * 64 + 128) >> 8);
    dest[2 * dest_pitch] = (unsigned char)((c * 128 + d * 128 + 128) >> 8);
    dest[3 * dest_pitch] = (unsigned char)((d * 64 + e * 192 + 128) >> 8);

    ++source;
    ++dest;
  }
}

/*7***************************************************************************
 *
 *  ROUTINE       : aom_horizontal_line_3_5_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width   : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width     : Stride of destination
 *                                                (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 3 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 *
 ****************************************************************************/
void aom_horizontal_line_5_3_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  const unsigned char *const source_end = source + source_width;
  (void)dest_width;
  while (source < source_end) {
    const unsigned int a = source[0];
    const unsigned int b = source[1];
    const unsigned int c = source[2];
    const unsigned int d = source[3];
    const unsigned int e = source[4];

    dest[0] = (unsigned char)a;
    dest[1] = (unsigned char)((b * 85 + c * 171 + 128) >> 8);
    dest[2] = (unsigned char)((d * 171 + e * 85 + 128) >> 8);

    source += 5;
    dest += 3;
  }
}

void aom_vertical_band_5_3_scale_c(unsigned char *source, int src_pitch,
                                   unsigned char *dest, int dest_pitch,
                                   unsigned int dest_width) {
  const unsigned char *const dest_end = dest + dest_width;
  while (dest < dest_end) {
    const unsigned int a = source[0 * src_pitch];
    const unsigned int b = source[1 * src_pitch];
    const unsigned int c = source[2 * src_pitch];
    const unsigned int d = source[3 * src_pitch];
    const unsigned int e = source[4 * src_pitch];

    dest[0 * dest_pitch] = (unsigned char)a;
    dest[1 * dest_pitch] = (unsigned char)((b * 85 + c * 171 + 128) >> 8);
    dest[2 * dest_pitch] = (unsigned char)((d * 171 + e * 85 + 128) >> 8);

    ++source;
    ++dest;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : aom_horizontal_line_1_2_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width   : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width     : Stride of destination
 *                                                (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 1 to 2.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void aom_horizontal_line_2_1_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  const unsigned char *const source_end = source + source_width;
  (void)dest_width;
  while (source < source_end) {
    dest[0] = source[0];
    source += 2;
    ++dest;
  }
}

void aom_vertical_band_2_1_scale_c(unsigned char *source, int src_pitch,
                                   unsigned char *dest, int dest_pitch,
                                   unsigned int dest_width) {
  (void)dest_pitch;
  (void)src_pitch;
  memcpy(dest, source, dest_width);
}

void aom_vertical_band_2_1_scale_i_c(unsigned char *source, int src_pitch,
                                     unsigned char *dest, int dest_pitch,
                                     unsigned int dest_width) {
  const unsigned char *const dest_end = dest + dest_width;
  (void)dest_pitch;
  while (dest < dest_end) {
    const unsigned int a = source[-src_pitch] * 3;
    const unsigned int b = source[0] * 10;
    const unsigned int c = source[src_pitch] * 3;
    dest[0] = (unsigned char)((8 + a + b + c) >> 4);
    ++source;
    ++dest;
  }
}
