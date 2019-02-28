/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebProcessLifetimeTracker.h"

#include "WebPageProxy.h"
#include "WebProcessLifetimeObserver.h"
#include "WebProcessProxy.h"

namespace WebKit {

WebProcessLifetimeTracker::WebProcessLifetimeTracker(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
{
}

WebProcessLifetimeTracker::~WebProcessLifetimeTracker()
{
}

void WebProcessLifetimeTracker::addObserver(WebProcessLifetimeObserver& observer)
{
    ASSERT(!m_observers.contains(&observer));

    m_observers.add(&observer);

    observer.webPageWasAdded(m_webPageProxy);

    if (processIsRunning(m_webPageProxy.process()))
        observer.addWebPage(m_webPageProxy, m_webPageProxy.process());
}

void WebProcessLifetimeTracker::webPageEnteringWebProcess(WebProcessProxy& process)
{
    ASSERT(processIsRunning(process));

    for (auto& observer : m_observers)
        observer->addWebPage(m_webPageProxy, process);
}

void WebProcessLifetimeTracker::webPageLeavingWebProcess(WebProcessProxy& process)
{
    ASSERT(processIsRunning(process));

    for (auto& observer : m_observers)
        observer->removeWebPage(m_webPageProxy, process);
}

void WebProcessLifetimeTracker::pageWasInvalidated()
{
    if (!processIsRunning(m_webPageProxy.process()))
        return;

    for (auto& observer : m_observers) {
        observer->removeWebPage(m_webPageProxy, m_webPageProxy.process());

        observer->webPageWasInvalidated(m_webPageProxy);
    }
}

bool WebProcessLifetimeTracker::processIsRunning(WebProcessProxy& process)
{
    return process.state() == WebProcessProxy::State::Running;
}

}
