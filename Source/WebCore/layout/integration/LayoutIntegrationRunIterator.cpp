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


#include "LayoutIntegrationLineIterator.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderLineBreak.h"
#include "RenderView.h"

namespace WebCore {
namespace LayoutIntegration {

RunIterator::RunIterator(PathRun::PathVariant&& pathVariant)
    : m_run(WTFMove(pathVariant))
{
}

RunIterator::RunIterator(const PathRun& run)
    : m_run(run)
{
}

bool RunIterator::operator==(const RunIterator& other) const
{
    return m_run.m_pathVariant == other.m_run.m_pathVariant;
}

bool RunIterator::atEnd() const
{
    return WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

RunIterator PathRun::nextOnLine() const
{
    return RunIterator(*this).traverseNextOnLine();
}

RunIterator PathRun::previousOnLine() const
{
    return RunIterator(*this).traversePreviousOnLine();
}

RunIterator PathRun::nextOnLineIgnoringLineBreak() const
{
    return RunIterator(*this).traverseNextOnLineIgnoringLineBreak();
}

RunIterator PathRun::previousOnLineIgnoringLineBreak() const
{
    return RunIterator(*this).traversePreviousOnLineIgnoringLineBreak();
}

LineIterator PathRun::line() const
{
    return WTF::switchOn(m_pathVariant, [](const RunIteratorLegacyPath& path) {
        return LineIterator(LineIteratorLegacyPath(&path.rootInlineBox()));
    }
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    , [](const RunIteratorModernPath& path) {
        return LineIterator(LineIteratorModernPath(*path.m_inlineContent, path.run().lineIndex()));
    }
#endif
    );
}

const RenderStyle& PathRun::style() const
{
    return line()->isFirst() ? renderer().firstLineStyle() : renderer().style();
}

TextRunIterator PathTextRun::nextTextRun() const
{
    return TextRunIterator(*this).traverseNextTextRun();
}

TextRunIterator PathTextRun::nextTextRunInTextOrder() const
{
    return TextRunIterator(*this).traverseNextTextRunInTextOrder();
}

RenderObject::HighlightState PathRun::selectionState() const
{
    if (isText()) {
        auto& text = downcast<PathTextRun>(*this);
        auto& renderer = text.renderer();
        return renderer.view().selection().highlightStateForTextBox(renderer, text.selectableRange());
    }
    return renderer().selectionState();
}

TextRunIterator::TextRunIterator(PathRun::PathVariant&& pathVariant)
    : RunIterator(WTFMove(pathVariant))
{
}

TextRunIterator::TextRunIterator(const PathRun& run)
    : RunIterator(run)
{
}

TextRunIterator& TextRunIterator::traverseNextTextRun()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextTextRun();
    });
    return *this;
}

TextRunIterator& TextRunIterator::traverseNextTextRunInTextOrder()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextTextRunInTextOrder();
    });
    return *this;
}

RunIterator& RunIterator::traverseNextOnLine()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextOnLine();
    });
    return *this;
}

RunIterator& RunIterator::traversePreviousOnLine()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traversePreviousOnLine();
    });
    return *this;
}

RunIterator& RunIterator::traverseNextOnLineIgnoringLineBreak()
{
    do {
        traverseNextOnLine();
    } while (!atEnd() && m_run.isLineBreak());
    return *this;
}

RunIterator& RunIterator::traversePreviousOnLineIgnoringLineBreak()
{
    do {
        traversePreviousOnLine();
    } while (!atEnd() && m_run.isLineBreak());
    return *this;
}

RunIterator& RunIterator::traverseNextOnLineInLogicalOrder()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traverseNextOnLineInLogicalOrder();
    });
    return *this;
}

RunIterator& RunIterator::traversePreviousOnLineInLogicalOrder()
{
    WTF::switchOn(m_run.m_pathVariant, [](auto& path) {
        path.traversePreviousOnLineInLogicalOrder();
    });
    return *this;
}

TextRunIterator firstTextRunFor(const RenderText& text)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LineLayout::containing(text))
        return lineLayout->textRunsFor(text);
#endif

    return { RunIteratorLegacyPath { text.firstTextBox() } };
}

TextRunIterator firstTextRunInTextOrderFor(const RenderText& text)
{
    if (text.firstTextBox() && text.containsReversedText()) {
        Vector<const LegacyInlineBox*> sortedTextBoxes;
        for (auto* textBox = text.firstTextBox(); textBox; textBox = textBox->nextTextBox())
            sortedTextBoxes.append(textBox);
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), [](auto* a, auto* b) {
            return LegacyInlineTextBox::compareByStart(downcast<LegacyInlineTextBox>(a), downcast<LegacyInlineTextBox>(b));
        });
        auto* first = sortedTextBoxes[0];
        return { RunIteratorLegacyPath { first, WTFMove(sortedTextBoxes), 0 } };
    }

    return firstTextRunFor(text);
}

TextRunIterator textRunFor(const LegacyInlineTextBox* legacyInlineTextBox)
{
    return { RunIteratorLegacyPath { legacyInlineTextBox } };
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
TextRunIterator textRunFor(const InlineContent& content, const Layout::Run& run)
{
    return textRunFor(content, content.indexForRun(run));
}

TextRunIterator textRunFor(const InlineContent& content, size_t runIndex)
{
    ASSERT(content.runs[runIndex].text());
    return { RunIteratorModernPath { content, runIndex } };
}
#endif

TextRunRange textRunsFor(const RenderText& text)
{
    return { firstTextRunFor(text) };
}

RunIterator runFor(const RenderLineBreak& renderer)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LineLayout::containing(renderer))
        return lineLayout->runFor(renderer);
#endif
    return { RunIteratorLegacyPath(renderer.inlineBoxWrapper()) };
}

RunIterator runFor(const RenderBox& renderer)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LineLayout::containing(renderer))
        return lineLayout->runFor(renderer);
#endif
    return { RunIteratorLegacyPath(renderer.inlineBoxWrapper()) };
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
RunIterator runFor(const InlineContent& content, size_t runIndex)
{
    return { RunIteratorModernPath { content, runIndex } };
}
#endif

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
const RunIteratorModernPath& PathRun::modernPath() const
{
    return WTF::get<RunIteratorModernPath>(m_pathVariant);
}
#endif

const RunIteratorLegacyPath& PathRun::legacyPath() const
{
    return WTF::get<RunIteratorLegacyPath>(m_pathVariant);
}

}
}
