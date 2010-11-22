/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContext.h"

#if PLATFORM(CAIRO)

#include "AffineTransform.h"
#include "CairoPath.h"
#include "CairoUtilities.h"
#include "ContextShadow.h"
#include "FloatRect.h"
#include "Font.h"
#include "GraphicsContextPlatformPrivateCairo.h"
#include "GraphicsContextPrivate.h"
#include "OwnPtrCairo.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Path.h"
#include "Pattern.h"
#include "RefPtrCairo.h"
#include "SimpleFontData.h"
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if PLATFORM(GTK)
#include <gdk/gdk.h>
#include <pango/pango.h>
#elif PLATFORM(WIN)
#include <cairo-win32.h>
#endif

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

static inline void setColor(cairo_t* cr, const Color& col)
{
    float red, green, blue, alpha;
    col.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(cr, red, green, blue, alpha);
}

static inline void setPlatformFill(GraphicsContext* context, cairo_t* cr, GraphicsContextPrivate* gcp)
{
    cairo_pattern_t* pattern = 0;
    cairo_save(cr);
    if (gcp->state.fillPattern) {
        AffineTransform affine;
        pattern = gcp->state.fillPattern->createPlatformPattern(affine);
        cairo_set_source(cr, pattern);
    } else if (gcp->state.fillGradient)
        cairo_set_source(cr, gcp->state.fillGradient->platformGradient());
    else
        setColor(cr, context->fillColor());
    cairo_clip_preserve(cr);
    cairo_paint_with_alpha(cr, gcp->state.globalAlpha);
    cairo_restore(cr);
    if (pattern)
        cairo_pattern_destroy(pattern);
}

static inline void setPlatformStroke(GraphicsContext* context, cairo_t* cr, GraphicsContextPrivate* gcp)
{
    cairo_pattern_t* pattern = 0;
    cairo_save(cr);
    if (gcp->state.strokePattern) {
        AffineTransform affine;
        pattern = gcp->state.strokePattern->createPlatformPattern(affine);
        cairo_set_source(cr, pattern);
    } else if (gcp->state.strokeGradient)
        cairo_set_source(cr, gcp->state.strokeGradient->platformGradient());
    else  {
        Color strokeColor = colorWithOverrideAlpha(context->strokeColor().rgb(), context->strokeColor().alpha() / 255.f * gcp->state.globalAlpha);
        setColor(cr, strokeColor);
    }
    if (gcp->state.globalAlpha < 1.0f && (gcp->state.strokePattern || gcp->state.strokeGradient)) {
        cairo_push_group(cr);
        cairo_paint_with_alpha(cr, gcp->state.globalAlpha);
        cairo_pop_group_to_source(cr);
    }
    cairo_stroke_preserve(cr);
    cairo_restore(cr);
    if (pattern)
        cairo_pattern_destroy(pattern);
}

// A fillRect helper
static inline void fillRectSourceOver(cairo_t* cr, const FloatRect& rect, const Color& col)
{
    setColor(cr, col);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);
}

static void addConvexPolygonToContext(cairo_t* context, size_t numPoints, const FloatPoint* points)
{
    cairo_move_to(context, points[0].x(), points[0].y());
    for (size_t i = 1; i < numPoints; i++)
        cairo_line_to(context, points[i].x(), points[i].y());
    cairo_close_path(context);
}

enum PathDrawingStyle { 
    Fill = 1,
    Stroke = 2,
    FillAndStroke = Fill + Stroke
};

static inline void drawPathShadow(GraphicsContext* context, GraphicsContextPrivate* contextPrivate, PathDrawingStyle drawingStyle)
{
    ContextShadow* shadow = context->contextShadow();
    ASSERT(shadow);
    if (shadow->m_type == ContextShadow::NoShadow)
        return;

    // Calculate the extents of the rendered solid paths.
    cairo_t* cairoContext = context->platformContext();
    cairo_path_t* path = cairo_copy_path(cairoContext);

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

    cairo_t* shadowContext = shadow->beginShadowLayer(cairoContext, solidFigureExtents);
    if (!shadowContext)
        return;

    // It's important to copy the context properties to the new shadow
    // context to preserve things such as the fill rule and stroke width.
    copyContextProperties(cairoContext, shadowContext);
    cairo_append_path(shadowContext, path);

    if (drawingStyle & Fill)
        setPlatformFill(context, shadowContext, contextPrivate);
    if (drawingStyle & Stroke)
        setPlatformStroke(context, shadowContext, contextPrivate);

    shadow->endShadowLayer(cairoContext);
}

