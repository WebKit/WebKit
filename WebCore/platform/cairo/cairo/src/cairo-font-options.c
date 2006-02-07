/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat Inc.
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
 *      Owen Taylor <otaylor@redhat.com>
 */

#include "cairoint.h"

static const cairo_font_options_t cairo_font_options_nil = {
    CAIRO_ANTIALIAS_DEFAULT,
    CAIRO_SUBPIXEL_ORDER_DEFAULT,
    CAIRO_HINT_STYLE_DEFAULT,
    CAIRO_HINT_METRICS_DEFAULT
};

/**
 * _cairo_font_options_init_default:
 * @options: a #cairo_font_options_t
 * 
 * Initializes all fileds of the font options object to default values.
 **/
void
_cairo_font_options_init_default (cairo_font_options_t *options)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;
  
    options->antialias = CAIRO_ANTIALIAS_DEFAULT;
    options->subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    options->hint_style = CAIRO_HINT_STYLE_DEFAULT;
    options->hint_metrics = CAIRO_HINT_METRICS_DEFAULT;
}

void
_cairo_font_options_init_copy (cairo_font_options_t		*options,
			       const cairo_font_options_t	*other)
{
    options->antialias = other->antialias;
    options->subpixel_order = other->subpixel_order;
    options->hint_style = other->hint_style;
    options->hint_metrics = other->hint_metrics;
}

/**
 * cairo_font_options_create:
 *
 * Allocates a new font options object with all options initialized
 *  to default values.
 * 
 * Return value: a newly allocated #cairo_font_options_t. Free with
 *   cairo_font_options_destroy(). This function always returns a
 *   valid pointer; if memory cannot be allocated, then a special
 *   error object is returned where all operations on the object do nothing.
 *   You can check for this with cairo_font_options_status().
 **/
cairo_font_options_t *
cairo_font_options_create (void)
{
    cairo_font_options_t *options = malloc (sizeof (cairo_font_options_t));

    if (!options)
	return (cairo_font_options_t *)&cairo_font_options_nil;

    _cairo_font_options_init_default (options);

    return options;
}

/**
 * cairo_font_options_copy:
 * @original: a #cairo_font_options_t
 *
 * Allocates a new font options object copying the option values from
 *  @original.
 * 
 * Return value: a newly allocated #cairo_font_options_t. Free with
 *   cairo_font_options_destroy(). This function always returns a
 *   valid pointer; if memory cannot be allocated, then a special
 *   error object is returned where all operations on the object do nothing.
 *   You can check for this with cairo_font_options_status().
 **/
cairo_font_options_t *
cairo_font_options_copy (const cairo_font_options_t *original)
{
    cairo_font_options_t *options = malloc (sizeof (cairo_font_options_t));

    if (!options)
	return (cairo_font_options_t *)&cairo_font_options_nil;

    _cairo_font_options_init_copy (options, original);

    return options;
}

/**
 * cairo_font_options_destroy:
 * @options: a #cairo_font_options_t
 * 
 * Destroys a #cairo_font_options_t object created with with
 * cairo_font_options_create() or cairo_font_options_copy().
 **/
void 
cairo_font_options_destroy (cairo_font_options_t *options)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;

    free (options);
}

/**
 * cairo_font_options_status:
 * @options: a #cairo_font_options_t
 * 
 * Checks whether an error has previously occurred for this
 * font options object
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY
 **/
cairo_status_t
cairo_font_options_status (cairo_font_options_t *options)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return CAIRO_STATUS_NO_MEMORY;
    else
	return CAIRO_STATUS_SUCCESS;
}	

/**
 * cairo_font_options_merge:
 * @options: a #cairo_font_options_t
 * @other: another #cairo_font_options_t
 * 
 * Merges non-default options from @other into @options, replacing
 * existing values. This operation can be thought of as somewhat
 * similar to compositing @other onto @options with the operation
 * of %CAIRO_OPERATION_OVER.
 **/
void
cairo_font_options_merge (cairo_font_options_t       *options,
			  const cairo_font_options_t *other)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;

    if (other->antialias != CAIRO_ANTIALIAS_DEFAULT)
	options->antialias = other->antialias;
    if (other->subpixel_order != CAIRO_SUBPIXEL_ORDER_DEFAULT)
	options->subpixel_order = other->subpixel_order;
    if (other->hint_style != CAIRO_HINT_STYLE_DEFAULT)
	options->hint_style = other->hint_style;
    if (other->hint_metrics != CAIRO_HINT_METRICS_DEFAULT)
	options->hint_metrics = other->hint_metrics;
}

/**
 * cairo_font_options_equal:
 * @options: a #cairo_font_options_t
 * @other: another #cairo_font_options_t
 * 
 * Compares two font options objects for equality.
 * 
 * Return value: %TRUE if all fields of the two font options objects match
 **/
