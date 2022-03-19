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
#include "InlineIteratorLine.h"

#include "InlineIteratorBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"

namespace WebCore {
namespace InlineIterator {

LineIterator::LineIterator(Line::PathVariant&& pathVariant)
    : m_line(WTFMove(pathVariant))
{
}

LineIterator::LineIterator(const Line& line)
    : m_line(line)
{
}

bool LineIterator::atEnd() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

LineIterator& LineIterator::traverseNext()
{
    WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.traverseNext();
    });
    return *this;
}

LineIterator& LineIterator::traversePrevious()
{
    WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.traversePrevious();
    });
    return *this;
}

bool LineIterator::operator==(const LineIterator& other) const
{
    return m_line.m_pathVariant == other.m_line.m_pathVariant;
}

LineIterator firstLineFor(const RenderBlockFlow& flow)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = flow.modernLineLayout())
        return lineLayout->firstLine();
#endif

    return { LineIteratorLegacyPath { flow.firstRootBox() } };
}

LineIterator lastLineFor(const RenderBlockFlow& flow)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = flow.modernLineLayout())
        return lineLayout->lastLine();
#endif

    return { LineIteratorLegacyPath { flow.lastRootBox() } };
}

LineIterator Line::next() const
{
    return LineIterator(*this).traverseNext();
}

LineIterator Line::previous() const
{
    return LineIterator(*this).traversePrevious();
}

LeafBoxIterator Line::firstLeafBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> LeafBoxIterator {
        return { path.firstLeafBox() };
    });
}

LeafBoxIterator Line::lastLeafBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> LeafBoxIterator {
        return { path.lastLeafBox() };
    });
}

LeafBoxIterator Line::closestBoxForPoint(const IntPoint& pointInContents, bool editableOnly) const
{
    return closestBoxForLogicalLeftPosition(isHorizontal() ? pointInContents.x() : pointInContents.y(), editableOnly);
}

LeafBoxIterator Line::closestBoxForLogicalLeftPosition(int leftPosition, bool editableOnly) const
{
    auto isEditable = [&](auto box) {
        return box && box->renderer().node() && box->renderer().node()->hasEditableStyle();
    };

    auto firstBox = this->firstLeafBox();
    auto lastBox = this->lastLeafBox();

    if (firstBox != lastBox) {
        if (firstBox->isLineBreak())
            firstBox = firstBox->nextOnLineIgnoringLineBreak();
        else if (lastBox->isLineBreak())
            lastBox = lastBox->previousOnLineIgnoringLineBreak();
    }

    if (firstBox == lastBox && (!editableOnly || isEditable(firstBox)))
        return firstBox;

    if (firstBox && leftPosition <= firstBox->logicalLeft() && !firstBox->renderer().isListMarker() && (!editableOnly || isEditable(firstBox)))
        return firstBox;

    if (lastBox && leftPosition >= lastBox->logicalRight() && !lastBox->renderer().isListMarker() && (!editableOnly || isEditable(lastBox)))
        return lastBox;

    auto closestBox = lastBox;
    for (auto box = firstBox; box; box = box.traverseNextOnLineIgnoringLineBreak()) {
        if (!box->renderer().isListMarker() && (!editableOnly || isEditable(box))) {
            if (leftPosition < box->logicalRight())
                return box;
            closestBox = box;
        }
    }

    return closestBox;
}

int Line::blockDirectionPointInLine() const
{
    return !containingBlock().style().isFlippedBlocksWritingMode() ? std::max(top(), enclosingTopForHitTesting()) : std::min(bottom(), enclosingBottom());
}

}
}

