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

UnplacedGridItem::UnplacedGridItem(const ElementBox& layoutBox, Style::GridPosition columnStart, Style::GridPosition columnEnd,
    Style::GridPosition rowStart, Style::GridPosition rowEnd)
    : m_layoutBox(layoutBox)
    , m_columnPosition({ columnStart, columnEnd })
    , m_rowPosition({ rowStart, rowEnd })
{
}

UnplacedGridItem::UnplacedGridItem(WTF::HashTableEmptyValueType)
    : m_layoutBox(WTF::HashTableEmptyValue)
    , m_columnPosition({ Style::ComputedStyle::initialGridItemColumnStart(), Style::ComputedStyle::initialGridItemColumnEnd() })
    , m_rowPosition({ Style::ComputedStyle::initialGridItemRowStart(), Style::ComputedStyle::initialGridItemRowEnd() })
{
}

int UnplacedGridItem::explicitColumnStart() const
{
    ASSERT(m_columnPosition.first.isExplicit());
    auto explicitColumnStart = m_columnPosition.first.explicitPosition();
    // https://www.w3.org/TR/css-grid-1/#line-placement
    // An <integer> value of zero makes the declaration invalid.
    ASSERT(explicitColumnStart);

    // Convert from 1-indexed CSS grid lines to 0-indexed matrix positions.
    return explicitColumnStart > 0 ? explicitColumnStart - 1 : explicitColumnStart;
}

size_t UnplacedGridItem::normalizedColumnStart() const
{
    return explicitColumnStart() + m_columnNormalizationOffset;
}

int UnplacedGridItem::explicitColumnEnd() const
{
    ASSERT(m_columnPosition.second.isExplicit());
    auto explicitColumnEnd = m_columnPosition.second.explicitPosition();
    // https://www.w3.org/TR/css-grid-1/#line-placement
    // An <integer> value of zero makes the declaration invalid.
    ASSERT(explicitColumnEnd);

    // Convert from 1-indexed CSS grid lines to 0-indexed matrix positions.
    return explicitColumnEnd > 0 ? explicitColumnEnd - 1 : explicitColumnEnd;
}

size_t UnplacedGridItem::normalizedColumnEnd() const
{
    return explicitColumnEnd() + m_columnNormalizationOffset;
}

int UnplacedGridItem::explicitRowStart() const
{
    ASSERT(m_rowPosition.first.isExplicit());
    auto explicitRowStart = m_rowPosition.first.explicitPosition();
    // https://www.w3.org/TR/css-grid-1/#line-placement
    // An <integer> value of zero makes the declaration invalid.
    ASSERT(explicitRowStart);

    // Convert from 1-indexed CSS grid lines to 0-indexed matrix positions.
    return explicitRowStart > 0 ? explicitRowStart - 1 : explicitRowStart;
}

size_t UnplacedGridItem::normalizedRowStart() const
{
    return explicitRowStart() + m_rowNormalizationOffset;
}

int UnplacedGridItem::explicitRowEnd() const
{
    ASSERT(m_rowPosition.second.isExplicit());
    auto explicitRowEnd = m_rowPosition.second.explicitPosition();
    // https://www.w3.org/TR/css-grid-1/#line-placement
    // An <integer> value of zero makes the declaration invalid.
    ASSERT(explicitRowEnd);

    // Convert from 1-indexed CSS grid lines to 0-indexed matrix positions.
    return explicitRowEnd > 0 ? explicitRowEnd - 1 : explicitRowEnd;
}

size_t UnplacedGridItem::normalizedRowEnd() const
{
    return explicitRowEnd() + m_rowNormalizationOffset;
}

bool UnplacedGridItem::hasDefiniteRowPosition() const
{
    return m_rowPosition.first.isExplicit() || m_rowPosition.second.isExplicit();
}

bool UnplacedGridItem::hasDefiniteColumnPosition() const
{
    return m_columnPosition.first.isExplicit() || m_columnPosition.second.isExplicit();
}

