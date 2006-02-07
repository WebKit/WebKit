/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 */

#include <stdlib.h>

#include "cairoint.h"
#include "cairo-surface-fallback-private.h"
#include "cairo-clip-private.h"

const cairo_surface_t _cairo_surface_nil = {
    &cairo_image_surface_backend,	/* backend */
    -1,					/* ref_count */
    CAIRO_STATUS_NO_MEMORY,		/* status */
    FALSE,				/* finished */
    { 0,	/* size */
      0,	/* num_elements */
      0,	/* element_size */
      NULL,	/* elements */
    },					/* user_data */
    0.0,				/* device_x_offset */
    0.0,				/* device_y_offset */
    0,					/* next_clip_serial */
    0					/* current_clip_serial */
};

const cairo_surface_t _cairo_surface_nil_file_not_found = {
    &cairo_image_surface_backend,	/* backend */
    -1,					/* ref_count */
    CAIRO_STATUS_FILE_NOT_FOUND,	/* status */
    FALSE,				/* finished */
    { 0,	/* size */
      0,	/* num_elements */
      0,	/* element_size */
      NULL,	/* elements */
    },					/* user_data */
    0.0,				/* device_x_offset */
    0.0,				/* device_y_offset */
    0,					/* next_clip_serial */
    0					/* current_clip_serial */
};

const cairo_surface_t _cairo_surface_nil_read_error = {
    &cairo_image_surface_backend,	/* backend */
    -1,					/* ref_count */
    CAIRO_STATUS_READ_ERROR,		/* status */
    FALSE,				/* finished */
    { 0,	/* size */
      0,	/* num_elements */
      0,	/* element_size */
      NULL,	/* elements */
    },					/* user_data */
    0.0,				/* device_x_offset */
    0.0,				/* device_y_offset */
    0,					/* next_clip_serial */
    0					/* current_clip_serial */
};

/* N.B.: set_device_offset already transforms the device offsets by the scale
 * before storing in device_[xy]_scale
 */

/* Helper macros for transforming surface coords to backend coords */
#define BACKEND_X(_surf, _sx)  ((_sx)*((_surf)->device_x_scale)+((_surf)->device_x_offset))
#define BACKEND_Y(_surf, _sy)  ((_sy)*((_surf)->device_y_scale)+((_surf)->device_y_offset))
#define BACKEND_X_SIZE(_surf, _sx)  ((_sx)*((_surf)->device_x_scale))
#define BACKEND_Y_SIZE(_surf, _sy)  ((_sy)*((_surf)->device_y_scale))

/* Helper macros for transforming backend coords to surface coords */
#define SURFACE_X(_surf, _bx)  (((_bx)-((_surf)->device_x_offset))/((_surf)->device_x_scale))
#define SURFACE_Y(_surf, _by)  (((_by)-((_surf)->device_y_offset))/((_surf)->device_y_scale))
#define SURFACE_X_SIZE(_surf, _bx)  ((_bx)/((_surf)->device_x_scale))
#define SURFACE_Y_SIZE(_surf, _by)  ((_by)/((_surf)->device_y_scale))

/**
 * _cairo_surface_set_error:
 * @surface: a surface
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 * 
 * Sets surface->status to @status and calls _cairo_error;
 *
 * All assignments of an error status to surface->status should happen
 * through _cairo_surface_set_error() or else _cairo_error() should be
 * called immediately after the assignment.
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
static void
_cairo_surface_set_error (cairo_surface_t *surface,
			  cairo_status_t status)
{
    /* Don't overwrite an existing error. This preserves the first
     * error, which is the most significant. It also avoids attempting
     * to write to read-only data (eg. from a nil surface). */
    if (surface->status == CAIRO_STATUS_SUCCESS)
	surface->status = status;

    _cairo_error (status);
}

/**
 * cairo_surface_status:
 * @surface: a #cairo_surface_t
 * 
 * Checks whether an error has previously occurred for this
 * surface.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, %CAIRO_STATUS_NULL_POINTER,
 * %CAIRO_STATUS_NO_MEMORY, %CAIRO_STATUS_READ_ERROR,
 * %CAIRO_STATUS_INVALID_CONTENT, %CAIRO_STATUS_INVALUE_FORMAT, or
 * %CAIRO_STATUS_INVALID_VISUAL.
 **/
cairo_status_t
cairo_surface_status (cairo_surface_t *surface)
{
    return surface->status;
}

void
_cairo_surface_init (cairo_surface_t			*surface,
		     const cairo_surface_backend_t	*backend)
{
    surface->backend = backend;

    surface->ref_count = 1;
    surface->status = CAIRO_STATUS_SUCCESS;
    surface->finished = FALSE;

    _cairo_user_data_array_init (&surface->user_data);

    surface->device_x_offset = 0.0;
    surface->device_y_offset = 0.0;
    surface->device_x_scale = 1.0;
    surface->device_y_scale = 1.0;

    surface->clip = NULL;
    surface->next_clip_serial = 0;
    surface->current_clip_serial = 0;

    surface->is_snapshot = FALSE;
}

