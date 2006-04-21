/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "FloatRect.h"
#include "Font.h"
#include "IntPointArray.h"
#include "IntRect.h"
#include <cairo-win32.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate();
    ~GraphicsContextPlatformPrivate();

    cairo_t* context;
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
    :  context(0)
{
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    cairo_destroy(context);
}

GraphicsContext::GraphicsContext(HDC dc)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    cairo_surface_t* surface = cairo_win32_surface_create(dc);
    m_data->context = cairo_create(surface);
}

GraphicsContext::GraphicsContext(cairo_t* context)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    m_data->context = cairo_reference(context);
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

cairo_t* GraphicsContext::platformContext() const
{
    return m_data->context;
}

void GraphicsContext::savePlatformState()
{
    cairo_save(m_data->context);
}

void GraphicsContext::restorePlatformState()
{
    cairo_restore(m_data->context);
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    
    cairo_t* context = m_data->context;
    if (fillColor() & 0xFF000000)
        fillRectSourceOver(context, rect, fillColor());

    if (pen().style() != Pen::NoPen) {
        setColorFromPen();
        FloatRect r(rect);
        r.inflate(-.5f);
        cairo_rectangle(context, r.x(), r.y(), r.width(), r.height());
        cairo_set_line_width(context, 1.0);
        cairo_stroke(context);
    }
}

void GraphicsContext::setColorFromFillColor()
{
    setColor(m_data->context, fillColor());
}
  
void GraphicsContext::setColorFromPen()
{
    setColor(m_data->context, pen().color());
}

// FIXME: Now that this is refactored, it should be shared by all contexts.
static void adjustLineToPixelBounderies(FloatPoint& p1, FloatPoint& p2, float strokeWidth, const Pen::PenStyle& penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == Pen::DotLine || penStyle == Pen::DashLine) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        }
        else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }
    
    if (((int)strokeWidth)%2) {
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

    cairo_t* context = m_data->context;
    cairo_save(context);

    Pen::PenStyle penStyle = pen().style();
    if (penStyle == Pen::NoPen)
        return;
    float width = pen().width();
    if (width < 1)
        width = 1;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    
    adjustLineToPixelBounderies(p1, p2, width, penStyle);
    cairo_set_line_width(context, width);

    int patWidth = 0;
    switch (penStyle) {
    case Pen::NoPen:
    case Pen::SolidLine:
        break;
    case Pen::DotLine:
        patWidth = (int)width;
        break;
    case Pen::DashLine:
        patWidth = 3*(int)width;
        break;
    }

    setColorFromPen();
    
    cairo_set_antialias(context, CAIRO_ANTIALIAS_NONE);
    
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (isVerticalLine) {
            fillRectSourceOver(context, FloatRect(p1.x()-width/2, p1.y()-width, width, width), pen().color());
            fillRectSourceOver(context, FloatRect(p2.x()-width/2, p2.y(), width, width), pen().color());
        } else {
            fillRectSourceOver(context, FloatRect(p1.x()-width, p1.y()-width/2, width, width), pen().color());
            fillRectSourceOver(context, FloatRect(p2.x(), p2.y()-width/2, width, width), pen().color());
        }
        
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*(int)width;
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
        cairo_set_dash(context, &dash, 1, patternOffset);
    }

    cairo_move_to(context, p1.x(), p1.y());
    cairo_line_to(context, p2.x(), p2.y());

    cairo_stroke(context);
    cairo_restore(context);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    
    cairo_t* context = m_data->context;
    cairo_save(context);
    float yRadius = .5 * rect.height();
    float xRadius = .5 * rect.width();
    cairo_translate(context, rect.x() + xRadius, rect.y() + yRadius);
    cairo_scale(context, xRadius, yRadius);
    cairo_arc(context, 0., 0., 1., 0., 2 * M_PI);
    cairo_restore(context);

    if (fillColor() & 0xFF000000) {
        setColorFromFillColor();
        cairo_fill(context);
    }
    if (pen().style() != Pen::NoPen) {
        setColorFromPen();
        unsigned penWidth = pen().width();
        if (penWidth == 0) 
            penWidth++;
        cairo_set_line_width(context, penWidth);
        cairo_stroke(context);
    }
}

void GraphicsContext::drawArc(int x, int y, int w, int h, int a, int alen)
{
    // Only supports arc on circles.  That's all khtml needs.
    ASSERT(w == h);

    if (paintingDisabled())
        return;
    
    cairo_t* context = m_data->context;
    if (pen().style() != Pen::NoPen) {        
        float r = (float)w / 2;
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        cairo_arc(context, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180);
        
        setColorFromPen();
        cairo_set_line_width(context, pen().width());
        cairo_stroke(context);
    }
}

void GraphicsContext::drawConvexPolygon(const IntPointArray& points)
{
    if (paintingDisabled())
        return;

    int npoints = points.size();
    if (npoints <= 1)
        return;

    cairo_t* context = m_data->context;

    cairo_save(context);
    cairo_set_antialias(context, CAIRO_ANTIALIAS_NONE);
    cairo_move_to(context, points[0].x(), points[0].y());
    for (int i = 1; i < npoints; i++)
        cairo_line_to(context, points[i].x(), points[i].y());
    cairo_close_path(context);

    if (fillColor() & 0xFF000000) {
        setColorFromFillColor();
        cairo_set_fill_rule(context, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill(context);
    }

    if (pen().style() != Pen::NoPen) {
        setColorFromPen();
        cairo_set_line_width(context, pen().width());
        cairo_stroke(context);
    }
    cairo_restore(context);
}

void GraphicsContext::fillRect(const IntRect& rect, const Brush& brush)
{
    if (paintingDisabled())
        return;

    if (brush.style() == Brush::SolidPattern)
        fillRectSourceOver(m_data->context, rect, brush.color());
}

void GraphicsContext::addClip(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* context = m_data->context;
    cairo_rectangle(context, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_clip(context);
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
    // FIXME: These rects should be rounded
    cairo_rectangle(m_data->context, finalFocusRect.x(), finalFocusRect.y(), finalFocusRect.width(), finalFocusRect.height());
    
    // Force the alpha to 50%.  This matches what the Mac does with outline rings.
    Color ringColor(color.red(), color.green(), color.blue(), 127);
    setColor(m_data->context, ringColor);
    cairo_stroke(m_data->context);
}

void GraphicsContext::setFocusRingClip(const IntRect&)
{
}

void GraphicsContext::clearFocusRingClip()
{
}

}
