/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "FontBaseline.h"
#include "InlineRect.h"

namespace WebCore {
namespace InlineDisplay {

class Line {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct EnclosingTopAndBottom {
        // This values encloses the root inline box and any other inline level box's border box.
        float top { 0 };
        float bottom { 0 };
    };
    Line(const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, const FloatRect& scrollableOverflow, EnclosingTopAndBottom, float aligmentBaseline, FontBaseline baselineType, float contentVisualOffsetInInlineDirection, float contentLogicalWidth, bool isHorizontal, std::optional<FloatRect> ellipsisVisualRect);

    float left() const { return m_lineBoxRect.x(); }
    float right() const { return m_lineBoxRect.maxX(); }
    float top() const { return m_lineBoxRect.y(); }
    float bottom() const { return m_lineBoxRect.maxY(); }

    FloatPoint topLeft() const { return m_lineBoxRect.location(); }

    const FloatRect& lineBoxRect() const { return m_lineBoxRect; }
    const FloatRect& lineBoxLogicalRect() const { return m_lineBoxLogicalRect; }
    const FloatRect& scrollableOverflow() const { return m_scrollableOverflow; }

    EnclosingTopAndBottom enclosingTopAndBottom() const { return m_enclosingTopAndBottom; }

    float baseline() const { return m_aligmentBaseline; }
    FontBaseline baselineType() const { return m_baselineType; }

    bool isHorizontal() const { return m_isHorizontal; }

    std::optional<FloatRect> ellipsisVisualRect() const { return m_ellipsisVisualRect; }

    float contentVisualOffsetInInlineDirection() const { return m_contentVisualOffsetInInlineDirection; }
    float contentLogicalWidth() const { return m_contentLogicalWidth; }

    void moveVertically(float offset) { m_lineBoxRect.move({ { }, offset }); }

private:
    // This is line box geometry (see https://www.w3.org/TR/css-inline-3/#line-box).
    FloatRect m_lineBoxRect;
    FloatRect m_lineBoxLogicalRect;
    FloatRect m_scrollableOverflow;
    // Enclosing top and bottom includes all inline level boxes (border box) vertically.
    // While the line box usually enclose them as well, its vertical geometry is based on
    // the layout bounds of the inline level boxes which may be different when line-height is present.
    EnclosingTopAndBottom m_enclosingTopAndBottom;
    float m_aligmentBaseline { 0.f };
    float m_contentVisualOffsetInInlineDirection { 0.f };
    float m_contentLogicalWidth { 0.f };
    FontBaseline m_baselineType { AlphabeticBaseline };
    bool m_isHorizontal { true };
    // This is visual rect ignoring block direction.
    std::optional<FloatRect> m_ellipsisVisualRect { };
};

inline Line::Line(const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, const FloatRect& scrollableOverflow, EnclosingTopAndBottom enclosingTopAndBottom, float aligmentBaseline, FontBaseline baselineType, float contentVisualOffsetInInlineDirection, float contentLogicalWidth, bool isHorizontal, std::optional<FloatRect> ellipsisVisualRect)
    : m_lineBoxRect(lineBoxRect)
    , m_lineBoxLogicalRect(lineBoxLogicalRect)
    , m_scrollableOverflow(scrollableOverflow)
    , m_enclosingTopAndBottom(enclosingTopAndBottom)
    , m_aligmentBaseline(aligmentBaseline)
    , m_contentVisualOffsetInInlineDirection(contentVisualOffsetInInlineDirection)
    , m_contentLogicalWidth(contentLogicalWidth)
    , m_baselineType(baselineType)
    , m_isHorizontal(isHorizontal)
    , m_ellipsisVisualRect(ellipsisVisualRect)
{
}

}
}

