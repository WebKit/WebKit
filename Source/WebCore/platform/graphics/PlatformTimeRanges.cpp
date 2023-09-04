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
#include <wtf/NeverDestroyed.h>
#include <wtf/PrintStream.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

PlatformTimeRanges::PlatformTimeRanges()
{
}

PlatformTimeRanges::PlatformTimeRanges(const MediaTime& start, const MediaTime& end)
{
    add(start, end);
}

PlatformTimeRanges::PlatformTimeRanges(Vector<Range>&& ranges)
    : m_ranges { WTFMove(ranges) }
{
}

const PlatformTimeRanges& PlatformTimeRanges::emptyRanges()
{
    static LazyNeverDestroyed<PlatformTimeRanges> emptyRanges;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        emptyRanges.construct();
    });
    return emptyRanges.get();
}

MediaTime PlatformTimeRanges::timeFudgeFactor()
{
    return { 2002, 24000 };
}

void PlatformTimeRanges::invert()
{
    PlatformTimeRanges inverted;
    MediaTime posInf = MediaTime::positiveInfiniteTime();
    MediaTime negInf = MediaTime::negativeInfiniteTime();

    if (!m_ranges.size())
        inverted.add(negInf, posInf);
    else {
        MediaTime start = m_ranges.first().start;
        if (start != negInf)
            inverted.add(negInf, start);

        for (size_t index = 0; index + 1 < m_ranges.size(); ++index)
            inverted.add(m_ranges[index].end, m_ranges[index + 1].start);

        MediaTime end = m_ranges.last().end;
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
        unioned.add(range.start, range.end);
    }

    m_ranges.swap(unioned.m_ranges);
}

PlatformTimeRanges& PlatformTimeRanges::operator+=(const PlatformTimeRanges& other)
{
    unionWith(other);
    return *this;
}

PlatformTimeRanges& PlatformTimeRanges::operator-=(const PlatformTimeRanges& other)
{
    for (const auto& range : other.m_ranges)
        *this -= range;

    return *this;
}

PlatformTimeRanges& PlatformTimeRanges::operator-=(const Range& range)
{
    if (range.isEmpty() || m_ranges.isEmpty())
        return *this;

    auto firstEnd = std::max(m_ranges[0].start, range.start);
    auto secondStart = std::min(m_ranges.last().end, range.end);
    Vector<Range> ranges { 2 };
    if (m_ranges[0].start != firstEnd)
        ranges.append({ m_ranges[0].start, firstEnd });
    if (secondStart != m_ranges.last().end)
        ranges.append({ secondStart, m_ranges.last().end });
    intersectWith(WTFMove(ranges));

    return *this;
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
    return m_ranges[index].start;
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
    return m_ranges[index].end;
}

MediaTime PlatformTimeRanges::duration(unsigned index) const
{
    if (index >= length())
        return MediaTime::invalidTime();

    return m_ranges[index].end - m_ranges[index].start;
}

MediaTime PlatformTimeRanges::maximumBufferedTime() const
{
    if (!length())
        return MediaTime::invalidTime();

    return m_ranges[length() - 1].end;
}

MediaTime PlatformTimeRanges::minimumBufferedTime() const
{
    if (!length())
        return MediaTime::invalidTime();

    return m_ranges[0].start;
}

void PlatformTimeRanges::add(const MediaTime& start, const MediaTime& end, AddTimeRangeOption addTimeRangeOption)
{
#if !PLATFORM(MAC) // https://bugs.webkit.org/show_bug.cgi?id=180253
    ASSERT(start.isValid());
    ASSERT(end.isValid());
#endif
    ASSERT(start <= end);

    auto startTime = start;
    auto endTime = end;
    if (addTimeRangeOption == AddTimeRangeOption::EliminateSmallGaps) {
        // Eliminate small gaps between buffered ranges by coalescing
        // disjoint ranges separated by less than a "fudge factor".
        auto nearestToPresentationStartTime = nearest(startTime);
        if (nearestToPresentationStartTime.isValid() && (startTime - nearestToPresentationStartTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
            startTime = nearestToPresentationStartTime;

        auto nearestToPresentationEndTime = nearest(endTime);
        if (nearestToPresentationEndTime.isValid() && (nearestToPresentationEndTime - endTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
            endTime = nearestToPresentationEndTime;
    }
    size_t overlappingArcIndex;
    Range addedRange { .start = startTime, .end = endTime };

    // For each present range check if we need to:
    // - merge with the added range, in case we are overlapping or contiguous
    // - Need to insert in place, we we are completely, not overlapping and not contiguous
    // in between two ranges.

    for (overlappingArcIndex = findLastRangeIndexBefore(start, end); overlappingArcIndex < m_ranges.size(); overlappingArcIndex++) {
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

void PlatformTimeRanges::clear()
{
    m_ranges.clear();
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

size_t PlatformTimeRanges::findWithEpsilon(const MediaTime& time, const MediaTime& epsilon)
{
    bool ignoreInvalid;
    for (unsigned n = 0; n < length(); n++) {
        if (time + epsilon >= start(n, ignoreInvalid) && time < end(n, ignoreInvalid))
            return n;
    }
    return notFound;
}

PlatformTimeRanges PlatformTimeRanges::copyWithEpsilon(const MediaTime& epsilon) const
{
    if (length() <= 1)
        return *this;
    Vector<Range> ranges;
    unsigned n1 = 0;
    for (unsigned n2 = 1; n2 < length(); n2++) {
        auto& previousRangeEnd = m_ranges[n2 - 1].end;
        if (previousRangeEnd + epsilon < m_ranges[n2].start) {
            ranges.append({ m_ranges[n1].start, previousRangeEnd });
            n1 = n2;
        }
    }
    ranges.append({ m_ranges[n1].start, m_ranges[length() - 1].end });
    return ranges;
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

String PlatformTimeRanges::toString() const
{
    StringBuilder result;

    for (size_t i = 0; i < length(); ++i)
        result.append("[", start(i).toString(), "..", end(i).toString(), "] ");

    return result.toString();
}

size_t PlatformTimeRanges::findLastRangeIndexBefore(const MediaTime& start, const MediaTime& end) const
{
    ASSERT(start <= end);

    if (m_ranges.isEmpty())
        return 0;

    const Range range { .start = start, .end = end };
    size_t first, last, middle;
    size_t index = 0;

    first = 0;
    last = m_ranges.size() - 1;
    middle = first + ((last - first) / 2);

    while (first < last && middle > 0) {
        if (m_ranges[middle].isBeforeRange(range)) {
            index = middle;
            first = middle + 1;
        } else
            last = middle - 1;

        middle = first + ((last - first) / 2);
    }
    return index;
}

}
