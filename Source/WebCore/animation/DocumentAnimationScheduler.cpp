/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentAnimationScheduler.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "DisplayRefreshMonitor.h"
#include "DisplayRefreshMonitorManager.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "Page.h"
#include "ScriptedAnimationController.h"

namespace WebCore {

Ref<DocumentAnimationScheduler> DocumentAnimationScheduler::create(Document& document, PlatformDisplayID displayID)
{
    return adoptRef(*new DocumentAnimationScheduler(document, displayID));
}

DocumentAnimationScheduler::DocumentAnimationScheduler(Document& document, PlatformDisplayID displayID)
    : m_document(&document)
{
    windowScreenDidChange(displayID);
}

DocumentAnimationScheduler::~DocumentAnimationScheduler() = default;

void DocumentAnimationScheduler::detachFromDocument()
{
    m_document = nullptr;
}

bool DocumentAnimationScheduler::scheduleWebAnimationsResolution()
{
    m_scheduledWebAnimationsResolution = true;
    return DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
}

void DocumentAnimationScheduler::unscheduleWebAnimationsResolution()
{
    m_scheduledWebAnimationsResolution = false;

    if (!m_scheduledScriptedAnimationResolution)
        DisplayRefreshMonitorManager::sharedManager().unregisterClient(*this);
}

bool DocumentAnimationScheduler::scheduleScriptedAnimationResolution()
{
    m_scheduledScriptedAnimationResolution = true;
    return DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
}

void DocumentAnimationScheduler::displayRefreshFired()
{
    if (!m_document || !m_document->domWindow())
        return;

    // This object could be deleted after scripts in the the requestAnimationFrame callbacks are executed.
    auto protectedThis = makeRef(*this);

    m_isFiring = true;
    m_lastTimestamp = Seconds(m_document->domWindow()->nowTimestamp());

    if (m_scheduledWebAnimationsResolution) {
        m_scheduledWebAnimationsResolution = false;
        m_document->timeline().documentAnimationSchedulerDidFire();
    }

    if (m_scheduledScriptedAnimationResolution) {
        m_scheduledScriptedAnimationResolution = false;
        if (auto* scriptedAnimationController = m_document->scriptedAnimationController())
            scriptedAnimationController->documentAnimationSchedulerDidFire();
    }

    m_isFiring = false;
}

void DocumentAnimationScheduler::windowScreenDidChange(PlatformDisplayID displayID)
{
    DisplayRefreshMonitorManager::sharedManager().windowScreenDidChange(displayID, *this);
}

RefPtr<DisplayRefreshMonitor> DocumentAnimationScheduler::createDisplayRefreshMonitor(PlatformDisplayID displayID) const
{
    if (!m_document || !m_document->page())
        return nullptr;

    if (auto monitor = m_document->page()->chrome().client().createDisplayRefreshMonitor(displayID))
        return monitor;

    return DisplayRefreshMonitor::createDefaultDisplayRefreshMonitor(displayID);
}

} // namespace WebCore

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
