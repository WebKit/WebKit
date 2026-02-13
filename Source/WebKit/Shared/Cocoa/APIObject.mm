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
#import "WKContentRuleListInternal.h"
#import "WKContentRuleListStoreInternal.h"
#import "WKContentWorldConfigurationInternal.h"
#import "WKContentWorldInternal.h"
#import "WKContextMenuElementInfoInternal.h"
#import "WKDownloadInternal.h"
#import "WKFormInfoInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHTTPCookieStoreInternal.h"
#import "WKJSHandleInternal.h"
#import "WKJSScriptingBufferInternal.h"
#import "WKJSSerializedNodeInternal.h"
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
#import "WKScriptMessageInternal.h"
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
#import "_WKJSBuffer.h"
#import "_WKJSHandle.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import "_WKResourceLoadInfoInternal.h"
#import "_WKResourceLoadStatisticsFirstPartyInternal.h"
#import "_WKResourceLoadStatisticsThirdPartyInternal.h"
#import "_WKSerializedNode.h"
#import "_WKTargetedElementInfoInternal.h"
#import "_WKTargetedElementRequestInternal.h"
#import "_WKTextRunInternal.h"
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

static const size_t minimumObjectAlignment = alignof(std::max_align_t);
static_assert(minimumObjectAlignment >= alignof(void*), "Objects should always be at least pointer-aligned.");
static const size_t maximumExtraSpaceForAlignment = minimumObjectAlignment - alignof(void*);

namespace API {

void Object::ref() const
{
    SUPPRESS_UNRETAINED_ARG CFRetain(m_wrapper);
}

void Object::deref() const
{
    SUPPRESS_UNRETAINED_ARG CFRelease(m_wrapper);
}

static id <WKObject> allocateWKObject(Class cls, size_t size)
{
    // This is an alloc function, the result will be adopted once we call init.
    SUPPRESS_RETAINPTR_CTOR_ADOPT return class_createInstance(cls, size + maximumExtraSpaceForAlignment);
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
    SUPPRESS_UNRETAINED_LOCAL id<WKObject> wrapper;

    // Wrappers that inherit from WKObject store the API::Object in their extra bytes, so they are
    // allocated using NSAllocatedObject. The other wrapper classes contain inline storage for the
    // API::Object, so they are allocated using +alloc.

    switch (type) {
    case Type::ApplicationManifest:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKApplicationManifest alloc];
        break;

    case Type::Array:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNSArray alloc];
        break;

#if ENABLE(ATTACHMENT_ELEMENT)
    case Type::Attachment:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKAttachment alloc];
        break;
#endif

    case Type::AuthenticationChallenge:
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = allocateWKObject([WKNSURLAuthenticationChallenge class], size);
ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::AutomationSession:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKAutomationSession alloc];
        break;

    case Type::BackForwardList:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKBackForwardList alloc];
        break;

    case Type::BackForwardListItem:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKBackForwardListItem alloc];
        break;

    case Type::Boolean:
    case Type::Double:
    case Type::UInt64:
    case Type::Int64:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNSNumber alloc];
        ((WKNSNumber *)wrapper)->_type = type;
        break;

    case Type::Bundle:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInController alloc];
        break;

    case Type::BundlePage:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInBrowserContextController alloc];
        break;

    case Type::DebuggableInfo:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKInspectorDebuggableInfo alloc];
        break;

    case Type::Preferences:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKPreferences alloc];
        break;

    case Type::ProcessPool:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKProcessPool alloc];
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::ProcessPoolConfiguration:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKProcessPoolConfiguration alloc];
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::PageConfiguration:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebViewConfiguration alloc];
        break;

    case Type::Data:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNSData alloc];
        break;

    case Type::DataTask:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKDataTask alloc];
        break;

    case Type::Dictionary:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNSDictionary alloc];
        break;

    case Type::Download:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKDownload alloc];
        break;

    case Type::Error:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = allocateWKObject([WKNSError class], size);
        break;

    case Type::Feature:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKFeature alloc];
        break;
        
    case Type::FrameHandle:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKFrameHandle alloc];
        break;

    case Type::FrameInfo:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKFrameInfo alloc];
        break;

    case Type::FrameTreeNode:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKFrameTreeNode alloc];
        break;
#if PLATFORM(IOS_FAMILY)
    case Type::GeolocationPosition:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKGeolocationPosition alloc];
        break;
