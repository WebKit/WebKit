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

#include "ExplicitGridTrackSizes.h"
#include "GridLinePositions.h"
#include "GridTypeAliases.h"
#include "UsedTrackSizes.h"
#include <WebCore/LayoutIntegrationUtils.h>
#include <WebCore/LayoutState.h>
#include <WebCore/StyleGapGutter.h>
#include <WebCore/StyleGridTrackSizes.h>
#include <WebCore/StylePrimitiveNumericTypes+Evaluation.h>
#include <WebCore/StyleZoomPrimitives.h>
#include <wtf/CheckedRef.h>

namespace WebCore {
namespace Layout {

class ElementBox;
class PlacedGridItem;
class UnplacedGridItem;

struct AutoRepeatConstraint;
struct GridAreaLines;
struct GridLayoutConstraints;

enum class PackingStrategy : bool {
    Sparse,
    Dense
};

enum class GridAutoFlowDirection : bool {
    Row,
    Column
};

struct GridAutoFlowOptions {
    PackingStrategy strategy;
    GridAutoFlowDirection direction;
};

// The grid container's baselines, as logical block offsets from the block-start edge of the grid container's content box.
// The caller adds the container's border and padding to make them relative to its border box.
// https://drafts.csswg.org/css-grid-1/#grid-baselines
struct GridContainerBaselines {
    std::optional<LayoutUnit> first;
    std::optional<LayoutUnit> last;
};

struct GridLayoutResult {
    UsedTrackSizes usedTrackSizes;
    GridLinePositions columnLines;
    GridLinePositions rowLines;
    GridItemRects gridItemRects;
    LayoutUnit usedBlockContentSize;
    // The number of explicit grid tracks that came from an auto-repeat.
    size_t autoRepeatColumnsCount { 0 };
    size_t autoRepeatRowsCount { 0 };
    // https://drafts.csswg.org/css-grid-1/#overlarge-grids
    // Whether the limit on the number of tracks left out repetitions of the auto-repeat, or part of the last one, that the container
    // would have fitted. The resolved track list needs it to name the last line of the auto-repeat (a repeat that ends exactly at the
    // limit is followed by the lines after it, one that was cut is not). Only integration reads it, to write the RenderGrid state that
    // the resolved track list is built from; the formatting context does not use it.
    bool autoRepeatColumnsTruncatedByLimit { false };
    bool autoRepeatRowsTruncatedByLimit { false };

    size_t leadingImplicitColumnsCount { 0 };
    size_t leadingImplicitRowsCount { 0 };

    BitVector collapsedColumnTracks;
    BitVector collapsedRowTracks;

    GridContainerBaselines baselines;
};

// The number of implicit tracks generated before the start of the explicit grid, per axis, because
// an item is placed with a negative line that resolves before line 1. Every item's resolved line is
// shifted forward by these counts when the UnplacedGridItems are constructed so that matrix indices
// are non-negative, and layout uses them to include the leading tracks in the grid's initial
// dimensions and track sizing functions.
struct LeadingImplicitTracks {
    size_t columnsCount { 0 };
    size_t rowsCount { 0 };
};

// https://drafts.csswg.org/css-grid-1/#grid-definition
struct GridDefinition {
    ExplicitGridTrackSizes explicitGridTrackSizes;
    Style::GridTrackSizes gridAutoColumns;
    Style::GridTrackSizes gridAutoRows;
    GridAutoFlowOptions autoFlowOptions;
    Style::ZoomFactor zoom;
};

// Static classification of how much grid-sizing work is required to compute
// the grid's intrinsic widths. Set once per GridFormattingContext before
// either the min-content or max-content scenario runs.
enum class IntrinsicWidthSizingPath : uint8_t {
    ColumnsOnly, // No item's inline contribution depends on the item's own block size.
    NeedsFullSizing, // At least one item's inline contribution depends on the item's own block size.
};

class GridFormattingContext {
    WTF_MAKE_TZONE_ALLOCATED(GridFormattingContext);
public:

    GridFormattingContext(const ElementBox& gridBox, LayoutState&);

    GridLayoutResult layout(GridLayoutConstraints);

    struct IntrinsicWidths {
        LayoutUnit minimum;
        LayoutUnit maximum;
    };

    IntrinsicWidths computeIntrinsicWidths(const AutoRepeatConstraint& inlineAxisAutoRepeatConstraint, const AutoRepeatConstraint& blockAxisAutoRepeatConstraint);
    IntrinsicWidthSizingPath intrinsicWidthSizingPath() const { return m_intrinsicWidthSizingPath; }

    const ElementBox& root() const { return m_gridBox; }

    const IntegrationUtils& integrationUtils() const LIFETIME_BOUND { return m_integrationUtils; }

    const WritingMode writingMode() const { return m_gridBox->style().writingMode(); }

private:
    IntrinsicWidthSizingPath classifyIntrinsicWidthSizingPath() const;

    const LayoutState& layoutState() const LIFETIME_BOUND { return m_globalLayoutState; }
    BoxGeometry& geometryForGridItem(const ElementBox&) LIFETIME_BOUND;
    void setGridItemGeometries(const GridItemRects&);

    const CheckedRef<const ElementBox> m_gridBox;
    const CheckedRef<LayoutState> m_globalLayoutState;
    const IntegrationUtils m_integrationUtils;
    const IntrinsicWidthSizingPath m_intrinsicWidthSizingPath;
};

} // namespace Layout
} // namespace WebCore
