/* cairo - a vector graphics library with display and print output
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
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

#include "cairo-clip-private.h"

/**
 * cairo_debug_reset_static_data:
 * 
 * Resets all static data within cairo to its original state,
 * (ie. identical to the state at the time of program invocation). For
 * example, all caches within cairo will be flushed empty.
 *
 * This function is intended to be useful when using memory-checking
 * tools such as valgrind. When valgrind's memcheck analyzes a
 * cairo-using program without a call to cairo_debug_reset_static_data,
 * it will report all data reachable via cairo's static objects as
 * "still reachable". Calling cairo_debug_reset_static_data just prior
 * to program termination will make it easier to get squeaky clean
 * reports from valgrind.
 *
 * WARNING: It is only safe to call this function when there are no
 * active cairo objects remaining, (ie. the appropriate destroy
 * functions have been called as necessary). If there are active cairo
 * objects, this call is likely to cause a crash, (eg. an assertion
 * failure due to a hash table being destroyed when non-empty).
 **/
void
cairo_debug_reset_static_data (void)
{
#if CAIRO_HAS_XLIB_SURFACE
    _cairo_xlib_screen_reset_static_data ();
#endif

    _cairo_font_reset_static_data ();

#if CAIRO_HAS_FT_FONT
    _cairo_ft_font_reset_static_data ();
#endif
}

/*
 * clip dumper
 */
void
cairo_debug_dump_clip (struct _cairo_clip *clip,
                       FILE *fp)
{
    fprintf (fp, "clip %p: %s ", (clip->mode == CAIRO_CLIP_MODE_PATH ? "PATH" :
                                  clip->mode == CAIRO_CLIP_MODE_REGION ? "REGION" :
                                  clip->mode == CAIRO_CLIP_MODE_MASK ? "MASK" :
                                  "INVALID?!"));
    if (clip->mode == CAIRO_CLIP_MODE_PATH) {
        fprintf (fp, "\n=== clip path ===\n");
    } else if (clip->mode == CAIRO_CLIP_MODE_REGION) {
        if (!clip->region) {
            fprintf (fp, "region = NULL");
        } else if (pixman_region_num_rects (clip->region) == 1) {
            pixman_box16_t *rects = pixman_region_rects (clip->region);
            fprintf (fp, "region [%d %d %d %d]",
                     rects[0].x1, rects[0].y1,
                     rects[0].x2, rects[0].y2);
        } else {
            pixman_box16_t *rects = pixman_region_rects (clip->region);
            int i, nr = pixman_region_num_rects(clip->region);
            fprintf (fp, "region (%d rects)\n", nr);
            for (i = 0; i < nr; i++) {
                fprintf (fp, "rect %d: [%d %d %d %d]", i,
                         rects[nr].x1, rects[nr].y1,
                         rects[nr].x2, rects[nr].y2);
            }
        }
    } else if (clip->mode == CAIRO_CLIP_MODE_MASK) {
        fprintf (fp, "mask, surface: %p rect: [%d %d %d %d]", clip->surface,
                 clip->surface_rect.x, clip->surface_rect.y, clip->surface_rect.width, clip->surface_rect.height);
    }

    fprintf (fp, "\n");
}

/*
 * path dumper
 */

typedef struct _cairo_debug_path_dump_closure {
    unsigned int op_count;
    FILE *fp;
} cairo_debug_path_dump_closure_t;

