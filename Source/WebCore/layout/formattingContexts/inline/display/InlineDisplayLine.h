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
#include "TextRun.h"
#include "TextUtil.h"

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
    Line(const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, const FloatRect& contentOverflow, EnclosingTopAndBottom, float aligmentBaseline, FontBaseline baselineType, float contentLogicalLeft, float contentLogicalLeftIgnoringInlineDirection, float contentLogicalWidth, bool isLeftToRightDirection, bool isHorizontal);

    float left() const { return m_lineBoxRect.x(); }
    float right() const { return m_lineBoxRect.maxX(); }
    float top() const { return m_lineBoxRect.y(); }
    float bottom() const { return m_lineBoxRect.maxY(); }

    FloatPoint topLeft() const { return m_lineBoxRect.location(); }

    float lineBoxTop() const { return m_lineBoxRect.y(); }
    float lineBoxBottom() const { return m_lineBoxRect.maxY(); }
    float lineBoxLeft() const { return m_lineBoxRect.x(); }
    float lineBoxRight() const { return m_lineBoxRect.maxX(); }
    float lineBoxHeight() const { return m_lineBoxRect.height(); }
    float lineBoxWidth() const { return m_lineBoxRect.width(); }

    const FloatRect& lineBoxRect() const { return m_lineBoxRect; }
    const FloatRect& lineBoxLogicalRect() const { return m_lineBoxLogicalRect; }
    const FloatRect& scrollableOverflow() const { return m_scrollableOverflow; }
    const FloatRect& contentOverflow() const { return m_contentOverflow; }
    const FloatRect& inkOverflow() const { return m_inkOverflow; }

    FloatRect visibleRectIgnoringBlockDirection() const;

    float enclosingContentLogicalTop() const { return m_enclosingLogicalTopAndBottom.top; }
    float enclosingContentLogicalBottom() const { return m_enclosingLogicalTopAndBottom.bottom; }

    float baseline() const { return m_aligmentBaseline; }
    FontBaseline baselineType() const { return m_baselineType; }

    bool isHorizontal() const { return m_isHorizontal; }

    bool hasEllipsis() const { return m_ellipsisVisualRect.has_value(); }
    std::optional<FloatRect> ellipsisVisualRect() const { return m_ellipsisVisualRect; }
    // FIXME: This should not be part of Line.
    TextRun ellipsisText() const { return TextRun { Layout::TextUtil::ellipsisTextRun(isHorizontal()) }; }

    float contentLogicalLeft() const { return m_contentLogicalLeft; }
    // This is "visual" left in inline direction (it is still considered logical as there's no flip for writing mode).
    float contentLogicalLeftIgnoringInlineDirection() const { return m_contentLogicalLeftIgnoringInlineDirection; }
    float contentLogicalWidth() const { return m_contentLogicalWidth; }

    size_t firstBoxIndex() const { return m_firstBoxIndex; }
    size_t boxCount() const { return m_boxCount; }
    bool isFirstAfterPageBreak() const { return m_isFirstAfterPageBreak; }

    void moveVertically(float offset);
    void setEllipsisVisualRect(const FloatRect& ellipsisVisualRect) { m_ellipsisVisualRect = ellipsisVisualRect; }

    void setFirstBoxIndex(size_t firstBoxIndex) { m_firstBoxIndex = firstBoxIndex; }
    void setBoxCount(size_t boxCount) { m_boxCount = boxCount; }
    void setIsFirstAfterPageBreak() { m_isFirstAfterPageBreak = true; }
    void setInkOverflow(const FloatRect inkOverflowRect) { m_inkOverflow = inkOverflowRect; }
    void setScrollableOverflow(const FloatRect scrollableOverflow) { m_scrollableOverflow = scrollableOverflow; }

private:
    // FIXME: Move these to a side structure.
    size_t m_firstBoxIndex { 0 };
    size_t m_boxCount { 0 };

    // This is line box geometry (see https://www.w3.org/TR/css-inline-3/#line-box).
    FloatRect m_lineBoxRect;
    FloatRect m_lineBoxLogicalRect;
    FloatRect m_scrollableOverflow;
    // FIXME: Merge this with scrollable overflow (see InlineContentBuilder::updateLineOverflow).
    FloatRect m_contentOverflow;
    // FIXME: This should be transitioned to spec aligned overflow value.
    FloatRect m_inkOverflow;
    // Enclosing top and bottom includes all inline level boxes (border box) vertically.
    // While the line box usually enclose them as well, its vertical geometry is based on
    // the layout bounds of the inline level boxes which may be different when line-height is present.
    EnclosingTopAndBottom m_enclosingLogicalTopAndBottom;
    float m_aligmentBaseline { 0.f };
    // Content is mostly in flush with the line box edge except for cases like text-align.
    float m_contentLogicalLeft { 0.f };
    float m_contentLogicalLeftIgnoringInlineDirection { 0.f };
    float m_contentLogicalWidth { 0.f };
    FontBaseline m_baselineType { AlphabeticBaseline };
    bool m_isLeftToRightDirection : 1 { true };
    bool m_isHorizontal : 1 { true };
    bool m_isFirstAfterPageBreak : 1 { false };
    // This is visual rect ignoring block direction.
    std::optional<FloatRect> m_ellipsisVisualRect { };
};

inline Line::Line(const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, const FloatRect& contentOverflow, EnclosingTopAndBottom enclosingLogicalTopAndBottom, float aligmentBaseline, FontBaseline baselineType, float contentLogicalLeft, float contentLogicalLeftIgnoringInlineDirection, float contentLogicalWidth, bool isLeftToRightDirection, bool isHorizontal)
    : m_lineBoxRect(lineBoxRect)
    , m_lineBoxLogicalRect(lineBoxLogicalRect)
    , m_contentOverflow(contentOverflow)
    , m_enclosingLogicalTopAndBottom(enclosingLogicalTopAndBottom)
    , m_aligmentBaseline(aligmentBaseline)
    , m_contentLogicalLeft(contentLogicalLeft)
    , m_contentLogicalLeftIgnoringInlineDirection(contentLogicalLeftIgnoringInlineDirection)
    , m_contentLogicalWidth(contentLogicalWidth)
    , m_baselineType(baselineType)
    , m_isLeftToRightDirection(isLeftToRightDirection)
    , m_isHorizontal(isHorizontal)
{
}

inline void Line::moveVertically(float offset)
{
    m_lineBoxRect.move({ { }, offset });
    m_lineBoxLogicalRect.move({ { }, offset });
    m_scrollableOverflow.move({ { }, offset });
    m_contentOverflow.move({ { }, offset });
    m_inkOverflow.move({ { }, offset });
    m_enclosingLogicalTopAndBottom.top += offset;
    m_enclosingLogicalTopAndBottom.bottom += offset;
    if (m_ellipsisVisualRect.has_value())
        m_ellipsisVisualRect->move({ { }, offset });
}

inline FloatRect Line::visibleRectIgnoringBlockDirection() const
{
    if (!hasEllipsis())
        return m_inkOverflow;
    if (m_isLeftToRightDirection) {
        auto visibleLineBoxRight = std::min(m_lineBoxRect.maxX(), m_ellipsisVisualRect->maxX());
        return { m_lineBoxRect.location(), FloatPoint { visibleLineBoxRight, m_lineBoxRect.maxY() } };
    }
    auto visibleLineBoxLeft = std::max(m_lineBoxRect.x(), m_ellipsisVisualRect->x());
    return { FloatPoint { visibleLineBoxLeft, m_lineBoxRect.y() }, FloatPoint { m_lineBoxRect.maxX(), m_lineBoxRect.maxY() } };
}

}
}

