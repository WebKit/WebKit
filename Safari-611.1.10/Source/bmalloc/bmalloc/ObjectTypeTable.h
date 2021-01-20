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

#pragma once

#include "Mutex.h"
#include "ObjectType.h"
#include "Sizes.h"

namespace bmalloc {

class Chunk;

// Querying ObjectType for Chunk without locking.
class ObjectTypeTable {
public:
    ObjectTypeTable();

    static constexpr unsigned shiftAmount = 20;
    static_assert((1ULL << shiftAmount) == chunkSize);
    static_assert((BOS_EFFECTIVE_ADDRESS_WIDTH - shiftAmount) <= 32);

    class Bits;

    ObjectType get(Chunk*);
    void set(UniqueLockHolder&, Chunk*, ObjectType);

private:
    static unsigned convertToIndex(Chunk* chunk)
    {
        uintptr_t address = reinterpret_cast<uintptr_t>(chunk);
        BASSERT(!(address & (~chunkMask)));
        return static_cast<unsigned>(address >> shiftAmount);
    }

    Bits* m_bits;
};

class ObjectTypeTable::Bits {
public:
    using WordType = unsigned;
    static constexpr unsigned bitCountPerWord = sizeof(WordType) * 8;
    static constexpr WordType one = 1;
    constexpr Bits(Bits* previous, unsigned begin, unsigned end)
        : m_previous(previous)
        , m_begin(begin)
        , m_end(end)
    {
    }

    bool get(unsigned index);
    void set(unsigned index, bool);

    Bits* previous() const { return m_previous; }
    unsigned begin() const { return m_begin; }
    unsigned end() const { return m_end; }
    unsigned count() const { return m_end - m_begin; }
    unsigned sizeInBytes() const { return count() / 8; }

    const WordType* words() const { return const_cast<Bits*>(this)->words(); }
    WordType* words() { return reinterpret_cast<WordType*>(reinterpret_cast<uintptr_t>(this) + sizeof(Bits)); }

    WordType* wordForIndex(unsigned);

private:
    Bits* m_previous { nullptr }; // Keeping the previous Bits* just to suppress Leaks warnings.
    unsigned m_begin { 0 };
    unsigned m_end { 0 };
};
static_assert(!(sizeof(ObjectTypeTable::Bits) % sizeof(ObjectTypeTable::Bits::WordType)));

extern BEXPORT ObjectTypeTable::Bits sentinelBits;

inline ObjectTypeTable::ObjectTypeTable()
    : m_bits(&sentinelBits)
{
}

inline ObjectType ObjectTypeTable::get(Chunk* chunk)
{
    Bits* bits = m_bits;
    unsigned index = convertToIndex(chunk);
    BASSERT(bits);
    if (bits->begin() <= index && index < bits->end())
        return static_cast<ObjectType>(bits->get(index));
    return { };
}

inline bool ObjectTypeTable::Bits::get(unsigned index)
{
    unsigned n = index - begin();
    return words()[n / bitCountPerWord] & (one << (n % bitCountPerWord));
}

inline void ObjectTypeTable::Bits::set(unsigned index, bool value)
{
    unsigned n = index - begin();
    if (value)
        words()[n / bitCountPerWord] |= (one << (n % bitCountPerWord));
    else
        words()[n / bitCountPerWord] &= ~(one << (n % bitCountPerWord));
}

inline ObjectTypeTable::Bits::WordType* ObjectTypeTable::Bits::wordForIndex(unsigned index)
{
    unsigned n = index - begin();
    return &words()[n / bitCountPerWord];
}

} // namespace bmalloc
