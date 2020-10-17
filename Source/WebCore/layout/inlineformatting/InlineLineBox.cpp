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
#include "LayoutContainerBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

LineBox::InlineLevelBox::InlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize,  Type type)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect({ }, logicalLeft, logicalSize.width(), logicalSize.height())
    , m_type(type)
{
}

bool LineBox::InlineLevelBox::hasLineBoxRelativeAlignment() const
{
    auto verticalAlignment = layoutBox().style().verticalAlign();
    return verticalAlignment == VerticalAlign::Top || verticalAlignment == VerticalAlign::Bottom;
}

LineBox::LineBox(InlineLayoutUnit contentLogicalWidth, IsLineVisuallyEmpty isLineVisuallyEmpty)
    : m_logicalSize(contentLogicalWidth, { })
    , m_isLineVisuallyEmpty(isLineVisuallyEmpty == IsLineVisuallyEmpty::Yes)
{
}

void LineBox::addRootInlineBox(std::unique_ptr<InlineLevelBox>&& rootInlineBox)
{
    std::exchange(m_rootInlineBox, WTFMove(rootInlineBox));
    m_inlineLevelBoxRectMap.set(&m_rootInlineBox->layoutBox(), m_rootInlineBox.get());
}

void LineBox::addInlineLevelBox(std::unique_ptr<InlineLevelBox>&& inlineLevelBox)
{
    m_inlineLevelBoxRectMap.set(&inlineLevelBox->layoutBox(), inlineLevelBox.get());
    m_nonRootInlineLevelBoxList.append(WTFMove(inlineLevelBox));
}

InlineRect LineBox::logicalRectForTextRun(const Line::Run& run) const
{
    ASSERT(run.isText() || run.isLineBreak());
    auto* parentInlineBox = &inlineLevelBoxForLayoutBox(run.layoutBox().parent());
    ASSERT(parentInlineBox->isInlineBox());
    auto& fontMetrics = parentInlineBox->fontMetrics();
    auto runlogicalTop = parentInlineBox->logicalTop() + parentInlineBox->baseline() - fontMetrics.ascent();

    while (parentInlineBox != m_rootInlineBox.get() && !parentInlineBox->hasLineBoxRelativeAlignment()) {
        parentInlineBox = &inlineLevelBoxForLayoutBox(parentInlineBox->layoutBox().parent());
        ASSERT(parentInlineBox->isInlineBox());
        runlogicalTop += parentInlineBox->logicalTop();
    }
    InlineLayoutUnit logicalHeight = fontMetrics.height();
    return { runlogicalTop, m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + run.logicalLeft(), run.logicalWidth(), logicalHeight };
}

InlineRect LineBox::logicalRectForInlineLevelBox(const Box& layoutBox) const
{
    auto* inlineBox = &inlineLevelBoxForLayoutBox(layoutBox);
    auto inlineBoxLogicalRect = inlineBox->logicalRect();
    auto inlineBoxAbsolutelogicalTop = inlineBox->logicalTop();

    while (inlineBox != m_rootInlineBox.get() && !inlineBox->hasLineBoxRelativeAlignment()) {
        inlineBox = &inlineLevelBoxForLayoutBox(inlineBox->layoutBox().parent());
        ASSERT(inlineBox->isInlineBox());
        inlineBoxAbsolutelogicalTop += inlineBox->logicalTop();
    }
    return { inlineBoxAbsolutelogicalTop, m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + inlineBoxLogicalRect.left(), inlineBoxLogicalRect.width(), inlineBoxLogicalRect.height() };
}

}
}

#endif
