/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "config.h"
#import "APIObject.h"

#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupInternal.h"
#import "WKConnectionInternal.h"
#import "WKContentRuleListInternal.h"
#import "WKContentRuleListStoreInternal.h"
#import "WKContentWorldInternal.h"
#import "WKContextMenuElementInfoInternal.h"
#import "WKDownloadInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHTTPCookieStoreInternal.h"
#import "WKNSArray.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNSError.h"
#import "WKNSNumber.h"
#import "WKNSString.h"
#import "WKNSURL.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLRequest.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDataInternal.h"
#import "WKNavigationInternal.h"
#import "WKNavigationResponseInternal.h"
#import "WKOpenPanelParametersInternal.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKURLSchemeTaskInternal.h"
#import "WKUserContentControllerInternal.h"
#import "WKUserScriptInternal.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WKWebProcessPlugInCSSStyleDeclarationHandleInternal.h"
#import "WKWebProcessPlugInFrameInternal.h"
#import "WKWebProcessPlugInHitTestResultInternal.h"
#import "WKWebProcessPlugInInternal.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInRangeHandleInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataRecordInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WKWindowFeaturesInternal.h"
#import "_WKAttachmentInternal.h"
#import "_WKAutomationSessionInternal.h"
#import "_WKContentRuleListActionInternal.h"
#import "_WKCustomHeaderFieldsInternal.h"
#import "_WKDataTaskInternal.h"
#import "_WKExperimentalFeatureInternal.h"
#import "_WKFrameHandleInternal.h"
#import "_WKFrameTreeNodeInternal.h"
#import "_WKGeolocationPositionInternal.h"
#import "_WKHitTestResultInternal.h"
#import "_WKInspectorConfigurationInternal.h"
#import "_WKInspectorDebuggableInfoInternal.h"
#import "_WKInspectorInternal.h"
#import "_WKInternalDebugFeatureInternal.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import "_WKResourceLoadInfoInternal.h"
#import "_WKResourceLoadStatisticsFirstPartyInternal.h"
#import "_WKResourceLoadStatisticsThirdPartyInternal.h"
#import "_WKUserContentWorldInternal.h"
#import "_WKUserInitiatedActionInternal.h"
#import "_WKUserStyleSheetInternal.h"
#import "_WKVisitedLinkStoreInternal.h"
#import "_WKWebAuthenticationAssertionResponseInternal.h"
#import "_WKWebAuthenticationPanelInternal.h"
#import "_WKWebsiteDataStoreConfigurationInternal.h"

#if ENABLE(APPLICATION_MANIFEST)
#import "_WKApplicationManifestInternal.h"
#endif

#if ENABLE(INSPECTOR_EXTENSIONS)
#import "_WKInspectorExtensionInternal.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#import "_WKWebExtensionControllerInternal.h"
#import "_WKWebExtensionInternal.h"
#import "_WKWebExtensionMatchPatternInternal.h"
#endif

static const size_t minimumObjectAlignment = alignof(std::aligned_storage<std::numeric_limits<size_t>::max()>::type);
static_assert(minimumObjectAlignment >= alignof(void*), "Objects should always be at least pointer-aligned.");
static const size_t maximumExtraSpaceForAlignment = minimumObjectAlignment - alignof(void*);

