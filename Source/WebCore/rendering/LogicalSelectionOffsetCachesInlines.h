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

#include "LogicalSelectionOffsetCaches.h"
#include "RenderBlock.h"
#include "RenderElementStyleInlines.h"

namespace WebCore {

void LogicalSelectionOffsetCaches::ContainingBlockInfo::setBlock(RenderBlock* block, const LogicalSelectionOffsetCaches* cache, bool parentCacheHasFloatsOrFragmentedFlows)
{
    m_block = block;
    bool blockHasFloatsOrFragmentedFlows = block && (block->containsFloats() || block->enclosingFragmentedFlow());
    m_hasFloatsOrFragmentedFlows = parentCacheHasFloatsOrFragmentedFlows || m_hasFloatsOrFragmentedFlows || blockHasFloatsOrFragmentedFlows;
    m_cache = cache;
    m_cachedLogicalLeftSelectionOffset = false;
    m_cachedLogicalRightSelectionOffset = false;
}

LayoutUnit LogicalSelectionOffsetCaches::ContainingBlockInfo::logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position) const
{
    ASSERT(m_cache);
    if (m_hasFloatsOrFragmentedFlows || !m_cachedLogicalLeftSelectionOffset) {
        m_cachedLogicalLeftSelectionOffset = true;
        m_logicalLeftSelectionOffset = m_block ? protect(m_block)->logicalLeftSelectionOffset(rootBlock, position, *m_cache) : 0_lu;
    } else
        ASSERT(m_logicalLeftSelectionOffset == (m_block ? m_block->logicalLeftSelectionOffset(rootBlock, position, *m_cache) : 0_lu));
    return m_logicalLeftSelectionOffset;
}

LayoutUnit LogicalSelectionOffsetCaches::ContainingBlockInfo::logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position) const
{
    ASSERT(m_cache);
    if (m_hasFloatsOrFragmentedFlows || !m_cachedLogicalRightSelectionOffset) {
        m_cachedLogicalRightSelectionOffset = true;
        m_logicalRightSelectionOffset = m_block ? protect(m_block)->logicalRightSelectionOffset(rootBlock, position, *m_cache) : 0_lu;
    } else
        ASSERT(m_logicalRightSelectionOffset == (m_block ? m_block->logicalRightSelectionOffset(rootBlock, position, *m_cache) : 0_lu));
    return m_logicalRightSelectionOffset;
}

LogicalSelectionOffsetCaches::LogicalSelectionOffsetCaches(RenderBlock& block, const LogicalSelectionOffsetCaches& cache)
{
    m_containingBlockForInflowPosition.setBlock(&block, &cache, cache.m_containingBlockForInflowPosition.hasFloatsOrFragmentedFlows());
}

auto LogicalSelectionOffsetCaches::containingBlockInfo(RenderBlock& block) const -> const ContainingBlockInfo&
{
    // Selection gaps skip out-of-flow boxes and a positioned block is a selection root, so only in-flow blocks below the root get here.
    ASSERT(!block.isOutOfFlowPositioned());
    ASSERT_UNUSED(block, block.containingBlock() == m_containingBlockForInflowPosition.block());
    return m_containingBlockForInflowPosition;
}

} // namespace WebCore
