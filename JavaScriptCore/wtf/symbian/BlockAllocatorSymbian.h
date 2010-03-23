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

#ifndef BlockAllocatorSymbian_h
#define BlockAllocatorSymbian_h

#include <e32cmn.h>
#include <e32std.h>
#include <hal.h>


#define SYMBIAN_PAGESIZE(x) (HAL::Get(HALData::EMemoryPageSize, x));
#define SYMBIAN_FREERAM(x)  (HAL::Get(HALData::EMemoryRAMFree, x));
#define SYMBIAN_ROUNDUPTOMULTIPLE(x, multipleof)    ( (x + multipleof - 1) & ~(multipleof - 1) )

// Set sane defaults if -D<flagname=value> wasn't provided via compiler args
#ifndef JSCCOLLECTOR_VIRTUALMEM_RESERVATION
#if defined(__WINS__) 
    // Emulator has limited virtual address space
    #define JSCCOLLECTOR_VIRTUALMEM_RESERVATION (4*1024*1024)
#else
    // HW has plenty of virtual addresses
    #define JSCCOLLECTOR_VIRTUALMEM_RESERVATION (128*1024*1024)
#endif
#endif

namespace WTF {

/** 
 *  Allocates contiguous region of size blockSize with blockSize-aligned address. 
 *  blockSize must be a multiple of system page size (typically 4K on Symbian/ARM)
 *
 *  @param reservationSize Virtual address range to be reserved upon creation of chunk (bytes).
 *  @param blockSize Size of a single allocation. Returned address will also be blockSize-aligned.
 */
class AlignedBlockAllocator {
    public:
        AlignedBlockAllocator(TUint32 reservationSize, TUint32 blockSize);
        ~AlignedBlockAllocator();
        void destroy();
        void* alloc();
        void free(void* data);
    
    private: 
        RChunk   m_chunk; // Symbian chunk that lets us reserve/commit/decommit
        TUint    m_offset; // offset of first committed region from base 
        TInt     m_pageSize; // cached value of system page size, typically 4K on Symbian
        TUint32  m_reservation;
        TUint32  m_blockSize;  

        // Tracks comitted/decommitted state of a blockSize region
        struct {
            
            TUint32 *bits; // array of bit flags 
            TUint32  numBits; // number of regions to keep track of
            
            bool get(TUint32 n) const
            {
                return !!(bits[n >> 5] & (1 << (n & 0x1F)));
            }
            
            void set(TUint32 n)
            {
                bits[n >> 5] |= (1 << (n & 0x1F));
            }
            
            void clear(TUint32 n)
            {
                bits[n >> 5] &= ~(1 << (n & 0x1F));
            }
            
            void clearAll()
            {
               for (TUint32 i = 0; i < numBits; i++)
                    clear(i);
            }
            
            TInt findFree() const
            {
                for (TUint32 i = 0; i < numBits; i++) {
                    if (!get(i)) 
                        return i;
                }
                return -1;
            }
            
        } m_map;  

};
 
}

#endif // end of BlockAllocatorSymbian_h