#endif

    case Type::HTTPCookieStore:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKHTTPCookieStore alloc];
        break;

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    case Type::HitTestResult:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKHitTestResult alloc];
        break;
#endif

    case Type::Inspector:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKInspector alloc];
        break;

    case Type::InspectorConfiguration:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKInspectorConfiguration alloc];
        break;

#if ENABLE(INSPECTOR_EXTENSIONS)
    case Type::InspectorExtension:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKInspectorExtension alloc];
        break;
#endif

    case Type::Navigation:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNavigation alloc];
        break;

    case Type::NavigationAction:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNavigationAction alloc];
        break;

    case Type::NavigationData:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNavigationData alloc];
        break;

    case Type::NavigationResponse:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKNavigationResponse alloc];
        break;

    case Type::OpenPanelParameters:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKOpenPanelParameters alloc];
        break;

    case Type::SecurityOrigin:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKSecurityOrigin alloc];
        break;

    case Type::String:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = allocateWKObject([WKNSString class], size);
        break;

    case Type::URL:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = allocateWKObject([WKNSURL class], size);
        break;

    case Type::URLRequest:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = allocateWKObject([WKNSURLRequest class], size);
        break;

    case Type::URLSchemeTask:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKURLSchemeTaskImpl alloc];
        break;

    case Type::UserContentController:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKUserContentController alloc];
        break;

    case Type::ContentRuleList:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKContentRuleList alloc];
        break;

    case Type::ContentRuleListAction:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKContentRuleListAction alloc];
        break;

    case Type::ContentRuleListStore:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKContentRuleListStore alloc];
        break;

#if PLATFORM(IOS_FAMILY)
    case Type::ContextMenuElementInfo:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKContextMenuElementInfo alloc];
        break;
#endif

#if PLATFORM(MAC)
    case Type::ContextMenuElementInfoMac:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKContextMenuElementInfo alloc];
        break;
#endif

    case Type::CustomHeaderFields:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKCustomHeaderFields alloc];
        break;

    case Type::ResourceLoadInfo:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKResourceLoadInfo alloc];
        break;
            
    case Type::ResourceLoadStatisticsFirstParty:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKResourceLoadStatisticsFirstParty alloc];
        break;

    case Type::ResourceLoadStatisticsThirdParty:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKResourceLoadStatisticsThirdParty alloc];
        break;

    case Type::ContentWorld:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKContentWorld alloc];
        break;

    case Type::ContentWorldConfiguration:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKContentWorldConfiguration alloc];
        break;

    case Type::TargetedElementInfo:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKTargetedElementInfo alloc];
        break;

    case Type::TargetedElementRequest:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKTargetedElementRequest alloc];
        break;

    case Type::TextRun:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKTextRun alloc];
        break;

    case Type::UserInitiatedAction:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKUserInitiatedAction alloc];
        break;

    case Type::UserScript:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKUserScript alloc];
        break;

    case Type::UserStyleSheet:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKUserStyleSheet alloc];
        break;

    case Type::VisitedLinkStore:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKVisitedLinkStore alloc];
        break;

#if ENABLE(WK_WEB_EXTENSIONS)
    case Type::WebExtension:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtension alloc];
        break;

    case Type::WebExtensionContext:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionContext alloc];
        break;

    case Type::WebExtensionAction:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionAction alloc];
        break;

    case Type::WebExtensionCommand:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionCommand alloc];
        break;

    case Type::WebExtensionController:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionController alloc];
        break;

    case Type::WebExtensionControllerConfiguration:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionControllerConfiguration alloc];
        break;

    case Type::WebExtensionDataRecord:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionDataRecord alloc];
        break;

    case Type::WebExtensionMatchPattern:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionMatchPattern alloc];
        break;

    case Type::WebExtensionMessagePort:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebExtensionMessagePort alloc];
        break;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    case Type::WebExtensionSidebar:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebExtensionSidebar alloc];
        break;
#endif
#endif // ENABLE(WK_WEB_EXTENSIONS)

    case Type::WebPushDaemonConnection:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebPushDaemonConnection alloc];
        break;

    case Type::WebPushMessage:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebPushMessage alloc];
        break;

    case Type::WebPushSubscriptionData:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebPushSubscriptionData alloc];
        break;

    case Type::WebsiteDataRecord:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebsiteDataRecord alloc];
        break;

    case Type::WebsiteDataStore:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebsiteDataStore alloc];
        break;
        
    case Type::WebsiteDataStoreConfiguration:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebsiteDataStoreConfiguration alloc];
        break;

    case Type::WebsitePolicies:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebpagePreferences alloc];
        break;

    case Type::WindowFeatures:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWindowFeatures alloc];
        break;