namespace API {

void Object::ref() const
{
    CFRetain((__bridge CFTypeRef)wrapper());
}

void Object::deref() const
{
    CFRelease((__bridge CFTypeRef)wrapper());
}

static id <WKObject> allocateWKObject(Class cls, size_t size)
{
    return class_createInstance(cls, size + maximumExtraSpaceForAlignment);
}

API::Object& Object::fromWKObjectExtraSpace(id <WKObject> obj)
{
    size_t size = sizeof(API::Object);
    size_t spaceAvailable = size + maximumExtraSpaceForAlignment;
    void* indexedIvars = object_getIndexedIvars(obj);
    return *static_cast<API::Object*>(std::align(minimumObjectAlignment, size, indexedIvars, spaceAvailable));
}

void* Object::newObject(size_t size, Type type)
{
    id <WKObject> wrapper;

    // Wrappers that inherit from WKObject store the API::Object in their extra bytes, so they are
    // allocated using NSAllocatedObject. The other wrapper classes contain inline storage for the
    // API::Object, so they are allocated using +alloc.

    switch (type) {
#if ENABLE(APPLICATION_MANIFEST)
    case Type::ApplicationManifest:
        wrapper = [_WKApplicationManifest alloc];
        break;
#endif

    case Type::Array:
        wrapper = [WKNSArray alloc];
        break;

#if ENABLE(ATTACHMENT_ELEMENT)
    case Type::Attachment:
        wrapper = [_WKAttachment alloc];
        break;
#endif

    case Type::AuthenticationChallenge:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        wrapper = allocateWKObject([WKNSURLAuthenticationChallenge class], size);
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::AutomationSession:
        wrapper = [_WKAutomationSession alloc];
        break;

    case Type::BackForwardList:
        wrapper = [WKBackForwardList alloc];
        break;

    case Type::BackForwardListItem:
        wrapper = [WKBackForwardListItem alloc];
        break;

    case Type::Boolean:
    case Type::Double:
    case Type::UInt64:
    case Type::Int64:
        wrapper = [WKNSNumber alloc];
        ((WKNSNumber *)wrapper)->_type = type;
        break;

    case Type::Bundle:
        wrapper = [WKWebProcessPlugInController alloc];
        break;

    case Type::BundlePage:
        wrapper = [WKWebProcessPlugInBrowserContextController alloc];
        break;

    case Type::Connection:
        // While not actually a WKObject instance, WKConnection uses allocateWKObject to allocate extra space
        // instead of using ObjectStorage because the wrapped C++ object is a subclass of WebConnection.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        wrapper = allocateWKObject([WKConnection class], size);
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::DebuggableInfo:
        wrapper = [_WKInspectorDebuggableInfo alloc];
        break;

    case Type::Preferences:
        wrapper = [WKPreferences alloc];
        break;

    case Type::ProcessPool:
        wrapper = [WKProcessPool alloc];
        break;

    case Type::ProcessPoolConfiguration:
        wrapper = [_WKProcessPoolConfiguration alloc];
        break;

    case Type::Data:
        wrapper = [WKNSData alloc];
        break;

    case Type::DataTask:
        wrapper = [_WKDataTask alloc];
        break;

    case Type::InternalDebugFeature:
        wrapper = [_WKInternalDebugFeature alloc];
        break;

    case Type::Dictionary:
        wrapper = [WKNSDictionary alloc];
        break;

    case Type::Download:
        wrapper = [WKDownload alloc];
        break;

    case Type::ExperimentalFeature:
        wrapper = [_WKExperimentalFeature alloc];
        break;

    case Type::Error:
        wrapper = allocateWKObject([WKNSError class], size);
        break;

    case Type::FrameHandle:
        wrapper = [_WKFrameHandle alloc];
        break;

    case Type::FrameInfo:
        wrapper = [WKFrameInfo alloc];
        break;

    case Type::FrameTreeNode:
        wrapper = [_WKFrameTreeNode alloc];
        break;
#if PLATFORM(IOS_FAMILY)
    case Type::GeolocationPosition:
        wrapper = [_WKGeolocationPosition alloc];
        break;
#endif

    case Type::HTTPCookieStore:
        wrapper = [WKHTTPCookieStore alloc];
        break;

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    case Type::HitTestResult:
        wrapper = [_WKHitTestResult alloc];
        break;
#endif

    case Type::Inspector:
        wrapper = [_WKInspector alloc];
        break;

    case Type::InspectorConfiguration:
        wrapper = [_WKInspectorConfiguration alloc];
        break;

#if ENABLE(INSPECTOR_EXTENSIONS)
    case Type::InspectorExtension:
        wrapper = [_WKInspectorExtension alloc];
        break;
#endif

    case Type::Navigation:
        wrapper = [WKNavigation alloc];
        break;

    case Type::NavigationAction:
        wrapper = [WKNavigationAction alloc];
        break;

    case Type::NavigationData:
        wrapper = [WKNavigationData alloc];
        break;

    case Type::NavigationResponse:
        wrapper = [WKNavigationResponse alloc];
        break;

#if PLATFORM(MAC)
    case Type::OpenPanelParameters:
        wrapper = [WKOpenPanelParameters alloc];
        break;
#endif

    case Type::PageGroup:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        wrapper = [WKBrowsingContextGroup alloc];
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::SecurityOrigin:
        wrapper = [WKSecurityOrigin alloc];
        break;

    case Type::String:
        wrapper = allocateWKObject([WKNSString class], size);
        break;

    case Type::URL:
        wrapper = allocateWKObject([WKNSURL class], size);
        break;

    case Type::URLRequest:
        wrapper = allocateWKObject([WKNSURLRequest class], size);
        break;

    case Type::URLSchemeTask:
        wrapper = [WKURLSchemeTaskImpl alloc];
        break;

    case Type::UserContentController:
        wrapper = [WKUserContentController alloc];
        break;

    case Type::ContentRuleList:
        wrapper = [WKContentRuleList alloc];
        break;

    case Type::ContentRuleListAction:
        wrapper = [_WKContentRuleListAction alloc];
        break;

    case Type::ContentRuleListStore:
        wrapper = [WKContentRuleListStore alloc];
        break;

#if PLATFORM(IOS_FAMILY)
    case Type::ContextMenuElementInfo:
        wrapper = [WKContextMenuElementInfo alloc];
        break;
#endif

    case Type::CustomHeaderFields:
        wrapper = [_WKCustomHeaderFields alloc];
        break;

    case Type::ResourceLoadInfo:
        wrapper = [_WKResourceLoadInfo alloc];
        break;
            
    case Type::ResourceLoadStatisticsFirstParty:
        wrapper = [_WKResourceLoadStatisticsFirstParty alloc];
        break;

    case Type::ResourceLoadStatisticsThirdParty:
        wrapper = [_WKResourceLoadStatisticsThirdParty alloc];
        break;

    case Type::ContentWorld:
        wrapper = [WKContentWorld alloc];
        break;

    case Type::UserInitiatedAction:
        wrapper = [_WKUserInitiatedAction alloc];
        break;

    case Type::UserScript:
        wrapper = [WKUserScript alloc];
        break;

    case Type::UserStyleSheet:
        wrapper = [_WKUserStyleSheet alloc];
        break;

    case Type::VisitedLinkStore:
        wrapper = [_WKVisitedLinkStore alloc];
        break;

#if ENABLE(WK_WEB_EXTENSIONS)
    case Type::WebExtension:
        wrapper = [_WKWebExtension alloc];
        break;

    case Type::WebExtensionController:
        wrapper = [_WKWebExtensionController alloc];
        break;

    case Type::WebExtensionMatchPattern:
        wrapper = [_WKWebExtensionMatchPattern alloc];
        break;
#endif

    case Type::WebsiteDataRecord:
        wrapper = [WKWebsiteDataRecord alloc];
        break;

    case Type::WebsiteDataStore:
        wrapper = [WKWebsiteDataStore alloc];
        break;
        
    case Type::WebsiteDataStoreConfiguration:
        wrapper = [_WKWebsiteDataStoreConfiguration alloc];
        break;

    case Type::WebsitePolicies:
        wrapper = [WKWebpagePreferences alloc];
        break;

    case Type::WindowFeatures:
        wrapper = [WKWindowFeatures alloc];
        break;

#if ENABLE(WEB_AUTHN)
    case Type::WebAuthenticationPanel:
        wrapper = [_WKWebAuthenticationPanel alloc];
        break;
    case Type::WebAuthenticationAssertionResponse:
        wrapper = [_WKWebAuthenticationAssertionResponse alloc];
        break;
#endif

    case Type::BundleFrame:
        wrapper = [WKWebProcessPlugInFrame alloc];
        break;

    case Type::BundleHitTestResult:
        wrapper = [WKWebProcessPlugInHitTestResult alloc];
        break;

    case Type::BundleCSSStyleDeclarationHandle:
        wrapper = [WKWebProcessPlugInCSSStyleDeclarationHandle alloc];
        break;

    case Type::BundleNodeHandle:
        wrapper = [WKWebProcessPlugInNodeHandle alloc];
        break;

    case Type::BundleRangeHandle:
        wrapper = [WKWebProcessPlugInRangeHandle alloc];
        break;

    case Type::BundleScriptWorld:
        wrapper = [WKWebProcessPlugInScriptWorld alloc];
        break;

    default:
        wrapper = allocateWKObject([WKObject class], size);
        break;
    }

    Object& object = wrapper._apiObject;
    object.m_wrapper = wrapper;

    return &object;
}

void* Object::wrap(API::Object* object)
{
    if (!object)
        return nullptr;

    return (__bridge void*)object->wrapper();
}

API::Object* Object::unwrap(void* object)
{
    if (!object)
        return nullptr;

    return &((__bridge id <WKObject>)object)._apiObject;
}

} // namespace API