cairo_surface_t *
_cairo_surface_create_similar_scratch (cairo_surface_t *other,
				       cairo_content_t	content,
				       int		width,
				       int		height)
{
    cairo_format_t format = _cairo_format_from_content (content);

    if (other->status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    if (other->backend->create_similar)
	return other->backend->create_similar (other, content, width, height);
    else
	return cairo_image_surface_create (format, width, height);
}

/**
 * cairo_surface_create_similar:
 * @other: an existing surface used to select the backend of the new surface
 * @content: the content for the new surface
 * @width: width of the new surface, (in device-space units)
 * @height: height of the new surface (in device-space units)
 * 
 * Create a new surface that is as compatible as possible with an
 * existing surface. The new surface will use the same backend as
 * @other unless that is not possible for some reason.
 * 
 * Return value: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if @other is already in an error state
 * or any other error occurs.
 **/
cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t  *other,
			      cairo_content_t	content,
			      int		width,
			      int		height)
{
    if (other->status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    if (! CAIRO_CONTENT_VALID (content)) {
	_cairo_error (CAIRO_STATUS_INVALID_CONTENT);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_surface_create_similar_solid (other, content,
						width, height,
						CAIRO_COLOR_TRANSPARENT);
}

cairo_surface_t *
_cairo_surface_create_similar_solid (cairo_surface_t	 *other,
				     cairo_content_t	  content,
				     int		  width,
				     int		  height,
				     const cairo_color_t *color)
{
    cairo_status_t status;
    cairo_surface_t *surface;
    cairo_pattern_t *source;

    surface = _cairo_surface_create_similar_scratch (other, content,
						     width, height);
    if (surface->status) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    source = _cairo_pattern_create_solid (color);
    if (source->status) {
	cairo_surface_destroy (surface);
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    status = _cairo_surface_paint (surface, CAIRO_OPERATOR_SOURCE, source);

    cairo_pattern_destroy (source);
    
    if (status) {
	cairo_surface_destroy (surface);
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return surface;
}

cairo_clip_mode_t
_cairo_surface_get_clip_mode (cairo_surface_t *surface)
{
    if (surface->backend->intersect_clip_path != NULL)
	return CAIRO_CLIP_MODE_PATH;
    else if (surface->backend->set_clip_region != NULL)
	return CAIRO_CLIP_MODE_REGION;
    else
	return CAIRO_CLIP_MODE_MASK;
}

/**
 * cairo_surface_reference:
 * @surface: a #cairo_surface_t
 * 
 * Increases the reference count on @surface by one. This prevents
 * @surface from being destroyed until a matching call to
 * cairo_surface_destroy() is made.
 *
 * Return value: the referenced #cairo_surface_t.
 **/
cairo_surface_t *
cairo_surface_reference (cairo_surface_t *surface)
{
    if (surface == NULL)
	return NULL;

    if (surface->ref_count == (unsigned int)-1)
	return surface;

    assert (surface->ref_count > 0);

    surface->ref_count++;

    return surface;
}

/**
 * cairo_surface_destroy:
 * @surface: a #cairo_t
 * 
 * Decreases the reference count on @surface by one. If the result is
 * zero, then @surface and all associated resources are freed.  See
 * cairo_surface_reference().
 **/
void
cairo_surface_destroy (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    if (surface->ref_count == (unsigned int)-1)
	return;

    assert (surface->ref_count > 0);

    surface->ref_count--;
    if (surface->ref_count)
	return;

    cairo_surface_finish (surface);

    _cairo_user_data_array_fini (&surface->user_data);

    free (surface);
}
slim_hidden_def(cairo_surface_destroy);

/**
 * cairo_surface_finish:
 * @surface: the #cairo_surface_t to finish
 * 
 * This function finishes the surface and drops all references to
 * external resources.  For example, for the Xlib backend it means
 * that cairo will no longer access the drawable, which can be freed.
 * After calling cairo_surface_finish() the only valid operations on a
 * surface are getting and setting user data and referencing and
 * destroying it.  Further drawing to the surface will not affect the
 * surface but will instead trigger a CAIRO_STATUS_SURFACE_FINISHED
 * error.
 *
 * When the last call to cairo_surface_destroy() decreases the
 * reference count to zero, cairo will call cairo_surface_finish() if
 * it hasn't been called already, before freeing the resources
 * associated with the surface.
 **/
void
cairo_surface_finish (cairo_surface_t *surface)
{
    cairo_status_t status;

    if (surface->finished) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_FINISHED);
	return;
    }

    if (surface->backend->finish == NULL) {
	surface->finished = TRUE;
	return;
    }

    if (!surface->status && surface->backend->flush) {
	status = surface->backend->flush (surface);
	if (status) {
	    _cairo_surface_set_error (surface, status);
	    return;
	}
    }

    status = surface->backend->finish (surface);
    if (status) {
	_cairo_surface_set_error (surface, status);
	return;
    }

    surface->finished = TRUE;
}

/**
 * cairo_surface_get_user_data:
 * @surface: a #cairo_surface_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 * 
 * Return user data previously attached to @surface using the specified
 * key.  If no user data has been attached with the given key this
 * function returns %NULL.
 * 
 * Return value: the user data previously attached or %NULL.
 **/
void *
cairo_surface_get_user_data (cairo_surface_t		 *surface,
			     const cairo_user_data_key_t *key)
{
    return _cairo_user_data_array_get_data (&surface->user_data,
					    key);
}

/**
 * cairo_surface_set_user_data:
 * @surface: a #cairo_surface_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach to the surface
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * surface is destroyed or when new user data is attached using the
 * same key.
 * 
 * Attach user data to @surface.  To remove user data from a surface,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 **/
cairo_status_t
cairo_surface_set_user_data (cairo_surface_t		 *surface,
			     const cairo_user_data_key_t *key,
			     void			 *user_data,
			     cairo_destroy_func_t	 destroy)
{
    if (surface->ref_count == -1)
	return CAIRO_STATUS_NO_MEMORY;
    
    return _cairo_user_data_array_set_data (&surface->user_data,
					    key, user_data, destroy);
}

/**
 * cairo_surface_get_font_options:
 * @surface: a #cairo_surface_t
 * @options: a #cairo_font_options_t object into which to store
 *   the retrieved options. All existing values are overwritten
 * 
 * Retrieves the default font rendering options for the surface.
 * This allows display surfaces to report the correct subpixel order
 * for rendering on them, print surfaces to disable hinting of
 * metrics and so forth. The result can then be used with
 * cairo_scaled_font_create().
 **/
void
cairo_surface_get_font_options (cairo_surface_t       *surface,
				cairo_font_options_t  *options)
{
    if (!surface->finished && surface->backend->get_font_options) {
	surface->backend->get_font_options (surface, options);
    } else {
	_cairo_font_options_init_default (options);
    }
}

/**
 * cairo_surface_flush:
 * @surface: a #cairo_surface_t
 * 
 * Do any pending drawing for the surface and also restore any
 * temporary modification's cairo has made to the surface's
 * state. This function must be called before switching from
 * drawing on the surface with cairo to drawing on it directly
 * with native APIs. If the surface doesn't support direct access,
 * then this function does nothing.
 **/
void
cairo_surface_flush (cairo_surface_t *surface)
{
    if (surface->status)
	return;

    if (surface->finished) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_FINISHED);
	return;
    }

    if (surface->backend->flush) {
	cairo_status_t status;

	status = surface->backend->flush (surface);
	
	if (status)
	    _cairo_surface_set_error (surface, status);
    }
}

/**
 * cairo_surface_mark_dirty:
 * @surface: a #cairo_surface_t
 *
 * Tells cairo that drawing has been done to surface using means other
 * than cairo, and that cairo should reread any cached areas. Note
 * that you must call cairo_surface_flush() before doing such drawing.
 */
void
cairo_surface_mark_dirty (cairo_surface_t *surface)
{
    assert (! surface->is_snapshot);

    cairo_surface_mark_dirty_rectangle (surface, 0, 0, -1, -1);
}

/**
 * cairo_surface_mark_dirty_rectangle:
 * @surface: a #cairo_surface_t
 * @x: X coordinate of dirty rectangle
 * @y: Y coordinate of dirty rectangle
 * @width: width of dirty rectangle
 * @height: height of dirty rectangle
 *
 * Like cairo_surface_mark_dirty(), but drawing has been done only to
 * the specified rectangle, so that cairo can retain cached contents
 * for other parts of the surface.
 *
 * Any cached clip set on the surface will be reset by this function,
 * to make sure that future cairo calls have the clip set that they
 * expect.
 */
void
cairo_surface_mark_dirty_rectangle (cairo_surface_t *surface,
				    int              x,
				    int              y,
				    int              width,
				    int              height)
{
    assert (! surface->is_snapshot);

    if (surface->status)
	return;

    if (surface->finished) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_FINISHED);
	return;
    }

    /* Always reset the clip here, to avoid having a SaveDC/RestoreDC around
     * cairo calls that update the surface clip resulting in a desync between
     * the cairo clip and the backend clip.
     */
    surface->current_clip_serial = -1;

    if (surface->backend->mark_dirty_rectangle) {
	cairo_status_t status;
	
	status = surface->backend->mark_dirty_rectangle (surface,
                                                         BACKEND_X(surface, x),
                                                         BACKEND_Y(surface, y),
							 width, height);
	
	if (status)
	    _cairo_surface_set_error (surface, status);
    }
}

