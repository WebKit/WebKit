/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include <png.h>
#include <errno.h>
#include "cairoint.h"

/* Unpremultiplies data and converts native endian ARGB => RGBA bytes */
static void
unpremultiply_data (png_structp png, png_row_infop row_info, png_bytep data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        uint8_t *b = &data[i];
        uint32_t pixel;
        uint8_t  alpha;

	memcpy (&pixel, b, sizeof (uint32_t));
	alpha = (pixel & 0xff000000) >> 24;
        if (alpha == 0) {
	    b[0] = b[1] = b[2] = b[3] = 0;
	} else {
            b[0] = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
            b[1] = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
            b[2] = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
	    b[3] = alpha;
	}
    }
}

/* Converts native endian xRGB => RGBx bytes */
static void
convert_data_to_bytes (png_structp png, png_row_infop row_info, png_bytep data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        uint8_t *b = &data[i];
        uint32_t pixel;

	memcpy (&pixel, b, sizeof (uint32_t));
	
	b[0] = (pixel & 0xff0000) >> 16;
	b[1] = (pixel & 0x00ff00) >>  8;
	b[2] = (pixel & 0x0000ff) >>  0;
	b[3] = 0;
    }
}

static cairo_status_t
write_png (cairo_surface_t	*surface,
	   png_rw_ptr		write_func,
	   void			*closure)
{
    int i;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_image_surface_t *image;
    void *image_extra;
    png_struct *png;
    png_info *info;
    png_time pt;
    png_byte **rows;
    png_color_16 white;
    int png_color_type;
    int depth;

    status = _cairo_surface_acquire_source_image (surface,
						  &image,
						  &image_extra);

    if (status == CAIRO_STATUS_NO_MEMORY)
        return CAIRO_STATUS_NO_MEMORY;
    else if (status != CAIRO_STATUS_SUCCESS)
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    rows = malloc (image->height * sizeof(png_byte*));
    if (rows == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL1;
    }

    for (i = 0; i < image->height; i++)
	rows[i] = (png_byte *) image->data + i * image->stride;

    png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL2;
    }

    info = png_create_info_struct (png);
    if (info == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL3;
    }

    if (setjmp (png_jmpbuf (png))) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL3;
    }
    
    png_set_write_fn (png, closure, write_func, NULL);

    switch (image->format) {
    case CAIRO_FORMAT_ARGB32:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	break;
    case CAIRO_FORMAT_RGB24:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB;
	break;
    case CAIRO_FORMAT_A8:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    case CAIRO_FORMAT_A1:
	depth = 1;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    default:
	status = CAIRO_STATUS_NULL_POINTER;
	goto BAIL3;
    }

    png_set_IHDR (png, info,
		  image->width,
		  image->height, depth,
		  png_color_type,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);

    white.red = 0xff;
    white.blue = 0xff;
    white.green = 0xff;
    png_set_bKGD (png, info, &white);

    png_convert_from_time_t (&pt, time (NULL));
    png_set_tIME (png, info, &pt);

    /* We have to call png_write_info() before setting up the write
     * transformation, since it stores data internally in 'png'
     * that is needed for the write transformation functions to work.
     */
    png_write_info (png, info);
    
    if (image->format == CAIRO_FORMAT_ARGB32)
	png_set_write_user_transform_fn (png, unpremultiply_data);
    else if (image->format == CAIRO_FORMAT_RGB24)
	png_set_write_user_transform_fn (png, convert_data_to_bytes);
    if (image->format == CAIRO_FORMAT_RGB24)
	png_set_filler (png, 0, PNG_FILLER_AFTER);
	
    png_write_image (png, rows);
    png_write_end (png, info);

BAIL3:
    png_destroy_write_struct (&png, &info);
BAIL2:
    free (rows);
BAIL1:
    _cairo_surface_release_source_image (surface, image, image_extra);

    return status;
}

static void
stdio_write_func (png_structp png, png_bytep data, png_size_t size)
{
    FILE *fp;

    fp = png_get_io_ptr (png);
    if (fwrite (data, 1, size, fp) != size)
	png_error(png, "Write Error");
}

