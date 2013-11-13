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

#include <wtf/RefCounted.h>

#if PLATFORM(MAC)
#include "WKFoundation.h"
#endif

#define DELEGATE_REF_COUNTING_TO_COCOA (PLATFORM(MAC) && WK_API_ENABLED)

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
    enum Type {
        // Base types
        TypeNull = 0,
        TypeArray,
        TypeAuthenticationChallenge,
        TypeAuthenticationDecisionListener,
        TypeCertificateInfo,
        TypeConnection,
        TypeContextMenuItem,
        TypeCredential,
        TypeData,
        TypeDictionary,
        TypeError,
        TypeGraphicsContext,
        TypeImage,
        TypeProtectionSpace,
        TypeRenderLayer,
        TypeRenderObject,
        TypeSecurityOrigin,
        TypeSerializedScriptValue,
        TypeString,
        TypeURL,
        TypeURLRequest,
        TypeURLResponse,
        TypeUserContentURLPattern,
        TypeWebArchive,
        TypeWebArchiveResource,

        // Base numeric types
        TypeBoolean,
        TypeDouble,
        TypeUInt64,
        
        // Geometry types
        TypePoint,
        TypeSize,
        TypeRect,
        
        // UIProcess types
        TypeApplicationCacheManager,
        TypeBackForwardList,
        TypeBackForwardListItem,
        TypeBatteryManager,
        TypeBatteryStatus,
        TypeCacheManager,
        TypeColorPickerResultListener,
        TypeContext,
        TypeCookieManager,
        TypeDatabaseManager,
        TypeDownload,
        TypeFormSubmissionListener,
        TypeFrame,
        TypeFramePolicyListener,
        TypeFullScreenManager,
        TypeGeolocationManager,
        TypeGeolocationPermissionRequest,
        TypeHitTestResult,
        TypeGeolocationPosition,
        TypeGrammarDetail,
        TypeIconDatabase,
        TypeInspector,
        TypeKeyValueStorageManager,
        TypeMediaCacheManager,
        TypeNavigationData,
        TypeNetworkInfo,
        TypeNetworkInfoManager,
        TypeNotification,
        TypeNotificationManager,
        TypeNotificationPermissionRequest,
        TypeOpenPanelParameters,
        TypeOpenPanelResultListener,
        TypeOriginDataManager,
        TypePage,
        TypePageGroup,
        TypePluginSiteDataManager,
        TypePreferences,
        TypeTextChecker,
        TypeVibration,
        TypeViewportAttributes,

        // Bundle types
        TypeBundle,
        TypeBundleBackForwardList,
        TypeBundleBackForwardListItem,
        TypeBundleDOMWindowExtension,
        TypeBundleFrame,
        TypeBundleHitTestResult,
        TypeBundleInspector,
        TypeBundleNavigationAction,
        TypeBundleNodeHandle,
        TypeBundlePage,
        TypeBundlePageBanner,
        TypeBundlePageGroup,
        TypeBundlePageOverlay,
        TypeBundleRangeHandle,
        TypeBundleScriptWorld,

        // Platform specific
        TypeEditCommandProxy,
        TypeObjCObjectGraph,
        TypeView,
#if USE(SOUP)
        TypeSoupRequestManager,
#endif
#if PLATFORM(EFL)
        TypePopupMenuItem,
#if ENABLE(TOUCH_EVENTS)
        TypeTouchPoint,
        TypeTouchEvent,
#endif
#endif
    };

    virtual ~Object()
    {
    }

    virtual Type type() const = 0;

#if DELEGATE_REF_COUNTING_TO_COCOA
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
class TypedObject : public Object {
public:
    static const Type APIType = ArgumentType;

    virtual ~TypedObject()
    {
    }

protected:
    TypedObject()
    {
    }

    virtual Type type() const OVERRIDE { return APIType; }

#if DELEGATE_REF_COUNTING_TO_COCOA
    void* operator new(size_t size) { return newObject(size, APIType); }
#endif
};

} // namespace Object

#undef DELEGATE_REF_COUNTING_TO_COCOA

#endif // APIObject_h
