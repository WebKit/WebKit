/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ShouldRelaxThirdPartyCookieBlocking.h"
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifest.h"
#endif

namespace WebCore {

class AlternativeTextClient;
class ApplicationCacheStorage;
class AuthenticatorCoordinatorClient;
class BackForwardClient;
class CacheStorageProvider;
class CookieJar;
class ChromeClient;
class ContextMenuClient;
class DatabaseProvider;
class DiagnosticLoggingClient;
class DragClient;
class EditorClient;
class FrameLoaderClient;
class InspectorClient;
class LibWebRTCProvider;
class MediaRecorderProvider;
class PaymentCoordinatorClient;
class PerformanceLoggingClient;
class PluginInfoProvider;
class ProgressTrackerClient;
class SocketProvider;
class SpeechRecognitionProvider;
class StorageNamespaceProvider;
class UserContentProvider;
class UserContentURLPattern;
class ValidationMessageClient;
class VisitedLinkStore;
class WebGLStateTracker;
class SpeechSynthesisClient;

class PageConfiguration {
    WTF_MAKE_NONCOPYABLE(PageConfiguration); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT PageConfiguration(PAL::SessionID, UniqueRef<EditorClient>&&, Ref<SocketProvider>&&, UniqueRef<LibWebRTCProvider>&&, Ref<CacheStorageProvider>&&, Ref<UserContentProvider>&&, Ref<BackForwardClient>&&, Ref<CookieJar>&&, UniqueRef<ProgressTrackerClient>&&, UniqueRef<FrameLoaderClient>&&, UniqueRef<SpeechRecognitionProvider>&&, UniqueRef<MediaRecorderProvider>&&);
    WEBCORE_EXPORT ~PageConfiguration();
    PageConfiguration(PageConfiguration&&);

    PAL::SessionID sessionID;
    std::unique_ptr<AlternativeTextClient> alternativeTextClient;
    ChromeClient* chromeClient { nullptr };
#if ENABLE(CONTEXT_MENUS)
    ContextMenuClient* contextMenuClient { nullptr };
#endif
    UniqueRef<EditorClient> editorClient;
    Ref<SocketProvider> socketProvider;
    std::unique_ptr<DragClient> dragClient;
    InspectorClient* inspectorClient { nullptr };
#if ENABLE(APPLE_PAY)
    PaymentCoordinatorClient* paymentCoordinatorClient { nullptr };
#endif

#if ENABLE(WEB_AUTHN)
    std::unique_ptr<AuthenticatorCoordinatorClient> authenticatorCoordinatorClient;
#endif

#if ENABLE(APPLICATION_MANIFEST)
    Optional<ApplicationManifest> applicationManifest;
#endif

    UniqueRef<LibWebRTCProvider> libWebRTCProvider;

    UniqueRef<ProgressTrackerClient> progressTrackerClient;
    Ref<BackForwardClient> backForwardClient;
    Ref<CookieJar> cookieJar;
    std::unique_ptr<ValidationMessageClient> validationMessageClient;
    UniqueRef<FrameLoaderClient> loaderClientForMainFrame;
    std::unique_ptr<DiagnosticLoggingClient> diagnosticLoggingClient;
    std::unique_ptr<PerformanceLoggingClient> performanceLoggingClient;
#if ENABLE(WEBGL)
    std::unique_ptr<WebGLStateTracker> webGLStateTracker;
#endif
#if ENABLE(SPEECH_SYNTHESIS)
    std::unique_ptr<SpeechSynthesisClient> speechSynthesisClient;
#endif

    RefPtr<ApplicationCacheStorage> applicationCacheStorage;
    RefPtr<DatabaseProvider> databaseProvider;
    Ref<CacheStorageProvider> cacheStorageProvider;
    RefPtr<PluginInfoProvider> pluginInfoProvider;
    RefPtr<StorageNamespaceProvider> storageNamespaceProvider;
    Ref<UserContentProvider> userContentProvider;
    RefPtr<VisitedLinkStore> visitedLinkStore;
    
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    RefPtr<DeviceOrientationUpdateProvider> deviceOrientationUpdateProvider;
#endif
    Vector<UserContentURLPattern> corsDisablingPatterns;
    UniqueRef<SpeechRecognitionProvider> speechRecognitionProvider;
    UniqueRef<MediaRecorderProvider> mediaRecorderProvider;
    bool loadsSubresources { true };
    bool loadsFromNetwork { true };
    bool userScriptsShouldWaitUntilNotification { true };
    ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking { ShouldRelaxThirdPartyCookieBlocking::No };
    bool textInteractionEnabled { true };
    bool httpsUpgradeEnabled { true };
};

}