/**
 * cairo_surface_write_to_png:
 * @surface: a #cairo_surface_t with pixel contents
 * @filename: the name of a file to write to
 * 
 * Writes the contents of @surface to a new file @filename as a PNG
 * image.
 * 
 * Return value: CAIRO_STATUS_SUCCESS if the PNG file was written
 * successfully. Otherwise, CAIRO_STATUS_NO_MEMORY if memory could not
 * be allocated for the operation or
 * CAIRO_STATUS_SURFACE_TYPE_MISMATCH if the surface does not have
 * pixel contents, or CAIRO_STATUS_WRITE_ERROR if an I/O error occurs
 * while attempting to write the file.
 **/
cairo_status_t
cairo_surface_write_to_png (cairo_surface_t	*surface,
			    const char		*filename)
{
    FILE *fp;
    cairo_status_t status;

    fp = fopen (filename, "wb");
    if (fp == NULL)
	return CAIRO_STATUS_WRITE_ERROR;
  
    status = write_png (surface, stdio_write_func, fp);

    if (fclose (fp) && status == CAIRO_STATUS_SUCCESS)
	status = CAIRO_STATUS_WRITE_ERROR;

    return status;
}

struct png_write_closure_t {
    cairo_write_func_t	write_func;
    void			*closure;
};

static void
stream_write_func (png_structp png, png_bytep data, png_size_t size)
{
    cairo_status_t status;
    struct png_write_closure_t *png_closure;

    png_closure = png_get_io_ptr (png);
    status = png_closure->write_func (png_closure->closure, data, size);
    if (status)
	png_error(png, "Write Error");
}

/**
 * cairo_surface_write_to_png_stream:
 * @surface: a #cairo_surface_t with pixel contents
 * @write_func: a #cairo_write_func_t
 * @closure: closure data for the write function
 * 
 * Writes the image surface to the write function.
 * 
 * Return value: CAIRO_STATUS_SUCCESS if the PNG file was written
 * successfully.  Otherwise, CAIRO_STATUS_NO_MEMORY is returned if
 * memory could not be allocated for the operation,
 * CAIRO_STATUS_SURFACE_TYPE_MISMATCH if the surface does not have
 * pixel contents.
 **/
cairo_status_t
cairo_surface_write_to_png_stream (cairo_surface_t	*surface,
				   cairo_write_func_t	write_func,
				   void			*closure)
{
    struct png_write_closure_t png_closure;

    png_closure.write_func = write_func;
    png_closure.closure = closure;

    return write_png (surface, stream_write_func, &png_closure);
}				     

static INLINE int
multiply_alpha (int alpha, int color)
{
    int temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

/* Premultiplies data and converts RGBA bytes => native endian */
static void
premultiply_data (png_structp   png,
                  png_row_infop row_info,
                  png_bytep     data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
	uint8_t *base = &data[i];
	uint8_t  alpha = base[3];
	uint32_t p;

	if (alpha == 0) {
	    p = 0;
	} else {
	    uint8_t  red = base[0];
	    uint8_t  green = base[1];
	    uint8_t  blue = base[2];

	    if (alpha != 0xff) {
		red = multiply_alpha (alpha, red);
		green = multiply_alpha (alpha, green);
		blue = multiply_alpha (alpha, blue);
	    }
	    p = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	}
	memcpy (base, &p, sizeof (uint32_t));
    }
}

