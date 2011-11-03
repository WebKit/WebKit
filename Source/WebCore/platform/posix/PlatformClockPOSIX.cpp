/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PlatformClockPOSIX.h"

#include "FloatConversion.h"

using namespace WebCore;

static const int32_t usecPerSec = 1e6;

static float timevalToFloat(timeval val)
{
    return val.tv_sec + (val.tv_usec / static_cast<float>(usecPerSec));
}

static timeval timevalDelta(timeval first, timeval second)
{
    timeval delta;
    delta.tv_sec = first.tv_sec - second.tv_sec;
    delta.tv_usec = first.tv_usec - second.tv_usec;
    while (delta.tv_usec < 0) {
        delta.tv_usec += usecPerSec;
        delta.tv_sec--;
    }
    return delta;
}

PlatformClockPOSIX::PlatformClockPOSIX()
    : m_running(false)
    , m_rate(1)
    , m_offset(0)
{
    m_startTime = m_lastTime = now();
}

void PlatformClockPOSIX::setCurrentTime(float time)
{
    m_startTime = m_lastTime = now();
    m_offset = time;
}

float PlatformClockPOSIX::currentTime() const
{
    if (m_running)
        m_lastTime = now();
    float time = (timevalToFloat(timevalDelta(m_lastTime, m_startTime)) * m_rate) + m_offset;
    return time;
}

void PlatformClockPOSIX::setPlayRate(float rate)
{
    m_offset = currentTime();
    m_lastTime = m_startTime = now();
    m_rate = rate;
}

void PlatformClockPOSIX::start()
{
    if (m_running)
        return;

    m_lastTime = m_startTime = now();
    m_running = true;
}

void PlatformClockPOSIX::stop()
{
    if (!m_running)
        return;

    m_offset = currentTime();
    m_lastTime = m_startTime = now();
    m_running = false;
}

timeval PlatformClockPOSIX::now() const
{
    timeval val;
    gettimeofday(&val, 0);
    return val;
}
