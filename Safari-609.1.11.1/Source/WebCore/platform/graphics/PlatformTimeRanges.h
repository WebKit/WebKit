/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef PlatformTimeRanges_h
#define PlatformTimeRanges_h

#include <algorithm>
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>

namespace WTF {
class PrintStream;
}

namespace WebCore {

class PlatformTimeRanges {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PlatformTimeRanges() { }
    PlatformTimeRanges(const MediaTime& start, const MediaTime& end);

    WEBCORE_EXPORT MediaTime start(unsigned index) const;
    MediaTime start(unsigned index, bool& valid) const;
    WEBCORE_EXPORT MediaTime end(unsigned index) const;
    MediaTime end(unsigned index, bool& valid) const;
    MediaTime duration(unsigned index) const;
    MediaTime maximumBufferedTime() const;

    void invert();
    void intersectWith(const PlatformTimeRanges&);
    void unionWith(const PlatformTimeRanges&);

    unsigned length() const { return m_ranges.size(); }

    void add(const MediaTime& start, const MediaTime& end);
    
    bool contain(const MediaTime&) const;

    size_t find(const MediaTime&) const;
    
    MediaTime nearest(const MediaTime&) const;

    MediaTime totalDuration() const;

    void dump(WTF::PrintStream&) const;

private:
    // We consider all the Ranges to be semi-bounded as follow: [start, end[
    struct Range {
        Range() { }
        Range(const MediaTime& start, const MediaTime& end)
            : m_start(start)
            , m_end(end)
        {
        }

        MediaTime m_start;
        MediaTime m_end;

        inline bool isPointInRange(const MediaTime& point) const
        {
            return m_start <= point && point < m_end;
        }
        
        inline bool isOverlappingRange(const Range& range) const
        {
            return isPointInRange(range.m_start) || isPointInRange(range.m_end) || range.isPointInRange(m_start);
        }

        inline bool isContiguousWithRange(const Range& range) const
        {
            return range.m_start == m_end || range.m_end == m_start;
        }
        
        inline Range unionWithOverlappingOrContiguousRange(const Range& range) const
        {
            Range ret;

            ret.m_start = std::min(m_start, range.m_start);
            ret.m_end = std::max(m_end, range.m_end);

            return ret;
        }

        inline bool isBeforeRange(const Range& range) const
        {
            return range.m_start >= m_end;
        }
    };
    
    Vector<Range> m_ranges;
};

} // namespace WebCore

#endif