static void fillCurrentCairoPath(GraphicsContext* context, GraphicsContextPrivate* gcp, cairo_t* cairoContext)
{
    cairo_set_fill_rule(cairoContext, context->fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    drawPathShadow(context, gcp, Fill);

    setPlatformFill(context, cairoContext, gcp);
    cairo_new_path(cairoContext);
}

static void strokeCurrentCairoPath(GraphicsContext* context, GraphicsContextPrivate* gcp, cairo_t* cairoContext)
{
    drawPathShadow(context, gcp, Stroke);
    setPlatformStroke(context, cairoContext, gcp);
    cairo_new_path(cairoContext);
}

GraphicsContext::GraphicsContext(PlatformGraphicsContext* cr)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    m_data->cr = cairo_reference(cr);
    m_data->syncContext(cr);
    setPaintingDisabled(!cr);
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

AffineTransform GraphicsContext::getCTM() const
{
    cairo_t* cr = platformContext();
    cairo_matrix_t m;
    cairo_get_matrix(cr, &m);
    return AffineTransform(m.xx, m.yx, m.xy, m.yy, m.x0, m.y0);
}

cairo_t* GraphicsContext::platformContext() const
{
    return m_data->cr;
}

void GraphicsContext::savePlatformState()
{
    cairo_save(m_data->cr);
    m_data->save();
    m_data->shadowStack.append(m_data->shadow);
}

void GraphicsContext::restorePlatformState()
{
    cairo_restore(m_data->cr);
    m_data->restore();

    if (m_data->shadowStack.isEmpty())
        m_data->shadow = ContextShadow();
    else {
        m_data->shadow = m_data->shadowStack.last();
        m_data->shadowStack.removeLast();
    }
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    if (fillColor().alpha())
        fillRectSourceOver(cr, rect, fillColor());

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        FloatRect r(rect);
        r.inflate(-.5f);
        cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle style = strokeStyle();
    if (style == NoStroke)
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    float width = strokeThickness();
    if (width < 1)
        width = 1;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());

    adjustLineToPixelBoundaries(p1, p2, width, style);
    cairo_set_line_width(cr, width);

    int patWidth = 0;
    switch (style) {
    case NoStroke:
    case SolidStroke:
        break;
    case DottedStroke:
        patWidth = static_cast<int>(width);
        break;
    case DashedStroke:
        patWidth = 3*static_cast<int>(width);
        break;
    }

    setColor(cr, strokeColor());

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (isVerticalLine) {
            fillRectSourceOver(cr, FloatRect(p1.x() - width/2, p1.y() - width, width, width), strokeColor());
            fillRectSourceOver(cr, FloatRect(p2.x() - width/2, p2.y(), width, width), strokeColor());
        } else {
            fillRectSourceOver(cr, FloatRect(p1.x() - width, p1.y() - width/2, width, width), strokeColor());
            fillRectSourceOver(cr, FloatRect(p2.x(), p2.y() - width/2, width, width), strokeColor());
        }

        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*static_cast<int>(width);
        int remainder = distance%patWidth;
        int coverage = distance-remainder;
        int numSegments = coverage/patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = !(numSegments % 2);
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2;
                } else
                    patternOffset = patWidth / 2;
            } else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2;
            }
        }

        double dash = patWidth;
        cairo_set_dash(cr, &dash, 1, patternOffset);
    }

    cairo_move_to(cr, p1.x(), p1.y());
    cairo_line_to(cr, p2.x(), p2.y());

    cairo_stroke(cr);
    cairo_restore(cr);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    float yRadius = .5 * rect.height();
    float xRadius = .5 * rect.width();
    cairo_translate(cr, rect.x() + xRadius, rect.y() + yRadius);
    cairo_scale(cr, xRadius, yRadius);
    cairo_arc(cr, 0., 0., 1., 0., 2 * M_PI);
    cairo_restore(cr);

    if (fillColor().alpha()) {
        setColor(cr, fillColor());
        cairo_fill_preserve(cr);
    }

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        cairo_set_line_width(cr, strokeThickness());
        cairo_stroke(cr);
    } else
        cairo_new_path(cr);
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled() || strokeStyle() == NoStroke)
        return;

    int x = rect.x();
    int y = rect.y();
    float w = rect.width();
    float h = rect.height();
    float scaleFactor = h / w;
    float reverseScaleFactor = w / h;

    float hRadius = w / 2;
    float vRadius = h / 2;
    float fa = startAngle;
    float falen =  fa + angleSpan;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    if (w != h)
        cairo_scale(cr, 1., scaleFactor);

    cairo_arc_negative(cr, x + hRadius, (y + vRadius) * reverseScaleFactor, hRadius, -fa * M_PI/180, -falen * M_PI/180);

    if (w != h)
        cairo_scale(cr, 1., reverseScaleFactor);

    float width = strokeThickness();
    int patWidth = 0;

    switch (strokeStyle()) {
    case DottedStroke:
        patWidth = static_cast<int>(width / 2);
        break;
    case DashedStroke:
        patWidth = 3 * static_cast<int>(width / 2);
        break;
    default:
        break;
    }

    setColor(cr, strokeColor());

    if (patWidth) {
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance;
        if (hRadius == vRadius)
            distance = static_cast<int>((M_PI * hRadius) / 2.0);
        else // We are elliptical and will have to estimate the distance
            distance = static_cast<int>((M_PI * sqrtf((hRadius * hRadius + vRadius * vRadius) / 2.0)) / 2.0);

        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0.0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = !(numSegments % 2);
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2.0;
                } else
                    patternOffset = patWidth / 2.0;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2.0;
            }
        }

        double dash = patWidth;
        cairo_set_dash(cr, &dash, 1, patternOffset);
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    cairo_t* cr = m_data->cr;

    cairo_save(cr);
    cairo_set_antialias(cr, shouldAntialias ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    addConvexPolygonToContext(cr, npoints, points);

    if (fillColor().alpha()) {
        setColor(cr, fillColor());
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill_preserve(cr);
    }

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        cairo_set_line_width(cr, strokeThickness());
        cairo_stroke(cr);
    } else
        cairo_new_path(cr);

    cairo_restore(cr);
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    cairo_t* cr = m_data->cr;

    cairo_new_path(cr);
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_antialias_t savedAntialiasRule = cairo_get_antialias(cr);

    cairo_set_antialias(cr, antialiased ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    addConvexPolygonToContext(cr, numPoints, points);
    cairo_clip(cr);

    cairo_set_antialias(cr, savedAntialiasRule);
    cairo_set_fill_rule(cr, savedFillRule);
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    setPathOnCairoContext(cr, m_data->m_pendingPath.context());
    fillCurrentCairoPath(this, m_common, cr);
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    setPathOnCairoContext(cr, m_data->m_pendingPath.context());
    strokeCurrentCairoPath(this, m_common, cr);
}

