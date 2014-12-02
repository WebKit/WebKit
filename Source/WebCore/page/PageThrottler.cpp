/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "PageThrottler.h"

namespace WebCore {

PageThrottler::PageThrottler(ViewState::Flags viewState)
    : m_viewState(viewState)
    , m_hysteresis(*this)
    , m_pageActivityCounter([this]() { pageActivityCounterValueDidChange(); })
{
    updateUserActivity();
}

void PageThrottler::createUserActivity()
{
    ASSERT(!m_activity);
    m_activity = std::make_unique<UserActivity::Impl>("Page is active.");
    updateUserActivity();
}

PageActivityAssertionToken PageThrottler::mediaActivityToken()
{
    return m_pageActivityCounter.count();
}

PageActivityAssertionToken PageThrottler::pageLoadActivityToken()
{
    return m_pageActivityCounter.count();
}

void PageThrottler::pageActivityCounterValueDidChange()
{
    if (m_pageActivityCounter.value())
        m_hysteresis.start();
    else
        m_hysteresis.stop();

    // If the counter is nonzero, state must be Started; if the counter is zero, state may be Waiting or Stopped.
    ASSERT(!!m_pageActivityCounter.value() == (m_hysteresis.state() == HysteresisState::Started));
}

void PageThrottler::updateUserActivity()
{
    if (!m_activity)
        return;

    // Allow throttling if there is no page activity, and the page is visually idle.
    if (m_hysteresis.state() == HysteresisState::Stopped && m_viewState & ViewState::IsVisuallyIdle)
        m_activity->endActivity();
    else
        m_activity->beginActivity();
}

void PageThrottler::setViewState(ViewState::Flags viewState)
{
    ViewState::Flags changed = m_viewState ^ viewState;
    m_viewState = viewState;

    if (changed & ViewState::IsVisuallyIdle)
        updateUserActivity();
}

void PageThrottler::started()
{
    updateUserActivity();
}

void PageThrottler::stopped()
{
    updateUserActivity();
}

}
