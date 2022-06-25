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
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class RenderTable;

enum class TableIntrinsics : uint8_t;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TableLayout);
class TableLayout {
    WTF_MAKE_NONCOPYABLE(TableLayout);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(TableLayout);
public:
    explicit TableLayout(RenderTable* table)
        : m_table(table)
    {
    }

    virtual ~TableLayout() = default;

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth, TableIntrinsics) = 0;
    virtual LayoutUnit scaledWidthFromPercentColumns() const { return 0_lu; }
    virtual void applyPreferredLogicalWidthQuirks(LayoutUnit& minWidth, LayoutUnit& maxWidth) const = 0;
    virtual void layout() = 0;

protected:
    // FIXME: Once we enable SATURATED_LAYOUT_ARITHMETHIC, this should just be LayoutUnit::nearlyMax().
    // Until then though, using nearlyMax causes overflow in some tests, so we just pick a large number.
    static constexpr int tableMaxWidth = 1000000;

    RenderTable* m_table;
};

} // namespace WebCore