void GraphicsContext::drawPath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    setPathOnCairoContext(cr, m_data->m_pendingPath.context());

    cairo_set_fill_rule(cr, fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    drawPathShadow(this, m_common, FillAndStroke);

    setPlatformFill(this, cr, m_common);
    setPlatformStroke(this, cr, m_common);
    cairo_new_path(cr);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    fillCurrentCairoPath(this, m_common, cr);
    cairo_restore(cr);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    if (m_data->hasShadow())
        m_data->shadow.drawRectShadow(this, enclosingIntRect(rect));

    if (color.alpha())
        fillRectSourceOver(m_data->cr, rect, color);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
    m_data->clip(rect);
}

void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_set_fill_rule(cr, clipRule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
}

void GraphicsContext::drawFocusRing(const Path& path, int width, int offset, const Color& color)
{
    // FIXME: implement
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int /* offset */, const Color& color)
{
    if (paintingDisabled())
        return;

    unsigned rectCount = rects.size();

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    cairo_push_group(cr);
    cairo_new_path(cr);

#if PLATFORM(GTK)
#ifdef GTK_API_VERSION_2
    GdkRegion* reg = gdk_region_new();
#else
    cairo_region_t* reg = cairo_region_create();
#endif

    for (unsigned i = 0; i < rectCount; i++) {
#ifdef GTK_API_VERSION_2
        GdkRectangle rect = rects[i];
        gdk_region_union_with_rect(reg, &rect);
#else
        cairo_rectangle_int_t rect = rects[i];
        cairo_region_union_rectangle(reg, &rect);
#endif
    }
    gdk_cairo_region(cr, reg);
#ifdef GTK_API_VERSION_2
    gdk_region_destroy(reg);
#else
    cairo_region_destroy(reg);
#endif

    setColor(cr, color);
    cairo_set_line_width(cr, 2.0f);
    setPlatformStrokeStyle(DottedStroke);
#else
    int radius = (width - 1) / 2;
    Path path;
    for (unsigned i = 0; i < rectCount; ++i) {
        if (i > 0)
            path.clear();
        path.addRoundedRect(rects[i], FloatSize(radius, radius));
        appendWebCorePathToCairoContext(cr, path);
    }

    // Force the alpha to 50%.  This matches what the Mac does with outline rings.
    Color ringColor(color.red(), color.green(), color.blue(), 127);
    setColor(cr, ringColor);
    cairo_set_line_width(cr, width);
    setPlatformStrokeStyle(SolidStroke);
#endif

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

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

#if !PLATFORM(GTK)
#include "DrawErrorUnderline.h"
#endif

void GraphicsContext::drawLineForTextChecking(const IntPoint& origin, int width, TextCheckingLineStyle style)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    switch (style) {
    case TextCheckingSpellingLineStyle:
        cairo_set_source_rgb(cr, 1, 0, 0);
        break;
    case TextCheckingGrammarLineStyle:
        cairo_set_source_rgb(cr, 0, 1, 0);
        break;
    default:
        cairo_restore(cr);
        return;
    }

#if PLATFORM(GTK)
    // We ignore most of the provided constants in favour of the platform style
    pango_cairo_show_error_underline(cr, origin.x(), origin.y(), width, cMisspellingLineThickness);
#else
    drawErrorUnderline(cr, origin.x(), origin.y(), width, cMisspellingLineThickness);
#endif

    cairo_restore(cr);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    FloatRect result;
    double x = frect.x();
    double y = frect.y();
    cairo_t* cr = m_data->cr;
    cairo_user_to_device(cr, &x, &y);
    x = round(x);
    y = round(y);
    cairo_device_to_user(cr, &x, &y);
    result.setX(static_cast<float>(x));
    result.setY(static_cast<float>(y));
    x = frect.width();
    y = frect.height();
    cairo_user_to_device_distance(cr, &x, &y);
    x = round(x);
    y = round(y);
    cairo_device_to_user_distance(cr, &x, &y);
    result.setWidth(static_cast<float>(x));
    result.setHeight(static_cast<float>(y));
    return result;
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_translate(cr, x, y);
    m_data->translate(x, y);
}

