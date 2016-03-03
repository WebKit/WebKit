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

#include "Page.h"

namespace WebCore {

PageThrottler::PageThrottler(Page& page)
    : m_page(page)
    , m_userInputHysteresis([this](HysteresisState state) { setActivityFlag(PageActivityState::UserInputActivity, state == HysteresisState::Started); })
    , m_mediaActivityHysteresis([this](HysteresisState state) { setActivityFlag(PageActivityState::MediaActivity, state == HysteresisState::Started); })
    , m_mediaActivityCounter([this](RefCounterEvent) { mediaActivityCounterChanged(); })
    , m_pageLoadActivityCounter([this](RefCounterEvent) { setActivityFlag(PageActivityState::PageLoadActivity, m_pageLoadActivityCounter.value()); })
{
}

PageActivityAssertionToken PageThrottler::mediaActivityToken()
{
    return m_mediaActivityCounter.count();
}

PageActivityAssertionToken PageThrottler::pageLoadActivityToken()
{
    return m_pageLoadActivityCounter.count();
}

void PageThrottler::mediaActivityCounterChanged()
{
    if (m_mediaActivityCounter.value())
        m_mediaActivityHysteresis.start();
    else
        m_mediaActivityHysteresis.stop();
}

void PageThrottler::setActivityFlag(PageActivityState::Flags flag, bool value)
{
    PageActivityState::Flags activityState = m_activityState;
    if (value)
        activityState |= flag;
    else
        activityState &= ~flag;

    if (m_activityState == activityState)
        return;
    m_activityState = activityState;

    m_page.setPageActivityState(m_activityState);
}

}
