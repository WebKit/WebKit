/* Copyright 2018 Bernhard R. Fischer, 4096R/8E24F29D <bf@abenteuerland.at>
 *
 * This file is part of Cairo_JPG.
 *
 * Cairo_JPG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cairo_JPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cairo_JPG.  If not, see <https://www.gnu.org/licenses/>.
 */

/*! \file cairo_jpg.c
 * This file contains two functions for reading and writing JPEG files from
 * and to Cairo image surfaces. It uses the functions from the libjpeg.
 * Most of the code is directly derived from the online example at
 * http://libjpeg-turbo.virtualgl.org/Documentation/Documentation
 *
 * All prototypes are defined in cairo_jpg.h All functions and their parameters
 * and return values are described below directly at the functions. You may
 * also have a look at the preprocessor macros defined below.
 *
 * To compile this code you need to have installed the packages libcairo2-dev
 * and libjpeg-dev. Compile with the following to create an object file to link
 * with your code:
 * gcc -std=c99 -Wall -c `pkg-config cairo libjpeg --cflags --libs` cairo_jpg.c
 * Use the following command to include the main() function and create an
 * executable for testing of this code:
 * gcc -std=c99 -Wall -o cairo_jpg -DCAIRO_JPEG_MAIN `pkg-config cairo libjpeg --cflags --libs` cairo_jpg.c
 *
 * @author Bernhard R. Fischer, 4096R/8E24F29D bf@abenteuerland.at
 * @version 2020/01/18
 * @license LGPL3.
 */

#if USE(CAIRO)

#include "CairoJpegEncoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cairo.h>
extern "C" {
#include "jpeglib.h"
}

/*! Macro to activate main() function. This is only used for testing. Comment
 * it out (#undef) if you link this file to your own program.
 */
//#define CAIRO_JPEG_MAIN
//
/*! Define this to use an alternate implementation of
 * cairo_image_surface_create_from_jpeg() which fstat(3)s the file before
 * reading (see below). For huge files this /may/ be slightly faster.
 */
#undef CAIRO_JPEG_USE_FSTAT

/*! This is the read block size for the stream reader
 * cairo_image_surface_create_from_jpeg_stream().
 */
#ifdef USE_CAIRO_READ_FUNC_LEN_T
#define CAIRO_JPEG_IO_BLOCK_SIZE 4096
#else
/*! Block size has to be one if cairo_read_func_t is in use because of the lack
 * to detect EOF (truncated reads).
 */
#define CAIRO_JPEG_IO_BLOCK_SIZE 1
/*! In case of original cairo_read_func_t is used fstat() should be used for
 * performance reasons (see CAIRO_JPEG_USE_FSTAT above).
 */
#define CAIRO_JPEG_USE_FSTAT
#endif

/*! Define this to test jpeg creation with non-image surfaces. This is only for
 * testing and is to be used together with CAIRO_JPEG_MAIN.  
 */
#undef CAIRO_JPEG_TEST_SIMILAR
#if defined(CAIRO_JPEG_TEST_SIMILAR) && defined(CAIRO_JPEG_MAIN)
#include <cairo-pdf.h>
#endif


#ifndef LIBJPEG_TURBO_VERSION
/*! This function makes a covnersion for "odd" pixel sizes which typically is a
 * conversion from a 3-byte to a 4-byte (or more) pixel size or vice versa.
 * The conversion is done from the source buffer src to the destination buffer
 * dst. The caller MUST ensure that src and dst have the correct memory size.
 * This is dw * num for dst and sw * num for src. src and dst may point to the
 * same memory address.
 * @param dst Pointer to destination buffer.
 * @param dw Pixel width (in bytes) of pixels in destination buffer, dw >= 3.
 * @param src Pointer to source buffer.
 * @param sw Pixel width (in bytes) of pixels in source buffer, sw >= 3.
 * @param num Number of pixels to convert, num >= 1;
 */
static void pix_conv(unsigned char *dst, int dw, const unsigned char *src, int sw, int num)
{
   int si, di;

   // safety check
   if (dw < 3 || sw < 3 || dst == NULL || src == NULL)
      return;

   num--;
   for (si = num * sw, di = num * dw; si >= 0; si -= sw, di -= dw)
   {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      dst[di + 2] = src[si    ];
      dst[di + 1] = src[si + 1];
      dst[di + 0] = src[si + 2];
#else
      // FIXME: This is untested, it may be wrong.
      dst[di - 3] = src[si - 3];
      dst[di - 2] = src[si - 2];
      dst[di - 1] = src[si - 1];
#endif
   }
}
#endif