/**
 * cairo_surface_set_device_offset:
 * @surface: a #cairo_surface_t
 * @x_offset: the offset in the X direction, in device units
 * @y_offset: the offset in the Y direction, in device units
 * 
 * Sets an offset that is added to the device coordinates determined
 * by the CTM when drawing to @surface. One use case for this function
 * is when we want to create a #cairo_surface_t that redirects drawing
 * for a portion of an onscreen surface to an offscreen surface in a
 * way that is completely invisible to the user of the cairo
 * API. Setting a transformation via cairo_translate() isn't
 * sufficient to do this, since functions like
 * cairo_device_to_user() will expose the hidden offset.
 *
 * Note that the offset only affects drawing to the surface, not using
 * the surface in a surface pattern.
 **/
void
cairo_surface_set_device_offset (cairo_surface_t *surface,
				 double           x_offset,
				 double           y_offset)
{
    assert (! surface->is_snapshot);

    if (surface->status)
	return;

    if (surface->finished) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_FINISHED);
	return;
    }

    surface->device_x_offset = x_offset * surface->device_x_scale;
    surface->device_y_offset = y_offset * surface->device_y_scale;
}

/**
 * _cairo_surface_acquire_source_image:
 * @surface: a #cairo_surface_t
 * @image_out: location to store a pointer to an image surface that
 *    has identical contents to @surface. This surface could be @surface
 *    itself, a surface held internal to @surface, or it could be a new
 *    surface with a copy of the relevant portion of @surface.
 * @image_extra: location to store image specific backend data
 * 
 * Gets an image surface to use when drawing as a fallback when drawing with
 * @surface as a source. _cairo_surface_release_source_image() must be called
 * when finished.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if a an image was stored in @image_out.
 * %CAIRO_INT_STATUS_UNSUPPORTED if an image cannot be retrieved for the specified
 * surface. Or %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
_cairo_surface_acquire_source_image (cairo_surface_t         *surface,
				     cairo_image_surface_t  **image_out,
				     void                   **image_extra)
{
    assert (!surface->finished);

    return surface->backend->acquire_source_image (surface,
						   image_out, image_extra);
}

/**
 * _cairo_surface_release_source_image:
 * @surface: a #cairo_surface_t
 * @image_extra: same as return from the matching _cairo_surface_acquire_source_image()
 * 
 * Releases any resources obtained with _cairo_surface_acquire_source_image()
 **/
void
_cairo_surface_release_source_image (cairo_surface_t        *surface,
				     cairo_image_surface_t  *image,
				     void                   *image_extra)
{
    assert (!surface->finished);

    if (surface->backend->release_source_image)
	surface->backend->release_source_image (surface, image, image_extra);
}

/**
 * _cairo_surface_acquire_dest_image:
 * @surface: a #cairo_surface_t
 * @interest_rect: area of @surface for which fallback drawing is being done.
 *    A value of %NULL indicates that the entire surface is desired.
 *    XXXX I'd like to get rid of being able to pass NULL here (nothing seems to)
 * @image_out: location to store a pointer to an image surface that includes at least
 *    the intersection of @interest_rect with the visible area of @surface.
 *    This surface could be @surface itself, a surface held internal to @surface,
 *    or it could be a new surface with a copy of the relevant portion of @surface.
 *    If a new surface is created, it should have the same channels and depth
 *    as @surface so that copying to and from it is exact.
 * @image_rect: location to store area of the original surface occupied 
 *    by the surface stored in @image.
 * @image_extra: location to store image specific backend data
 * 
 * Retrieves a local image for a surface for implementing a fallback drawing
 * operation. After calling this function, the implementation of the fallback
 * drawing operation draws the primitive to the surface stored in @image_out
 * then calls _cairo_surface_release_dest_image(),
 * which, if a temporary surface was created, copies the bits back to the
 * main surface and frees the temporary surface.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY.
 *  %CAIRO_INT_STATUS_UNSUPPORTED can be returned but this will mean that
 *  the backend can't draw with fallbacks. It's possible for the routine
 *  to store NULL in @local_out and return %CAIRO_STATUS_SUCCESS;
 *  that indicates that no part of @interest_rect is visible, so no drawing
 *  is necessary. _cairo_surface_release_dest_image() should not be called in that
 *  case.
 **/
cairo_status_t
_cairo_surface_acquire_dest_image (cairo_surface_t         *surface,
				   cairo_rectangle_t       *interest_rect,
				   cairo_image_surface_t  **image_out,
				   cairo_rectangle_t       *image_rect,
				   void                   **image_extra)
{
    cairo_rectangle_t dev_interest_rect;
    cairo_status_t status;

    assert (!surface->finished);

    if (interest_rect) {
	dev_interest_rect = *interest_rect;
        dev_interest_rect.x = BACKEND_X(surface, dev_interest_rect.x);
        dev_interest_rect.y = BACKEND_Y(surface, dev_interest_rect.y);
    }

    status = surface->backend->acquire_dest_image (surface,
						   interest_rect ? &dev_interest_rect : NULL,
						   image_out, image_rect, image_extra);

    /* move image_rect back into surface coordinates from backend device coordinates */
    image_rect->x = SURFACE_X(surface, image_rect->x);
    image_rect->y = SURFACE_Y(surface, image_rect->y);

    return status;
}

/**
 * _cairo_surface_release_dest_image:
 * @surface: a #cairo_surface_t
 * @interest_rect: same as passed to the matching _cairo_surface_acquire_dest_image()
 * @image: same as returned from the matching _cairo_surface_acquire_dest_image()
 * @image_rect: same as returned from the matching _cairo_surface_acquire_dest_image()
 * @image_extra: same as return from the matching _cairo_surface_acquire_dest_image()
 * 
 * Finishes the operation started with _cairo_surface_acquire_dest_image(), by, if
 * necessary, copying the image from @image back to @surface and freeing any
 * resources that were allocated.
 **/
void
_cairo_surface_release_dest_image (cairo_surface_t        *surface,
				   cairo_rectangle_t      *interest_rect,
				   cairo_image_surface_t  *image,
				   cairo_rectangle_t      *image_rect,
				   void                   *image_extra)
{
    cairo_rectangle_t dev_interest_rect;

    assert (!surface->finished);

    /* move image_rect into backend device coords (opposite of acquire_dest_image) */
    image_rect->x = BACKEND_X(surface, image_rect->x);
    image_rect->y = BACKEND_Y(surface, image_rect->y);

    if (interest_rect) {
	dev_interest_rect = *interest_rect;
        dev_interest_rect.x = BACKEND_X(surface, dev_interest_rect.x);
        dev_interest_rect.y = BACKEND_Y(surface, dev_interest_rect.y);
    }

    if (surface->backend->release_dest_image)
	surface->backend->release_dest_image (surface, &dev_interest_rect,
					      image, image_rect, image_extra);
}