static cairo_surface_t *
read_png (png_rw_ptr	read_func,
	  void		*closure)
{
    cairo_surface_t *surface = (cairo_surface_t*) &_cairo_surface_nil;
    png_byte *data = NULL;
    int i;
    png_struct *png = NULL;
    png_info *info;
    png_uint_32 png_width, png_height, stride;
    int depth, color_type, interlace;
    unsigned int pixel_size;
    png_byte **row_pointers = NULL;

    /* XXX: Perhaps we'll want some other error handlers? */
    png = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                  NULL,
                                  NULL,
                                  NULL);
    if (png == NULL)
	goto BAIL;

    info = png_create_info_struct (png);
    if (info == NULL)
	goto BAIL;

    png_set_read_fn (png, closure, read_func);

    if (setjmp (png_jmpbuf (png))) {
	surface = (cairo_surface_t*) &_cairo_surface_nil_read_error;
	goto BAIL;
    }

    png_read_info (png, info);

    png_get_IHDR (png, info,
                  &png_width, &png_height, &depth,
                  &color_type, &interlace, NULL, NULL);
    stride = 4 * png_width;

    /* convert palette/gray image to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb (png);

    /* expand gray bit depth if needed */
    if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_gray_1_2_4_to_8 (png);
    /* transform transparency to alpha */
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha (png);

    if (depth == 16)
        png_set_strip_16 (png);

    if (depth < 8)
        png_set_packing (png);

    /* convert grayscale to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY
        || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb (png);

    if (interlace != PNG_INTERLACE_NONE)
        png_set_interlace_handling (png);

    png_set_filler (png, 0xff, PNG_FILLER_AFTER);

    png_set_read_user_transform_fn (png, premultiply_data);

    png_read_update_info (png, info);

    pixel_size = 4;
    data = malloc (png_width * png_height * pixel_size);
    if (data == NULL)
	goto BAIL;

    row_pointers = malloc (png_height * sizeof(char *));
    if (row_pointers == NULL)
	goto BAIL;

    for (i = 0; i < png_height; i++)
        row_pointers[i] = &data[i * png_width * pixel_size];

    png_read_image (png, row_pointers);
    png_read_end (png, info);

    surface = cairo_image_surface_create_for_data (data,
						   CAIRO_FORMAT_ARGB32,
						   png_width, png_height, stride);
    if (surface->status)
	goto BAIL;

    _cairo_image_surface_assume_ownership_of_data ((cairo_image_surface_t*)surface);
    data = NULL;

 BAIL:
    if (row_pointers)
	free (row_pointers);
    if (data)
	free (data);
    if (png)
	png_destroy_read_struct (&png, &info, NULL);

    if (surface->status)
	_cairo_error (surface->status);

    return surface;
}

static void
stdio_read_func (png_structp png, png_bytep data, png_size_t size)
{
    FILE *fp;

    fp = png_get_io_ptr (png);
    if (fread (data, 1, size, fp) != size)
	png_error(png, "Read Error");
}

/**
 * cairo_image_surface_create_from_png:
 * @filename: name of PNG file to load 
 * 
 * Creates a new image surface and initializes the contents to the
 * given PNG file.
 * 
 * Return value: a new #cairo_surface_t initialized with the contents
 * of the PNG file, or a "nil" surface if any error occurred. A nil
 * surface can be checked for with cairo_surface_status(surface) which
 * may return one of the following values: 
 *
 *	CAIRO_STATUS_NO_MEMORY
 *	CAIRO_STATUS_FILE_NOT_FOUND
 *	CAIRO_STATUS_READ_ERROR
 **/
cairo_surface_t *
cairo_image_surface_create_from_png (const char *filename)
{
    FILE *fp;
    cairo_surface_t *surface;

    fp = fopen (filename, "rb");
    if (fp == NULL) {
	switch (errno) {
	case ENOMEM:
	    _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    return (cairo_surface_t*) &_cairo_surface_nil;
	case ENOENT:
	    _cairo_error (CAIRO_STATUS_FILE_NOT_FOUND);
	    return (cairo_surface_t*) &_cairo_surface_nil_file_not_found;
	default:
	    _cairo_error (CAIRO_STATUS_READ_ERROR);
	    return (cairo_surface_t*) &_cairo_surface_nil_read_error;
	}
    }
  
    surface = read_png (stdio_read_func, fp);

    fclose (fp);

    return surface;
}

struct png_read_closure_t {
    cairo_read_func_t	read_func;
    void			*closure;
};

static void
stream_read_func (png_structp png, png_bytep data, png_size_t size)
{
    cairo_status_t status;
    struct png_read_closure_t *png_closure;

    png_closure = png_get_io_ptr (png);
    status = png_closure->read_func (png_closure->closure, data, size);
    if (status)
	png_error(png, "Read Error");
}

/**
 * cairo_image_surface_create_from_png_stream:
 * @read_func: function called to read the data of the file
 * @closure: data to pass to @read_func.
 * 
 * Creates a new image surface from PNG data read incrementally
 * via the @read_func function.
 * 
 * Return value: a new #cairo_surface_t initialized with the contents
 * of the PNG file or %NULL if the data read is not a valid PNG image or
 * memory could not be allocated for the operation.
 **/
cairo_surface_t *
cairo_image_surface_create_from_png_stream (cairo_read_func_t	read_func,
					    void		*closure)
{
    struct png_read_closure_t png_closure;

    png_closure.read_func = read_func;
    png_closure.closure = closure;

    return read_png (stream_read_func, &png_closure);
}

