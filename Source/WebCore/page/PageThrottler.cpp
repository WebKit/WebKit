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

#include "Chrome.h"
#include "ChromeClient.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageActivityAssertionToken.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

PageThrottler::PageThrottler(Page& page, ViewState::Flags viewState)
    : m_page(page)
    , m_viewState(viewState)
    , m_weakPtrFactory(this)
    , m_hysteresis(*this)
    , m_activity("Page is active.")
    , m_activityCount(0)
{
    if (!(m_viewState & ViewState::IsVisuallyIdle))
        m_hysteresis.start();
}

std::unique_ptr<PageActivityAssertionToken> PageThrottler::mediaActivityToken()
{
    return std::make_unique<PageActivityAssertionToken>(*this);
}

std::unique_ptr<PageActivityAssertionToken> PageThrottler::pageLoadActivityToken()
{
    return std::make_unique<PageActivityAssertionToken>(*this);
}

void PageThrottler::incrementActivityCount()
{
    ++m_activityCount;
    updateHysteresis();
}

void PageThrottler::decrementActivityCount()
{
    --m_activityCount;
    updateHysteresis();
}

void PageThrottler::updateHysteresis()
{
    if (m_activityCount || !(m_viewState & ViewState::IsVisuallyIdle))
        m_hysteresis.start();
    else
        m_hysteresis.stop();
}

void PageThrottler::setViewState(ViewState::Flags viewState)
{
    ViewState::Flags changed = m_viewState ^ viewState;
    m_viewState = viewState;

    if (changed & ViewState::IsVisuallyIdle)
        updateHysteresis();
}

void PageThrottler::started()
{
    m_activity.beginActivity();
}

void PageThrottler::stopped()
{
    m_activity.endActivity();
}

}
