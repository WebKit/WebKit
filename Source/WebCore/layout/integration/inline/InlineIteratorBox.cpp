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

#include "config.h"
#include "InlineIteratorBox.h"

#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorTextBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderLineBreak.h"
#include "RenderObjectDocument.h"
#include "RenderView.h"

namespace WebCore {
namespace InlineIterator {

static_assert(std::ranges::bidirectional_range<LeafBoxRange> && std::ranges::common_range<LeafBoxRange> && std::ranges::view<LeafBoxRange> && std::ranges::borrowed_range<LeafBoxRange>);
static_assert(std::ranges::bidirectional_range<BoxRange<BoxIterator>> && std::ranges::common_range<BoxRange<BoxIterator>> && std::ranges::view<BoxRange<BoxIterator>> && std::ranges::borrowed_range<BoxRange<BoxIterator>>);

bool Box::isSVGText() const
{
    return isText() && renderer().isRenderSVGInlineText();
}

LeafBoxIterator Box::nextLineRightwardOnLine() const
{
    return std::ranges::next(LeafBoxIterator { *this });
}

LeafBoxIterator Box::nextLineLeftwardOnLine() const
{
    return std::ranges::prev(LeafBoxIterator { *this });
}

LeafBoxIterator Box::nextLineRightwardOnLineIgnoringLineBreak() const
{
    auto box = LeafBoxIterator { *this };
    box.traverseLineRightwardOnLineIgnoringLineBreak();
    return box;
}

LeafBoxIterator Box::nextLineLeftwardOnLineIgnoringLineBreak() const
{
    auto box = LeafBoxIterator { *this };
    box.traverseLineLeftwardOnLineIgnoringLineBreak();
    return box;
}

InlineBoxIterator Box::parentInlineBox() const
{
    ASSERT(!atEnd());

    if (isRootInlineBox())
        return { };

    auto candidate = *this;
    CheckedRef parentLayoutBox = displayBox().layoutBox().parent();
    do {
        candidate.traversePreviousBox();
    } while (!candidate.atEnd() && &candidate.displayBox().layoutBox() != parentLayoutBox.ptr());

    if (candidate.atEnd())
        return { };

    ASSERT(candidate.displayBox().isInlineBox());
    return { candidate };
}

LineBox Box::lineBox() const
{
    return lineBoxFor(*m_inlineContent, lineIndex());
}

FloatRect Box::visualRect() const
{
    auto rect = visualRectIgnoringBlockDirection();
    formattingContextRoot().flipForWritingMode(rect);
    return rect;
}

RenderObject::HighlightState Box::selectionState() const
{
    if (!hasRenderer())
        return { };

    if (auto* text = dynamicDowncast<TextBox>(*this)) {
        CheckedRef renderer = text->renderer();
        return renderer->view().selection().highlightStateForTextBox(renderer, text->selectableRange());
    }
    return renderer().selectionState();
}

LeafBoxIterator boxFor(const RenderLineBreak& renderer)
{
    if (CheckedPtr lineLayout = LayoutIntegration::LineLayout::containing(renderer))
        return lineLayout->boxFor(renderer);
    return { };
}

LeafBoxIterator boxFor(const RenderBox& renderer)
{
    if (CheckedPtr lineLayout = LayoutIntegration::LineLayout::containing(renderer))
        return lineLayout->boxFor(renderer);
    return { };
}

Box boxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    return { content, boxIndex };
}

}
}
