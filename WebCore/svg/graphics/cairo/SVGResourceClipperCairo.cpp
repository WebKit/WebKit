/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Dirk Schulze <vbs85@gmx.de>
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
#include "CairoPath.h"
#include "GraphicsContext.h"

#include <cairo.h>

namespace WebCore {

void SVGResourceClipper::applyClip(GraphicsContext* context, const FloatRect& boundingBox) const
{
    cairo_t* cr = context->platformContext();
    if (m_clipData.clipData().size() < 1)
        return;

    cairo_reset_clip(cr);
    context->beginPath();

    for (unsigned int x = 0; x < m_clipData.clipData().size(); x++) {
        ClipData data = m_clipData.clipData()[x];

        Path path = data.path;
        if (path.isEmpty())
            continue;
        path.closeSubpath();

        if (data.bboxUnits) {
            // Make use of the clipping units
            AffineTransform transform;
            transform.translate(boundingBox.x(), boundingBox.y());
            transform.scale(boundingBox.width(), boundingBox.height());
            path.transform(transform);
        }
        cairo_path_t* clipPath = cairo_copy_path(path.platformPath()->m_cr);
        cairo_append_path(cr, clipPath);

        cairo_set_fill_rule(cr, data.windRule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    }

    cairo_clip(cr);
}

} // namespace WebCore

#endif
