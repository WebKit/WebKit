/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 *
 */

#pragma once

namespace WebCore {

class RenderBlock;

class LogicalSelectionOffsetCaches {
public:
    class ContainingBlockInfo {
    public:
        ContainingBlockInfo()
            : m_hasFloatsOrFragmentedFlows(false)
            , m_cachedLogicalLeftSelectionOffset(false)
            , m_cachedLogicalRightSelectionOffset(false)
        { }

        inline void setBlock(RenderBlock*, const LogicalSelectionOffsetCaches*, bool parentCacheHasFloatsOrFragmentedFlows = false);
        inline LayoutUnit logicalLeftSelectionOffset(RenderBlock&, LayoutUnit) const;
        inline LayoutUnit logicalRightSelectionOffset(RenderBlock&, LayoutUnit) const;

        RenderBlock* block() const { return m_block; }
        const LogicalSelectionOffsetCaches* cache() const { return m_cache; }
        bool hasFloatsOrFragmentedFlows() const { return m_hasFloatsOrFragmentedFlows; }

    private:
        RenderBlock* m_block { nullptr };
        const LogicalSelectionOffsetCaches* m_cache { nullptr };
        bool m_hasFloatsOrFragmentedFlows : 1;
        mutable bool m_cachedLogicalLeftSelectionOffset : 1;
        mutable bool m_cachedLogicalRightSelectionOffset : 1;
        mutable LayoutUnit m_logicalLeftSelectionOffset;
        mutable LayoutUnit m_logicalRightSelectionOffset;
    };

    inline explicit LogicalSelectionOffsetCaches(RenderBlock&);
    inline LogicalSelectionOffsetCaches(RenderBlock&, const LogicalSelectionOffsetCaches&);

    inline const ContainingBlockInfo& containingBlockInfo(RenderBlock&) const;

private:
    ContainingBlockInfo m_containingBlockForFixedPosition;
    ContainingBlockInfo m_containingBlockForAbsolutePosition;
    ContainingBlockInfo m_containingBlockForInflowPosition;
};

} // namespace WebCore
