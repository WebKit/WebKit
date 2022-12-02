/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(COCOA)
#include "WKFoundation.h"
#ifdef __OBJC__
#include "WKObject.h"
#endif
#endif

#define DELEGATE_REF_COUNTING_TO_COCOA PLATFORM(COCOA)

namespace API {

class Object
#if !DELEGATE_REF_COUNTING_TO_COCOA
    : public ThreadSafeRefCounted<Object>
#endif
{
public:
    enum class Type {
        // Base types
        Null = 0,
        Array,
        AuthenticationChallenge,
        AuthenticationDecisionListener,
        CertificateInfo,
        Connection,
        ContextMenuItem,
        Credential,
        Data,
        Dictionary,
        Error,
        FrameHandle,
        Image,
        PageHandle,
        ProtectionSpace,
        RenderLayer,
        RenderObject,
        ResourceLoadInfo,
        SecurityOrigin,
        SessionState,
        SerializedScriptValue,
        String,
        URL,
        URLRequest,
        URLResponse,
        UserContentURLPattern,
        UserScript,
        UserStyleSheet,
        WebArchive,
        WebArchiveResource,

        // Base numeric types
        Boolean,
        Double,
        UInt64,
        Int64,
        
        // Geometry types
        Point,
        Size,
        Rect,
        
        // UIProcess types
        ApplicationCacheManager,
#if ENABLE(APPLICATION_MANIFEST)
        ApplicationManifest,
#endif
        Attachment,
        AutomationSession,
        BackForwardList,
        BackForwardListItem,
        CacheManager,
        ColorPickerResultListener,
        ContentRuleList,
        ContentRuleListAction,
        ContentRuleListStore,
        ContentWorld,
#if PLATFORM(IOS_FAMILY)
        ContextMenuElementInfo,
#endif
        ContextMenuListener,
        CustomHeaderFields,
        InternalDebugFeature,
        DataTask,
        DebuggableInfo,
        Download,
        ExperimentalFeature,
        FormSubmissionListener,
        Frame,
        FrameInfo,
        FramePolicyListener,
        FrameTreeNode,
        FullScreenManager,
        GeolocationManager,
        GeolocationPermissionRequest,
        HTTPCookieStore,
        HitTestResult,
        GeolocationPosition,
        GrammarDetail,
        IconDatabase,
        Inspector,
        InspectorConfiguration,
#if ENABLE(INSPECTOR_EXTENSIONS)
        InspectorExtension,
#endif
        KeyValueStorageManager,
        MediaCacheManager,
        MessageListener,
        Navigation,
        NavigationAction,
        NavigationData,
        NavigationResponse,
        Notification,
        NotificationManager,
        NotificationPermissionRequest,
        OpenPanelParameters,
        OpenPanelResultListener,
        OriginDataManager,
        Page,
        PageConfiguration,
        PageGroup,
        ProcessPool,
        ProcessPoolConfiguration,
        PluginSiteDataManager,
        Preferences,
        RequestStorageAccessConfirmResultListener,
        ResourceLoadStatisticsStore,
        ResourceLoadStatisticsFirstParty,
        ResourceLoadStatisticsThirdParty,
        RunBeforeUnloadConfirmPanelResultListener,
        RunJavaScriptAlertResultListener,
        RunJavaScriptConfirmResultListener,
        RunJavaScriptPromptResultListener,
        SpeechRecognitionPermissionCallback,
        TextChecker,
        URLSchemeTask,
        UserContentController,
        UserInitiatedAction,
        UserMediaPermissionCheck,
        UserMediaPermissionRequest,
        ViewportAttributes,
        VisitedLinkStore,
#if ENABLE(WK_WEB_EXTENSIONS)
        WebExtension,
        WebExtensionContext,
        WebExtensionController,
        WebExtensionControllerConfiguration,
        WebExtensionMatchPattern,
#endif
        WebResourceLoadStatisticsManager,
        WebsiteDataRecord,
        WebsiteDataStore,
        WebsiteDataStoreConfiguration,
        WebsitePolicies,
        WindowFeatures,

#if ENABLE(WEB_AUTHN)
        WebAuthenticationAssertionResponse,
        WebAuthenticationPanel,
#endif

        MediaKeySystemPermissionCallback,
        QueryPermissionResultCallback,

        // Bundle types
        Bundle,
        BundleBackForwardList,
        BundleBackForwardListItem,
        BundleCSSStyleDeclarationHandle,
        BundleDOMWindowExtension,
        BundleFrame,
        BundleHitTestResult,
        BundleInspector,
        BundleNodeHandle,
        BundlePage,
        BundlePageBanner,
        BundlePageOverlay,
        BundleRangeHandle,
        BundleScriptWorld,

        // Platform specific
        EditCommandProxy,
        ObjCObjectGraph,
        View,
#if USE(SOUP)
        SoupRequestManager,
        SoupCustomProtocolRequestManager,
#endif
    };

    virtual ~Object()
    {
    }

    virtual Type type() const = 0;

#if DELEGATE_REF_COUNTING_TO_COCOA
#ifdef __OBJC__
    template<typename T, typename... Args>
    static void constructInWrapper(id <WKObject> wrapper, Args&&... args)
    {
        Object* object = new (&wrapper._apiObject) T(std::forward<Args>(args)...);
        object->m_wrapper = (__bridge CFTypeRef)wrapper;
    }

    id <WKObject> wrapper() const { return (__bridge id <WKObject>)m_wrapper; }
#endif

    void ref() const;
    void deref() const;
#endif // DELEGATE_REF_COUNTING_TO_COCOA

    static void* wrap(API::Object*);
    static API::Object* unwrap(void*);

#if PLATFORM(COCOA) && defined(__OBJC__)
    static API::Object& fromWKObjectExtraSpace(id <WKObject>);
#endif

protected:
    Object();

#if DELEGATE_REF_COUNTING_TO_COCOA
    static void* newObject(size_t, Type);

private:
    // Derived classes must override operator new and call newObject().
    void* operator new(size_t) = delete;

    CFTypeRef m_wrapper;
#endif // DELEGATE_REF_COUNTING_TO_COCOA
};

template <Object::Type ArgumentType>
class ObjectImpl : public Object {
public:
    static const Type APIType = ArgumentType;

