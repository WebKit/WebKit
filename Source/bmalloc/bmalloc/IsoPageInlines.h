/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CryptoRandom.h"
#include "IsoDirectory.h"
#include "IsoPage.h"
#include "StdLibExtras.h"
#include "VMAllocate.h"

namespace bmalloc {

template<typename Config>
IsoPage<Config>* IsoPage<Config>::tryCreate(IsoDirectoryBase<Config>& directory, unsigned index)
{
    void* memory = allocatePageMemory();
    if (!memory)
        return nullptr;
    
    return new (memory) IsoPage(directory, index);
}

template<typename Config>
IsoPage<Config>::IsoPage(IsoDirectoryBase<Config>& directory, unsigned index)
    : m_directory(directory)
    , m_index(index)
{
    memset(m_allocBits, 0, sizeof(m_allocBits));
}

template<typename Config>
IsoPage<Config>* IsoPage<Config>::pageFor(void* ptr)
{
    return reinterpret_cast<IsoPage<Config>*>(reinterpret_cast<uintptr_t>(ptr) & ~(pageSize - 1));
}

template<typename Config>
void IsoPage<Config>::free(void* passedPtr)
{
    unsigned offset = static_cast<char*>(passedPtr) - reinterpret_cast<char*>(this);
    unsigned index = offset / Config::objectSize;
    
    if (!m_eligibilityHasBeenNoted) {
        m_eligibilityTrigger.didBecome(*this);
        m_eligibilityHasBeenNoted = true;
    }
    
    unsigned wordIndex = index / 32;
    unsigned bitIndex = index % 32;
    
    unsigned newWord = m_allocBits[wordIndex] &= ~(1 << bitIndex);
    if (!newWord) {
        if (!--m_numNonEmptyWords)
            m_emptyTrigger.didBecome(*this);
    }
}

template<typename Config>
FreeList IsoPage<Config>::startAllocating()
{
    static constexpr bool verbose = false;
    
    if (verbose)
        fprintf(stderr, "%p: starting allocation.\n", this);
    
    RELEASE_BASSERT(!m_isInUseForAllocation);
    m_isInUseForAllocation = true;
    m_eligibilityHasBeenNoted = false;
    
    FreeList result;
    if (!m_numNonEmptyWords) {
        if (verbose)
            fprintf(stderr, "%p: preparing to bump.\n", this);

        char* payloadEnd = reinterpret_cast<char*>(this) + numObjects * Config::objectSize;
        result.initializeBump(payloadEnd, (numObjects - indexOfFirstObject()) * Config::objectSize);
        
        unsigned begin = indexOfFirstObject();
        unsigned end = numObjects;
        
        unsigned beginWord = begin >> 5;
        unsigned endWord = end >> 5;
        
        if (verbose) {
            fprintf(stderr, "begin = %u\n", begin);
            fprintf(stderr, "end = %u\n", end);
            fprintf(stderr, "beginWord = %u\n", beginWord);
            fprintf(stderr, "endWord = %u\n", endWord);
        }
        
        auto setSpan = [&] (unsigned word, unsigned begin, unsigned end) -> unsigned {
            for (unsigned i = begin; i < end; ++i)
                word |= (1 << (i & 31));
            return word;
        };
        
        if (beginWord == endWord) {
            m_allocBits[beginWord] = setSpan(m_allocBits[beginWord], begin, end);
            m_numNonEmptyWords = 1;
        } else {
            unsigned endBeginSlop = (begin + 31) & ~31;
            unsigned beginEndSlop = end & ~31;
            
            if (verbose) {
                fprintf(stderr, "endBeginSlop = %u\n", endBeginSlop);
                fprintf(stderr, "beginEndSlop = %u\n", beginEndSlop);
            }
            
            m_allocBits[beginWord] = setSpan(m_allocBits[beginWord], begin, endBeginSlop);
            if (verbose)
                fprintf(stderr, "m_allocBits[beginWord] = %u\n", m_allocBits[beginWord]);
            if (end > beginEndSlop) {
                m_allocBits[endWord] = setSpan(m_allocBits[endWord], beginEndSlop, end);
                if (verbose)
                    fprintf(stderr, "m_allocBits[endWord] = %u\n", m_allocBits[endWord]);
            }
            
            unsigned beginWordContiguous = endBeginSlop / 32;
            unsigned endWordContiguous = beginEndSlop / 32;
            if (verbose) {
                fprintf(stderr, "beginWordContiguous = %u\n", beginWordContiguous);
                fprintf(stderr, "endWordContiguous = %u\n", endWordContiguous);
            }
            
            for (size_t i = beginWordContiguous; i < endWordContiguous; ++i)
                m_allocBits[i] = UINT_MAX;
            m_numNonEmptyWords = endWordContiguous - beginWordContiguous +
                (endBeginSlop > begin) + (end > beginEndSlop);
        }
        
        static constexpr bool verify = false;
        if (verify) {
            for (unsigned index = 0; index < indexOfFirstObject(); ++index) {
                if (!(m_allocBits[index >> 5] & (1 << (index & 31))))
                    continue;
                fprintf(stderr, "Bit is set even though it should not be: %u\n", index);
                BCRASH();
            }
            for (unsigned index = indexOfFirstObject(); index < numObjects; ++index) {
                if (m_allocBits[index >> 5] & (1 << (index & 31)))
                    continue;
                fprintf(stderr, "Bit is not set even though it should be: %u\n", index);
                fprintf(stderr, "Word contents: %u\n", m_allocBits[index >> 5]);
                fprintf(stderr, "Mask: %u\n", 1 << (index & 31));
                BCRASH();
            }
        }
        
        return result;
    }
    
    uintptr_t secret;
    cryptoRandom(&secret, sizeof(secret));
    FreeCell* head = nullptr;
    unsigned bytes = 0;
    
    for (unsigned index = indexOfFirstObject(); index < numObjects; ++index) {
        unsigned wordIndex = index >> 5;
        unsigned word = m_allocBits[wordIndex];
        unsigned bitMask = 1 << (index & 31);
        if (word & bitMask)
            continue;
        if (!word)
            m_numNonEmptyWords++;
        m_allocBits[wordIndex] = word | bitMask;
        char* cellByte = reinterpret_cast<char*>(this) + index * Config::objectSize;
        if (verbose)
            fprintf(stderr, "%p: putting %p on free list.\n", this, cellByte);
        FreeCell* cell = bitwise_cast<FreeCell*>(cellByte);
        cell->setNext(head, secret);
        head = cell;
        bytes += Config::objectSize;
    }
    
    result.initializeList(head, secret, bytes);
    return result;
}

template<typename Config>
void IsoPage<Config>::stopAllocating(FreeList freeList)
{
    static constexpr bool verbose = false;
    
    if (verbose)
        fprintf(stderr, "%p: stopping allocation.\n", this);
    
    freeList.forEach<Config>(
        [&] (void* ptr) {
            free(ptr);
        });

    RELEASE_BASSERT(m_isInUseForAllocation);
    m_isInUseForAllocation = false;

    m_eligibilityTrigger.handleDeferral(*this);
    m_emptyTrigger.handleDeferral(*this);
}

template<typename Config>
template<typename Func>
void IsoPage<Config>::forEachLiveObject(const Func& func)
{
    for (unsigned wordIndex = 0; wordIndex < bitsArrayLength(numObjects); ++wordIndex) {
        unsigned word = m_allocBits[wordIndex];
        if (!word)
            continue;
        unsigned firstBitIndex = wordIndex * 32;
        char* cellByte = reinterpret_cast<char*>(this) + firstBitIndex * Config::objectSize;
        for (unsigned bitIndex = 0; bitIndex < 32; ++bitIndex) {
            if (word & 1)
                func(static_cast<void*>(cellByte));
            word >>= 1;
            cellByte += Config::objectSize;
        }
    }
}

template<typename Config>
IsoHeapImpl<Config>& IsoPage<Config>::heap()
{
    return m_directory.heap();
}

} // namespace bmalloc

