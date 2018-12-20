/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "WebPreferencesStore.h"
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>

namespace WebKit {
class VisitedLinkStore;
class WebPageGroup;
class WebPageProxy;
class WebPreferences;
class WebProcessPool;
class WebURLSchemeHandler;
class WebUserContentControllerProxy;
}

namespace API {

class ApplicationManifest;
class WebsiteDataStore;

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

    WebKit::WebPreferencesStore::ValueMap& preferenceValues() { return m_preferenceValues; }

    WebKit::WebPageProxy* relatedPage() const;
    void setRelatedPage(WebKit::WebPageProxy*);

    WebKit::VisitedLinkStore* visitedLinkStore();
    void setVisitedLinkStore(WebKit::VisitedLinkStore*);

    WebsiteDataStore* websiteDataStore();
    void setWebsiteDataStore(WebsiteDataStore*);

    // FIXME: Once PageConfigurations *always* have a data store, get rid of the separate sessionID.
    PAL::SessionID sessionID();
    void setSessionID(PAL::SessionID);

    bool treatsSHA1SignedCertificatesAsInsecure() { return m_treatsSHA1SignedCertificatesAsInsecure; }
    void setTreatsSHA1SignedCertificatesAsInsecure(bool treatsSHA1SignedCertificatesAsInsecure) { m_treatsSHA1SignedCertificatesAsInsecure = treatsSHA1SignedCertificatesAsInsecure; } 

#if PLATFORM(IOS_FAMILY)
    bool alwaysRunsAtForegroundPriority() { return m_alwaysRunsAtForegroundPriority; }
    void setAlwaysRunsAtForegroundPriority(bool alwaysRunsAtForegroundPriority) { m_alwaysRunsAtForegroundPriority = alwaysRunsAtForegroundPriority; } 
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

private:

    RefPtr<WebKit::WebProcessPool> m_processPool;
    RefPtr<WebKit::WebUserContentControllerProxy> m_userContentController;
    RefPtr<WebKit::WebPageGroup> m_pageGroup;
    RefPtr<WebKit::WebPreferences> m_preferences;
    WebKit::WebPreferencesStore::ValueMap m_preferenceValues;
    RefPtr<WebKit::WebPageProxy> m_relatedPage;
    RefPtr<WebKit::VisitedLinkStore> m_visitedLinkStore;

    RefPtr<WebsiteDataStore> m_websiteDataStore;
    // FIXME: We currently have to pass the session ID separately here to support the legacy private browsing session.
    // Once we get rid of it we should get rid of this configuration parameter as well.
    PAL::SessionID m_sessionID;

    bool m_treatsSHA1SignedCertificatesAsInsecure { true };
#if PLATFORM(IOS_FAMILY)
    bool m_alwaysRunsAtForegroundPriority { false };
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
};

} // namespace API