bool UnplacedGridItem::hasAutoColumnPosition() const
{
    return m_columnPosition.first.isAuto() && m_columnPosition.second.isAuto();
}

bool UnplacedGridItem::hasAutoRowPosition() const
{
    return m_rowPosition.first.isAuto() && m_rowPosition.second.isAuto();
}

size_t UnplacedGridItem::columnSpanSize() const
{
    auto firstPosition = m_columnPosition.first;
    auto secondPosition = m_columnPosition.second;

    // Case 1: Both positions are explicit - calculate span size
    if (firstPosition.isExplicit() && secondPosition.isExplicit())
        return explicitColumnEnd() - explicitColumnStart();

    // Case 2: One position is a span - extract its span size.
    ASSERT(!(firstPosition.isSpan() && secondPosition.isSpan()));
    if (firstPosition.isSpan())
        return firstPosition.spanPosition();
    if (secondPosition.isSpan())
        return secondPosition.spanPosition();

    // Default to span 1
    ASSERT(hasAutoColumnPosition());
    return 1;
}

size_t UnplacedGridItem::rowSpanSize() const
{
    auto& firstPosition = m_rowPosition.first;
    auto& secondPosition = m_rowPosition.second;

    // Case 1: Both positions are explicit - calculate span size
    if (firstPosition.isExplicit() && secondPosition.isExplicit())
        return explicitRowEnd() - explicitRowStart();

    // Case 2: One position is a span - extract its span size.
    ASSERT(!(firstPosition.isSpan() && secondPosition.isSpan()));
    if (firstPosition.isSpan())
        return firstPosition.spanPosition();
    if (secondPosition.isSpan())
        return secondPosition.spanPosition();

    // Default to span 1
    ASSERT(hasAutoRowPosition());
    return 1;
}

std::pair<int, int> UnplacedGridItem::definiteRowStartEnd() const
{
    auto startPosition = m_rowPosition.first;
    auto endPosition = m_rowPosition.second;

    if (startPosition.isExplicit() && endPosition.isExplicit())
        return { explicitRowStart(), explicitRowEnd() };

    if (startPosition.isExplicit() && endPosition.isSpan())
        return { explicitRowStart(), explicitRowStart() + endPosition.spanPosition() };

    if (startPosition.isSpan() && endPosition.isExplicit())
        return { explicitRowEnd() - startPosition.spanPosition(), explicitRowEnd() };

    if (startPosition.isExplicit() && endPosition.isAuto())
        return { explicitRowStart(), explicitRowStart() + 1 };

    if (startPosition.isAuto() && endPosition.isExplicit()) {
        auto explicitEnd = explicitRowEnd();
        ASSERT(explicitEnd >= 1);
        return { explicitEnd - 1, explicitEnd };
    }

    ASSERT_NOT_REACHED();
    return { 0, 0 };
}

std::pair<int, int> UnplacedGridItem::definiteColumnStartEnd() const
{
    auto startPosition = m_columnPosition.first;
    auto endPosition = m_columnPosition.second;

    if (startPosition.isExplicit() && endPosition.isExplicit())
        return { explicitColumnStart(), explicitColumnEnd() };

    if (startPosition.isExplicit() && endPosition.isSpan())
        return { explicitColumnStart(), explicitColumnStart() + endPosition.spanPosition() };

    if (startPosition.isSpan() && endPosition.isExplicit())
        return { explicitColumnEnd() - startPosition.spanPosition(), explicitColumnEnd() };

    if (startPosition.isExplicit() && endPosition.isAuto())
        return { explicitColumnStart(), explicitColumnStart() + 1 };

    if (startPosition.isAuto() && endPosition.isExplicit()) {
        auto explicitEnd = explicitColumnEnd();
        return { explicitEnd - 1, explicitEnd };
    }

    ASSERT_NOT_REACHED();
    return { 0, 0 };
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
    addArgs(hasher, unplacedGridItem.m_layoutBox.ptr(), unplacedGridItem.m_columnPosition, unplacedGridItem.m_rowPosition);
}

} // namespace Layout

} // namespace WebCore
