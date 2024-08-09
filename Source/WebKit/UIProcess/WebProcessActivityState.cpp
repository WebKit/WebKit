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

WebProcessActivityState::WebProcessActivityState(WebProcessProxy& process)
    : m_process(process)
#if PLATFORM(MAC)
    , m_wasRecentlyVisibleActivity(makeUniqueRef<ProcessThrottlerTimedActivity>(8_min))
#endif
{
}

void WebProcessActivityState::takeVisibleActivity()
{
    m_isVisibleActivity = m_process->throttler().foregroundActivity("View is visible"_s).moveToUniquePtr();
#if PLATFORM(MAC)
    *m_wasRecentlyVisibleActivity = nullptr;
#endif
}

void WebProcessActivityState::takeAudibleActivity()
{
    m_isAudibleActivity = m_process->throttler().foregroundActivity("View is playing audio"_s).moveToUniquePtr();
}

void WebProcessActivityState::takeCapturingActivity()
{
    m_isCapturingActivity = m_process->throttler().foregroundActivity("View is capturing media"_s).moveToUniquePtr();
}

void WebProcessActivityState::reset()
{
    m_isVisibleActivity = nullptr;
#if PLATFORM(MAC)
    *m_wasRecentlyVisibleActivity = nullptr;
#endif
    m_isAudibleActivity = nullptr;
    m_isCapturingActivity = nullptr;
#if PLATFORM(IOS_FAMILY)
    m_openingAppLinkActivity = nullptr;
#endif
}

void WebProcessActivityState::dropVisibleActivity()
{
#if PLATFORM(MAC)
    if (WTF::numberOfProcessorCores() > 4)
        *m_wasRecentlyVisibleActivity = m_process->throttler().backgroundActivity("View was recently visible"_s);
    else
        *m_wasRecentlyVisibleActivity = m_process->throttler().foregroundActivity("View was recently visible"_s);
#endif
    m_isVisibleActivity = nullptr;
}

void WebProcessActivityState::dropAudibleActivity()
{
    m_isAudibleActivity = nullptr;
}

void WebProcessActivityState::dropCapturingActivity()
{
    m_isCapturingActivity = nullptr;
}

bool WebProcessActivityState::hasValidVisibleActivity() const
{
    return m_isVisibleActivity && m_isVisibleActivity->isValid();
}

bool WebProcessActivityState::hasValidAudibleActivity() const
{
    return m_isAudibleActivity && m_isAudibleActivity->isValid();
}

bool WebProcessActivityState::hasValidCapturingActivity() const
{
    return m_isCapturingActivity && m_isCapturingActivity->isValid();
}

#if PLATFORM(IOS_FAMILY)
void WebProcessActivityState::takeOpeningAppLinkActivity()
{
    m_openingAppLinkActivity = m_process->throttler().backgroundActivity("Opening AppLink"_s).moveToUniquePtr();
}

void WebProcessActivityState::dropOpeningAppLinkActivity()
{
    m_openingAppLinkActivity = nullptr;
}

bool WebProcessActivityState::hasValidOpeningAppLinkActivity() const
{
    return m_openingAppLinkActivity && m_openingAppLinkActivity->isValid();
}
#endif

} // namespace WebKit
