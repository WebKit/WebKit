/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineIteratorInlineBox.h"

#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace InlineIterator {

InlineBox::InlineBox(PathVariant&& path)
    : Box(WTFMove(path))
{
}

std::pair<bool, bool> InlineBox::hasClosedLeftAndRightEdge() const
{
    // FIXME: Layout knows the answer to this question so we should consult it.
    if (style().boxDecorationBreak() == BoxDecorationBreak::Clone)
        return { true, true };
    bool isLTR = style().isLeftToRightDirection();
    bool isFirst = !previousInlineBox() && !renderer().isContinuation();
    bool isLast = !nextInlineBox() && !renderer().continuation();
    return { isLTR ? isFirst : isLast, isLTR ? isLast : isFirst };
};

InlineBoxIterator InlineBox::nextInlineBox() const
{
    return InlineBoxIterator(*this).traverseNextInlineBox();
}

InlineBoxIterator InlineBox::previousInlineBox() const
{
    return InlineBoxIterator(*this).traversePreviousInlineBox();
}

LeafBoxIterator InlineBox::firstLeafBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> LeafBoxIterator {
        return { path.firstLeafBoxForInlineBox() };
    });
}

LeafBoxIterator InlineBox::lastLeafBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> LeafBoxIterator {
        return { path.lastLeafBoxForInlineBox() };
    });
}

LeafBoxIterator InlineBox::endLeafBox() const
{
    if (auto last = lastLeafBox())
        return last->nextOnLine();
    return { };
}

InlineBoxIterator::InlineBoxIterator(Box::PathVariant&& pathVariant)
    : BoxIterator(WTFMove(pathVariant))
{
}

InlineBoxIterator::InlineBoxIterator(const Box& box)
    : BoxIterator(box)
{
}

InlineBoxIterator& InlineBoxIterator::traverseNextInlineBox()
{
    WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        path.traverseNextInlineBox();
    });
    return *this;
}

InlineBoxIterator& InlineBoxIterator::traversePreviousInlineBox()
{
    WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        path.traversePreviousInlineBox();
    });
    return *this;
}

InlineBoxIterator firstInlineBoxFor(const RenderInline& renderInline)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(renderInline))
        return lineLayout->firstInlineBoxFor(renderInline);
    return { BoxLegacyPath { renderInline.firstLineBox() } };
}

InlineBoxIterator firstRootInlineBoxFor(const RenderBlockFlow& block)
{
    if (auto* lineLayout = block.modernLineLayout())
        return lineLayout->firstRootInlineBox();
    return { BoxLegacyPath { block.firstRootBox() } };
}

InlineBoxIterator inlineBoxFor(const LegacyInlineFlowBox& legacyInlineFlowBox)
{
    return { BoxLegacyPath { &legacyInlineFlowBox } };
}

InlineBoxIterator inlineBoxFor(const LayoutIntegration::InlineContent& content, const InlineDisplay::Box& box)
{
    return inlineBoxFor(content, content.indexForBox(box));
}

InlineBoxIterator inlineBoxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    ASSERT(content.displayContent().boxes[boxIndex].isInlineBox());
    return { BoxModernPath { content, boxIndex } };
}

}
}
