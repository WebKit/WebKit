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

bool LineIterator::atEnd() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) {
        return path.atEnd();
    });
}

LineIterator LineIterator::next() const
{
    return LineIterator(*this).traverseNext();
}

LineIterator LineIterator::previous() const
{
    return LineIterator(*this).traversePrevious();
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
    if (m_line.m_pathVariant.index() != other.m_line.m_pathVariant.index())
        return false;

    return WTF::switchOn(m_line.m_pathVariant, [&](const auto& path) {
        return path == WTF::get<std::decay_t<decltype(path)>>(other.m_line.m_pathVariant);
    });
}

LineRunIterator LineIterator::firstRun() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.firstRun() };
    });
}

LineRunIterator LineIterator::lastRun() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.lastRun() };
    });
}

LineRunIterator LineIterator::logicalStartRunWithNode() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.logicalStartRunWithNode() };
    });
}

LineRunIterator LineIterator::logicalEndRunWithNode() const
{
    return WTF::switchOn(m_line.m_pathVariant, [](auto& path) -> RunIterator {
        return { path.logicalEndRunWithNode() };
    });
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

LineRunIterator LineIterator::closestRunForPoint(const IntPoint& pointInContents, bool editableOnly)
{
    if (atEnd())
        return { };
    return closestRunForLogicalLeftPosition(m_line.isHorizontal() ? pointInContents.x() : pointInContents.y(), editableOnly);
}

LineRunIterator LineIterator::closestRunForLogicalLeftPosition(int leftPosition, bool editableOnly)
{
    auto isEditable = [&](auto run)
    {
        return run && run->renderer().node() && run->renderer().node()->hasEditableStyle();
    };

    auto firstRun = this->firstRun();
    auto lastRun = this->lastRun();

    if (firstRun != lastRun) {
        if (firstRun->isLineBreak())
            firstRun = firstRun.nextOnLineIgnoringLineBreak();
        else if (lastRun->isLineBreak())
            lastRun = lastRun.previousOnLineIgnoringLineBreak();
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

IntRect PathLine::computeCaretRect(float logicalLeftPosition, unsigned caretWidth, LayoutUnit* extraWidthToEndOfLine) const
{
    int height = selectionBottom() - selectionTop();
    int top = selectionTop();

    // Distribute the caret's width to either side of the offset.
    float left = logicalLeftPosition;
    int caretWidthLeftOfOffset = caretWidth / 2;
    left -= caretWidthLeftOfOffset;
    int caretWidthRightOfOffset = caretWidth - caretWidthLeftOfOffset;
    left = roundf(left);

    float lineLeft = logicalLeft();
    float lineRight = logicalRight();

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = lineRight - (left + caretWidth);

    const RenderStyle& blockStyle = containingBlock().style();

    bool rightAligned = false;
    switch (blockStyle.textAlign()) {
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        rightAligned = true;
        break;
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        break;
    case TextAlignMode::Justify:
    case TextAlignMode::Start:
        rightAligned = !blockStyle.isLeftToRightDirection();
        break;
    case TextAlignMode::End:
        rightAligned = blockStyle.isLeftToRightDirection();
        break;
    }

    float leftEdge = std::min<float>(0, lineLeft);
    float rightEdge = std::max<float>(containingBlock().logicalWidth(), lineRight);

    if (rightAligned) {
        left = std::max(left, leftEdge);
        left = std::min(left, lineRight - caretWidth);
    } else {
        left = std::min(left, rightEdge - caretWidthRightOfOffset);
        left = std::max(left, lineLeft);
    }
    return blockStyle.isHorizontalWritingMode() ? IntRect(left, top, caretWidth, height) : IntRect(top, left, height, caretWidth);
}

}
}

