/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "ExecutableAllocator.h"

#if ENABLE(EXECUTABLE_ALLOCATOR_DEMAND) && OS(SYMBIAN)

#include <e32hal.h>
#include <e32std.h>

// Set the page size to 256 Kb to compensate for moving memory model limitation
const size_t MOVING_MEM_PAGE_SIZE = 256 * 1024; 

namespace JSC {

void ExecutableAllocator::intializePageSize()
{
#if CPU(ARMV5_OR_LOWER)
    // The moving memory model (as used in ARMv5 and earlier platforms)
    // on Symbian OS limits the number of chunks for each process to 16. 
    // To mitigate this limitation increase the pagesize to 
    // allocate less of larger chunks.
    ExecutableAllocator::pageSize = MOVING_MEM_PAGE_SIZE;
#else
    TInt page_size;
    UserHal::PageSizeInBytes(page_size);
    ExecutableAllocator::pageSize = page_size;
#endif
}

ExecutablePool::Allocation ExecutablePool::systemAlloc(size_t n)
{
    RChunk* codeChunk = new RChunk();

    TInt errorCode = codeChunk->CreateLocalCode(n, n);

    char* allocation = reinterpret_cast<char*>(codeChunk->Base());
    if (!allocation)
        CRASH();
    ExecutablePool::Allocation alloc = { allocation, n, codeChunk };
    return alloc;
}

void ExecutablePool::systemRelease(const ExecutablePool::Allocation& alloc)
{ 
    alloc.chunk->Close();
    delete alloc.chunk;
}

#if ENABLE(ASSEMBLER_WX_EXCLUSIVE)
#error "ASSEMBLER_WX_EXCLUSIVE not yet suported on this platform."
#endif

}

#endif // HAVE(ASSEMBLER)
