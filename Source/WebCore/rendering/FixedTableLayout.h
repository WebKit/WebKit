/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
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
#include "TableLayout.h"
#include <wtf/Vector.h>

namespace WebCore {

namespace Style {
struct PreferredSize;
}

class RenderTable;

class FixedTableLayout final : public TableLayout {
public:
    explicit FixedTableLayout(RenderTable*);

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths(TableIntrinsics) override;
    void applyContentLogicalWidthQuirks(LayoutUnit& minWidth, LayoutUnit& maxWidth) const override;
    void layout() override;

private:
    float calcWidthArray();

    Vector<Style::PreferredSize> m_width;
    // For percentage-width columns sized by a content-box cell, the cell's border and
    // padding is reserved in addition to the resolved percentage (matching Firefox and
    // current Chrome). Parallel to m_width; zero for fixed/auto columns.
    Vector<float> m_columnBorderPadding;
};

} // namespace WebCore
