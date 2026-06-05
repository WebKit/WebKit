/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSExportMacros.h>
#include <wtf/Compiler.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>

#include "MarkedBlock.h"
#include "wtf/StringPrintStream.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class HeapCell;

class FreeList {
public:
    FreeList(unsigned cellSize);
    ~FreeList();
    
    void NODELETE clear();
    
    JS_EXPORT_PRIVATE void initialize(MarkedBlock::Handle* block, const WTF::BitSet<MarkedBlock::atomsPerBlock>& free, unsigned startIndex, unsigned bytes);
    JS_EXPORT_PRIVATE void initializeEmpty(MarkedBlock::Handle* block, char* intervalStart, char* intervalEnd);
    
    bool allocationWillFail() { return !allocationWillSucceed(); }
    bool allocationWillSucceed() { return m_intervalStart < m_intervalEnd || findNextInterval(); }

    unsigned currentAllocationIndex() {
        if (m_intervalStart >= m_intervalEnd)
            findNextInterval();
        return m_block->block().candidateAtomNumber(m_intervalStart);
    }
    
    template<typename Func>
    HeapCell* allocateWithCellSize(const Func& slowPath, size_t cellSize);
    
    const WTF::BitSet<MarkedBlock::atomsPerBlock>& live() { return m_live; }
    template<typename Func>
    void forEachRemaining(const Func&);
    
    unsigned originalSize() const { return m_originalSize; }

    static constexpr ptrdiff_t offsetOfIntervalStart() { return OBJECT_OFFSETOF(FreeList, m_intervalStart); }
    static constexpr ptrdiff_t offsetOfIntervalEnd() { return OBJECT_OFFSETOF(FreeList, m_intervalEnd); }
    static constexpr ptrdiff_t offsetOfOriginalSize() { return OBJECT_OFFSETOF(FreeList, m_originalSize); }
    static constexpr ptrdiff_t offsetOfCellSize() { return OBJECT_OFFSETOF(FreeList, m_cellSize); }
    static constexpr ptrdiff_t offsetOfNextCachedInterval() { return OBJECT_OFFSETOF(FreeList, m_nextCachedInterval); }
    static constexpr ptrdiff_t offsetOfCachedIntervals() { return OBJECT_OFFSETOF(FreeList, m_cachedIntervals); }
    
    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    unsigned cellSize() const { return m_cellSize; }
    unsigned atomsPerCell() const {
        ASSERT(m_cellSize % MarkedBlock::atomSize == 0);
        static_assert(MarkedBlock::atomSize);
        return m_cellSize / MarkedBlock::atomSize;
    }
    
// private:
    template<bool value> // which value to find
    ALWAYS_INLINE bool findFreeCellFast() {
        uint64_t ones = ~((uint64_t)0x00);
        unsigned atomsPerCell = m_cellSize >> 4;
        static_assert(MarkedBlock::atomsPerBlock == 1024);
        uint64_t* bits = std::bit_cast<uint64_t*>(&m_live);
        uint64_t* bitmaskBits = std::bit_cast<uint64_t*>(&m_bitmask);
        ASSERT(bits == &m_live.storage()[0]);
        // unsigned startIndexOriginal = m_startIndex;
        ASSERT((uint32_t*)bits == std::bit_cast<uint32_t*>(&m_live));
        // WTFLogAlways("afryer_findFreeCell %u : %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx\n", m_startIndex, bits[0], bits[1], bits[2], bits[3], bits[4], bits[5], bits[6], bits[7], bits[8], bits[9], bits[10], bits[11], bits[12], bits[13], bits[14], bits[15]);
        uint64_t startIndex = m_startIndex;
        while (startIndex < MarkedBlock::atomsPerBlock) {
            unsigned startIndexWordIndex = startIndex >> 6;
            unsigned startIndexBitIndex = startIndex & 0x3F;
            uint64_t bitmask = bitmaskBits[startIndexWordIndex];
            uint64_t word = bits[startIndexWordIndex];
            uint64_t searchResults = (value ? word : ~word) & bitmask & (ones << startIndexBitIndex); // has a 1 on each bit we're looking for
            if (searchResults) {
                // find in word
                // WTFLogAlways("afryer_findFreeCell_ctzll %d %llx %llx %llx %llx %llu\n", value, word, m_bitmask, bitmask, searchResults, (uint64_t)__builtin_ctzll(searchResults));
                ASSERT(__builtin_ctzll(searchResults) >= (int)startIndexBitIndex);
                startIndex += __builtin_ctzll(searchResults) - startIndexBitIndex;
                // WTFLogAlways("afryer_findFreeCell %d %u %u %u %llx\n", value, startIndexOriginal, startIndex, atomsPerCell, bitmask);
                m_startIndex = startIndex;
                return true;
            }
            // m_startIndex += atomsPerCell; // slow
            unsigned skipSize = ((atomsPerCell + (64 - startIndexBitIndex) - 1) / atomsPerCell) * atomsPerCell;
            // WTFLogAlways("afryer_findFreeCell_skip %u %u %u\n", startIndex, atomsPerCell, skipSize);
            startIndex += skipSize;
        }
        m_startIndex = startIndex;
        return false;
    }

