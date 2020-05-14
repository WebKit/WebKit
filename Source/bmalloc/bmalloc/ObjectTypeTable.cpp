/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ObjectTypeTable.h"

#include "VMAllocate.h"

namespace bmalloc {

ObjectTypeTable::Bits sentinelBits { nullptr, 0, 0 };

void ObjectTypeTable::set(UniqueLockHolder&, Chunk* chunk, ObjectType objectType)
{
    unsigned index = convertToIndex(chunk);
    Bits* bits = m_bits;
    if (!(bits->begin() <= index && index < bits->end())) {
        unsigned newBegin = 0;
        unsigned newEnd = 0;
        if (bits == &sentinelBits) {
            // This is initial allocation of ObjectTypeTable. In this case, it could be possible that for the first registration,
            // some VAs are already allocated for a different purpose, and later they will be reused for bmalloc. In that case,
            // soon, we will see a smaller index request than this initial one. We subtract a 128MB offset to the initial newBegin
            // to cover such patterns without extending table too quickly.
            newBegin = std::min<unsigned>(index, index - ObjectTypeTable::Bits::bitCountPerWord * 4);
            newEnd = index + 1;
        } else if (index < bits->begin()) {
            BASSERT(bits->begin());
            BASSERT(bits->end());
            newBegin = std::min<unsigned>(index, bits->begin() - bits->count());
            newEnd = bits->end();
        } else {
            BASSERT(bits->begin());
            BASSERT(bits->end());
            newBegin = bits->begin();
            newEnd = std::max<unsigned>(index + 1, bits->end() + bits->count());
        }
        newBegin = static_cast<unsigned>(roundDownToMultipleOf<size_t>(ObjectTypeTable::Bits::bitCountPerWord, newBegin));
        BASSERT(newEnd > newBegin);

        unsigned count = newEnd - newBegin;
        size_t size = vmSize(sizeof(Bits) + (roundUpToMultipleOf<size_t>(ObjectTypeTable::Bits::bitCountPerWord, count) / 8));
        RELEASE_BASSERT(size <= 0x80000000U); // Too large bitvector, out-of-memory.
        size = roundUpToPowerOfTwo(size);
        newEnd = newBegin + ((size - sizeof(Bits)) / sizeof(ObjectTypeTable::Bits::WordType)) * ObjectTypeTable::Bits::bitCountPerWord;
        BASSERT(newEnd > newBegin);
        void* allocated = vmAllocate(size);
        memset(allocated, 0, size);
        auto* newBits = new (allocated) Bits(bits, newBegin, newEnd);

        memcpy(newBits->wordForIndex(bits->begin()), bits->words(), bits->sizeInBytes());
#if !defined(NDEBUG)
        for (unsigned index = bits->begin(); index < bits->end(); ++index)
            BASSERT(bits->get(index) == newBits->get(index));
#endif
        std::atomic_thread_fence(std::memory_order_seq_cst); // Ensure table gets valid when it is visible to the other threads since ObjectTypeTable::get does not take a lock.
        m_bits = newBits;
        bits = newBits;
    }
    bool value = !!static_cast<std::underlying_type_t<ObjectType>>(objectType);
    BASSERT(static_cast<ObjectType>(value) == objectType);
    bits->set(index, value);
}

} // namespace bmalloc
