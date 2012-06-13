/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef WKAPICast_h
#define WKAPICast_h

#include "CacheModel.h"
#include "FontSmoothingLevel.h"
#include "HTTPCookieAcceptPolicy.h"
#include "ResourceCachesToClear.h"
#include "WebGrammarDetail.h"
#include "WKContext.h"
#include "WKCookieManager.h"
#include "WKCredentialTypes.h"
#include "WKPage.h"
#include "WKPreferencesPrivate.h"
#include "WKProtectionSpaceTypes.h"
#include "WKResourceCacheManager.h"
#include "WKSharedAPICast.h"
#include <WebCore/CookieJar.h>
#include <WebCore/Credential.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/Settings.h>

namespace WebKit {

class AuthenticationChallengeProxy;
class AuthenticationDecisionListener;
class DownloadProxy;
class GeolocationPermissionRequestProxy;
class NotificationPermissionRequest;
class WebApplicationCacheManagerProxy;
class WebBackForwardList;
class WebBackForwardListItem;
class WebResourceCacheManagerProxy;
class WebContext;
class WebCookieManagerProxy;
class WebCredential;
class WebDatabaseManagerProxy;
class WebFormSubmissionListenerProxy;
class WebFramePolicyListenerProxy;
class WebFrameProxy;
class WebGeolocationManagerProxy;
class WebGeolocationPosition;
class WebGrammarDetail;
class WebHitTestResult;
class WebIconDatabase;
class WebInspectorProxy;
class WebKeyValueStorageManagerProxy;
class WebMediaCacheManagerProxy;
class WebNavigationData;
class WebNotification;
class WebNotificationProvider;
class WebNotificationManagerProxy;
class WebOpenPanelParameters;
class WebOpenPanelResultListenerProxy;
class WebPageGroup;
class WebPageProxy;
class WebPluginSiteDataManager;
class WebPreferences;
class WebProtectionSpace;
class WebRenderLayer;
class WebRenderObject;
class WebTextChecker;

WK_ADD_API_MAPPING(WKApplicationCacheManagerRef, WebApplicationCacheManagerProxy)
WK_ADD_API_MAPPING(WKAuthenticationChallengeRef, AuthenticationChallengeProxy)
WK_ADD_API_MAPPING(WKAuthenticationDecisionListenerRef, AuthenticationDecisionListener)
WK_ADD_API_MAPPING(WKBackForwardListItemRef, WebBackForwardListItem)
WK_ADD_API_MAPPING(WKBackForwardListRef, WebBackForwardList)
WK_ADD_API_MAPPING(WKResourceCacheManagerRef, WebResourceCacheManagerProxy)
WK_ADD_API_MAPPING(WKContextRef, WebContext)
WK_ADD_API_MAPPING(WKCookieManagerRef, WebCookieManagerProxy)
WK_ADD_API_MAPPING(WKCredentialRef, WebCredential)
WK_ADD_API_MAPPING(WKDatabaseManagerRef, WebDatabaseManagerProxy)
WK_ADD_API_MAPPING(WKDownloadRef, DownloadProxy)
WK_ADD_API_MAPPING(WKFormSubmissionListenerRef, WebFormSubmissionListenerProxy)
WK_ADD_API_MAPPING(WKFramePolicyListenerRef, WebFramePolicyListenerProxy)
WK_ADD_API_MAPPING(WKFrameRef, WebFrameProxy)
WK_ADD_API_MAPPING(WKGeolocationManagerRef, WebGeolocationManagerProxy)
WK_ADD_API_MAPPING(WKGeolocationPermissionRequestRef, GeolocationPermissionRequestProxy)
WK_ADD_API_MAPPING(WKGeolocationPositionRef, WebGeolocationPosition)
WK_ADD_API_MAPPING(WKGrammarDetailRef, WebGrammarDetail)
WK_ADD_API_MAPPING(WKHitTestResultRef, WebHitTestResult)
WK_ADD_API_MAPPING(WKIconDatabaseRef, WebIconDatabase)
WK_ADD_API_MAPPING(WKKeyValueStorageManagerRef, WebKeyValueStorageManagerProxy)
WK_ADD_API_MAPPING(WKMediaCacheManagerRef, WebMediaCacheManagerProxy)
WK_ADD_API_MAPPING(WKNavigationDataRef, WebNavigationData)
WK_ADD_API_MAPPING(WKNotificationManagerRef, WebNotificationManagerProxy)
WK_ADD_API_MAPPING(WKNotificationPermissionRequestRef, NotificationPermissionRequest)
WK_ADD_API_MAPPING(WKNotificationProviderRef, WebNotificationProvider)
WK_ADD_API_MAPPING(WKNotificationRef, WebNotification)
WK_ADD_API_MAPPING(WKOpenPanelParametersRef, WebOpenPanelParameters)
WK_ADD_API_MAPPING(WKOpenPanelResultListenerRef, WebOpenPanelResultListenerProxy)
WK_ADD_API_MAPPING(WKPageGroupRef, WebPageGroup)
WK_ADD_API_MAPPING(WKPageRef, WebPageProxy)
WK_ADD_API_MAPPING(WKPluginSiteDataManagerRef, WebPluginSiteDataManager)
WK_ADD_API_MAPPING(WKPreferencesRef, WebPreferences)
WK_ADD_API_MAPPING(WKProtectionSpaceRef, WebProtectionSpace)
WK_ADD_API_MAPPING(WKRenderLayerRef, WebRenderLayer)
WK_ADD_API_MAPPING(WKRenderObjectRef, WebRenderObject)
WK_ADD_API_MAPPING(WKTextCheckerRef, WebTextChecker)

#if ENABLE(INSPECTOR)
WK_ADD_API_MAPPING(WKInspectorRef, WebInspectorProxy)
#endif

/* Enum conversions */

inline CacheModel toCacheModel(WKCacheModel wkCacheModel)
{
    switch (wkCacheModel) {
    case kWKCacheModelDocumentViewer:
        return CacheModelDocumentViewer;
    case kWKCacheModelDocumentBrowser:
        return CacheModelDocumentBrowser;
    case kWKCacheModelPrimaryWebBrowser:
        return CacheModelPrimaryWebBrowser;
    }

    ASSERT_NOT_REACHED();
    return CacheModelDocumentViewer;
}

inline WKCacheModel toAPI(CacheModel cacheModel)
{
    switch (cacheModel) {
    case CacheModelDocumentViewer:
        return kWKCacheModelDocumentViewer;
    case CacheModelDocumentBrowser:
        return kWKCacheModelDocumentBrowser;
    case CacheModelPrimaryWebBrowser:
        return kWKCacheModelPrimaryWebBrowser;
    }
    
    return kWKCacheModelDocumentViewer;
}

inline FontSmoothingLevel toFontSmoothingLevel(WKFontSmoothingLevel wkLevel)
{
    switch (wkLevel) {
    case kWKFontSmoothingLevelNoSubpixelAntiAliasing:
        return FontSmoothingLevelNoSubpixelAntiAliasing;
    case kWKFontSmoothingLevelLight:
        return FontSmoothingLevelLight;
    case kWKFontSmoothingLevelMedium:
        return FontSmoothingLevelMedium;
    case kWKFontSmoothingLevelStrong:
        return FontSmoothingLevelStrong;
#if PLATFORM(WIN)
    case kWKFontSmoothingLevelWindows:
        return FontSmoothingLevelWindows;
#endif
    }

    ASSERT_NOT_REACHED();
    return FontSmoothingLevelMedium;
}


inline WKFontSmoothingLevel toAPI(FontSmoothingLevel level)
{
    switch (level) {
    case FontSmoothingLevelNoSubpixelAntiAliasing:
        return kWKFontSmoothingLevelNoSubpixelAntiAliasing;
    case FontSmoothingLevelLight:
        return kWKFontSmoothingLevelLight;
    case FontSmoothingLevelMedium:
        return kWKFontSmoothingLevelMedium;
    case FontSmoothingLevelStrong:
        return kWKFontSmoothingLevelStrong;
#if PLATFORM(WIN)
    case FontSmoothingLevelWindows:
        return kWKFontSmoothingLevelWindows;
#endif
    }

    ASSERT_NOT_REACHED();
    return kWKFontSmoothingLevelMedium;
}

inline WKEditableLinkBehavior toAPI(WebCore::EditableLinkBehavior behavior)
{
    switch (behavior) {
    case WebCore::EditableLinkDefaultBehavior:
        return kWKEditableLinkBehaviorDefault;
    case WebCore::EditableLinkAlwaysLive:
        return kWKEditableLinkBehaviorAlwaysLive;
    case WebCore::EditableLinkOnlyLiveWithShiftKey:
        return kWKEditableLinkBehaviorOnlyLiveWithShiftKey;
    case WebCore::EditableLinkLiveWhenNotFocused:
        return kWKEditableLinkBehaviorLiveWhenNotFocused;
    case WebCore::EditableLinkNeverLive:
        return kWKEditableLinkBehaviorNeverLive;
    }
    
    ASSERT_NOT_REACHED();
    return kWKEditableLinkBehaviorNeverLive;
}

inline WebCore::EditableLinkBehavior toEditableLinkBehavior(WKEditableLinkBehavior wkBehavior)
{
    switch (wkBehavior) {
    case kWKEditableLinkBehaviorDefault:
        return WebCore::EditableLinkDefaultBehavior;
    case kWKEditableLinkBehaviorAlwaysLive:
        return WebCore::EditableLinkAlwaysLive;
    case kWKEditableLinkBehaviorOnlyLiveWithShiftKey:
        return WebCore::EditableLinkOnlyLiveWithShiftKey;
    case kWKEditableLinkBehaviorLiveWhenNotFocused:
        return WebCore::EditableLinkLiveWhenNotFocused;
    case kWKEditableLinkBehaviorNeverLive:
        return WebCore::EditableLinkNeverLive;
    }
    
    ASSERT_NOT_REACHED();
    return WebCore::EditableLinkNeverLive;
}
    
inline WKProtectionSpaceServerType toAPI(WebCore::ProtectionSpaceServerType type)
{
    switch (type) {
    case WebCore::ProtectionSpaceServerHTTP:
        return kWKProtectionSpaceServerTypeHTTP;
    case WebCore::ProtectionSpaceServerHTTPS:
        return kWKProtectionSpaceServerTypeHTTPS;
    case WebCore::ProtectionSpaceServerFTP:
        return kWKProtectionSpaceServerTypeFTP;
    case WebCore::ProtectionSpaceServerFTPS:
        return kWKProtectionSpaceServerTypeFTPS;
    case WebCore::ProtectionSpaceProxyHTTP:
        return kWKProtectionSpaceProxyTypeHTTP;
    case WebCore::ProtectionSpaceProxyHTTPS:
        return kWKProtectionSpaceProxyTypeHTTPS;
    case WebCore::ProtectionSpaceProxyFTP:
        return kWKProtectionSpaceProxyTypeFTP;
    case WebCore::ProtectionSpaceProxySOCKS:
        return kWKProtectionSpaceProxyTypeSOCKS;
    }
    return kWKProtectionSpaceServerTypeHTTP;
}

inline WKProtectionSpaceAuthenticationScheme toAPI(WebCore::ProtectionSpaceAuthenticationScheme type)
{
    switch (type) {
    case WebCore::ProtectionSpaceAuthenticationSchemeDefault:
        return kWKProtectionSpaceAuthenticationSchemeDefault;
    case WebCore::ProtectionSpaceAuthenticationSchemeHTTPBasic:
        return kWKProtectionSpaceAuthenticationSchemeHTTPBasic;
    case WebCore::ProtectionSpaceAuthenticationSchemeHTTPDigest:
        return kWKProtectionSpaceAuthenticationSchemeHTTPDigest;
    case WebCore::ProtectionSpaceAuthenticationSchemeHTMLForm:
        return kWKProtectionSpaceAuthenticationSchemeHTMLForm;
    case WebCore::ProtectionSpaceAuthenticationSchemeNTLM:
        return kWKProtectionSpaceAuthenticationSchemeNTLM;
    case WebCore::ProtectionSpaceAuthenticationSchemeNegotiate:
        return kWKProtectionSpaceAuthenticationSchemeNegotiate;
    case WebCore::ProtectionSpaceAuthenticationSchemeClientCertificateRequested:
        return kWKProtectionSpaceAuthenticationSchemeClientCertificateRequested;
    case WebCore::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        return kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
    default:
        return kWKProtectionSpaceAuthenticationSchemeUnknown;
    }
}

inline WebCore::CredentialPersistence toCredentialPersistence(WKCredentialPersistence type)
{
    switch (type) {
    case kWKCredentialPersistenceNone:
        return WebCore::CredentialPersistenceNone;
    case kWKCredentialPersistenceForSession:
        return WebCore::CredentialPersistenceForSession;
    case kWKCredentialPersistencePermanent:
        return WebCore::CredentialPersistencePermanent;
    default:
        return WebCore::CredentialPersistenceNone;
    }
}

inline ResourceCachesToClear toResourceCachesToClear(WKResourceCachesToClear wkResourceCachesToClear)
{
    switch (wkResourceCachesToClear) {
    case WKResourceCachesToClearAll:
        return AllResourceCaches;
    case WKResourceCachesToClearInMemoryOnly:
        return InMemoryResourceCachesOnly;
    }

    ASSERT_NOT_REACHED();
    return AllResourceCaches;
}

inline HTTPCookieAcceptPolicy toHTTPCookieAcceptPolicy(WKHTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case kWKHTTPCookieAcceptPolicyAlways:
        return HTTPCookieAcceptPolicyAlways;
    case kWKHTTPCookieAcceptPolicyNever:
        return HTTPCookieAcceptPolicyNever;
    case kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        return HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return HTTPCookieAcceptPolicyAlways;
}

inline WKHTTPCookieAcceptPolicy toAPI(HTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case HTTPCookieAcceptPolicyAlways:
        return kWKHTTPCookieAcceptPolicyAlways;
    case HTTPCookieAcceptPolicyNever:
        return kWKHTTPCookieAcceptPolicyNever;
    case HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        return kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return kWKHTTPCookieAcceptPolicyAlways;
}

inline ProxyingRefPtr<WebGrammarDetail> toAPI(const WebCore::GrammarDetail& grammarDetail)
{
    return ProxyingRefPtr<WebGrammarDetail>(WebGrammarDetail::create(grammarDetail));
}

} // namespace WebKit

#if defined(WIN32) || defined(_WIN32)
#include "WKAPICastWin.h"
#endif

#if defined(BUILDING_GTK__)
#include "WKAPICastGtk.h"
#endif

#if USE(SOUP)
#include "WKAPICastSoup.h"
#endif

#endif // WKAPICast_h