void GraphicsContext::setPlatformFillColor(const Color& col, ColorSpace colorSpace)
{
    // Cairo contexts can't hold separate fill and stroke colors
    // so we set them just before we actually fill or stroke
}

void GraphicsContext::setPlatformStrokeColor(const Color& col, ColorSpace colorSpace)
{
    // Cairo contexts can't hold separate fill and stroke colors
    // so we set them just before we actually fill or stroke
}

void GraphicsContext::setPlatformStrokeThickness(float strokeThickness)
{
    if (paintingDisabled())
        return;

    cairo_set_line_width(m_data->cr, strokeThickness);
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& strokeStyle)
{
    static double dashPattern[] = {5.0, 5.0};
    static double dotPattern[] = {1.0, 1.0};

    if (paintingDisabled())
        return;

    switch (strokeStyle) {
    case NoStroke:
        // FIXME: is it the right way to emulate NoStroke?
        cairo_set_line_width(m_data->cr, 0);
        break;
    case SolidStroke:
        cairo_set_dash(m_data->cr, 0, 0, 0);
        break;
    case DottedStroke:
        cairo_set_dash(m_data->cr, dotPattern, 2, 0);
        break;
    case DashedStroke:
        cairo_set_dash(m_data->cr, dashPattern, 2, 0);
        break;
    }
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    notImplemented();
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    const cairo_matrix_t matrix = cairo_matrix_t(transform);
    cairo_transform(cr, &matrix);
    m_data->concatCTM(transform);
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    clip(rect);

    Path p;
    FloatRect r(rect);
    // Add outer ellipse
    p.addEllipse(r);
    // Add inner ellipse
    r.inflate(-thickness);
    p.addEllipse(r);
    appendWebCorePathToCairoContext(cr, p);

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void GraphicsContext::setPlatformShadow(FloatSize const& size, float blur, Color const& color, ColorSpace)
{
    // Cairo doesn't support shadows natively, they are drawn manually in the draw* functions
    if (m_common->state.shadowsIgnoreTransforms) {
        // Meaning that this graphics context is associated with a CanvasRenderingContext
        // We flip the height since CG and HTML5 Canvas have opposite Y axis
        m_common->state.shadowOffset = FloatSize(size.width(), -size.height());
        m_data->shadow = ContextShadow(color, blur, FloatSize(size.width(), -size.height()));
    } else
        m_data->shadow = ContextShadow(color, blur, FloatSize(size.width(), size.height()));
}

ContextShadow* GraphicsContext::contextShadow()
{
    return &m_data->shadow;
}

void GraphicsContext::clearPlatformShadow()
{
    m_data->shadow.clear();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_push_group(cr);
    m_data->layers.append(opacity);
    m_data->beginTransparencyLayer();
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, m_data->layers.last());
    m_data->layers.removeLast();
    m_data->endTransparencyLayer();
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_fill(cr);
    cairo_restore(cr);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_line_width(cr, width);
    strokeCurrentCairoPath(this, m_common, cr);
    cairo_restore(cr);
}

