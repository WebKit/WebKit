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

Ref<API::Navigation> WebNavigationState::createLoadRequestNavigation(ResourceRequest&& request, WebBackForwardListItem* currentItem)
{
    auto navigation = API::Navigation::create(*this, WTFMove(request), currentItem);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createBackForwardNavigation(WebBackForwardListItem& targetItem, WebBackForwardListItem* currentItem, FrameLoadType frameLoadType)
{
    auto navigation = API::Navigation::create(*this, targetItem, currentItem, frameLoadType);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createReloadNavigation()
{
    auto navigation = API::Navigation::create(*this);

    m_navigations.set(navigation->navigationID(), navigation.ptr());

    return navigation;
}

Ref<API::Navigation> WebNavigationState::createLoadDataNavigation(std::unique_ptr<API::SubstituteData>&& substituteData)
{
    auto navigation = API::Navigation::create(*this, WTFMove(substituteData));

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

void WebNavigationState::didDestroyNavigation(uint64_t navigationID)
{
    ASSERT(navigationID);

    m_navigations.remove(navigationID);
}

void WebNavigationState::clearAllNavigations()
{
    m_navigations.clear();
}

} // namespace WebKit
