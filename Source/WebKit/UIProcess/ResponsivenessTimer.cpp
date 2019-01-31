/*
 * Copyright (C) 2010, 2012 Apple Inc. All rights reserved.
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
#include "ResponsivenessTimer.h"

namespace WebKit {

static const Seconds responsivenessTimeout { 3_s };

ResponsivenessTimer::ResponsivenessTimer(ResponsivenessTimer::Client& client)
    : m_client(client)
    , m_timer(*this, &ResponsivenessTimer::timerFired)
{
}

ResponsivenessTimer::~ResponsivenessTimer() = default;

void ResponsivenessTimer::invalidate()
{
    m_waitingForTimer = false;
    m_useLazyStop = false;
    m_timer.stop();
}

void ResponsivenessTimer::timerFired()
{
    if (!m_waitingForTimer)
        return;

    m_waitingForTimer = false;
    m_useLazyStop = false;

    if (!m_isResponsive)
        return;

    if (!m_client.mayBecomeUnresponsive()) {
        m_waitingForTimer = true;
        m_timer.startOneShot(responsivenessTimeout);
        return;
    }

    m_client.willChangeIsResponsive();
    m_isResponsive = false;
    m_client.didChangeIsResponsive();

    m_client.didBecomeUnresponsive();
}
    
void ResponsivenessTimer::start()
{
    if (m_waitingForTimer)
        return;

    m_waitingForTimer = true;
    m_useLazyStop = false;
    m_timer.startOneShot(responsivenessTimeout);
}

void ResponsivenessTimer::startWithLazyStop()
{
    if (!m_waitingForTimer) {
        start();
        m_useLazyStop = true;
    }
}

void ResponsivenessTimer::stop()
{
    if (!m_isResponsive) {
        // We got a life sign from the web process.
        m_client.willChangeIsResponsive();
        m_isResponsive = true;
        m_client.didChangeIsResponsive();

        m_client.didBecomeResponsive();
    }

    m_waitingForTimer = false;

    if (m_useLazyStop)
        m_useLazyStop = false;
    else
        m_timer.stop();
}

void ResponsivenessTimer::processTerminated()
{
    // Since there is no web process, we must not be waiting for it anymore.
    m_waitingForTimer = false;
    m_timer.stop();
}

} // namespace WebKit
