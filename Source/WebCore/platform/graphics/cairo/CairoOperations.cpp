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
#include "GraphicsContext.h"
#include "Image.h"
#include "Path.h"
#include "PlatformContextCairo.h"
#include "PlatformPathCairo.h"
#include <cairo.h>

namespace WebCore {
namespace Cairo {

static inline void fillRectWithColor(cairo_t* cr, const FloatRect& rect, const Color& color)
{
    if (!color.isVisible() && cairo_get_operator(cr) == CAIRO_OPERATOR_OVER)
        return;

    setSourceRGBAFromColor(cr, color);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(cr);
}

enum PathDrawingStyle {
    Fill = 1,
    Stroke = 2,
    FillAndStroke = Fill + Stroke
};

static inline void drawPathShadow(PlatformContextCairo& platformContext, const GraphicsContextState& contextState, GraphicsContext& targetContext, PathDrawingStyle drawingStyle)
{
    ShadowBlur& shadow = platformContext.shadowBlur();
    if (shadow.type() == ShadowBlur::NoShadow)
        return;

    // Calculate the extents of the rendered solid paths.
    cairo_t* cairoContext = platformContext.cr();
    std::unique_ptr<cairo_path_t, void(*)(cairo_path_t*)> path(cairo_copy_path(cairoContext), [](cairo_path_t* path) {
        cairo_path_destroy(path);
    });

    FloatRect solidFigureExtents;
    double x0 = 0;
    double x1 = 0;
    double y0 = 0;
    double y1 = 0;
    if (drawingStyle & Stroke) {
        cairo_stroke_extents(cairoContext, &x0, &y0, &x1, &y1);
        solidFigureExtents = FloatRect(x0, y0, x1 - x0, y1 - y0);
    }
    if (drawingStyle & Fill) {
        cairo_fill_extents(cairoContext, &x0, &y0, &x1, &y1);
        FloatRect fillExtents(x0, y0, x1 - x0, y1 - y0);
        solidFigureExtents.unite(fillExtents);
    }

    GraphicsContext* shadowContext = shadow.beginShadowLayer(targetContext, solidFigureExtents);
    if (!shadowContext)
        return;

    cairo_t* cairoShadowContext = shadowContext->platformContext()->cr();

    // It's important to copy the context properties to the new shadow
    // context to preserve things such as the fill rule and stroke width.
    copyContextProperties(cairoContext, cairoShadowContext);

    if (drawingStyle & Fill) {
        cairo_save(cairoShadowContext);
        cairo_append_path(cairoShadowContext, path.get());
        shadowContext->platformContext()->prepareForFilling(contextState, PlatformContextCairo::NoAdjustment);
        cairo_fill(cairoShadowContext);
        cairo_restore(cairoShadowContext);
    }

    if (drawingStyle & Stroke) {
        cairo_append_path(cairoShadowContext, path.get());
        shadowContext->platformContext()->prepareForStroking(contextState, PlatformContextCairo::DoNotPreserveAlpha);
        cairo_stroke(cairoShadowContext);
    }

    // The original path may still be hanging around on the context and endShadowLayer
    // will take care of properly creating a path to draw the result shadow. We remove the path
    // temporarily and then restore it.
    // See: https://bugs.webkit.org/show_bug.cgi?id=108897
    cairo_new_path(cairoContext);
    shadow.endShadowLayer(targetContext);
    cairo_append_path(cairoContext, path.get());
}

static inline void fillCurrentCairoPath(PlatformContextCairo& platformContext, const GraphicsContextState& contextState)
{
    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    platformContext.prepareForFilling(contextState, PlatformContextCairo::AdjustPatternForGlobalAlpha);
    cairo_fill(cr);

    cairo_restore(cr);
}

static inline void adjustFocusRingColor(Color& color)
{
#if !PLATFORM(GTK)
    // Force the alpha to 50%. This matches what the Mac does with outline rings.
    color = Color(makeRGBA(color.red(), color.green(), color.blue(), 127));
#else
    UNUSED_PARAM(color);
#endif
}

static inline void adjustFocusRingLineWidth(float& width)
{
#if PLATFORM(GTK)
    width = 2;
#else
    UNUSED_PARAM(width);
#endif
}

static inline StrokeStyle focusRingStrokeStyle()
{
#if PLATFORM(GTK)
    return DottedStroke;
#else
    return SolidStroke;
#endif
}

static void drawGlyphsToContext(cairo_t* context, cairo_scaled_font_t* scaledFont, double syntheticBoldOffset, const Vector<cairo_glyph_t>& glyphs)
{
    cairo_matrix_t originalTransform;
    if (syntheticBoldOffset)
        cairo_get_matrix(context, &originalTransform);

    cairo_set_scaled_font(context, scaledFont);
    cairo_show_glyphs(context, glyphs.data(), glyphs.size());

    if (syntheticBoldOffset) {
        cairo_translate(context, syntheticBoldOffset, 0);
        cairo_show_glyphs(context, glyphs.data(), glyphs.size());

        cairo_set_matrix(context, &originalTransform);
    }
}

static void drawGlyphsShadow(PlatformContextCairo& platformContext, const GraphicsContextState& state, bool mustUseShadowBlur, const FloatPoint& point, cairo_scaled_font_t* scaledFont, double syntheticBoldOffset, const Vector<cairo_glyph_t>& glyphs, GraphicsContext& targetContext)
{
    ShadowBlur& shadow = platformContext.shadowBlur();

    if (!(state.textDrawingMode & TextModeFill) || shadow.type() == ShadowBlur::NoShadow)
        return;

    if (!mustUseShadowBlur) {
        // Optimize non-blurry shadows, by just drawing text without the ShadowBlur.
        cairo_t* context = platformContext.cr();
        cairo_save(context);

        FloatSize shadowOffset(state.shadowOffset);
        cairo_translate(context, shadowOffset.width(), shadowOffset.height());
        setSourceRGBAFromColor(context, state.shadowColor);
        drawGlyphsToContext(context, scaledFont, syntheticBoldOffset, glyphs);

        cairo_restore(context);
        return;
    }

    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(scaledFont, glyphs.data(), glyphs.size(), &extents);
    FloatRect fontExtentsRect(point.x() + extents.x_bearing, point.y() + extents.y_bearing, extents.width, extents.height);

    if (GraphicsContext* shadowContext = shadow.beginShadowLayer(targetContext, fontExtentsRect)) {
        drawGlyphsToContext(shadowContext->platformContext()->cr(), scaledFont, syntheticBoldOffset, glyphs);
        shadow.endShadowLayer(targetContext);
    }
}

namespace State {

void setStrokeStyle(PlatformContextCairo& platformContext, StrokeStyle strokeStyle)
{
    static const double dashPattern[] = { 5.0, 5.0 };
    static const double dotPattern[] = { 1.0, 1.0 };

    cairo_t* cr = platformContext.cr();
    switch (strokeStyle) {
    case NoStroke:
        // FIXME: is it the right way to emulate NoStroke?
        cairo_set_line_width(cr, 0);
        break;
    case SolidStroke:
    case DoubleStroke:
    case WavyStroke:
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=94110 - Needs platform support.
        cairo_set_dash(cr, 0, 0, 0);
        break;
    case DottedStroke:
        cairo_set_dash(cr, dotPattern, 2, 0);
        break;
    case DashedStroke:
        cairo_set_dash(cr, dashPattern, 2, 0);
        break;
    }
}

} // namespace State

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

void fillRect(PlatformContextCairo& platformContext, const FloatRect& rect, const GraphicsContextState& contextState, GraphicsContext& targetContext)
{
    cairo_t* cr = platformContext.cr();

    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    drawPathShadow(platformContext, contextState, targetContext, Fill);
    fillCurrentCairoPath(platformContext, contextState);
}

void fillRect(PlatformContextCairo& platformContext, const FloatRect& rect, const Color& color, bool hasShadow, GraphicsContext& targetContext)
{
    if (hasShadow)
        platformContext.shadowBlur().drawRectShadow(targetContext, FloatRoundedRect(rect));

    fillRectWithColor(platformContext.cr(), rect, color);
}

void fillRect(PlatformContextCairo& platformContext, const FloatRect& rect, cairo_pattern_t* platformPattern)
{
    cairo_t* cr = platformContext.cr();

    cairo_set_source(cr, platformPattern);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(cr);
}

void fillRoundedRect(PlatformContextCairo& platformContext, const FloatRoundedRect& rect, const Color& color, bool hasShadow, GraphicsContext& targetContext)
{
    if (hasShadow)
        platformContext.shadowBlur().drawRectShadow(targetContext, rect);

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    Path path;
    path.addRoundedRect(rect);
    appendWebCorePathToCairoContext(cr, path);
    setSourceRGBAFromColor(cr, color);
    cairo_fill(cr);

    cairo_restore(cr);
}

void fillRectWithRoundedHole(PlatformContextCairo& platformContext, const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const GraphicsContextState& contextState, bool mustUseShadowBlur, GraphicsContext& targetContext)
{
    // FIXME: this should leverage the specified color.

    if (mustUseShadowBlur)
        platformContext.shadowBlur().drawInsetShadow(targetContext, rect, roundedHoleRect);

    Path path;
    path.addRect(rect);
    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    cairo_t* cr = platformContext.cr();

    cairo_save(cr);
    setPathOnCairoContext(platformContext.cr(), path.platformPath()->context());
    fillCurrentCairoPath(platformContext, contextState);
    cairo_restore(cr);
}

void fillPath(PlatformContextCairo& platformContext, const Path& path, const GraphicsContextState& contextState, GraphicsContext& targetContext)
{
    cairo_t* cr = platformContext.cr();

    setPathOnCairoContext(cr, path.platformPath()->context());
    drawPathShadow(platformContext, contextState, targetContext, Fill);
    fillCurrentCairoPath(platformContext, contextState);
}

void strokeRect(PlatformContextCairo& platformContext, const FloatRect& rect, float lineWidth, const GraphicsContextState& contextState, GraphicsContext& targetContext)
{
    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_line_width(cr, lineWidth);
    drawPathShadow(platformContext, contextState, targetContext, Stroke);
    platformContext.prepareForStroking(contextState);
    cairo_stroke(cr);

    cairo_restore(cr);
}

void strokePath(PlatformContextCairo& platformContext, const Path& path, const GraphicsContextState& contextState, GraphicsContext& targetContext)
{
    cairo_t* cr = platformContext.cr();

    setPathOnCairoContext(cr, path.platformPath()->context());
    drawPathShadow(platformContext, contextState, targetContext, Stroke);
    platformContext.prepareForStroking(contextState);
    cairo_stroke(cr);
}

void clearRect(PlatformContextCairo& platformContext, const FloatRect& rect)
{
    cairo_t* cr = platformContext.cr();

    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_fill(cr);
    cairo_restore(cr);
}

void drawGlyphs(PlatformContextCairo& platformContext, const GraphicsContextState& state, bool mustUseShadowBlur, const FloatPoint& point, cairo_scaled_font_t* scaledFont, double syntheticBoldOffset, const Vector<cairo_glyph_t>& glyphs, float xOffset, GraphicsContext& targetContext)
{
    drawGlyphsShadow(platformContext, state, mustUseShadowBlur, point, scaledFont, syntheticBoldOffset, glyphs, targetContext);

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    if (state.textDrawingMode & TextModeFill) {
        platformContext.prepareForFilling(state, PlatformContextCairo::AdjustPatternForGlobalAlpha);
        drawGlyphsToContext(cr, scaledFont, syntheticBoldOffset, glyphs);
    }

    // Prevent running into a long computation within cairo. If the stroke width is
    // twice the size of the width of the text we will not ask cairo to stroke
    // the text as even one single stroke would cover the full wdth of the text.
    //  See https://bugs.webkit.org/show_bug.cgi?id=33759.
    if (state.textDrawingMode & TextModeStroke && state.strokeThickness < 2 * xOffset) {
        platformContext.prepareForStroking(state);
        cairo_set_line_width(cr, state.strokeThickness);

        // This may disturb the CTM, but we are going to call cairo_restore soon after.
        cairo_set_scaled_font(cr, scaledFont);
        cairo_glyph_path(cr, glyphs.data(), glyphs.size());
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

void drawFocusRing(PlatformContextCairo& platformContext, const Path& path, float width, const Color& color)
{
    // FIXME: We should draw paths that describe a rectangle with rounded corners
    // so as to be consistent with how we draw rectangular focus rings.
    Color ringColor = color;
    adjustFocusRingColor(ringColor);
    adjustFocusRingLineWidth(width);

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    cairo_push_group(cr);
    appendWebCorePathToCairoContext(cr, path);
    setSourceRGBAFromColor(cr, ringColor);
    cairo_set_line_width(cr, width);
    Cairo::State::setStrokeStyle(platformContext, focusRingStrokeStyle());
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_stroke_preserve(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_fill(cr);

    cairo_pop_group_to_source(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);

    cairo_restore(cr);
}

void drawFocusRing(PlatformContextCairo& platformContext, const Vector<FloatRect>& rects, float width, const Color& color)
{
    Path path;
#if PLATFORM(GTK)
    for (const auto& rect : rects)
        path.addRect(rect);
#else
    unsigned rectCount = rects.size();
    int radius = (width - 1) / 2;
    Path subPath;
    for (unsigned i = 0; i < rectCount; ++i) {
        if (i > 0)
            subPath.clear();
        subPath.addRoundedRect(rects[i], FloatSize(radius, radius));
        path.addPath(subPath, AffineTransform());
    }
#endif

    drawFocusRing(platformContext, path, width, color);
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
