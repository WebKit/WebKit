/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#include "CairoPath.h"
#include "FloatRect.h"
#include "Font.h"
#include "FontData.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Path.h"
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if PLATFORM(WIN)
#include <cairo-win32.h>
#endif

#if PLATFORM(GTK)
#include <gdk/gdk.h>
#endif


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate();
    ~GraphicsContextPlatformPrivate();

    cairo_t* cr;
    Vector<float> layers;

#if PLATFORM(GTK)
    GdkEventExpose* expose;
#endif
};

static inline void setColor(cairo_t* cr, const Color& col)
{
    float red, green, blue, alpha;
    col.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(cr, red, green, blue, alpha);
}

// A fillRect helper
static inline void fillRectSourceOver(cairo_t* cr, const FloatRect& rect, const Color& col)
{
    setColor(cr, col);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);
}

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate()
    :  cr(0)
#if PLATFORM(GTK)
    , expose(0)
#endif
{
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    cairo_destroy(cr);
}

#if PLATFORM(WIN)
GraphicsContext::GraphicsContext(HDC dc)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    if (dc) {
        cairo_surface_t* surface = cairo_win32_surface_create(dc);
        m_data->cr = cairo_create(surface);
    } else {
        setPaintingDisabled(true);
        m_data->cr = 0;
    }
}
#endif

GraphicsContext::GraphicsContext(PlatformGraphicsContext* cr)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    m_data->cr = cairo_reference(cr);
    setPaintingDisabled(!cr);
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

cairo_t* GraphicsContext::platformContext() const
{
    return m_data->cr;
}

void GraphicsContext::savePlatformState()
{
    cairo_save(m_data->cr);
}

void GraphicsContext::restorePlatformState()
{
    cairo_restore(m_data->cr);
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

// FIXME: Now that this is refactored, it should be shared by all contexts.
static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle style)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (style == DottedStroke || style == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        }
        else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (static_cast<int>(strokeWidth) % 2) {
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5);
            p2.setX(p2.x() + 0.5);
        }
        else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5);
            p2.setY(p2.y() + 0.5);
        }
    }
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
            bool evenNumberOfSegments = numSegments%2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder/2;
                }
                else
                    patternOffset = patWidth/2;
            }
            else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
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
    }

    cairo_new_path(cr);
}

// FIXME: This function needs to be adjusted to match the functionality on the Mac side.
void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke)
        return;

    int x = rect.x();
    int y = rect.y();
    float w = rect.width();
#if 0 // FIXME: unused so far
    float h = rect.height();
    float scaleFactor = h / w;
    float reverseScaleFactor = w / h;
#endif
    float r = w / 2;
    float fa = startAngle;
    float falen =  fa + angleSpan;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    cairo_arc_negative(cr, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180);
    setColor(cr, strokeColor());
    cairo_set_line_width(cr, strokeThickness());
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
    cairo_move_to(cr, points[0].x(), points[0].y());
    for (size_t i = 1; i < npoints; i++)
        cairo_line_to(cr, points[i].x(), points[i].y());
    cairo_close_path(cr);

    if (fillColor().alpha()) {
        setColor(cr, fillColor());
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill_preserve(cr);
    }

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        cairo_set_line_width(cr, strokeThickness());
        cairo_stroke(cr);
    }

    cairo_new_path(cr);
    cairo_restore(cr);
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.alpha())
        fillRectSourceOver(m_data->cr, rect, color);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.alpha())
        fillRectSourceOver(m_data->cr, rect, color);
}

void GraphicsContext::clip(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_clip(cr);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    int radius = (focusRingWidth() - 1) / 2;
    int offset = radius + focusRingOffset();

    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();
    IntRect finalFocusRect;
    for (unsigned i = 0; i < rectCount; i++) {
        IntRect focusRect = rects[i];
        focusRect.inflate(offset);
        finalFocusRect.unite(focusRect);
    }

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    // FIXME: These rects should be rounded
    cairo_rectangle(cr, finalFocusRect.x(), finalFocusRect.y(), finalFocusRect.width(), finalFocusRect.height());

    // Force the alpha to 50%.  This matches what the Mac does with outline rings.
    Color ringColor(color.red(), color.green(), color.blue(), 127);
    setColor(cr, ringColor);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void GraphicsContext::setFocusRingClip(const IntRect&)
{
    // hopefully a no-op. Comment in CG version says that it exists
    // to work around bugs in Mac focus ring clipping
}

void GraphicsContext::clearFocusRingClip()
{
    // hopefully a no-op. Comment in CG version says that it exists
    // to work around bugs in Mac focus ring clipping
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint&, int width, bool grammar)
{
    notImplemented();
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
}

IntPoint GraphicsContext::origin()
{
    cairo_matrix_t matrix;
    cairo_t* cr = m_data->cr;
    cairo_get_matrix(cr, &matrix);
    return IntPoint(static_cast<int>(matrix.x0), static_cast<int>(matrix.y0));
}

void GraphicsContext::setPlatformFillColor(const Color& col)
{
    // FIXME: this is probably a no-op but I'm not sure
    // notImplemented(); // commented-out because it's chatty and clutters output
}

void GraphicsContext::setPlatformStrokeColor(const Color& col)
{
    // FIXME: this is probably a no-op but I'm not sure
    //notImplemented(); // commented-out because it's chatty and clutters output
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
    default:
        notImplemented();
        break;
    }
}

