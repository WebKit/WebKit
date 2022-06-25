/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc.  All rights reserved.
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
#include "TimeRanges.h"

namespace WebCore {

Ref<TimeRanges> TimeRanges::create()
{
    return adoptRef(*new TimeRanges);
}

Ref<TimeRanges> TimeRanges::create(double start, double end)
{
    return adoptRef(*new TimeRanges(start, end));
}

Ref<TimeRanges> TimeRanges::create(const PlatformTimeRanges& other)
{
    return adoptRef(*new TimeRanges(other));
}

TimeRanges::TimeRanges()
{
}

TimeRanges::TimeRanges(double start, double end)
    : m_ranges(PlatformTimeRanges(MediaTime::createWithDouble(start), MediaTime::createWithDouble(end)))
{
}

TimeRanges::TimeRanges(const PlatformTimeRanges& other)
    : m_ranges(other)
{
}

ExceptionOr<double> TimeRanges::start(unsigned index) const
{
    bool valid;
    MediaTime result = m_ranges.start(index, valid);
    if (!valid)
        return Exception { IndexSizeError };
    return result.toDouble();
}

ExceptionOr<double> TimeRanges::end(unsigned index) const
{ 
    bool valid;
    MediaTime result = m_ranges.end(index, valid);
    if (!valid)
        return Exception { IndexSizeError };
    return result.toDouble();
}

void TimeRanges::invert()
{
    m_ranges.invert();
}

Ref<TimeRanges> TimeRanges::copy() const
{
    return TimeRanges::create(m_ranges);
}

void TimeRanges::intersectWith(const TimeRanges& other)
{
    m_ranges.intersectWith(other.ranges());
}

void TimeRanges::unionWith(const TimeRanges& other)
{
    m_ranges.unionWith(other.ranges());
}

unsigned TimeRanges::length() const
{
    return m_ranges.length();
}

void TimeRanges::add(double start, double end)
{
    m_ranges.add(MediaTime::createWithDouble(start), MediaTime::createWithDouble(end));
}

bool TimeRanges::contain(double time) const
{
    return m_ranges.contain(MediaTime::createWithDouble(time));
}

size_t TimeRanges::find(double time) const
{
    return m_ranges.find(MediaTime::createWithDouble(time));
}

double TimeRanges::nearest(double time) const
{
    return m_ranges.nearest(MediaTime::createWithDouble(time)).toDouble();
}

double TimeRanges::totalDuration() const
{
    return m_ranges.totalDuration().toDouble();
}

}