    ALWAYS_INLINE bool findNextIntervalFast(char*& intervalStart, char*& intervalEnd) {
        if (m_startIndex >= MarkedBlock::atomsPerBlock)
            return false;
        // ASSERT(m_intervalStart >= m_intervalEnd); // you should only advance to the next interval if the current interval is depleted
        if (findFreeCellFast<false>()) {
            intervalStart = (char*)m_block->atomAt(m_startIndex);
            bool r = findFreeCellFast<true>();
            (void)r;
            ASSERT(r || m_startIndex == 1024);
            ASSERT(m_startIndex <= 1024);
            intervalEnd = (char*)m_block->atomAt(m_startIndex);
            // for (char* p = m_intervalStart; p < m_intervalEnd; p += m_cellSize) {
            //     if (m_live.get(m_block->block().candidateAtomNumber(p)))
            //         WTFLogAlways("afryer_findNextIntervalFast_failed %lu\n", m_block->block().candidateAtomNumber(p));
            // }
            // if (m_startIndex < 1024 && !m_live.get(m_block->block().candidateAtomNumber(m_intervalEnd)))
            //     WTFLogAlways("afryer_findNextIntervalFast_failed2 %lu\n", m_block->block().candidateAtomNumber(m_intervalEnd));
            WTFLogAlways("afryer_findNextIntervalFast %p %p %p\n", this, intervalStart, intervalEnd);
            return true;
        }
        return false;
    }

    ALWAYS_INLINE bool findNextInterval() {
        // I'm changing this to actually queue up the next (up to) 16 intervals in m_cachedIntervals
        // if we don't already have an interval cached.
        if (m_nextCachedInterval >= &m_cachedIntervals[0]) {
            // std::pair<char*, char*> interval = *(std::bit_cast<std::pair<char*, char*>*>(m_nextCachedInterval));
            m_intervalStart = m_nextCachedInterval->first;
            m_intervalEnd = m_nextCachedInterval->second;
            // m_nextCachedInterval -= sizeof(std::pair<char*, char*>);
            m_nextCachedInterval--;
            WTFLogAlways("afryer_findNextIntervalFast1 %p %p %p\n", this, m_intervalStart, m_intervalEnd);
            return true;
        }
        m_nextCachedInterval = &m_cachedIntervals[0];
        m_nextCachedInterval--; // this means no cached intervals
        // We reverse the order of results here so that we allocate in the order the intervals are in the block.
        // This makes things easier for stopAllocating and is a bit more intuitive IMHO.
        std::array<std::pair<char*, char*>, 16> intervalStack;
        unsigned i = 0;
        while (i < 16 && findNextIntervalFast(intervalStack[i].first, intervalStack[i].second))
            i++;
        bool result = !!i;
        while (i) {
            i--;
            m_nextCachedInterval++;
            *m_nextCachedInterval = intervalStack[i];
        }
        if (result) {
            m_intervalStart = m_nextCachedInterval->first;
            m_intervalEnd = m_nextCachedInterval->second;
            // m_nextCachedInterval -= sizeof(std::pair<char*, char*>);
            m_nextCachedInterval--;
            WTFLogAlways("afryer_findNextIntervalFast2 %p %p %p\n", this, m_intervalStart, m_intervalEnd);
        }
        return result;
        // // Todo: optimize this using word iteration and bitmask
        // ASSERT(m_block);
        // unsigned stride = cellSize >> 4; // cellSize / MarkedBlock::atomSize
        // for (unsigned i = 0; i < MarkedBlock::atomsPerBlock; i += stride) {
        //     if (!m_live.get(i)) {
        //         unsigned j;
        //         for (j = i + 1; j < MarkedBlock::atomsPerBlock; j += stride) {
        //             if (m_live.get(j))
        //                 break;
        //         }
        //         m_startIndex = i;
        //         m_intervalStart = (char*)m_block->atomAt(m_startIndex);
        //         // m_endIndex = j;
        //         m_intervalEnd = (char*)m_block->atomAt(j);
        //         return true;
        //     }
        // }
        // return false;
    }

