/* Cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Partially on code from xftdpy.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>

#include "cairo-xlib-private.h"

#include <fontconfig/fontconfig.h>

#include <X11/Xlibint.h>	/* For XESetCloseDisplay */
#include <X11/extensions/Xrender.h>

static int
parse_boolean (const char *v)
{
    char c0, c1;
  
    c0 = *v;
    if (c0 == 't' || c0 == 'T' || c0 == 'y' || c0 == 'Y' || c0 == '1')
	return 1;
    if (c0 == 'f' || c0 == 'F' || c0 == 'n' || c0 == 'N' || c0 == '0')
	return 0;
    if (c0 == 'o')
    {
	c1 = v[1];
	if (c1 == 'n' || c1 == 'N')
	    return 1;
	if (c1 == 'f' || c1 == 'F')
	    return 0;
    }
  
    return -1;
}

static cairo_bool_t
get_boolean_default (Display       *dpy,
		     const char    *option,
		     cairo_bool_t  *value)
{
    char *v;
    int i;
  
    v = XGetDefault (dpy, "Xft", option);
    if (v) {
	i = parse_boolean (v);
	if (i >= 0) {
	    *value = i;
	    return TRUE;
	}
    }
  
    return FALSE;
}

static cairo_bool_t
get_integer_default (Display    *dpy,
		     const char *option,
		     int        *value)
{
    int i;
    char *v, *e;
  
    v = XGetDefault (dpy, "Xft", option);
    if (v) {
	if (FcNameConstant ((FcChar8 *) v, value))
	    return TRUE;
      
	i = strtol (v, &e, 0);
	if (e != v)
	    return TRUE;
    }
  
    return FALSE;
}

/* Old versions of fontconfig didn't have these options */
#ifndef FC_HINT_NONE
#define FC_HINT_NONE        0
#define FC_HINT_SLIGHT      1
#define FC_HINT_MEDIUM      2
#define FC_HINT_FULL        3
#endif

static void
_cairo_xlib_init_screen_font_options (cairo_xlib_screen_info_t *info)
{
    cairo_bool_t xft_hinting;
    cairo_bool_t xft_antialias;
    int xft_hintstyle;
    int xft_rgba;
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_hint_style_t hint_style;

    if (!get_boolean_default (info->display, "antialias", &xft_antialias))
	xft_antialias = TRUE;
  
    if (!get_boolean_default (info->display, "hinting", &xft_hinting))
	xft_hinting = TRUE;
  
    if (!get_integer_default (info->display, "hintstyle", &xft_hintstyle))
	xft_hintstyle = FC_HINT_FULL;

    if (!get_integer_default (info->display, "rgba", &xft_rgba))
    {
	xft_rgba = FC_RGBA_UNKNOWN;
      
#if RENDER_MAJOR > 0 || RENDER_MINOR >= 6
	if (info->has_render)
	{
	    int render_order = XRenderQuerySubpixelOrder (info->display,
							  XScreenNumberOfScreen (info->screen));
	  
	    switch (render_order)
	    {
	    default:
	    case SubPixelUnknown:
		xft_rgba = FC_RGBA_UNKNOWN;
		break;
	    case SubPixelHorizontalRGB:
		xft_rgba = FC_RGBA_RGB;
		break;
	    case SubPixelHorizontalBGR:
		xft_rgba = FC_RGBA_BGR;
		break;
	    case SubPixelVerticalRGB:
		xft_rgba = FC_RGBA_VRGB;
		break;
	    case SubPixelVerticalBGR:
		xft_rgba = FC_RGBA_VBGR;
		break;
	    case SubPixelNone:
		xft_rgba = FC_RGBA_NONE;
		break;
	    }
	}
#endif
    }

    if (xft_hinting) {
	switch (xft_hintstyle) {
	case FC_HINT_NONE:
	    hint_style = CAIRO_HINT_STYLE_NONE;
	    break;
	case FC_HINT_SLIGHT:
	    hint_style = CAIRO_HINT_STYLE_SLIGHT;
	    break;
	case FC_HINT_MEDIUM:
	    hint_style = CAIRO_HINT_STYLE_MEDIUM;
	    break;
	case FC_HINT_FULL:
	    hint_style = CAIRO_HINT_STYLE_FULL;
	    break;
	default:
	    hint_style = CAIRO_HINT_STYLE_DEFAULT;
	}
    } else {
	hint_style = CAIRO_HINT_STYLE_NONE;
    }
    
    switch (xft_rgba) {
    case FC_RGBA_RGB:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
	break;
    case FC_RGBA_BGR:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
	break;
    case FC_RGBA_VRGB:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
	break;
    case FC_RGBA_VBGR:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
	break;
    case FC_RGBA_UNKNOWN:
    case FC_RGBA_NONE:
    default:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    }

    if (xft_antialias) {
	if (subpixel_order == CAIRO_SUBPIXEL_ORDER_DEFAULT)
	    antialias = CAIRO_ANTIALIAS_GRAY;
	else
	    antialias = CAIRO_ANTIALIAS_SUBPIXEL;
    } else {
	antialias = CAIRO_ANTIALIAS_NONE;
    }

    _cairo_font_options_init_default (&info->font_options);
    cairo_font_options_set_hint_style (&info->font_options, hint_style);
    cairo_font_options_set_antialias (&info->font_options, antialias);
    cairo_font_options_set_subpixel_order (&info->font_options, subpixel_order);
}

