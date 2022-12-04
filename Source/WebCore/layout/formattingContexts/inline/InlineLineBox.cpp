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

#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"

namespace WebCore {
namespace Layout {

LineBox::LineBox(const Box& rootLayoutBox, InlineLayoutUnit rootInlineBoxAlignmentOffset, InlineLayoutUnit contentLogicalWidth, size_t lineIndex, size_t nonSpanningInlineLevelBoxCount)
    : m_lineIndex(lineIndex)
    , m_rootInlineBoxAlignmentOffset(rootInlineBoxAlignmentOffset)
    , m_rootInlineBox({ rootLayoutBox, !lineIndex ? rootLayoutBox.firstLineStyle() : rootLayoutBox.style(), { }, InlineLayoutSize { contentLogicalWidth, { } }, InlineLevelBox::Type::RootInlineBox })
{
    m_nonRootInlineLevelBoxList.reserveInitialCapacity(nonSpanningInlineLevelBoxCount);
    m_nonRootInlineLevelBoxMap.reserveInitialCapacity(nonSpanningInlineLevelBoxCount);
}

void LineBox::addInlineLevelBox(InlineLevelBox&& inlineLevelBox)
{
    m_boxTypes.add(inlineLevelBox.type());
    m_nonRootInlineLevelBoxMap.set(&inlineLevelBox.layoutBox(), m_nonRootInlineLevelBoxList.size());
    m_nonRootInlineLevelBoxList.append(WTFMove(inlineLevelBox));
}

InlineRect LineBox::logicalRectForTextRun(const Line::Run& run) const
{
    ASSERT(run.isText() || run.isSoftLineBreak());
    auto* parentInlineBox = &inlineLevelBoxForLayoutBox(run.layoutBox().parent());
    ASSERT(parentInlineBox->isInlineBox());
    auto runlogicalTop = parentInlineBox->logicalTop() - parentInlineBox->inlineBoxContentOffsetForLeadingTrim();
    InlineLayoutUnit logicalHeight = parentInlineBox->primarymetricsOfPrimaryFont().height();

    while (parentInlineBox != &m_rootInlineBox && !parentInlineBox->hasLineBoxRelativeAlignment()) {
        parentInlineBox = &inlineLevelBoxForLayoutBox(parentInlineBox->layoutBox().parent());
        ASSERT(parentInlineBox->isInlineBox());
        runlogicalTop += (parentInlineBox->logicalTop() - parentInlineBox->inlineBoxContentOffsetForLeadingTrim());
    }
    return { runlogicalTop, run.logicalLeft(), run.logicalWidth(), logicalHeight };
}

InlineRect LineBox::logicalRectForLineBreakBox(const Box& layoutBox) const
{
    ASSERT(layoutBox.isLineBreakBox());
    return logicalRectForInlineLevelBox(layoutBox);
}

InlineLayoutUnit LineBox::inlineLevelBoxAbsoluteTop(const InlineLevelBox& inlineLevelBox) const
{
    // Inline level boxes are relative to their parent unless the vertical alignment makes them relative to the line box (e.g. top, bottom).
    auto top = inlineLevelBox.logicalTop();
    if (inlineLevelBox.isRootInlineBox() || inlineLevelBox.hasLineBoxRelativeAlignment())
        return top;

    // Fast path for inline level boxes on the root inline box (e.g <div><img></div>).
    if (&inlineLevelBox.layoutBox().parent() == &m_rootInlineBox.layoutBox())
        return top + m_rootInlineBox.logicalTop();

    // Nested inline content e.g <div><span><img></span></div>
    auto* ancestorInlineBox = &inlineLevelBox;
    while (ancestorInlineBox != &m_rootInlineBox && !ancestorInlineBox->hasLineBoxRelativeAlignment()) {
        ancestorInlineBox = &inlineLevelBoxForLayoutBox(ancestorInlineBox->layoutBox().parent());
        ASSERT(ancestorInlineBox->isInlineBox());
        top += ancestorInlineBox->logicalTop();
    }
    return top;
}

InlineRect LineBox::logicalRectForInlineLevelBox(const Box& layoutBox) const
{
    ASSERT(layoutBox.isInlineLevelBox() || layoutBox.isLineBreakBox());
    auto& inlineBox = inlineLevelBoxForLayoutBox(layoutBox);
    auto inlineBoxLogicalRect = inlineBox.logicalRect();
    return InlineRect { inlineLevelBoxAbsoluteTop(inlineBox), inlineBoxLogicalRect.left(), inlineBoxLogicalRect.width(), inlineBoxLogicalRect.height() };
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
    auto verticalBorderAndPadding = boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu);
    logicalRect.expandVertically(verticalBorderAndPadding);
    logicalRect.moveVertically(-(boxGeometry.borderBefore() + boxGeometry.paddingBefore().value_or(0_lu)));
    return logicalRect;
}

} // namespace Layout
} // namespace WebCore

