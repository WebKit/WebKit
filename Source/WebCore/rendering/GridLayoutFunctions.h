/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayoutUnit.h"
#include "RenderBox.h"

namespace WebCore {

namespace Style {
enum class GridTrackSizingDirection : bool;
}

class GridSpan;
class RenderElement;
class RenderGrid;

enum class ItemPosition : uint8_t;

struct ExtraMarginsFromSubgrids {
    inline LayoutUnit extraTrackStartMargin() const { return m_extraMargins.first; }
    inline LayoutUnit extraTrackEndMargin() const { return m_extraMargins.second; }
    inline LayoutUnit extraTotalMargin() const { return m_extraMargins.first + m_extraMargins.second; }

    ExtraMarginsFromSubgrids& operator+=(const ExtraMarginsFromSubgrids& rhs)
    {
        m_extraMargins.first += rhs.extraTrackStartMargin();
        m_extraMargins.second += rhs.extraTrackEndMargin();
        return *this;
    }

    void addTrackStartMargin(LayoutUnit extraMargin) { m_extraMargins.first += extraMargin; }
    void addTrackEndMargin(LayoutUnit extraMargin) { m_extraMargins.second += extraMargin; }

    std::pair<LayoutUnit, LayoutUnit> m_extraMargins;
};

namespace GridLayoutFunctions {

LayoutUnit computeMarginLogicalSizeForGridItem(const RenderGrid&, Style::GridTrackSizingDirection, const RenderBox&);
LayoutUnit marginLogicalSizeForGridItem(const RenderGrid&, Style::GridTrackSizingDirection, const RenderBox&);
void setOverridingContentSizeForGridItem(const RenderGrid&, RenderBox& gridItem, LayoutUnit, Style::GridTrackSizingDirection);
void clearOverridingContentSizeForGridItem(const RenderGrid&, RenderBox& gridItem, Style::GridTrackSizingDirection);
bool isOrthogonalGridItem(const RenderGrid&, const RenderBox&);
bool isGridItemInlineSizeDependentOnBlockConstraints(const RenderBox& gridItem, const RenderGrid& parentGrid, ItemPosition gridItemAlignSelf);
bool isOrthogonalParent(const RenderGrid&, const RenderElement& parent);
bool isAspectRatioBlockSizeDependentGridItem(const RenderBox&);
Style::GridTrackSizingDirection flowAwareDirectionForGridItem(const RenderGrid&, const RenderBox&, Style::GridTrackSizingDirection);
Style::GridTrackSizingDirection flowAwareDirectionForParent(const RenderGrid&, const RenderElement& parent, Style::GridTrackSizingDirection);
std::optional<RenderBox::GridAreaSize> overridingContainingBlockContentSizeForGridItem(const RenderBox&, Style::GridTrackSizingDirection);
bool hasRelativeOrIntrinsicSizeForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection);

bool isFlippedDirection(const RenderGrid&, Style::GridTrackSizingDirection);
bool isSubgridReversedDirection(const RenderGrid&, Style::GridTrackSizingDirection outerDirection, const RenderGrid& subgrid);
ExtraMarginsFromSubgrids extraMarginForSubgridAncestors(Style::GridTrackSizingDirection, const RenderBox& gridItem);

unsigned alignmentContextForBaselineAlignment(const GridSpan&, const ItemPosition& alignment);

} // namespace GridLayoutFunctions

} // namespace WebCore
