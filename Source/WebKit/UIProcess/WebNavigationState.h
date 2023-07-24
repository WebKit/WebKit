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

#include <WebCore/ProcessIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
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
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebNavigationState();
    ~WebNavigationState();

    Ref<API::Navigation> createBackForwardNavigation(WebCore::ProcessIdentifier, WebBackForwardListItem& targetItem, WebBackForwardListItem* currentItem, WebCore::FrameLoadType);
    Ref<API::Navigation> createLoadRequestNavigation(WebCore::ProcessIdentifier, WebCore::ResourceRequest&&, WebBackForwardListItem* currentItem);
    Ref<API::Navigation> createReloadNavigation(WebCore::ProcessIdentifier, WebBackForwardListItem* currentAndTargetItem);
    Ref<API::Navigation> createLoadDataNavigation(WebCore::ProcessIdentifier, std::unique_ptr<API::SubstituteData>&&);
    Ref<API::Navigation> createSimulatedLoadWithDataNavigation(WebCore::ProcessIdentifier, WebCore::ResourceRequest&&, std::unique_ptr<API::SubstituteData>&&, WebBackForwardListItem* currentItem);

    bool hasNavigation(uint64_t navigationID) const { return m_navigations.contains(navigationID); }
    API::Navigation* navigation(uint64_t navigationID);
    RefPtr<API::Navigation> takeNavigation(uint64_t navigationID);
    void didDestroyNavigation(WebCore::ProcessIdentifier, uint64_t navigationID);
    void clearAllNavigations();

    void clearNavigationsFromProcess(WebCore::ProcessIdentifier);

    uint64_t generateNavigationID()
    {
        return ++m_navigationID;
    }

    using NavigationMap = HashMap<uint64_t, RefPtr<API::Navigation>>;

private:
    NavigationMap m_navigations;
    uint64_t m_navigationID { 0 };
};

} // namespace WebKit