cairo_bool_t
cairo_font_options_equal (const cairo_font_options_t *options,
			  const cairo_font_options_t *other)
{
    return (options->antialias == other->antialias &&
	    options->subpixel_order == other->subpixel_order &&
	    options->hint_style == other->hint_style &&
	    options->hint_metrics == other->hint_metrics);
}

/**
 * cairo_font_options_hash:
 * @options: a #cairo_font_options_t
 * 
 * Compute a hash for the font options object; this value will
 * be useful when storing an object containing a cairo_font_options_t
 * in a hash table.
 * 
 * Return value: the hash value for the font options object.
 *   The return value can be cast to a 32-bit type if a
 *   32-bit hash value is needed.
 **/
unsigned long
cairo_font_options_hash (const cairo_font_options_t *options)
{
    return ((options->antialias) |
	    (options->subpixel_order << 4) |
	    (options->hint_style << 8) | 
	    (options->hint_metrics << 16));
}

/**
 * cairo_font_options_set_antialias:
 * @options: a #cairo_font_options_t
 * @antialias: the new antialiasing mode
 * 
 * Sets the antiliasing mode for the font options object. This
 * specifies the type of antialiasing to do when rendering text.
 **/
void
cairo_font_options_set_antialias (cairo_font_options_t *options,
				  cairo_antialias_t     antialias)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;
    
    options->antialias = antialias;
}

/**
 * cairo_font_options_get_antialias:
 * @options: a #cairo_font_options_t
 * 
 * Gets the antialising mode for the font options object.
 * 
 * Return value: the antialiasing mode
 **/
cairo_antialias_t
cairo_font_options_get_antialias (const cairo_font_options_t *options)
{
    return options->antialias;
}

/**
 * cairo_font_options_set_subpixel_order:
 * @options: a #cairo_font_options_t
 * @subpixel_order: the new subpixel order
 * 
 * Sets the subpixel order for the font options object. The subpixel
 * order specifies the order of color elements within each pixel on
 * the display device when rendering with an antialiasing mode of
 * %CAIRO_ANTIALIAS_SUBPIXEL. See the documentation for
 * #cairo_subpixel_order_t for full details.
 **/
void
cairo_font_options_set_subpixel_order (cairo_font_options_t   *options,
				       cairo_subpixel_order_t  subpixel_order)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;
    
    options->subpixel_order = subpixel_order;
}

/**
 * cairo_font_options_get_subpixel_order:
 * @options: a #cairo_font_options_t
 * 
 * Gets the subpixel order for the font options object.
 * See the documentation for #cairo_subpixel_order_t for full details.
 * 
 * Return value: the subpixel order for the font options object
 **/
cairo_subpixel_order_t
cairo_font_options_get_subpixel_order (const cairo_font_options_t *options)
{
    return options->subpixel_order;
}

/**
 * cairo_font_options_set_hint_style:
 * @options: a #cairo_font_options_t
 * @hint_style: the new hint style
 * 
 * Sets the hint style for font outlines for the font options object.
 * This controls whether to fit font outlines to the pixel grid,
 * and if so, whether to optimize for fidelity or contrast.
 * See the documentation for #cairo_hint_style_t for full details.
 **/
void
cairo_font_options_set_hint_style (cairo_font_options_t *options,
				   cairo_hint_style_t    hint_style)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;
    
    options->hint_style = hint_style;
}

/**
 * cairo_font_options_get_hint_style:
 * @options: a #cairo_font_options_t
 * 
 * Gets the hint style for font outlines for the font options object.
 * See the documentation for #cairo_hint_style_t for full details.
 * 
 * Return value: the hint style for the font options object
 **/
cairo_hint_style_t
cairo_font_options_get_hint_style (const cairo_font_options_t *options)
{
    return options->hint_style;
}

/**
 * cairo_font_options_set_hint_metrics:
 * @options: a #cairo_font_options_t
 * @hint_metrics: the new metrics hinting mode
 * 
 * Sets the metrics hinting mode for the font options object. This
 * controls whether metrics are quantized to integer values in
 * device units.
 * See the documentation for #cairo_hint_metrics_t for full details.
 **/
void
cairo_font_options_set_hint_metrics (cairo_font_options_t *options,
				     cairo_hint_metrics_t  hint_metrics)
{
    if (options == (cairo_font_options_t *)&cairo_font_options_nil)
	return;

    options->hint_metrics = hint_metrics;
}

/**
 * cairo_font_options_get_hint_metrics:
 * @options: a #cairo_font_options_t
 * 
 * Gets the metrics hinting mode for the font options object.
 * See the documentation for #cairo_hint_metrics_t for full details.
 * 
 * Return value: the metrics hinting mode for the font options object
 **/
cairo_hint_metrics_t
cairo_font_options_get_hint_metrics (const cairo_font_options_t *options)
{
    return options->hint_metrics;
}
