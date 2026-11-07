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

#include "GridTypeAliases.h"
#include "LayoutState.h"
#include <wtf/CheckedPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class RenderGrid;

namespace Layout {
class ElementBox;
class GridLinePositions;
struct GridLayoutConstraints;
struct UsedTrackSizes;
}

namespace LayoutIntegration {

class GridLayout {
public:
    GridLayout(RenderGrid&);
    ~GridLayout();

    void updateFormattingContextGeometries();
    void clearGridItemGridAreas();

    void layout();

    static void resetStateOnLayoutPathChange(RenderGrid&);
    static void clearGridItemGridAreasLeftByGridFormattingContext(RenderGrid&);
    // The grid container's border box minimum and maximum inline size transferred through its aspect-ratio, from RenderBox.
    static std::pair<LayoutUnit, LayoutUnit> transferredInlineAxisMinMaxSizes(const RenderGrid&);

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicWidths();

    friend WTF::TextStream& operator<<(WTF::TextStream&, const GridLayout&);

private:
    using GridItemBorderBoxRects = Vector<LayoutRect, 4>;
    GridItemBorderBoxRects gridItemBorderBoxRects() const;

    void updateGridItemRenderers(const GridItemBorderBoxRects& previousGridItemRects);
    void updateFormattingContextRootRendererBeforeLayout();
    void updateGridItemGridAreas(const Layout::GridLinePositions& columnLines, const Layout::GridLinePositions& rowLines, const Layout::GridItemRects&);
    void updateFormattingContextRootRenderer(const Layout::UsedTrackSizes&, LayoutUnit usedBlockContentSize, const Layout::GridItemRects&, size_t autoRepeatColumnsCount, size_t autoRepeatRowsCount, bool autoRepeatColumnsTruncatedByLimit, bool autoRepeatRowsTruncatedByLimit, size_t leadingImplicitColumnsCount, size_t leadingImplicitRowsCount, const BitVector& collapsedColumnTracks, const BitVector& collapsedRowTracks);
    void layoutOutOfFlowBoxes();
    void updateOverflow(RenderGrid&);
    void populateGridPositions(const Layout::GridLinePositions& columnLines, const Layout::GridLinePositions& rowLines);

    const Layout::ElementBox& gridBox() const { return *m_gridBox; }
    Layout::ElementBox& gridBox() { return *m_gridBox; }

    const RenderGrid& gridBoxRenderer() const { return downcast<RenderGrid>(*m_gridBox->rendererForIntegration()); }
    RenderGrid& gridBoxRenderer() { return downcast<RenderGrid>(*m_gridBox->rendererForIntegration()); }

    Layout::LayoutState& layoutState() LIFETIME_BOUND { return m_layoutState; }
    const Layout::LayoutState& layoutState() const LIFETIME_BOUND { return m_layoutState; }

    const CheckedPtr<Layout::ElementBox> m_gridBox;
    CheckedRef<Layout::LayoutState> m_layoutState;
};

WTF::TextStream& operator<<(WTF::TextStream&, const GridLayout&);

} // namespace LayoutIntegration

} // namespace WebCore
