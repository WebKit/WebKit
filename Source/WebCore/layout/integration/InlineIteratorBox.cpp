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


#include "InlineIteratorLine.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderCombineText.h"
#include "RenderLineBreak.h"
#include "RenderView.h"

namespace WebCore {
namespace InlineIterator {

BoxIterator::BoxIterator(Box::PathVariant&& pathVariant)
    : m_run(WTFMove(pathVariant))
{
}

BoxIterator::BoxIterator(const Box& run)
    : m_run(run)
{
}

bool BoxIterator::operator==(const BoxIterator& other) const
{
    return m_run.m_pathVariant == other.m_run.m_pathVariant;
}

bool BoxIterator::atEnd() const
{
    return WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

BoxIterator Box::nextOnLine() const
{
    return BoxIterator(*this).traverseNextOnLine();
}

BoxIterator Box::previousOnLine() const
{
    return BoxIterator(*this).traversePreviousOnLine();
}

BoxIterator Box::nextOnLineIgnoringLineBreak() const
{
    return BoxIterator(*this).traverseNextOnLineIgnoringLineBreak();
}

BoxIterator Box::previousOnLineIgnoringLineBreak() const
{
    return BoxIterator(*this).traversePreviousOnLineIgnoringLineBreak();
}

LineIterator Box::line() const
{
    return WTF::switchOn(m_pathVariant, [](const BoxIteratorLegacyPath& path) {
        return LineIterator(LineIteratorLegacyPath(&path.rootInlineBox()));
    }
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    , [](const BoxIteratorModernPath& path) {
        return LineIterator(LineIteratorModernPath(path.inlineContent(), path.box().lineIndex()));
    }
#endif
    );
}

const RenderStyle& Box::style() const
{
    return line()->isFirst() ? renderer().firstLineStyle() : renderer().style();
}

TextBoxIterator TextBox::nextTextRun() const
{
    return TextBoxIterator(*this).traverseNextTextRun();
}

TextBoxIterator TextBox::nextTextRunInTextOrder() const
{
    return TextBoxIterator(*this).traverseNextTextRunInTextOrder();
}

LayoutRect TextBox::selectionRect(unsigned rangeStart, unsigned rangeEnd) const
{
    auto [clampedStart, clampedEnd] = selectableRange().clamp(rangeStart, rangeEnd);

    if (clampedStart >= clampedEnd && !(rangeStart == rangeEnd && rangeStart >= start() && rangeStart <= end()))
        return { };

    auto selectionTop = line()->selectionTop();
    auto selectionHeight = line()->selectionHeight();

    LayoutRect selectionRect { logicalLeft(), selectionTop, logicalWidth(), selectionHeight };

    TextRun textRun = createTextRun();
    if (clampedStart || clampedEnd != textRun.length())
        fontCascade().adjustSelectionRectForText(textRun, selectionRect, clampedStart, clampedEnd);

    return snappedSelectionRect(selectionRect, logicalRight(), selectionTop, selectionHeight, isHorizontal());
}

bool TextBox::isCombinedText() const
{
    auto& renderer = this->renderer();
    return is<RenderCombineText>(renderer) && downcast<RenderCombineText>(renderer).isCombined();
}

const FontCascade& TextBox::fontCascade() const
{
    if (isCombinedText())
        return downcast<RenderCombineText>(renderer()).textCombineFont();

    return style().fontCascade();
}

RenderObject::HighlightState Box::selectionState() const
{
    if (isText()) {
        auto& text = downcast<TextBox>(*this);
        auto& renderer = text.renderer();
        return renderer.view().selection().highlightStateForTextBox(renderer, text.selectableRange());
    }
    return renderer().selectionState();
}

TextBoxIterator::TextBoxIterator(Box::PathVariant&& pathVariant)
    : BoxIterator(WTFMove(pathVariant))
{
}

TextBoxIterator::TextBoxIterator(const Box& run)
    : BoxIterator(run)
{
}

TextBoxIterator& TextBoxIterator::traverseNextTextRun()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextTextRun();
    });
    return *this;
}

TextBoxIterator& TextBoxIterator::traverseNextTextRunInTextOrder()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextTextRunInTextOrder();
    });
    return *this;
}

BoxIterator& BoxIterator::traverseNextOnLine()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextOnLine();
    });
    return *this;
}

BoxIterator& BoxIterator::traversePreviousOnLine()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traversePreviousOnLine();
    });
    return *this;
}

BoxIterator& BoxIterator::traverseNextOnLineIgnoringLineBreak()
{
    do {
        traverseNextOnLine();
    } while (!atEnd() && m_run.isLineBreak());
    return *this;
}

BoxIterator& BoxIterator::traversePreviousOnLineIgnoringLineBreak()
{
    do {
        traversePreviousOnLine();
    } while (!atEnd() && m_run.isLineBreak());
    return *this;
}

BoxIterator& BoxIterator::traverseNextOnLineInLogicalOrder()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextOnLineInLogicalOrder();
    });
    return *this;
}

BoxIterator& BoxIterator::traversePreviousOnLineInLogicalOrder()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traversePreviousOnLineInLogicalOrder();
    });
    return *this;
}

TextBoxIterator firstTextRunFor(const RenderText& text)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(text))
        return lineLayout->textRunsFor(text);
#endif

    return { BoxIteratorLegacyPath { text.firstTextBox() } };
}

TextBoxIterator firstTextRunInTextOrderFor(const RenderText& text)
{
    if (text.firstTextBox() && text.containsReversedText()) {
        Vector<const LegacyInlineBox*> sortedTextBoxes;
        for (auto* textBox = text.firstTextBox(); textBox; textBox = textBox->nextTextBox())
            sortedTextBoxes.append(textBox);
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), [](auto* a, auto* b) {
            return LegacyInlineTextBox::compareByStart(downcast<LegacyInlineTextBox>(a), downcast<LegacyInlineTextBox>(b));
        });
        auto* first = sortedTextBoxes[0];
        return { BoxIteratorLegacyPath { first, WTFMove(sortedTextBoxes), 0 } };
    }

    return firstTextRunFor(text);
}

TextBoxIterator textRunFor(const LegacyInlineTextBox* legacyInlineTextBox)
{
    return { BoxIteratorLegacyPath { legacyInlineTextBox } };
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
TextBoxIterator textRunFor(const LayoutIntegration::InlineContent& content, const InlineDisplay::Box& box)
{
    return textRunFor(content, content.indexForBox(box));
}

TextBoxIterator textRunFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    ASSERT(content.boxes[boxIndex].text());
    return { BoxIteratorModernPath { content, boxIndex } };
}
#endif

TextRunRange textRunsFor(const RenderText& text)
{
    return { firstTextRunFor(text) };
}

BoxIterator runFor(const RenderLineBreak& renderer)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(renderer))
        return lineLayout->runFor(renderer);
#endif
    return { BoxIteratorLegacyPath(renderer.inlineBoxWrapper()) };
}

BoxIterator runFor(const RenderBox& renderer)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(renderer))
        return lineLayout->runFor(renderer);
#endif
    return { BoxIteratorLegacyPath(renderer.inlineBoxWrapper()) };
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
BoxIterator runFor(const LayoutIntegration::InlineContent& content, size_t runIndex)
{
    return { BoxIteratorModernPath { content, runIndex } };
}
#endif

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
const BoxIteratorModernPath& Box::modernPath() const
{
    return WTF::get<BoxIteratorModernPath>(m_pathVariant);
}
#endif

const BoxIteratorLegacyPath& Box::legacyPath() const
{
    return WTF::get<BoxIteratorLegacyPath>(m_pathVariant);
}

}
}
