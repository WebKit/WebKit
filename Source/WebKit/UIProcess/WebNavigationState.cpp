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
#include "WebPageProxy.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebNavigationState);

WebNavigationState::WebNavigationState()
{
}

WebNavigationState::~WebNavigationState()
{
}

Ref<API::Navigation> WebNavigationState::createLoadRequestNavigation(WebCore::ProcessIdentifier processID, ResourceRequest&& request, RefPtr<WebBackForwardListItem>&& currentItem)
{
    auto navigation = API::Navigation::create(processID, WTFMove(request), WTFMove(currentItem));

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createBackForwardNavigation(WebCore::ProcessIdentifier processID, Ref<WebBackForwardListItem>&& targetItem, RefPtr<WebBackForwardListItem>&& currentItem, FrameLoadType frameLoadType)
{
    auto navigation = API::Navigation::create(processID, WTFMove(targetItem), WTFMove(currentItem), frameLoadType);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createReloadNavigation(WebCore::ProcessIdentifier processID, RefPtr<WebBackForwardListItem>&& currentAndTargetItem)
{
    auto navigation = API::Navigation::create(processID, WTFMove(currentAndTargetItem));

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createLoadDataNavigation(WebCore::ProcessIdentifier processID, std::unique_ptr<API::SubstituteData>&& substituteData)
{
    auto navigation = API::Navigation::create(processID, WTFMove(substituteData));

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createSimulatedLoadWithDataNavigation(WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& request, std::unique_ptr<API::SubstituteData>&& substituteData, RefPtr<WebBackForwardListItem>&& currentItem)
{
    auto navigation = API::Navigation::create(processID, WTFMove(request), WTFMove(substituteData), WTFMove(currentItem));

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

API::Navigation* WebNavigationState::navigation(WebCore::NavigationIdentifier navigationID)
{
    return m_navigations.get(navigationID);
}

RefPtr<API::Navigation> WebNavigationState::takeNavigation(WebCore::NavigationIdentifier navigationID)
{
    ASSERT(m_navigations.contains(navigationID));
    
    return m_navigations.take(navigationID);
}

void WebNavigationState::didDestroyNavigation(WebCore::ProcessIdentifier processID, WebCore::NavigationIdentifier navigationID)
{
    auto it = m_navigations.find(navigationID);
    if (it != m_navigations.end() && (*it).value->processID() == processID)
        m_navigations.remove(it);
}

void WebNavigationState::clearAllNavigations()
{
    m_navigations.clear();
}

void WebNavigationState::clearNavigationsFromProcess(WebCore::ProcessIdentifier processID)
{
    Vector<WebCore::NavigationIdentifier> navigationIDsToRemove;
    for (auto& navigation : m_navigations.values()) {
        if (navigation->processID() == processID)
            navigationIDsToRemove.append(navigation->navigationID());
    }
    for (auto navigationID : navigationIDsToRemove)
        m_navigations.remove(navigationID);
}

} // namespace WebKit
