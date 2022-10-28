/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#include "InjectedBundleHitTestResultMediaType.h"
#include "ProcessTerminationReason.h"
#include "WKBundleHitTestResult.h"
#include "WKContext.h"
#include "WKCookieManager.h"
#include "WKCredentialTypes.h"
#include "WKPage.h"
#include "WKPreferencesRef.h"
#include "WKPreferencesRefPrivate.h"
#include "WKProcessTerminationReason.h"
#include "WKProtectionSpaceTypes.h"
#include "WKResourceCacheManager.h"
#include "WKSharedAPICast.h"
#include <WebCore/Credential.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/PluginData.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/Settings.h>

namespace API {
class ContentRuleList;
class ContentRuleListStore;
class InternalDebugFeature;
class ExperimentalFeature;
class FrameHandle;
class FrameInfo;
class HTTPCookieStore;
class HitTestResult;
class MessageListener;
class Navigation;
class NavigationAction;
class NavigationData;
class NavigationResponse;
class OpenPanelParameters;
class PageConfiguration;
class ProcessPoolConfiguration;
class SessionState;
class UserScript;
class WebsitePolicies;
class WindowFeatures;
}

namespace WebKit {

class AuthenticationChallengeProxy;
class AuthenticationDecisionListener;
class DownloadProxy;
class GeolocationPermissionRequest;
class MediaKeySystemPermissionCallback;
class NotificationPermissionRequest;
class QueryPermissionResultCallback;
class SpeechRecognitionPermissionCallback;
class UserMediaPermissionCheckProxy;
class UserMediaPermissionRequestProxy;
class WebBackForwardList;
class WebBackForwardListItem;
class WebColorPickerResultListenerProxy;
class WebContextMenuListenerProxy;
class WebCredential;
class WebFormSubmissionListenerProxy;
class WebFramePolicyListenerProxy;
class WebFrameProxy;
class WebGeolocationManagerProxy;
class WebGeolocationPosition;
class WebIconDatabase;
class WebInspectorUIProxy;
class WebNotification;
class WebNotificationManagerProxy;
class WebNotificationProvider;
class WebOpenPanelResultListenerProxy;
class WebPageGroup;
class WebPageProxy;
class WebPreferences;
class WebProcessPool;
class WebProtectionSpace;
class WebResourceLoadStatisticsManager;
class WebTextChecker;
class WebUserContentControllerProxy;
class WebViewportAttributes;
class WebsiteDataStore;
class WebsiteDataStoreConfiguration;

WK_ADD_API_MAPPING(WKAuthenticationChallengeRef, AuthenticationChallengeProxy)
WK_ADD_API_MAPPING(WKAuthenticationDecisionListenerRef, AuthenticationDecisionListener)
WK_ADD_API_MAPPING(WKBackForwardListItemRef, WebBackForwardListItem)
WK_ADD_API_MAPPING(WKBackForwardListRef, WebBackForwardList)
WK_ADD_API_MAPPING(WKBundleHitTestResultMediaType, BundleHitTestResultMediaType)
WK_ADD_API_MAPPING(WKColorPickerResultListenerRef, WebColorPickerResultListenerProxy)
WK_ADD_API_MAPPING(WKContextRef, WebProcessPool)
WK_ADD_API_MAPPING(WKContextConfigurationRef, API::ProcessPoolConfiguration)
WK_ADD_API_MAPPING(WKContextMenuListenerRef, WebContextMenuListenerProxy)
WK_ADD_API_MAPPING(WKCredentialRef, WebCredential)
WK_ADD_API_MAPPING(WKDownloadRef, DownloadProxy)
WK_ADD_API_MAPPING(WKExperimentalFeatureRef, API::ExperimentalFeature)
WK_ADD_API_MAPPING(WKFormSubmissionListenerRef, WebFormSubmissionListenerProxy)
WK_ADD_API_MAPPING(WKFramePolicyListenerRef, WebFramePolicyListenerProxy)
WK_ADD_API_MAPPING(WKFrameHandleRef, API::FrameHandle)
WK_ADD_API_MAPPING(WKFrameInfoRef, API::FrameInfo)
WK_ADD_API_MAPPING(WKFrameRef, WebFrameProxy)
WK_ADD_API_MAPPING(WKGeolocationManagerRef, WebGeolocationManagerProxy)
WK_ADD_API_MAPPING(WKGeolocationPermissionRequestRef, GeolocationPermissionRequest)
WK_ADD_API_MAPPING(WKGeolocationPositionRef, WebGeolocationPosition)
WK_ADD_API_MAPPING(WKHTTPCookieStoreRef, API::HTTPCookieStore)
WK_ADD_API_MAPPING(WKHitTestResultRef, API::HitTestResult)
WK_ADD_API_MAPPING(WKIconDatabaseRef, WebIconDatabase)
WK_ADD_API_MAPPING(WKInspectorRef, WebInspectorUIProxy)
WK_ADD_API_MAPPING(WKInternalDebugFeatureRef, API::InternalDebugFeature)
WK_ADD_API_MAPPING(WKMediaKeySystemPermissionCallbackRef, MediaKeySystemPermissionCallback)
WK_ADD_API_MAPPING(WKQueryPermissionResultCallbackRef, QueryPermissionResultCallback)
WK_ADD_API_MAPPING(WKMessageListenerRef, API::MessageListener)
WK_ADD_API_MAPPING(WKNavigationActionRef, API::NavigationAction)
WK_ADD_API_MAPPING(WKNavigationDataRef, API::NavigationData)
WK_ADD_API_MAPPING(WKNavigationRef, API::Navigation)
WK_ADD_API_MAPPING(WKNavigationResponseRef, API::NavigationResponse)
WK_ADD_API_MAPPING(WKNotificationManagerRef, WebNotificationManagerProxy)
WK_ADD_API_MAPPING(WKNotificationPermissionRequestRef, NotificationPermissionRequest)
WK_ADD_API_MAPPING(WKNotificationProviderRef, WebNotificationProvider)
WK_ADD_API_MAPPING(WKNotificationRef, WebNotification)
WK_ADD_API_MAPPING(WKOpenPanelParametersRef, API::OpenPanelParameters)
WK_ADD_API_MAPPING(WKOpenPanelResultListenerRef, WebOpenPanelResultListenerProxy)
WK_ADD_API_MAPPING(WKPageGroupRef, WebPageGroup)
WK_ADD_API_MAPPING(WKPageConfigurationRef, API::PageConfiguration)
WK_ADD_API_MAPPING(WKPageRef, WebPageProxy)
WK_ADD_API_MAPPING(WKPreferencesRef, WebPreferences)
WK_ADD_API_MAPPING(WKProtectionSpaceRef, WebProtectionSpace)
WK_ADD_API_MAPPING(WKResourceLoadStatisticsManagerRef, WebResourceLoadStatisticsManager)
WK_ADD_API_MAPPING(WKSessionStateRef, API::SessionState)
WK_ADD_API_MAPPING(WKSpeechRecognitionPermissionCallbackRef, SpeechRecognitionPermissionCallback)
WK_ADD_API_MAPPING(WKTextCheckerRef, WebTextChecker)
WK_ADD_API_MAPPING(WKUserContentControllerRef, WebUserContentControllerProxy)
WK_ADD_API_MAPPING(WKUserContentExtensionStoreRef, API::ContentRuleListStore)
WK_ADD_API_MAPPING(WKUserContentFilterRef, API::ContentRuleList)
WK_ADD_API_MAPPING(WKUserMediaPermissionCheckRef, UserMediaPermissionCheckProxy)
WK_ADD_API_MAPPING(WKUserMediaPermissionRequestRef, UserMediaPermissionRequestProxy)
WK_ADD_API_MAPPING(WKUserScriptRef, API::UserScript)
WK_ADD_API_MAPPING(WKViewportAttributesRef, WebViewportAttributes)
WK_ADD_API_MAPPING(WKWebsiteDataStoreRef, WebKit::WebsiteDataStore)
WK_ADD_API_MAPPING(WKWebsiteDataStoreConfigurationRef, WebKit::WebsiteDataStoreConfiguration)
WK_ADD_API_MAPPING(WKWebsitePoliciesRef, API::WebsitePolicies)
WK_ADD_API_MAPPING(WKWindowFeaturesRef, API::WindowFeatures)

/* Enum conversions */

inline BundleHitTestResultMediaType toBundleHitTestResultMediaType(WKBundleHitTestResultMediaType wkMediaType)
{
    switch (wkMediaType) {
    case kWKBundleHitTestResultMediaTypeNone:
        return BundleHitTestResultMediaTypeNone;
    case kWKBundleHitTestResultMediaTypeAudio:
        return BundleHitTestResultMediaTypeAudio;
    case kWKBundleHitTestResultMediaTypeVideo:
        return BundleHitTestResultMediaTypeVideo;
    }
    
    ASSERT_NOT_REACHED();
    return BundleHitTestResultMediaTypeNone;
}
    
inline WKBundleHitTestResultMediaType toAPI(BundleHitTestResultMediaType mediaType)
{
    switch (mediaType) {
    case BundleHitTestResultMediaTypeNone:
        return kWKBundleHitTestResultMediaTypeNone;
    case BundleHitTestResultMediaTypeAudio:
        return kWKBundleHitTestResultMediaTypeAudio;
    case BundleHitTestResultMediaTypeVideo:
        return kWKBundleHitTestResultMediaTypeVideo;
    }
    
    ASSERT_NOT_REACHED();
    return kWKBundleHitTestResultMediaTypeNone;
}

inline CacheModel toCacheModel(WKCacheModel wkCacheModel)
{
    switch (wkCacheModel) {
    case kWKCacheModelDocumentViewer:
        return CacheModel::DocumentViewer;
    case kWKCacheModelDocumentBrowser:
        return CacheModel::DocumentBrowser;
    case kWKCacheModelPrimaryWebBrowser:
        return CacheModel::PrimaryWebBrowser;
    }

    ASSERT_NOT_REACHED();
    return CacheModel::DocumentViewer;
}

inline WKCacheModel toAPI(CacheModel cacheModel)
{
    switch (cacheModel) {
    case CacheModel::DocumentViewer:
        return kWKCacheModelDocumentViewer;
    case CacheModel::DocumentBrowser:
        return kWKCacheModelDocumentBrowser;
    case CacheModel::PrimaryWebBrowser:
        return kWKCacheModelPrimaryWebBrowser;
    }
    
    return kWKCacheModelDocumentViewer;
}

inline WKProcessTerminationReason toAPI(ProcessTerminationReason reason)
{
    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
        return kWKProcessTerminationReasonExceededMemoryLimit;
    case ProcessTerminationReason::ExceededCPULimit:
        return kWKProcessTerminationReasonExceededCPULimit;
    case ProcessTerminationReason::IdleExit:
    case ProcessTerminationReason::NavigationSwap:
        // We probably shouldn't bother coming up with a new C-API type for process-swapping.
        // "Requested by client" seems like the best match for existing types.
        FALLTHROUGH;
    case ProcessTerminationReason::RequestedByClient:
        return kWKProcessTerminationReasonRequestedByClient;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::Unresponsive:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
    case ProcessTerminationReason::Crash:
        return kWKProcessTerminationReasonCrash;
    }

