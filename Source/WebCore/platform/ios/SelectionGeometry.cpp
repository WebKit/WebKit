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

#include "config.h"
#include "SelectionGeometry.h"

#include "FloatQuad.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

SelectionGeometry::SelectionGeometry(const FloatQuad& quad, SelectionRenderingBehavior behavior, bool isHorizontal, int pageNumber)
    : m_quad(quad)
    , m_behavior(behavior)
    , m_isHorizontal(isHorizontal)
    , m_pageNumber(pageNumber)
{
}

// FIXME: We should move some of these arguments to an auxillary struct.
SelectionGeometry::SelectionGeometry(const FloatQuad& quad, SelectionRenderingBehavior behavior, TextDirection direction, int minX, int maxX, int maxY, int lineNumber, bool isLineBreak, bool isFirstOnLine, bool isLastOnLine, bool containsStart, bool containsEnd, bool isHorizontal, bool isInFixedPosition, bool isRubyText, int pageNumber)
    : m_quad(quad)
    , m_behavior(behavior)
    , m_direction(direction)
    , m_minX(minX)
    , m_maxX(maxX)
    , m_maxY(maxY)
    , m_lineNumber(lineNumber)
    , m_isLineBreak(isLineBreak)
    , m_isFirstOnLine(isFirstOnLine)
    , m_isLastOnLine(isLastOnLine)
    , m_containsStart(containsStart)
    , m_containsEnd(containsEnd)
    , m_isHorizontal(isHorizontal)
    , m_isInFixedPosition(isInFixedPosition)
    , m_isRubyText(isRubyText)
    , m_pageNumber(pageNumber)
{
}

SelectionGeometry::SelectionGeometry(const FloatQuad& quad, SelectionRenderingBehavior behavior, TextDirection direction, int minX, int maxX, int maxY, int lineNumber, bool isLineBreak, bool isFirstOnLine, bool isLastOnLine, bool containsStart, bool containsEnd, bool isHorizontal)
    : m_quad(quad)
    , m_behavior(behavior)
    , m_direction(direction)
    , m_minX(minX)
    , m_maxX(maxX)
    , m_maxY(maxY)
    , m_lineNumber(lineNumber)
    , m_isLineBreak(isLineBreak)
    , m_isFirstOnLine(isFirstOnLine)
    , m_isLastOnLine(isLastOnLine)
    , m_containsStart(containsStart)
    , m_containsEnd(containsEnd)
    , m_isHorizontal(isHorizontal)
{
}

void SelectionGeometry::setLogicalLeft(int left)
{
    auto rect = this->rect();
    if (m_isHorizontal)
        rect.setX(left);
    else
        rect.setY(left);
    setRect(rect);
}

void SelectionGeometry::setLogicalWidth(int width)
{
    auto rect = this->rect();
    if (m_isHorizontal)
        rect.setWidth(width);
    else
        rect.setHeight(width);
    setRect(rect);
}

void SelectionGeometry::setLogicalTop(int top)
{
    auto rect = this->rect();
    if (m_isHorizontal)
        rect.setY(top);
    else
        rect.setX(top);
    setRect(rect);
}

void SelectionGeometry::setLogicalHeight(int height)
{
    auto rect = this->rect();
    if (m_isHorizontal)
        rect.setHeight(height);
    else
        rect.setWidth(height);
    setRect(rect);
}

IntRect SelectionGeometry::rect() const
{
    if (!m_cachedEnclosingRect)
        m_cachedEnclosingRect = m_quad.enclosingBoundingBox();
    return *m_cachedEnclosingRect;
}

void SelectionGeometry::setQuad(const FloatQuad& quad)
{
    m_quad = quad;
    m_cachedEnclosingRect.reset();
}

void SelectionGeometry::setRect(const IntRect& rect)
{
    m_quad = FloatQuad { rect };
    m_cachedEnclosingRect = rect;
}

TextStream& operator<<(TextStream& stream, SelectionGeometry rect)
{
    TextStream::GroupScope group(stream);
    stream << "selection geometry";

    stream.dumpProperty("quad", rect.quad());
    stream.dumpProperty("direction", isLeftToRightDirection(rect.direction()) ? "ltr" : "rtl");

    stream.dumpProperty("min-x", rect.minX());
    stream.dumpProperty("max-x", rect.maxX());
    stream.dumpProperty("max-y", rect.maxY());

    stream.dumpProperty("line number", rect.lineNumber());
    if (rect.isLineBreak())
        stream.dumpProperty("is line break", true);
    if (rect.isFirstOnLine())
        stream.dumpProperty("is first on line", true);
    if (rect.isLastOnLine())
        stream.dumpProperty("is last on line", true);

    if (rect.containsStart())
        stream.dumpProperty("contains start", true);

    if (rect.containsEnd())
        stream.dumpProperty("contains end", true);

    if (rect.isHorizontal())
        stream.dumpProperty("is horizontal", true);

    if (rect.isInFixedPosition())
        stream.dumpProperty("is in fixed position", true);

    if (rect.isRubyText())
        stream.dumpProperty("is ruby text", true);

    if (rect.behavior() == SelectionRenderingBehavior::UseIndividualQuads)
        stream.dumpProperty("using individual quads", true);

    stream.dumpProperty("page number", rect.pageNumber());
    return stream;
}

} // namespace WebCore
