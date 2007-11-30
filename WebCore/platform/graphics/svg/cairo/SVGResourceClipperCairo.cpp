/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGResourceClipper.h"
#include "AffineTransform.h"
#include "GraphicsContext.h"

#include <cairo.h>

namespace WebCore {

void SVGResourceClipper::applyClip(GraphicsContext* context, const FloatRect& boundingBox) const
{
    Vector<ClipData> data = m_clipData.clipData();
    unsigned int count = data.size();
    if (!count)
        return;

    cairo_t* cr = context->platformContext();
    cairo_reset_clip(cr);

    for (unsigned int x = 0; x < count; x++) {
        Path path = data[x].path;
        if (path.isEmpty())
            continue;
        path.closeSubpath();

        if (data[x].bboxUnits) {
            // Make use of the clipping units
            AffineTransform transform;
            transform.translate(boundingBox.x(), boundingBox.y());
            transform.scale(boundingBox.width(), boundingBox.height());
            path.transform(transform);
        }

        cairo_set_fill_rule(cr, data[x].windRule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);

        // TODO: review this code, clipping may not be having the desired effect
        context->clip(path);
    }
}

} // namespace WebCore

#endif