    return kWKProcessTerminationReasonCrash;
}

inline WKEditableLinkBehavior toAPI(WebCore::EditableLinkBehavior behavior)
{
    switch (behavior) {
    case WebCore::EditableLinkBehavior::Default:
        return kWKEditableLinkBehaviorDefault;
    case WebCore::EditableLinkBehavior::AlwaysLive:
        return kWKEditableLinkBehaviorAlwaysLive;
    case WebCore::EditableLinkBehavior::OnlyLiveWithShiftKey:
        return kWKEditableLinkBehaviorOnlyLiveWithShiftKey;
    case WebCore::EditableLinkBehavior::LiveWhenNotFocused:
        return kWKEditableLinkBehaviorLiveWhenNotFocused;
    case WebCore::EditableLinkBehavior::NeverLive:
        return kWKEditableLinkBehaviorNeverLive;
    }
    
    ASSERT_NOT_REACHED();
    return kWKEditableLinkBehaviorNeverLive;
}

inline WebCore::EditableLinkBehavior toEditableLinkBehavior(WKEditableLinkBehavior wkBehavior)
{
    switch (wkBehavior) {
    case kWKEditableLinkBehaviorDefault:
        return WebCore::EditableLinkBehavior::Default;
    case kWKEditableLinkBehaviorAlwaysLive:
        return WebCore::EditableLinkBehavior::AlwaysLive;
    case kWKEditableLinkBehaviorOnlyLiveWithShiftKey:
        return WebCore::EditableLinkBehavior::OnlyLiveWithShiftKey;
    case kWKEditableLinkBehaviorLiveWhenNotFocused:
        return WebCore::EditableLinkBehavior::LiveWhenNotFocused;
    case kWKEditableLinkBehaviorNeverLive:
        return WebCore::EditableLinkBehavior::NeverLive;
    }
    
    ASSERT_NOT_REACHED();
    return WebCore::EditableLinkBehavior::NeverLive;
}
    
