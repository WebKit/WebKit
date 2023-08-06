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

#include "APIObject.h"
#include "APIWebsitePolicies.h"
#include "DataReference.h"
#include "FrameInfoData.h"
#include "NavigationActionData.h"
#include "ProcessThrottler.h"
#include "WebBackForwardListItem.h"
#include "WebContentMode.h"
#include <WebCore/AdvancedPrivacyProtections.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SubstituteData.h>
#include <wtf/Ref.h>

namespace WebCore {
enum class FrameLoadType : uint8_t;
class ResourceResponse;
}

namespace WebKit {
class WebNavigationState;
}

namespace API {

struct SubstituteData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    SubstituteData(Vector<uint8_t>&& content, const WTF::String& MIMEType, const WTF::String& encoding, const WTF::String& baseURL, API::Object* userData, WebCore::SubstituteData::SessionHistoryVisibility sessionHistoryVisibility = WebCore::SubstituteData::SessionHistoryVisibility::Hidden)
        : content(WTFMove(content))
        , MIMEType(MIMEType)
        , encoding(encoding)
        , baseURL(baseURL)
        , userData(userData)
        , sessionHistoryVisibility(sessionHistoryVisibility)
    { }

    SubstituteData(Vector<uint8_t>&& content, const WebCore::ResourceResponse&, WebCore::SubstituteData::SessionHistoryVisibility);

    Vector<uint8_t> content;
    WTF::String MIMEType;
    WTF::String encoding;
    WTF::String baseURL;
    RefPtr<API::Object> userData;
    WebCore::SubstituteData::SessionHistoryVisibility sessionHistoryVisibility { WebCore::SubstituteData::SessionHistoryVisibility::Hidden };
};

class Navigation : public ObjectImpl<Object::Type::Navigation> {
    WTF_MAKE_NONCOPYABLE(Navigation);
public:
    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ProcessIdentifier processID, WebKit::WebBackForwardListItem* currentAndTargetItem)
    {
        return adoptRef(*new Navigation(state, processID, currentAndTargetItem));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ProcessIdentifier processID, WebKit::WebBackForwardListItem& targetItem, WebKit::WebBackForwardListItem* fromItem, WebCore::FrameLoadType backForwardFrameLoadType)
    {
        return adoptRef(*new Navigation(state, processID, targetItem, fromItem, backForwardFrameLoadType));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& request, WebKit::WebBackForwardListItem* fromItem)
    {
        return adoptRef(*new Navigation(state, processID, WTFMove(request), fromItem));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ProcessIdentifier processID, std::unique_ptr<SubstituteData>&& substituteData)
    {
        return adoptRef(*new Navigation(state, processID, WTFMove(substituteData)));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& simulatedRequest, std::unique_ptr<SubstituteData>&& substituteData, WebKit::WebBackForwardListItem* fromItem)
    {
        return adoptRef(*new Navigation(state, processID, WTFMove(simulatedRequest), WTFMove(substituteData), fromItem));
    }

    virtual ~Navigation();

    uint64_t navigationID() const { return m_navigationID; }

    const WebCore::ResourceRequest& originalRequest() const { return m_originalRequest; }
    void setCurrentRequest(WebCore::ResourceRequest&&, WebCore::ProcessIdentifier);
    const WebCore::ResourceRequest& currentRequest() const { return m_currentRequest; }
    std::optional<WebCore::ProcessIdentifier> currentRequestProcessIdentifier() const { return m_currentRequestProcessIdentifier; }

    bool currentRequestIsRedirect() const { return !m_lastNavigationAction.redirectResponse.isNull(); }

    WebKit::WebBackForwardListItem* targetItem() const { return m_targetItem.get(); }
    WebKit::WebBackForwardListItem* fromItem() const { return m_fromItem.get(); }
    std::optional<WebCore::FrameLoadType> backForwardFrameLoadType() const { return m_backForwardFrameLoadType; }
    WebKit::WebBackForwardListItem* reloadItem() const { return m_reloadItem.get(); }

    void appendRedirectionURL(const WTF::URL&);
    Vector<WTF::URL> takeRedirectChain() { return WTFMove(m_redirectChain); }

    bool wasUserInitiated() const { return !!m_lastNavigationAction.userGestureTokenIdentifier; }

    bool shouldPerformDownload() const { return !m_lastNavigationAction.downloadAttribute.isNull(); }

    bool treatAsSameOriginNavigation() const { return m_lastNavigationAction.treatAsSameOriginNavigation; }
    bool hasOpenedFrames() const { return m_lastNavigationAction.hasOpenedFrames; }
    bool openedByDOMWithOpener() const { return m_lastNavigationAction.openedByDOMWithOpener; }
    const WebCore::SecurityOriginData& requesterOrigin() const { return m_lastNavigationAction.requesterOrigin; }

    void setUserContentExtensionsEnabled(bool enabled) { m_userContentExtensionsEnabled = enabled; }
    bool userContentExtensionsEnabled() const { return m_userContentExtensionsEnabled; }

    WebCore::LockHistory lockHistory() const { return m_lastNavigationAction.lockHistory; }
    WebCore::LockBackForwardList lockBackForwardList() const { return m_lastNavigationAction.lockBackForwardList; }

    WTF::String clientRedirectSourceForHistory() const { return m_lastNavigationAction.clientRedirectSourceForHistory; }
    WebCore::SandboxFlags effectiveSandboxFlags() const { return m_lastNavigationAction.effectiveSandboxFlags; }

    void setLastNavigationAction(const WebKit::NavigationActionData& navigationAction) { m_lastNavigationAction = navigationAction; }
    const WebKit::NavigationActionData& lastNavigationAction() const { return m_lastNavigationAction; }

    void setOriginatingFrameInfo(const WebKit::FrameInfoData& frameInfo) { m_originatingFrameInfo = frameInfo; }
    const WebKit::FrameInfoData& originatingFrameInfo() const { return m_originatingFrameInfo; }

    void setDestinationFrameSecurityOrigin(const WebCore::SecurityOriginData& origin) { m_destinationFrameSecurityOrigin = origin; }
    const WebCore::SecurityOriginData& destinationFrameSecurityOrigin() const { return m_destinationFrameSecurityOrigin; }

    void setEffectiveContentMode(WebKit::WebContentMode mode) { m_effectiveContentMode = mode; }
    WebKit::WebContentMode effectiveContentMode() const { return m_effectiveContentMode; }

