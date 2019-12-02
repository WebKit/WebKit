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
#include "LineLayoutTraversal.h"

#include "RenderBlockFlowLineLayout.h"
#include "RenderLineBreak.h"

namespace WebCore {
namespace LineLayoutTraversal {

TextBoxIterator::TextBoxIterator(Box::PathVariant&& pathVariant)
    : m_textBox(WTFMove(pathVariant))
{
}

TextBoxIterator& TextBoxIterator::traverseNextInVisualOrder()
{
    WTF::switchOn(m_textBox.m_pathVariant, [](auto& path) {
        path.traverseNextTextBoxInVisualOrder();
    });
    return *this;
}

TextBoxIterator& TextBoxIterator::traverseNextInTextOrder()
{
    WTF::switchOn(m_textBox.m_pathVariant, [](auto& path) {
        path.traverseNextTextBoxInTextOrder();
    });
    return *this;
}

bool TextBoxIterator::operator==(const TextBoxIterator& other) const
{
    if (m_textBox.m_pathVariant.index() != other.m_textBox.m_pathVariant.index())
        return false;

    return WTF::switchOn(m_textBox.m_pathVariant, [&](const auto& path) {
        return path == WTF::get<std::decay_t<decltype(path)>>(other.m_textBox.m_pathVariant);
    });
}

bool TextBoxIterator::atEnd() const
{
    return WTF::switchOn(m_textBox.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

static const RenderBlockFlow& flowForText(const RenderText& text)
{
    return downcast<RenderBlockFlow>(*text.containingBlockForObjectInFlow());
}

TextBoxIterator firstTextBoxFor(const RenderText& text)
{
    auto& flow = flowForText(text);

    if (auto* simpleLineLayout = flow.simpleLineLayout()) {
        auto range = simpleLineLayout->runResolver().rangeForRenderer(text);
        return { SimplePath { range.begin(), range.end() } };
    }

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lfcLineLayout = flow.lfcLineLayout())
        return lfcLineLayout->textBoxesFor(text);
#endif

    return { ComplexPath { text.firstTextBox() } };
}

TextBoxIterator firstTextBoxInTextOrderFor(const RenderText& text)
{
    auto& flow = flowForText(text);

    if (flow.complexLineLayout() && text.containsReversedText() && text.firstTextBox()) {
        Vector<const InlineTextBox*> sortedTextBoxes;
        for (auto* textBox = text.firstTextBox(); textBox; textBox = textBox->nextTextBox())
            sortedTextBoxes.append(textBox);
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), InlineTextBox::compareByStart);
        auto* first = sortedTextBoxes[0];
        return { ComplexPath { first, WTFMove(sortedTextBoxes), 0 } };
    }

    return firstTextBoxFor(text);
}

TextBoxRange textBoxesFor(const RenderText& text)
{
    return { firstTextBoxFor(text) };
}

ElementBoxIterator::ElementBoxIterator(Box::PathVariant&& pathVariant)
    : m_box(WTFMove(pathVariant))
{
}

bool ElementBoxIterator::atEnd() const
{
    return WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

ElementBoxIterator elementBoxFor(const RenderLineBreak& renderElement)
{
    auto* containingBlock = renderElement.containingBlock();
    if (!is<RenderBlockFlow>(containingBlock))
        return { };

    auto& flow = downcast<RenderBlockFlow>(*containingBlock);

    if (auto* simpleLineLayout = flow.simpleLineLayout()) {
        auto range = simpleLineLayout->runResolver().rangeForRenderer(renderElement);
        return { SimplePath(range.begin(), range.end()) };
    }

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lfcLineLayout = flow.lfcLineLayout())
        return lfcLineLayout->elementBoxFor(renderElement);
#endif

    return { ComplexPath(renderElement.inlineBoxWrapper()) };
}

}
}
