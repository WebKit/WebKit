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
#include "RenderView.h"

namespace WebCore {
namespace InlineIterator {

BoxIterator::BoxIterator(Box::PathVariant&& pathVariant)
    : m_box(WTFMove(pathVariant))
{
}

BoxIterator::BoxIterator(const Box& run)
    : m_box(run)
{
}

bool BoxIterator::operator==(const BoxIterator& other) const
{
    if (atEnd() && other.atEnd())
        return true;

    return m_box.m_pathVariant == other.m_box.m_pathVariant;
}

bool BoxIterator::atEnd() const
{
    return WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

LeafBoxIterator Box::nextOnLine() const
{
    return LeafBoxIterator(*this).traverseNextOnLine();
}

LeafBoxIterator Box::previousOnLine() const
{
    return LeafBoxIterator(*this).traversePreviousOnLine();
}

LeafBoxIterator Box::nextOnLineIgnoringLineBreak() const
{
    return LeafBoxIterator(*this).traverseNextOnLineIgnoringLineBreak();
}

LeafBoxIterator Box::previousOnLineIgnoringLineBreak() const
{
    return LeafBoxIterator(*this).traversePreviousOnLineIgnoringLineBreak();
}

InlineBoxIterator Box::parentInlineBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> InlineBoxIterator {
        return { path.parentInlineBox() };
    });
}

LineBoxIterator Box::lineBox() const
{
    return WTF::switchOn(m_pathVariant, [](const BoxLegacyPath& path) {
        return LineBoxIterator(LineBoxIteratorLegacyPath(&path.rootInlineBox()));
    }
    , [](const BoxModernPath& path) {
        return LineBoxIterator(LineBoxIteratorModernPath(path.inlineContent(), path.box().lineIndex()));
    }
    );
}

FloatRect Box::visualRect() const
{
    auto rect = visualRectIgnoringBlockDirection();
    formattingContextRoot().flipForWritingMode(rect);
    return rect;
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

LeafBoxIterator::LeafBoxIterator(Box::PathVariant&& pathVariant)
    : BoxIterator(WTFMove(pathVariant))
{
}

LeafBoxIterator::LeafBoxIterator(const Box& run)
    : BoxIterator(run)
{
}

LeafBoxIterator& LeafBoxIterator::traverseNextOnLine()
{
    WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        path.traverseNextOnLine();
    });
    return *this;
}

LeafBoxIterator& LeafBoxIterator::traversePreviousOnLine()
{
    WTF::switchOn(m_box.m_pathVariant, [](auto& path) {
        path.traversePreviousOnLine();
    });
    return *this;
}

LeafBoxIterator& LeafBoxIterator::traverseNextOnLineIgnoringLineBreak()
{
    do {
        traverseNextOnLine();
    } while (!atEnd() && m_box.isLineBreak());
    return *this;
}

LeafBoxIterator& LeafBoxIterator::traversePreviousOnLineIgnoringLineBreak()
{
    do {
        traversePreviousOnLine();
    } while (!atEnd() && m_box.isLineBreak());
    return *this;
}

LeafBoxIterator boxFor(const RenderLineBreak& renderer)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(renderer))
        return lineLayout->boxFor(renderer);
    return { BoxLegacyPath(renderer.inlineBoxWrapper()) };
}

LeafBoxIterator boxFor(const RenderBox& renderer)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(renderer))
        return lineLayout->boxFor(renderer);
    return { BoxLegacyPath(renderer.inlineBoxWrapper()) };
}

LeafBoxIterator boxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    return { BoxModernPath { content, boxIndex } };
}

const BoxModernPath& Box::modernPath() const
{
    return std::get<BoxModernPath>(m_pathVariant);
}

const BoxLegacyPath& Box::legacyPath() const
{
    return std::get<BoxLegacyPath>(m_pathVariant);
}

}
}
