/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "BackgroundProcessResponsivenessTimer.h"

#include "Logging.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

static const Seconds initialCheckingInterval { 20_s };
static const Seconds maximumCheckingInterval { 8_h };
static const Seconds responsivenessTimeout { 90_s };

BackgroundProcessResponsivenessTimer::BackgroundProcessResponsivenessTimer(WebProcessProxy& webProcessProxy)
    : m_webProcessProxy(webProcessProxy)
    , m_checkingInterval(initialCheckingInterval)
    , m_responsivenessCheckTimer(RunLoop::main(), this, &BackgroundProcessResponsivenessTimer::responsivenessCheckTimerFired)
    , m_timeoutTimer(RunLoop::main(), this, &BackgroundProcessResponsivenessTimer::timeoutTimerFired)
{
}

BackgroundProcessResponsivenessTimer::~BackgroundProcessResponsivenessTimer()
{
}

void BackgroundProcessResponsivenessTimer::updateState()
{
    if (!shouldBeActive()) {
        if (m_responsivenessCheckTimer.isActive()) {
            m_checkingInterval = initialCheckingInterval;
            m_responsivenessCheckTimer.stop();
        }
        m_timeoutTimer.stop();
        m_isResponsive = true;
        return;
    }

    if (!isActive())
        m_responsivenessCheckTimer.startOneShot(m_checkingInterval);
}

void BackgroundProcessResponsivenessTimer::didReceiveBackgroundResponsivenessPong()
{
    if (!m_timeoutTimer.isActive())
        return;

    m_timeoutTimer.stop();
    scheduleNextResponsivenessCheck();

    setResponsive(true);
}

void BackgroundProcessResponsivenessTimer::invalidate()
{
    m_timeoutTimer.stop();
    m_responsivenessCheckTimer.stop();
}

void BackgroundProcessResponsivenessTimer::processTerminated()
{
    invalidate();
    setResponsive(true);
}

void BackgroundProcessResponsivenessTimer::responsivenessCheckTimerFired()
{
    ASSERT(shouldBeActive());
    ASSERT(!m_timeoutTimer.isActive());

    m_timeoutTimer.startOneShot(responsivenessTimeout);
    m_webProcessProxy.send(Messages::WebProcess::BackgroundResponsivenessPing(), 0);
}

void BackgroundProcessResponsivenessTimer::timeoutTimerFired()
{
    ASSERT(shouldBeActive());

    scheduleNextResponsivenessCheck();

    if (!m_isResponsive)
        return;

    if (!client().mayBecomeUnresponsive())
        return;

    setResponsive(false);
}

void BackgroundProcessResponsivenessTimer::setResponsive(bool isResponsive)
{
    if (m_isResponsive == isResponsive)
        return;

    client().willChangeIsResponsive();
    m_isResponsive = isResponsive;
    client().didChangeIsResponsive();

    if (m_isResponsive) {
        RELEASE_LOG_ERROR(PerformanceLogging, "Notifying the client that background WebProcess with pid %d has become responsive again", m_webProcessProxy.processIdentifier());
        client().didBecomeResponsive();
    } else {
        RELEASE_LOG_ERROR(PerformanceLogging, "Notifying the client that background WebProcess with pid %d has become unresponsive", m_webProcessProxy.processIdentifier());
        client().didBecomeUnresponsive();
    }
}

bool BackgroundProcessResponsivenessTimer::shouldBeActive() const
{
#if !PLATFORM(IOS_FAMILY)
    return !m_webProcessProxy.visiblePageCount() && m_webProcessProxy.pageCount();
#else
    // Disable background process responsiveness checking on iOS since such processes usually get suspended.
    return false;
#endif
}

bool BackgroundProcessResponsivenessTimer::isActive() const
{
    return m_responsivenessCheckTimer.isActive() || m_timeoutTimer.isActive();
}

void BackgroundProcessResponsivenessTimer::scheduleNextResponsivenessCheck()
{
    // Exponential backoff to avoid waking up the process too often.
    ASSERT(!m_responsivenessCheckTimer.isActive());
    m_checkingInterval = std::min(m_checkingInterval * 2, maximumCheckingInterval);
    m_responsivenessCheckTimer.startOneShot(m_checkingInterval);
}

ResponsivenessTimer::Client& BackgroundProcessResponsivenessTimer::client() const
{
    return m_webProcessProxy;
}

} // namespace WebKit
