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

#include "FloatRect.h"
#include "Font.h"
#include "IntRect.h"
#include "Widget.h"

namespace WebCore {

struct GraphicsContextState {
    GraphicsContextState() : paintingDisabled(false) { }
    Font font;
    Pen pen;
    Brush brush;
    bool paintingDisabled;
};
        
class GraphicsContextPrivate {
public:
    GraphicsContextPrivate(bool isForPrinting);
    
    GraphicsContextState state;
    Vector<GraphicsContextState> stack;
    Vector<IntRect> m_focusRingRects;
    int m_focusRingWidth;
    int m_focusRingOffset;
    bool m_isForPrinting;
    bool m_usesInactiveTextBackgroundColor;
    bool m_updatingControlTints;
};

GraphicsContextPrivate::GraphicsContextPrivate(bool isForPrinting)
    : m_focusRingWidth(0)
    , m_focusRingOffset(0)
    , m_isForPrinting(isForPrinting)
    , m_usesInactiveTextBackgroundColor(false)
    , m_updatingControlTints(false)
{
}

GraphicsContextPrivate* GraphicsContext::createGraphicsContextPrivate(bool isForPrinting)
{
    return new GraphicsContextPrivate(isForPrinting);
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

void GraphicsContext::setBrush(const Brush& brush)
{
    m_common->state.brush = brush;
}

void GraphicsContext::setBrush(Brush::BrushStyle style)
{
    m_common->state.brush.setStyle(style);
    m_common->state.brush.setColor(Color::black);
}

void GraphicsContext::setBrush(RGBA32 rgb)
{
    m_common->state.brush.setStyle(Brush::SolidPattern);
    m_common->state.brush.setColor(rgb);
}

const Brush& GraphicsContext::brush() const
{
    return m_common->state.brush;
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

bool GraphicsContext::printing() const
{
    return m_common->m_isForPrinting;
}

void GraphicsContext::drawImageAtPoint(Image* image, const IntPoint& p, Image::CompositeOperator compositeOperator)
{        
    drawImage(image, p, 0, 0, -1, -1, compositeOperator);
}

void GraphicsContext::drawImageInRect(Image* image, const IntRect& r, Image::CompositeOperator compositeOperator)
{
    drawImage(image, r, 0, 0, -1, -1, compositeOperator);
}

void GraphicsContext::drawImage(Image* image, const IntPoint& dest,
                         int sx, int sy, int sw, int sh, Image::CompositeOperator compositeOperator, void* context)
{
    drawImage(image, IntRect(dest, IntSize(sw, sh)), sx, sy, sw, sh, compositeOperator, context);
}

void GraphicsContext::drawImage(Image* image, const IntRect& dest,
                         int sx, int sy, int sw, int sh, Image::CompositeOperator compositeOperator, void* context)
{
    drawImage(image, FloatRect(dest), (float)sx, (float)sy, (float)sw, (float)sh, compositeOperator, context);
}

// FIXME: We should consider removing this function and having callers just call the lower-level drawText directly.
// FIXME: The int parameter should change to a HorizontalAlignment parameter.
// FIXME: HorizontalAlignment should be moved into a separate header so it's not in Widget.h.
// FIXME: We should consider changing this function to take a character pointer and length instead of a DeprecatedString.
void GraphicsContext::drawText(int x, int y, int horizontalAlignment, const DeprecatedString& deprecatedString)
{
    if (paintingDisabled())
        return;

    if (horizontalAlignment == AlignRight)
        x -= font().width(deprecatedString.unicode(), deprecatedString.length(), 0, 0);
    drawText(x, y, 0, 0, deprecatedString.unicode(), deprecatedString.length(), 0, deprecatedString.length(), 0);
}

void GraphicsContext::drawText(int x, int y, int tabWidth, int xpos, const QChar *str, int slen, int pos, int len, int toAdd,
                               TextDirection d, bool visuallyOrdered, int from, int to)
{
    if (paintingDisabled())
        return;

    int length = std::min(slen - pos, len);
    if (length <= 0)
        return;
    
    font().drawText(this, x, y, tabWidth, xpos, str + pos, length, from, to, toAdd, d, visuallyOrdered);
}

void GraphicsContext::drawHighlightForText(int x, int y, int h, int tabWidth, int xpos, const QChar *str, int slen, int pos, int len, int toAdd,
                                           TextDirection d, bool visuallyOrdered, int from, int to, const Color& backgroundColor)
{
    if (paintingDisabled())
        return;
        
    int length = std::min(slen - pos, len);
    if (length <= 0)
        return;

    return font().drawHighlightForText(this, x, y, h, tabWidth, xpos, str + pos, length, from, to,
                                       toAdd, d, visuallyOrdered, backgroundColor);
}

void GraphicsContext::drawLineForText(int x, int y, int yOffset, int width)
{
    if (paintingDisabled())
        return;

    return font().drawLineForText(this, x, y, yOffset, width);
}


void GraphicsContext::drawLineForMisspelling(int x, int y, int width)
{
    if (paintingDisabled())
        return;

    return font().drawLineForMisspelling(this, x, y, width);
}

int GraphicsContext::misspellingLineThickness() const
{
    return font().misspellingLineThickness(this);
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

}
