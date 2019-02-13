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

#include "APIObject.h"
#include "DataReference.h"
#include "FrameInfoData.h"
#include "NavigationActionData.h"
#include "WebBackForwardListItem.h"
#include <WebCore/AdClickAttribution.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>

namespace WebCore {
enum class FrameLoadType : uint8_t;
}

namespace WebKit {
class WebNavigationState;
}

namespace API {

struct SubstituteData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SubstituteData(Vector<uint8_t>&& content, const WTF::String& MIMEType, const WTF::String& encoding, const WTF::String& baseURL, API::Object* userData)
        : content(WTFMove(content))
        , MIMEType(MIMEType)
        , encoding(encoding)
        , baseURL(baseURL)
        , userData(userData)
    { }

    Vector<uint8_t> content;
    WTF::String MIMEType;
    WTF::String encoding;
    WTF::String baseURL;
    RefPtr<API::Object> userData;
};

class Navigation : public ObjectImpl<Object::Type::Navigation> {
    WTF_MAKE_NONCOPYABLE(Navigation);
public:
    static Ref<Navigation> create(WebKit::WebNavigationState& state)
    {
        return adoptRef(*new Navigation(state));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebKit::WebBackForwardListItem& targetItem, WebKit::WebBackForwardListItem* fromItem, WebCore::FrameLoadType backForwardFrameLoadType)
    {
        return adoptRef(*new Navigation(state, targetItem, fromItem, backForwardFrameLoadType));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ResourceRequest&& request, WebKit::WebBackForwardListItem* fromItem)
    {
        return adoptRef(*new Navigation(state, WTFMove(request), fromItem));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, std::unique_ptr<SubstituteData>&& substituteData)
    {
        return adoptRef(*new Navigation(state, WTFMove(substituteData)));
    }

    virtual ~Navigation();

    uint64_t navigationID() const { return m_navigationID; }

    const WebCore::ResourceRequest& originalRequest() const { return m_originalRequest; }
    void setCurrentRequest(WebCore::ResourceRequest&&, WebCore::ProcessIdentifier);
    const WebCore::ResourceRequest& currentRequest() const { return m_currentRequest; }
    Optional<WebCore::ProcessIdentifier> currentRequestProcessIdentifier() const { return m_currentRequestProcessIdentifier; }

    bool currentRequestIsRedirect() const { return m_lastNavigationAction.isRedirect; }

    WebKit::WebBackForwardListItem* targetItem() const { return m_targetItem.get(); }
    WebKit::WebBackForwardListItem* fromItem() const { return m_fromItem.get(); }
    Optional<WebCore::FrameLoadType> backForwardFrameLoadType() const { return m_backForwardFrameLoadType; }

    void appendRedirectionURL(const WTF::URL&);
    Vector<WTF::URL> takeRedirectChain() { return WTFMove(m_redirectChain); }

    bool wasUserInitiated() const { return !!m_lastNavigationAction.userGestureTokenIdentifier; }

    bool shouldForceDownload() const { return !m_lastNavigationAction.downloadAttribute.isNull(); }

    bool isSystemPreview() const
    {
#if USE(SYSTEM_PREVIEW)
        return currentRequest().isSystemPreview();
#else
        return false;
#endif
    }

    bool treatAsSameOriginNavigation() const { return m_lastNavigationAction.treatAsSameOriginNavigation; }
    bool hasOpenedFrames() const { return m_lastNavigationAction.hasOpenedFrames; }
    bool openedByDOMWithOpener() const { return m_lastNavigationAction.openedByDOMWithOpener; }
    const WebCore::SecurityOriginData& requesterOrigin() const { return m_lastNavigationAction.requesterOrigin; }

    WebCore::LockHistory lockHistory() const { return m_lastNavigationAction.lockHistory; }
    WebCore::LockBackForwardList lockBackForwardList() const { return m_lastNavigationAction.lockBackForwardList; }

    WTF::String clientRedirectSourceForHistory() const { return m_lastNavigationAction.clientRedirectSourceForHistory; }

    void setLastNavigationAction(const WebKit::NavigationActionData& navigationAction) { m_lastNavigationAction = navigationAction; }
    const WebKit::NavigationActionData& lastNavigationAction() const { return m_lastNavigationAction; }

    void setOriginatingFrameInfo(const WebKit::FrameInfoData& frameInfo) { m_originatingFrameInfo = frameInfo; }
    const WebKit::FrameInfoData& originatingFrameInfo() const { return m_originatingFrameInfo; }

    void setDestinationFrameSecurityOrigin(const WebCore::SecurityOriginData& origin) { m_destinationFrameSecurityOrigin = origin; }
    const WebCore::SecurityOriginData& destinationFrameSecurityOrigin() const { return m_destinationFrameSecurityOrigin; }

#if !LOG_DISABLED
    const char* loggingString() const;
#endif

    const std::unique_ptr<SubstituteData>& substituteData() const { return m_substituteData; }

    const Optional<WebCore::AdClickAttribution>& adClickAttribution() const { return m_lastNavigationAction.adClickAttribution; }

private:
    explicit Navigation(WebKit::WebNavigationState&);
    Navigation(WebKit::WebNavigationState&, WebCore::ResourceRequest&&, WebKit::WebBackForwardListItem* fromItem);
    Navigation(WebKit::WebNavigationState&, WebKit::WebBackForwardListItem& targetItem, WebKit::WebBackForwardListItem* fromItem, WebCore::FrameLoadType);
    Navigation(WebKit::WebNavigationState&, std::unique_ptr<SubstituteData>&&);

    uint64_t m_navigationID;
    WebCore::ResourceRequest m_originalRequest;
    WebCore::ResourceRequest m_currentRequest;
    Optional<WebCore::ProcessIdentifier> m_currentRequestProcessIdentifier;
    Vector<WTF::URL> m_redirectChain;

    RefPtr<WebKit::WebBackForwardListItem> m_targetItem;
    RefPtr<WebKit::WebBackForwardListItem> m_fromItem;
    Optional<WebCore::FrameLoadType> m_backForwardFrameLoadType;
    std::unique_ptr<SubstituteData> m_substituteData;
    WebKit::NavigationActionData m_lastNavigationAction;
    WebKit::FrameInfoData m_originatingFrameInfo;
    WebCore::SecurityOriginData m_destinationFrameSecurityOrigin;
};

} // namespace API
