/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef APIObject_h
#define APIObject_h

#include <functional>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(COCOA)
#include "WKFoundation.h"
#ifdef __OBJC__
#include "WKObject.h"
#endif
#endif

#define DELEGATE_REF_COUNTING_TO_COCOA (PLATFORM(COCOA) && WK_API_ENABLED)

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
        ProtectionSpace,
        RenderLayer,
        RenderObject,
        SecurityOrigin,
        SessionState,
        SerializedScriptValue,
        String,
        URL,
        URLRequest,
        URLResponse,
        UserContentURLPattern,
        WebArchive,
        WebArchiveResource,

        // Base numeric types
        Boolean,
        Double,
        UInt64,
        
        // Geometry types
        Point,
        Size,
        Rect,
        
        // UIProcess types
        ApplicationCacheManager,
        BackForwardList,
        BackForwardListItem,
        BatteryManager,
        BatteryStatus,
        CacheManager,
        ColorPickerResultListener,
        Context,
        ContextConfiguration,
        CookieManager,
        DatabaseManager,
        Download,
        FormSubmissionListener,
        Frame,
        FramePolicyListener,
        FullScreenManager,
        GeolocationManager,
        GeolocationPermissionRequest,
        HitTestResult,
        GeolocationPosition,
        GrammarDetail,
        IconDatabase,
        Inspector,
        KeyValueStorageManager,
        MediaCacheManager,
        NavigationData,
        Notification,
        NotificationManager,
        NotificationPermissionRequest,
        OpenPanelParameters,
        OpenPanelResultListener,
        OriginDataManager,
        Page,
        PageGroup,
        PluginSiteDataManager,
        Preferences,
        Session,
        TextChecker,
        Vibration,
        ViewportAttributes,

        // Bundle types
        Bundle,
        BundleBackForwardList,
        BundleBackForwardListItem,
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
#if PLATFORM(EFL)
        PopupMenuItem,
#if ENABLE(TOUCH_EVENTS)
        TouchPoint,
        TouchEvent,
#endif
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

    NSObject *wrapper() { return m_wrapper; }

    void ref();
    void deref();
#endif // DELEGATE_REF_COUNTING_TO_COCOA

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

    virtual Type type() const override { return APIType; }

#if DELEGATE_REF_COUNTING_TO_COCOA
    void* operator new(size_t size) { return newObject(size, APIType); }
    void* operator new(size_t size, void* value) { return value; }
#endif
};

} // namespace Object

#undef DELEGATE_REF_COUNTING_TO_COCOA

#endif // APIObject_h
