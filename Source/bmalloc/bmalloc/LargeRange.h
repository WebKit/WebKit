/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "BAssert.h"
#include "Range.h"

#if !BUSE(LIBPAS)

namespace bmalloc {

class LargeRange : public Range {
public:
    LargeRange()
        : Range()
        , m_startPhysicalSize(0)
        , m_totalPhysicalSize(0)
        , m_physicalEnd(begin())
        , m_isEligible(true)
        , m_usedSinceLastScavenge(false)
    {
    }

    LargeRange(const Range& other, size_t startPhysicalSize, size_t totalPhysicalSize, void* physicalEnd)
        : Range(other)
        , m_startPhysicalSize(startPhysicalSize)
        , m_totalPhysicalSize(totalPhysicalSize)
        , m_physicalEnd(static_cast<char*>(physicalEnd))
        , m_isEligible(true)
        , m_usedSinceLastScavenge(false)
    {
        BASSERT(this->size() >= this->totalPhysicalSize());
        BASSERT(this->totalPhysicalSize() >= this->startPhysicalSize());
    }

    LargeRange(void* begin, size_t size, size_t startPhysicalSize, size_t totalPhysicalSize, void* physicalEnd, bool usedSinceLastScavenge = false)
        : Range(begin, size)
        , m_startPhysicalSize(startPhysicalSize)
        , m_totalPhysicalSize(totalPhysicalSize)
        , m_physicalEnd(static_cast<char*>(physicalEnd))
        , m_isEligible(true)
        , m_usedSinceLastScavenge(usedSinceLastScavenge)
    {
        BASSERT(this->size() >= this->totalPhysicalSize());
        BASSERT(this->totalPhysicalSize() >= this->startPhysicalSize());
    }

    // Returns a lower bound on physical size at the start of the range. Ranges that
    // span non-physical fragments use this number to remember the physical size of
    // the first fragment.
    size_t startPhysicalSize() const { return m_startPhysicalSize; }
    void setStartPhysicalSize(size_t startPhysicalSize) { m_startPhysicalSize = startPhysicalSize; }

    // This is accurate in the sense that if you take a range A and split it N ways
    // and sum totalPhysicalSize over each of the N splits, you'll end up with A's
    // totalPhysicalSize. This means if you take a LargeRange out of a LargeMap, split it,
    // then insert the subsequent two ranges back into the LargeMap, the sum of the
    // totalPhysicalSize of each LargeRange in the LargeMap will stay constant. This
    // property is not true of startPhysicalSize. This invariant about totalPhysicalSize
    // is good enough to get an accurate footprint estimate for memory used in bmalloc.
    // The reason this is just an estimate is that splitting LargeRanges may lead to this
    // number being rebalanced in arbitrary ways between the two resulting ranges. This
    // is why the footprint is just an estimate. In practice, this arbitrary rebalance
    // doesn't really affect accuracy.
    size_t totalPhysicalSize() const { return m_totalPhysicalSize; }
    void setTotalPhysicalSize(size_t totalPhysicalSize) { m_totalPhysicalSize = totalPhysicalSize; }

    // This is the address past the end of physical memory in this range.
    // When decomitting this range, we decommitt [begin(), physicalEnd).
    char* physicalEnd() const { return m_physicalEnd; }
    void setPhysicalEnd(void* physicalEnd) { m_physicalEnd = static_cast<char*>(physicalEnd); }
    void clearPhysicalEnd() { m_physicalEnd = begin(); }
    bool hasPhysicalPages() { return m_physicalEnd != begin(); }

    std::pair<LargeRange, LargeRange> split(size_t) const;

    void setEligible(bool eligible) { m_isEligible = eligible; }
    bool isEligibile() const { return m_isEligible; }

    bool usedSinceLastScavenge() const { return m_usedSinceLastScavenge; }
    void clearUsedSinceLastScavenge() { m_usedSinceLastScavenge = false; }
    void setUsedSinceLastScavenge() { m_usedSinceLastScavenge = true; }

    bool operator<(const void* other) const { return begin() < other; }
    bool operator<(const LargeRange& other) const { return begin() < other.begin(); }

private:
    size_t m_startPhysicalSize;
    size_t m_totalPhysicalSize;
    char* m_physicalEnd;
    unsigned m_isEligible: 1;
    unsigned m_usedSinceLastScavenge: 1;
};

inline bool canMerge(const LargeRange& a, const LargeRange& b)
{
    if (!a.isEligibile() || !b.isEligibile()) {
        // FIXME: We can make this work if we find it's helpful as long as the merged
        // range is only eligible if a and b are eligible.
        return false;
    }

    if (a.end() == b.begin())
        return true;
    
    if (b.end() == a.begin())
        return true;
    
    return false;
}

inline LargeRange merge(const LargeRange& a, const LargeRange& b)
{
    const LargeRange& left = std::min(a, b);
    const LargeRange& right = std::max(a, b);
    void* physicalEnd = right.totalPhysicalSize() ? right.physicalEnd() : left.physicalEnd();
    bool mergedUsedSinceLastScavenge = a.usedSinceLastScavenge() || b.usedSinceLastScavenge();
    if (left.size() == left.startPhysicalSize()) {
        return LargeRange(
            left.begin(),
            a.size() + b.size(),
            a.startPhysicalSize() + b.startPhysicalSize(),
            a.totalPhysicalSize() + b.totalPhysicalSize(),
            physicalEnd,
            mergedUsedSinceLastScavenge
        );
        
    }

    return LargeRange(
        left.begin(),
        a.size() + b.size(),
        left.startPhysicalSize(),
        a.totalPhysicalSize() + b.totalPhysicalSize(),
        physicalEnd,
        mergedUsedSinceLastScavenge
    );
}

inline std::pair<LargeRange, LargeRange> LargeRange::split(size_t leftSize) const
{
    BASSERT(leftSize <= this->size());
    size_t rightSize = this->size() - leftSize;
    char* physicalEnd = this->physicalEnd();

    if (leftSize <= startPhysicalSize()) {
        BASSERT(totalPhysicalSize() >= leftSize);
        LargeRange left(begin(), leftSize, leftSize, leftSize, std::min(physicalEnd, begin() + leftSize));
        LargeRange right(left.end(), rightSize, startPhysicalSize() - leftSize, totalPhysicalSize() - leftSize, std::max(physicalEnd, left.end()));
        return std::make_pair(left, right);
    }

    double ratio = static_cast<double>(leftSize) / static_cast<double>(this->size());
    size_t leftTotalPhysicalSize = static_cast<size_t>(ratio * totalPhysicalSize());
    BASSERT(leftTotalPhysicalSize <= leftSize);
    leftTotalPhysicalSize = std::max(startPhysicalSize(), leftTotalPhysicalSize);
    size_t rightTotalPhysicalSize = totalPhysicalSize() - leftTotalPhysicalSize;
    if (rightTotalPhysicalSize > rightSize) { // This may happen because of rounding.
        leftTotalPhysicalSize += rightTotalPhysicalSize - rightSize;
        BASSERT(leftTotalPhysicalSize <= leftSize);
        rightTotalPhysicalSize = rightSize;
    }

    LargeRange left(begin(), leftSize, startPhysicalSize(), leftTotalPhysicalSize, std::min(physicalEnd, begin() + leftSize));
    LargeRange right(left.end(), rightSize, 0, rightTotalPhysicalSize, std::max(physicalEnd, left.end()));
    return std::make_pair(left, right);
}

} // namespace bmalloc

#endif
