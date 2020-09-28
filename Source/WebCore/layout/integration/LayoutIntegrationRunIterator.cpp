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
#include "LayoutIntegrationRunIterator.h"

#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderLineBreak.h"

namespace WebCore {
namespace LayoutIntegration {

TextRunIterator::TextRunIterator(Run::PathVariant&& pathVariant)
    : m_textRun(WTFMove(pathVariant))
{
}

TextRunIterator& TextRunIterator::traverseNextInVisualOrder()
{
    WTF::switchOn(m_textRun.m_pathVariant, [](auto& path) {
        path.traverseNextTextRunInVisualOrder();
    });
    return *this;
}

TextRunIterator& TextRunIterator::traverseNextInTextOrder()
{
    WTF::switchOn(m_textRun.m_pathVariant, [](auto& path) {
        path.traverseNextTextRunInTextOrder();
    });
    return *this;
}

bool TextRunIterator::operator==(const TextRunIterator& other) const
{
    if (m_textRun.m_pathVariant.index() != other.m_textRun.m_pathVariant.index())
        return false;

    return WTF::switchOn(m_textRun.m_pathVariant, [&](const auto& path) {
        return path == WTF::get<std::decay_t<decltype(path)>>(other.m_textRun.m_pathVariant);
    });
}

bool TextRunIterator::atEnd() const
{
    return WTF::switchOn(m_textRun.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

static const RenderBlockFlow* lineLayoutSystemFlowForRenderer(const RenderObject& renderer)
{
    // In currently supported cases the renderer is always direct child of the flow.
    if (!is<RenderBlockFlow>(*renderer.parent()))
        return nullptr;
    return downcast<RenderBlockFlow>(renderer.parent());
}

TextRunIterator firstTextRunFor(const RenderText& text)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* flow = lineLayoutSystemFlowForRenderer(text)) {
        if (auto* layoutFormattingContextLineLayout = flow->layoutFormattingContextLineLayout())
            return layoutFormattingContextLineLayout->textRunsFor(text);
    }
#endif

    return { LegacyPath { text.firstTextBox() } };
}

TextRunIterator firstTextRunInTextOrderFor(const RenderText& text)
{
    if (text.firstTextBox() && text.containsReversedText()) {
        Vector<const InlineTextBox*> sortedTextBoxes;
        for (auto* textBox = text.firstTextBox(); textBox; textBox = textBox->nextTextBox())
            sortedTextBoxes.append(textBox);
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), InlineTextBox::compareByStart);
        auto* first = sortedTextBoxes[0];
        return { LegacyPath { first, WTFMove(sortedTextBoxes), 0 } };
    }

    return firstTextRunFor(text);
}

TextRunRange textRunsFor(const RenderText& text)
{
    return { firstTextRunFor(text) };
}

ElementRunIterator::ElementRunIterator(Run::PathVariant&& pathVariant)
    : m_run(WTFMove(pathVariant))
{
}

bool ElementRunIterator::atEnd() const
{
    return WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

ElementRunIterator elementRunFor(const RenderLineBreak& renderElement)
{
    if (auto* flow = lineLayoutSystemFlowForRenderer(renderElement)) {
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        if (auto* layoutFormattingContextLineLayout = flow->layoutFormattingContextLineLayout())
            return layoutFormattingContextLineLayout->elementRunFor(renderElement);
#endif
    }

    return { LegacyPath(renderElement.inlineBoxWrapper()) };
}

}
}
