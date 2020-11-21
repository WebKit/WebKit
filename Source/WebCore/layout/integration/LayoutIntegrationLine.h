/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatRect.h"

namespace WebCore {
namespace LayoutIntegration {

class Line {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct EnclosingTopAndBottom {
        // This values encloses the root inline box and any other inline level box's border box.
        float top { 0 };
        float bottom { 0 };
    };
    Line(size_t firstRunIndex, size_t runCount, const FloatRect& lineBoxRect, EnclosingTopAndBottom enclosingTopAndBottom, const FloatRect& scrollableOverflow, const FloatRect& inkOverflow, float baseline, float contentLeftOffset, float contentWidth)
        : m_firstRunIndex(firstRunIndex)
        , m_runCount(runCount)
        , m_lineBoxRect(lineBoxRect)
        , m_enclosingTopAndBottom(enclosingTopAndBottom)
        , m_scrollableOverflow(scrollableOverflow)
        , m_inkOverflow(inkOverflow)
        , m_baseline(baseline)
        , m_contentLeftOffset(contentLeftOffset)
        , m_contentWidth(contentWidth)
    {
    }

    size_t firstRunIndex() const { return m_firstRunIndex; }
    size_t runCount() const { return m_runCount; }

    float lineBoxTop() const { return m_lineBoxRect.y(); }
    float lineBoxBottom() const { return m_lineBoxRect.maxY(); }
    float lineBoxLeft() const { return m_lineBoxRect.x(); }
    float lineBoxRight() const { return m_lineBoxRect.maxX(); }

    float enclosingContentTop() const { return m_enclosingTopAndBottom.top; }
    float enclosingContentBottom() const { return m_enclosingTopAndBottom.bottom; }

    const FloatRect& scrollableOverflow() const { return m_scrollableOverflow; }
    const FloatRect& inkOverflow() const { return m_inkOverflow; }

    float baseline() const { return m_baseline; }

    float contentLeftOffset() const { return m_contentLeftOffset; }
    float contentWidth() const { return m_contentWidth; }

private:
    size_t m_firstRunIndex { 0 };
    size_t m_runCount { 0 };
    // This is line box geometry (see https://www.w3.org/TR/css-inline-3/#line-box).
    FloatRect m_lineBoxRect;
    // Enclosing top and bottom includes all inline level boxes (border box) vertically.
    // While the line box usually enclose them as well, its vertical geometry is based on
    // the layout bounds of the inline level boxes which may be different when line-height is present.
    EnclosingTopAndBottom m_enclosingTopAndBottom;
    FloatRect m_scrollableOverflow;
    FloatRect m_inkOverflow;
    float m_baseline { 0 };
    float m_contentLeftOffset { 0 };
    float m_contentWidth { 0 };
};

}
}

#endif
