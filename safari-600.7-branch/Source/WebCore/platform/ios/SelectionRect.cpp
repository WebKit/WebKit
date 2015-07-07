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

#include "config.h"
#include "SelectionRect.h"

namespace WebCore {

SelectionRect::SelectionRect(const IntRect& rect, bool isHorizontal, int pageNumber)
    : m_rect(rect)
    , m_direction(LTR)
    , m_minX(0)
    , m_maxX(0)
    , m_maxY(0)
    , m_lineNumber(0)
    , m_isLineBreak(false)
    , m_isFirstOnLine(false)
    , m_isLastOnLine(false)
    , m_containsStart(false)
    , m_containsEnd(false)
    , m_isHorizontal(isHorizontal)
    , m_isInFixedPosition(false)
    , m_isRubyText(false)
    , m_pageNumber(pageNumber)
{
}

// FIXME: We should move some of these arguments to an auxillary struct.
SelectionRect::SelectionRect(const IntRect& rect, TextDirection direction, int minX, int maxX, int maxY,
    int lineNumber, bool isLineBreak, bool isFirstOnLine, bool isLastOnLine, bool containsStart, bool containsEnd,
    bool isHorizontal, bool isInFixedPosition, bool isRubyText, int pageNumber)
    : m_rect(rect)
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

SelectionRect::SelectionRect()
    : m_direction(LTR)
    , m_minX(0)
    , m_maxX(0)
    , m_maxY(0)
    , m_lineNumber(0)
    , m_isLineBreak(false)
    , m_isFirstOnLine(false)
    , m_isLastOnLine(false)
    , m_containsStart(false)
    , m_containsEnd(false)
    , m_isHorizontal(true)
    , m_isInFixedPosition(false)
    , m_isRubyText(false)
    , m_pageNumber(0)
{
}

} // namespace WebCore
