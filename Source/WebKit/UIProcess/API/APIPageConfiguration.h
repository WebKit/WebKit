/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "WebViewCategory.h"
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
OBJC_PROTOCOL(_UIClickInteractionDriving);
#include <wtf/RetainPtr.h>
#endif

namespace WebKit {
class VisitedLinkStore;
class WebPageGroup;
class WebPageProxy;
class WebPreferences;
class WebProcessPool;
class WebURLSchemeHandler;
class WebUserContentControllerProxy;
class WebsiteDataStore;
}

namespace API {

class ApplicationManifest;
class WebsitePolicies;

class PageConfiguration : public ObjectImpl<Object::Type::PageConfiguration> {
public:
    static Ref<PageConfiguration> create();

    explicit PageConfiguration();
    virtual ~PageConfiguration();

    Ref<PageConfiguration> copy() const;

    // FIXME: The configuration properties should return their default values
    // rather than nullptr.
    
    WebKit::WebProcessPool* processPool();
    void setProcessPool(WebKit::WebProcessPool*);

    WebKit::WebUserContentControllerProxy* userContentController();
    void setUserContentController(WebKit::WebUserContentControllerProxy*);

    WebKit::WebPageGroup* pageGroup();
    void setPageGroup(WebKit::WebPageGroup*);

    WebKit::WebPreferences* preferences();
    void setPreferences(WebKit::WebPreferences*);

    WebKit::WebPageProxy* relatedPage() const;
    void setRelatedPage(WebKit::WebPageProxy*);

    WebKit::VisitedLinkStore* visitedLinkStore();
    void setVisitedLinkStore(WebKit::VisitedLinkStore*);

    WebKit::WebsiteDataStore* websiteDataStore();
    void setWebsiteDataStore(WebKit::WebsiteDataStore*);

    WebsitePolicies* defaultWebsitePolicies() const;
    void setDefaultWebsitePolicies(WebsitePolicies*);

#if PLATFORM(IOS_FAMILY)
    bool clientNavigationsRunAtForegroundPriority() const { return m_clientNavigationsRunAtForegroundPriority; }
    void setClientNavigationsRunAtForegroundPriority(bool value) { m_clientNavigationsRunAtForegroundPriority = value; }

    bool canShowWhileLocked() const { return m_canShowWhileLocked; }
    void setCanShowWhileLocked(bool canShowWhileLocked) { m_canShowWhileLocked = canShowWhileLocked; }

    const RetainPtr<_UIClickInteractionDriving>& clickInteractionDriverForTesting() const { return m_clickInteractionDriverForTesting; }
    void setClickInteractionDriverForTesting(RetainPtr<_UIClickInteractionDriving>&& driver) { m_clickInteractionDriverForTesting = WTFMove(driver); }
#endif
    bool initialCapitalizationEnabled() { return m_initialCapitalizationEnabled; }
    void setInitialCapitalizationEnabled(bool initialCapitalizationEnabled) { m_initialCapitalizationEnabled = initialCapitalizationEnabled; }

    Optional<double> cpuLimit() const { return m_cpuLimit; }
    void setCPULimit(double cpuLimit) { m_cpuLimit = cpuLimit; }

    bool waitsForPaintAfterViewDidMoveToWindow() const { return m_waitsForPaintAfterViewDidMoveToWindow; }
    void setWaitsForPaintAfterViewDidMoveToWindow(bool shouldSynchronize) { m_waitsForPaintAfterViewDidMoveToWindow = shouldSynchronize; }

    bool drawsBackground() const { return m_drawsBackground; }
    void setDrawsBackground(bool drawsBackground) { m_drawsBackground = drawsBackground; }

    bool isControlledByAutomation() const { return m_controlledByAutomation; }
    void setControlledByAutomation(bool controlledByAutomation) { m_controlledByAutomation = controlledByAutomation; }

    const WTF::String& overrideContentSecurityPolicy() const { return m_overrideContentSecurityPolicy; }
    void setOverrideContentSecurityPolicy(const WTF::String& overrideContentSecurityPolicy) { m_overrideContentSecurityPolicy = overrideContentSecurityPolicy; }

#if PLATFORM(COCOA)
    const WTF::Vector<WTF::String>& additionalSupportedImageTypes() const { return m_additionalSupportedImageTypes; }
    void setAdditionalSupportedImageTypes(WTF::Vector<WTF::String>&& additionalSupportedImageTypes) { m_additionalSupportedImageTypes = WTFMove(additionalSupportedImageTypes); }
#endif

#if ENABLE(APPLICATION_MANIFEST)
    ApplicationManifest* applicationManifest() const;
    void setApplicationManifest(ApplicationManifest*);
#endif

    RefPtr<WebKit::WebURLSchemeHandler> urlSchemeHandlerForURLScheme(const WTF::String&);
    void setURLSchemeHandlerForURLScheme(Ref<WebKit::WebURLSchemeHandler>&&, const WTF::String&);
    const HashMap<WTF::String, Ref<WebKit::WebURLSchemeHandler>>& urlSchemeHandlers() { return m_urlSchemeHandlers; }

