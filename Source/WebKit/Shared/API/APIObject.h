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

#if DELEGATE_REF_COUNTING_TO_COCOA
OBJC_CLASS NSObject;
#endif

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
        PageGroupData,
        PageHandle,
        PageGroupHandle,
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
        CookieManager,
        CustomHeaderFields,
        InternalDebugFeature,
        DebuggableInfo,
        Download,
        ExperimentalFeature,
        FormSubmissionListener,
        Frame,
        FrameInfo,
        FramePolicyListener,
        FullScreenManager,
        GeolocationManager,
        GeolocationPermissionRequest,
        HTTPCookieStore,
        HitTestResult,
        GeolocationPosition,
        GrammarDetail,
        IconDatabase,
        Inspector,
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
        TextChecker,
        URLSchemeTask,
        UserContentController,
        UserContentWorld,
        UserInitiatedAction,
        UserMediaPermissionCheck,
        UserMediaPermissionRequest,
        ViewportAttributes,
        VisitedLinkStore,
        WebResourceLoadStatisticsManager,
        WebsiteDataRecord,
        WebsiteDataStore,
        WebsiteDataStoreConfiguration,
        WebsitePolicies,
        WindowFeatures,

#if ENABLE(MEDIA_SESSION)
        MediaSessionFocusManager,
        MediaSessionMetadata,
#endif

#if ENABLE(WEB_AUTHN)
        WebAuthenticationAssertionResponse,
        WebAuthenticationPanel,
#endif

        // Bundle types
        Bundle,
        BundleBackForwardList,
        BundleBackForwardListItem,
        BundleCSSStyleDeclarationHandle,
        BundleDOMWindowExtension,
        BundleFrame,
        BundleHitTestResult,
        BundleInspector,
        BundleNavigationAction,
        BundleNodeHandle,
        BundlePage,
        BundlePageBanner,
        BundlePageGroup,
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
    static void constructInWrapper(NSObject <WKObject> *wrapper, Args&&... args)
    {
        Object* object = new (&wrapper._apiObject) T(std::forward<Args>(args)...);
        object->m_wrapper = wrapper;
    }
#endif

    NSObject *wrapper() const { return m_wrapper; }

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

    NSObject *m_wrapper;
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

#undef DELEGATE_REF_COUNTING_TO_COCOA
