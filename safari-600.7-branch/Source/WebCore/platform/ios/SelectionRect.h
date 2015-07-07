/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SelectionRect_h
#define SelectionRect_h

#include "IntRect.h"
#include "TextDirection.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

class SelectionRect {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SelectionRect(const IntRect&, bool isHorizontal, int columnNumber);

    // FIXME: We should move some of these arguments to an auxillary struct.
    SelectionRect(const IntRect&, TextDirection, int, int, int, int, bool, bool, bool, bool, bool, bool, bool, bool, int);
    SelectionRect();
    ~SelectionRect() { }

    IntRect rect() const { return m_rect; }

    int logicalLeft() const { return m_isHorizontal ? m_rect.x() : m_rect.y(); }
    int logicalWidth() const { return m_isHorizontal ? m_rect.width() : m_rect.height(); }
    int logicalTop() const { return m_isHorizontal ? m_rect.y() : m_rect.x(); }
    int logicalHeight() const { return m_isHorizontal ? m_rect.height() : m_rect.width(); }

    TextDirection direction() const { return m_direction; }
    int minX() const { return m_minX; }
    int maxX() const { return m_maxX; }
    int maxY() const { return m_maxY; }
    int lineNumber() const { return m_lineNumber; }
    bool isLineBreak() const { return m_isLineBreak; }
    bool isFirstOnLine() const { return m_isFirstOnLine; }
    bool isLastOnLine() const { return m_isLastOnLine; }
    bool containsStart() const { return m_containsStart; }
    bool containsEnd() const { return m_containsEnd; }
    bool isHorizontal() const { return m_isHorizontal; }
    bool isInFixedPosition() const { return m_isInFixedPosition; }
    bool isRubyText() const { return m_isRubyText; }
    int pageNumber() const { return m_pageNumber; }

    void setRect(const IntRect& rect) { m_rect = rect; }

    void setLogicalLeft(int left)
    {
        if (m_isHorizontal)
            m_rect.setX(left);
        else
            m_rect.setY(left);
    }

    void setLogicalWidth(int width)
    {
        if (m_isHorizontal)
            m_rect.setWidth(width);
        else
            m_rect.setHeight(width);
    }

    void setLogicalTop(int top)
    {
        if (m_isHorizontal)
            m_rect.setY(top);
        else
            m_rect.setX(top);
    }

    void setLogicalHeight(int height)
    {
        if (m_isHorizontal)
            m_rect.setHeight(height);
        else
            m_rect.setWidth(height);
    }

    void setDirection(TextDirection direction) { m_direction = direction; }
    void setMinX(int minX) { m_minX = minX; }
    void setMaxX(int maxX) { m_maxX = maxX; }
    void setMaxY(int maxY) { m_maxY = maxY; }
    void setLineNumber(int lineNumber) { m_lineNumber = lineNumber; }
    void setIsLineBreak(bool isLineBreak) { m_isLineBreak = isLineBreak; }
    void setIsFirstOnLine(bool isFirstOnLine) { m_isFirstOnLine = isFirstOnLine; }
    void setIsLastOnLine(bool isLastOnLine) { m_isLastOnLine = isLastOnLine; }
    void setContainsStart(bool containsStart) { m_containsStart = containsStart; }
    void setContainsEnd(bool containsEnd) { m_containsEnd = containsEnd; }
    void setIsHorizontal(bool isHorizontal) { m_isHorizontal = isHorizontal; }

private:
    IntRect m_rect;
    TextDirection m_direction;
    int m_minX;
    int m_maxX;
    int m_maxY;
    int m_lineNumber;
    bool m_isLineBreak;
    bool m_isFirstOnLine;
    bool m_isLastOnLine;
    bool m_containsStart;
    bool m_containsEnd;
    bool m_isHorizontal;
    bool m_isInFixedPosition;
    bool m_isRubyText;
    int m_pageNumber;
};

} // namespace WebCore

#endif // SelectionRect_h
