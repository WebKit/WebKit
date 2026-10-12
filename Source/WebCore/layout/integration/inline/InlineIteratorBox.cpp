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

BoxIterator::BoxIterator(const Box& run)
    : m_box(run)
{
}

bool BoxIterator::operator==(const BoxIterator& other) const
{
    if (atEnd() && other.atEnd())
        return true;

    return m_box.m_inlineContent == other.m_box.m_inlineContent && m_box.m_boxIndex == other.m_box.m_boxIndex;
}

bool BoxIterator::atEnd() const
{
    return m_box.atEnd();
}

BoxIterator& BoxIterator::traverseLineRightwardOnLine()
{
    m_box.traverseNextBoxOnLine();
    return *this;
}

BoxIterator& BoxIterator::traverseLineRightwardOnLineSkippingChildren()
{
    m_box.traverseNextBoxOnLineSkippingChildren();
    return *this;
}

BoxIterator& BoxIterator::traverseLineLeftwardOnLine()
{
    m_box.traversePreviousBoxOnLine();
    return *this;
}

bool Box::isSVGText() const
{
    return isText() && renderer().isRenderSVGInlineText();
}

LeafBoxIterator Box::nextLineRightwardOnLine() const
{
    return LeafBoxIterator(*this).traverseLineRightwardOnLine();
}

LeafBoxIterator Box::nextLineLeftwardOnLine() const
{
    return LeafBoxIterator(*this).traverseLineLeftwardOnLine();
}

LeafBoxIterator Box::nextLineRightwardOnLineIgnoringLineBreak() const
{
    return LeafBoxIterator(*this).traverseLineRightwardOnLineIgnoringLineBreak();
}

LeafBoxIterator Box::nextLineLeftwardOnLineIgnoringLineBreak() const
{
    return LeafBoxIterator(*this).traverseLineLeftwardOnLineIgnoringLineBreak();
}

InlineBoxIterator Box::parentInlineBox() const
{
    ASSERT(!atEnd());

    auto candidate = *this;

    if (isRootInlineBox()) {
        candidate.setAtEnd();
        return { candidate };
    }

    CheckedRef parentLayoutBox = displayBox().layoutBox().parent();
    do {
        candidate.traversePreviousBox();
    } while (!candidate.atEnd() && &candidate.displayBox().layoutBox() != parentLayoutBox.ptr());

    ASSERT(candidate.atEnd() || candidate.displayBox().isInlineBox());

    return { candidate };
}

LineBoxIterator Box::lineBox() const
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

LeafBoxIterator::LeafBoxIterator(const Box& run)
    : BoxIterator(run)
{
}

LeafBoxIterator& LeafBoxIterator::traverseLineRightwardOnLine()
{
    m_box.traverseNextLeafOnLine();
    return *this;
}

LeafBoxIterator& LeafBoxIterator::traverseLineLeftwardOnLine()
{
    m_box.traversePreviousLeafOnLine();
    return *this;
}

LeafBoxIterator& LeafBoxIterator::traverseLineRightwardOnLineIgnoringLineBreak()
{
    do {
        traverseLineRightwardOnLine();
    } while (!atEnd() && m_box.isLineBreak());
    return *this;
}

LeafBoxIterator& LeafBoxIterator::traverseLineLeftwardOnLineIgnoringLineBreak()
{
    do {
        traverseLineLeftwardOnLine();
    } while (!atEnd() && m_box.isLineBreak());
    return *this;
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

LeafBoxIterator boxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    return { Box { content, boxIndex } };
}

}
}
