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

#include <WebCore/GridLayoutConstraints.h>
#include <WebCore/GridTypeAliases.h>
#include <WebCore/LayoutIntegrationUtils.h>
#include <WebCore/LayoutState.h>
#include <wtf/CheckedRef.h>

namespace WebCore {
namespace Layout {

class ElementBox;
class PlacedGridItem;

class UnplacedGridItem;

struct GridAreaLines;
struct UnplacedGridItems;
struct UsedTrackSizes;

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

// https://drafts.csswg.org/css-grid-1/#grid-definition
struct GridDefinition {
    Style::GridTemplateList gridTemplateColumns;
    Style::GridTemplateList gridTemplateRows;
    Style::GridTrackSizes gridAutoColumns;
    Style::GridTrackSizes gridAutoRows;
    GridAutoFlowOptions autoFlowOptions;
};

class GridFormattingContext {
    WTF_MAKE_TZONE_ALLOCATED(GridFormattingContext);
public:

    GridFormattingContext(const ElementBox& gridBox, LayoutState&);

    UsedTrackSizes layout(GridLayoutConstraints);

    struct IntrinsicWidths {
        LayoutUnit minimum;
        LayoutUnit maximum;
    };

    IntrinsicWidths computeIntrinsicWidths();

    PlacedGridItems constructPlacedGridItems(const GridAreas&) const;

    const ElementBox& root() const { return m_gridBox; }

    const IntegrationUtils& integrationUtils() const { return m_integrationUtils; }

    const BoxGeometry& geometryForGridItem(const ElementBox&) const;

    const Style::ZoomFactor zoomFactor() const { return m_gridBox->style().usedZoomForLength(); }

    const WritingMode writingMode() const { return m_gridBox->style().writingMode(); }

    // FIXME: This is only here because the integration code needs to know the
    // row gap to update RenderGrid. We should figure out a way to do that and remove
    // this from the public API.
    static LayoutUnit usedGapValue(const Style::GapGutter& gap)
    {
        if (gap.isNormal())
            return { };

        // Only handle fixed length gaps for now
        if (auto fixedGap = gap.tryFixed())
            return Style::evaluate<LayoutUnit>(*fixedGap, 0_lu, Style::ZoomNeeded { });

        ASSERT_NOT_REACHED();
        return { };
    }

private:
    UnplacedGridItems constructUnplacedGridItems() const;

    const LayoutState& layoutState() const { return m_globalLayoutState; }
    BoxGeometry& geometryForGridItem(const ElementBox&);
    void setGridItemGeometries(const GridItemRects&);

    const RenderStyle& gridContainerStyle() const { return m_gridBox->style(); }

    const CheckedRef<const ElementBox> m_gridBox;
    const CheckedRef<LayoutState> m_globalLayoutState;
    const IntegrationUtils m_integrationUtils;
};

} // namespace Layout
} // namespace WebCore
