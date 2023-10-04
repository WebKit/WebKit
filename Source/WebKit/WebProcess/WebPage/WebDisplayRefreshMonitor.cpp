/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "WebDisplayRefreshMonitor.h"

#if HAVE(DISPLAY_LINK)

#include "Logging.h"
#include "WebProcess.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/DisplayRefreshMonitor.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(MAC)
#include <WebCore/RunLoopObserver.h>
#endif

namespace WebKit {
using namespace WebCore;

// Avoid repeated start/stop IPC when rescheduled inside the callback.
constexpr unsigned maxUnscheduledFireCount { 1 };

WebDisplayRefreshMonitor::WebDisplayRefreshMonitor(PlatformDisplayID displayID)
    : DisplayRefreshMonitor(displayID)
    , m_observerID(DisplayLinkObserverID::generate())
{
    ASSERT(isMainRunLoop());
    setMaxUnscheduledFireCount(maxUnscheduledFireCount);
}

WebDisplayRefreshMonitor::~WebDisplayRefreshMonitor()
{
    // stop() should have been called.
    ASSERT(!m_displayLinkIsActive);
}

#if PLATFORM(MAC)
void WebDisplayRefreshMonitor::dispatchDisplayDidRefresh(const DisplayUpdate& displayUpdate)
{
    // FIXME: This will perturb displayUpdate.
    if (!m_firstCallbackInCurrentRunloop) {
        RELEASE_LOG(DisplayLink, "[Web] WebDisplayRefreshMonitor::dispatchDisplayDidRefresh() for display %u - m_firstCallbackInCurrentRunloop is false", displayID());
        Locker locker { lock() };
        setIsPreviousFrameDone(true);
        return;
    }

    DisplayRefreshMonitor::dispatchDisplayDidRefresh(displayUpdate);
}
#endif

bool WebDisplayRefreshMonitor::startNotificationMechanism()
{
    if (m_displayLinkIsActive)
        return true;

    LOG_WITH_STREAM(DisplayLink, stream << "[Web] WebDisplayRefreshMonitor::requestRefreshCallback for display " << displayID() << " - starting");
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StartDisplayLink(m_observerID, displayID(), maxClientPreferredFramesPerSecond().value_or(FullSpeedFramesPerSecond)), 0);

#if PLATFORM(MAC)
    if (!m_runLoopObserver) {
        // The RunLoopObserver repeats.
        // FIXME: Double check whether the value of `DisplayRefreshMonitor` (1) is the appropriate runloop order here,
        // and also whether we should be specifying `RunLoopObserver::Activity::Entry` when scheduling the observer below.
        m_runLoopObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::DisplayRefreshMonitor, [this] {
            m_firstCallbackInCurrentRunloop = true;
        });
    }

    m_runLoopObserver->schedule(CFRunLoopGetCurrent());
#endif
    m_displayLinkIsActive = true;

    return true;
}

void WebDisplayRefreshMonitor::stopNotificationMechanism()
{
    if (!m_displayLinkIsActive)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[Web] WebDisplayRefreshMonitor::requestRefreshCallback - stopping");
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopDisplayLink(m_observerID, displayID()), 0);
#if PLATFORM(MAC)
    m_runLoopObserver->invalidate();
#endif
    m_displayLinkIsActive = false;
}

void WebDisplayRefreshMonitor::adjustPreferredFramesPerSecond(FramesPerSecond preferredFramesPerSecond)
{
    LOG_WITH_STREAM(DisplayLink, stream << "[Web] WebDisplayRefreshMonitor::adjustPreferredFramesPerSecond for display link on display " << displayID() << " to " << preferredFramesPerSecond);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::SetDisplayLinkPreferredFramesPerSecond(m_observerID, displayID(), preferredFramesPerSecond), 0);
}

} // namespace WebKit

#endif // HAVE(DISPLAY_LINK)
