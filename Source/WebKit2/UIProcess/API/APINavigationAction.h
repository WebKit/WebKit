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

#ifndef APINavigationAction_h
#define APINavigationAction_h

#include "APIFrameInfo.h"
#include "APIObject.h"
#include "NavigationActionData.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/URL.h>

namespace API {

class FrameInfo;

class NavigationAction final : public ObjectImpl<Object::Type::NavigationAction> {
public:
    static Ref<NavigationAction> create(const WebKit::NavigationActionData& navigationActionData, API::FrameInfo* sourceFrame, API::FrameInfo* targetFrame, const WebCore::ResourceRequest& request, const WebCore::URL& originalURL, bool shouldOpenAppLinks)
    {
        return adoptRef(*new NavigationAction(navigationActionData, sourceFrame, targetFrame, request, originalURL, shouldOpenAppLinks));
    }

    NavigationAction(const WebKit::NavigationActionData& navigationActionData, API::FrameInfo* sourceFrame, API::FrameInfo* targetFrame, const WebCore::ResourceRequest& request, const WebCore::URL& originalURL, bool shouldOpenAppLinks)
        : m_sourceFrame(sourceFrame)
        , m_targetFrame(targetFrame)
        , m_request(request)
        , m_originalURL(originalURL)
        , m_shouldOpenAppLinks(shouldOpenAppLinks)
        , m_navigationActionData(navigationActionData)
    {
    }

    FrameInfo* sourceFrame() const { return m_sourceFrame.get(); }
    FrameInfo* targetFrame() const { return m_targetFrame.get(); }

    const WebCore::ResourceRequest& request() const { return m_request; }
    const WebCore::URL& originalURL() const { return m_originalURL; }

    WebCore::NavigationType navigationType() const { return m_navigationActionData.navigationType; }
    WebKit::WebEvent::Modifiers modifiers() const { return m_navigationActionData.modifiers; }
    WebKit::WebMouseEvent::Button mouseButton() const { return m_navigationActionData.mouseButton; }
    bool isProcessingUserGesture() const { return m_navigationActionData.isProcessingUserGesture; }
    bool canHandleRequest() const { return m_navigationActionData.canHandleRequest; }
    bool shouldOpenExternalSchemes() const { return m_navigationActionData.shouldOpenExternalURLsPolicy == WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow || m_navigationActionData.shouldOpenExternalURLsPolicy == WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes; }
    bool shouldOpenAppLinks() const { return m_shouldOpenAppLinks && m_navigationActionData.shouldOpenExternalURLsPolicy == WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow; }
    bool shouldPerformDownload() const { return !m_navigationActionData.downloadAttribute.isNull(); }

private:
    RefPtr<FrameInfo> m_sourceFrame;
    RefPtr<FrameInfo> m_targetFrame;

    WebCore::ResourceRequest m_request;
    WebCore::URL m_originalURL;

    bool m_shouldOpenAppLinks;

    WebKit::NavigationActionData m_navigationActionData;
};

} // namespace API

#endif // APINavigationAction_h
