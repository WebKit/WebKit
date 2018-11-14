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

#pragma once

#include <wtf/HashMap.h>
#include <wtf/Ref.h>

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

class WebNavigationState {
public:
    explicit WebNavigationState();
    ~WebNavigationState();

    Ref<API::Navigation> createBackForwardNavigation(WebBackForwardListItem& targetItem, WebBackForwardListItem* currentItem, WebCore::FrameLoadType);
    Ref<API::Navigation> createLoadRequestNavigation(WebCore::ResourceRequest&&, WebBackForwardListItem* currentItem);
    Ref<API::Navigation> createReloadNavigation();
    Ref<API::Navigation> createLoadDataNavigation(std::unique_ptr<API::SubstituteData>&&);

    API::Navigation* navigation(uint64_t navigationID);
    RefPtr<API::Navigation> takeNavigation(uint64_t navigationID);
    void didDestroyNavigation(uint64_t navigationID);
    void clearAllNavigations();

    uint64_t generateNavigationID()
    {
        return ++m_navigationID;
    }

private:
    HashMap<uint64_t, RefPtr<API::Navigation>> m_navigations;
    uint64_t m_navigationID { 0 };
};

} // namespace WebKit
