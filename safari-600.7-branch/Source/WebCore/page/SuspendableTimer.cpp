/*
 * Copyright (C) 2008, 2013 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "SuspendableTimer.h"

#include "ScriptExecutionContext.h"

namespace WebCore {

SuspendableTimer::SuspendableTimer(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_suspended(false)
    , m_savedNextFireInterval(0)
    , m_savedRepeatInterval(0)
    , m_savedIsActive(false)
{
}

SuspendableTimer::~SuspendableTimer()
{
}

bool SuspendableTimer::hasPendingActivity() const
{
    return isActive();
}

void SuspendableTimer::stop()
{
    if (!m_suspended)
        TimerBase::stop();

    // Make it less likely that SuspendableTimer is accidentally revived and fires after being stopped.
    // It is not allowed for an ActiveDOMObject to become active again after stop().
    m_suspended = true;
    m_savedIsActive = false;

    didStop();
}

void SuspendableTimer::suspend(ReasonForSuspension)
{
    ASSERT(!m_suspended);
    m_suspended = true;

    m_savedIsActive = TimerBase::isActive();
    if (m_savedIsActive) {
        m_savedNextFireInterval = TimerBase::nextUnalignedFireInterval();
        m_savedRepeatInterval = TimerBase::repeatInterval();
        TimerBase::stop();
    }
}

void SuspendableTimer::resume()
{
    ASSERT(m_suspended);
    m_suspended = false;

    if (m_savedIsActive)
        start(m_savedNextFireInterval, m_savedRepeatInterval);
}

bool SuspendableTimer::canSuspend() const
{
    return true;
}

void SuspendableTimer::didStop()
{
}

void SuspendableTimer::cancel()
{
    if (!m_suspended)
        TimerBase::stop();
    else
        m_suspended = false;
}

void SuspendableTimer::startRepeating(double repeatInterval)
{
    if (!m_suspended)
        TimerBase::startRepeating(repeatInterval);
    else {
        m_savedIsActive = true;
        m_savedNextFireInterval = repeatInterval;
        m_savedRepeatInterval = repeatInterval;
    }
}

void SuspendableTimer::startOneShot(double interval)
{
    if (!m_suspended)
        TimerBase::startOneShot(interval);
    else {
        m_savedIsActive = true;
        m_savedNextFireInterval = interval;
        m_savedRepeatInterval = 0;
    }
}

double SuspendableTimer::repeatInterval() const
{
    if (!m_suspended)
        return TimerBase::repeatInterval();
    if (m_savedIsActive)
        return m_savedRepeatInterval;
    return 0;
}

void SuspendableTimer::augmentFireInterval(double delta)
{
    if (!m_suspended)
        TimerBase::augmentFireInterval(delta);
    else if (m_savedIsActive) {
        m_savedNextFireInterval += delta;
    } else {
        m_savedIsActive = true;
        m_savedNextFireInterval = delta;
        m_savedRepeatInterval = 0;
    }
}

void SuspendableTimer::augmentRepeatInterval(double delta)
{
    if (!m_suspended)
        TimerBase::augmentRepeatInterval(delta);
    else if (m_savedIsActive) {
        m_savedNextFireInterval += delta;
        m_savedRepeatInterval += delta;
    } else {
        m_savedIsActive = true;
        m_savedNextFireInterval = delta;
        m_savedRepeatInterval = delta;
    }
}

} // namespace WebCore