    const Vector<WTF::String>& corsDisablingPatterns() const { return m_corsDisablingPatterns; }
    void setCORSDisablingPatterns(Vector<WTF::String>&& patterns) { m_corsDisablingPatterns = WTFMove(patterns); }
    
    bool userScriptsShouldWaitUntilNotification() const { return m_userScriptsShouldWaitUntilNotification; }
    void setUserScriptsShouldWaitUntilNotification(bool value) { m_userScriptsShouldWaitUntilNotification = value; }

    bool crossOriginAccessControlCheckEnabled() const { return m_crossOriginAccessControlCheckEnabled; }
    void setCrossOriginAccessControlCheckEnabled(bool enabled) { m_crossOriginAccessControlCheckEnabled = enabled; }

    const WTF::String& processDisplayName() const { return m_processDisplayName; }
    void setProcessDisplayName(const WTF::String& name) { m_processDisplayName = name; }

    WebKit::WebViewCategory webViewCategory() const { return m_webViewCategory; }
    void setWebViewCategory(WebKit::WebViewCategory category) { m_webViewCategory = category; }

    bool loadsSubresources() const { return m_loadsSubresources; }
    void setLoadsSubresources(bool loads) { m_loadsSubresources = loads; }

    bool loadsFromNetwork() const { return m_loadsFromNetwork; }
    void setLoadsFromNetwork(bool loads) { m_loadsFromNetwork = loads; }

#if ENABLE(APP_BOUND_DOMAINS)
    bool ignoresAppBoundDomains() const { return m_ignoresAppBoundDomains; }
    void setIgnoresAppBoundDomains(bool shouldIgnore) { m_ignoresAppBoundDomains = shouldIgnore; }
    
    bool limitsNavigationsToAppBoundDomains() const { return m_limitsNavigationsToAppBoundDomains; }
    void setLimitsNavigationsToAppBoundDomains(bool limits) { m_limitsNavigationsToAppBoundDomains = limits; }
#endif

    void setMediaCaptureEnabled(bool value) { m_mediaCaptureEnabled = value; }
    bool mediaCaptureEnabled() const { return m_mediaCaptureEnabled; }

    void setShouldRelaxThirdPartyCookieBlocking(WebCore::ShouldRelaxThirdPartyCookieBlocking value) { m_shouldRelaxThirdPartyCookieBlocking = value; }
    WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking() const { return m_shouldRelaxThirdPartyCookieBlocking; }
    
private:

    RefPtr<WebKit::WebProcessPool> m_processPool;
    RefPtr<WebKit::WebUserContentControllerProxy> m_userContentController;
    RefPtr<WebKit::WebPageGroup> m_pageGroup;
    RefPtr<WebKit::WebPreferences> m_preferences;
    RefPtr<WebKit::WebPageProxy> m_relatedPage;
    RefPtr<WebKit::VisitedLinkStore> m_visitedLinkStore;

    RefPtr<WebKit::WebsiteDataStore> m_websiteDataStore;
    RefPtr<WebsitePolicies> m_defaultWebsitePolicies;

#if PLATFORM(IOS_FAMILY)
    bool m_clientNavigationsRunAtForegroundPriority { true };
    bool m_canShowWhileLocked { false };
    RetainPtr<_UIClickInteractionDriving> m_clickInteractionDriverForTesting;
#endif
    bool m_initialCapitalizationEnabled { true };
    bool m_waitsForPaintAfterViewDidMoveToWindow { true };
    bool m_drawsBackground { true };
    bool m_controlledByAutomation { false };
    Optional<double> m_cpuLimit;

    WTF::String m_overrideContentSecurityPolicy;

#if PLATFORM(COCOA)
    WTF::Vector<WTF::String> m_additionalSupportedImageTypes;
#endif

#if ENABLE(APPLICATION_MANIFEST)
    RefPtr<ApplicationManifest> m_applicationManifest;
#endif

    HashMap<WTF::String, Ref<WebKit::WebURLSchemeHandler>> m_urlSchemeHandlers;
    Vector<WTF::String> m_corsDisablingPatterns;
    bool m_userScriptsShouldWaitUntilNotification { true };
    bool m_crossOriginAccessControlCheckEnabled { true };
    WTF::String m_processDisplayName;
    WebKit::WebViewCategory m_webViewCategory { WebKit::WebViewCategory::AppBoundDomain };
    bool m_loadsSubresources { true };
    bool m_loadsFromNetwork { true };
    
#if ENABLE(APP_BOUND_DOMAINS)
    bool m_ignoresAppBoundDomains { false };
    bool m_limitsNavigationsToAppBoundDomains { false };
#endif

    bool m_mediaCaptureEnabled { false };

    WebCore::ShouldRelaxThirdPartyCookieBlocking m_shouldRelaxThirdPartyCookieBlocking { WebCore::ShouldRelaxThirdPartyCookieBlocking::No };
};

} // namespace API