    virtual ~ObjectImpl()
    {
    }

protected:
    friend class Object;

    ObjectImpl()
    {
    }

    Type type() const override { return APIType; }

#if DELEGATE_REF_COUNTING_TO_COCOA
    void* operator new(size_t size) { return newObject(size, APIType); }
    void* operator new(size_t, void* value) { return value; }
#endif
};

#if !DELEGATE_REF_COUNTING_TO_COCOA
inline void* Object::wrap(API::Object* object)
{
    return static_cast<void*>(object);
}

inline API::Object* Object::unwrap(void* object)
{
    return static_cast<API::Object*>(object);
}
#endif

} // namespace Object

namespace WTF {

template<> struct EnumTraits<API::Object::Type> {
    using values = EnumValues<
        API::Object::Type,
        // Base types
        API::Object::Type::Null,
        API::Object::Type::Array,
        API::Object::Type::AuthenticationChallenge,
        API::Object::Type::AuthenticationDecisionListener,
        API::Object::Type::CertificateInfo,
        API::Object::Type::Connection,
        API::Object::Type::ContextMenuItem,
        API::Object::Type::Credential,
        API::Object::Type::Data,
        API::Object::Type::Dictionary,
        API::Object::Type::Error,
        API::Object::Type::FrameHandle,
        API::Object::Type::Image,
        API::Object::Type::PageHandle,
        API::Object::Type::ProtectionSpace,
        API::Object::Type::ResourceLoadInfo,
        API::Object::Type::SecurityOrigin,
        API::Object::Type::SessionState,
        API::Object::Type::SerializedScriptValue,
        API::Object::Type::String,
        API::Object::Type::URL,
        API::Object::Type::URLRequest,
        API::Object::Type::URLResponse,
        API::Object::Type::UserContentURLPattern,
        API::Object::Type::UserScript,
        API::Object::Type::UserStyleSheet,
        API::Object::Type::WebArchive,
        API::Object::Type::WebArchiveResource,

        // Base numeric types
        API::Object::Type::Boolean,
        API::Object::Type::Double,
        API::Object::Type::UInt64,
        API::Object::Type::Int64,

        // Geometry types
        API::Object::Type::Point,
        API::Object::Type::Size,
        API::Object::Type::Rect,

        // UIProcess types
        API::Object::Type::ApplicationCacheManager,
#if ENABLE(APPLICATION_MANIFEST)
        API::Object::Type::ApplicationManifest,
#endif
        API::Object::Type::Attachment,
        API::Object::Type::AutomationSession,
        API::Object::Type::BackForwardList,
        API::Object::Type::BackForwardListItem,
        API::Object::Type::CacheManager,
        API::Object::Type::ColorPickerResultListener,
        API::Object::Type::ContentRuleList,
        API::Object::Type::ContentRuleListAction,
        API::Object::Type::ContentRuleListStore,
        API::Object::Type::ContentWorld,
#if PLATFORM(IOS_FAMILY)
        API::Object::Type::ContextMenuElementInfo,
#endif
        API::Object::Type::ContextMenuListener,
        API::Object::Type::CustomHeaderFields,
        API::Object::Type::InternalDebugFeature,
        API::Object::Type::DataTask,
        API::Object::Type::DebuggableInfo,
        API::Object::Type::Download,
        API::Object::Type::ExperimentalFeature,
        API::Object::Type::FormSubmissionListener,
        API::Object::Type::Frame,
        API::Object::Type::FrameInfo,
        API::Object::Type::FramePolicyListener,
        API::Object::Type::FrameTreeNode,
        API::Object::Type::FullScreenManager,
        API::Object::Type::GeolocationManager,
        API::Object::Type::GeolocationPermissionRequest,
        API::Object::Type::HTTPCookieStore,
        API::Object::Type::HitTestResult,
        API::Object::Type::GeolocationPosition,
        API::Object::Type::GrammarDetail,
        API::Object::Type::IconDatabase,
        API::Object::Type::Inspector,
        API::Object::Type::InspectorConfiguration,
#if ENABLE(INSPECTOR_EXTENSIONS)
        API::Object::Type::InspectorExtension,
#endif
        API::Object::Type::KeyValueStorageManager,
        API::Object::Type::MediaCacheManager,
        API::Object::Type::MessageListener,
        API::Object::Type::Navigation,
        API::Object::Type::NavigationAction,
        API::Object::Type::NavigationData,
        API::Object::Type::NavigationResponse,
        API::Object::Type::Notification,
        API::Object::Type::NotificationManager,
        API::Object::Type::NotificationPermissionRequest,
        API::Object::Type::OpenPanelParameters,
        API::Object::Type::OpenPanelResultListener,
        API::Object::Type::OriginDataManager,
        API::Object::Type::Page,
        API::Object::Type::PageConfiguration,
        API::Object::Type::PageGroup,
        API::Object::Type::ProcessPool,
        API::Object::Type::ProcessPoolConfiguration,
        API::Object::Type::PluginSiteDataManager,
        API::Object::Type::Preferences,
        API::Object::Type::RequestStorageAccessConfirmResultListener,
        API::Object::Type::ResourceLoadStatisticsStore,
        API::Object::Type::ResourceLoadStatisticsFirstParty,
        API::Object::Type::ResourceLoadStatisticsThirdParty,
        API::Object::Type::RunBeforeUnloadConfirmPanelResultListener,
        API::Object::Type::RunJavaScriptAlertResultListener,
        API::Object::Type::RunJavaScriptConfirmResultListener,
        API::Object::Type::RunJavaScriptPromptResultListener,
        API::Object::Type::SpeechRecognitionPermissionCallback,
        API::Object::Type::TextChecker,
        API::Object::Type::URLSchemeTask,
        API::Object::Type::UserContentController,
        API::Object::Type::UserInitiatedAction,
        API::Object::Type::UserMediaPermissionCheck,
        API::Object::Type::UserMediaPermissionRequest,
        API::Object::Type::ViewportAttributes,
        API::Object::Type::VisitedLinkStore,
#if ENABLE(WK_WEB_EXTENSIONS)
        API::Object::Type::WebExtension,
        API::Object::Type::WebExtensionContext,
        API::Object::Type::WebExtensionController,
        API::Object::Type::WebExtensionControllerConfiguration,
        API::Object::Type::WebExtensionMatchPattern,
#endif
        API::Object::Type::WebResourceLoadStatisticsManager,
        API::Object::Type::WebsiteDataRecord,
        API::Object::Type::WebsiteDataStore,
        API::Object::Type::WebsiteDataStoreConfiguration,
        API::Object::Type::WebsitePolicies,
        API::Object::Type::WindowFeatures,

#if ENABLE(WEB_AUTHN)
        API::Object::Type::WebAuthenticationAssertionResponse,
        API::Object::Type::WebAuthenticationPanel,
#endif

        API::Object::Type::MediaKeySystemPermissionCallback,

        // Bundle types
        API::Object::Type::Bundle,
        API::Object::Type::BundleBackForwardList,
        API::Object::Type::BundleBackForwardListItem,
        API::Object::Type::BundleCSSStyleDeclarationHandle,
        API::Object::Type::BundleDOMWindowExtension,
        API::Object::Type::BundleFrame,
        API::Object::Type::BundleHitTestResult,
        API::Object::Type::BundleInspector,
        API::Object::Type::BundleNodeHandle,
        API::Object::Type::BundlePage,
        API::Object::Type::BundlePageBanner,
        API::Object::Type::BundlePageOverlay,
        API::Object::Type::BundleRangeHandle,
        API::Object::Type::BundleScriptWorld,

        // Platform specific
        API::Object::Type::EditCommandProxy,
        API::Object::Type::ObjCObjectGraph,
        API::Object::Type::View
#if USE(SOUP)
        , API::Object::Type::SoupRequestManager
        , API::Object::Type::SoupCustomProtocolRequestManager
#endif
    >;
};

} // namespace WTF

#undef DELEGATE_REF_COUNTING_TO_COCOA
