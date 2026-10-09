/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "InlineDisplayContent.h"

namespace WebCore {
namespace InlineDisplay {

void Content::clear()
{
    lines.clear();
    boxes.clear();
    lineEllipses = { };
    maxedOutGlyphOverflows.clear();
}

void Content::set(Content&& newContent)
{
    lines = WTF::move(newContent.lines);
    boxes = WTF::move(newContent.boxes);
    lineEllipses = WTF::move(newContent.lineEllipses);
    maxedOutGlyphOverflows = WTF::move(newContent.maxedOutGlyphOverflows);
}

void Content::append(Content&& newContent)
{
    auto oldLineCount = lines.size();
    auto oldBoxCount = boxes.size();
    lines.appendVector(WTF::move(newContent.lines));
    boxes.appendVector(WTF::move(newContent.boxes));

    for (auto& entry : newContent.maxedOutGlyphOverflows)
        maxedOutGlyphOverflows.append({ entry.boxIndex + oldBoxCount, entry.top, entry.bottom });

    if (newContent.lineEllipses) {
        if (!lineEllipses)
            lineEllipses = makeUnique<LineEllipses>();
        if (lineEllipses->size() < oldLineCount)
            lineEllipses->grow(oldLineCount);
        lineEllipses->appendVector(WTF::move(*newContent.lineEllipses));
    }
}

void Content::insert(Content&& newContent, size_t lineIndex, size_t boxIndex)
{
    auto newBoxCount = newContent.boxes.size();
    lines.insertVector(lineIndex, WTF::move(newContent.lines));
    boxes.insertVector(boxIndex, WTF::move(newContent.boxes));

    auto insertionPosition = maxedOutGlyphOverflowPosition(boxIndex);
    for (auto position = insertionPosition; position < maxedOutGlyphOverflows.size(); ++position)
        maxedOutGlyphOverflows[position].boxIndex += newBoxCount;
    for (auto& entry : newContent.maxedOutGlyphOverflows)
        entry.boxIndex += boxIndex;
    maxedOutGlyphOverflows.insertVector(insertionPosition, WTF::move(newContent.maxedOutGlyphOverflows));

    if (newContent.lineEllipses) {
        if (!lineEllipses) {
            lineEllipses = makeUnique<LineEllipses>();
            lineEllipses->grow(lineIndex);
        }
        lineEllipses->insertVector(lineIndex, WTF::move(*newContent.lineEllipses));
    }
}

void Content::remove(size_t firstLineIndex, size_t numberOfLines, size_t firstBoxIndex, size_t numberOfBoxes)
{
    lines.removeAt(firstLineIndex, numberOfLines);
    boxes.removeAt(firstBoxIndex, numberOfBoxes);

    if (lineEllipses) {
        auto end = std::min(firstLineIndex + numberOfLines, lineEllipses->size());
        if (end > firstLineIndex)
            lineEllipses->removeAt(firstLineIndex, end - firstLineIndex);
    }

    auto firstRemoved = maxedOutGlyphOverflowPosition(firstBoxIndex);
    auto firstKept = maxedOutGlyphOverflowPosition(firstBoxIndex + numberOfBoxes);
    for (auto position = firstKept; position < maxedOutGlyphOverflows.size(); ++position)
        maxedOutGlyphOverflows[position].boxIndex -= numberOfBoxes;
    maxedOutGlyphOverflows.removeAt(firstRemoved, firstKept - firstRemoved);
}

size_t Content::maxedOutGlyphOverflowPosition(size_t boxIndex) const
{
    auto* position = std::lower_bound(maxedOutGlyphOverflows.begin(), maxedOutGlyphOverflows.end(), boxIndex, [](auto& entry, size_t boxIndex) {
        return entry.boxIndex < boxIndex;
    });
    return position - maxedOutGlyphOverflows.begin();
}

FloatBoxExtent Content::glyphOverflow(size_t boxIndex) const
{
    auto& box = boxes[boxIndex];
    if (!box.hasMaxedOutGlyphOverflow())
        return box.glyphOverflow();

    auto position = maxedOutGlyphOverflowPosition(boxIndex);
    if (position == maxedOutGlyphOverflows.size() || maxedOutGlyphOverflows[position].boxIndex != boxIndex) {
        ASSERT_NOT_REACHED();
        return box.glyphOverflow();
    }
    auto& entry = maxedOutGlyphOverflows[position];
    return { entry.top, 0.f, entry.bottom, 0.f };
}

void Content::setMaxedOutGlyphOverflow(size_t boxIndex, const FloatBoxExtent& glyphOverflow)
{
    ASSERT(boxes[boxIndex].hasMaxedOutGlyphOverflow());
    auto entry = MaxedOutGlyphOverflow { boxIndex, glyphOverflow.top(), glyphOverflow.bottom() };
    auto position = maxedOutGlyphOverflowPosition(boxIndex);
    if (position < maxedOutGlyphOverflows.size() && maxedOutGlyphOverflows[position].boxIndex == boxIndex) {
        maxedOutGlyphOverflows[position] = entry;
        return;
    }
    maxedOutGlyphOverflows.insert(position, entry);
}

void Content::setLineEllipsis(size_t lineIndex, Line::Ellipsis&& ellipsis)
{
    if (!lineEllipses)
        lineEllipses = makeUnique<LineEllipses>();

    if (lineEllipses->size() <= lineIndex)
        lineEllipses->grow(lineIndex + 1);
    else
        ASSERT(lineEllipses->at(lineIndex));

    lineEllipses->at(lineIndex) = WTF::move(ellipsis);
}

void Content::setEllipsisOnTrailingLine(Line::Ellipsis&& ellipsis)
{
    if (lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    setLineEllipsis(lines.size() - 1, WTF::move(ellipsis));
}

std::optional<Line::Ellipsis> Content::lineEllipsis(size_t lineIndex) const
{
    if (!lines[lineIndex].hasEllipsis())
        return { };

    if (!lineEllipses) {
        ASSERT_NOT_REACHED();
        return { };
    }
    if (lineEllipses->size() <= lineIndex) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return lineEllipses->at(lineIndex);
}

void Content::moveLineInBlockDirection(size_t lineIndex, float offset)
{
    if (!offset)
        return;

    auto& line = lines[lineIndex];
    line.moveInBlockDirection(offset);

    if (line.hasEllipsis()) {
        auto ellipsis = *lineEllipsis(lineIndex);
        auto physicalOffset = line.isHorizontal() ? FloatSize { { }, offset } : FloatSize { offset, { } };
        ellipsis.visualRect.move(physicalOffset);
        setLineEllipsis(lineIndex, WTF::move(ellipsis));
    }
}

void Content::shrinkLineInBlockDirection(size_t lineIndex, float delta)
{
    if (!delta)
        return;

    auto& line = lines[lineIndex];
    line.shrinkInBlockDirection(delta);

    if (line.hasEllipsis()) {
        auto ellipsis = *lineEllipsis(lineIndex);
        auto physicalDelta = line.isHorizontal() ? FloatSize { { }, delta } : FloatSize { delta, { } };
        ellipsis.visualRect.contract(physicalDelta);
        setLineEllipsis(lineIndex, WTF::move(ellipsis));
    }
}


}
}