static cairo_status_t
_cairo_debug_path_move_to (void *closure,
                           cairo_point_t *point)
{
    cairo_debug_path_dump_closure_t *fdc =
        (cairo_debug_path_dump_closure_t*) closure;
    fprintf (fdc->fp, "%d: moveto (%f, %f)\n",
             fdc->op_count++,
             _cairo_fixed_to_double(point->x),
             _cairo_fixed_to_double(point->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_debug_path_line_to (void *closure,
                           cairo_point_t *point)
{
    cairo_debug_path_dump_closure_t *fdc =
        (cairo_debug_path_dump_closure_t*) closure;
    fprintf (fdc->fp, "%d: lineto (%f, %f)\n",
             fdc->op_count++,
             _cairo_fixed_to_double(point->x),
             _cairo_fixed_to_double(point->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_debug_path_curve_to (void *closure,
                            cairo_point_t *p0,
                            cairo_point_t *p1,
                            cairo_point_t *p2)
{
    cairo_debug_path_dump_closure_t *fdc =
        (cairo_debug_path_dump_closure_t*) closure;
    fprintf (fdc->fp, "%d: curveto (%f, %f) (%f, %f) (%f, %f)\n",
             fdc->op_count++,
             _cairo_fixed_to_double(p0->x),
             _cairo_fixed_to_double(p0->y),
             _cairo_fixed_to_double(p1->x),
             _cairo_fixed_to_double(p1->y),
             _cairo_fixed_to_double(p2->x),
             _cairo_fixed_to_double(p2->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_debug_path_close_path (void *closure)
{
    cairo_debug_path_dump_closure_t *fdc =
        (cairo_debug_path_dump_closure_t*) closure;
    fprintf (fdc->fp, "%d: close\n",
             fdc->op_count++);
    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_debug_dump_path
 * @path: a #cairo_path_fixed_t
 * @fp: the file pointer where to dump the given path
 *
 * Dumps @path in human-readable form to @fp.
 */
void
cairo_debug_dump_path (cairo_path_fixed_t *path,
                       FILE *fp)
{
    cairo_debug_path_dump_closure_t fdc;
    fdc.fp = fp;
    fdc.op_count = 0;

    fprintf (fp, "=== path %p ===\n", path);
    _cairo_path_fixed_interpret (path,
                                 CAIRO_DIRECTION_FORWARD,
                                 _cairo_debug_path_move_to,
                                 _cairo_debug_path_line_to,
                                 _cairo_debug_path_curve_to,
                                 _cairo_debug_path_close_path,
                                 &fdc);
    fprintf (fp, "======================\n");
}

/*
 * traps dumping
 */

/**
 * cairo_debug_dump_traps
 * @traps: a #cairo_traps_t
 * @fp: the file pointer where to dump the traps
 *
 * Dumps @traps in human-readable form to @fp.
 */
void
cairo_debug_dump_traps (cairo_traps_t *traps,
                        FILE *fp)
{
    fprintf (fp, "=== traps %p ===\n", traps);
    fprintf (fp, "extents: (%f, %f) (%f, %f)\n",
             _cairo_fixed_to_double (traps->extents.p1.x),
             _cairo_fixed_to_double (traps->extents.p1.y),
             _cairo_fixed_to_double (traps->extents.p2.x),
             _cairo_fixed_to_double (traps->extents.p2.y));

    cairo_debug_dump_trapezoid_array (traps->traps,
                                      traps->num_traps,
                                      fp);

    fprintf (fp, "=======================\n");
}

/**
 * cairo_debug_dump_trapezoid_array
 * @traps: a #cairo_trapezoid_t pointer
 * @num_traps: the number of trapezoids in @traps
 * @fp: the file pointer where to dump the traps
 *
 * Dumps num_traps in the @traps array in human-readable form to @fp.
 */
void
cairo_debug_dump_trapezoid_array (cairo_trapezoid_t *traps,
                                  int num_traps,
                                  FILE *fp)
{
    int i;

    for (i = 0; i < num_traps; i++) {
        fprintf (fp, "% 3d: t: %f b: %f l: (%f,%f)->(%f,%f) r: (%f,%f)->(%f,%f)\n",
                 i,
                 _cairo_fixed_to_double (traps[i].top),
                 _cairo_fixed_to_double (traps[i].bottom),
                 _cairo_fixed_to_double (traps[i].left.p1.x),
                 _cairo_fixed_to_double (traps[i].left.p1.y),
                 _cairo_fixed_to_double (traps[i].left.p2.x),
                 _cairo_fixed_to_double (traps[i].left.p2.y),
                 _cairo_fixed_to_double (traps[i].right.p1.x),
                 _cairo_fixed_to_double (traps[i].right.p1.y),
                 _cairo_fixed_to_double (traps[i].right.p2.x),
                 _cairo_fixed_to_double (traps[i].right.p2.y));
    }
}

