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
#include "Font.h"
#include "Pen.h"
#include "FloatRect.h"
#include "IntPointArray.h"

#include <cairo.h>
#include <cairo-win32.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

struct GraphicsContextState {
    GraphicsContextState() : paintingDisabled(false) { }
    Font font;
    Pen pen;
    Brush brush;
    bool paintingDisabled;
};

struct GraphicsContextPrivate {
    GraphicsContextPrivate();
    ~GraphicsContextPrivate();

    GraphicsContextState state;    
    Vector<GraphicsContextState> stack;
    cairo_t* context;
};

static inline void setColor(cairo_t* cr, const Color& col)
{
    float red, green, blue, alpha;
    col.getRgbaF(&red, &green, &blue, &alpha);
    cairo_set_source_rgba(cr, red, green, blue, alpha);
}

// A fillRect helper
static inline void fillRectSourceOver(cairo_t* cr, float x, float y, float w, float h, const Color& col)
{
    setColor(cr, col);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);
}

GraphicsContextPrivate::GraphicsContextPrivate()
    :  context(0)
{
}

GraphicsContextPrivate::~GraphicsContextPrivate()
{
    cairo_destroy(context);
}

GraphicsContext::GraphicsContext(HDC dc)
    : m_data(new GraphicsContextPrivate)
    , m_isForPrinting(false)
    , m_usesInactiveTextBackgroundColor(false)
    , m_updatingControlTints(false)
{
    cairo_surface_t* surface = cairo_win32_surface_create(dc);
    m_data->context = cairo_create(surface);
}

GraphicsContext::GraphicsContext(bool forPrinting)
    : m_data(new GraphicsContextPrivate)
    , m_isForPrinting(forPrinting)
    , m_usesInactiveTextBackgroundColor(false)
    , m_updatingControlTints(false)
{
}

GraphicsContext::~GraphicsContext()
{
    delete m_data;
}

const Pen& GraphicsContext::pen() const
{
    return m_data->state.pen;
}

void GraphicsContext::setPen(const Pen& pen)
{
    m_data->state.pen = pen;
}

void GraphicsContext::setPen(Pen::PenStyle style)
{
    m_data->state.pen.setStyle(style);
    m_data->state.pen.setColor(Color::black);
    m_data->state.pen.setWidth(0);
}

void GraphicsContext::setPen(RGBA32 rgb)
{
    m_data->state.pen.setStyle(Pen::SolidLine);
    m_data->state.pen.setColor(rgb);
    m_data->state.pen.setWidth(0);
}

void GraphicsContext::setBrush(const Brush& brush)
{
    m_data->state.brush = brush;
}

void GraphicsContext::setBrush(Brush::BrushStyle style)
{
    m_data->state.brush.setStyle(style);
    m_data->state.brush.setColor(Color::black);
}

void GraphicsContext::setBrush(RGBA32 rgb)
{
    m_data->state.brush.setStyle(Brush::SolidPattern);
    m_data->state.brush.setColor(rgb);
}

const Brush& GraphicsContext::brush() const
{
    return m_data->state.brush;
}

void GraphicsContext::save()
{
    if (m_data->state.paintingDisabled)
        return;

    m_data->stack.append(m_data->state);

    cairo_save(m_data->context);
}

