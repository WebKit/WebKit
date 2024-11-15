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

#pragma once

#include <WebCore/NavigationIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace API {
class Navigation;
struct SubstituteData;
}

namespace WebCore {
class ResourceRequest;

enum class FrameLoadType : uint8_t;
}

namespace WebKit {

class WebPageProxy;
class WebBackForwardListItem;

class WebNavigationState : public CanMakeWeakPtr<WebNavigationState> {
    WTF_MAKE_TZONE_ALLOCATED(WebNavigationState);
public:
    explicit WebNavigationState(WebPageProxy&);
    ~WebNavigationState();

    void ref() const;
    void deref() const;

    Ref<API::Navigation> createBackForwardNavigation(WebCore::ProcessIdentifier, Ref<WebBackForwardListItem>&& targetItem, RefPtr<WebBackForwardListItem>&& currentItem, WebCore::FrameLoadType);
    Ref<API::Navigation> createLoadRequestNavigation(WebCore::ProcessIdentifier, WebCore::ResourceRequest&&, RefPtr<WebBackForwardListItem>&& currentItem);
    Ref<API::Navigation> createReloadNavigation(WebCore::ProcessIdentifier, RefPtr<WebBackForwardListItem>&& currentAndTargetItem);
    Ref<API::Navigation> createLoadDataNavigation(WebCore::ProcessIdentifier, std::unique_ptr<API::SubstituteData>&&);
    Ref<API::Navigation> createSimulatedLoadWithDataNavigation(WebCore::ProcessIdentifier, WebCore::ResourceRequest&&, std::unique_ptr<API::SubstituteData>&&, RefPtr<WebBackForwardListItem>&& currentItem);

    bool hasNavigation(WebCore::NavigationIdentifier navigationID) const { return m_navigations.contains(navigationID); }
    API::Navigation* navigation(WebCore::NavigationIdentifier navigationID) { return m_navigations.get(navigationID); }
    RefPtr<API::Navigation> takeNavigation(WebCore::NavigationIdentifier);
    void didDestroyNavigation(WebCore::ProcessIdentifier, WebCore::NavigationIdentifier);
    void clearAllNavigations();

    void clearNavigationsFromProcess(WebCore::ProcessIdentifier);

    using NavigationMap = HashMap<WebCore::NavigationIdentifier, RefPtr<API::Navigation>>;

private:
    WeakRef<WebPageProxy> m_page;
    NavigationMap m_navigations;
};

} // namespace WebKit
