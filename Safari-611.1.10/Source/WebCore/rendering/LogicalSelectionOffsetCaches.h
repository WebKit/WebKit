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

#include "RenderBlock.h"

namespace WebCore {

class LogicalSelectionOffsetCaches {
public:
    class ContainingBlockInfo {
    public:
        ContainingBlockInfo()
            : m_hasFloatsOrFragmentedFlows(false)
            , m_cachedLogicalLeftSelectionOffset(false)
            , m_cachedLogicalRightSelectionOffset(false)
        { }

        void setBlock(RenderBlock* block, const LogicalSelectionOffsetCaches* cache, bool parentCacheHasFloatsOrFragmentedFlows = false)
        {
            m_block = block;
            bool blockHasFloatsOrFragmentedFlows = m_block ? (m_block->containsFloats() || m_block->enclosingFragmentedFlow()) : false;
            m_hasFloatsOrFragmentedFlows = parentCacheHasFloatsOrFragmentedFlows || m_hasFloatsOrFragmentedFlows || blockHasFloatsOrFragmentedFlows;
            m_cache = cache;
            m_cachedLogicalLeftSelectionOffset = false;
            m_cachedLogicalRightSelectionOffset = false;
        }

        LayoutUnit logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position) const
        {
            ASSERT(m_cache);
            if (m_hasFloatsOrFragmentedFlows || !m_cachedLogicalLeftSelectionOffset) {
                m_cachedLogicalLeftSelectionOffset = true;
                m_logicalLeftSelectionOffset = m_block ? m_block->logicalLeftSelectionOffset(rootBlock, position, *m_cache) : 0_lu;
            } else
                ASSERT(m_logicalLeftSelectionOffset == (m_block ? m_block->logicalLeftSelectionOffset(rootBlock, position, *m_cache) : 0_lu));
            return m_logicalLeftSelectionOffset;
        }

        LayoutUnit logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position) const
        {
            ASSERT(m_cache);
            if (m_hasFloatsOrFragmentedFlows || !m_cachedLogicalRightSelectionOffset) {
                m_cachedLogicalRightSelectionOffset = true;
                m_logicalRightSelectionOffset = m_block ? m_block->logicalRightSelectionOffset(rootBlock, position, *m_cache) : 0_lu;
            } else
                ASSERT(m_logicalRightSelectionOffset == (m_block ? m_block->logicalRightSelectionOffset(rootBlock, position, *m_cache) : 0_lu));
            return m_logicalRightSelectionOffset;
        }

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

    explicit LogicalSelectionOffsetCaches(RenderBlock& rootBlock)
    {
#if ENABLE(TEXT_SELECTION)
        // FIXME: We should either move this assertion to the caller (if applicable) or structure the code
        // such that we can remove this assertion.
        ASSERT(rootBlock.isSelectionRoot());
#endif
        // LogicalSelectionOffsetCaches should not be used on an orphaned tree.
        m_containingBlockForFixedPosition.setBlock(rootBlock.containingBlockForFixedPosition(), nullptr);
        m_containingBlockForAbsolutePosition.setBlock(rootBlock.containingBlockForAbsolutePosition(), nullptr);
        m_containingBlockForInflowPosition.setBlock(rootBlock.containingBlockForObjectInFlow(), nullptr);
    }

    LogicalSelectionOffsetCaches(RenderBlock& block, const LogicalSelectionOffsetCaches& cache)
        : m_containingBlockForFixedPosition(cache.m_containingBlockForFixedPosition)
        , m_containingBlockForAbsolutePosition(cache.m_containingBlockForAbsolutePosition)
    {
        if (block.canContainFixedPositionObjects())
            m_containingBlockForFixedPosition.setBlock(&block, &cache, cache.m_containingBlockForFixedPosition.hasFloatsOrFragmentedFlows());

        if (block.canContainAbsolutelyPositionedObjects() && !block.isRenderInline() && !block.isAnonymousBlock())
            m_containingBlockForAbsolutePosition.setBlock(&block, &cache, cache.m_containingBlockForAbsolutePosition.hasFloatsOrFragmentedFlows());

        m_containingBlockForInflowPosition.setBlock(&block, &cache, cache.m_containingBlockForInflowPosition.hasFloatsOrFragmentedFlows());
    }

    const ContainingBlockInfo& containingBlockInfo(RenderBlock& block) const
    {
        auto position = block.style().position();
        if (position == PositionType::Fixed) {
            ASSERT(block.containingBlock() == m_containingBlockForFixedPosition.block());
            return m_containingBlockForFixedPosition;
        }
        if (position == PositionType::Absolute) {
            ASSERT(block.containingBlock() == m_containingBlockForAbsolutePosition.block());
            return m_containingBlockForAbsolutePosition;
        }
        ASSERT(block.containingBlock() == m_containingBlockForInflowPosition.block());
        return m_containingBlockForInflowPosition;
    }

private:
    ContainingBlockInfo m_containingBlockForFixedPosition;
    ContainingBlockInfo m_containingBlockForAbsolutePosition;
    ContainingBlockInfo m_containingBlockForInflowPosition;
};

} // namespace WebCore
