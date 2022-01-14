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

LeafBoxIterator Line::closestRunForPoint(const IntPoint& pointInContents, bool editableOnly) const
{
    return closestRunForLogicalLeftPosition(isHorizontal() ? pointInContents.x() : pointInContents.y(), editableOnly);
}

LeafBoxIterator Line::closestRunForLogicalLeftPosition(int leftPosition, bool editableOnly) const
{
    auto isEditable = [&](auto run) {
        return run && run->renderer().node() && run->renderer().node()->hasEditableStyle();
    };

    auto firstRun = this->firstLeafBox();
    auto lastRun = this->lastLeafBox();

    if (firstRun != lastRun) {
        if (firstRun->isLineBreak())
            firstRun = firstRun->nextOnLineIgnoringLineBreak();
        else if (lastRun->isLineBreak())
            lastRun = lastRun->previousOnLineIgnoringLineBreak();
    }

    if (firstRun == lastRun && (!editableOnly || isEditable(firstRun)))
        return firstRun;

    if (firstRun && leftPosition <= firstRun->logicalLeft() && !firstRun->renderer().isListMarker() && (!editableOnly || isEditable(firstRun)))
        return firstRun;

    if (lastRun && leftPosition >= lastRun->logicalRight() && !lastRun->renderer().isListMarker() && (!editableOnly || isEditable(lastRun)))
        return lastRun;

    auto closestRun = lastRun;
    for (auto run = firstRun; run; run = run.traverseNextOnLineIgnoringLineBreak()) {
        if (!run->renderer().isListMarker() && (!editableOnly || isEditable(run))) {
            if (leftPosition < run->logicalRight())
                return run;
            closestRun = run;
        }
    }

    return closestRun;
}

int Line::blockDirectionPointInLine() const
{
    return !containingBlock().style().isFlippedBlocksWritingMode() ? std::max(top(), selectionTopForHitTesting()) : std::min(bottom(), selectionBottom());
}

LayoutUnit Line::selectionTopAdjustedForPrecedingBlock() const
{
    return containingBlock().adjustSelectionTopForPrecedingBlock(selectionTop());
}

LayoutUnit Line::selectionHeightAdjustedForPrecedingBlock() const
{
    return std::max<LayoutUnit>(0, selectionBottom() - selectionTopAdjustedForPrecedingBlock());
}

RenderObject::HighlightState Line::selectionState() const
{
    auto& block = containingBlock();
    if (block.selectionState() == RenderObject::HighlightState::None)
        return RenderObject::HighlightState::None;

    auto lineState = RenderObject::HighlightState::None;
    for (auto box = firstLeafBox(); box; box.traverseNextOnLine()) {
        auto boxState = box->selectionState();
        if (lineState == RenderObject::HighlightState::None)
            lineState = boxState;
        else if (lineState == RenderObject::HighlightState::Start) {
            if (boxState == RenderObject::HighlightState::End || boxState == RenderObject::HighlightState::None)
                lineState = RenderObject::HighlightState::Both;
        } else if (lineState == RenderObject::HighlightState::Inside) {
            if (boxState == RenderObject::HighlightState::Start || boxState == RenderObject::HighlightState::End)
                lineState = boxState;
            else if (boxState == RenderObject::HighlightState::None)
                lineState = RenderObject::HighlightState::End;
        } else if (lineState == RenderObject::HighlightState::End) {
            if (boxState == RenderObject::HighlightState::Start)
                lineState = RenderObject::HighlightState::Both;
        }

        if (lineState == RenderObject::HighlightState::Both)
            break;
    }
    return lineState;
}

LeafBoxIterator Line::firstSelectedBox() const
{
    for (auto box = firstLeafBox(); box; box.traverseNextOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return { };
}

LeafBoxIterator Line::lastSelectedBox() const
{
    for (auto box = lastLeafBox(); box; box.traversePreviousOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return { };
}

}
}