/**
 * _cairo_surface_clone_similar:
 * @surface: a #cairo_surface_t
 * @src: the source image
 * @clone_out: location to store a surface compatible with @surface
 *   and with contents identical to @src. The caller must call
 *   cairo_surface_destroy() on the result.
 * 
 * Creates a surface with contents identical to @src but that
 *   can be used efficiently with @surface. If @surface and @src are
 *   already compatible then it may return a new reference to @src.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if a surface was created and stored
 *   in @clone_out. Otherwise %CAIRO_INT_STATUS_UNSUPPORTED or another
 *   error like %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
_cairo_surface_clone_similar (cairo_surface_t  *surface,
			      cairo_surface_t  *src,
			      cairo_surface_t **clone_out)
{
    cairo_status_t status;
    cairo_image_surface_t *image;
    void *image_extra;
    
    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (surface->backend->clone_similar == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
      
    status = surface->backend->clone_similar (surface, src, clone_out);
    if (status == CAIRO_STATUS_SUCCESS) {
        (*clone_out)->device_x_offset = src->device_x_offset;
        (*clone_out)->device_y_offset = src->device_y_offset;
        (*clone_out)->device_x_scale = src->device_x_scale;
        (*clone_out)->device_y_scale = src->device_y_scale;
    }

    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    status = _cairo_surface_acquire_source_image (src, &image, &image_extra);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    status = surface->backend->clone_similar (surface, &image->base, clone_out);
    if (status == CAIRO_STATUS_SUCCESS) {
        (*clone_out)->device_x_offset = src->device_x_offset;
        (*clone_out)->device_y_offset = src->device_y_offset;
        (*clone_out)->device_x_scale = src->device_x_scale;
        (*clone_out)->device_y_scale = src->device_y_scale;
    }

    /* If the above failed point, we could implement a full fallback
     * using acquire_dest_image, but that's going to be very
     * inefficient compared to a backend-specific implementation of
     * clone_similar() with an image source. So we don't bother
     */
    
    _cairo_surface_release_source_image (src, image, image_extra);
    return status;
}

/* XXX: Shouldn't really need to do this here. */
#include "cairo-meta-surface-private.h"

/**
 * _cairo_surface_snapshot
 * @surface: a #cairo_surface_t
 *
 * Make an immutable copy of @surface. It is an error to call a
 * surface-modifying function on the result of this function.
 *
 * The caller owns the return value and should call
 * cairo_surface_destroy when finished with it. This function will not
 * return NULL, but will return a nil surface instead.
 *
 * Return value: The snapshot surface. Note that the return surface
 * may not necessarily be of the same type as @surface.
 **/
cairo_surface_t *
_cairo_surface_snapshot (cairo_surface_t *surface)
{
    if (surface->finished)
	return (cairo_surface_t *) &_cairo_surface_nil;

    if (surface->backend->snapshot)
	return surface->backend->snapshot (surface);

    return _cairo_surface_fallback_snapshot (surface);
}

