/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "IntRect.h"
#include "WritingMode.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

class SelectionGeometry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT explicit SelectionGeometry(const IntRect&, bool isHorizontal, int columnNumber);

    // FIXME: We should move some of these arguments to an auxillary struct.
    SelectionGeometry(const IntRect&, TextDirection, int, int, int, int, bool, bool, bool, bool, bool, bool, bool, bool, int);
    SelectionGeometry() = default;
    ~SelectionGeometry() = default;

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

    void setLogicalLeft(int);
    void setLogicalWidth(int);
    void setLogicalTop(int);
    void setLogicalHeight(int);

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
    TextDirection m_direction { TextDirection::LTR };
    int m_minX { 0 };
    int m_maxX { 0 };
    int m_maxY { 0 };
    int m_lineNumber { 0 };
    bool m_isLineBreak { false };
    bool m_isFirstOnLine { false };
    bool m_isLastOnLine { false };
    bool m_containsStart { false };
    bool m_containsEnd { false };
    bool m_isHorizontal { true };
    bool m_isInFixedPosition { false };
    bool m_isRubyText { false };
    int m_pageNumber { 0 };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, SelectionGeometry);

} // namespace WebCore
