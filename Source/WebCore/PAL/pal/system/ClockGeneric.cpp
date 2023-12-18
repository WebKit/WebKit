/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ClockGeneric.h"


namespace PAL {

std::unique_ptr<Clock> Clock::create()
{
    return makeUnique<ClockGeneric>();
}

ClockGeneric::ClockGeneric()
    : m_running(false)
    , m_rate(1)
    , m_offset(0)
{
    m_startTime = m_lastTime = now();
}

void ClockGeneric::setCurrentTime(double time)
{
    m_startTime = m_lastTime = now();
    m_offset = time;
}

double ClockGeneric::currentTime() const
{
    return currentDelta() + m_offset;
}

void ClockGeneric::setPlayRate(double rate)
{
    m_offset += currentDelta();
    m_lastTime = m_startTime = now();
    m_rate = rate;
}

void ClockGeneric::start()
{
    if (m_running)
        return;

    m_lastTime = m_startTime = now();
    m_running = true;
}

void ClockGeneric::stop()
{
    if (!m_running)
        return;

    m_offset += currentDelta();
    m_lastTime = m_startTime = now();
    m_running = false;
}

MonotonicTime ClockGeneric::now() const
{
    return MonotonicTime::now();
}

double ClockGeneric::currentDelta() const
{
    if (m_running)
        m_lastTime = now();
    return (m_lastTime - m_startTime).seconds() * m_rate;
}

}