    unsigned countIntervals() {
        bool inInterval = false;
        unsigned result = 0;
        m_bitmask.forEachSetBit([&](unsigned i) {
            if (!m_live.get(i) != inInterval) {
                inInterval = !inInterval;
                result++;
            }
        });
        return result;
    }

    ALWAYS_INLINE WTF::BitSet<MarkedBlock::atomsPerBlock> allocatedBits(unsigned start) {
        // unsigned startOriginal = start;
        unsigned atomsPerCell = m_cellSize >> 4;
        unsigned endIndex = currentAllocationIndex() - 1; // -1 makes inclusive
        unsigned endWord = endIndex >> 6;
        unsigned endBit = endIndex & 0x3F;
        WTF::BitSet<MarkedBlock::atomsPerBlock> result;
        uint64_t* resultBits = std::bit_cast<uint64_t*>(&result);
        while (start < MarkedBlock::atomsPerBlock) {
            unsigned startWord = start >> 6;
            unsigned startBit = start & 0x3F;
            uint64_t bitmask = std::bit_cast<uint64_t*>(&m_bitmask)[startWord];
            resultBits[startWord] = bitmask;
            if (startWord >= endWord) { // should be exactly equal
                resultBits[startWord] &= (~((uint64_t)0x00)) >> (63 - endBit);
                break;
            }
            unsigned skipSize = ((atomsPerCell + (64 - startBit) - 1) / atomsPerCell) * atomsPerCell;
            start += skipSize;
        }
        // WTFLogAlways("afryer_allocatedBits %u %u %u %u\n", endWord, endBit & 0x3F, startOriginal >> 6, startOriginal & 0x3F);
        return result;
    }

/*

I think I should actually store m_free and clear the bits as we use them so that finding the next interval becomes:
word;
// i is a member variable
while (true) {
    if (!(i < 16)) {
        return false;
    }
    word = m_freeBits[i];
    if (word)
        break;
    i++;
}
m_start = get_ptr(i << 6 + ctz(word));
while (true) {
    if (!(i < 16)) {
        m_end = get_ptr(i); // i == 16
        return true;
    }
    word = m_freeBits[i] ^ m_bitmaskBits[i];
    if (word)
        break;
    m_freeBits[i] = 0x00;
    i++;
}
bitInd = ctz(word);
m_end = get_ptr(i << 6 + bitInd);
m_freeBits[i] &= ~((uint64_t)0x00) << bitInd;

One benefit of the above is that stopAllocating can compute newlyAllocated as:
ASSERT(m_bitmask.subsumes(m_freeBits));
m_bitmask ^ m_freeBits


Actually, I think I should instrument to find the distribution of the number of intervals in a block.
I wouldn't be surprised if it is almost always very low.
In that case, we could just store a std::array that is sufficient for most cases and then getting the next interval becomes:
if (i >= l)
    slowPath();
start, end = arr[i];
i++;
...where arr is a pointer to either the std::array in the FreeList or to a std::array that is heap allocated...
Actually, the slow case could just call into c++ that looks at m_free.
The std::array only needs to store 2 values < 1024, so 2 `uint16_t`s for each interval.
We could easily stick 32 intervals into 16 `uint64_t`s, which I bet covers close to 100% of the cases in real life.
Actually, if l is small in practice than it's probably faster to store full pointers in arr, so each entry is 128 bytes.


I don't really think there's a benefit to storing m_free instead of m_live, although maybe there is slightly.
What actually makes things complicated is that a word in the BitSet might contain several intervals.
This means you need to do some bit operations to occlude any interval that's already used.
You can either write this to the BitSet, or filter when reading.
I think it's probably a bit better to write this to the BitSet.

*/

