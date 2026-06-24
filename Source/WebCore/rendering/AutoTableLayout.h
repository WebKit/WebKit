/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "LayoutUnit.h"
#include "StylePreferredSize.h"
#include "TableLayout.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderTable;
class RenderTableCell;

class AutoTableLayout final : public TableLayout {
public:
    explicit AutoTableLayout(RenderTable*);
    virtual ~AutoTableLayout();

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths(TableIntrinsics) override;
    LayoutUnit scaledWidthFromPercentColumns() const override { return m_scaledWidthFromPercentColumns; }
    void applyContentLogicalWidthQuirks(LayoutUnit& minWidth, LayoutUnit& maxWidth) const override;
    void layout() override;

private:
    void fullRecalc();
    void recalcColumn(unsigned effCol);

    float calcEffectiveLogicalWidth();
    template<typename> float NODELETE shrinkCellWidthForType(float available);

    void insertSpanCell(RenderTableCell*);

    bool isColumnCollapsed(unsigned effCol) const;

    struct Layout {
        Style::PreferredSize logicalWidth { CSS::Keyword::Auto { } };
        Style::PreferredSize effectiveLogicalWidth { CSS::Keyword::Auto { } };
        float minLogicalWidth { 0 };
        float maxLogicalWidth { 0 };
        float effectiveMinLogicalWidth { 0 };
        float effectiveMaxLogicalWidth { 0 };
        float computedLogicalWidth { 0 };
        bool emptyCellsOnly { true };
        float usedZoom { 1.0f };
    };

    Vector<Layout> m_layoutStruct;
    Vector<RenderTableCell*> m_spanCells;
    bool m_hasPercent : 1 { false };
    mutable bool m_effectiveLogicalWidthDirty : 1 { true };
    LayoutUnit m_scaledWidthFromPercentColumns;
};

} // namespace WebCore
