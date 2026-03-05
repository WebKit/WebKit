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

#include <WebCore/FontBaseline.h>
#include <WebCore/InlineRect.h>
#include <WebCore/TextRun.h>
#include <WebCore/TextUtil.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace InlineDisplay {

struct Content;

class Line {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Line);
public:
    struct EnclosingTopAndBottom {
        // This values encloses the root inline box and any other inline level box's border box.
        float top { 0 };
        float bottom { 0 };
    };
    Line(bool hasInflowBox, bool hasContentfulBox, bool hasBlockLevelBox, const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, const FloatRect& contentOverflow, EnclosingTopAndBottom, float alignmentBaseline, FontBaseline baselineType, float contentLogicalLeft, float contentLogicalLeftIgnoringInlineDirection, float contentLogicalWidth, bool isLeftToRightDirection, bool isHorizontal, bool isTruncatedInBlockDirection);
    Line(const FloatRect& lineBoxRect, EnclosingTopAndBottom enclosingLogicalTopAndBottom, float alignmentBaseline, float contentLogicalLeft, float contentLogicalWidth);

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
    const FloatRect& inkOverflow() const { return m_inkOverflow; }

    float enclosingContentLogicalTop() const { return m_enclosingLogicalTopAndBottom.top; }
    float enclosingContentLogicalBottom() const { return m_enclosingLogicalTopAndBottom.bottom; }

    float baseline() const { return m_alignmentBaseline; }
    FontBaseline baselineType() const { return m_baselineType; }

    bool isHorizontal() const { return m_isHorizontal; }
    bool isLeftToRightInlineDirection() const { return m_isLeftToRightDirection; }

    float contentLogicalLeft() const { return m_contentLogicalLeft; }
    // This is "visual" left in inline direction (it is still considered logical as there's no flip for writing mode).
    float contentLogicalLeftIgnoringInlineDirection() const { return m_contentLogicalLeftIgnoringInlineDirection; }
    float contentLogicalWidth() const { return m_contentLogicalWidth; }

    size_t firstBoxIndex() const { return m_firstBoxIndex; }
    size_t lastBoxIndex() const { return firstBoxIndex() + boxCount() - 1; }
    size_t boxCount() const { return m_boxCount; }
    bool isFirstAfterPageBreak() const { return m_isFirstAfterPageBreak; }

    struct Ellipsis {
        enum class Type : uint8_t { Inline, Block };
        Type type { Type::Inline };
        // This is visual rect ignoring block direction.
        FloatRect visualRect;
        AtomString text;
    };
    bool hasEllipsis() const { return m_hasEllipsis; }

    bool isFullyTruncatedInBlockDirection() const { return m_isFullyTruncatedInBlockDirection; }

    bool hasContentAfterEllipsisBox() const { return m_hasContentAfterEllipsisBox; }
    void setHasContentAfterEllipsisBox() { m_hasContentAfterEllipsisBox = true; }

    bool hasInflowBox() const { return m_hasInflowBox; }
    bool hasContentfulInFlowBox() const { return m_hasContentfulBox && m_hasInflowBox; }
    bool hasInlineLevelBox() const { return hasInflowBox() && !hasBlockLevelBox(); }
    bool hasContentfulInlineLevelBox() const { return hasContentfulInFlowBox() && hasInlineLevelBox(); }
    bool hasBlockLevelBox() const { return m_hasBlockLevelBox; }

    void setFirstBoxIndex(size_t firstBoxIndex) { m_firstBoxIndex = firstBoxIndex; }
    void setBoxCount(size_t boxCount) { m_boxCount = boxCount; }
    void setIsFirstAfterPageBreak() { m_isFirstAfterPageBreak = true; }
    void setInkOverflow(const FloatRect inkOverflowRect) { m_inkOverflow = inkOverflowRect; }
    void setScrollableOverflow(const FloatRect scrollableOverflow) { m_scrollableOverflow = scrollableOverflow; }
    void setLineBoxRectForSVGText(const FloatRect&);
    void setHasEllipsis() { m_hasEllipsis = true; }

