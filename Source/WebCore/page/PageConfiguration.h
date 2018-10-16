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

#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifest.h"
#endif

namespace WebCore {

class AlternativeTextClient;
class ApplicationCacheStorage;
class AuthenticatorCoordinatorClient;
class BackForwardClient;
class CacheStorageProvider;
class ChromeClient;
class ContextMenuClient;
class DatabaseProvider;
class DiagnosticLoggingClient;
class DragClient;
class EditorClient;
class FrameLoaderClient;
class InspectorClient;
class LibWebRTCProvider;
class PaymentCoordinatorClient;
class PerformanceLoggingClient;
class PlugInClient;
class PluginInfoProvider;
class ProgressTrackerClient;
class SocketProvider;
class StorageNamespaceProvider;
class UserContentProvider;
class ValidationMessageClient;
class VisitedLinkStore;
class WebGLStateTracker;

class PageConfiguration {
    WTF_MAKE_NONCOPYABLE(PageConfiguration); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT PageConfiguration(UniqueRef<EditorClient>&&, Ref<SocketProvider>&&, UniqueRef<LibWebRTCProvider>&&, Ref<CacheStorageProvider>&&, Ref<BackForwardClient>&&);
    WEBCORE_EXPORT ~PageConfiguration();
    PageConfiguration(PageConfiguration&&);

    AlternativeTextClient* alternativeTextClient { nullptr };
    ChromeClient* chromeClient { nullptr };
#if ENABLE(CONTEXT_MENUS)
    ContextMenuClient* contextMenuClient { nullptr };
#endif
    UniqueRef<EditorClient> editorClient;
    Ref<SocketProvider> socketProvider;
    DragClient* dragClient { nullptr };
    InspectorClient* inspectorClient { nullptr };
#if ENABLE(APPLE_PAY)
    PaymentCoordinatorClient* paymentCoordinatorClient { nullptr };
#endif

#if ENABLE(WEB_AUTHN)
    std::unique_ptr<AuthenticatorCoordinatorClient> authenticatorCoordinatorClient;
#endif

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<ApplicationManifest> applicationManifest;
#endif

    UniqueRef<LibWebRTCProvider> libWebRTCProvider;

    PlugInClient* plugInClient { nullptr };
    ProgressTrackerClient* progressTrackerClient { nullptr };
    Ref<BackForwardClient> backForwardClient;
    std::unique_ptr<ValidationMessageClient> validationMessageClient;
    FrameLoaderClient* loaderClientForMainFrame { nullptr };
    std::unique_ptr<DiagnosticLoggingClient> diagnosticLoggingClient;
    std::unique_ptr<PerformanceLoggingClient> performanceLoggingClient;
    std::unique_ptr<WebGLStateTracker> webGLStateTracker;

    RefPtr<ApplicationCacheStorage> applicationCacheStorage;
    RefPtr<DatabaseProvider> databaseProvider;
    Ref<CacheStorageProvider> cacheStorageProvider;
    RefPtr<PluginInfoProvider> pluginInfoProvider;
    RefPtr<StorageNamespaceProvider> storageNamespaceProvider;
    RefPtr<UserContentProvider> userContentProvider;
    RefPtr<VisitedLinkStore> visitedLinkStore;
};

}
