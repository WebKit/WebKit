/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCFrameRateController.h"

#include "TraceEvent.h"
#include "cc/CCDelayBasedTimeSource.h"
#include "cc/CCTimeSource.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

class CCFrameRateControllerTimeSourceAdapter : public CCTimeSourceClient {
public:
    static PassOwnPtr<CCFrameRateControllerTimeSourceAdapter> create(CCFrameRateController* frameRateController)
    {
        return adoptPtr(new CCFrameRateControllerTimeSourceAdapter(frameRateController));
    }
    virtual ~CCFrameRateControllerTimeSourceAdapter() { }

    virtual void onTimerTick() OVERRIDE { m_frameRateController->onTimerTick(); }
private:
    explicit CCFrameRateControllerTimeSourceAdapter(CCFrameRateController* frameRateController)
            : m_frameRateController(frameRateController) { }

    CCFrameRateController* m_frameRateController;
};

CCFrameRateController::CCFrameRateController(PassRefPtr<CCTimeSource> timer)
    : m_client(0)
    , m_numFramesPending(0)
    , m_maxFramesPending(0)
    , m_timeSource(timer)
    , m_active(false)
    , m_isTimeSourceThrottling(true)
{
    m_timeSourceClientAdapter = CCFrameRateControllerTimeSourceAdapter::create(this);
    m_timeSource->setClient(m_timeSourceClientAdapter.get());
}

CCFrameRateController::CCFrameRateController(CCThread* thread)
    : m_client(0)
    , m_numFramesPending(0)
    , m_maxFramesPending(0)
    , m_active(false)
    , m_isTimeSourceThrottling(false)
{
    m_manualTicker = adoptPtr(new CCTimer(thread, this));
}

CCFrameRateController::~CCFrameRateController()
{
    if (m_isTimeSourceThrottling)
        m_timeSource->setActive(false);
}

void CCFrameRateController::setActive(bool active)
{
    TRACE_EVENT1("cc", "CCFrameRateController::setActive", "active", active);
    if (m_active == active)
        return;
    m_active = active;

    if (m_isTimeSourceThrottling)
        m_timeSource->setActive(active);
    else {
        if (active)
            postManualTick();
        else
            m_manualTicker->stop();
    }
}

void CCFrameRateController::setMaxFramesPending(int maxFramesPending)
{
    m_maxFramesPending = maxFramesPending;
}

void CCFrameRateController::setTimebaseAndInterval(double timebase, double intervalSeconds)
{
    m_timeSource->setTimebaseAndInterval(timebase, intervalSeconds);
}

void CCFrameRateController::onTimerTick()
{
    ASSERT(m_active);

    // Don't forward the tick if we have too many frames in flight.
    if (m_maxFramesPending && m_numFramesPending >= m_maxFramesPending) {
        TRACE_EVENT0("cc", "CCFrameRateController::onTimerTickButMaxFramesPending");
        return;
    }

    if (m_client)
        m_client->vsyncTick();
}

void CCFrameRateController::postManualTick()
{
    if (m_active)
        m_manualTicker->startOneShot(0);
}

void CCFrameRateController::onTimerFired()
{
    onTimerTick();
}

void CCFrameRateController::didBeginFrame()
{
    m_numFramesPending++;
    if (!m_isTimeSourceThrottling)
        postManualTick();
}

void CCFrameRateController::didFinishFrame()
{
    m_numFramesPending--;
    if (!m_isTimeSourceThrottling)
        postManualTick();
}

void CCFrameRateController::didAbortAllPendingFrames()
{
    m_numFramesPending = 0;
}

double CCFrameRateController::nextTickTime()
{
    if (m_isTimeSourceThrottling)
        return m_timeSource->nextTickTime();

    return monotonicallyIncreasingTime();
}

}