private:
    friend struct Content;
    void moveInBlockDirection(float offset);
    void shrinkInBlockDirection(float delta);

    uint32_t m_firstBoxIndex { 0 };
    uint32_t m_boxCount { 0 };

    // This is line box geometry (see https://www.w3.org/TR/css-inline-3/#line-box).
    FloatRect m_lineBoxRect;
    FloatRect m_lineBoxLogicalRect;
    FloatRect m_scrollableOverflow;
    // FIXME: This should be transitioned to spec aligned overflow value.
    FloatRect m_inkOverflow;
    // Enclosing top and bottom includes all inline level boxes (border box) vertically.
    // While the line box usually enclose them as well, its vertical geometry is based on
    // the layout bounds of the inline level boxes which may be different when line-height is present.
    EnclosingTopAndBottom m_enclosingLogicalTopAndBottom;
    float m_alignmentBaseline { 0.f };
    // Content is mostly in flush with the line box edge except for cases like text-align.
    float m_contentLogicalLeft { 0.f };
    float m_contentLogicalLeftIgnoringInlineDirection { 0.f };
    float m_contentLogicalWidth { 0.f };
    FontBaseline m_baselineType : 2 { FontBaseline::Alphabetic };
    bool m_isLeftToRightDirection : 1 { true };
    bool m_isHorizontal : 1 { true };
    bool m_isFirstAfterPageBreak : 1 { false };
    bool m_isFullyTruncatedInBlockDirection : 1 { false };
    bool m_hasContentAfterEllipsisBox : 1 { false };
    bool m_hasInflowBox : 1 { false };
    bool m_hasContentfulBox : 1 { false };
    bool m_hasBlockLevelBox : 1 { false };
    bool m_hasEllipsis : 1 { false };
};

inline Line::Line(bool hasInflowBox, bool hasContentfulBox, bool hasBlockLevelBox, const FloatRect& lineBoxLogicalRect, const FloatRect& lineBoxRect, const FloatRect& scrollableOverflow, EnclosingTopAndBottom enclosingLogicalTopAndBottom, float alignmentBaseline, FontBaseline baselineType, float contentLogicalLeft, float contentLogicalLeftIgnoringInlineDirection, float contentLogicalWidth, bool isLeftToRightDirection, bool isHorizontal, bool isTruncatedInBlockDirection)
    : m_lineBoxRect(lineBoxRect)
    , m_lineBoxLogicalRect(lineBoxLogicalRect)
    , m_scrollableOverflow(scrollableOverflow)
    , m_enclosingLogicalTopAndBottom(enclosingLogicalTopAndBottom)
    , m_alignmentBaseline(alignmentBaseline)
    , m_contentLogicalLeft(contentLogicalLeft)
    , m_contentLogicalLeftIgnoringInlineDirection(contentLogicalLeftIgnoringInlineDirection)
    , m_contentLogicalWidth(contentLogicalWidth)
    , m_baselineType(baselineType)
    , m_isLeftToRightDirection(isLeftToRightDirection)
    , m_isHorizontal(isHorizontal)
    , m_isFullyTruncatedInBlockDirection(isTruncatedInBlockDirection)
    , m_hasInflowBox(hasInflowBox)
    , m_hasContentfulBox(hasContentfulBox)
    , m_hasBlockLevelBox(hasBlockLevelBox)
{
}

inline Line::Line(const FloatRect& lineBoxRect, EnclosingTopAndBottom enclosingLogicalTopAndBottom, float alignmentBaseline, float contentLogicalLeft, float contentLogicalWidth)
    : m_lineBoxRect(lineBoxRect)
    , m_lineBoxLogicalRect(lineBoxRect)
    , m_scrollableOverflow(lineBoxRect)
    , m_enclosingLogicalTopAndBottom(enclosingLogicalTopAndBottom)
    , m_alignmentBaseline(alignmentBaseline)
    , m_contentLogicalLeft(contentLogicalLeft)
    , m_contentLogicalLeftIgnoringInlineDirection(contentLogicalLeft)
    , m_contentLogicalWidth(contentLogicalWidth)
    , m_hasInflowBox(true)
    , m_hasContentfulBox(true)
{
}

inline void Line::moveInBlockDirection(float offset)
{
    auto physicalOffset = isHorizontal() ? FloatSize { { }, offset } : FloatSize { offset, { } };

    m_lineBoxRect.move(physicalOffset);
    m_scrollableOverflow.move(physicalOffset);
    m_inkOverflow.move(physicalOffset);
    m_lineBoxLogicalRect.move({ { }, offset });
    m_enclosingLogicalTopAndBottom.top += offset;
    m_enclosingLogicalTopAndBottom.bottom += offset;
}

inline void Line::shrinkInBlockDirection(float delta)
{
    auto physicalDelta = isHorizontal() ? FloatSize { { }, delta } : FloatSize { delta, { } };

    m_lineBoxRect.contract(physicalDelta);
    m_scrollableOverflow.contract(physicalDelta);
    m_inkOverflow.contract(physicalDelta);
    m_lineBoxLogicalRect.contract({ { }, delta });
    m_enclosingLogicalTopAndBottom.bottom -= delta;
}

inline void Line::setLineBoxRectForSVGText(const FloatRect& rect)
{
    m_lineBoxRect = rect;
    m_scrollableOverflow = rect;
    m_inkOverflow = rect;
    m_lineBoxLogicalRect = m_isHorizontal ? rect : rect.transposedRect();
    m_enclosingLogicalTopAndBottom.top = m_lineBoxLogicalRect.y();
    m_enclosingLogicalTopAndBottom.bottom = m_lineBoxLogicalRect.maxY();
}

}
}

