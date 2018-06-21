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
#include "DrawingArea.h"

#include "WebPage.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"

#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/DisplayRefreshMonitorManager.h>
#include <WebCore/RunLoopObserver.h>

using namespace WebCore;

namespace WebKit {

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)

class DisplayRefreshMonitorMac : public DisplayRefreshMonitor {
public:
    static Ref<DisplayRefreshMonitorMac> create(PlatformDisplayID displayID, WebPage& webPage)
    {
        return adoptRef(*new DisplayRefreshMonitorMac(displayID, webPage));
    }
    
    virtual ~DisplayRefreshMonitorMac();
    
    void displayLinkFired() override;
    bool requestRefreshCallback() override;
    
private:
    explicit DisplayRefreshMonitorMac(PlatformDisplayID, WebPage&);
    
    bool hasRequestedRefreshCallback() const override { return m_hasSentMessage; }

    Ref<WebPage> m_webPage;
    bool m_hasSentMessage { false };
    unsigned m_observerID;
    static unsigned m_counterID;
    std::unique_ptr<RunLoopObserver> m_runLoopObserver;
    bool m_firstCallbackInCurrentRunloop { false };
};

unsigned DisplayRefreshMonitorMac::m_counterID = 0;

DisplayRefreshMonitorMac::DisplayRefreshMonitorMac(PlatformDisplayID displayID, WebPage& webPage)
    : DisplayRefreshMonitor(displayID)
    , m_webPage(webPage)
    , m_observerID(++m_counterID)
{
}

DisplayRefreshMonitorMac::~DisplayRefreshMonitorMac()
{
    m_webPage->send(Messages::WebPageProxy::StopDisplayLink(m_observerID));
}

bool DisplayRefreshMonitorMac::requestRefreshCallback()
{
    if (!isActive())
        return false;

    if (!m_hasSentMessage) {
        m_webPage->send(Messages::WebPageProxy::StartDisplayLink(m_observerID));
        m_hasSentMessage = true;
        m_runLoopObserver = std::make_unique<RunLoopObserver>(kCFRunLoopEntry, [this]() {
            this->m_firstCallbackInCurrentRunloop = true;
        });
        m_runLoopObserver->schedule(CFRunLoopGetCurrent());
    }
    
    setIsScheduled(true);

    return true;
}

void DisplayRefreshMonitorMac::displayLinkFired()
{
    ASSERT(isMainThread());
    if (!m_firstCallbackInCurrentRunloop)
        return;
    m_firstCallbackInCurrentRunloop = false;

    // Since we are on the main thread, we can just call handleDisplayRefreshedNotificationOnMainThread here.
    handleDisplayRefreshedNotificationOnMainThread(this);
}

void DrawingArea::displayWasRefreshed()
{
    DisplayRefreshMonitorManager::sharedManager().displayWasUpdated();
}

RefPtr<WebCore::DisplayRefreshMonitor> DrawingArea::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return DisplayRefreshMonitorMac::create(displayID, m_webPage);
}
#endif

}