    ALWAYS_INLINE WTF::BitSet<MarkedBlock::atomsPerBlock> nonRemainingBits() {
        unsigned current = currentAllocationIndex() - 1;
        WTF::BitSet<MarkedBlock::atomsPerBlock> result = m_bitmask;
        if (current >= 1023)
            return result;
        unsigned currentWord = current >> 6;
        unsigned currentBit = current & 0x3F;
        uint64_t* resultBits = std::bit_cast<uint64_t*>(&result);
        uint64_t* liveBits = std::bit_cast<uint64_t*>(&m_live);
        uint64_t ones = ~((uint64_t)0x00);
        resultBits[currentWord] &= ones >> (64 - currentBit);
        resultBits[currentWord] |= (ones << currentBit) & liveBits[currentWord];
        for (unsigned wordIndex = currentWord + 1; wordIndex < 16; wordIndex++) {
            resultBits[wordIndex] = liveBits[wordIndex];
        }
        // WTFLogAlways("afryer_nonRemainingBits %u %u\n", currentWord, currentBit);
        return result;
    }

    const WTF::BitSet<MarkedBlock::atomsPerBlock>& bitmask() {
        return m_bitmask;
    }

    ALWAYS_INLINE WTF::BitSet<MarkedBlock::atomsPerBlock> allocatedBitsIncludingPreviouslyMarked() {
        // first clean up m_live because it might have stale data from initializeEmpty
        if (m_intervalEnd >= (char*)m_block->atomAt(MarkedBlock::atomsPerBlock))
            m_live.clearAll();

        WTF::BitSet<MarkedBlock::atomsPerBlock> result;
        uint64_t* resultBits = std::bit_cast<uint64_t*>(&result);
        uint64_t* liveBits = std::bit_cast<uint64_t*>(&m_live);
        uint64_t* maskBits = std::bit_cast<uint64_t*>(&m_bitmask);
        constexpr uint64_t ones = ~((uint64_t)0x00);

        unsigned lastAllocated = currentAllocationIndex() - 1;
        unsigned lastAllocatedWord = lastAllocated >> 6;
        unsigned lastAllocatedBit = lastAllocated & 0x3F;
        for (unsigned i = 0; i < 16; i++) {
            bool allocated = i < lastAllocatedWord;
            resultBits[i] = liveBits[i] | (allocated * maskBits[i]);
        }
        resultBits[lastAllocatedWord] |= (ones >> (63 - lastAllocatedBit)) & maskBits[lastAllocatedWord];
        return result;
    }
    
    char* m_intervalStart { nullptr };
    char* m_intervalEnd { nullptr };
    unsigned m_cellSize { 0 };
    unsigned m_originalSize { 0 };
    std::pair<char*, char*>* m_nextCachedInterval { nullptr };
    std::array<std::pair<char*, char*>, 16> m_cachedIntervals;

    MarkedBlock::Handle* m_block { nullptr };
    WTF::BitSet<MarkedBlock::atomsPerBlock> m_bitmask;
    unsigned m_startIndex { MarkedBlock::atomsPerBlock }; // could be just uin16_t // FreeList should fail allocation until it is initialized

    WTF::BitSet<MarkedBlock::atomsPerBlock> m_live;
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
