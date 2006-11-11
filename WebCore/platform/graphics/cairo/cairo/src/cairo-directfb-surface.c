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
 *	Michael Emmel <mike.emmel@gmail.com>
 */
#include <stdio.h>
#include <directfb.h>
#include "cairo-directfb.h"
#include "cairoint.h"

#define DFB_UNSUPPORTED -1
#define DFB_SUPPORTED 1 

/*
Glyph support not working
*/
#define DFB_SHOW_GLYPHS 0
/*
Composite support not working
*/
#define DFB_COMPOSITE 0


#if DFB_SHOW_GLYPHS 
static cairo_int_status_t
_cairo_directfb_surface_show_glyphs (cairo_scaled_font_t    *scaled_font,
                 cairo_operator_t       operator,
                 cairo_pattern_t        *pattern,
                 void               *abstract_surface,
                 int                    source_x,
                 int                    source_y,
                 int            dest_x,
                 int            dest_y,
                 unsigned int       width,
                 unsigned int       height,
                 const cairo_glyph_t    *glyphs,
                 int                    num_glyphs);
#endif

typedef struct _cairo_directfb_surface {
		cairo_surface_t   base;
		cairo_format_t format;
		IDirectFB *dfb;
		IDirectFBSurface *main_surface;
		IDirectFBSurface *buffer;
		pixman_region16_t *clip;
		cairo_surface_t *buffer_image;
		void *buffer_data;
		int width;
		int height;
} cairo_directfb_surface_t;


static int
_dfb_set_operator (cairo_operator_t operator,IDirectFBSurface *dest)
{
	dest->SetDrawingFlags(dest,DSDRAW_BLEND);
	dest->SetPorterDuff(dest,DSPD_NONE);
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	dest->SetPorterDuff(dest,DSPD_CLEAR);
	break;
    case CAIRO_OPERATOR_SOURCE:
	dest->SetPorterDuff(dest,DSPD_SRC);
	break;
    case CAIRO_OPERATOR_OVER:
	dest->SetPorterDuff(dest,DSPD_SRC_OVER);
	break;
    case CAIRO_OPERATOR_IN:
	dest->SetPorterDuff(dest,DSPD_SRC_IN);
	break;
    case CAIRO_OPERATOR_OUT:
	dest->SetPorterDuff(dest,DSPD_SRC_OUT);
	break;
    case CAIRO_OPERATOR_DEST_OVER:
	dest->SetPorterDuff(dest,DSPD_DST_OVER);
	break;
    case CAIRO_OPERATOR_DEST_IN:
	dest->SetPorterDuff(dest,DSPD_DST_IN);
	break;
    case CAIRO_OPERATOR_DEST_OUT:
	dest->SetPorterDuff(dest,DSPD_DST_OUT);
	break;

	/*not sure about these yet */
    case CAIRO_OPERATOR_ATOP:
    	return DFB_UNSUPPORTED;
	break;
    case CAIRO_OPERATOR_DEST:
    	return DFB_UNSUPPORTED;
	break;
    case CAIRO_OPERATOR_DEST_ATOP:
    	return DFB_UNSUPPORTED;
	break;
    case CAIRO_OPERATOR_XOR:
		dest->SetDrawingFlags(dest,DSDRAW_XOR);
	break;
    case CAIRO_OPERATOR_ADD:
    	return DFB_UNSUPPORTED;
	break;
    case CAIRO_OPERATOR_SATURATE: 
    	return DFB_UNSUPPORTED;
	break;
    default:
    	return DFB_UNSUPPORTED;
    }
    return DFB_SUPPORTED;
}


static inline int cairo_to_directfb_format(cairo_format_t format ) {
		switch( format ) {
				case CAIRO_FORMAT_RGB24:
						return DSPF_RGB24;
				case CAIRO_FORMAT_ARGB32:
						return DSPF_ARGB; 
				case CAIRO_FORMAT_A8:
						return DSPF_A8; 
				case CAIRO_FORMAT_A1:
						return DSPF_A1; 
				default:
				{
						/*assert(0);*/
						return DSPF_UNKNOWN; 
				}
		}
}

