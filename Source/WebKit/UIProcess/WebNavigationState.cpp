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

namespace WebKit {
using namespace WebCore;

WebNavigationState::WebNavigationState()
{
}

WebNavigationState::~WebNavigationState()
{
}

Ref<API::Navigation> WebNavigationState::createLoadRequestNavigation(WebCore::ProcessIdentifier processID, ResourceRequest&& request, WebBackForwardListItem* currentItem)
{
    auto navigation = API::Navigation::create(*this, processID, WTFMove(request), currentItem);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createBackForwardNavigation(WebCore::ProcessIdentifier processID, WebBackForwardListItem& targetItem, WebBackForwardListItem* currentItem, FrameLoadType frameLoadType)
{
    auto navigation = API::Navigation::create(*this, processID, targetItem, currentItem, frameLoadType);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createReloadNavigation(WebCore::ProcessIdentifier processID, WebBackForwardListItem* currentAndTargetItem)
{
    auto navigation = API::Navigation::create(*this, processID, currentAndTargetItem);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createLoadDataNavigation(WebCore::ProcessIdentifier processID, std::unique_ptr<API::SubstituteData>&& substituteData)
{
    auto navigation = API::Navigation::create(*this, processID, WTFMove(substituteData));

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createSimulatedLoadWithDataNavigation(WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& request, std::unique_ptr<API::SubstituteData>&& substituteData, WebBackForwardListItem* currentItem)
{
    auto navigation = API::Navigation::create(*this, processID, WTFMove(request), WTFMove(substituteData), currentItem);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

API::Navigation* WebNavigationState::navigation(uint64_t navigationID)
{
    ASSERT(navigationID);
    ASSERT(m_navigations.contains(navigationID));

    return m_navigations.get(navigationID);
}

RefPtr<API::Navigation> WebNavigationState::takeNavigation(uint64_t navigationID)
{
    ASSERT(navigationID);
    ASSERT(m_navigations.contains(navigationID));
    
    return m_navigations.take(navigationID);
}

void WebNavigationState::didDestroyNavigation(WebCore::ProcessIdentifier processID, uint64_t navigationID)
{
    ASSERT(navigationID);
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
    Vector<uint64_t> navigationIDsToRemove;
    for (auto& navigation : m_navigations.values()) {
        if (navigation->processID() == processID)
            navigationIDsToRemove.append(navigation->navigationID());
    }
    for (auto navigationID : navigationIDsToRemove)
        m_navigations.remove(navigationID);
}

} // namespace WebKit