CAIRO_MUTEX_DECLARE(_xlib_screen_mutex);

static cairo_xlib_screen_info_t *_cairo_xlib_screen_list = NULL;

/* XXX: From this function we should also run through and cleanup
 * anything else that still has a pointer to this Display*. For
 * example, we should clean up any Xlib-specific glyph caches. */
static int
_cairo_xlib_close_display (Display *dpy, XExtCodes *codes)
{
    cairo_xlib_screen_info_t *info, *prev;

    /*
     * Unhook from the global list
     */
    CAIRO_MUTEX_LOCK (_xlib_screen_mutex);

    prev = NULL;
    for (info = _cairo_xlib_screen_list; info; info = info->next) {
	if (info->display == dpy) {
	    if (prev)
		prev->next = info->next;
	    else
		_cairo_xlib_screen_list = info->next;
	    free (info);
	    break;
	}
	prev = info;
    }
    CAIRO_MUTEX_UNLOCK (_xlib_screen_mutex);

    /* Return value in accordance with requirements of
     * XESetCloseDisplay */
    return 0;
}

static void
_cairo_xlib_screen_info_reset (void)
{
    cairo_xlib_screen_info_t *info, *next;

    /*
     * Delete everything in the list.
     */
    CAIRO_MUTEX_LOCK (_xlib_screen_mutex);

    for (info = _cairo_xlib_screen_list; info; info = next) {
	next = info->next;
	free (info);
    }

    _cairo_xlib_screen_list = NULL;

    CAIRO_MUTEX_UNLOCK (_xlib_screen_mutex);

}

cairo_xlib_screen_info_t *
_cairo_xlib_screen_info_get (Display *dpy, Screen *screen)
{
    cairo_xlib_screen_info_t *info;
    cairo_xlib_screen_info_t **prev;
    int event_base, error_base;
    XExtCodes *codes;
    cairo_bool_t seen_display = FALSE;

    /* There is an apparent deadlock between this mutex and the
     * mutex for the display, but it's actually safe. For the
     * app to call XCloseDisplay() while any other thread is
     * inside this function would be an error in the logic
     * app, and the CloseDisplay hook is the only other place we
     * acquire this mutex.
     */
    CAIRO_MUTEX_LOCK (_xlib_screen_mutex);

    for (prev = &_cairo_xlib_screen_list; (info = *prev); prev = &(*prev)->next)
    {
	if (info->display == dpy) {
	    seen_display = TRUE;
	    if (info->screen == screen)
	    {
		/*
		 * MRU the list
		 */
		if (prev != &_cairo_xlib_screen_list)
		{
		    *prev = info->next;
		    info->next = _cairo_xlib_screen_list;
		    _cairo_xlib_screen_list = info;
		}
		break;
	    }
	}
    }

    if (info)
	goto out;

    info = malloc (sizeof (cairo_xlib_screen_info_t));
    if (!info)
	goto out;

    if (!seen_display) {
	codes = XAddExtension (dpy);
	if (!codes) {
	    free (info);
	    info = NULL;
	    goto out;
	}
	
	XESetCloseDisplay (dpy, codes->extension, _cairo_xlib_close_display);
    }

    info->display = dpy;
    info->screen = screen;
    info->has_render = (XRenderQueryExtension (dpy, &event_base, &error_base) &&
			(XRenderFindVisualFormat (dpy, DefaultVisual (dpy, DefaultScreen (dpy))) != 0));
    
    _cairo_xlib_init_screen_font_options (info);
    
    info->next = _cairo_xlib_screen_list;
    _cairo_xlib_screen_list = info;

 out:
    CAIRO_MUTEX_UNLOCK (_xlib_screen_mutex);
    
    return info;
}

void
_cairo_xlib_screen_reset_static_data (void)
{
    _cairo_xlib_screen_info_reset ();

#if HAVE_XRMFINALIZE
    XrmFinalize ();
#endif

}
