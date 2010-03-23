/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if OS(SYMBIAN)

#include "BlockAllocatorSymbian.h"


namespace WTF {

/** Efficiently allocates blocks of size blockSize with blockSize alignment. 
 * Primarly designed for JSC Collector's needs. 
 * Not thread-safe.    
 */
AlignedBlockAllocator::AlignedBlockAllocator(TUint32 reservationSize, TUint32 blockSize )
    : m_reservation(reservationSize), 
      m_blockSize(blockSize)
{

     // Get system's page size value.
     SYMBIAN_PAGESIZE(m_pageSize); 
     
     // We only accept multiples of system page size for both initial reservation and the alignment/block size
     m_reservation = SYMBIAN_ROUNDUPTOMULTIPLE(m_reservation, m_pageSize);
     __ASSERT_ALWAYS(SYMBIAN_ROUNDUPTOMULTIPLE(m_blockSize, m_pageSize), User::Panic(_L("AlignedBlockAllocator1"), KErrArgument));
     
     // Calculate max. bit flags we need to carve a reservationSize range into blockSize-sized blocks
     m_map.numBits = m_reservation / m_blockSize;   
     const TUint32 bitsPerWord = 8*sizeof(TUint32); 
     const TUint32 numWords = (m_map.numBits + bitsPerWord -1) / bitsPerWord; 
   
     m_map.bits = new TUint32[numWords];
     __ASSERT_ALWAYS(m_map.bits, User::Panic(_L("AlignedBlockAllocator2"), KErrNoMemory));
     m_map.clearAll();
     
     // Open a Symbian RChunk, and reserve requested virtual address range   
     // Any thread in this process can operate this rchunk due to EOwnerProcess access rights. 
     TInt ret = m_chunk.CreateDisconnectedLocal(0 , 0, (TInt)m_reservation , EOwnerProcess);  
     if (ret != KErrNone) 
         User::Panic(_L("AlignedBlockAllocator3"), ret);
       
     // This is the offset to m_chunk.Base() required to make it m_blockSize-aligned
     m_offset = SYMBIAN_ROUNDUPTOMULTIPLE(TUint32(m_chunk.Base()), m_blockSize) - TUint(m_chunk.Base()); 

}

void* AlignedBlockAllocator::alloc()
{

    TInt  freeRam = 0; 
    void* address = 0;
    
    // Look up first free slot in bit map
    const TInt freeIdx = m_map.findFree();
        
    // Pseudo OOM: We ate up the address space we reserved..
    // ..even though the device may have free RAM left
    if (freeIdx < 0)
        return 0;
        
    TInt ret = m_chunk.Commit(m_offset + (m_blockSize * freeIdx), m_blockSize);
    if (ret != KErrNone)  
        return 0; // True OOM: Device didn't have physical RAM to spare
        
    // Updated bit to mark region as in use. 
    m_map.set(freeIdx); 
    
    // Calculate address of committed region (block)
    address = (void*)( (m_chunk.Base() + m_offset) + (TUint)(m_blockSize * freeIdx) );
    
    return address;
}

void AlignedBlockAllocator::free(void* block)
{
    // Calculate index of block to be freed
    TInt idx = TUint(static_cast<TUint8*>(block) - m_chunk.Base() - m_offset) / m_blockSize;
    
    __ASSERT_DEBUG(idx >= 0 && idx < m_map.numBits, User::Panic(_L("AlignedBlockAllocator4"), KErrCorrupt)); // valid index check
    __ASSERT_DEBUG(m_map.get(idx), User::Panic(_L("AlignedBlockAllocator5"), KErrCorrupt)); // in-use flag check    
    
    // Return committed region to system RAM pool (the physical RAM becomes usable by others)
    TInt ret = m_chunk.Decommit(m_offset + m_blockSize * idx, m_blockSize);
            
    // mark this available again
    m_map.clear(idx); 
}

void AlignedBlockAllocator::destroy() 
{
    // release everything!
    m_chunk.Decommit(0, m_chunk.MaxSize());
    m_map.clearAll();
}

AlignedBlockAllocator::~AlignedBlockAllocator()
{
    destroy();
    m_chunk.Close();
    delete [] m_map.bits;
}

} // end of namespace

#endif // SYMBIAN
