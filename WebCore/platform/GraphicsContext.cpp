/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "DeprecatedString.h"
#include "Font.h"

using namespace std;

namespace WebCore {

struct GraphicsContextState {
    GraphicsContextState() : fillColor(Color::black), paintingDisabled(false) { }
    Font font;
    Pen pen;
    Color fillColor;
    bool paintingDisabled;
};
        
class GraphicsContextPrivate {
public:
    GraphicsContextPrivate();
    
    GraphicsContextState state;
    Vector<GraphicsContextState> stack;
    Vector<IntRect> m_focusRingRects;
    int m_focusRingWidth;
    int m_focusRingOffset;
    bool m_usesInactiveTextBackgroundColor;
    bool m_updatingControlTints;
};

GraphicsContextPrivate::GraphicsContextPrivate()
    : m_focusRingWidth(0)
    , m_focusRingOffset(0)
    , m_usesInactiveTextBackgroundColor(false)
    , m_updatingControlTints(false)
{
}

GraphicsContextPrivate* GraphicsContext::createGraphicsContextPrivate()
{
    return new GraphicsContextPrivate;
}

void GraphicsContext::destroyGraphicsContextPrivate(GraphicsContextPrivate* deleteMe)
{
    delete deleteMe;
}

void GraphicsContext::save()
{
    if (paintingDisabled())
        return;

    m_common->stack.append(m_common->state);
    
    savePlatformState();
}

void GraphicsContext::restore()
{
    if (paintingDisabled())
        return;

    if (m_common->stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }
    m_common->state = m_common->stack.last();
    m_common->stack.removeLast();
    
    restorePlatformState();
}

const Font& GraphicsContext::font() const
{
    return m_common->state.font;
}

void GraphicsContext::setFont(const Font& aFont)
{
    m_common->state.font = aFont;
}

const Pen& GraphicsContext::pen() const
{
    return m_common->state.pen;
}

void GraphicsContext::setPen(const Pen& pen)
{
    m_common->state.pen = pen;
}

void GraphicsContext::setPen(Pen::PenStyle style)
{
    m_common->state.pen.setStyle(style);
    m_common->state.pen.setColor(Color::black);
    m_common->state.pen.setWidth(0);
}

void GraphicsContext::setPen(RGBA32 rgb)
{
    m_common->state.pen.setStyle(Pen::SolidLine);
    m_common->state.pen.setColor(rgb);
    m_common->state.pen.setWidth(0);
}

void GraphicsContext::setFillColor(const Color& color)
{
    m_common->state.fillColor = color;
}

Color GraphicsContext::fillColor() const
{
    return m_common->state.fillColor;
}

void GraphicsContext::setUsesInactiveTextBackgroundColor(bool u)
{
    m_common->m_usesInactiveTextBackgroundColor = u;
}

bool GraphicsContext::usesInactiveTextBackgroundColor() const
{
    return m_common->m_usesInactiveTextBackgroundColor;
}

bool GraphicsContext::updatingControlTints() const
{
    return m_common->m_updatingControlTints;
}

void GraphicsContext::setUpdatingControlTints(bool b)
{
    setPaintingDisabled(b);
    m_common->m_updatingControlTints = b;
}

void GraphicsContext::setPaintingDisabled(bool f)
{
    m_common->state.paintingDisabled = f;
}

bool GraphicsContext::paintingDisabled() const
{
    return m_common->state.paintingDisabled;
}

void GraphicsContext::drawImage(Image* image, const IntPoint& p, CompositeOperator op)
{        
    drawImage(image, p, IntRect(0, 0, -1, -1), op);
}

void GraphicsContext::drawImage(Image* image, const IntRect& r, CompositeOperator op)
{
    drawImage(image, r, IntRect(0, 0, -1, -1), op);
}

void GraphicsContext::drawImage(Image* image, const IntPoint& dest, const IntRect& srcRect, CompositeOperator op)
{
    drawImage(image, IntRect(dest, srcRect.size()), srcRect, op);
}

void GraphicsContext::drawImage(Image* image, const IntRect& dest, const IntRect& srcRect, CompositeOperator op)
{
    drawImage(image, FloatRect(dest), srcRect, op);
}

void GraphicsContext::drawText(const TextRun& run, const IntPoint& point, int tabWidth, int xpos, int toAdd,
                               TextDirection d, bool visuallyOrdered)
{
    if (paintingDisabled())
        return;
    
    font().drawText(this, run, point, tabWidth, xpos, toAdd, d, visuallyOrdered);
}

void GraphicsContext::drawHighlightForText(const TextRun& run, const IntPoint& point, int h, int tabWidth, int xpos, int toAdd,
                                           TextDirection d, bool visuallyOrdered, const Color& backgroundColor)
{
    if (paintingDisabled())
        return;

    fillRect(font().selectionRectForText(run, point, h, tabWidth, xpos, toAdd, d == RTL, visuallyOrdered), backgroundColor);
}

void GraphicsContext::initFocusRing(int width, int offset)
{
    if (paintingDisabled())
        return;
    clearFocusRing();
    
    m_common->m_focusRingWidth = width;
    m_common->m_focusRingOffset = offset;
}

void GraphicsContext::clearFocusRing()
{
    m_common->m_focusRingRects.clear();
}

void GraphicsContext::addFocusRingRect(const IntRect& rect)
{
    if (paintingDisabled() || rect.isEmpty())
        return;
    m_common->m_focusRingRects.append(rect);
}

int GraphicsContext::focusRingWidth() const
{
    return m_common->m_focusRingWidth;
}

int GraphicsContext::focusRingOffset() const
{
    return m_common->m_focusRingOffset;
}

const Vector<IntRect>& GraphicsContext::focusRingRects() const
{
    return m_common->m_focusRingRects;
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    if (paintingDisabled())
        return;

    float tsw = src.width();
    float tsh = src.height();
    float tw = dest.width();
    float th = dest.height();
        
    if (tsw == -1)
        tsw = image->width();
    if (tsh == -1)
        tsh = image->height();

    if (tw == -1)
        tw = image->width();
    if (th == -1)
        th = image->height();

    image->draw(this, FloatRect(dest.location(), FloatSize(tw, th)), FloatRect(src.location(), FloatSize(tsw, tsh)), op);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& rect, const IntPoint& srcPoint, const IntSize& tileSize)
{
    if (paintingDisabled())
        return;

    image->drawTiled(this, rect, srcPoint, tileSize);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& dest, const IntRect& srcRect, Image::TileRule hRule, Image::TileRule vRule)
{
    if (paintingDisabled())
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile)
        // Just do a scale.
        return drawImage(image, dest, srcRect);

    image->drawTiled(this, dest, srcRect, hRule, vRule);
}

}
