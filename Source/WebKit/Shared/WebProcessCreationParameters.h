/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include "CacheModel.h"
#include "SandboxExtension.h"
#include "TextCheckerState.h"
#include "UserData.h"
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/ProcessID.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <WebCore/MachSendRight.h>
#endif

#if USE(SOUP)
#include "HTTPCookieAcceptPolicy.h"
#include <WebCore/SoupNetworkProxySettings.h>
#endif

namespace API {
class Data;
}

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct WebProcessCreationParameters {
    WebProcessCreationParameters();
    ~WebProcessCreationParameters();

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, WebProcessCreationParameters&);

    String injectedBundlePath;
    SandboxExtension::Handle injectedBundlePathExtensionHandle;
    SandboxExtension::HandleArray additionalSandboxExtensionHandles;

    UserData initializationUserData;

    String applicationCacheDirectory;
    String applicationCacheFlatFileSubdirectoryName;
    SandboxExtension::Handle applicationCacheDirectoryExtensionHandle;
    String webSQLDatabaseDirectory;
    SandboxExtension::Handle webSQLDatabaseDirectoryExtensionHandle;
    String mediaCacheDirectory;
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    String javaScriptConfigurationDirectory;
    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
#if PLATFORM(MAC)
    Vector<uint8_t> uiProcessCookieStorageIdentifier;
#endif
#if PLATFORM(IOS)
    SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle containerTemporaryDirectoryExtensionHandle;
#endif
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
#if ENABLE(MEDIA_STREAM)
    SandboxExtension::Handle audioCaptureExtensionHandle;
    bool shouldCaptureAudioInUIProcess { false };
#endif
    String mediaKeyStorageDirectory;

    String webCoreLoggingChannels;
    String webKitLoggingChannels;

    Vector<String> urlSchemesRegisteredAsEmptyDocument;
    Vector<String> urlSchemesRegisteredAsSecure;
    Vector<String> urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    Vector<String> urlSchemesForWhichDomainRelaxationIsForbidden;
    Vector<String> urlSchemesRegisteredAsLocal;
    Vector<String> urlSchemesRegisteredAsNoAccess;
    Vector<String> urlSchemesRegisteredAsDisplayIsolated;
    Vector<String> urlSchemesRegisteredAsCORSEnabled;
    Vector<String> urlSchemesRegisteredAsAlwaysRevalidated;
    Vector<String> urlSchemesRegisteredAsCachePartitioned;
    Vector<String> urlSchemesServiceWorkersCanHandle;

    Vector<String> fontWhitelist;
    Vector<String> languages;

    CacheModel cacheModel;

    double defaultRequestTimeoutInterval { INT_MAX };

    bool shouldUseTestingNetworkSession { false };
    bool shouldAlwaysUseComplexTextCodePath { false };
    bool shouldEnableMemoryPressureReliefLogging { false };
    bool shouldSuppressMemoryPressureHandler { false };
    bool shouldUseFontSmoothing { true };
    bool resourceLoadStatisticsEnabled { false };
    bool fullKeyboardAccessEnabled { false };
    bool memoryCacheDisabled { false };

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices { false };
    bool hasSelectionServices { false };
    bool hasRichContentServices { false };
#endif

#if ENABLE(SERVICE_WORKER)
    bool hasRegisteredServiceWorkers { true };
#endif

    Seconds terminationTimeout;

    TextCheckerState textCheckerState;

#if PLATFORM(COCOA)
    String uiProcessBundleIdentifier;
#endif

    ProcessID presentingApplicationPID { 0 };

#if PLATFORM(COCOA)
    WebCore::MachSendRight acceleratedCompositingPort;

    String uiProcessBundleResourcePath;
    SandboxExtension::Handle uiProcessBundleResourcePathExtensionHandle;

    bool shouldEnableJIT { false };
    bool shouldEnableFTLJIT { false };
    bool accessibilityEnhancedUserInterfaceEnabled { false };
    
    RefPtr<API::Data> bundleParameterData;
#endif // PLATFORM(COCOA)

#if ENABLE(NOTIFICATIONS)
    HashMap<String, bool> notificationPermissions;
#endif

    HashMap<PAL::SessionID, HashMap<unsigned, WallTime>> plugInAutoStartOriginHashes;
    Vector<String> plugInAutoStartOrigins;

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, uint8_t>>> pluginLoadClientPolicies;
#endif

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> networkATSContext;
#endif

#if OS(LINUX)
    IPC::Attachment memoryPressureMonitorHandle;
#endif

#if PLATFORM(WAYLAND)
    String waylandCompositorDisplayName;
#endif

#if USE(SOUP)
    WebCore::SoupNetworkProxySettings proxySettings;
#endif

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    bool shouldLogUserInteraction { false };
#endif
};

} // namespace WebKit
