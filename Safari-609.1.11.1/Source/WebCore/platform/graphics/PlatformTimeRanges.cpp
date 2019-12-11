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
#include <wtf/PrintStream.h>

namespace WebCore {
    
PlatformTimeRanges::PlatformTimeRanges(const MediaTime& start, const MediaTime& end)
{
    add(start, end);
}

void PlatformTimeRanges::invert()
{
    PlatformTimeRanges inverted;
    MediaTime posInf = MediaTime::positiveInfiniteTime();
    MediaTime negInf = MediaTime::negativeInfiniteTime();

    if (!m_ranges.size())
        inverted.add(negInf, posInf);
    else {
        MediaTime start = m_ranges.first().m_start;
        if (start != negInf)
            inverted.add(negInf, start);

        for (size_t index = 0; index + 1 < m_ranges.size(); ++index)
            inverted.add(m_ranges[index].m_end, m_ranges[index + 1].m_start);

        MediaTime end = m_ranges.last().m_end;
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

MediaTime PlatformTimeRanges::start(unsigned index) const
{
    bool ignoredValid;
    return start(index, ignoredValid);
}

MediaTime PlatformTimeRanges::start(unsigned index, bool& valid) const
{ 
    if (index >= length()) {
        valid = false;
        return MediaTime::zeroTime();
    }
    
    valid = true;
    return m_ranges[index].m_start;
}

MediaTime PlatformTimeRanges::end(unsigned index) const
{
    bool ignoredValid;
    return end(index, ignoredValid);
}

MediaTime PlatformTimeRanges::end(unsigned index, bool& valid) const
{ 
    if (index >= length()) {
        valid = false;
        return MediaTime::zeroTime();
    }

    valid = true;
    return m_ranges[index].m_end;
}

MediaTime PlatformTimeRanges::duration(unsigned index) const
{
    if (index >= length())
        return MediaTime::invalidTime();

    return m_ranges[index].m_end - m_ranges[index].m_start;
}

MediaTime PlatformTimeRanges::maximumBufferedTime() const
{
    if (!length())
        return MediaTime::invalidTime();

    return m_ranges[length() - 1].m_end;
}

void PlatformTimeRanges::add(const MediaTime& start, const MediaTime& end)
{
#if !PLATFORM(MAC) // https://bugs.webkit.org/show_bug.cgi?id=180253
    ASSERT(start.isValid());
    ASSERT(end.isValid());
#endif
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

bool PlatformTimeRanges::contain(const MediaTime& time) const
{
    return find(time) != notFound;
}

size_t PlatformTimeRanges::find(const MediaTime& time) const
{
    bool ignoreInvalid;
    for (unsigned n = 0; n < length(); n++) {
        if (time >= start(n, ignoreInvalid) && time <= end(n, ignoreInvalid))
            return n;
    }
    return notFound;
}

MediaTime PlatformTimeRanges::nearest(const MediaTime& time) const
{
    MediaTime closestDelta = MediaTime::positiveInfiniteTime();
    MediaTime closestTime = MediaTime::zeroTime();
    unsigned count = length();
    if (!count)
        return MediaTime::invalidTime();

    bool ignoreInvalid;

    for (unsigned ndx = 0; ndx < count; ndx++) {
        MediaTime startTime = start(ndx, ignoreInvalid);
        MediaTime endTime = end(ndx, ignoreInvalid);
        if (time >= startTime && time <= endTime)
            return time;

        MediaTime startTimeDelta = abs(startTime - time);
        if (startTimeDelta < closestDelta) {
            closestTime = startTime;
            closestDelta = startTimeDelta;
        }

        MediaTime endTimeDelta = abs(endTime - time);
        if (endTimeDelta < closestDelta) {
            closestTime = endTime;
            closestDelta = endTimeDelta;
        }
    }
    return closestTime;
}

MediaTime PlatformTimeRanges::totalDuration() const
{
    MediaTime total = MediaTime::zeroTime();

    for (unsigned n = 0; n < length(); n++)
        total += abs(end(n) - start(n));
    return total;
}

void PlatformTimeRanges::dump(PrintStream& out) const
{
    if (!length())
        return;

    for (size_t i = 0; i < length(); ++i)
        out.print("[", start(i), "..", end(i), "] ");
}

}
