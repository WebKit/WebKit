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

#pragma once

#include "StyleGridPosition.h"

namespace WebCore {
namespace Layout {

class ElementBox;
class GridLayout;

class UnplacedGridItem {
private:
    // https://drafts.csswg.org/css-grid-1/#placement
    // A definite value for any two of Start, End, and Span in a given dimension implies
    // a definite value for the third.
    //
    // A "definite" axis (at least one edge references an explicit line) resolves to a concrete
    // 0-based line range. Everything else (auto/auto, span/auto, auto/span) is auto-positioned
    // and only carries the span size that the placement algorithm resolves into a position later.
    struct DefinitePosition {
        // 0-based grid line indices; endLine is exclusive.
        size_t startLine { 0 };
        size_t endLine { 0 };

        bool operator==(const DefinitePosition&) const = default;
    };

    struct AutoPosition {
        // Number of tracks the item spans; resolved to a position during auto-placement.
        size_t span { 1 };

        bool operator==(const AutoPosition&) const = default;
    };

    class GridPosition {
    public:
        static GridPosition create(const Style::GridPosition& start, const Style::GridPosition& end);

        bool isDefinite() const { return std::holds_alternative<DefinitePosition>(m_position); }
        bool isAuto() const { return std::holds_alternative<AutoPosition>(m_position); }

        const DefinitePosition& definitePosition() const { return std::get<DefinitePosition>(m_position); }

        size_t span() const;

        bool operator==(const GridPosition&) const = default;

    private:
        using Value = Variant<DefinitePosition, AutoPosition>;

        GridPosition(DefinitePosition position)
            : m_position(position) { }
        GridPosition(AutoPosition position)
            : m_position(position) { }

        Value m_position;
    };

public:
    UnplacedGridItem(const ElementBox&, Style::GridPosition columnStart, Style::GridPosition columnEnd, Style::GridPosition rowStart, Style::GridPosition rowEnd);
    UnplacedGridItem(WTF::HashTableEmptyValueType);

    bool operator==(const UnplacedGridItem& other) const;

    bool isHashTableDeletedValue() const { return m_layoutBox.isHashTableDeletedValue(); }
    bool isHashTableEmptyValue() const { return m_layoutBox.isHashTableEmptyValue(); }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    size_t normalizedColumnStart() const;
    size_t normalizedColumnEnd() const;
    size_t normalizedRowStart() const;
    size_t normalizedRowEnd() const;

    bool NODELETE hasDefiniteRowPosition() const;
    bool NODELETE hasDefiniteColumnPosition() const;
    bool NODELETE hasAutoColumnPosition() const;
    bool NODELETE hasAutoRowPosition() const;
    size_t columnSpanSize() const;
    size_t rowSpanSize() const;

    std::pair<size_t, size_t> normalizedRowStartEnd() const;
    std::pair<size_t, size_t> normalizedColumnStartEnd() const;

private:
    CheckedRef<const ElementBox> m_layoutBox;

    // https://drafts.csswg.org/css-grid-1/#typedef-grid-row-start-grid-line
    GridPosition m_columnPosition;
    GridPosition m_rowPosition;

    std::pair<int, int> definiteRowStartEnd() const;
    std::pair<int, int> definiteColumnStartEnd() const;

    void NODELETE applyGridOffsets(size_t rowOffset, size_t columnOffset);

    // Offsets applied to normalize negative grid positions to non-negative matrix indices.
    size_t m_rowNormalizationOffset { 0 };
    size_t m_columnNormalizationOffset { 0 };

    // Flag to track whether applyGridOffsets() has been called.
    // This helps catch bugs where normalized methods are used before offsets are applied,
    // or where offsets are applied multiple times.
    bool m_hasAppliedGridOffsets { false };

    friend class GridFormattingContext;
    friend class GridLayout;
    friend class PlacedGridItem;
    friend void add(Hasher&, const WebCore::Layout::UnplacedGridItem&);
};

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
struct UnplacedGridItems {
    // 1. Position anything that’s not auto-positioned.
    Vector<UnplacedGridItem> nonAutoPositionedItems;
    // 2. Process the items locked to a given row.
    Vector<UnplacedGridItem> definiteRowPositionedItems;
    // 4. Position the remaining grid items.
    Vector<UnplacedGridItem> autoPositionedItems;
};

}
}

namespace WTF {

template<> struct HashTraits<WebCore::Layout::UnplacedGridItem> : SimpleClassHashTraits<WebCore::Layout::UnplacedGridItem> {
    static const bool emptyValueIsZero = HashTraits<CheckedRef<const WebCore::Layout::ElementBox>>::emptyValueIsZero;
    static constexpr bool hasIsEmptyValueFunction = true;

    static bool isEmptyValue(const WebCore::Layout::UnplacedGridItem& unplacedGridItem) { return unplacedGridItem.isHashTableEmptyValue(); }
    static WebCore::Layout::UnplacedGridItem emptyValue() { return WebCore::Layout::UnplacedGridItem { HashTableEmptyValueType::HashTableEmptyValue }; }
};

}