inline WKProtectionSpaceServerType toAPI(WebCore::ProtectionSpace::ServerType type)
{
    switch (type) {
    case WebCore::ProtectionSpace::ServerType::HTTP:
        return kWKProtectionSpaceServerTypeHTTP;
    case WebCore::ProtectionSpace::ServerType::HTTPS:
        return kWKProtectionSpaceServerTypeHTTPS;
    case WebCore::ProtectionSpace::ServerType::FTP:
        return kWKProtectionSpaceServerTypeFTP;
    case WebCore::ProtectionSpace::ServerType::FTPS:
        return kWKProtectionSpaceServerTypeFTPS;
    case WebCore::ProtectionSpace::ServerType::ProxyHTTP:
        return kWKProtectionSpaceProxyTypeHTTP;
    case WebCore::ProtectionSpace::ServerType::ProxyHTTPS:
        return kWKProtectionSpaceProxyTypeHTTPS;
    case WebCore::ProtectionSpace::ServerType::ProxyFTP:
        return kWKProtectionSpaceProxyTypeFTP;
    case WebCore::ProtectionSpace::ServerType::ProxySOCKS:
        return kWKProtectionSpaceProxyTypeSOCKS;
    }
    return kWKProtectionSpaceServerTypeHTTP;
}

inline WKProtectionSpaceAuthenticationScheme toAPI(WebCore::ProtectionSpace::AuthenticationScheme type)
{
    switch (type) {
    case WebCore::ProtectionSpace::AuthenticationScheme::Default:
        return kWKProtectionSpaceAuthenticationSchemeDefault;
    case WebCore::ProtectionSpace::AuthenticationScheme::HTTPBasic:
        return kWKProtectionSpaceAuthenticationSchemeHTTPBasic;
    case WebCore::ProtectionSpace::AuthenticationScheme::HTTPDigest:
        return kWKProtectionSpaceAuthenticationSchemeHTTPDigest;
    case WebCore::ProtectionSpace::AuthenticationScheme::HTMLForm:
        return kWKProtectionSpaceAuthenticationSchemeHTMLForm;
    case WebCore::ProtectionSpace::AuthenticationScheme::NTLM:
        return kWKProtectionSpaceAuthenticationSchemeNTLM;
    case WebCore::ProtectionSpace::AuthenticationScheme::Negotiate:
        return kWKProtectionSpaceAuthenticationSchemeNegotiate;
    case WebCore::ProtectionSpace::AuthenticationScheme::ClientCertificateRequested:
        return kWKProtectionSpaceAuthenticationSchemeClientCertificateRequested;
    case WebCore::ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested:
        return kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
    case WebCore::ProtectionSpace::AuthenticationScheme::OAuth:
        return kWKProtectionSpaceAuthenticationSchemeOAuth;
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

inline WebCore::HTTPCookieAcceptPolicy toHTTPCookieAcceptPolicy(WKHTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case kWKHTTPCookieAcceptPolicyAlways:
        return WebCore::HTTPCookieAcceptPolicy::AlwaysAccept;
    case kWKHTTPCookieAcceptPolicyNever:
        return WebCore::HTTPCookieAcceptPolicy::Never;
    case kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        return WebCore::HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain;
    case kWKHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain:
        return WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return WebCore::HTTPCookieAcceptPolicy::AlwaysAccept;
}

inline WKHTTPCookieAcceptPolicy toAPI(WebCore::HTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case WebCore::HTTPCookieAcceptPolicy::AlwaysAccept:
        return kWKHTTPCookieAcceptPolicyAlways;
    case WebCore::HTTPCookieAcceptPolicy::Never:
        return kWKHTTPCookieAcceptPolicyNever;
    case WebCore::HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain:
        return kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    case WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        return kWKHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return kWKHTTPCookieAcceptPolicyAlways;
}

inline WebCore::StorageBlockingPolicy toStorageBlockingPolicy(WKStorageBlockingPolicy policy)
{
    switch (policy) {
    case kWKAllowAllStorage:
        return WebCore::StorageBlockingPolicy::AllowAll;
    case kWKBlockThirdPartyStorage:
        return WebCore::StorageBlockingPolicy::BlockThirdParty;
    case kWKBlockAllStorage:
        return WebCore::StorageBlockingPolicy::BlockAll;
    }

    ASSERT_NOT_REACHED();
    return WebCore::StorageBlockingPolicy::AllowAll;
}

inline WKStorageBlockingPolicy toAPI(WebCore::StorageBlockingPolicy policy)
{
    switch (policy) {
    case WebCore::StorageBlockingPolicy::AllowAll:
        return kWKAllowAllStorage;
    case WebCore::StorageBlockingPolicy::BlockThirdParty:
        return kWKBlockThirdPartyStorage;
    case WebCore::StorageBlockingPolicy::BlockAll:
        return kWKBlockAllStorage;
    }

    ASSERT_NOT_REACHED();
    return kWKAllowAllStorage;
}

} // namespace WebKit

#if defined(BUILDING_GTK__)
#include "WKAPICastGtk.h"
#endif

#if defined(BUILDING_WPE__)
#include "WKAPICastWPE.h"
#endif

#if defined(WIN32) || defined(_WIN32)
#include "WKAPICastWin.h"
#endif

#if defined(__SCE__)
#include "WKAPICastPlayStation.h"
#endif

#endif // WKAPICast_h
