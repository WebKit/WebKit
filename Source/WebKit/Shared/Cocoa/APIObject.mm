/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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
#import "WKWebViewConfigurationInternal.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataRecordInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WKWindowFeaturesInternal.h"
#import "_WKApplicationManifestInternal.h"
#import "_WKAttachmentInternal.h"
#import "_WKAutomationSessionInternal.h"
#import "_WKContentRuleListActionInternal.h"
#import "_WKContextMenuElementInfoInternal.h"
#import "_WKCustomHeaderFieldsInternal.h"
#import "_WKDataTaskInternal.h"
#import "_WKFeatureInternal.h"
#import "_WKFrameHandleInternal.h"
#import "_WKFrameTreeNodeInternal.h"
#import "_WKGeolocationPositionInternal.h"
#import "_WKHitTestResultInternal.h"
#import "_WKInspectorConfigurationInternal.h"
#import "_WKInspectorDebuggableInfoInternal.h"
#import "_WKInspectorInternal.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import "_WKResourceLoadInfoInternal.h"
#import "_WKResourceLoadStatisticsFirstPartyInternal.h"
#import "_WKResourceLoadStatisticsThirdPartyInternal.h"
#import "_WKTargetedElementInfoInternal.h"
#import "_WKTargetedElementRequestInternal.h"
#import "_WKUserContentWorldInternal.h"
#import "_WKUserInitiatedActionInternal.h"
#import "_WKUserStyleSheetInternal.h"
#import "_WKVisitedLinkStoreInternal.h"
#import "_WKWebAuthenticationAssertionResponseInternal.h"
#import "_WKWebAuthenticationPanelInternal.h"
#import "_WKWebPushDaemonConnectionInternal.h"
#import "_WKWebPushMessageInternal.h"
#import "_WKWebPushSubscriptionDataInternal.h"
#import "_WKWebsiteDataStoreConfigurationInternal.h"

#if ENABLE(INSPECTOR_EXTENSIONS)
#import "_WKInspectorExtensionInternal.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#import "WKWebExtensionActionInternal.h"
#import "WKWebExtensionCommandInternal.h"
#import "WKWebExtensionContextInternal.h"
#import "WKWebExtensionControllerConfigurationInternal.h"
#import "WKWebExtensionControllerInternal.h"
#import "WKWebExtensionDataRecordInternal.h"
#import "WKWebExtensionInternal.h"
#import "WKWebExtensionMatchPatternInternal.h"
#import "WKWebExtensionMessagePortInternal.h"
#import "_WKWebExtensionSidebarInternal.h"
#endif

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static const size_t minimumObjectAlignment = alignof(std::aligned_storage<std::numeric_limits<size_t>::max()>::type);
ALLOW_DEPRECATED_DECLARATIONS_END
static_assert(minimumObjectAlignment >= alignof(void*), "Objects should always be at least pointer-aligned.");
static const size_t maximumExtraSpaceForAlignment = minimumObjectAlignment - alignof(void*);

