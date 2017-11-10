/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "APIFrameInfo.h"
#include "APIObject.h"
#include "APIUserInitiatedAction.h"
#include "NavigationActionData.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/URL.h>

namespace API {

class NavigationAction final : public ObjectImpl<Object::Type::NavigationAction> {
public:
    static Ref<NavigationAction> create(WebKit::NavigationActionData&& navigationActionData, API::FrameInfo* sourceFrame, API::FrameInfo* targetFrame, std::optional<WTF::String> targetFrameName, WebCore::ResourceRequest&& request, const WebCore::URL& originalURL, bool shouldOpenAppLinks, RefPtr<UserInitiatedAction>&& userInitiatedAction)
    {
        return adoptRef(*new NavigationAction(WTFMove(navigationActionData), sourceFrame, targetFrame, targetFrameName, WTFMove(request), originalURL, shouldOpenAppLinks, WTFMove(userInitiatedAction)));
    }

    FrameInfo* sourceFrame() const { return m_sourceFrame.get(); }
    FrameInfo* targetFrame() const { return m_targetFrame.get(); }
    std::optional<WTF::String> targetFrameName() const { return m_targetFrameName; }

    const WebCore::ResourceRequest& request() const { return m_request; }
    const WebCore::URL& originalURL() const { return !m_originalURL.isNull() ? m_originalURL : m_request.url(); }

    WebCore::NavigationType navigationType() const { return m_navigationActionData.navigationType; }
    WebKit::WebEvent::Modifiers modifiers() const { return m_navigationActionData.modifiers; }
    WebKit::WebMouseEvent::Button mouseButton() const { return m_navigationActionData.mouseButton; }
    WebKit::WebMouseEvent::SyntheticClickType syntheticClickType() const { return m_navigationActionData.syntheticClickType; }
    WebCore::FloatPoint clickLocationInRootViewCoordinates() const { return m_navigationActionData.clickLocationInRootViewCoordinates; }
    bool canHandleRequest() const { return m_navigationActionData.canHandleRequest; }
    bool shouldOpenExternalSchemes() const { return m_navigationActionData.shouldOpenExternalURLsPolicy == WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow || m_navigationActionData.shouldOpenExternalURLsPolicy == WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes; }
    bool shouldOpenAppLinks() const { return m_shouldOpenAppLinks && m_navigationActionData.shouldOpenExternalURLsPolicy == WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow; }
    bool shouldPerformDownload() const { return !m_navigationActionData.downloadAttribute.isNull(); }
    bool isRedirect() const { return m_navigationActionData.isRedirect; }

    bool isProcessingUserGesture() const { return m_userInitiatedAction; }
    UserInitiatedAction* userInitiatedAction() const { return m_userInitiatedAction.get(); }

private:
NavigationAction(WebKit::NavigationActionData&& navigationActionData, API::FrameInfo* sourceFrame, API::FrameInfo* targetFrame, std::optional<WTF::String> targetFrameName, WebCore::ResourceRequest&& request, const WebCore::URL& originalURL, bool shouldOpenAppLinks, RefPtr<UserInitiatedAction>&& userInitiatedAction)
        : m_sourceFrame(sourceFrame)
        , m_targetFrame(targetFrame)
        , m_targetFrameName(targetFrameName)
        , m_request(WTFMove(request))
        , m_originalURL(originalURL)
        , m_shouldOpenAppLinks(shouldOpenAppLinks)
        , m_userInitiatedAction(WTFMove(userInitiatedAction))
        , m_navigationActionData(WTFMove(navigationActionData))
    {
    }

    RefPtr<FrameInfo> m_sourceFrame;
    RefPtr<FrameInfo> m_targetFrame;
    std::optional<WTF::String> m_targetFrameName;

    WebCore::ResourceRequest m_request;
    WebCore::URL m_originalURL;

    bool m_shouldOpenAppLinks;

    RefPtr<UserInitiatedAction> m_userInitiatedAction;

    WebKit::NavigationActionData m_navigationActionData;
};

} // namespace API