cairo_status_t
_cairo_surface_composite (cairo_operator_t	op,
			  cairo_pattern_t	*src,
			  cairo_pattern_t	*mask,
			  cairo_surface_t	*dst,
			  int			src_x,
			  int			src_y,
			  int			mask_x,
			  int			mask_y,
			  int			dst_x,
			  int			dst_y,
			  unsigned int		width,
			  unsigned int		height)
{
    cairo_int_status_t status;

    assert (! dst->is_snapshot);

    if (mask) {
	/* These operators aren't interpreted the same way by the backends;
	 * they are implemented in terms of other operators in cairo-gstate.c
	 */
	assert (op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_CLEAR);
    }

    if (dst->status)
	return dst->status;
	
    if (dst->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (dst->backend->composite) {
        int backend_src_x = src_x;
        int backend_src_y = src_y;
        int backend_mask_x = mask_x;
        int backend_mask_y = mask_y;

        if (src->type == CAIRO_PATTERN_SURFACE) {
            cairo_surface_t *src_surface = ((cairo_surface_pattern_t*)src)->surface;
            backend_src_x = BACKEND_X(src_surface, src_x);
            backend_src_y = BACKEND_X(src_surface, src_y);
        }

        if (mask && mask->type == CAIRO_PATTERN_SURFACE) {
            cairo_surface_t *mask_surface = ((cairo_surface_pattern_t*)mask)->surface;
            backend_mask_x = BACKEND_X(mask_surface, mask_x);
            backend_mask_y = BACKEND_X(mask_surface, mask_y);
        }

	status = dst->backend->composite (op,
					  src, mask, dst,
                                          backend_src_x, backend_src_y,
                                          backend_mask_x, backend_mask_y,
                                          BACKEND_X(dst, dst_x),
                                          BACKEND_Y(dst, dst_y),
					  width, height);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_composite (op,
					      src, mask, dst,
					      src_x, src_y,
					      mask_x, mask_y,
					      dst_x, dst_y,
					      width, height);
}

/**
 * _cairo_surface_fill_rectangle:
 * @surface: a #cairo_surface_t
 * @op: the operator to apply to the rectangle
 * @color: the source color
 * @x: X coordinate of rectangle, in backend coordinates
 * @y: Y coordinate of rectangle, in backend coordinates
 * @width: width of rectangle, in backend coordinates
 * @height: height of rectangle, in backend coordinates
 * 
 * Applies an operator to a rectangle using a solid color as the source.
 * See _cairo_surface_fill_rectangles() for full details.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or the error that occurred
 **/
cairo_status_t
_cairo_surface_fill_rectangle (cairo_surface_t	   *surface,
			       cairo_operator_t	    op,
			       const cairo_color_t *color,
			       int		    x,
			       int		    y,
			       int		    width,
			       int		    height)
{
    cairo_rectangle_t rect;

    assert (! surface->is_snapshot);

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    return _cairo_surface_fill_rectangles (surface, op, color, &rect, 1);
}

/**
 * _cairo_surface_fill_region:
 * @surface: a #cairo_surface_t
 * @op: the operator to apply to the region
 * @color: the source color
 * @region: the region to modify, in backend coordinates
 * 
 * Applies an operator to a set of rectangles specified as a
 * #pixman_region16_t using a solid color as the source.
 * See _cairo_surface_fill_rectangles() for full details.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or the error that occurred
 **/
cairo_status_t
_cairo_surface_fill_region (cairo_surface_t	   *surface,
			    cairo_operator_t	    op,
			    const cairo_color_t    *color,
			    pixman_region16_t      *region)
{
    int num_rects = pixman_region_num_rects (region);
    pixman_box16_t *boxes = pixman_region_rects (region);
    cairo_rectangle_t *rects;
    cairo_status_t status;
    int i;

    assert (! surface->is_snapshot);

    if (!num_rects)
	return CAIRO_STATUS_SUCCESS;
    
    rects = malloc (sizeof (pixman_rectangle_t) * num_rects);
    if (!rects)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_rects; i++) {
	rects[i].x = boxes[i].x1;
	rects[i].y = boxes[i].y1;
	rects[i].width = boxes[i].x2 - boxes[i].x1;
	rects[i].height = boxes[i].y2 - boxes[i].y1;
    }

    status =  _cairo_surface_fill_rectangles (surface, op,
					      color, rects, num_rects);
    
    free (rects);

    return status;
}

/**
 * _cairo_surface_fill_rectangles:
 * @surface: a #cairo_surface_t
 * @op: the operator to apply to the region
 * @color: the source color
 * @rects: the rectangles to modify, in backend coordinates
 * @num_rects: the number of rectangles in @rects
 * 
 * Applies an operator to a set of rectangles using a solid color
 * as the source. Note that even if the operator is an unbounded operator
 * such as %CAIRO_OPERATOR_IN, only the given set of rectangles
 * is affected. This differs from _cairo_surface_composite_trapezoids()
 * where the entire destination rectangle is cleared.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or the error that occurred
 **/
cairo_status_t
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t	op,
				const cairo_color_t	*color,
				cairo_rectangle_t	*rects,
				int			num_rects)
{
    cairo_int_status_t status;
    cairo_rectangle_t *dev_rects = NULL;
    int i;

    assert (! surface->is_snapshot);

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;

    if (surface->backend->fill_rectangles) {
	if (surface->device_x_offset != 0.0 ||
	    surface->device_y_offset != 0.0 ||
	    surface->device_x_scale != 1.0 ||
	    surface->device_y_scale != 1.0)
	{
	    dev_rects = malloc(sizeof(cairo_rectangle_t) * num_rects);
	    for (i = 0; i < num_rects; i++) {
		dev_rects[i].x = BACKEND_X(surface, rects[i].x);
		dev_rects[i].y = BACKEND_Y(surface, rects[i].y);

		dev_rects[i].width = BACKEND_X_SIZE(surface, rects[i].width);
		dev_rects[i].height = BACKEND_Y_SIZE(surface, rects[i].height);
	    }
	}

	status = surface->backend->fill_rectangles (surface,
						    op,
						    color,
						    dev_rects ? dev_rects : rects, num_rects);
	free (dev_rects);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_fill_rectangles (surface, op, color,
						    rects, num_rects);
}

cairo_status_t
_cairo_surface_paint (cairo_surface_t	*surface,
		      cairo_operator_t	 op,
		      cairo_pattern_t	*source)
{
    cairo_status_t status;

    assert (! surface->is_snapshot);

    if (surface->backend->paint) {
	status = surface->backend->paint (surface, op, source);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_paint (surface, op, source);
}

cairo_status_t
_cairo_surface_mask (cairo_surface_t	*surface,
		     cairo_operator_t	 op,
		     cairo_pattern_t	*source,
		     cairo_pattern_t	*mask)
{
    cairo_status_t status;

    assert (! surface->is_snapshot);

    if (surface->backend->mask) {
	status = surface->backend->mask (surface, op, source, mask);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_mask (surface, op, source, mask);
}

cairo_status_t
_cairo_surface_stroke (cairo_surface_t		*surface,
		       cairo_operator_t		 op,
		       cairo_pattern_t		*source,
		       cairo_path_fixed_t	*path,
		       cairo_stroke_style_t	*stroke_style,
		       cairo_matrix_t		*ctm,
		       cairo_matrix_t		*ctm_inverse,
		       double			 tolerance,
		       cairo_antialias_t	 antialias)
{
    assert (! surface->is_snapshot);

    if (surface->backend->stroke) {
	cairo_status_t status;
	cairo_path_fixed_t *dev_path = path;
	cairo_path_fixed_t real_dev_path;

	if (surface->device_x_offset != 0.0 ||
	    surface->device_y_offset != 0.0 ||
	    surface->device_x_scale != 1.0 ||
	    surface->device_y_scale != 1.0)
        {
	    _cairo_path_fixed_init_copy (&real_dev_path, path);
	    _cairo_path_fixed_offset_and_scale (&real_dev_path,
						_cairo_fixed_from_double (surface->device_x_offset),
						_cairo_fixed_from_double (surface->device_y_offset),
						_cairo_fixed_from_double (surface->device_x_scale),
						_cairo_fixed_from_double (surface->device_y_scale));
	    dev_path = &real_dev_path;
	}

	status = surface->backend->stroke (surface, op, source,
					   dev_path, stroke_style,
					   ctm, ctm_inverse,
					   tolerance, antialias);

	if (dev_path == &real_dev_path)
	    _cairo_path_fixed_fini (&real_dev_path);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_stroke (surface, op, source,
					   path, stroke_style,
					   ctm, ctm_inverse,
					   tolerance, antialias);
}

cairo_status_t
_cairo_surface_fill (cairo_surface_t	*surface,
		     cairo_operator_t	 op,
		     cairo_pattern_t	*source,
		     cairo_path_fixed_t	*path,
		     cairo_fill_rule_t	 fill_rule,
		     double		 tolerance,
		     cairo_antialias_t	 antialias)
{
    cairo_status_t status;
    cairo_path_fixed_t *dev_path = path;
    cairo_path_fixed_t real_dev_path;

    assert (! surface->is_snapshot);

    if (surface->backend->fill) {
        if (surface->device_x_offset != 0.0 ||
            surface->device_y_offset != 0.0 ||
            surface->device_x_scale != 1.0 ||
            surface->device_y_scale != 1.0)
        {
            _cairo_path_fixed_init_copy (&real_dev_path, path);
            _cairo_path_fixed_offset_and_scale (&real_dev_path,
                                                _cairo_fixed_from_double (surface->device_x_offset),
                                                _cairo_fixed_from_double (surface->device_y_offset),
                                                _cairo_fixed_from_double (surface->device_x_scale),
                                                _cairo_fixed_from_double (surface->device_y_scale));
            dev_path = &real_dev_path;
        }

	status = surface->backend->fill (surface, op, source,
					 dev_path, fill_rule,
					 tolerance, antialias);

        if (dev_path == &real_dev_path)
            _cairo_path_fixed_fini (&real_dev_path);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_fill (surface, op, source,
					 path, fill_rule,
					 tolerance, antialias);
}
  
cairo_status_t
_cairo_surface_composite_trapezoids (cairo_operator_t		op,
				     cairo_pattern_t		*pattern,
				     cairo_surface_t		*dst,
				     cairo_antialias_t		antialias,
				     int			src_x,
				     int			src_y,
				     int			dst_x,
				     int			dst_y,
				     unsigned int		width,
				     unsigned int		height,
				     cairo_trapezoid_t		*traps,
				     int			num_traps)
{
    cairo_int_status_t status;
    cairo_trapezoid_t *dev_traps = NULL;

    assert (! dst->is_snapshot);

    /* These operators aren't interpreted the same way by the backends;
     * they are implemented in terms of other operators in cairo-gstate.c
     */
    assert (op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_CLEAR);

    if (dst->status)
	return dst->status;

    if (dst->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (dst->backend->composite_trapezoids) {
	if (dst->device_x_offset != 0.0 ||
	    dst->device_y_offset != 0.0 ||
	    dst->device_x_scale != 1.0 ||
	    dst->device_y_scale != 1.0)
	{
	    dev_traps = malloc (sizeof (cairo_trapezoid_t) * num_traps);
	    if (!dev_traps)
		return CAIRO_STATUS_NO_MEMORY;

	    _cairo_trapezoid_array_translate_and_scale
		(dev_traps, traps, num_traps,
		 dst->device_x_offset,
		 dst->device_y_offset,
		 dst->device_x_scale,
		 dst->device_y_scale);
	}


	status = dst->backend->composite_trapezoids (op,
						     pattern, dst,
						     antialias,
						     src_x, src_y,
						     BACKEND_X(dst, dst_x),
						     BACKEND_Y(dst, dst_y),
						     // XXX what the heck do I do with width/height?
						     // they're not the same for src and dst!
						     width, height,
						     dev_traps ? dev_traps : traps, num_traps);
	free (dev_traps);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return  _cairo_surface_fallback_composite_trapezoids (op, pattern, dst,
							  antialias,
							  src_x, src_y,
							  dst_x, dst_y,
							  width, height,
							  traps, num_traps);
}

/* _copy_page and _show_page are unique among _cairo_surface functions
 * in that they will actually return CAIRO_INT_STATUS_UNSUPPORTED
 * rather than performing any fallbacks. */
cairo_int_status_t
_cairo_surface_copy_page (cairo_surface_t *surface)
{
    assert (! surface->is_snapshot);

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (surface->backend->copy_page == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return surface->backend->copy_page (surface);
}

/* _show_page and _copy_page are unique among _cairo_surface functions
 * in that they will actually return CAIRO_INT_STATUS_UNSUPPORTED
 * rather than performing any fallbacks. */
cairo_int_status_t
_cairo_surface_show_page (cairo_surface_t *surface)
{
    assert (! surface->is_snapshot);

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (surface->backend->show_page == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return surface->backend->show_page (surface);
}

/**
 * _cairo_surface_get_current_clip_serial:
 * @surface: the #cairo_surface_t to return the serial number for
 *
 * Returns the serial number associated with the current
 * clip in the surface.  All gstate functions must
 * verify that the correct clip is set in the surface before
 * invoking any surface drawing function
 */
unsigned int
_cairo_surface_get_current_clip_serial (cairo_surface_t *surface)
{
    return surface->current_clip_serial;
}

/**
 * _cairo_surface_allocate_clip_serial:
 * @surface: the #cairo_surface_t to allocate a serial number from
 *
 * Each surface has a separate set of clipping serial numbers, and
 * this function allocates one from the specified surface.  As zero is
 * reserved for the special no-clipping case, this function will not
 * return that except for an in-error surface, (ie. surface->status !=
 * CAIRO_STATUS_SUCCESS).
 */
unsigned int
_cairo_surface_allocate_clip_serial (cairo_surface_t *surface)
{
    unsigned int    serial;
    
    if (surface->status)
	return 0;

    if ((serial = ++(surface->next_clip_serial)) == 0)
	serial = ++(surface->next_clip_serial);
    return serial;
}

/**
 * _cairo_surface_reset_clip:
 * @surface: the #cairo_surface_t to reset the clip on
 *
 * This function sets the clipping for the surface to
 * None, which is to say that drawing is entirely
 * unclipped.  It also sets the clip serial number
 * to zero.
 */
cairo_status_t
_cairo_surface_reset_clip (cairo_surface_t *surface)
{
    cairo_status_t  status;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;
    
    surface->current_clip_serial = 0;

    if (surface->backend->intersect_clip_path) {
	status = surface->backend->intersect_clip_path (surface,
							NULL,
							CAIRO_FILL_RULE_WINDING,
							0,
							CAIRO_ANTIALIAS_DEFAULT);
	if (status)
	    return status;
    }

    if (surface->backend->set_clip_region != NULL) {
	status = surface->backend->set_clip_region (surface, NULL);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_surface_set_clip_region:
 * @surface: the #cairo_surface_t to reset the clip on
 * @region: the #pixman_region16_t to use for clipping
 * @serial: the clip serial number associated with the region
 *
 * This function sets the clipping for the surface to
 * the specified region and sets the surface clipping
 * serial number to the associated serial number.
 */
cairo_status_t
_cairo_surface_set_clip_region (cairo_surface_t	    *surface,
				pixman_region16_t   *region,
				unsigned int	    serial)
{
    pixman_region16_t *dev_region = NULL;
    cairo_status_t status;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;
    
    assert (surface->backend->set_clip_region != NULL);

    if (surface->device_x_offset != 0.0 ||
	surface->device_y_offset != 0.0 ||
	surface->device_x_scale != 1.0 ||
	surface->device_y_scale != 1.0)
    {
	dev_region = pixman_region_create ();
	if (surface->device_x_scale == 1.0 &&
	    surface->device_y_scale == 1.0)
	{
	    pixman_region_copy (dev_region, region);
	    pixman_region_translate (dev_region, surface->device_x_offset, surface->device_y_offset);
	} else {
	    int i, nr = pixman_region_num_rects (region);
	    pixman_box16_t *rects = pixman_region_rects (region);
	    for (i = 0; i < nr; i++) {
		pixman_box16_t tmpb;
		pixman_region16_t *tmpr;

		tmpb.x1 = BACKEND_X(surface, rects[i].x1);
		tmpb.y1 = BACKEND_Y(surface, rects[i].y1);
		tmpb.x2 = BACKEND_X(surface, rects[i].x2);
		tmpb.y2 = BACKEND_Y(surface, rects[i].y2);

		tmpr = pixman_region_create_simple (&tmpb);

		pixman_region_append (dev_region, tmpr);
		pixman_region_destroy (tmpr);
	    }

	    pixman_region_validate (dev_region, &i);
	}

	region = dev_region;
    }

    surface->current_clip_serial = serial;

    status = surface->backend->set_clip_region (surface, region);

    if (dev_region)
	pixman_region_destroy (dev_region);

    return status;
}

cairo_int_status_t
_cairo_surface_intersect_clip_path (cairo_surface_t    *surface,
				    cairo_path_fixed_t *path,
				    cairo_fill_rule_t   fill_rule,
				    double		tolerance,
				    cairo_antialias_t	antialias)
{
    cairo_path_fixed_t *dev_path = path;
    cairo_path_fixed_t real_dev_path;
    cairo_status_t status;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;
    
    assert (surface->backend->intersect_clip_path != NULL);

    if (surface->device_x_offset != 0.0 ||
	surface->device_y_offset != 0.0 ||
	surface->device_x_scale != 1.0 ||
	surface->device_y_scale != 1.0)
    {
	_cairo_path_fixed_init_copy (&real_dev_path, path);
	_cairo_path_fixed_offset_and_scale (&real_dev_path,
					    _cairo_fixed_from_double (surface->device_x_offset),
					    _cairo_fixed_from_double (surface->device_y_offset),
					    _cairo_fixed_from_double (surface->device_x_scale),
					    _cairo_fixed_from_double (surface->device_y_scale));
	dev_path = &real_dev_path;
    }

    status = surface->backend->intersect_clip_path (surface,
						    dev_path,
						    fill_rule,
						    tolerance,
						    antialias);

    if (dev_path == &real_dev_path)
	_cairo_path_fixed_fini (&real_dev_path);

    return status;
}

static cairo_status_t
_cairo_surface_set_clip_path_recursive (cairo_surface_t *surface,
					cairo_clip_path_t *clip_path)
{
    cairo_status_t status;

    if (clip_path == NULL)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_set_clip_path_recursive (surface, clip_path->prev);
    if (status)
	return status;

    return _cairo_surface_intersect_clip_path (surface,
					       &clip_path->path,
					       clip_path->fill_rule,
					       clip_path->tolerance,
					       clip_path->antialias);
}

/**
 * _cairo_surface_set_clip_path:
 * @surface: the #cairo_surface_t to set the clip on
 * @clip_path: the clip path to set
 * @serial: the clip serial number associated with the clip path
 * 
 * Sets the given clipping path for the surface and assigns the
 * clipping serial to the surface.
 **/
static cairo_status_t
_cairo_surface_set_clip_path (cairo_surface_t	*surface,
			      cairo_clip_path_t	*clip_path,
			      unsigned int	serial)
{
    cairo_status_t status;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    assert (surface->backend->intersect_clip_path != NULL);

    status = surface->backend->intersect_clip_path (surface,
						    NULL,
						    CAIRO_FILL_RULE_WINDING,
						    0,
						    CAIRO_ANTIALIAS_DEFAULT);
    if (status)
	return status;

    status = _cairo_surface_set_clip_path_recursive (surface, clip_path);
    if (status)
	return status;

    surface->current_clip_serial = serial;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_set_clip (cairo_surface_t *surface, cairo_clip_t *clip)
{
    unsigned int serial = 0;
    
    if (!surface)
	return CAIRO_STATUS_NULL_POINTER;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (clip) {
	serial = clip->serial;
	if (serial == 0)
	    clip = NULL;
    }
    
    surface->clip = clip;

    if (serial == _cairo_surface_get_current_clip_serial (surface))
	return CAIRO_STATUS_SUCCESS;

    if (clip) {
	if (clip->path)
	    return _cairo_surface_set_clip_path (surface,
						 clip->path,
						 clip->serial);
    
	if (clip->region)
	    return _cairo_surface_set_clip_region (surface, 
						   clip->region,
						   clip->serial);
    }
    
    return _cairo_surface_reset_clip (surface);
}

/**
 * _cairo_surface_get_extents:
 * @surface: the #cairo_surface_t to fetch extents for
 *
 * This function returns a bounding box for the surface.  The
 * surface bounds are defined as a region beyond which no
 * rendering will possibly be recorded, in otherwords, 
 * it is the maximum extent of potentially usable
 * coordinates.  For simple pixel-based surfaces,
 * it can be a close bound on the retained pixel
 * region.  For virtual surfaces (PDF et al), it
 * cannot and must extend to the reaches of the
 * target system coordinate space.
 */

cairo_status_t
_cairo_surface_get_extents (cairo_surface_t   *surface,
			    cairo_rectangle_t *rectangle)
{
    cairo_status_t status;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    status = surface->backend->get_extents (surface, rectangle);

    rectangle->x = SURFACE_X(surface, rectangle->x);
    rectangle->y = SURFACE_Y(surface, rectangle->y);

    return status;
}

cairo_status_t
_cairo_surface_show_glyphs (cairo_surface_t	*surface,
			    cairo_operator_t	 op,
			    cairo_pattern_t	*source,
			    const cairo_glyph_t	*glyphs,
			    int			 num_glyphs,
			    cairo_scaled_font_t	*scaled_font)
{
    cairo_status_t status;
    cairo_glyph_t *dev_glyphs = NULL;

    assert (! surface->is_snapshot);

    if (surface->backend->show_glyphs) {
	if (surface->device_x_offset != 0.0 ||
	    surface->device_y_offset != 0.0 ||
	    surface->device_x_scale != 1.0 ||
	    surface->device_y_scale != 1.0)
	{
            int i;

            dev_glyphs = malloc (sizeof(cairo_glyph_t) * num_glyphs);
            if (!dev_glyphs)
                return CAIRO_STATUS_NO_MEMORY;

            for (i = 0; i < num_glyphs; i++) {
                dev_glyphs[i].index = glyphs[i].index;
		// err, we really should scale the size of the glyphs, no?
                dev_glyphs[i].x = BACKEND_X(surface, glyphs[i].x);
                dev_glyphs[i].y = BACKEND_Y(surface, glyphs[i].y);
            }
        }

	status = surface->backend->show_glyphs (surface, op, source,
						dev_glyphs ? dev_glyphs : glyphs,
						num_glyphs, scaled_font);
	free (dev_glyphs);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_surface_fallback_show_glyphs (surface, op, source,
						glyphs, num_glyphs,
						scaled_font);
}

/* XXX: Previously, we had a function named _cairo_surface_show_glyphs
 * with not-so-useful semantics. We've now got a new
 * _cairo_surface_show_glyphs with the proper semantics, and its
 * fallback still uses this old function (which still needs to be
 * cleaned up in terms of both semantics and naming). */
cairo_status_t
_cairo_surface_old_show_glyphs (cairo_scaled_font_t	*scaled_font,
				cairo_operator_t	 op,
				cairo_pattern_t		*pattern,
				cairo_surface_t		*dst,
				int			 source_x,
				int			 source_y,
				int			 dest_x,
				int			 dest_y,
				unsigned int		 width,
				unsigned int		 height,
				const cairo_glyph_t	*glyphs,
				int			 num_glyphs)
{
    cairo_status_t status;
    cairo_glyph_t *dev_glyphs = NULL;

    assert (! dst->is_snapshot);

    if (dst->status)
	return dst->status;

    if (dst->finished)
	return CAIRO_STATUS_SURFACE_FINISHED;

    if (dst->backend->old_show_glyphs) {
	if (dst->device_x_offset != 0.0 ||
	    dst->device_y_offset != 0.0 ||
	    dst->device_x_scale != 1.0 ||
	    dst->device_y_scale != 1.0)
	{
	    int i;

	    dev_glyphs = malloc(sizeof(cairo_glyph_t) * num_glyphs);
	    for (i = 0; i < num_glyphs; i++) {
		dev_glyphs[i] = glyphs[i];
		// err, we really should scale the size of the glyphs, no?
		dev_glyphs[i].x = BACKEND_X(dst, dev_glyphs[i].x);
		dev_glyphs[i].y = BACKEND_Y(dst, dev_glyphs[i].y);
	    }

	    glyphs = dev_glyphs;
	}

	status = dst->backend->old_show_glyphs (scaled_font,
						op, pattern, dst,
						source_x, source_y,
						BACKEND_X(dst, dest_x),
						BACKEND_Y(dst, dest_y),
						width, height,
						glyphs, num_glyphs);

	free (dev_glyphs);
    } else
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    return status;
}

static cairo_status_t
_cairo_surface_composite_fixup_unbounded_internal (cairo_surface_t            *dst,
						   cairo_rectangle_t          *src_rectangle,
						   cairo_rectangle_t          *mask_rectangle,
						   int			       dst_x,
						   int			       dst_y,
						   unsigned int		       width,
						   unsigned int		       height)
{
    cairo_rectangle_t dst_rectangle;
    cairo_rectangle_t drawn_rectangle;
    pixman_region16_t *drawn_region;
    pixman_region16_t *clear_region;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    /* The area that was drawn is the area in the destination rectangle but not within
     * the source or the mask.
     */
    dst_rectangle.x = dst_x;
    dst_rectangle.y = dst_y;
    dst_rectangle.width = width;
    dst_rectangle.height = height;

    drawn_rectangle = dst_rectangle;

    if (src_rectangle)
	_cairo_rectangle_intersect (&drawn_rectangle, src_rectangle);

    if (mask_rectangle)
	_cairo_rectangle_intersect (&drawn_rectangle, mask_rectangle);

    /* Now compute the area that is in dst_rectangle but not in drawn_rectangle
     */
    drawn_region = _cairo_region_create_from_rectangle (&drawn_rectangle);
    clear_region = _cairo_region_create_from_rectangle (&dst_rectangle);
    if (!drawn_region || !clear_region) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_REGIONS;
    }

    if (pixman_region_subtract (clear_region, clear_region, drawn_region) != PIXMAN_REGION_STATUS_SUCCESS) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_REGIONS;
    }

    status = _cairo_surface_fill_region (dst, CAIRO_OPERATOR_SOURCE,
					 CAIRO_COLOR_TRANSPARENT,
					 clear_region);

 CLEANUP_REGIONS:
    if (drawn_region)
	pixman_region_destroy (drawn_region);
    if (clear_region)
	pixman_region_destroy (clear_region);

    return status;
}

/**
 * _cairo_surface_composite_fixup_unbounded:
 * @dst: the destination surface
 * @src_attr: source surface attributes (from _cairo_pattern_acquire_surface())
 * @src_width: width of source surface
 * @src_height: height of source surface
 * @mask_attr: mask surface attributes or %NULL if no mask
 * @mask_width: width of mask surface
 * @mask_height: height of mask surface
 * @src_x: @src_x from _cairo_surface_composite()
 * @src_y: @src_y from _cairo_surface_composite()
 * @mask_x: @mask_x from _cairo_surface_composite()
 * @mask_y: @mask_y from _cairo_surface_composite()
 * @dst_x: @dst_x from _cairo_surface_composite()
 * @dst_y: @dst_y from _cairo_surface_composite()
 * @width: @width from _cairo_surface_composite()
 * @height: @height_x from _cairo_surface_composite()
 * 
 * Eeek! Too many parameters! This is a helper function to take care of fixing
 * up for bugs in libpixman and RENDER where, when asked to composite an
 * untransformed surface with an unbounded operator (like CLEAR or SOURCE)
 * only the region inside both the source and the mask is affected.
 * This function clears the region that should have been drawn but was wasn't.
 **/
cairo_status_t
_cairo_surface_composite_fixup_unbounded (cairo_surface_t            *dst,
					  cairo_surface_attributes_t *src_attr,
					  int                         src_width,
					  int                         src_height,
					  cairo_surface_attributes_t *mask_attr,
					  int                         mask_width,
					  int                         mask_height,
					  int			      src_x,
					  int			      src_y,
					  int			      mask_x,
					  int			      mask_y,
					  int			      dst_x,
					  int			      dst_y,
					  unsigned int		      width,
					  unsigned int		      height)
{
    cairo_rectangle_t src_tmp, mask_tmp;
    cairo_rectangle_t *src_rectangle = NULL;
    cairo_rectangle_t *mask_rectangle = NULL;

    assert (! dst->is_snapshot);

    /* This is a little odd; this function is called from the xlib/image surfaces,
     * where the coordinates have already been transformed by the device_xy_offset.
     * We need to undo this before running through this function,
     * otherwise those offsets get applied twice.
     */
    dst_x = SURFACE_X(dst, dst_x);
    dst_y = SURFACE_Y(dst, dst_y);

    /* The RENDER/libpixman operators are clipped to the bounds of the untransformed,
     * non-repeating sources and masks. Other sources and masks can be ignored.
     */
    if (_cairo_matrix_is_integer_translation (&src_attr->matrix, NULL, NULL) &&
	src_attr->extend == CAIRO_EXTEND_NONE)
    {
	src_tmp.x = (dst_x - (src_x + src_attr->x_offset));
	src_tmp.y = (dst_y - (src_y + src_attr->y_offset));
	src_tmp.width = src_width;
	src_tmp.height = src_height;

	src_rectangle = &src_tmp;
    }

    if (mask_attr &&
	_cairo_matrix_is_integer_translation (&mask_attr->matrix, NULL, NULL) &&
	mask_attr->extend == CAIRO_EXTEND_NONE)
    {
	mask_tmp.x = (dst_x - (mask_x + mask_attr->x_offset));
	mask_tmp.y = (dst_y - (mask_y + mask_attr->y_offset));
	mask_tmp.width = mask_width;
	mask_tmp.height = mask_height;

	mask_rectangle = &mask_tmp;
    }

    return _cairo_surface_composite_fixup_unbounded_internal (dst, src_rectangle, mask_rectangle,
							      dst_x, dst_y, width, height);
}

/**
 * _cairo_surface_composite_shape_fixup_unbounded:
 * @dst: the destination surface
 * @src_attr: source surface attributes (from _cairo_pattern_acquire_surface())
 * @src_width: width of source surface
 * @src_height: height of source surface
 * @mask_width: width of mask surface
 * @mask_height: height of mask surface
 * @src_x: @src_x from _cairo_surface_composite()
 * @src_y: @src_y from _cairo_surface_composite()
 * @mask_x: @mask_x from _cairo_surface_composite()
 * @mask_y: @mask_y from _cairo_surface_composite()
 * @dst_x: @dst_x from _cairo_surface_composite()
 * @dst_y: @dst_y from _cairo_surface_composite()
 * @width: @width from _cairo_surface_composite()
 * @height: @height_x from _cairo_surface_composite()
 * 
 * Like _cairo_surface_composite_fixup_unbounded(), but instead of
 * handling the case where we have a source pattern and a mask
 * pattern, handle the case where we are compositing a source pattern
 * using a mask we create ourselves, as in
 * _cairo_surface_composite_glyphs() or _cairo_surface_composite_trapezoids()
 **/
cairo_status_t
_cairo_surface_composite_shape_fixup_unbounded (cairo_surface_t            *dst,
						cairo_surface_attributes_t *src_attr,
						int                         src_width,
						int                         src_height,
						int                         mask_width,
						int                         mask_height,
						int			    src_x,
						int			    src_y,
						int			    mask_x,
						int			    mask_y,
						int			    dst_x,
						int			    dst_y,
						unsigned int		    width,
						unsigned int		    height)
{
    cairo_rectangle_t src_tmp, mask_tmp;
    cairo_rectangle_t *src_rectangle = NULL;
    cairo_rectangle_t *mask_rectangle = NULL;

    assert (! dst->is_snapshot);

    /* See comment at start of _cairo_surface_composite_fixup_unbounded */
    dst_x = SURFACE_X(dst, dst_x);
    dst_y = SURFACE_Y(dst, dst_y);
  
    /* The RENDER/libpixman operators are clipped to the bounds of the untransformed,
     * non-repeating sources and masks. Other sources and masks can be ignored.
     */
    if (_cairo_matrix_is_integer_translation (&src_attr->matrix, NULL, NULL) &&
	src_attr->extend == CAIRO_EXTEND_NONE)
    {
	src_tmp.x = (dst_x - (src_x + src_attr->x_offset));
	src_tmp.y = (dst_y - (src_y + src_attr->y_offset));
	src_tmp.width = src_width;
	src_tmp.height = src_height;

	src_rectangle = &src_tmp;
    }

    mask_tmp.x = dst_x - mask_x;
    mask_tmp.y = dst_y - mask_y;
    mask_tmp.width = mask_width;
    mask_tmp.height = mask_height;
    
    mask_rectangle = &mask_tmp;

    return _cairo_surface_composite_fixup_unbounded_internal (dst, src_rectangle, mask_rectangle,
							      dst_x, dst_y, width, height);
}
