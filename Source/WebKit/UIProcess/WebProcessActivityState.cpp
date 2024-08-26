/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WebProcessActivityState.h"

#include "ProcessThrottler.h"
#include "WebProcessProxy.h"

namespace WebKit {
using namespace WebCore;

WebProcessActivityState::WebProcessActivityState(WebProcessProxy& process)
    : m_process(process)
{
}

void WebProcessActivityState::takeVisibleActivity(PageIdentifier pageID)
{
    m_isVisibleActivity.add(pageID, m_process->throttler().foregroundActivity("View is visible"_s));
#if PLATFORM(MAC)
    if (auto it = m_wasRecentlyVisibleActivity.find(pageID); it != m_wasRecentlyVisibleActivity.end())
        *it->value = nullptr;
#endif
}

void WebProcessActivityState::takeAudibleActivity(PageIdentifier pageID)
{
    m_isAudibleActivity.add(pageID, m_process->throttler().foregroundActivity("View is playing audio"_s));
}

void WebProcessActivityState::takeCapturingActivity(PageIdentifier pageID)
{
    m_isCapturingActivity.add(pageID, m_process->throttler().foregroundActivity("View is capturing media"_s));
}

void WebProcessActivityState::reset(PageIdentifier pageID)
{
    m_isVisibleActivity.remove(pageID);
#if PLATFORM(MAC)
    if (auto it = m_wasRecentlyVisibleActivity.find(pageID); it != m_wasRecentlyVisibleActivity.end())
        *it->value = nullptr;
#endif
    m_isAudibleActivity.remove(pageID);
    m_isCapturingActivity.remove(pageID);
#if PLATFORM(IOS_FAMILY)
    m_openingAppLinkActivity.remove(pageID);
#endif
}

void WebProcessActivityState::dropVisibleActivity(PageIdentifier pageID)
{
#if PLATFORM(MAC)
    if (auto it = m_wasRecentlyVisibleActivity.find(pageID); it != m_wasRecentlyVisibleActivity.end()) {
        if (WTF::numberOfProcessorCores() > 4)
            *it->value = m_process->throttler().backgroundActivity("View was recently visible"_s);
        else
            *it->value = m_process->throttler().foregroundActivity("View was recently visible"_s);
    }
#endif
    m_isVisibleActivity.remove(pageID);
}

void WebProcessActivityState::dropAudibleActivity(PageIdentifier pageID)
{
    m_isAudibleActivity.remove(pageID);
}

void WebProcessActivityState::dropCapturingActivity(PageIdentifier pageID)
{
    m_isCapturingActivity.remove(pageID);
}

bool WebProcessActivityState::hasValidVisibleActivity(PageIdentifier pageID) const
{
    auto it = m_isVisibleActivity.find(pageID);
    return it != m_isVisibleActivity.end() && it->value->isValid();
}

bool WebProcessActivityState::hasValidAudibleActivity(PageIdentifier pageID) const
{
    auto it = m_isAudibleActivity.find(pageID);
    return it != m_isAudibleActivity.end() && it->value->isValid();
}

bool WebProcessActivityState::hasValidCapturingActivity(PageIdentifier pageID) const
{
    auto it = m_isCapturingActivity.find(pageID);
    return it != m_isCapturingActivity.end() && it->value->isValid();
}

#if PLATFORM(IOS_FAMILY)
void WebProcessActivityState::takeOpeningAppLinkActivity(PageIdentifier pageID)
{
    m_openingAppLinkActivity.add(pageID, m_process->throttler().backgroundActivity("Opening AppLink"_s));
}

void WebProcessActivityState::dropOpeningAppLinkActivity(PageIdentifier pageID)
{
    m_openingAppLinkActivity.remove(pageID);
}

bool WebProcessActivityState::hasValidOpeningAppLinkActivity(PageIdentifier pageID) const
{
    auto it = m_openingAppLinkActivity.find(pageID);
    return it != m_openingAppLinkActivity.end() && it->value->isValid();
}
#endif

#if PLATFORM(MAC)
void WebProcessActivityState::takeWasRecentlyVisibleActivity(PageIdentifier pageID)
{
    m_wasRecentlyVisibleActivity.add(pageID, makeUniqueRef<ProcessThrottlerTimedActivity>(8_min));
}
#endif

} // namespace WebKit
