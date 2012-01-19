/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "MarkedSpace.h"

#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObject.h"
#include "ScopeChain.h"

namespace JSC {

class Structure;

MarkedSpace::MarkedSpace(Heap* heap)
    : m_waterMark(0)
    , m_nurseryWaterMark(0)
    , m_heap(heap)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        sizeClassFor(cellSize).cellSize = cellSize;

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        sizeClassFor(cellSize).cellSize = cellSize;
}

void MarkedSpace::addBlock(SizeClass& sizeClass, MarkedBlock* block)
{
    ASSERT(!sizeClass.currentBlock);
    ASSERT(!sizeClass.firstFreeCell);

    sizeClass.blockList.append(block);
    sizeClass.currentBlock = block;
    sizeClass.firstFreeCell = block->sweep(MarkedBlock::SweepToFreeList);
}

void MarkedSpace::removeBlock(MarkedBlock* block)
{
    SizeClass& sizeClass = sizeClassFor(block->cellSize());
    if (sizeClass.currentBlock == block)
        sizeClass.currentBlock = 0;
    sizeClass.blockList.remove(block);
}

void MarkedSpace::resetAllocator()
{
    m_waterMark = 0;
    m_nurseryWaterMark = 0;

    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        sizeClassFor(cellSize).resetAllocator();

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        sizeClassFor(cellSize).resetAllocator();
}

void MarkedSpace::canonicalizeCellLivenessData()
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        sizeClassFor(cellSize).zapFreeList();

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        sizeClassFor(cellSize).zapFreeList();
}

} // namespace JSC
