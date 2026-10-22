/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "WebNavigationState.h"

#include "APINavigation.h"
#include "WebAutomationSession.h"
#include "WebPageProxy.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebNavigationState);

WebNavigationState::WebNavigationState(WebPageProxy& page)
    : m_page(page)
{
}

WebNavigationState::~WebNavigationState() = default;

Ref<API::Navigation> WebNavigationState::createLoadRequestNavigation(WebCore::ProcessIdentifier processID, ResourceRequest&& request, RefPtr<WebBackForwardListItem>&& currentItem, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    Ref navigation = API::Navigation::create(processID, WTF::move(request), WTF::move(currentItem), navigationID);

    m_navigations.set(navigation->navigationID(), navigation);

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createBackForwardNavigation(WebCore::ProcessIdentifier processID, Ref<WebBackForwardListFrameItem>&& targetFrameItem, RefPtr<WebBackForwardListItem>&& currentItem, FrameLoadType frameLoadType, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    Ref navigation = API::Navigation::create(processID, WTF::move(targetFrameItem), WTF::move(currentItem), frameLoadType, navigationID);

    m_navigations.set(navigation->navigationID(), navigation);

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createReloadNavigation(WebCore::ProcessIdentifier processID, RefPtr<WebBackForwardListItem>&& currentAndTargetItem)
{
    Ref navigation = API::Navigation::create(processID, WTF::move(currentAndTargetItem));

    m_navigations.set(navigation->navigationID(), navigation);

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createLoadDataNavigation(WebCore::ProcessIdentifier processID, std::unique_ptr<API::SubstituteData>&& substituteData)
{
    Ref navigation = API::Navigation::create(processID, WTF::move(substituteData));

    m_navigations.set(navigation->navigationID(), navigation);

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createSimulatedLoadWithDataNavigation(WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& request, std::unique_ptr<API::SubstituteData>&& substituteData, RefPtr<WebBackForwardListItem>&& currentItem)
{
    Ref navigation = API::Navigation::create(processID, WTF::move(request), WTF::move(substituteData), WTF::move(currentItem));

    m_navigations.set(navigation->navigationID(), navigation);

    return navigation;
}

API::Navigation* WebNavigationState::navigation(WebCore::NavigationIdentifier navigationID)
{
    return m_navigations.get(navigationID);
}

RefPtr<API::Navigation> WebNavigationState::takeNavigation(WebCore::NavigationIdentifier navigationID)
{
    ASSERT(m_navigations.contains(navigationID));

    RefPtr navigation = m_navigations.take(navigationID);
    if (navigation)
        navigation->clearAgentClusterKeyingReservation();
#if ENABLE(WEBDRIVER_BIDI)
    // The web process still owns the navigation until it reports it destroyed.
    if (navigation && protect(m_page.get())->activeAutomationSession())
        m_takenNavigationsAwaitingRelease.set(navigationID, navigation->processID());
#endif
    return navigation;
}

void WebNavigationState::didDestroyNavigation(WebCore::ProcessIdentifier processID, WebCore::NavigationIdentifier navigationID)
{
    auto it = m_navigations.find(navigationID);
    if (it != m_navigations.end() && (*it).value->processID() == processID) {
        protect(it->value)->clearAgentClusterKeyingReservation();
        m_navigations.remove(it);
        didReleaseNavigation(navigationID);
        return;
    }
#if ENABLE(WEBDRIVER_BIDI)
    if (auto taken = m_takenNavigationsAwaitingRelease.find(navigationID); taken != m_takenNavigationsAwaitingRelease.end() && taken->value == processID) {
        m_takenNavigationsAwaitingRelease.remove(taken);
        didReleaseNavigation(navigationID);
    }
#endif
}

void WebNavigationState::clearAllNavigations()
{
    for (Ref navigation : m_navigations.values())
        navigation->clearAgentClusterKeyingReservation();
    auto navigationIDs = copyToVector(m_navigations.keys());
    m_navigations.clear();
#if ENABLE(WEBDRIVER_BIDI)
    navigationIDs.appendVector(copyToVector(m_takenNavigationsAwaitingRelease.keys()));
    m_takenNavigationsAwaitingRelease.clear();
#endif
    for (auto navigationID : navigationIDs)
        didReleaseNavigation(navigationID);
}

void WebNavigationState::clearNavigationsFromProcess(WebCore::ProcessIdentifier processID)
{
    Vector<WebCore::NavigationIdentifier> releasedNavigationIDs;
    m_navigations.removeIf([&](auto& entry) {
        if (entry.value->processID() != processID)
            return false;
        protect(entry.value)->clearAgentClusterKeyingReservation();
        releasedNavigationIDs.append(entry.key);
        return true;
    });
#if ENABLE(WEBDRIVER_BIDI)
    m_takenNavigationsAwaitingRelease.removeIf([&](auto& entry) {
        if (entry.value != processID)
            return false;
        releasedNavigationIDs.append(entry.key);
        return true;
    });
#endif
    for (auto navigationID : releasedNavigationIDs)
        didReleaseNavigation(navigationID);
}

void WebNavigationState::didReleaseNavigation(WebCore::NavigationIdentifier navigationID)
{
#if ENABLE(WEBDRIVER_BIDI)
    // No further report for this navigation can reach the UI process from the process that owned it.
    if (RefPtr automationSession = protect(m_page.get())->activeAutomationSession())
        automationSession->navigationReleased(navigationID);
#else
    UNUSED_PARAM(navigationID);
#endif
}

void WebNavigationState::ref() const
{
    m_page->ref();
}

void WebNavigationState::deref() const
{
    m_page->deref();
}

} // namespace WebKit
