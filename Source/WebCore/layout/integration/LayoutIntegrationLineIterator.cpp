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
#include "LayoutIntegrationLineIterator.h"

#include "LayoutIntegrationLineLayout.h"
#include "LayoutIntegrationRunIterator.h"
#include "RenderBlockFlow.h"

namespace WebCore {
namespace LayoutIntegration {

LineIterator::LineIterator(PathLine::PathVariant&& pathVariant)
    : m_line(WTFMove(pathVariant))
{
}

LineIterator::LineIterator(const PathLine& line)
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

LineIterator PathLine::next() const
{
    return LineIterator(*this).traverseNext();
}

LineIterator PathLine::previous() const
{
    return LineIterator(*this).traversePrevious();
}

RunIterator PathLine::firstRun() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> RunIterator {
        return { path.firstRun() };
    });
}

RunIterator PathLine::lastRun() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> RunIterator {
        return { path.lastRun() };
    });
}

RunIterator PathLine::logicalStartRun() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> RunIterator {
        return { path.logicalStartRun() };
    });
}

RunIterator PathLine::logicalEndRun() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> RunIterator {
        return { path.logicalEndRun() };
    });
}

RunIterator PathLine::logicalStartRunWithNode() const
{
    for (auto run = logicalStartRun(); run; run.traverseNextOnLineInLogicalOrder()) {
        if (run->renderer().node())
            return run;
    }
    return { };
}

RunIterator PathLine::logicalEndRunWithNode() const
{
    for (auto run = logicalEndRun(); run; run.traversePreviousOnLineInLogicalOrder()) {
        if (run->renderer().node())
            return run;
    }
    return { };
}

RunIterator PathLine::closestRunForPoint(const IntPoint& pointInContents, bool editableOnly) const
{
    return closestRunForLogicalLeftPosition(isHorizontal() ? pointInContents.x() : pointInContents.y(), editableOnly);
}

RunIterator PathLine::closestRunForLogicalLeftPosition(int leftPosition, bool editableOnly) const
{
    auto isEditable = [&](auto run) {
        return run && run->renderer().node() && run->renderer().node()->hasEditableStyle();
    };

    auto firstRun = this->firstRun();
    auto lastRun = this->lastRun();

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

int PathLine::blockDirectionPointInLine() const
{
    return !containingBlock().style().isFlippedBlocksWritingMode() ? std::max(top(), selectionTop()) : std::min(bottom(), selectionBottom());
}

LayoutUnit PathLine::selectionTopAdjustedForPrecedingBlock() const
{
    return containingBlock().adjustSelectionTopForPrecedingBlock(selectionTop());
}

LayoutUnit PathLine::selectionHeightAdjustedForPrecedingBlock() const
{
    return std::max<LayoutUnit>(0, selectionBottom() - selectionTopAdjustedForPrecedingBlock());
}

RenderObject::HighlightState PathLine::selectionState() const
{
    auto& block = containingBlock();
    if (block.selectionState() == RenderObject::None)
        return RenderObject::None;

    auto state = RenderObject::None;
    for (auto box = firstRun(); box; box.traverseNextOnLine()) {
        auto boxState = box->selectionState();
        if ((boxState == RenderObject::HighlightState::Start && state == RenderObject::HighlightState::End)
            || (boxState == RenderObject::HighlightState::End && state == RenderObject::HighlightState::Start))
            state = RenderObject::HighlightState::Both;
        else if (state == RenderObject::HighlightState::None || ((boxState == RenderObject::HighlightState::Start || boxState == RenderObject::HighlightState::End)
            && (state == RenderObject::HighlightState::None || state == RenderObject::HighlightState::Inside)))
            state = boxState;
        else if (boxState == RenderObject::HighlightState::None && state == RenderObject::HighlightState::Start) {
            // We are past the end of the selection.
            state = RenderObject::HighlightState::Both;
        }
        if (state == RenderObject::HighlightState::Both)
            break;
    }
    return state;
}

RunIterator PathLine::firstSelectedBox() const
{
    for (auto box = firstRun(); box; box.traverseNextOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return { };
}

RunIterator PathLine::lastSelectedBox() const
{
    for (auto box = lastRun(); box; box.traversePreviousOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return { };
}

}
}