void GraphicsContext::restore()
{
    if (m_data->state.paintingDisabled)
        return;

    if (m_data->stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }
    m_data->state = m_data->stack.last();
    m_data->stack.removeLast();
     
    cairo_restore(m_data->context);
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(int x, int y, int w, int h)
{
    if (m_data->state.paintingDisabled)
        return;
    
    cairo_t* context = m_data->context;
    if (m_data->state.brush.style() != Brush::NoBrush)
        fillRectSourceOver(context, x, y, w, h, m_data->state.brush.color());

    if (m_data->state.pen.style() != Pen::NoPen) {
        setColorFromPen();
        cairo_rectangle(context, x+.5, y+.5 , w-.5 , h-.5);
        cairo_set_line_width(context, 1.0);
        cairo_stroke(context);
    }
}

void GraphicsContext::setColorFromBrush()
{
    setColor(m_data->context, m_data->state.brush.color());
}
  
void GraphicsContext::setColorFromPen()
{
    setColor(m_data->context, m_data->state.brush.color());
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
void GraphicsContext::drawLine(int x1, int y1, int x2, int y2)
{
    if (m_data->state.paintingDisabled)
        return;

    cairo_t* context = m_data->context;
    cairo_save(context);

    Pen::PenStyle penStyle = m_data->state.pen.style();
    if (penStyle == Pen::NoPen)
        return;
    float width = m_data->state.pen.width();
    if (width < 1)
        width = 1;

    FloatPoint p1 = FloatPoint(x1, y1);
    FloatPoint p2 = FloatPoint(x2, y2);
    
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
        if (x1 == x2) {
            fillRectSourceOver(context, p1.x()-width/2, p1.y()-width, width, width, m_data->state.pen.color());
            fillRectSourceOver(context, p2.x()-width/2, p2.y(), width, width, m_data->state.pen.color());
        } else {
            fillRectSourceOver(context, p1.x()-width, p1.y()-width/2, width, width, m_data->state.pen.color());
            fillRectSourceOver(context, p2.x(), p2.y()-width/2, width, width, m_data->state.pen.color());
        }
        
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = ((x1 == x2) ? (y2 - y1) : (x2 - x1)) - 2*(int)width;
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
void GraphicsContext::drawEllipse(int x, int y, int width, int height)
{
    if (m_data->state.paintingDisabled)
        return;
    
    cairo_t* context = m_data->context;
    cairo_save(context);
    float yRadius = .5 * height;
    float xRadius = .5 * width;
    cairo_translate(context, x + xRadius, y + yRadius);
    cairo_scale(context, xRadius, yRadius);
    cairo_arc(context, 0., 0., 1., 0., 2 * M_PI);
    cairo_restore(context);

    if (m_data->state.brush.style() != Brush::NoBrush) {
        setColorFromBrush();
        cairo_fill(context);
    }
    if (m_data->state.pen.style() != Pen::NoPen) {
        setColorFromPen();
        uint penWidth = m_data->state.pen.width();
        if (penWidth == 0) 
            penWidth++;
        cairo_set_line_width(context, penWidth);
        cairo_stroke(context);
    }
}

void GraphicsContext::drawArc (int x, int y, int w, int h, int a, int alen)
{ 
    // Only supports arc on circles.  That's all khtml needs.
    ASSERT(w == h);

    if (m_data->state.paintingDisabled)
        return;
    
    cairo_t* context = m_data->context;
    if (m_data->state.pen.style() != Pen::NoPen) {        
        float r = (float)w / 2;
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        cairo_arc(context, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180);
        
        setColorFromPen();
        cairo_set_line_width(context, m_data->state.pen.width());
        cairo_stroke(context);
    }
}

void GraphicsContext::drawConvexPolygon(const IntPointArray& points)
{
    if (m_data->state.paintingDisabled)
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

    if (m_data->state.brush.style() != Brush::NoBrush) {
        setColorFromBrush();
        cairo_set_fill_rule(context, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill(context);
    }

    if (m_data->state.pen.style() != Pen::NoPen) {
        setColorFromPen();
        cairo_set_line_width(context, m_data->state.pen.width());
        cairo_stroke(context);
    }
    cairo_restore(context);
}

void GraphicsContext::drawFloatImage(Image* image, float x, float y, float w, float h, 
                              float sx, float sy, float sw, float sh, Image::CompositeOperator compositeOperator, void* context)
{
    if (m_data->state.paintingDisabled)
        return;

    float tsw = sw;
    float tsh = sh;
    float tw = w;
    float th = h;
        
    if (tsw == -1)
        tsw = image->width();
    if (tsh == -1)
        tsh = image->height();

    if (tw == -1)
        tw = image->width();
    if (th == -1)
        th = image->height();

    image->drawInRect(FloatRect(x, y, tw, th), FloatRect(sx, sy, tsw, tsh), compositeOperator, context);
}

void GraphicsContext::drawTiledImage(Image* image, int x, int y, int w, int h, int sx, int sy, void* context)
{
    if (m_data->state.paintingDisabled)
        return;
    
    image->tileInRect(FloatRect(x, y, w, h), FloatPoint(sx, sy), context);
}

void GraphicsContext::drawScaledAndTiledImage(Image* image, int x, int y, int w, int h, int sx, int sy, int sw, int sh, 
    Image::TileRule hRule, Image::TileRule vRule, void* context)
{
    if (m_data->state.paintingDisabled)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile)
        // Just do a scale.
        return drawImage(image, x, y, w, h, sx, sy, sw, sh, Image::CompositeSourceOver, context);

    image->scaleAndTileInRect(FloatRect(x, y, w, h), FloatRect(sx, sy, sw, sh), hRule, vRule, context);
}

void GraphicsContext::fillRect(int x, int y, int w, int h, const Brush& brush)
{
    if (m_data->state.paintingDisabled)
        return;

    if (brush.style() == Brush::SolidPattern)
        fillRectSourceOver(m_data->context, x, y, w, h, brush.color());
}

void GraphicsContext::fillRect(const IntRect& rect, const Brush& brush)
{
    fillRect(rect.x(), rect.y(), rect.width(), rect.height(), brush);
}

void GraphicsContext::addClip(const IntRect& rect)
{
    if (m_data->state.paintingDisabled)
        return;

    cairo_t* context = m_data->context;
    cairo_rectangle(context, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_clip(context);
}

void GraphicsContext::setPaintingDisabled(bool f)
{
    m_data->state.paintingDisabled = f;
}

bool GraphicsContext::paintingDisabled() const
{
    return m_data->state.paintingDisabled;
}

}