static inline int directfb_to_cairo_format(DFBSurfacePixelFormat dfbformat ) {
        switch( dfbformat ) {
                case DSPF_RGB24 :
                case DSPF_RGB32 :
                        return CAIRO_FORMAT_RGB24;
                        break;
                case DSPF_AiRGB :
                case DSPF_ARGB :
                        return CAIRO_FORMAT_ARGB32;
                        break;
                case DSPF_A8 :
                        return CAIRO_FORMAT_A8;
                        break;
                case DSPF_A1 :
                        return CAIRO_FORMAT_A1;
                        break;
                case DSPF_UNKNOWN :
                case DSPF_ARGB1555 :
                case DSPF_RGB16 :
                case DSPF_YUY2 :
                case DSPF_RGB332 :
                case DSPF_UYVY :
                case DSPF_I420 :
                case DSPF_ALUT44 :
                case DSPF_NV12 :
                case DSPF_NV16 :
                default :
                	return DFB_UNSUPPORTED;
	}
}


static IDirectFBSurface *cairo_directfb_buffer_surface_create(IDirectFB *dfb,void *data,int pitch, int format,
				int width, int height) {

		DFBResult  ret;
		IDirectFBSurface      *buffer;
		DFBSurfaceDescription  dsc;

		dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT; 
		dsc.caps = DSCAPS_NONE;
		dsc.width       = width;
		dsc.height      = height;
		dsc.pixelformat = format;
		if( data ) {
				dsc.flags |= DSDESC_CAPS;
				dsc.flags |= DSDESC_PREALLOCATED;
				dsc.caps = DSCAPS_NONE;
				dsc.preallocated[0].data = data;
				dsc.preallocated[0].pitch = pitch;
				dsc.preallocated[1].data = NULL;
				dsc.preallocated[1].pitch = 0;
		}

		ret =dfb->CreateSurface (dfb, &dsc, &buffer);
		if (ret) {
				DirectFBError ("cairo_directfb_buffer_surface_create failed ", ret);
				assert(0);
				return NULL;
		}
#if 0
		{
				DFBSurfacePixelFormat dfbformat;
				int nwidth=0;
				int nheight=0;
				int npitch;
				void *ndata;
				buffer->GetSize(buffer,&nwidth,&nheight);
				buffer->GetPixelFormat(buffer,&dfbformat);
				buffer->Lock(buffer,DSLF_READ,&ndata,&npitch);
				buffer->Unlock(buffer);
				assert( ( nwidth == width) && (nheight == height));
		}	
#endif
		return buffer;
}

