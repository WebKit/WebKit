/*
 * This code was mostly copied from gdk-pango.
 * source: http://git.gnome.org/browse/gtk+/tree/gdk/gdkpango.c
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2011 ProFUSION embedded systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "PangoUtilities.h"

#include <cairo.h>
#include <pango/pango.h>

namespace WebCore {

static cairo_region_t* getLineClipRegionFromLayoutIter(PangoLayoutIter* iter, const int* indexRange)
{
    PangoRectangle logicalRect;
    PangoLayoutLine* line = pango_layout_iter_get_line_readonly(iter);
    cairo_region_t* clipRegion = cairo_region_create();
    pango_layout_iter_get_line_extents(iter, 0, &logicalRect);
    int baseline = pango_layout_iter_get_baseline(iter);

    int* pixelRanges = 0;
    int pixelRangeCount = 0;

    if (indexRange[1] >= line->start_index
        && indexRange[0] < line->start_index + line->length)
        pango_layout_line_get_x_ranges(line,
                                       indexRange[0],
                                       indexRange[1],
                                       &pixelRanges, &pixelRangeCount);

    for (int j = 0; j < pixelRangeCount; ++j) {
        int offsetX = PANGO_PIXELS(pixelRanges[2 * j] - logicalRect.x);
        int offsetY = PANGO_PIXELS(baseline - logicalRect.y);

        cairo_rectangle_int_t rect;
        rect.x = offsetX;
        rect.y = -offsetY;
        rect.width = PANGO_PIXELS(pixelRanges[2 * j + 1] - logicalRect.x) - offsetX;
        rect.height = PANGO_PIXELS(baseline - logicalRect.y + logicalRect.height) - offsetY;

        cairo_region_union_rectangle(clipRegion, &rect);
    }

    free(pixelRanges);

    return clipRegion;
}

cairo_region_t* getClipRegionFromPangoLayoutLine(PangoLayoutLine* line, const int* indexRange)
{
    if (!indexRange)
        return 0;

    PangoLayoutIter* iter = pango_layout_get_iter(line->layout);
    while (pango_layout_iter_get_line_readonly(iter) != line)
        pango_layout_iter_next_line(iter);

    cairo_region_t* clipRegion = getLineClipRegionFromLayoutIter(iter, indexRange);

    pango_layout_iter_free(iter);

    return clipRegion;
}

} // namespace WebCore