void GraphicsContext::setPlatformFont(const Font& font)
{
    if (paintingDisabled())
        return;

#if PLATFORM(GTK)
    // FIXME: is it the right thing to do? Also, doesn't work on Win because
    // there FontData doesn't have ::setFont()
    const FontData *fontData = font.primaryFont();
    fontData->setFont(m_data->cr);
#endif
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
    const cairo_matrix_t* matrix = reinterpret_cast<const cairo_matrix_t*>(&transform);
    cairo_transform(cr, matrix);
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    notImplemented();
}

void GraphicsContext::setShadow(IntSize const&, int, Color const&)
{
    notImplemented();
}

void GraphicsContext::clearShadow()
{
    notImplemented();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_push_group(cr);
    m_data->layers.append(opacity);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, m_data->layers.last());
    m_data->layers.removeLast();
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

void GraphicsContext::strokeRect(const FloatRect&, float)
{
    notImplemented();
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

void GraphicsContext::setAlpha(float)
{
    notImplemented();
}

static inline cairo_operator_t toCairoOperator(CompositeOperator op)
{
    switch (op) {
        case CompositeClear:
            return CAIRO_OPERATOR_CLEAR;
        case CompositeCopy:
            return CAIRO_OPERATOR_SOURCE;
        case CompositeSourceOver:
            return CAIRO_OPERATOR_OVER;
        case CompositeSourceIn:
            return CAIRO_OPERATOR_IN;
        case CompositeSourceOut:
            return CAIRO_OPERATOR_OUT;
        case CompositeSourceAtop:
            return CAIRO_OPERATOR_ATOP;
        case CompositeDestinationOver:
            return CAIRO_OPERATOR_DEST_OVER;
        case CompositeDestinationIn:
            return CAIRO_OPERATOR_DEST_IN;
        case CompositeDestinationOut:
            return CAIRO_OPERATOR_DEST_OUT;
        case CompositeDestinationAtop:
            return CAIRO_OPERATOR_DEST_ATOP;
        case CompositeXOR:
            return CAIRO_OPERATOR_XOR;
        case CompositePlusDarker:
            return CAIRO_OPERATOR_OVER;
        case CompositeHighlight:
            return CAIRO_OPERATOR_OVER;
        case CompositePlusLighter:
            return CAIRO_OPERATOR_OVER;
    }

    return CAIRO_OPERATOR_OVER;
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

    cairo_t* cr = m_data->cr;
    cairo_new_path(cr);
}

void GraphicsContext::addPath(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_path_t *p = cairo_copy_path(path.platformPath()->m_cr);
    cairo_append_path(cr, p);
    cairo_path_destroy(p);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_path_t *p = cairo_copy_path(path.platformPath()->m_cr);
    cairo_append_path(cr, p);
    cairo_path_destroy(p);
    cairo_clip(cr);
}

void GraphicsContext::clipOut(const Path&)
{
    notImplemented();
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    cairo_rotate(m_data->cr, radians);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    cairo_scale(m_data->cr, size.width(), size.height());
}

void GraphicsContext::clipOut(const IntRect&)
{
    notImplemented();
}

void GraphicsContext::clipOutEllipseInRect(const IntRect&)
{
    notImplemented();
}

void GraphicsContext::fillRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color&)
{
    notImplemented();
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
    return GDK_DRAWABLE(m_data->expose->window);
}

IntPoint GraphicsContext::translatePoint(const IntPoint& point) const
{
    cairo_matrix_t tm;
    cairo_get_matrix(m_data->cr, &tm);
    double x = point.x();
    double y = point.y();

    cairo_matrix_transform_point(&tm, &x, &y);
    return IntPoint(x, y);
}
#endif

} // namespace WebCore

#endif // PLATFORM(CAIRO)