static cairo_status_t
_cairo_directfb_surface_get_image (cairo_directfb_surface_t *surface,
				cairo_rectangle_t     *interest,
				cairo_image_surface_t **image_out,
				cairo_rectangle_t     *rect_out,
				DFBSurfaceLockFlags flags)
{
		int pitch;
		void *data;

		(void)interest;
		if( surface->buffer->Lock(surface->buffer,flags,&data,&pitch) != DFB_OK ) 
		return CAIRO_STATUS_NO_MEMORY;

		/*lock the dest agianst other changes*/
		if( surface->buffer_image ) { 
			if( surface->buffer_data == data ) {
				cairo_surface_reference(surface->buffer_image);
			}
		} 

		if( surface->buffer_data != data ){
			/* new off screen buffer */
			int width;
			int height;
			DFBSurfacePixelFormat dfbformat;
			/*surface moved free image if allocated */
			if( surface->buffer_image ) {
			cairo_surface_destroy (surface->buffer_image);
			surface->buffer_image=NULL;
			surface->buffer_data=NULL;
			}
			surface->buffer->Unlock(surface->buffer);
		

			surface->main_surface->GetSize(surface->main_surface,&width,&height);
			surface->main_surface->GetPixelFormat(surface->main_surface,&dfbformat);
			surface->format = directfb_to_cairo_format(dfbformat);

			if( surface->format == DFB_UNSUPPORTED ) {
				surface->format = CAIRO_FORMAT_ARGB32;
				surface->buffer = cairo_directfb_buffer_surface_create(surface->dfb,NULL,
											0,DSPF_ARGB,width,height);
				if( !surface->buffer ) 
				return CAIRO_STATUS_NO_MEMORY;
				/*Have to flip the main surface if its double buffered to blit the buffer*/
				surface->main_surface->Flip(surface->main_surface,NULL,0);
				surface->buffer->Blit(surface->buffer,surface->main_surface,NULL,0,0);
			}else {
				surface->buffer = surface->main_surface;
			}

			surface->width=width;
			surface->height=height;

			if( surface->buffer->Lock(surface->buffer,flags,&data,&pitch) != DFB_OK ) 
			return CAIRO_STATUS_NO_MEMORY;

			surface->buffer_data = data;
			surface->buffer_image = cairo_image_surface_create_for_data (
				(unsigned char *)data,surface->format,width, height, pitch);
			if( surface->buffer_image == NULL ) {
			surface->buffer->Release(surface->buffer);
			surface->buffer = NULL;
			surface->buffer_data = NULL;
			return CAIRO_STATUS_NO_MEMORY;
			}
			#if 0
			if( surface->clip) 
				_cairo_image_surface_set_clip_region (
					(cairo_image_surface_t *)surface->buffer_image,surface->clip);
			#endif
		}

		if (rect_out) {
				rect_out->x = 0;
				rect_out->y = 0;
				rect_out->width = surface->width;
				rect_out->height = surface->height;
		}
		if( image_out )
		*image_out=(cairo_image_surface_t *)surface->buffer_image;
		return CAIRO_STATUS_SUCCESS;
}


static cairo_surface_t *
_cairo_directfb_surface_create_similar (void *abstract_src,
				cairo_content_t content,
				int        width,
				int        height)
{
   	cairo_format_t format = _cairo_format_from_content (content);
	int dfbformat;
	switch( content ) {
		case CAIRO_CONTENT_COLOR:
			dfbformat=DSPF_ARGB;
		break;
		case CAIRO_CONTENT_ALPHA:
			dfbformat=DSPF_A8;
		break;
		case CAIRO_CONTENT_COLOR_ALPHA:
			dfbformat= DSPF_ARGB;
		break;
		default:
		{
			return cairo_image_surface_create (format, width, height);
		}
	}
	cairo_directfb_surface_t *src = abstract_src;
	IDirectFBSurface *buffer =cairo_directfb_buffer_surface_create(src->dfb,
					NULL,0,dfbformat,width,height);
	cairo_surface_t *sur =cairo_directfb_surface_create (src->dfb,buffer);
	((cairo_directfb_surface_t *)sur)->format = format;
	return sur;
}


