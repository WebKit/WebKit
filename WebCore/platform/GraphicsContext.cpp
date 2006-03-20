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

#include "IntRect.h"
#include "DeprecatedString.h"
#include "Font.h"
#include "Widget.h"

namespace WebCore {

void GraphicsContext::drawImageAtPoint(Image* image, const IntPoint& p, Image::CompositeOperator compositeOperator)
{        
    drawImage(image, p.x(), p.y(), 0, 0, -1, -1, compositeOperator);
}

void GraphicsContext::drawImageInRect(Image* image, const IntRect& r, Image::CompositeOperator compositeOperator)
{
    drawImage(image, r.x(), r.y(), r.width(), r.height(), 0, 0, -1, -1, compositeOperator);
}

void GraphicsContext::drawImage(Image* image, int x, int y,
                         int sx, int sy, int sw, int sh, Image::CompositeOperator compositeOperator, void* context)
{
    drawImage(image, x, y, sw, sh, sx, sy, sw, sh, compositeOperator, context);
}

void GraphicsContext::drawImage(Image* image, int x, int y, int w, int h,
                         int sx, int sy, int sw, int sh, Image::CompositeOperator compositeOperator, void* context)
{
    drawFloatImage(image, (float)x, (float)y, (float)w, (float)h, (float)sx, (float)sy, (float)sw, (float)sh, compositeOperator, context);
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
    
    m_focusRingWidth = width;
    m_focusRingOffset = offset;
}

void GraphicsContext::clearFocusRing()
{
    m_focusRingRects.clear();
}

void GraphicsContext::addFocusRingRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    m_focusRingRects.append(rect);
}

}
