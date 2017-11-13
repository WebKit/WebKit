/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CairoOperations.h"

#if USE(CAIRO)

#include "FloatRect.h"
#include "Image.h"
#include "Path.h"
#include "PlatformContextCairo.h"
#include "PlatformPathCairo.h"
#include <cairo.h>

namespace WebCore {
namespace Cairo {

void setLineCap(PlatformContextCairo& platformContext, LineCap lineCap)
{
    cairo_line_cap_t cairoCap;
    switch (lineCap) {
    case ButtCap:
        cairoCap = CAIRO_LINE_CAP_BUTT;
        break;
    case RoundCap:
        cairoCap = CAIRO_LINE_CAP_ROUND;
        break;
    case SquareCap:
        cairoCap = CAIRO_LINE_CAP_SQUARE;
        break;
    }
    cairo_set_line_cap(platformContext.cr(), cairoCap);
}

void setLineDash(PlatformContextCairo& platformContext, const DashArray& dashes, float dashOffset)
{
    if (std::all_of(dashes.begin(), dashes.end(), [](auto& dash) { return !dash; }))
        cairo_set_dash(platformContext.cr(), 0, 0, 0);
    else
        cairo_set_dash(platformContext.cr(), dashes.data(), dashes.size(), dashOffset);
}

void setLineJoin(PlatformContextCairo& platformContext, LineJoin lineJoin)
{
    cairo_line_join_t cairoJoin;
    switch (lineJoin) {
    case MiterJoin:
        cairoJoin = CAIRO_LINE_JOIN_MITER;
        break;
    case RoundJoin:
        cairoJoin = CAIRO_LINE_JOIN_ROUND;
        break;
    case BevelJoin:
        cairoJoin = CAIRO_LINE_JOIN_BEVEL;
        break;
    }
    cairo_set_line_join(platformContext.cr(), cairoJoin);
}

void setMiterLimit(PlatformContextCairo& platformContext, float miterLimit)
{
    cairo_set_miter_limit(platformContext.cr(), miterLimit);
}

void save(PlatformContextCairo& platformContext)
{
    platformContext.save();
}

void restore(PlatformContextCairo& platformContext)
{
    platformContext.restore();
}

void translate(PlatformContextCairo& platformContext, float x, float y)
{
    cairo_translate(platformContext.cr(), x, y);
}

void rotate(PlatformContextCairo& platformContext, float angleInRadians)
{
    cairo_rotate(platformContext.cr(), angleInRadians);
}

void scale(PlatformContextCairo& platformContext, const FloatSize& size)
{
    cairo_scale(platformContext.cr(), size.width(), size.height());
}

void concatCTM(PlatformContextCairo& platformContext, const AffineTransform& transform)
{
    const cairo_matrix_t matrix = toCairoMatrix(transform);
    cairo_transform(platformContext.cr(), &matrix);
}

void beginTransparencyLayer(PlatformContextCairo& platformContext, float opacity)
{
    cairo_push_group(platformContext.cr());
    platformContext.layers().append(opacity);
}

void endTransparencyLayer(PlatformContextCairo& platformContext)
{
    cairo_t* cr = platformContext.cr();
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, platformContext.layers().takeLast());
}

void clip(PlatformContextCairo& platformContext, const FloatRect& rect)
{
    cairo_t* cr = platformContext.cr();
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    // The rectangular clip function is traditionally not expected to
    // antialias. If we don't force antialiased clipping here,
    // edge fringe artifacts may occur at the layer edges
    // when a transformation is applied to the GraphicsContext
    // while drawing the transformed layer.
    cairo_antialias_t savedAntialiasRule = cairo_get_antialias(cr);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
    cairo_set_antialias(cr, savedAntialiasRule);
}

void clipOut(PlatformContextCairo& platformContext, const FloatRect& rect)
{
    cairo_t* cr = platformContext.cr();
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipOut(PlatformContextCairo& platformContext, const Path& path)
{
    cairo_t* cr = platformContext.cr();
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    appendWebCorePathToCairoContext(cr, path);

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipPath(PlatformContextCairo& platformContext, const Path& path, WindRule clipRule)
{
    cairo_t* cr = platformContext.cr();

    if (!path.isNull())
        setPathOnCairoContext(cr, path.platformPath()->context());

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, clipRule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipToImageBuffer(PlatformContextCairo& platformContext, Image& image, const FloatRect& destRect)
{
    RefPtr<cairo_surface_t> surface = image.nativeImageForCurrentFrame();
    if (surface)
        platformContext.pushImageMask(surface.get(), destRect);
}

} // namespace Cairo
} // namespace WebCore

#endif // USE(CAIRO)