static cairo_status_t
_cairo_directfb_surface_finish (void *data ) {
		cairo_directfb_surface_t *surface=(cairo_directfb_surface_t *)data;
		if( surface->buffer_image ) 
		cairo_surface_destroy (surface->buffer_image);
		if (surface->clip)
	    pixman_region_destroy (surface->clip);

		if( surface->main_surface != surface->buffer ) {
				surface->main_surface->SetClip (surface->main_surface,NULL);
				surface->main_surface->Blit(surface->main_surface,surface->buffer,NULL,0,0);
				surface->buffer->Release (surface->buffer);
				surface->buffer=NULL;
		}
#if 0 /* No don't do this */
		surface->main_surface->Flip(surface->main_surface,NULL,0);
#endif
		surface->main_surface->Release (surface->main_surface);
		surface->main_surface=NULL;
		surface->dfb->Release(surface->dfb);	
		surface->dfb=NULL;
		return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_directfb_surface_acquire_source_image (void  *abstract_surface,
				cairo_image_surface_t **image_out,
				void                  **image_extra)
{
		cairo_directfb_surface_t *surface = abstract_surface;
		*image_extra = surface;
		return _cairo_directfb_surface_get_image (surface, NULL,image_out,NULL,DSLF_READ);
}

		static void
_cairo_directfb_surface_release_source_image (void  *abstract_surface,
				cairo_image_surface_t *image,
				void                  *image_extra)
{
		cairo_directfb_surface_t *surface = abstract_surface;
		surface->buffer->Unlock(surface->buffer);
}

		static cairo_status_t
_cairo_directfb_surface_acquire_dest_image (void                *abstract_surface,
				cairo_rectangle_t     *interest_rect,
				cairo_image_surface_t **image_out,
				cairo_rectangle_t     *image_rect_out,
				void                  **image_extra)
{
		cairo_directfb_surface_t *surface = abstract_surface;
		*image_extra = interest_rect;
		return  _cairo_directfb_surface_get_image (surface, interest_rect,image_out,
						image_rect_out,DSLF_READ|DSLF_WRITE);
}

static void
_cairo_directfb_surface_release_dest_image (void  *abstract_surface,
				cairo_rectangle_t     *interest_rect,
				cairo_image_surface_t *image,
				cairo_rectangle_t     *image_rect,
				void                  *image_extra)
{
		cairo_directfb_surface_t *surface = abstract_surface;
		IDirectFBSurface *buffer=surface->buffer;
		buffer->Unlock(buffer);
}

static cairo_status_t
_cairo_directfb_surface_clone_similar (void	    *abstract_surface,
				cairo_surface_t *src,
				cairo_surface_t **clone_out)
{
		cairo_directfb_surface_t *surface = abstract_surface;
		cairo_directfb_surface_t *clone;

		if (src->backend == surface->base.backend) {
				*clone_out = src;
				cairo_surface_reference (src);
				return CAIRO_STATUS_SUCCESS;
		} else if (_cairo_surface_is_image (src)) {
				cairo_image_surface_t *image_src = (cairo_image_surface_t *) src;
				clone = (cairo_directfb_surface_t *)
						_cairo_directfb_surface_create_similar (surface, image_src->format,
										image_src->width,
										image_src->height);
				if (!clone)
						return CAIRO_STATUS_NO_MEMORY;
				IDirectFBSurface *tmpbuffer;
				int format = cairo_to_directfb_format(image_src->format);
				tmpbuffer =cairo_directfb_buffer_surface_create(surface->dfb,
								(void *)image_src->data,image_src->stride,format,
								image_src->width,image_src->height);

				if (!tmpbuffer){
						cairo_surface_destroy((cairo_surface_t *)surface);
						return CAIRO_STATUS_NO_MEMORY;
				}
				clone->buffer->Blit(clone->buffer,tmpbuffer,NULL,0,0);
				tmpbuffer->Release(tmpbuffer);
				*clone_out = &clone->base;
				return CAIRO_STATUS_SUCCESS;
		}
		return CAIRO_INT_STATUS_UNSUPPORTED;
}

#if DFB_COMPOSITE
static cairo_int_status_t
_cairo_directfb_surface_composite (cairo_operator_t op,
				cairo_pattern_t  *src_pattern,
				cairo_pattern_t  *mask_pattern,
				void         *abstract_dst,
				int      src_x,
				int      src_y,
				int      mask_x,
				int      mask_y,
				int      dst_x,
				int      dst_y,
				unsigned int     width,
				unsigned int     height)
{
    cairo_directfb_surface_t *dst = abstract_dst;
    cairo_directfb_surface_t *src;
    cairo_surface_pattern_t *src_surface_pattern;
    int alpha;
    int integer_transform;
    int itx, ity;

	cairo_directfb_surface_t *surface = abstract_dst;
	if( _dfb_set_operator(op,surface->buffer) == DFB_UNSUPPORTED ) 
		return CAIRO_INT_STATUS_UNSUPPORTED;

    if (src_pattern->type == CAIRO_PATTERN_TYPE_SOLID ) { 

    } else if (src_pattern->type != CAIRO_PATTERN_TYPE_SURFACE ||
			src_pattern->extend != CAIRO_EXTEND_NONE) {
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}

    if (mask_pattern) {
	/* FIXME: When we fully support RENDER style 4-channel
	 * masks we need to check r/g/b != 1.0.
	 */
	if (mask_pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

		alpha = ((cairo_solid_pattern_t *)mask_pattern)->color.alpha_short >> 8;
    } else {
    	alpha = 255;
    }

    src_surface_pattern = (cairo_surface_pattern_t *)src_pattern;
    src = (cairo_directfb_surface_t *)src_surface_pattern->surface;

    if (src->base.backend != dst->base.backend){
	return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    
    integer_transform = _cairo_matrix_is_integer_translation (&src_pattern->matrix, &itx, &ity);
    if (!integer_transform) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
	}
	surface->buffer->SetPorterDuff(surface->buffer,DSPD_NONE);
	return CAIRO_INT_STATUS_UNSUPPORTED;
}
#endif

static cairo_int_status_t
_cairo_directfb_surface_fill_rectangles (void		  *abstract_surface,
				cairo_operator_t	  op,
				const cairo_color_t *color,
				cairo_rectangle_t	  *rects,
				int		  n_rects)
{
	int i,k;
	cairo_directfb_surface_t *surface = abstract_surface;
	IDirectFBSurface *buffer = surface->buffer;

	if( _dfb_set_operator(op,buffer) == DFB_UNSUPPORTED ) {
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	buffer->SetColor(buffer,color->red_short >> 8,
				  color->green_short >> 8, 
				  color->blue_short >> 8, 
				  color->alpha_short >> 8 );
	/*Not optimized not sure of the sorting on region*/
	if( surface->clip ) {
		DFBRegion region;	
    	int n_boxes = pixman_region_num_rects (surface->clip);
    	pixman_box16_t *boxes = pixman_region_rects (surface->clip);
		for( k = 0; k < n_boxes; k++ ) {  
        	region.x1 = boxes[k].x1;
        	region.y1 = boxes[k].y1;
        	region.x2 = boxes[k].x2;
        	region.y2 = boxes[k].y2;
			buffer->SetClip (buffer,&region);
    		for (i = 0; i < n_rects; i++) {
				buffer->FillRectangle(buffer,rects[i].x,rects[i].y,
					rects[i].width,rects[i].height);
			}
		}
		buffer->SetClip (buffer, NULL);
	}else {
		buffer->SetClip (buffer, NULL);
    	for (i = 0; i < n_rects; i++) {
			buffer->FillRectangle(buffer,rects[i].x,rects[i].y,
				rects[i].width,rects[i].height);
		}
	}
	return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_directfb_surface_set_clip_region (void		*abstract_surface,
				pixman_region16_t *region)
{
	cairo_directfb_surface_t *surface = abstract_surface;
	if( region == NULL ) {
		pixman_region_destroy (surface->clip);
		surface->clip = NULL;
	}else {
		if (!surface->clip) {
	    	surface->clip = pixman_region_create ();
	   		 if (!surface->clip)
			return CAIRO_STATUS_NO_MEMORY;
		}
		pixman_region_copy (surface->clip, region);
	}
#if 0
	if( surface->buffer_image ) 
		_cairo_image_surface_set_clip_region (
			(cairo_image_surface_t *)surface->buffer_image,region);
#endif

	return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_directfb_abstract_surface_get_extents (void		    *abstract_surface,
				cairo_rectangle_t *rectangle)
{
		if( rectangle ) {
				cairo_directfb_surface_t *surface = abstract_surface;
				rectangle->x = 0;
				rectangle->y = 0;
				rectangle->width = surface->width;
				rectangle->height = surface->height;
		}
		return CAIRO_STATUS_SUCCESS;
}
static cairo_status_t
_cairo_directfb_surface_mark_dirty_rectangle (void * abstract_surface,
				int                    x,
				int                    y,
				int                    width,
				int                    height)
{
	cairo_directfb_surface_t *surface = abstract_surface;
	if( surface->main_surface != surface->buffer) {
		DFBRegion region;	
		region.x1=x;
		region.y1=y;
		region.x2=x+width;
		region.y2=y+height;
		surface->buffer->SetClip (surface->buffer,&region);
		surface->buffer->Blit(surface->buffer,surface->main_surface,NULL,0,0);
		surface->buffer->SetClip (surface->buffer,NULL);
	}
	return CAIRO_STATUS_SUCCESS;
}
static cairo_status_t 
_cairo_directfb_surface_flush (void  *abstract_surface)
{
		cairo_directfb_surface_t *surface = abstract_surface;
		if( surface->main_surface != surface->buffer) 
		surface->main_surface->Blit(surface->main_surface,surface->buffer,NULL,0,0);
		return CAIRO_STATUS_SUCCESS;
}

#if DFB_SHOW_GLYPHS
static void
_cairo_directfb_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_directfb_surface_font_private_t *font_private;

    font_private = scaled_font->surface_private;
    if (font_private)
    {
    xxx_destroy (font_private);
    }
}
#endif

#if DFB_SHOW_GLYPHS
static void
_cairo_directfb_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
                    cairo_scaled_font_t  *scaled_font)
{
    cairo_directfb_surface_glyph_private_t *glyph_private;

    glyph_private = scaled_glyph->surface_private;
    if (glyph_private)
    {
        xxx_release(glyph_private);
    }
}
#endif


static const cairo_surface_backend_t cairo_directfb_surface_backend = {
		CAIRO_SURFACE_TYPE_DIRECTFB,    
		_cairo_directfb_surface_create_similar,
		_cairo_directfb_surface_finish,
		_cairo_directfb_surface_acquire_source_image,
		_cairo_directfb_surface_release_source_image,
		_cairo_directfb_surface_acquire_dest_image,
		_cairo_directfb_surface_release_dest_image,
		_cairo_directfb_surface_clone_similar,
#if DFB_COMPOSITE
		_cairo_directfb_surface_composite,
#else
		NULL,
#endif
		_cairo_directfb_surface_fill_rectangles,
		NULL,/*composite_trapezoids*/
		NULL, /* copy_page */
		NULL, /* show_page */
		_cairo_directfb_surface_set_clip_region,
		NULL, /* intersect_clip_path */
		_cairo_directfb_abstract_surface_get_extents,
#if DFB_SHOW_GLYPHS
		_cairo_directfb_surface_show_glyphs,
#else
		NULL,
#endif
		NULL, /* get_font_options */
		_cairo_directfb_surface_flush,
		_cairo_directfb_surface_mark_dirty_rectangle,
#if DFB_SHOW_GLYPHS
		_cairo_directfb_surface_scaled_font_fini,
		_cairo_directfb_surface_scaled_glyph_fini
#else
		NULL,
		NULL
#endif
};

#if DFB_SHOW_GLYPHS
static cairo_int_status_t
_cairo_directfb_surface_show_glyphs (cairo_scaled_font_t    *scaled_font,
                 cairo_operator_t       operator,
                 cairo_pattern_t        *pattern,
                 void               *abstract_surface,
                 int                    source_x,
                 int                    source_y,
                 int            dest_x,
                 int            dest_y,
                 unsigned int       width,
                 unsigned int       height,
                 const cairo_glyph_t    *glyphs,
                 int                    num_glyphs)
{
	int i;
	cairo_int_status_t status;
	cairo_directfb_surface_t *surface = abstract_surface;
	cairo_scaled_glyph_t *scaled_glyph;

    if ((scaled_font->surface_backend != NULL &&
     scaled_font->surface_backend != &cairo_directfb_surface_backend) )
    return CAIRO_INT_STATUS_UNSUPPORTED;

    if (scaled_font->surface_backend == NULL) {
		scaled_font->surface_backend = &cairo_directfb_surface_backend;
	}
   /* Send all unsent glyphs to the server */
    for (i = 0; i < num_glyphs; i++) {
        IDirectFBSurface *tmpbuffer;
		int x;
		int y;
		cairo_image_surface_t *glyph_img;
    	status = _cairo_scaled_glyph_lookup (scaled_font,
                         glyphs[i].index,
                         CAIRO_SCALED_GLYPH_INFO_SURFACE,
                         &scaled_glyph);
		glyph_img = scaled_glyph->surface;
    	if (status != CAIRO_STATUS_SUCCESS)
        return status;
		/* its a space or something*/
		if( !glyph_img->data ) {
			continue;
		}
    	if (scaled_glyph->surface_private == NULL  ) {
        int dfbformat = cairo_to_directfb_format(glyph_img->format);
		if( dfbformat == DSPF_UNKNOWN ) {
					printf(" BAD IMAGE FORMAT[%d] cf=%d dfbf=%d data=%p \n",i,glyph_img->format,dfbformat,glyph_img->data);
			continue;
		}
        	tmpbuffer = cairo_directfb_buffer_surface_create(surface->dfb,
        	(void *)glyph_img->data,glyph_img->stride,dfbformat,glyph_img->width,glyph_img->height);
			/*scaled_glyph->surface_private = tmpbuffer;*/
    	}else {
			tmpbuffer = (IDirectFBSurface *)scaled_glyph->surface_private;
		}
		if( !tmpbuffer ) {
			assert(0); /*something really bad happend*/
    		return CAIRO_INT_STATUS_UNSUPPORTED;
		}
   /* round glyph locations to the nearest pixel */
    x = (int) floor (glyphs[i].x +
             glyph_img->base.device_x_offset +
             0.5);
    y = (int) floor (glyphs[i].y +
             glyph_img->base.device_y_offset +
             0.5);
	x +=dest_x;
	y +=dest_y;

		printf(" IMAGE FORMAT[%d] src_x=%d src_y=%d cf=%d data=%p x=%d y=%d w=%d h=%d\n",i,source_x,source_y,glyph_img->format,glyph_img->data,x,y,glyph_img->width,glyph_img->height);
#if 0
		surface->buffer->SetColor(surface->buffer,0,0xff,0,0xff);
		surface->buffer->FillRectangle(surface->buffer,x,y,glyph_img->width,glyph_img->height);
		surface->buffer->SetColor(surface->buffer,0,0xff,0xff,0xff);
#endif
		surface->buffer->Blit(surface->buffer,tmpbuffer,NULL,x,y);
		
    }
	return CAIRO_INT_STATUS_UNSUPPORTED;
}
#endif



cairo_surface_t *
cairo_directfb_surface_create (IDirectFB *dfb,IDirectFBSurface *dfbsurface) 
{
		cairo_directfb_surface_t *surface = calloc(1,sizeof(cairo_directfb_surface_t));
		if( surface == NULL ) 
		return NULL;
		_cairo_surface_init (&surface->base, &cairo_directfb_surface_backend);
		/*Reference the surface */
		dfb->AddRef(dfb);	
		dfbsurface->AddRef(dfbsurface);	
		surface->dfb=dfb;
		surface->main_surface = dfbsurface;
		dfbsurface->GetSize(dfbsurface,&surface->width,&surface->height);
		surface->buffer = surface->main_surface;
		surface->format = DFB_UNSUPPORTED;
		surface->clip=NULL;
		return ((cairo_surface_t *)surface);
}

