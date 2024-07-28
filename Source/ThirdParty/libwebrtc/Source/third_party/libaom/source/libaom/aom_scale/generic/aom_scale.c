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

/****************************************************************************
 *
 *   Module Title :     scale.c
 *
 *   Description  :     Image scaling functions.
 *
 ***************************************************************************/

/****************************************************************************
 *  Header Files
 ****************************************************************************/
#include "config/aom_scale_rtcd.h"

#include "aom_mem/aom_mem.h"
#include "aom_scale/aom_scale.h"
#include "aom_scale/yv12config.h"

typedef struct {
  int expanded_frame_width;
  int expanded_frame_height;

  int HScale;
  int HRatio;
  int VScale;
  int VRatio;

  YV12_BUFFER_CONFIG *src_yuv_config;
  YV12_BUFFER_CONFIG *dst_yuv_config;

} SCALE_VARS;

/****************************************************************************
 *
 *  ROUTINE       : scale1d_2t1_i
 *
 *  INPUTS        : const unsigned char *source : Pointer to data to be scaled.
 *                  int source_step             : Number of pixels to step on
 *                                                in source.
 *                  unsigned int source_scale   : Scale for source (UNUSED).
 *                  unsigned int source_length  : Length of source (UNUSED).
 *                  unsigned char *dest         : Pointer to output data array.
 *                  int dest_step               : Number of pixels to step on
 *                                                in destination.
 *                  unsigned int dest_scale     : Scale for destination
 *                                                (UNUSED).
 *                  unsigned int dest_length    : Length of destination.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-to-1 interpolated scaling.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static void scale1d_2t1_i(const unsigned char *source, int source_step,
                          unsigned int source_scale, unsigned int source_length,
                          unsigned char *dest, int dest_step,
                          unsigned int dest_scale, unsigned int dest_length) {
  const unsigned char *const dest_end = dest + dest_length * dest_step;
  (void)source_length;
  (void)source_scale;
  (void)dest_scale;

  source_step *= 2;  // Every other row.

  dest[0] = source[0];  // Special case: 1st pixel.
  source += source_step;
  dest += dest_step;

  while (dest < dest_end) {
    const unsigned int a = 3 * source[-source_step];
    const unsigned int b = 10 * source[0];
    const unsigned int c = 3 * source[source_step];
    *dest = (unsigned char)((8 + a + b + c) >> 4);
    source += source_step;
    dest += dest_step;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : scale1d_2t1_ps
 *
 *  INPUTS        : const unsigned char *source : Pointer to data to be scaled.
 *                  int source_step             : Number of pixels to step on
 *                                                in source.
 *                  unsigned int source_scale   : Scale for source (UNUSED).
 *                  unsigned int source_length  : Length of source (UNUSED).
 *                  unsigned char *dest         : Pointer to output data array.
 *                  int dest_step               : Number of pixels to step on
 *                                                in destination.
 *                  unsigned int dest_scale     : Scale for destination
 *                                                (UNUSED).
 *                  unsigned int dest_length    : Length of destination.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-to-1 point subsampled scaling.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static void scale1d_2t1_ps(const unsigned char *source, int source_step,
                           unsigned int source_scale,
                           unsigned int source_length, unsigned char *dest,
                           int dest_step, unsigned int dest_scale,
                           unsigned int dest_length) {
  const unsigned char *const dest_end = dest + dest_length * dest_step;
  (void)source_length;
  (void)source_scale;
  (void)dest_scale;

  source_step *= 2;  // Every other row.

  while (dest < dest_end) {
    *dest = *source;
    source += source_step;
    dest += dest_step;
  }
}
/****************************************************************************
 *
 *  ROUTINE       : scale1d_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to data to be scaled.
 *                  int source_step             : Number of pixels to step on
 *                                                in source.
 *                  unsigned int source_scale   : Scale for source.
 *                  unsigned int source_length  : Length of source (UNUSED).
 *                  unsigned char *dest         : Pointer to output data array.
 *                  int dest_step               : Number of pixels to step on
 *                                                in destination.
 *                  unsigned int dest_scale     : Scale for destination.
 *                  unsigned int dest_length    : Length of destination.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs linear interpolation in one dimension.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static void scale1d_c(const unsigned char *source, int source_step,
                      unsigned int source_scale, unsigned int source_length,
                      unsigned char *dest, int dest_step,
                      unsigned int dest_scale, unsigned int dest_length) {
  const unsigned char *const dest_end = dest + dest_length * dest_step;
  const unsigned int round_value = dest_scale / 2;
  unsigned int left_modifier = dest_scale;
  unsigned int right_modifier = 0;
  unsigned char left_pixel = source[0];
  unsigned char right_pixel = source[source_step];

  (void)source_length;

  /* These asserts are needed if there are boundary issues... */
  /* assert ( dest_scale > source_scale );*/
  /* assert ( (source_length - 1) * dest_scale >= (dest_length - 1) *
   * source_scale);*/

  while (dest < dest_end) {
    *dest = (unsigned char)((left_modifier * left_pixel +
                             right_modifier * right_pixel + round_value) /
                            dest_scale);

    right_modifier += source_scale;

    while (right_modifier > dest_scale) {
      right_modifier -= dest_scale;
      source += source_step;
      left_pixel = source[0];
      right_pixel = source[source_step];
    }

    left_modifier = dest_scale - right_modifier;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : Scale2D
 *
 *  INPUTS        : const unsigned char *source    : Pointer to data to be
 *                                                   scaled.
 *                  int source_pitch               : Stride of source image.
 *                  unsigned int source_width      : Width of input image.
 *                  unsigned int source_height     : Height of input image.
 *                  unsigned char *dest            : Pointer to output data
 *                                                   array.
 *                  int dest_pitch                 : Stride of destination
 *                                                   image.
 *                  unsigned int dest_width        : Width of destination image.
 *                  unsigned int dest_height       : Height of destination
 *                                                   image.
 *                  unsigned char *temp_area       : Pointer to temp work area.
 *                  unsigned char temp_area_height : Height of temp work area.
 *                  unsigned int hscale            : Horizontal scale factor
 *                                                   numerator.
 *                  unsigned int hratio            : Horizontal scale factor
 *                                                   denominator.
 *                  unsigned int vscale            : Vertical scale factor
 *                                                   numerator.
 *                  unsigned int vratio            : Vertical scale factor
 *                                                   denominator.
 *                  unsigned int interlaced        : Interlace flag.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-tap linear interpolation in two dimensions.
 *
 *  SPECIAL NOTES : Expansion is performed one band at a time to help with
 *                  caching.
 *
 ****************************************************************************/
static void Scale2D(
    /*const*/
    unsigned char *source, int source_pitch, unsigned int source_width,
    unsigned int source_height, unsigned char *dest, int dest_pitch,
    unsigned int dest_width, unsigned int dest_height, unsigned char *temp_area,
    unsigned char temp_area_height, unsigned int hscale, unsigned int hratio,
    unsigned int vscale, unsigned int vratio, unsigned int interlaced) {
  unsigned int i, j, k;
  unsigned int bands;
  unsigned int dest_band_height;
  unsigned int source_band_height;

  typedef void (*Scale1D)(const unsigned char *source, int source_step,
                          unsigned int source_scale, unsigned int source_length,
                          unsigned char *dest, int dest_step,
                          unsigned int dest_scale, unsigned int dest_length);

  Scale1D Scale1Dv = scale1d_c;
  Scale1D Scale1Dh = scale1d_c;

  void (*horiz_line_scale)(const unsigned char *, unsigned int, unsigned char *,
                           unsigned int) = NULL;
  void (*vert_band_scale)(unsigned char *, int, unsigned char *, int,
                          unsigned int) = NULL;

  int ratio_scalable = 1;
  int interpolation = 0;

  unsigned char *source_base;
  unsigned char *line_src;

  source_base = (unsigned char *)source;

  if (source_pitch < 0) {
    int offset;

    offset = (source_height - 1);
    offset *= source_pitch;

    source_base += offset;
  }

  /* find out the ratio for each direction */
  switch (hratio * 10 / hscale) {
    case 8:
      /* 4-5 Scale in Width direction */
      horiz_line_scale = aom_horizontal_line_5_4_scale;
      break;
    case 6:
      /* 3-5 Scale in Width direction */
      horiz_line_scale = aom_horizontal_line_5_3_scale;
      break;
    case 5:
      /* 1-2 Scale in Width direction */
      horiz_line_scale = aom_horizontal_line_2_1_scale;
      break;
    default:
      /* The ratio is not acceptable now */
      /* throw("The ratio is not acceptable for now!"); */
      ratio_scalable = 0;
      break;
  }

  switch (vratio * 10 / vscale) {
    case 8:
      /* 4-5 Scale in vertical direction */
      vert_band_scale = aom_vertical_band_5_4_scale;
      source_band_height = 5;
      dest_band_height = 4;
      break;
    case 6:
      /* 3-5 Scale in vertical direction */
      vert_band_scale = aom_vertical_band_5_3_scale;
      source_band_height = 5;
      dest_band_height = 3;
      break;
    case 5:
      /* 1-2 Scale in vertical direction */

      if (interlaced) {
        /* if the content is interlaced, point sampling is used */
        vert_band_scale = aom_vertical_band_2_1_scale;
      } else {
        interpolation = 1;
        /* if the content is progressive, interplo */
        vert_band_scale = aom_vertical_band_2_1_scale_i;
      }

      source_band_height = 2;
      dest_band_height = 1;
      break;
    default:
      /* The ratio is not acceptable now */
      /* throw("The ratio is not acceptable for now!"); */
      ratio_scalable = 0;
      break;
  }

  if (ratio_scalable) {
    if (source_height == dest_height) {
      /* for each band of the image */
      for (k = 0; k < dest_height; ++k) {
        horiz_line_scale(source, source_width, dest, dest_width);
        source += source_pitch;
        dest += dest_pitch;
      }

      return;
    }

    if (interpolation) {
      if (source < source_base) source = source_base;

      horiz_line_scale(source, source_width, temp_area, dest_width);
    }

    for (k = 0; k < (dest_height + dest_band_height - 1) / dest_band_height;
         ++k) {
      /* scale one band horizontally */
      for (i = 0; i < source_band_height; ++i) {
        /* Trap case where we could read off the base of the source buffer */

        line_src = source + i * source_pitch;

        if (line_src < source_base) line_src = source_base;

        horiz_line_scale(line_src, source_width,
                         temp_area + (i + 1) * dest_pitch, dest_width);
      }

      /* Vertical scaling is in place */
      vert_band_scale(temp_area + dest_pitch, dest_pitch, dest, dest_pitch,
                      dest_width);

      if (interpolation)
        memcpy(temp_area, temp_area + source_band_height * dest_pitch,
               dest_width);

      /* Next band... */
      source += (unsigned long)source_band_height * source_pitch;
      dest += (unsigned long)dest_band_height * dest_pitch;
    }

    return;
  }

  if (hscale == 2 && hratio == 1) Scale1Dh = scale1d_2t1_ps;

  if (vscale == 2 && vratio == 1) {
    if (interlaced)
      Scale1Dv = scale1d_2t1_ps;
    else
      Scale1Dv = scale1d_2t1_i;
  }

  if (source_height == dest_height) {
    /* for each band of the image */
    for (k = 0; k < dest_height; ++k) {
      Scale1Dh(source, 1, hscale, source_width + 1, dest, 1, hratio,
               dest_width);
      source += source_pitch;
      dest += dest_pitch;
    }

    return;
  }

  if (dest_height > source_height) {
    dest_band_height = temp_area_height - 1;
    source_band_height = dest_band_height * source_height / dest_height;
  } else {
    source_band_height = temp_area_height - 1;
    dest_band_height = source_band_height * vratio / vscale;
  }

  /* first row needs to be done so that we can stay one row ahead for vertical
   * zoom */
  Scale1Dh(source, 1, hscale, source_width + 1, temp_area, 1, hratio,
           dest_width);

  /* for each band of the image */
  bands = (dest_height + dest_band_height - 1) / dest_band_height;

  for (k = 0; k < bands; ++k) {
    /* scale one band horizontally */
    for (i = 1; i < source_band_height + 1; ++i) {
      if (k * source_band_height + i < source_height) {
        Scale1Dh(source + i * source_pitch, 1, hscale, source_width + 1,
                 temp_area + i * dest_pitch, 1, hratio, dest_width);
      } else { /*  Duplicate the last row */
        /* copy temp_area row 0 over from last row in the past */
        memcpy(temp_area + i * dest_pitch, temp_area + (i - 1) * dest_pitch,
               dest_pitch);
      }
    }

    /* scale one band vertically */
    for (j = 0; j < dest_width; ++j) {
      Scale1Dv(&temp_area[j], dest_pitch, vscale, source_band_height + 1,
               &dest[j], dest_pitch, vratio, dest_band_height);
    }

    /* copy temp_area row 0 over from last row in the past */
    memcpy(temp_area, temp_area + source_band_height * dest_pitch, dest_pitch);

    /* move to the next band */
    source += source_band_height * source_pitch;
    dest += dest_band_height * dest_pitch;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : aom_scale_frame
 *
 *  INPUTS        : YV12_BUFFER_CONFIG *src        : Pointer to frame to be
 *                                                   scaled.
 *                  YV12_BUFFER_CONFIG *dst        : Pointer to buffer to hold
 *                                                   scaled frame.
 *                  unsigned char *temp_area       : Pointer to temp work area.
 *                  unsigned char temp_area_height : Height of temp work area.
 *                  unsigned int hscale            : Horizontal scale factor
 *                                                   numerator.
 *                  unsigned int hratio            : Horizontal scale factor
 *                                                   denominator.
 *                  unsigned int vscale            : Vertical scale factor
 *                                                   numerator.
 *                  unsigned int vratio            : Vertical scale factor
 *                                                   denominator.
 *                  unsigned int interlaced        : Interlace flag.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-tap linear interpolation in two dimensions.
 *
 *  SPECIAL NOTES : Expansion is performed one band at a time to help with
 *                  caching.
 *
 ****************************************************************************/
void aom_scale_frame(YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                     unsigned char *temp_area, unsigned char temp_height,
                     unsigned int hscale, unsigned int hratio,
                     unsigned int vscale, unsigned int vratio,
                     unsigned int interlaced, const int num_planes) {
  const int dw = (hscale - 1 + src->y_width * hratio) / hscale;
  const int dh = (vscale - 1 + src->y_height * vratio) / vscale;

  for (int plane = 0; plane < num_planes; ++plane) {
    const int is_uv = plane > 0;
    const int plane_dw = dw >> is_uv;
    const int plane_dh = dh >> is_uv;

    Scale2D((unsigned char *)src->buffers[plane], src->strides[is_uv],
            src->widths[is_uv], src->heights[is_uv],
            (unsigned char *)dst->buffers[plane], dst->strides[is_uv], plane_dw,
            plane_dh, temp_area, temp_height, hscale, hratio, vscale, vratio,
            interlaced);

    if (plane_dw < dst->widths[is_uv])
      for (int i = 0; i < plane_dh; ++i)
        memset(dst->buffers[plane] + i * dst->strides[is_uv] + plane_dw - 1,
               dst->buffers[plane][i * dst->strides[is_uv] + plane_dw - 2],
               dst->widths[is_uv] - plane_dw + 1);

    if (plane_dh < dst->heights[is_uv])
      for (int i = plane_dh - 1; i < dst->heights[is_uv]; ++i)
        memcpy(dst->buffers[plane] + i * dst->strides[is_uv],
               dst->buffers[plane] + (plane_dh - 2) * dst->strides[is_uv],
               dst->widths[is_uv] + 1);
  }
}
