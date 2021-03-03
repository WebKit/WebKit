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

#include "config.h"
#include "InlineLineBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBoxGeometry.h"
#include "LayoutContainerBox.h"

namespace WebCore {
namespace Layout {

LineBox::InlineLevelBox::InlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize, Type type)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect({ }, logicalLeft, logicalSize.width(), logicalSize.height())
    , m_type(type)
{
}

void LineBox::InlineLevelBox::setBaseline(InlineLayoutUnit baseline)
{
    // FIXME: Remove legacy rounding.
    m_baseline = roundToInt(baseline);
}

void LineBox::InlineLevelBox::setDescent(InlineLayoutUnit descent)
{
    // FIXME: Remove legacy rounding.
    m_descent = roundToInt(descent);
}

void LineBox::InlineLevelBox::setLayoutBounds(const LayoutBounds& layoutBounds)
{
    // FIXME: Remove legacy rounding.
    m_layoutBounds = { InlineLayoutUnit(roundToInt(layoutBounds.ascent)), InlineLayoutUnit(roundToInt(layoutBounds.descent)) };
}

void LineBox::InlineLevelBox::setLogicalTop(InlineLayoutUnit logicalTop)
{
    // FIXME: Remove legacy rounding.
    m_logicalRect.setTop(roundToInt(logicalTop));
}

void LineBox::InlineLevelBox::setLogicalHeight(InlineLayoutUnit logicalHeight)
{
    // FIXME: Remove legacy rounding.
    m_logicalRect.setHeight(roundToInt(logicalHeight));
}

void LineBox::InlineLevelBox::setHasContent()
{
    ASSERT(isInlineBox());
    m_hasContent = true;
}

bool LineBox::InlineLevelBox::hasLineBoxRelativeAlignment() const
{
    auto verticalAlignment = layoutBox().style().verticalAlign();
    return verticalAlignment == VerticalAlign::Top || verticalAlignment == VerticalAlign::Bottom;
}

LineBox::LineBox(const InlineLayoutPoint& logicalTopleft, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, size_t numberOfRuns)
    : m_logicalRect(logicalTopleft, InlineLayoutSize { lineLogicalWidth, { } })
    , m_contentLogicalWidth(contentLogicalWidth)
{
    m_nonRootInlineLevelBoxList.reserveInitialCapacity(numberOfRuns);
    m_inlineLevelBoxRectMap.reserveInitialCapacity(numberOfRuns);
}

void LineBox::addRootInlineBox(std::unique_ptr<InlineLevelBox>&& rootInlineBox)
{
    std::exchange(m_rootInlineBox, WTFMove(rootInlineBox));
    m_inlineLevelBoxRectMap.set(&m_rootInlineBox->layoutBox(), m_rootInlineBox.get());
}

void LineBox::addInlineLevelBox(std::unique_ptr<InlineLevelBox>&& inlineLevelBox)
{
    m_boxTypes.add(inlineLevelBox->type());
    m_inlineLevelBoxRectMap.set(&inlineLevelBox->layoutBox(), inlineLevelBox.get());
    m_nonRootInlineLevelBoxList.append(WTFMove(inlineLevelBox));
}

InlineRect LineBox::logicalRectForTextRun(const Line::Run& run) const
{
    ASSERT(run.isText() || run.isSoftLineBreak());
    auto* parentInlineBox = &inlineLevelBoxForLayoutBox(run.layoutBox().parent());
    ASSERT(parentInlineBox->isInlineBox());
    auto& fontMetrics = parentInlineBox->style().fontMetrics();
    auto runlogicalTop = parentInlineBox->logicalTop() + parentInlineBox->baseline() - fontMetrics.ascent();

    while (parentInlineBox != m_rootInlineBox.get() && !parentInlineBox->hasLineBoxRelativeAlignment()) {
        parentInlineBox = &inlineLevelBoxForLayoutBox(parentInlineBox->layoutBox().parent());
        ASSERT(parentInlineBox->isInlineBox());
        runlogicalTop += parentInlineBox->logicalTop();
    }
    InlineLayoutUnit logicalHeight = fontMetrics.height();
    return { runlogicalTop, m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + run.logicalLeft(), run.logicalWidth(), logicalHeight };
}

InlineRect LineBox::logicalRectForLineBreakBox(const Box& layoutBox) const
{
    ASSERT(layoutBox.isLineBreakBox());
    return logicalRectForInlineLevelBox(layoutBox);
}

InlineRect LineBox::logicalRectForInlineLevelBox(const Box& layoutBox) const
{
    ASSERT(layoutBox.isInlineLevelBox() || layoutBox.isLineBreakBox());
    // Inline level boxes are relative to their parent unless the vertical alignment makes them relative to the line box (e.g. top, bottom).
    auto* inlineBox = &inlineLevelBoxForLayoutBox(layoutBox);
    auto inlineBoxLogicalRect = inlineBox->logicalRect();
    if (inlineBox->hasLineBoxRelativeAlignment())
        return inlineBoxLogicalRect;

    // Fast path for inline level boxes on the root inline box (e.g <div><img></div>).
    if (&layoutBox.parent() == &m_rootInlineBox->layoutBox()) {
        inlineBoxLogicalRect.moveVertically(m_rootInlineBox->logicalTop());
        return inlineBoxLogicalRect;
    }

    // e.g <div><span><img></span></div>
    auto inlineBoxAbsolutelogicalTop = inlineBoxLogicalRect.top();
    while (inlineBox != m_rootInlineBox.get() && !inlineBox->hasLineBoxRelativeAlignment()) {
        inlineBox = &inlineLevelBoxForLayoutBox(inlineBox->layoutBox().parent());
        ASSERT(inlineBox->isInlineBox());
        inlineBoxAbsolutelogicalTop += inlineBox->logicalTop();
    }
    return InlineRect { inlineBoxAbsolutelogicalTop, inlineBoxLogicalRect.left(), inlineBoxLogicalRect.width(), inlineBoxLogicalRect.height() };
}

InlineRect LineBox::logicalBorderBoxForAtomicInlineLevelBox(const Box& layoutBox, const BoxGeometry& boxGeometry) const
{
    ASSERT(layoutBox.isAtomicInlineLevelBox());
    auto logicalRect = logicalRectForInlineLevelBox(layoutBox);
    // Inline level boxes use their margin box for vertical alignment. Let's covert them to border boxes.
    logicalRect.moveVertically(boxGeometry.marginBefore());
    auto verticalMargin = boxGeometry.marginBefore() + boxGeometry.marginAfter();
    logicalRect.expandVertically(-verticalMargin);
    return logicalRect;
}

InlineRect LineBox::logicalBorderBoxForInlineBox(const Box& layoutBox, const BoxGeometry& boxGeometry) const
{
    auto logicalRect = logicalRectForInlineLevelBox(layoutBox);
    // This logical rect is as tall as the "text" content is. Let's adjust with vertical border and padding.
    auto verticalBorderAndPadding = boxGeometry.verticalBorder() + boxGeometry.verticalPadding().valueOr(0_lu);
    logicalRect.expandVertically(verticalBorderAndPadding);
    logicalRect.moveVertically(-(boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0_lu)));
    return logicalRect;
}

} // namespace Layout
} // namespace WebCore

#endif