#if !LOG_DISABLED
    const char* loggingString() const;
#endif

    const std::unique_ptr<SubstituteData>& substituteData() const { return m_substituteData; }

    const std::optional<WebCore::PrivateClickMeasurement>& privateClickMeasurement() const { return m_lastNavigationAction.privateClickMeasurement; }

    void setClientNavigationActivity(WebKit::ProcessThrottler::ActivityVariant&& activity) { m_clientNavigationActivity = WTFMove(activity); }

    void setIsLoadedWithNavigationShared(bool value) { m_isLoadedWithNavigationShared = value; }
    bool isLoadedWithNavigationShared() const { return m_isLoadedWithNavigationShared; }

    void setWebsitePolicies(RefPtr<API::WebsitePolicies>&& policies) { m_websitePolicies = WTFMove(policies); }
    API::WebsitePolicies* websitePolicies() { return m_websitePolicies.get(); }

    void setOriginatorAdvancedPrivacyProtections(OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections) { m_originatorAdvancedPrivacyProtections = advancedPrivacyProtections; }
    OptionSet<WebCore::AdvancedPrivacyProtections> originatorAdvancedPrivacyProtections() const { return m_originatorAdvancedPrivacyProtections; }

    WebCore::ProcessIdentifier processID() const { return m_processID; }
    void setProcessID(WebCore::ProcessIdentifier processID) { m_processID = processID; }

private:
    Navigation(WebKit::WebNavigationState&, WebCore::ProcessIdentifier);
    Navigation(WebKit::WebNavigationState&, WebCore::ProcessIdentifier, WebKit::WebBackForwardListItem*);
    Navigation(WebKit::WebNavigationState&, WebCore::ProcessIdentifier, WebCore::ResourceRequest&&, WebKit::WebBackForwardListItem* fromItem);
    Navigation(WebKit::WebNavigationState&, WebCore::ProcessIdentifier, WebKit::WebBackForwardListItem& targetItem, WebKit::WebBackForwardListItem* fromItem, WebCore::FrameLoadType);
    Navigation(WebKit::WebNavigationState&, WebCore::ProcessIdentifier, std::unique_ptr<SubstituteData>&&);
    Navigation(WebKit::WebNavigationState&, WebCore::ProcessIdentifier, WebCore::ResourceRequest&&, std::unique_ptr<SubstituteData>&&, WebKit::WebBackForwardListItem* fromItem);

    uint64_t m_navigationID;
    WebCore::ProcessIdentifier m_processID;
    WebCore::ResourceRequest m_originalRequest;
    WebCore::ResourceRequest m_currentRequest;
    std::optional<WebCore::ProcessIdentifier> m_currentRequestProcessIdentifier;
    Vector<WTF::URL> m_redirectChain;

    RefPtr<WebKit::WebBackForwardListItem> m_targetItem;
    RefPtr<WebKit::WebBackForwardListItem> m_fromItem;
    RefPtr<WebKit::WebBackForwardListItem> m_reloadItem;
    std::optional<WebCore::FrameLoadType> m_backForwardFrameLoadType;
    std::unique_ptr<SubstituteData> m_substituteData;
    WebKit::NavigationActionData m_lastNavigationAction;
    WebKit::FrameInfoData m_originatingFrameInfo;
    WebCore::SecurityOriginData m_destinationFrameSecurityOrigin;
    bool m_userContentExtensionsEnabled { true };
    WebKit::WebContentMode m_effectiveContentMode { WebKit::WebContentMode::Recommended };
    WebKit::ProcessThrottler::TimedActivity m_clientNavigationActivity;
    bool m_isLoadedWithNavigationShared { false };
    RefPtr<API::WebsitePolicies> m_websitePolicies;
    OptionSet<WebCore::AdvancedPrivacyProtections> m_originatorAdvancedPrivacyProtections;
};

} // namespace API
