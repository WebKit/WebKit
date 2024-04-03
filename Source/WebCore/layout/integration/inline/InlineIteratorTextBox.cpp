/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "InlineIteratorTextBox.h"

#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorTextBoxInlines.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderCombineText.h"
#include "RenderStyleInlines.h"
#include "SVGInlineTextBox.h"

namespace WebCore {
namespace InlineIterator {

TextBoxIterator TextBox::nextTextBox() const
{
    return TextBoxIterator(*this).traverseNextTextBox();
}

const FontCascade& TextBox::fontCascade() const
{
    if (auto* renderer = dynamicDowncast<RenderCombineText>(this->renderer()); renderer && renderer->isCombined())
        return renderer->textCombineFont();

    return style().fontCascade();
}

TextBoxIterator::TextBoxIterator(Box::PathVariant&& pathVariant)
    : LeafBoxIterator(WTFMove(pathVariant))
{
}

TextBoxIterator::TextBoxIterator(const Box& box)
    : LeafBoxIterator(box)
{
}

TextBoxIterator& TextBoxIterator::traverseNextTextBox()
{
    WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        path.traverseNextTextBox();
    });
    return *this;
}

TextBoxIterator firstTextBoxFor(const RenderText& text)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(text))
        return lineLayout->textBoxesFor(text);

    return { BoxLegacyPath { text.firstTextBox() } };
}

TextBoxIterator textBoxFor(const LegacyInlineTextBox* legacyInlineTextBox)
{
    return { BoxLegacyPath { legacyInlineTextBox } };
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, const InlineDisplay::Box& box)
{
    return textBoxFor(content, content.indexForBox(box));
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    ASSERT(content.displayContent().boxes[boxIndex].isTextOrSoftLineBreak());
    return { BoxModernPath { content, boxIndex } };
}

TextBoxRange textBoxesFor(const RenderText& text)
{
    return { firstTextBoxFor(text) };
}

}
}