/*! This function creates a JPEG file in memory from a Cairo image surface.
 * @param sfc Pointer to a Cairo surface. It should be an image surface of
 * either CAIRO_FORMAT_ARGB32 or CAIRO_FORMAT_RGB24. Other formats are
 * converted to CAIRO_FORMAT_RGB24 before compression.
 * Please note that this may give unexpected results because JPEG does not
 * support transparency. Thus, default background color is used to replace
 * transparent regions. The default background color is black if not specified
 * explicitly. Thus converting e.g. PDF surfaces without having any specific
 * background color set will apear with black background and not white as you
 * might expect. In such cases it is suggested to manually convert the surface
 * to RGB24 before calling this function.
 * @param data Pointer to a memory pointer. This parameter receives a pointer
 * to the memory area where the final JPEG data is found in memory. This
 * function reserves the memory properly and it has to be freed by the caller
 * with free(3).
 * @param len Pointer to a variable of type size_t which will receive the final
 * lenght of the memory buffer.
 * @param quality Compression quality, 0-100.
 * @return On success the function returns CAIRO_STATUS_SUCCESS. In case of
 * error CAIRO_STATUS_INVALID_FORMAT is returned.
 */
cairo_status_t cairo_image_surface_write_to_jpeg_mem(cairo_surface_t *sfc, unsigned char **data, size_t *len, int quality)
{
   struct jpeg_compress_struct cinfo;
   struct jpeg_error_mgr jerr;
   JSAMPROW row_pointer[1];
   cairo_surface_t *other = NULL;

   // check valid input format (must be IMAGE_SURFACE && (ARGB32 || RGB24))
   if (cairo_surface_get_type(sfc) != CAIRO_SURFACE_TYPE_IMAGE ||
         (cairo_image_surface_get_format(sfc) != CAIRO_FORMAT_ARGB32 &&
         cairo_image_surface_get_format(sfc) != CAIRO_FORMAT_RGB24))
   {
      // create a similar surface with a proper format if supplied input format
      // does not fulfill the requirements
      double x1, y1, x2, y2;
      other = sfc;
      cairo_t *ctx = cairo_create(other);
      // get extents of original surface
      cairo_clip_extents(ctx, &x1, &y1, &x2, &y2);
      cairo_destroy(ctx);

      // create new image surface
      sfc = cairo_surface_create_similar_image(other, CAIRO_FORMAT_RGB24, x2 - x1, y2 - y1);
      if (cairo_surface_status(sfc) != CAIRO_STATUS_SUCCESS)
         return CAIRO_STATUS_INVALID_FORMAT;

      // paint original surface to new surface
      ctx = cairo_create(sfc);
      cairo_set_source_surface(ctx, other, 0, 0);
      cairo_paint(ctx);
      cairo_destroy(ctx);
   }

   // finish queued drawing operations
   cairo_surface_flush(sfc);

   // init jpeg compression structures
   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_compress(&cinfo);

   // set compression parameters
   unsigned long targetSize;
   jpeg_mem_dest(&cinfo, data, &targetSize);

   cinfo.image_width = cairo_image_surface_get_width(sfc);
   cinfo.image_height = cairo_image_surface_get_height(sfc);
#ifdef LIBJPEG_TURBO_VERSION
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
   //cinfo.in_color_space = JCS_EXT_BGRX;
   cinfo.in_color_space = cairo_image_surface_get_format(sfc) == CAIRO_FORMAT_ARGB32 ? JCS_EXT_BGRA : JCS_EXT_BGRX;
#else
   //cinfo.in_color_space = JCS_EXT_XRGB;
   cinfo.in_color_space = cairo_image_surface_get_format(sfc) == CAIRO_FORMAT_ARGB32 ? JCS_EXT_ARGB : JCS_EXT_XRGB;
#endif
   cinfo.input_components = 4;
#else
   cinfo.in_color_space = JCS_RGB;
   cinfo.input_components = 3;
#endif
   jpeg_set_defaults(&cinfo);
   jpeg_set_quality(&cinfo, quality, TRUE);

   // start compressor
   jpeg_start_compress(&cinfo, TRUE);

   // loop over all lines and compress
   while (cinfo.next_scanline < cinfo.image_height)
   {
#ifdef LIBJPEG_TURBO_VERSION
      row_pointer[0] = cairo_image_surface_get_data(sfc) + (cinfo.next_scanline
            * cairo_image_surface_get_stride(sfc));
#else
      unsigned char row_buf[3 * cinfo.image_width];
      pix_conv(row_buf, 3, cairo_image_surface_get_data(sfc) +
            (cinfo.next_scanline * cairo_image_surface_get_stride(sfc)), 4, cinfo.image_width);
      row_pointer[0] = row_buf;
#endif
      (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
   }

   // finalize and close everything
   jpeg_finish_compress(&cinfo);
   jpeg_destroy_compress(&cinfo);

   // destroy temporary image surface (if available)
   if (other != NULL)
      cairo_surface_destroy(sfc);

   *len = targetSize;
   return CAIRO_STATUS_SUCCESS;
}

#endif
