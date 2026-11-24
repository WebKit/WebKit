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
#import "WKDOMNodeSnapshotInternal.h"
#import "WKDownloadInternal.h"
#import "WKFormInfoInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHTTPCookieStoreInternal.h"
#import "WKJSHandleInternal.h"
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
#import "_WKJSHandle.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import "_WKResourceLoadInfoInternal.h"
#import "_WKResourceLoadStatisticsFirstPartyInternal.h"
#import "_WKResourceLoadStatisticsThirdPartyInternal.h"
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

// Use a raw CFTypeRef to avoid ARC retain/release traffic before the object is fully constructed.
static CFTypeRef allocateWrapper(Class cls)
{
    return reinterpret_cast<CFTypeRef (*)(Class, SEL)>(objc_msgSend)(cls, @selector(alloc));
}

static CFTypeRef allocateWKObject(Class cls, size_t size)
{
    return reinterpret_cast<CFTypeRef (*)(Class, size_t)>(reinterpret_cast<void (*)()>(class_createInstance))(cls, size + maximumExtraSpaceForAlignment);
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
    SUPPRESS_UNRETAINED_LOCAL CFTypeRef wrapper = nullptr;

    // Wrappers that inherit from WKObject store the API::Object in their extra bytes, so they are
    // allocated using NSAllocatedObject. The other wrapper classes contain inline storage for the
    // API::Object, so they are allocated using +alloc.

    switch (type) {
    case Type::ApplicationManifest:
        wrapper = allocateWrapper(_WKApplicationManifest.class);
        break;

    case Type::Array:
        wrapper = allocateWrapper(WKNSArray.class);
        break;

#if ENABLE(ATTACHMENT_ELEMENT)
    case Type::Attachment:
        wrapper = allocateWrapper(_WKAttachment.class);
        break;
#endif

    case Type::AuthenticationChallenge:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        wrapper = allocateWKObject([WKNSURLAuthenticationChallenge class], size);
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::AutomationSession:
        wrapper = allocateWrapper(_WKAutomationSession.class);
        break;

    case Type::BackForwardList:
        wrapper = allocateWrapper(WKBackForwardList.class);
        break;

    case Type::BackForwardListItem:
        wrapper = allocateWrapper(WKBackForwardListItem.class);
        break;

    case Type::Boolean:
    case Type::Double:
    case Type::UInt64:
    case Type::Int64:
        wrapper = allocateWrapper(WKNSNumber.class);
        ((__bridge WKNSNumber *)wrapper)->_type = type;
        break;

    case Type::Bundle:
        wrapper = allocateWrapper(WKWebProcessPlugInController.class);
        break;

    case Type::BundlePage:
        wrapper = allocateWrapper(WKWebProcessPlugInBrowserContextController.class);
        break;

    case Type::DebuggableInfo:
        wrapper = allocateWrapper(_WKInspectorDebuggableInfo.class);
        break;

    case Type::Preferences:
        wrapper = allocateWrapper(WKPreferences.class);
        break;

    case Type::ProcessPool:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        wrapper = allocateWrapper(WKProcessPool.class);
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::ProcessPoolConfiguration:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        wrapper = allocateWrapper(_WKProcessPoolConfiguration.class);
        ALLOW_DEPRECATED_DECLARATIONS_END
        break;

    case Type::PageConfiguration:
        wrapper = allocateWrapper(WKWebViewConfiguration.class);
        break;

    case Type::Data:
        wrapper = allocateWrapper(WKNSData.class);
        break;

    case Type::DataTask:
        wrapper = allocateWrapper(_WKDataTask.class);
        break;

    case Type::Dictionary:
        wrapper = allocateWrapper(WKNSDictionary.class);
        break;

    case Type::Download:
        wrapper = allocateWrapper(WKDownload.class);
        break;

    case Type::Error:
        wrapper = allocateWKObject([WKNSError class], size);
        break;

    case Type::Feature:
        wrapper = allocateWrapper(_WKFeature.class);
        break;
        
    case Type::FrameHandle:
        wrapper = allocateWrapper(_WKFrameHandle.class);
        break;

    case Type::FrameInfo:
        wrapper = allocateWrapper(WKFrameInfo.class);
        break;

    case Type::FrameTreeNode:
        wrapper = allocateWrapper(_WKFrameTreeNode.class);
        break;
#if PLATFORM(IOS_FAMILY)
    case Type::GeolocationPosition:
        wrapper = allocateWrapper(_WKGeolocationPosition.class);
        break;
#endif

    case Type::HTTPCookieStore:
        wrapper = allocateWrapper(WKHTTPCookieStore.class);
        break;

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    case Type::HitTestResult:
        wrapper = allocateWrapper(_WKHitTestResult.class);
        break;
#endif

    case Type::Inspector:
        wrapper = allocateWrapper(_WKInspector.class);
        break;

    case Type::InspectorConfiguration:
        wrapper = allocateWrapper(_WKInspectorConfiguration.class);
        break;

#if ENABLE(INSPECTOR_EXTENSIONS)
    case Type::InspectorExtension:
        wrapper = allocateWrapper(_WKInspectorExtension.class);
        break;
#endif

    case Type::Navigation:
        wrapper = allocateWrapper(WKNavigation.class);
        break;

    case Type::NavigationAction:
        wrapper = allocateWrapper(WKNavigationAction.class);
        break;

    case Type::NavigationData:
        wrapper = allocateWrapper(WKNavigationData.class);
        break;

    case Type::NavigationResponse:
        wrapper = allocateWrapper(WKNavigationResponse.class);
        break;

    case Type::OpenPanelParameters:
        wrapper = allocateWrapper(WKOpenPanelParameters.class);
        break;

    case Type::SecurityOrigin:
        wrapper = allocateWrapper(WKSecurityOrigin.class);
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
        wrapper = allocateWrapper(WKURLSchemeTaskImpl.class);
        break;

    case Type::UserContentController:
        wrapper = allocateWrapper(WKUserContentController.class);
        break;

    case Type::ContentRuleList:
        wrapper = allocateWrapper(WKContentRuleList.class);
        break;

    case Type::ContentRuleListAction:
        wrapper = allocateWrapper(_WKContentRuleListAction.class);
        break;

    case Type::ContentRuleListStore:
        wrapper = allocateWrapper(WKContentRuleListStore.class);
        break;

#if PLATFORM(IOS_FAMILY)
    case Type::ContextMenuElementInfo:
        wrapper = allocateWrapper(WKContextMenuElementInfo.class);
        break;
#endif

#if PLATFORM(MAC)
    case Type::ContextMenuElementInfoMac:
        wrapper = allocateWrapper(_WKContextMenuElementInfo.class);
        break;
#endif

    case Type::CustomHeaderFields:
        wrapper = allocateWrapper(_WKCustomHeaderFields.class);
        break;

    case Type::ResourceLoadInfo:
        wrapper = allocateWrapper(_WKResourceLoadInfo.class);
        break;
            
    case Type::ResourceLoadStatisticsFirstParty:
        wrapper = allocateWrapper(_WKResourceLoadStatisticsFirstParty.class);
        break;

    case Type::ResourceLoadStatisticsThirdParty:
        wrapper = allocateWrapper(_WKResourceLoadStatisticsThirdParty.class);
        break;

    case Type::ContentWorld:
        wrapper = allocateWrapper(WKContentWorld.class);
        break;

    case Type::ContentWorldConfiguration:
        wrapper = allocateWrapper(WKContentWorldConfiguration.class);
        break;

    case Type::TargetedElementInfo:
        wrapper = allocateWrapper(_WKTargetedElementInfo.class);
        break;

    case Type::TargetedElementRequest:
        wrapper = allocateWrapper(_WKTargetedElementRequest.class);
        break;

    case Type::TextRun:
        wrapper = allocateWrapper(_WKTextRun.class);
        break;

    case Type::UserInitiatedAction:
        wrapper = allocateWrapper(_WKUserInitiatedAction.class);
        break;

    case Type::UserScript:
        wrapper = allocateWrapper(WKUserScript.class);
        break;

    case Type::UserStyleSheet:
        wrapper = allocateWrapper(_WKUserStyleSheet.class);
        break;

    case Type::VisitedLinkStore:
        wrapper = allocateWrapper(_WKVisitedLinkStore.class);
        break;

#if ENABLE(WK_WEB_EXTENSIONS)
    case Type::WebExtension:
        wrapper = allocateWrapper(WKWebExtension.class);
        break;

    case Type::WebExtensionContext:
        wrapper = allocateWrapper(WKWebExtensionContext.class);
        break;

    case Type::WebExtensionAction:
        wrapper = allocateWrapper(WKWebExtensionAction.class);
        break;

    case Type::WebExtensionCommand:
        wrapper = allocateWrapper(WKWebExtensionCommand.class);
        break;

    case Type::WebExtensionController:
        wrapper = allocateWrapper(WKWebExtensionController.class);
        break;

    case Type::WebExtensionControllerConfiguration:
        wrapper = allocateWrapper(WKWebExtensionControllerConfiguration.class);
        break;

    case Type::WebExtensionDataRecord:
        wrapper = allocateWrapper(WKWebExtensionDataRecord.class);
        break;

    case Type::WebExtensionMatchPattern:
        wrapper = allocateWrapper(WKWebExtensionMatchPattern.class);
        break;

    case Type::WebExtensionMessagePort:
        wrapper = allocateWrapper(WKWebExtensionMessagePort.class);
        break;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    case Type::WebExtensionSidebar:
        wrapper = allocateWrapper(_WKWebExtensionSidebar.class);
        break;
#endif
#endif // ENABLE(WK_WEB_EXTENSIONS)

    case Type::WebPushDaemonConnection:
        wrapper = allocateWrapper(_WKWebPushDaemonConnection.class);
        break;

    case Type::WebPushMessage:
        wrapper = allocateWrapper(_WKWebPushMessage.class);
        break;

    case Type::WebPushSubscriptionData:
        wrapper = allocateWrapper(_WKWebPushSubscriptionData.class);
        break;

    case Type::WebsiteDataRecord:
        wrapper = allocateWrapper(WKWebsiteDataRecord.class);
        break;

    case Type::WebsiteDataStore:
        wrapper = allocateWrapper(WKWebsiteDataStore.class);
        break;
        
    case Type::WebsiteDataStoreConfiguration:
        wrapper = allocateWrapper(_WKWebsiteDataStoreConfiguration.class);
        break;

    case Type::WebsitePolicies:
        wrapper = allocateWrapper(WKWebpagePreferences.class);
        break;

    case Type::WindowFeatures:
        wrapper = allocateWrapper(WKWindowFeatures.class);
        break;

#if ENABLE(WEB_AUTHN)
    case Type::WebAuthenticationPanel:
        wrapper = allocateWrapper(_WKWebAuthenticationPanel.class);
        break;
    case Type::WebAuthenticationAssertionResponse:
        wrapper = allocateWrapper(_WKWebAuthenticationAssertionResponse.class);
        break;
#endif

    case Type::BundleFrame:
        wrapper = allocateWrapper(WKWebProcessPlugInFrame.class);
        break;

    case Type::BundleHitTestResult:
        wrapper = allocateWrapper(WKWebProcessPlugInHitTestResult.class);
        break;

    case Type::BundleCSSStyleDeclarationHandle:
        wrapper = allocateWrapper(WKWebProcessPlugInCSSStyleDeclarationHandle.class);
        break;

    case Type::BundleNodeHandle:
        wrapper = allocateWrapper(WKWebProcessPlugInNodeHandle.class);
        break;

    case Type::BundleRangeHandle:
        wrapper = allocateWrapper(WKWebProcessPlugInRangeHandle.class);
        break;

    case Type::BundleScriptWorld:
        wrapper = allocateWrapper(WKWebProcessPlugInScriptWorld.class);
        break;

    case Type::ScriptMessage:
        wrapper = allocateWrapper(WKScriptMessage.class);
        break;

    case Type::SerializedNode:
        wrapper = allocateWrapper(WKDOMNodeSnapshot.class);
        break;

    case Type::JSHandle:
        wrapper = allocateWrapper(_WKJSHandle.class);
        break;

    case Type::FormInfo:
        wrapper = allocateWrapper(WKFormInfo.class);
        break;

    default:
        wrapper = allocateWKObject([WKObject class], size);
        break;
    }

    SUPPRESS_UNRETAINED_LOCAL __unsafe_unretained id<WKObject> object = (__bridge id<WKObject>)wrapper;
    apiObjectsUnderConstruction().add(&object._apiObject, wrapper);
    return &object._apiObject;
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

RefPtr<API::Object> Object::fromNSObject(id object)
{
    if (auto *str = dynamic_objc_cast<NSString>(object))
        return API::String::create(str);
    if (auto *data = dynamic_objc_cast<NSData>(object))
        return API::Data::createWithoutCopying(data);
    if (auto *number = dynamic_objc_cast<NSNumber>(object))
        return API::Double::create([number doubleValue]);
    if (auto *array = dynamic_objc_cast<NSArray>(object)) {
        Vector<RefPtr<API::Object>> result(ReserveCapacityTag { }, array.count);
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