namespace API {

void Object::ref() const
{
    CFRetain(m_wrapper);
}

void Object::deref() const
{
    CFRelease(m_wrapper);
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
    case Type::ApplicationManifest:
        wrapper = [_WKApplicationManifest alloc];
        break;

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

    case Type::PageConfiguration:
        wrapper = [WKWebViewConfiguration alloc];
        break;

    case Type::Data:
        wrapper = [WKNSData alloc];
        break;

    case Type::DataTask:
        wrapper = [_WKDataTask alloc];
        break;

    case Type::Dictionary:
        wrapper = [WKNSDictionary alloc];
        break;

    case Type::Download:
        wrapper = [WKDownload alloc];
        break;

    case Type::Error:
        wrapper = allocateWKObject([WKNSError class], size);
        break;

    case Type::Feature:
        wrapper = [_WKFeature alloc];
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

    case Type::OpenPanelParameters:
        wrapper = [WKOpenPanelParameters alloc];
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

#if PLATFORM(MAC)
    case Type::ContextMenuElementInfoMac:
        wrapper = [_WKContextMenuElementInfo alloc];
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

    case Type::TargetedElementInfo:
        wrapper = [_WKTargetedElementInfo alloc];
        break;

    case Type::TargetedElementRequest:
        wrapper = [_WKTargetedElementRequest alloc];
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
        wrapper = [WKWebExtension alloc];
        break;

    case Type::WebExtensionContext:
        wrapper = [WKWebExtensionContext alloc];
        break;

    case Type::WebExtensionAction:
        wrapper = [WKWebExtensionAction alloc];
        break;

    case Type::WebExtensionCommand:
        wrapper = [WKWebExtensionCommand alloc];
        break;

    case Type::WebExtensionController:
        wrapper = [WKWebExtensionController alloc];
        break;

    case Type::WebExtensionControllerConfiguration:
        wrapper = [WKWebExtensionControllerConfiguration alloc];
        break;

    case Type::WebExtensionDataRecord:
        wrapper = [WKWebExtensionDataRecord alloc];
        break;

    case Type::WebExtensionMatchPattern:
        wrapper = [WKWebExtensionMatchPattern alloc];
        break;

    case Type::WebExtensionMessagePort:
        wrapper = [WKWebExtensionMessagePort alloc];
        break;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    case Type::WebExtensionSidebar:
        wrapper = [_WKWebExtensionSidebar alloc];
        break;
#endif
#endif // ENABLE(WK_WEB_EXTENSIONS)

    case Type::WebPushDaemonConnection:
        wrapper = [_WKWebPushDaemonConnection alloc];
        break;

    case Type::WebPushMessage:
        wrapper = [_WKWebPushMessage alloc];
        break;

    case Type::WebPushSubscriptionData:
        wrapper = [_WKWebPushSubscriptionData alloc];
        break;

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

    apiObjectsUnderConstruction().add(&object, (__bridge CFTypeRef)wrapper);

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

RetainPtr<NSObject<NSSecureCoding>> Object::toNSObject()
{
    switch (type()) {
    case Object::Type::Dictionary: {
        auto& dictionary = static_cast<API::Dictionary&>(*this);
        auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:dictionary.size()]);
        for (auto& pair : dictionary.map()) {
            if (auto nsObject = pair.value ? pair.value->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>())
                [result setObject:nsObject.get() forKey:(NSString *)pair.key];
        }
        return result;
    }
    case Object::Type::Array: {
        auto& array = static_cast<API::Array&>(*this);
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:array.size()]);
        for (auto& element : array.elements()) {
            if (auto nsObject = element ? element->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>())
                [result addObject:nsObject.get()];
        }
        return result;
    }
    case Object::Type::Double:
        return adoptNS([[NSNumber alloc] initWithDouble:static_cast<API::Double&>(*this).value()]);
    case Object::Type::Boolean:
        return adoptNS([[NSNumber alloc] initWithBool:static_cast<API::Boolean&>(*this).value()]);
    case Object::Type::UInt64:
        return adoptNS([[NSNumber alloc] initWithUnsignedLongLong:static_cast<API::UInt64&>(*this).value()]);
    case Object::Type::Int64:
        return adoptNS([[NSNumber alloc] initWithLongLong:static_cast<API::Int64&>(*this).value()]);
    case Object::Type::Data:
        return API::wrapper(static_cast<API::Data&>(*this));
    case Object::Type::String:
        return (NSString *)static_cast<API::String&>(*this).string();
    default:
        // Other API::Object::Types are intentionally not supported.
        break;
    }
    return nullptr;
}

RefPtr<API::Object> Object::fromNSObject(NSObject<NSSecureCoding> *object)
{
    if ([object isKindOfClass:NSString.class])
        return API::String::create((NSString *)object);
    if ([object isKindOfClass:NSData.class])
        return API::Data::createWithoutCopying((NSData *)object);
    if ([object isKindOfClass:NSNumber.class])
        return API::Double::create([(NSNumber *)object doubleValue]);
    if ([object isKindOfClass:NSArray.class]) {
        NSArray *array = (NSArray *)object;
        Vector<RefPtr<API::Object>> result;
        result.reserveInitialCapacity(array.count);
        for (id member in array) {
            if (auto memberObject = fromNSObject(member))
                result.append(WTFMove(memberObject));
        }
        return API::Array::create(WTFMove(result));
    }
    if ([object isKindOfClass:NSDictionary.class]) {
        __block HashMap<WTF::String, RefPtr<API::Object>> result;
        [(NSDictionary *)object enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, BOOL *stop) {
            if (auto valueObject = fromNSObject(value); valueObject && [key isKindOfClass:NSString.class])
                result.add(key, WTFMove(valueObject));
        }];
        return API::Dictionary::create(WTFMove(result));
    }
    // Other NSObject types are intentionally not supported.
    return nullptr;
}

} // namespace API
