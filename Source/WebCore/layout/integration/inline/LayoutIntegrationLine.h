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

#include "FloatRect.h"
#include "LayoutBox.h"
#include "TextRun.h"
#include "TextUtil.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace LayoutIntegration {

class Line {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Ellipsis {
        FloatRect visualRect;
        bool isLeftPositioned { true };
    };
    Line(size_t firstBoxIndex, size_t boxCount, const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, float enclosingContentTop, float enclosingContentBottom, const FloatRect& scrollableOverflow, const FloatRect& inkOverflow, float baseline, FontBaseline baselineType, float contentVisualOffsetInInlineDirection, float contentLogicalWidth, bool isHorizontal, std::optional<Ellipsis> ellipsis, bool isFirstAfterPageBreak = false)
        : m_firstBoxIndex(firstBoxIndex)
        , m_boxCount(boxCount)
        , m_lineBoxRect(lineBoxRect)
        , m_lineBoxLogicalRect(lineBoxLogicalRect)
        , m_enclosingContentTop(enclosingContentTop)
        , m_enclosingContentBottom(enclosingContentBottom)
        , m_scrollableOverflow(scrollableOverflow)
        , m_inkOverflow(inkOverflow)
        , m_baseline(baseline)
        , m_contentVisualOffsetInInlineDirection(contentVisualOffsetInInlineDirection)
        , m_contentLogicalWidth(contentLogicalWidth)
        , m_baselineType(baselineType)
        , m_isHorizontal(isHorizontal)
        , m_ellipsis(ellipsis)
        , m_isFirstAfterPageBreak(isFirstAfterPageBreak)
    {
    }

    size_t firstBoxIndex() const { return m_firstBoxIndex; }
    size_t boxCount() const { return m_boxCount; }

    float lineBoxTop() const { return m_lineBoxRect.y(); }
    float lineBoxBottom() const { return m_lineBoxRect.maxY(); }
    float lineBoxLeft() const { return m_lineBoxRect.x(); }
    float lineBoxRight() const { return m_lineBoxRect.maxX(); }
    float lineBoxHeight() const { return m_lineBoxRect.height(); }
    float lineBoxWidth() const { return m_lineBoxRect.width(); }

    FloatRect lineBoxLogicalRect() const { return m_lineBoxLogicalRect; }

    float enclosingContentTop() const { return m_enclosingContentTop; }
    float enclosingContentBottom() const { return m_enclosingContentBottom; }

    const FloatRect& scrollableOverflow() const { return m_scrollableOverflow; }
    const FloatRect& inkOverflow() const { return m_inkOverflow; }
    FloatRect visibleRectIgnoringBlockDirection() const;

    float baseline() const { return m_baseline; }
    FontBaseline baselineType() const { return m_baselineType; }

    bool hasEllipsis() const { return m_ellipsis.has_value(); }
    FloatRect ellipsisVisualRect() const { return m_ellipsis->visualRect; }
    TextRun ellipsisText() const { return TextRun { Layout::TextUtil::ellipsisTextRun(isHorizontal()) }; }

    bool isHorizontal() const { return m_isHorizontal; }

    float contentVisualOffsetInInlineDirection() const { return m_contentVisualOffsetInInlineDirection; }
    float contentLogicalWidth() const { return m_contentLogicalWidth; }

    bool isFirstAfterPageBreak() const { return m_isFirstAfterPageBreak; }

private:
    size_t m_firstBoxIndex { 0 };
    size_t m_boxCount { 0 };
    // This is line box geometry (see https://www.w3.org/TR/css-inline-3/#line-box).
    FloatRect m_lineBoxRect;
    FloatRect m_lineBoxLogicalRect;
    // Enclosing top and bottom includes all inline level boxes (border box) vertically.
    // While the line box usually enclose them as well, its vertical geometry is based on
    // the layout bounds of the inline level boxes which may be different when line-height is present.
    float m_enclosingContentTop { 0 };
    float m_enclosingContentBottom { 0 };
    FloatRect m_scrollableOverflow;
    FloatRect m_inkOverflow;
    float m_baseline { 0.f };
    float m_contentVisualOffsetInInlineDirection { 0.f };
    float m_contentLogicalWidth { 0.f };
    FontBaseline m_baselineType { AlphabeticBaseline };
    bool m_isHorizontal { true };
    // This is visual rect ignoring block direction.
    std::optional<Ellipsis> m_ellipsis;
    // FIXME: This is to match paginated legacy lines. Move it to some other, paginated related structure.
    bool m_isFirstAfterPageBreak { false };
};

inline FloatRect Line::visibleRectIgnoringBlockDirection() const
{
    if (!hasEllipsis())
        return m_inkOverflow;
    if (m_ellipsis->isLeftPositioned) {
        auto visibleLineBoxRight = std::min(m_lineBoxRect.maxX(), m_ellipsis->visualRect.maxX());
        return { m_lineBoxRect.location(), FloatPoint { visibleLineBoxRight, m_lineBoxRect.maxY() } };
    }
    auto visibleLineBoxLeft = std::max(m_lineBoxRect.x(), m_ellipsis->visualRect.x());
    return { FloatPoint { visibleLineBoxLeft, m_lineBoxRect.y() }, FloatPoint { m_lineBoxRect.maxX(), m_lineBoxRect.maxY() } };
}

}
}