void GraphicsContext::setLineCap(LineCap lineCap)
{
    if (paintingDisabled())
        return;

    cairo_line_cap_t cairoCap = CAIRO_LINE_CAP_BUTT;
    switch (lineCap) {
    case ButtCap:
        // no-op
        break;
    case RoundCap:
        cairoCap = CAIRO_LINE_CAP_ROUND;
        break;
    case SquareCap:
        cairoCap = CAIRO_LINE_CAP_SQUARE;
        break;
    }
    cairo_set_line_cap(m_data->cr, cairoCap);
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    cairo_set_dash(m_data->cr, dashes.data(), dashes.size(), dashOffset);
}

void GraphicsContext::setLineJoin(LineJoin lineJoin)
{
    if (paintingDisabled())
        return;

    cairo_line_join_t cairoJoin = CAIRO_LINE_JOIN_MITER;
    switch (lineJoin) {
    case MiterJoin:
        // no-op
        break;
    case RoundJoin:
        cairoJoin = CAIRO_LINE_JOIN_ROUND;
        break;
    case BevelJoin:
        cairoJoin = CAIRO_LINE_JOIN_BEVEL;
        break;
    }
    cairo_set_line_join(m_data->cr, cairoJoin);
}

void GraphicsContext::setMiterLimit(float miter)
{
    if (paintingDisabled())
        return;

    cairo_set_miter_limit(m_data->cr, miter);
}

void GraphicsContext::setAlpha(float alpha)
{
    m_common->state.globalAlpha = alpha;
}

float GraphicsContext::getAlpha()
{
    return m_common->state.globalAlpha;
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    cairo_set_operator(m_data->cr, toCairoOperator(op));
}

void GraphicsContext::beginPath()
{
    if (paintingDisabled())
        return;

    cairo_new_path(m_data->m_pendingPath.context());
}

void GraphicsContext::addPath(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_matrix_t currentMatrix;
    cairo_get_matrix(m_data->cr, &currentMatrix);
    cairo_set_matrix(m_data->m_pendingPath.context(), &currentMatrix);
    appendWebCorePathToCairoContext(m_data->m_pendingPath.context(), path);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    OwnPtr<cairo_path_t> p(cairo_copy_path(path.platformPath()->context()));
    cairo_append_path(cr, p.get());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
    m_data->clip(path);
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    appendWebCorePathToCairoContext(cr, path);

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    cairo_rotate(m_data->cr, radians);
    m_data->rotate(radians);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    cairo_scale(m_data->cr, size.width(), size.height());
    m_data->scale(size);
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

static inline FloatPoint getPhase(const FloatRect& dest, const FloatRect& tile)
{
    FloatPoint phase = dest.location();
    phase.move(-tile.x(), -tile.y());

    return phase;
}

void GraphicsContext::fillRoundedRect(const IntRect& r, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    if (m_data->hasShadow())
        m_data->shadow.drawRectShadow(this, r, topLeft, topRight, bottomLeft, bottomRight);

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    Path path;
    path.addRoundedRect(r, topLeft, topRight, bottomLeft, bottomRight);
    appendWebCorePathToCairoContext(cr, path);
    setColor(cr, color);
    cairo_fill(cr);
    cairo_restore(cr);
}

#if PLATFORM(GTK)
void GraphicsContext::setGdkExposeEvent(GdkEventExpose* expose)
{
    m_data->expose = expose;
}

GdkEventExpose* GraphicsContext::gdkExposeEvent() const
{
    return m_data->expose;
}

GdkDrawable* GraphicsContext::gdkDrawable() const
{
    if (!m_data->expose)
        return 0;

    return GDK_DRAWABLE(m_data->expose->window);
}
#endif

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    // When true, use the default Cairo backend antialias mode (usually this
    // enables standard 'grayscale' antialiasing); false to explicitly disable
    // antialiasing. This is the same strategy as used in drawConvexPolygon().
    cairo_set_antialias(m_data->cr, enable ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return InterpolationDefault;
}

} // namespace WebCore

#endif // PLATFORM(CAIRO)
