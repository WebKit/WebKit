/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "UnplacedGridItem.h"

#include "LayoutElementBox.h"
#include "StyleComputedStyle+InitialInlines.h"

namespace WebCore {
namespace Layout {

// Convert a 1-indexed explicit CSS grid line into a 0-indexed grid line. For example
// grid-column-start: 1 maps to 0. https://www.w3.org/TR/css-grid-1/#line-placement
static int explicitLineToIndex(const Style::GridPosition& position)
{
    ASSERT(position.isExplicit());
    auto line = position.explicitPosition();
    // A value of zero makes the declaration invalid.
    ASSERT(line);
    return line > 0 ? line - 1 : line;
}

UnplacedGridItem::GridPosition UnplacedGridItem::GridPosition::create(const Style::GridPosition& start, const Style::GridPosition& end)
{
    // An axis is only definite when one of its edges references an explicit line. Anything
    // else (auto/auto, span/auto, auto/span) is auto-positioned; carry the span forward for
    // the placement algorithm to resolve.
    if (!start.isExplicit() && !end.isExplicit()) {
        size_t span = 1;
        if (start.isSpan())
            span = start.spanPosition();
        else if (end.isSpan())
            span = end.spanPosition();
        return AutoPosition { span };
    }

    int startLine = 0;
    int endLine = 0;
    if (start.isExplicit() && end.isExplicit()) {
        startLine = explicitLineToIndex(start);
        endLine = explicitLineToIndex(end);
    } else if (start.isExplicit() && end.isSpan()) {
        startLine = explicitLineToIndex(start);
        endLine = startLine + end.spanPosition();
    } else if (start.isSpan() && end.isExplicit()) {
        endLine = explicitLineToIndex(end);
        startLine = endLine - static_cast<int>(start.spanPosition());
    } else if (start.isExplicit() && end.isAuto()) {
        startLine = explicitLineToIndex(start);
        endLine = startLine + 1;
    } else {
        ASSERT(start.isAuto() && end.isExplicit());
        endLine = explicitLineToIndex(end);
        startLine = endLine - 1;
    }

    // Negative line placement is not yet supported (grid coverage keeps such items on the
    // legacy path), so the resolved lines are non-negative and safe to store as unsigned.
    ASSERT(startLine >= 0 && endLine >= 0);
    // The range is always forward. The span/auto branches derive endLine from startLine, and the
    // explicit/explicit branch only reaches GFC for single-track placements: grid coverage requires
    // the end line to be exactly one past the start (a distance of 1), so an inverted placement like
    // grid-column: 3 / 1 stays on the legacy path and can never underflow span() here.
    ASSERT(startLine <= endLine);
    return DefinitePosition { static_cast<size_t>(startLine), static_cast<size_t>(endLine) };
}

size_t UnplacedGridItem::GridPosition::span() const
{
    if (auto* definitePosition = std::get_if<DefinitePosition>(&m_position))
        return definitePosition->endLine - definitePosition->startLine;
    return std::get<AutoPosition>(m_position).span;
}

UnplacedGridItem::UnplacedGridItem(const ElementBox& layoutBox, Style::GridPosition columnStart, Style::GridPosition columnEnd,
    Style::GridPosition rowStart, Style::GridPosition rowEnd)
    : m_layoutBox(layoutBox)
    , m_columnPosition(GridPosition::create(columnStart, columnEnd))
    , m_rowPosition(GridPosition::create(rowStart, rowEnd))
{
}

UnplacedGridItem::UnplacedGridItem(WTF::HashTableEmptyValueType)
    : m_layoutBox(WTF::HashTableEmptyValue)
    , m_columnPosition(GridPosition::create(Style::ComputedStyle::initialGridItemColumnStart(), Style::ComputedStyle::initialGridItemColumnEnd()))
    , m_rowPosition(GridPosition::create(Style::ComputedStyle::initialGridItemRowStart(), Style::ComputedStyle::initialGridItemRowEnd()))
{
}

size_t UnplacedGridItem::normalizedColumnStart() const
{
    ASSERT(m_hasAppliedGridOffsets);
    return m_columnPosition.definitePosition().startLine + m_columnNormalizationOffset;
}

size_t UnplacedGridItem::normalizedColumnEnd() const
{
    ASSERT(m_hasAppliedGridOffsets);
    return m_columnPosition.definitePosition().endLine + m_columnNormalizationOffset;
}

size_t UnplacedGridItem::normalizedRowStart() const
{
    ASSERT(m_hasAppliedGridOffsets);
    return m_rowPosition.definitePosition().startLine + m_rowNormalizationOffset;
}

size_t UnplacedGridItem::normalizedRowEnd() const
{
    ASSERT(m_hasAppliedGridOffsets);
    return m_rowPosition.definitePosition().endLine + m_rowNormalizationOffset;
}

bool UnplacedGridItem::hasDefiniteRowPosition() const
{
    return m_rowPosition.isDefinite();
}

bool UnplacedGridItem::hasDefiniteColumnPosition() const
{
    return m_columnPosition.isDefinite();
}

bool UnplacedGridItem::hasAutoColumnPosition() const
{
    return m_columnPosition.isAuto();
}

bool UnplacedGridItem::hasAutoRowPosition() const
{
    return m_rowPosition.isAuto();
}

size_t UnplacedGridItem::columnSpanSize() const
{
    return m_columnPosition.span();
}

size_t UnplacedGridItem::rowSpanSize() const
{
    return m_rowPosition.span();
}

std::pair<int, int> UnplacedGridItem::definiteRowStartEnd() const
{
    auto& definitePosition = m_rowPosition.definitePosition();
    return { static_cast<int>(definitePosition.startLine), static_cast<int>(definitePosition.endLine) };
}

std::pair<int, int> UnplacedGridItem::definiteColumnStartEnd() const
{
    auto& definitePosition = m_columnPosition.definitePosition();
    return { static_cast<int>(definitePosition.startLine), static_cast<int>(definitePosition.endLine) };
}

std::pair<size_t, size_t> UnplacedGridItem::normalizedRowStartEnd() const
{
    ASSERT(m_hasAppliedGridOffsets);
    auto rowStart = normalizedRowStart();
    auto rowEnd = normalizedRowEnd();

    // Handle inverted ranges by swapping start and end
    if (rowEnd < rowStart)
        return { rowEnd, rowStart };

    return { rowStart, rowEnd };
}

std::pair<size_t, size_t> UnplacedGridItem::normalizedColumnStartEnd() const
{
    ASSERT(m_hasAppliedGridOffsets);
    auto columnStart = normalizedColumnStart();
    auto columnEnd = normalizedColumnEnd();

    // Handle inverted ranges by swapping start and end
    if (columnEnd < columnStart)
        return { columnEnd, columnStart };

    return { columnStart, columnEnd };
}

bool UnplacedGridItem::operator==(const UnplacedGridItem& other) const
{
    // Since the hash table empty value uses CheckedRef's empty value,
    // we need to check if either |this| or |other| are the empty value
    // so we do not compare the uninitialized ref.
    bool isEmpty = isHashTableEmptyValue();
    if (isEmpty)
        return other.isHashTableEmptyValue();
    if (other.isHashTableEmptyValue())
        return isEmpty;

    return m_layoutBox.ptr() == other.m_layoutBox.ptr() && m_columnPosition == other.m_columnPosition && m_rowPosition == other.m_rowPosition;
}

void UnplacedGridItem::applyGridOffsets(size_t rowOffset, size_t columnOffset)
{
    ASSERT(!m_hasAppliedGridOffsets);
    m_rowNormalizationOffset = rowOffset;
    m_columnNormalizationOffset = columnOffset;
    m_hasAppliedGridOffsets = true;
}

void add(Hasher& hasher, const WebCore::Layout::UnplacedGridItem& unplacedGridItem)
{
    addArgs(hasher, unplacedGridItem.m_layoutBox.ptr());

    auto addPosition = [&](const auto& position) {
        if (position.isDefinite()) {
            auto& definitePosition = position.definitePosition();
            addArgs(hasher, static_cast<size_t>(0), definitePosition.startLine, definitePosition.endLine);
        } else
            addArgs(hasher, static_cast<size_t>(1), position.span());
    };
    addPosition(unplacedGridItem.m_columnPosition);
    addPosition(unplacedGridItem.m_rowPosition);
}

} // namespace Layout

} // namespace WebCore
