/*
 * Copyright (C) 2014 University of Washington. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cmath>
#include <utility>
#include <wtf/MonotonicTime.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WTF {

class Stopwatch final : public RefCounted<Stopwatch> {
public:
    static Ref<Stopwatch> create()
    {
        return adoptRef(*new Stopwatch());
    }

    void reset();
    void start();
    void stop();

    Seconds elapsedTime() const;
    Seconds elapsedTimeSince(MonotonicTime) const;
    std::tuple<Seconds, MonotonicTime> elapsedTimeAndTimestamp() const;

    std::optional<Seconds> fromMonotonicTime(MonotonicTime) const;

    bool isActive() const { return !m_lastStartTime.isNaN(); }
private:
    Stopwatch() { reset(); }

    Seconds m_elapsedTime;
    MonotonicTime m_lastStartTime;
    Vector<std::pair<MonotonicTime, MonotonicTime>> m_pastInternals;
};

inline void Stopwatch::reset()
{
    m_elapsedTime = 0_s;
    m_lastStartTime = MonotonicTime::nan();
}

inline void Stopwatch::start()
{
    ASSERT_WITH_MESSAGE(m_lastStartTime.isNaN(), "Tried to start the stopwatch, but it is already running.");

    m_lastStartTime = MonotonicTime::now();
}

inline void Stopwatch::stop()
{
    ASSERT_WITH_MESSAGE(!m_lastStartTime.isNaN(), "Tried to stop the stopwatch, but it is not running.");

    auto stopTime = MonotonicTime::now();
    m_pastInternals.append({ m_lastStartTime, stopTime });
    m_elapsedTime += stopTime - m_lastStartTime;
    m_lastStartTime = MonotonicTime::nan();
}

inline Seconds Stopwatch::elapsedTime() const
{
    return std::get<0>(elapsedTimeAndTimestamp());
}

inline std::tuple<Seconds, MonotonicTime> Stopwatch::elapsedTimeAndTimestamp() const
{
    auto timestamp = MonotonicTime::now();
    if (!isActive())
        return std::tuple { m_elapsedTime, timestamp };

    return std::tuple { m_elapsedTime + (timestamp - m_lastStartTime), timestamp };
}

inline Seconds Stopwatch::elapsedTimeSince(MonotonicTime timeStamp) const
{
    if (!isActive())
        return m_elapsedTime;

    return m_elapsedTime + (timeStamp - m_lastStartTime);
}

inline std::optional<Seconds> Stopwatch::fromMonotonicTime(MonotonicTime timeStamp) const
{
    if (!m_lastStartTime.isNaN() && m_lastStartTime < timeStamp)
        return Stopwatch::elapsedTimeSince(timeStamp);

    Seconds elapsedTime;
    for (auto& interval : m_pastInternals) {
        if (timeStamp < interval.first)
            return std::nullopt;
        if (interval.first <= timeStamp && timeStamp <= interval.second)
            return elapsedTime + timeStamp - interval.first;
        elapsedTime += interval.second - interval.first;
    }

    return std::nullopt;
}

} // namespace WTF

using WTF::Stopwatch;
