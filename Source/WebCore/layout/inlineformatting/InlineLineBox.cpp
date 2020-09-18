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

namespace WebCore {
namespace Layout {

LineBox::InlineBox::InlineBox(const Box& layoutBox, const InlineRect& rect, InlineLayoutUnit baseline)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect(rect)
    , m_baseline(baseline)
    , m_isEmpty(false)
{
    ASSERT(rect.height());
}

LineBox::InlineBox::InlineBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect({ }, logicalLeft, logicalWidth, { })
{
}

LineBox::LineBox(InlineLayoutUnit contentLogicalWidth, IsLineVisuallyEmpty isLineVisuallyEmpty)
    : m_logicalSize(contentLogicalWidth, { })
    , m_isLineVisuallyEmpty(isLineVisuallyEmpty == IsLineVisuallyEmpty::Yes)
{
}

void LineBox::addRootInlineBox(std::unique_ptr<InlineBox>&& rootInlineBox)
{
    std::exchange(m_rootInlineBox, WTFMove(rootInlineBox));
    m_inlineBoxRectMap.set(&m_rootInlineBox->layoutBox(), m_rootInlineBox.get());
}

void LineBox::addInlineBox(std::unique_ptr<InlineBox>&& inlineBox)
{
    m_inlineBoxRectMap.set(&inlineBox->layoutBox(), inlineBox.get());
    m_nonRootInlineBoxList.append(WTFMove(inlineBox));
}

InlineRect LineBox::logicalRectForTextRun(const Line::Run& run) const
{
    ASSERT(run.isText() || run.isLineBreak());
    auto& parentInlineBox = inlineBoxForLayoutBox(run.layoutBox().parent());
    auto& fontMetrics = parentInlineBox.fontMetrics();
    auto runlogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - fontMetrics.ascent();
    InlineLayoutUnit logicalHeight = fontMetrics.height();
    return { runlogicalTop, m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + run.logicalLeft(), run.logicalWidth(), logicalHeight };
}

}
}

#endif
