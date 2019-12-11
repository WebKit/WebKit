/*
 * Copyright (C) 2012, 2015 Apple Inc.  All rights reserved.
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

#import "config.h"

#if USE(COREMEDIA)
#import "ClockCM.h"

#import <pal/avfoundation/MediaTimeAVFoundation.h>

#import "CoreMediaSoftLink.h"

using namespace PAL;

// A default time scale of 1000 allows milli-second CMTime precision without scaling the timebase.
static const int32_t DefaultTimeScale = 1000;

std::unique_ptr<Clock> Clock::create()
{
    return makeUnique<ClockCM>();
}

ClockCM::ClockCM()
    : m_timebase(0)
    , m_rate(1)
    , m_running(false)
{
    CMClockRef rawClockPtr = 0;
#if PLATFORM(IOS_FAMILY)
    CMAudioClockCreate(kCFAllocatorDefault, &rawClockPtr);
#else
    CMAudioDeviceClockCreate(kCFAllocatorDefault, NULL, &rawClockPtr);
#endif
    RetainPtr<CMClockRef> clock = adoptCF(rawClockPtr);
    initializeWithTimingSource(clock.get());
}

ClockCM::ClockCM(CMClockRef clock)
    : m_timebase(0)
    , m_running(false)
{
    initializeWithTimingSource(clock);
}

void ClockCM::initializeWithTimingSource(CMClockRef clock)
{
    CMTimebaseRef rawTimebasePtr = 0;
    CMTimebaseCreateWithMasterClock(kCFAllocatorDefault, clock, &rawTimebasePtr);
    m_timebase = adoptCF(rawTimebasePtr);
}

void ClockCM::setCurrentTime(double time)
{
    CMTime cmTime = CMTimeMakeWithSeconds(time, DefaultTimeScale);
    CMTimebaseSetTime(m_timebase.get(), cmTime);
}

double ClockCM::currentTime() const
{
    CMTime cmTime = CMTimebaseGetTime(m_timebase.get());
    return CMTimeGetSeconds(cmTime);
}

void ClockCM::setCurrentMediaTime(const MediaTime& time)
{
    CMTimebaseSetTime(m_timebase.get(), PAL::toCMTime(time));
}

MediaTime ClockCM::currentMediaTime() const
{
    return PAL::toMediaTime(CMTimebaseGetTime(m_timebase.get()));
}

void ClockCM::setPlayRate(double rate)
{
    if (m_rate == rate)
        return;

    m_rate = rate;
    if (m_running)
        CMTimebaseSetRate(m_timebase.get(), rate);
}

void ClockCM::start()
{
    if (m_running)
        return;
    m_running = true;
    CMTimebaseSetRate(m_timebase.get(), m_rate);
}

void ClockCM::stop()
{
    if (!m_running)
        return;
    m_running = false;
    CMTimebaseSetRate(m_timebase.get(), 0);
}

#endif
