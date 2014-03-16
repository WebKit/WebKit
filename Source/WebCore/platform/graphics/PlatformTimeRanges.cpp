/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "PlatformTimeRanges.h"

#include <math.h>

namespace WebCore {

std::unique_ptr<PlatformTimeRanges> PlatformTimeRanges::create()
{
    return std::make_unique<PlatformTimeRanges>();
}

std::unique_ptr<PlatformTimeRanges> PlatformTimeRanges::create(double start, double end)
{
    return std::make_unique<PlatformTimeRanges>(start, end);
}

std::unique_ptr<PlatformTimeRanges> PlatformTimeRanges::create(const PlatformTimeRanges& other)
{
    return std::make_unique<PlatformTimeRanges>(other);
}
    
PlatformTimeRanges::PlatformTimeRanges(double start, double end)
{
    add(start, end);
}

PlatformTimeRanges::PlatformTimeRanges(const PlatformTimeRanges& other)
{
    copy(other);
}

PlatformTimeRanges& PlatformTimeRanges::operator=(const PlatformTimeRanges& other)
{
    return copy(other);
}

PlatformTimeRanges& PlatformTimeRanges::copy(const PlatformTimeRanges& other)
{
    unsigned size = other.m_ranges.size();
    for (unsigned i = 0; i < size; i++)
        add(other.m_ranges[i].m_start, other.m_ranges[i].m_end);
    
    return *this;
}

void PlatformTimeRanges::invert()
{
    PlatformTimeRanges inverted;
    double posInf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();

    if (!m_ranges.size())
        inverted.add(negInf, posInf);
    else {
        double start = m_ranges.first().m_start;
        if (start != negInf)
            inverted.add(negInf, start);

        for (size_t index = 0; index + 1 < m_ranges.size(); ++index)
            inverted.add(m_ranges[index].m_end, m_ranges[index + 1].m_start);

        double end = m_ranges.last().m_end;
        if (end != posInf)
            inverted.add(end, posInf);
    }

    m_ranges.swap(inverted.m_ranges);
}

void PlatformTimeRanges::intersectWith(const PlatformTimeRanges& other)
{
    PlatformTimeRanges invertedOther(other);

    invertedOther.invert();
    invert();
    unionWith(invertedOther);
    invert();
}

void PlatformTimeRanges::unionWith(const PlatformTimeRanges& other)
{
    PlatformTimeRanges unioned(*this);

    for (size_t index = 0; index < other.m_ranges.size(); ++index) {
        const Range& range = other.m_ranges[index];
        unioned.add(range.m_start, range.m_end);
    }

    m_ranges.swap(unioned.m_ranges);
}

double PlatformTimeRanges::start(unsigned index, bool& valid) const
{ 
    if (index >= length()) {
        valid = false;
        return 0;
    }
    
    valid = true;
    return m_ranges[index].m_start;
}

double PlatformTimeRanges::end(unsigned index, bool& valid) const
{ 
    if (index >= length()) {
        valid = false;
        return 0;
    }

    valid = true;
    return m_ranges[index].m_end;
}

void PlatformTimeRanges::add(double start, double end)
{
    ASSERT(start <= end);
    unsigned overlappingArcIndex;
    Range addedRange(start, end);

    // For each present range check if we need to:
    // - merge with the added range, in case we are overlapping or contiguous
    // - Need to insert in place, we we are completely, not overlapping and not contiguous
    // in between two ranges.
    //
    // TODO: Given that we assume that ranges are correctly ordered, this could be optimized.

    for (overlappingArcIndex = 0; overlappingArcIndex < m_ranges.size(); overlappingArcIndex++) {
        if (addedRange.isOverlappingRange(m_ranges[overlappingArcIndex]) || addedRange.isContiguousWithRange(m_ranges[overlappingArcIndex])) {
            // We need to merge the addedRange and that range.
            addedRange = addedRange.unionWithOverlappingOrContiguousRange(m_ranges[overlappingArcIndex]);
            m_ranges.remove(overlappingArcIndex);
            overlappingArcIndex--;
        } else {
            // Check the case for which there is no more to do
            if (!overlappingArcIndex) {
                if (addedRange.isBeforeRange(m_ranges[0])) {
                    // First index, and we are completely before that range (and not contiguous, nor overlapping).
                    // We just need to be inserted here.
                    break;
                }
            } else {
                if (m_ranges[overlappingArcIndex - 1].isBeforeRange(addedRange) && addedRange.isBeforeRange(m_ranges[overlappingArcIndex])) {
                    // We are exactly after the current previous range, and before the current range, while
                    // not overlapping with none of them. Insert here.
                    break;
                }
            }
        }
    }

    // Now that we are sure we don't overlap with any range, just add it.
    m_ranges.insert(overlappingArcIndex, addedRange);
}

bool PlatformTimeRanges::contain(double time) const
{
    return find(time) != notFound;
}

size_t PlatformTimeRanges::find(double time) const
{
    bool ignoreInvalid;
    for (unsigned n = 0; n < length(); n++) {
        if (time >= start(n, ignoreInvalid) && time <= end(n, ignoreInvalid))
            return n;
    }
    return notFound;
}

double PlatformTimeRanges::nearest(double time) const
{
    double closestDelta = std::numeric_limits<double>::infinity();
    double closestTime = 0;
    unsigned count = length();
    bool ignoreInvalid;

    for (unsigned ndx = 0; ndx < count; ndx++) {
        double startTime = start(ndx, ignoreInvalid);
        double endTime = end(ndx, ignoreInvalid);
        if (time >= startTime && time <= endTime)
            return time;

        double startTimeDelta = fabs(startTime - time);
        if (startTimeDelta < closestDelta) {
            closestTime = startTime;
            closestDelta = startTimeDelta;
        }

        double endTimeDelta = fabs(endTime - time);
        if (endTimeDelta < closestDelta) {
            closestTime = endTime;
            closestDelta = endTimeDelta;
        }
    }
    return closestTime;
}

double PlatformTimeRanges::totalDuration() const
{
    double total = 0;
    bool ignoreInvalid;

    for (unsigned n = 0; n < length(); n++)
        total += fabs(end(n, ignoreInvalid) - start(n, ignoreInvalid));
    return total;
}

}
