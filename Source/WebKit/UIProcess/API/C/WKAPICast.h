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
#include "WKAXTypes.h"
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
#include <WebCore/AXObjectCache.h>
#include <WebCore/Credential.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/PluginData.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/Settings.h>

namespace API {
class CaptionUserPreferencesTestingModeToken;
class ContentRuleList;
class ContentRuleListStore;
class Feature;
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
WK_ADD_API_MAPPING(WKCaptionUserPreferencesTestingModeTokenRef, API::CaptionUserPreferencesTestingModeToken)
WK_ADD_API_MAPPING(WKColorPickerResultListenerRef, WebColorPickerResultListenerProxy)
WK_ADD_API_MAPPING(WKContextRef, WebProcessPool)
WK_ADD_API_MAPPING(WKContextConfigurationRef, API::ProcessPoolConfiguration)
WK_ADD_API_MAPPING(WKContextMenuListenerRef, WebContextMenuListenerProxy)
WK_ADD_API_MAPPING(WKCredentialRef, WebCredential)
WK_ADD_API_MAPPING(WKDownloadRef, DownloadProxy)
WK_ADD_API_MAPPING(WKFeatureRef, API::Feature)
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
        return WebCore::CredentialPersistence::None;
    case kWKCredentialPersistenceForSession:
        return WebCore::CredentialPersistence::ForSession;
    case kWKCredentialPersistencePermanent:
        return WebCore::CredentialPersistence::Permanent;
    default:
        return WebCore::CredentialPersistence::None;
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

inline WKAXNotification toAPI(WebCore::AXObjectCache::AXNotification notification)
{
    switch (notification) {
    case WebCore::AXObjectCache::AXNotification::AXAccessKeyChanged:
        return kWKAXAccessKeyChanged;
    case WebCore::AXObjectCache::AXNotification::AXActiveDescendantChanged:
        return kWKAXActiveDescendantChanged;
    case WebCore::AXObjectCache::AXNotification::AXAnnouncementRequested:
        return kWKAXAnnouncementRequested;
    case WebCore::AXObjectCache::AXNotification::AXAutocorrectionOccured:
        return kWKAXAutocorrectionOccured;
    case WebCore::AXObjectCache::AXNotification::AXAutofillTypeChanged:
        return kWKAXAutofillTypeChanged;
    case WebCore::AXObjectCache::AXNotification::AXCellSlotsChanged:
        return kWKAXCellSlotsChanged;
    case WebCore::AXObjectCache::AXNotification::AXCheckedStateChanged:
        return kWKAXCheckedStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXChildrenChanged:
        return kWKAXChildrenChanged;
    case WebCore::AXObjectCache::AXNotification::AXColumnCountChanged:
        return kWKAXColumnCountChanged;
    case WebCore::AXObjectCache::AXNotification::AXColumnIndexChanged:
        return kWKAXColumnIndexChanged;
    case WebCore::AXObjectCache::AXNotification::AXColumnSpanChanged:
        return kWKAXColumnSpanChanged;
    case WebCore::AXObjectCache::AXNotification::AXContentEditableAttributeChanged:
        return kWKAXContentEditableAttributeChanged;
    case WebCore::AXObjectCache::AXNotification::AXControlledObjectsChanged:
        return kWKAXControlledObjectsChanged;
    case WebCore::AXObjectCache::AXNotification::AXCurrentStateChanged:
        return kWKAXCurrentStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXDescribedByChanged:
        return kWKAXDescribedByChanged;
    case WebCore::AXObjectCache::AXNotification::AXDisabledStateChanged:
        return kWKAXDisabledStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXDropEffectChanged:
        return kWKAXDropEffectChanged;
    case WebCore::AXObjectCache::AXNotification::AXExtendedDescriptionChanged:
        return kWKAXExtendedDescriptionChanged;
    case WebCore::AXObjectCache::AXNotification::AXFlowToChanged:
        return kWKAXFlowToChanged;
    case WebCore::AXObjectCache::AXNotification::AXFocusableStateChanged:
        return kWKAXFocusableStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXFocusedUIElementChanged:
        return kWKAXFocusedUIElementChanged;
    case WebCore::AXObjectCache::AXNotification::AXFrameLoadComplete:
        return kWKAXFrameLoadComplete;
    case WebCore::AXObjectCache::AXNotification::AXGrabbedStateChanged:
        return kWKAXGrabbedStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXHasPopupChanged:
        return kWKAXHasPopupChanged;
    case WebCore::AXObjectCache::AXNotification::AXIdAttributeChanged:
        return kWKAXIdAttributeChanged;
    case WebCore::AXObjectCache::AXNotification::AXImageOverlayChanged:
        return kWKAXImageOverlayChanged;
    case WebCore::AXObjectCache::AXNotification::AXIsAtomicChanged:
        return kWKAXIsAtomicChanged;
    case WebCore::AXObjectCache::AXNotification::AXKeyShortcutsChanged:
        return kWKAXKeyShortcutsChanged;
    case WebCore::AXObjectCache::AXNotification::AXLanguageChanged:
        return kWKAXLanguageChanged;
    case WebCore::AXObjectCache::AXNotification::AXLayoutComplete:
        return kWKAXLayoutComplete;
    case WebCore::AXObjectCache::AXNotification::AXLevelChanged:
        return kWKAXLevelChanged;
    case WebCore::AXObjectCache::AXNotification::AXLoadComplete:
        return kWKAXLoadComplete;
    case WebCore::AXObjectCache::AXNotification::AXNameChanged:
        return kWKAXNameChanged;
    case WebCore::AXObjectCache::AXNotification::AXNewDocumentLoadComplete:
        return kWKAXNewDocumentLoadComplete;
    case WebCore::AXObjectCache::AXNotification::AXPageScrolled:
        return kWKAXPageScrolled;
    case WebCore::AXObjectCache::AXNotification::AXPlaceholderChanged:
        return kWKAXPlaceholderChanged;
    case WebCore::AXObjectCache::AXNotification::AXPopoverTargetChanged:
        return kWKAXPopoverTargetChanged;
    case WebCore::AXObjectCache::AXNotification::AXPositionInSetChanged:
        return kWKAXPositionInSetChanged;
    case WebCore::AXObjectCache::AXNotification::AXRoleChanged:
        return kWKAXRoleChanged;
    case WebCore::AXObjectCache::AXNotification::AXRoleDescriptionChanged:
        return kWKAXRoleDescriptionChanged;
    case WebCore::AXObjectCache::AXNotification::AXRowIndexChanged:
        return kWKAXRowIndexChanged;
    case WebCore::AXObjectCache::AXNotification::AXRowSpanChanged:
        return kWKAXRowSpanChanged;
    case WebCore::AXObjectCache::AXNotification::AXCellScopeChanged:
        return kWKAXCellScopeChanged;
    case WebCore::AXObjectCache::AXNotification::AXSelectedChildrenChanged:
        return kWKAXSelectedChildrenChanged;
    case WebCore::AXObjectCache::AXNotification::AXSelectedCellsChanged:
        return kWKAXSelectedCellsChanged;
    case WebCore::AXObjectCache::AXNotification::AXSelectedStateChanged:
        return kWKAXSelectedStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXSelectedTextChanged:
        return kWKAXSelectedTextChanged;
    case WebCore::AXObjectCache::AXNotification::AXSetSizeChanged:
        return kWKAXSetSizeChanged;
    case WebCore::AXObjectCache::AXNotification::AXTableHeadersChanged:
        return kWKAXTableHeadersChanged;
    case WebCore::AXObjectCache::AXNotification::AXTextCompositionBegan:
        return kWKAXTextCompositionBegan;
    case WebCore::AXObjectCache::AXNotification::AXTextCompositionEnded:
        return kWKAXTextCompositionEnded;
    case WebCore::AXObjectCache::AXNotification::AXURLChanged:
        return kWKAXURLChanged;
    case WebCore::AXObjectCache::AXNotification::AXValueChanged:
        return kWKAXValueChanged;
    case WebCore::AXObjectCache::AXNotification::AXVisibilityChanged:
        return kWKAXVisibilityChanged;
    case WebCore::AXObjectCache::AXNotification::AXScrolledToAnchor:
        return kWKAXScrolledToAnchor;
    case WebCore::AXObjectCache::AXNotification::AXLabelChanged:
        return kWKAXLabelChanged;
    case WebCore::AXObjectCache::AXNotification::AXLiveRegionCreated:
        return kWKAXLiveRegionCreated;
    case WebCore::AXObjectCache::AXNotification::AXLiveRegionChanged:
        return kWKAXLiveRegionChanged;
    case WebCore::AXObjectCache::AXNotification::AXLiveRegionRelevantChanged:
        return kWKAXLiveRegionRelevantChanged;
    case WebCore::AXObjectCache::AXNotification::AXLiveRegionStatusChanged:
        return kWKAXLiveRegionStatusChanged;
    case WebCore::AXObjectCache::AXNotification::AXMaximumValueChanged:
        return kWKAXMaximumValueChanged;
    case WebCore::AXObjectCache::AXNotification::AXMenuListItemSelected:
        return kWKAXMenuListItemSelected;
    case WebCore::AXObjectCache::AXNotification::AXMenuListValueChanged:
        return kWKAXMenuListValueChanged;
    case WebCore::AXObjectCache::AXNotification::AXMenuClosed:
        return kWKAXMenuClosed;
    case WebCore::AXObjectCache::AXNotification::AXMenuOpened:
        return kWKAXMenuOpened;
    case WebCore::AXObjectCache::AXNotification::AXMinimumValueChanged:
        return kWKAXMinimumValueChanged;
    case WebCore::AXObjectCache::AXNotification::AXMultiSelectableStateChanged:
        return kWKAXMultiSelectableStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXOrientationChanged:
        return kWKAXOrientationChanged;
    case WebCore::AXObjectCache::AXNotification::AXRowCountChanged:
        return kWKAXRowCountChanged;
    case WebCore::AXObjectCache::AXNotification::AXRowCollapsed:
        return kWKAXRowCollapsed;
    case WebCore::AXObjectCache::AXNotification::AXRowExpanded:
        return kWKAXRowExpanded;
    case WebCore::AXObjectCache::AXNotification::AXExpandedChanged:
        return kWKAXExpandedChanged;
    case WebCore::AXObjectCache::AXNotification::AXInvalidStatusChanged:
        return kWKAXInvalidStatusChanged;
    case WebCore::AXObjectCache::AXNotification::AXPressDidSucceed:
        return kWKAXPressDidSucceed;
    case WebCore::AXObjectCache::AXNotification::AXPressDidFail:
        return kWKAXPressDidFail;
    case WebCore::AXObjectCache::AXNotification::AXPressedStateChanged:
        return kWKAXPressedStateChanged;
    case WebCore::AXObjectCache::AXNotification::AXReadOnlyStatusChanged:
        return kWKAXReadOnlyStatusChanged;
    case WebCore::AXObjectCache::AXNotification::AXRequiredStatusChanged:
        return kWKAXRequiredStatusChanged;
    case WebCore::AXObjectCache::AXNotification::AXSortDirectionChanged:
        return kWKAXSortDirectionChanged;
    case WebCore::AXObjectCache::AXNotification::AXTextChanged:
        return kWKAXTextChanged;
    case WebCore::AXObjectCache::AXNotification::AXTextCompositionChanged:
        return kWKAXTextCompositionChanged;
    case WebCore::AXObjectCache::AXNotification::AXTextSecurityChanged:
        return kWKAXTextSecurityChanged;
    case WebCore::AXObjectCache::AXNotification::AXElementBusyChanged:
        return kWKAXElementBusyChanged;
    case WebCore::AXObjectCache::AXNotification::AXDraggingStarted:
        return kWKAXDraggingStarted;
    case WebCore::AXObjectCache::AXNotification::AXDraggingEnded:
        return kWKAXDraggingEnded;
    case WebCore::AXObjectCache::AXNotification::AXDraggingEnteredDropZone:
        return kWKAXDraggingEnteredDropZone;
    case WebCore::AXObjectCache::AXNotification::AXDraggingDropped:
        return kWKAXDraggingDropped;
    case WebCore::AXObjectCache::AXNotification::AXDraggingExitedDropZone:
        return kWKAXDraggingExitedDropZone;
    default:
        ASSERT_NOT_REACHED();
        return kWKAXActiveDescendantChanged;
    }
}

#if !PLATFORM(COCOA)
inline WKAXTextChange toAPI(WebCore::AXTextChange textChange)
{
    switch (textChange) {
    case WebCore::AXTextChange::AXTextInserted:
        return kWKAXTextInserted;
    case WebCore::AXTextChange::AXTextDeleted:
        return kWKAXTextDeleted;
    case WebCore::AXTextChange::AXTextAttributesChanged:
        return kWKAXTextAttributesChanged;
    default:
        ASSERT_NOT_REACHED();
        return kWKAXTextInserted;
    }
}
#endif

inline WKAXLoadingEvent toAPI(WebCore::AXObjectCache::AXLoadingEvent event)
{
    switch (event) {
    case WebCore::AXObjectCache::AXLoadingEvent::AXLoadingStarted:
        return kWKAXLoadingStarted;
    case WebCore::AXObjectCache::AXLoadingEvent::AXLoadingReloaded:
        return kWKAXLoadingReloaded;
    case WebCore::AXObjectCache::AXLoadingEvent::AXLoadingFailed:
        return kWKAXLoadingFailed;
    case WebCore::AXObjectCache::AXLoadingEvent::AXLoadingFinished:
        return kWKAXLoadingFinished;
    default:
        ASSERT_NOT_REACHED();
        return kWKAXLoadingStarted;
    }
}

inline WKAXRole toAPI(WebCore::AccessibilityRole role)
{
    switch (role) {
    case WebCore::AccessibilityRole::Application:
        return kWKApplication;
    case WebCore::AccessibilityRole::ApplicationAlert:
        return kWKApplicationAlert;
    case WebCore::AccessibilityRole::ApplicationAlertDialog:
        return kWKApplicationAlertDialog;
    case WebCore::AccessibilityRole::ApplicationDialog:
        return kWKApplicationDialog;
    case WebCore::AccessibilityRole::ApplicationGroup:
        return kWKApplicationGroup;
    case WebCore::AccessibilityRole::ApplicationLog:
        return kWKApplicationLog;
    case WebCore::AccessibilityRole::ApplicationMarquee:
        return kWKApplicationMarquee;
    case WebCore::AccessibilityRole::ApplicationStatus:
        return kWKApplicationStatus;
    case WebCore::AccessibilityRole::ApplicationTextGroup:
        return kWKApplicationTextGroup;
    case WebCore::AccessibilityRole::ApplicationTimer:
        return kWKApplicationTimer;
    case WebCore::AccessibilityRole::Audio:
        return kWKAudio;
    case WebCore::AccessibilityRole::Blockquote:
        return kWKBlockquote;
    case WebCore::AccessibilityRole::Button:
        return kWKButton;
    case WebCore::AccessibilityRole::Canvas:
        return kWKCanvas;
    case WebCore::AccessibilityRole::Caption:
        return kWKCaption;
    case WebCore::AccessibilityRole::Cell:
        return kWKCell;
    case WebCore::AccessibilityRole::Checkbox:
        return kWKCheckBox;
    case WebCore::AccessibilityRole::Code:
        return kWKCode;
    case WebCore::AccessibilityRole::ColorWell:
        return kWKColorWell;
    case WebCore::AccessibilityRole::Column:
        return kWKColumn;
    case WebCore::AccessibilityRole::ColumnHeader:
        return kWKColumnHeader;
    case WebCore::AccessibilityRole::ComboBox:
        return kWKComboBox;
    case WebCore::AccessibilityRole::Definition:
        return kWKDefinition;
    case WebCore::AccessibilityRole::Deletion:
        return kWKDeletion;
    case WebCore::AccessibilityRole::DescriptionList:
        return kWKDescriptionList;
    case WebCore::AccessibilityRole::DescriptionListDetail:
        return kWKDescriptionListDetail;
    case WebCore::AccessibilityRole::DescriptionListTerm:
        return kWKDescriptionListTerm;
    case WebCore::AccessibilityRole::Details:
        return kWKDetails;
    case WebCore::AccessibilityRole::Directory:
        return kWKDirectory;
    case WebCore::AccessibilityRole::Document:
        return kWKDocument;
    case WebCore::AccessibilityRole::DocumentArticle:
        return kWKDocumentArticle;
    case WebCore::AccessibilityRole::DocumentMath:
        return kWKDocumentMath;
    case WebCore::AccessibilityRole::DocumentNote:
        return kWKDocumentNote;
    case WebCore::AccessibilityRole::Feed:
        return kWKFeed;
    case WebCore::AccessibilityRole::Figure:
        return kWKFigure;
    case WebCore::AccessibilityRole::Footer:
        return kWKFooter;
    case WebCore::AccessibilityRole::Footnote:
        return kWKFootnote;
    case WebCore::AccessibilityRole::Form:
        return kWKForm;
    case WebCore::AccessibilityRole::Generic:
        return kWKDiv;
    case WebCore::AccessibilityRole::GraphicsDocument:
        return kWKGraphicsDocument;
    case WebCore::AccessibilityRole::GraphicsObject:
        return kWKGraphicsObject;
    case WebCore::AccessibilityRole::GraphicsSymbol:
        return kWKGraphicsSymbol;
    case WebCore::AccessibilityRole::Grid:
        return kWKGrid;
    case WebCore::AccessibilityRole::GridCell:
        return kWKGridCell;
    case WebCore::AccessibilityRole::Group:
        return kWKGroup;
    case WebCore::AccessibilityRole::Heading:
        return kWKHeading;
    case WebCore::AccessibilityRole::HorizontalRule:
        return kWKHorizontalRule;
    case WebCore::AccessibilityRole::Ignored:
        return kWKIgnored;
    case WebCore::AccessibilityRole::Inline:
        return kWKInline;
    case WebCore::AccessibilityRole::Image:
        return kWKImage;
    case WebCore::AccessibilityRole::ImageMap:
        return kWKImageMap;
    case WebCore::AccessibilityRole::ImageMapLink:
        return kWKImageMapLink;
    case WebCore::AccessibilityRole::Incrementor:
        return kWKIncrementor;
    case WebCore::AccessibilityRole::Insertion:
        return kWKInsertion;
    case WebCore::AccessibilityRole::Label:
        return kWKLabel;
    case WebCore::AccessibilityRole::LandmarkBanner:
        return kWKLandmarkBanner;
    case WebCore::AccessibilityRole::LandmarkComplementary:
        return kWKLandmarkComplementary;
    case WebCore::AccessibilityRole::LandmarkContentInfo:
        return kWKLandmarkContentInfo;
    case WebCore::AccessibilityRole::LandmarkDocRegion:
        return kWKLandmarkDocRegion;
    case WebCore::AccessibilityRole::LandmarkMain:
        return kWKLandmarkMain;
    case WebCore::AccessibilityRole::LandmarkNavigation:
        return kWKLandmarkNavigation;
    case WebCore::AccessibilityRole::LandmarkRegion:
        return kWKLandmarkRegion;
    case WebCore::AccessibilityRole::LandmarkSearch:
        return kWKLandmarkSearch;
    case WebCore::AccessibilityRole::Legend:
        return kWKLegend;
    case WebCore::AccessibilityRole::LineBreak:
        return kWKLineBreak;
    case WebCore::AccessibilityRole::Link:
        return kWKLink;
    case WebCore::AccessibilityRole::List:
        return kWKList;
    case WebCore::AccessibilityRole::ListBox:
        return kWKListBox;
    case WebCore::AccessibilityRole::ListBoxOption:
        return kWKListBoxOption;
    case WebCore::AccessibilityRole::ListItem:
        return kWKListItem;
    case WebCore::AccessibilityRole::ListMarker:
        return kWKListMarker;
    case WebCore::AccessibilityRole::Mark:
        return kWKMark;
    case WebCore::AccessibilityRole::MathElement:
        return kWKMathElement;
    case WebCore::AccessibilityRole::Menu:
        return kWKMenu;
    case WebCore::AccessibilityRole::MenuBar:
        return kWKMenuBar;
    case WebCore::AccessibilityRole::MenuButton:
        return kWKMenuButton;
    case WebCore::AccessibilityRole::MenuItem:
        return kWKMenuItem;
    case WebCore::AccessibilityRole::MenuItemCheckbox:
        return kWKMenuItemCheckbox;
    case WebCore::AccessibilityRole::MenuItemRadio:
        return kWKMenuItemRadio;
    case WebCore::AccessibilityRole::MenuListPopup:
        return kWKMenuListPopup;
    case WebCore::AccessibilityRole::MenuListOption:
        return kWKMenuListOption;
    case WebCore::AccessibilityRole::Meter:
        return kWKMeter;
    case WebCore::AccessibilityRole::Model:
        return kWKModel;
    case WebCore::AccessibilityRole::Paragraph:
        return kWKParagraph;
    case WebCore::AccessibilityRole::PopUpButton:
        return kWKPopUpButton;
    case WebCore::AccessibilityRole::Pre:
        return kWKPre;
    case WebCore::AccessibilityRole::Presentational:
        return kWKPresentational;
    case WebCore::AccessibilityRole::ProgressIndicator:
        return kWKProgressIndicator;
    case WebCore::AccessibilityRole::RadioButton:
        return kWKRadioButton;
    case WebCore::AccessibilityRole::RadioGroup:
        return kWKRadioGroup;
    case WebCore::AccessibilityRole::RowHeader:
        return kWKRowHeader;
    case WebCore::AccessibilityRole::Row:
        return kWKRow;
    case WebCore::AccessibilityRole::RowGroup:
        return kWKRowGroup;
    case WebCore::AccessibilityRole::RubyBase:
        return kWKRubyBase;
    case WebCore::AccessibilityRole::RubyBlock:
        return kWKRubyBlock;
    case WebCore::AccessibilityRole::RubyInline:
        return kWKRubyInline;
    case WebCore::AccessibilityRole::RubyRun:
        return kWKRubyRun;
    case WebCore::AccessibilityRole::RubyText:
        return kWKRubyText;
    case WebCore::AccessibilityRole::ScrollArea:
        return kWKScrollArea;
    case WebCore::AccessibilityRole::ScrollBar:
        return kWKScrollBar;
    case WebCore::AccessibilityRole::SearchField:
        return kWKSearchField;
    case WebCore::AccessibilityRole::Slider:
        return kWKSlider;
    case WebCore::AccessibilityRole::SliderThumb:
        return kWKSliderThumb;
    case WebCore::AccessibilityRole::SpinButton:
        return kWKSpinButton;
    case WebCore::AccessibilityRole::SpinButtonPart:
        return kWKSpinButtonPart;
    case WebCore::AccessibilityRole::Splitter:
        return kWKSplitter;
    case WebCore::AccessibilityRole::StaticText:
        return kWKStaticText;
    case WebCore::AccessibilityRole::Subscript:
        return kWKSubscript;
    case WebCore::AccessibilityRole::Suggestion:
        return kWKSuggestion;
    case WebCore::AccessibilityRole::Summary:
        return kWKSummary;
    case WebCore::AccessibilityRole::Superscript:
        return kWKSuperscript;
    case WebCore::AccessibilityRole::Switch:
        return kWKSwitch;
    case WebCore::AccessibilityRole::SVGRoot:
        return kWKSVGRoot;
    case WebCore::AccessibilityRole::SVGText:
        return kWKSVGText;
    case WebCore::AccessibilityRole::SVGTSpan:
        return kWKSVGTSpan;
    case WebCore::AccessibilityRole::SVGTextPath:
        return kWKSVGTextPath;
    case WebCore::AccessibilityRole::TabGroup:
        return kWKTabGroup;
    case WebCore::AccessibilityRole::TabList:
        return kWKTabList;
    case WebCore::AccessibilityRole::TabPanel:
        return kWKTabPanel;
    case WebCore::AccessibilityRole::Tab:
        return kWKTab;
    case WebCore::AccessibilityRole::Table:
        return kWKTable;
    case WebCore::AccessibilityRole::TableHeaderContainer:
        return kWKTableHeaderContainer;
    case WebCore::AccessibilityRole::Term:
        return kWKTerm;
    case WebCore::AccessibilityRole::TextArea:
        return kWKTextArea;
    case WebCore::AccessibilityRole::TextField:
        return kWKTextField;
    case WebCore::AccessibilityRole::TextGroup:
        return kWKTextGroup;
    case WebCore::AccessibilityRole::Time:
        return kWKTime;
    case WebCore::AccessibilityRole::Tree:
        return kWKTree;
    case WebCore::AccessibilityRole::TreeGrid:
        return kWKTreeGrid;
    case WebCore::AccessibilityRole::TreeItem:
        return kWKTreeItem;
    case WebCore::AccessibilityRole::ToggleButton:
        return kWKToggleButton;
    case WebCore::AccessibilityRole::Toolbar:
        return kWKToolbar;
    case WebCore::AccessibilityRole::Unknown:
        return kWKUnknown;
    case WebCore::AccessibilityRole::UserInterfaceTooltip:
        return kWKUserInterfaceTooltip;
    case WebCore::AccessibilityRole::Video:
        return kWKVideo;
    case WebCore::AccessibilityRole::WebApplication:
        return kWKWebApplication;
    case WebCore::AccessibilityRole::WebArea:
        return kWKWebArea;
    case WebCore::AccessibilityRole::WebCoreLink:
        return kWKWebCoreLink;
    default:
        ASSERT_NOT_REACHED();
        return kWKAnnotation;
    }

}

inline WKAXButtonState toAPI(WebCore::AccessibilityButtonState state)
{
    switch (state) {
    case WebCore::AccessibilityButtonState::Off:
        return kWKButtonStateOff;
    case WebCore::AccessibilityButtonState::On:
        return kWKButtonStateOn;
    case WebCore::AccessibilityButtonState::Mixed:
        return kWKButtonStateMixed;
    default:
        ASSERT_NOT_REACHED();
        return kWKButtonStateOff;
    }
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
