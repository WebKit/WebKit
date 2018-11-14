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
#include "WebBackForwardListItem.h"
#include <WebCore/Process.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
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
    std::optional<WebCore::ProcessIdentifier> currentRequestProcessIdentifier() const { return m_currentRequestProcessIdentifier; }

    void setCurrentRequestIsRedirect(bool isRedirect) { m_isRedirect = isRedirect; }
    bool currentRequestIsRedirect() const { return m_isRedirect; }

    void setTargetItem(WebKit::WebBackForwardListItem& item) { m_targetItem = &item; }
    WebKit::WebBackForwardListItem* targetItem() const { return m_targetItem.get(); }
    WebKit::WebBackForwardListItem* fromItem() const { return m_fromItem.get(); }
    std::optional<WebCore::FrameLoadType> backForwardFrameLoadType() const { return m_backForwardFrameLoadType; }

    void appendRedirectionURL(const WebCore::URL&);
    Vector<WebCore::URL> takeRedirectChain() { return WTFMove(m_redirectChain); }

    void setWasUserInitiated(bool value) { m_wasUserInitiated = value; }
    bool wasUserInitiated() const { return m_wasUserInitiated; }

    void setShouldForceDownload(bool value) { m_shouldForceDownload = value; }
    bool shouldForceDownload() const { return m_shouldForceDownload; }

    void setTreatAsSameOriginNavigation(bool value) { m_treatAsSameOriginNavigation = value; }
    bool treatAsSameOriginNavigation() const { return m_treatAsSameOriginNavigation; }

    void setHasOpenedFrames(bool value) { m_hasOpenedFrames = value; }
    bool hasOpenedFrames() const { return m_hasOpenedFrames; }

    bool openedViaWindowOpenWithOpener() const { return m_openedViaWindowOpenWithOpener; }
    void setOpenedViaWindowOpenWithOpener() { m_openedViaWindowOpenWithOpener = true; }

    void setOpener(const std::optional<std::pair<uint64_t, uint64_t>>& opener) { m_opener = opener; }
    const std::optional<std::pair<uint64_t, uint64_t>>& opener() const { return m_opener; }

    void setRequesterOrigin(const WebCore::SecurityOriginData& origin) { m_requesterOrigin = origin; }
    const WebCore::SecurityOriginData& requesterOrigin() const { return m_requesterOrigin; }

    void setLockHistory(WebCore::LockHistory lockHistory) { m_lockHistory = lockHistory; }
    WebCore::LockHistory lockHistory() const { return m_lockHistory; }

    void setLockBackForwardList(WebCore::LockBackForwardList lockBackForwardList) { m_lockBackForwardList = lockBackForwardList; }
    WebCore::LockBackForwardList lockBackForwardList() const { return m_lockBackForwardList; }

    void setClientRedirectSourceForHistory(const WTF::String& clientRedirectSourceForHistory) { m_clientRedirectSourceForHistory = clientRedirectSourceForHistory; }
    WTF::String clientRedirectSourceForHistory() const { return m_clientRedirectSourceForHistory; }

#if !LOG_DISABLED
    const char* loggingString() const;
#endif

    const std::unique_ptr<SubstituteData>& substituteData() const { return m_substituteData; }

private:
    explicit Navigation(WebKit::WebNavigationState&);
    Navigation(WebKit::WebNavigationState&, WebCore::ResourceRequest&&, WebKit::WebBackForwardListItem* fromItem);
    Navigation(WebKit::WebNavigationState&, WebKit::WebBackForwardListItem& targetItem, WebKit::WebBackForwardListItem* fromItem, WebCore::FrameLoadType);
    Navigation(WebKit::WebNavigationState&, std::unique_ptr<SubstituteData>&&);

    uint64_t m_navigationID;
    WebCore::ResourceRequest m_originalRequest;
    WebCore::ResourceRequest m_currentRequest;
    std::optional<WebCore::ProcessIdentifier> m_currentRequestProcessIdentifier;
    Vector<WebCore::URL> m_redirectChain;
    bool m_wasUserInitiated { true };
    bool m_shouldForceDownload { false };
    bool m_isRedirect { false };

    RefPtr<WebKit::WebBackForwardListItem> m_targetItem;
    RefPtr<WebKit::WebBackForwardListItem> m_fromItem;
    std::optional<WebCore::FrameLoadType> m_backForwardFrameLoadType;
    bool m_treatAsSameOriginNavigation { false };
    bool m_hasOpenedFrames { false };
    bool m_openedViaWindowOpenWithOpener { false };
    std::optional<std::pair<uint64_t, uint64_t>> m_opener;
    WebCore::SecurityOriginData m_requesterOrigin;
    WebCore::LockHistory m_lockHistory;
    WebCore::LockBackForwardList m_lockBackForwardList;
    WTF::String m_clientRedirectSourceForHistory;
    std::unique_ptr<SubstituteData> m_substituteData;
};

} // namespace API
