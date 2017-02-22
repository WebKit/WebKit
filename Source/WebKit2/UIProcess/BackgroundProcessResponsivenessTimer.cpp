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

namespace WebKit {

static const std::chrono::seconds initialInterval { 20s };
static const std::chrono::seconds maximumInterval { 8h };

BackgroundProcessResponsivenessTimer::BackgroundProcessResponsivenessTimer(WebProcessProxy& webProcessProxy)
    : m_webProcessProxy(webProcessProxy)
    , m_interval(initialInterval)
    , m_timer(RunLoop::main(), this, &BackgroundProcessResponsivenessTimer::timerFired)
{
}

void BackgroundProcessResponsivenessTimer::updateState()
{
    if (!shouldBeActive()) {
        if (m_timer.isActive()) {
            m_interval = initialInterval;
            m_timer.stop();
        }
        return;
    }

    if (!m_timer.isActive())
        m_timer.startOneShot(m_interval);
}

void BackgroundProcessResponsivenessTimer::timerFired()
{
    ASSERT(shouldBeActive());
    // WebProcessProxy::responsive() takes care of calling processDidBecomeUnresponsive for us.
    m_webProcessProxy.isResponsive([this](bool processIsResponsive) {
        if (processIsResponsive) {
            // Exponential backoff to avoid waking up the process too often.
            m_interval = std::min(m_interval * 2, maximumInterval);
            m_timer.startOneShot(m_interval);
            return;
        }

        RELEASE_LOG_ERROR(PerformanceLogging, "Notified the client that a background WebProcess has become unresponsive");
        WTFLogAlways("Notified the client that a background WebProcess has become unresponsive");
        m_interval = initialInterval;
    });
}

} // namespace WebKit