#if ENABLE(WEB_AUTHN)
    case Type::WebAuthenticationPanel:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebAuthenticationPanel alloc];
        break;
    case Type::WebAuthenticationAssertionResponse:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKWebAuthenticationAssertionResponse alloc];
        break;
#endif

    case Type::BundleFrame:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInFrame alloc];
        break;

    case Type::BundleHitTestResult:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInHitTestResult alloc];
        break;

    case Type::BundleCSSStyleDeclarationHandle:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInCSSStyleDeclarationHandle alloc];
        break;

    case Type::BundleNodeHandle:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInNodeHandle alloc];
        break;

    case Type::BundleRangeHandle:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInRangeHandle alloc];
        break;

    case Type::BundleScriptWorld:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKWebProcessPlugInScriptWorld alloc];
        break;

    case Type::ScriptMessage:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKScriptMessage alloc];
        break;

    case Type::SerializedNode:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKSerializedNode alloc];
        break;

    case Type::JSHandle:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKJSHandle alloc];
        break;

    case Type::JSBuffer:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [_WKJSBuffer alloc];
        break;

    case Type::FormInfo:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = [WKFormInfo alloc];
        break;

    default:
        SUPPRESS_RETAINPTR_CTOR_ADOPT wrapper = allocateWKObject([WKObject class], size);
        break;
    }

    apiObjectsUnderConstruction().add(&wrapper._apiObject, (__bridge CFTypeRef)wrapper);
    return &wrapper._apiObject;
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
        auto& dictionary = downcast<API::Dictionary>(*this);
        auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:dictionary.size()]);
        for (auto& pair : dictionary.map()) {
            if (auto nsObject = pair.value ? Ref { *pair.value }->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>())
                [result setObject:nsObject.get() forKey:pair.key.createNSString().get()];
        }
        return result;
    }
    case Object::Type::Array: {
        auto& array = downcast<API::Array>(*this);
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:array.size()]);
        for (auto& element : array.elements()) {
            if (auto nsObject = element ? element->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>())
                [result addObject:nsObject.get()];
        }
        return result;
    }
    case Object::Type::Double:
        return adoptNS([[NSNumber alloc] initWithDouble:downcast<API::Double>(*this).value()]);
    case Object::Type::Boolean:
        return adoptNS([[NSNumber alloc] initWithBool:downcast<API::Boolean>(*this).value()]);
    case Object::Type::UInt64:
        return adoptNS([[NSNumber alloc] initWithUnsignedLongLong:downcast<API::UInt64>(*this).value()]);
    case Object::Type::Int64:
        return adoptNS([[NSNumber alloc] initWithLongLong:downcast<API::Int64>(*this).value()]);
    case Object::Type::Data:
        return API::wrapper(downcast<API::Data>(*this));
    case Object::Type::String:
        return downcast<API::String>(*this).string().createNSString();
    default:
        // Other API::Object::Types are intentionally not supported.
        break;
    }
    return nullptr;
}

RefPtr<API::Object> Object::fromNSObject(NSObject<NSSecureCoding> *object)
{
    if (auto *str = dynamic_objc_cast<NSString>(object))
        return API::String::create(str);
    if (auto *data = dynamic_objc_cast<NSData>(object))
        return API::Data::createWithoutCopying(data);
    if (auto *number = dynamic_objc_cast<NSNumber>(object))
        return API::Double::create([number doubleValue]);
    if (auto *array = dynamic_objc_cast<NSArray>(object)) {
        Vector<RefPtr<API::Object>> result;
        result.reserveInitialCapacity(array.count);
        for (id member in array) {
            if (auto memberObject = fromNSObject(member))
                result.append(WTF::move(memberObject));
        }
        return API::Array::create(WTF::move(result));
    }
    if (auto *dictionary = dynamic_objc_cast<NSDictionary>(object)) {
        __block HashMap<WTF::String, RefPtr<API::Object>> result;
        [dictionary enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, BOOL *stop) {
            if (auto valueObject = fromNSObject(value); valueObject && [key isKindOfClass:NSString.class])
                result.add(key, WTF::move(valueObject));
        }];
        return API::Dictionary::create(WTF::move(result));
    }
    // Other NSObject types are intentionally not supported.
    return nullptr;
}

} // namespace API
