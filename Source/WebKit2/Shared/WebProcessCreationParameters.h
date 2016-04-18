/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#ifndef WebProcessCreationParameters_h
#define WebProcessCreationParameters_h

#include "CacheModel.h"
#include "SandboxExtension.h"
#include "TextCheckerState.h"
#include "UserData.h"
#include <WebCore/SessionID.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "MachPort.h"
#include <WebCore/MachSendRight.h>
#endif

#if USE(SOUP)
#include "HTTPCookieAcceptPolicy.h"
#endif

namespace API {
class Data;
}

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

struct WebProcessCreationParameters {
    WebProcessCreationParameters();
    ~WebProcessCreationParameters();

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebProcessCreationParameters&);

    String injectedBundlePath;
    SandboxExtension::Handle injectedBundlePathExtensionHandle;

    UserData initializationUserData;

    String applicationCacheDirectory;    
    SandboxExtension::Handle applicationCacheDirectoryExtensionHandle;
    String webSQLDatabaseDirectory;
    SandboxExtension::Handle webSQLDatabaseDirectoryExtensionHandle;
    String mediaCacheDirectory;
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
#if ENABLE(SECCOMP_FILTERS)
    String cookieStorageDirectory;
#endif
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    Vector<uint8_t> uiProcessCookieStorageIdentifier;
#endif
#if PLATFORM(IOS)
    SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle containerTemporaryDirectoryExtensionHandle;
#endif
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    String mediaKeyStorageDirectory;

    bool shouldUseTestingNetworkSession;

    Vector<String> urlSchemesRegisteredAsEmptyDocument;
    Vector<String> urlSchemesRegisteredAsSecure;
    Vector<String> urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    Vector<String> urlSchemesForWhichDomainRelaxationIsForbidden;
    Vector<String> urlSchemesRegisteredAsLocal;
    Vector<String> urlSchemesRegisteredAsNoAccess;
    Vector<String> urlSchemesRegisteredAsDisplayIsolated;
    Vector<String> urlSchemesRegisteredAsCORSEnabled;
    Vector<String> urlSchemesRegisteredAsAlwaysRevalidated;
#if ENABLE(CACHE_PARTITIONING)
    Vector<String> urlSchemesRegisteredAsCachePartitioned;
#endif

    CacheModel cacheModel;

    bool shouldAlwaysUseComplexTextCodePath;
    bool shouldEnableMemoryPressureReliefLogging;
    bool shouldSuppressMemoryPressureHandler { false };
    bool shouldUseFontSmoothing;
    bool resourceLoadStatisticsEnabled { false };

    Vector<String> fontWhitelist;

    bool iconDatabaseEnabled;

    double terminationTimeout;

    Vector<String> languages;

    TextCheckerState textCheckerState;

    bool fullKeyboardAccessEnabled;

    double defaultRequestTimeoutInterval;

#if PLATFORM(COCOA) || USE(CFNETWORK)
    String uiProcessBundleIdentifier;
#endif

#if PLATFORM(COCOA)
    pid_t presenterApplicationPid;

    bool accessibilityEnhancedUserInterfaceEnabled;

    WebCore::MachSendRight acceleratedCompositingPort;

    String uiProcessBundleResourcePath;
    SandboxExtension::Handle uiProcessBundleResourcePathExtensionHandle;

    bool shouldEnableJIT;
    bool shouldEnableFTLJIT;
    
    RefPtr<API::Data> bundleParameterData;

#endif // PLATFORM(COCOA)

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    HashMap<String, bool> notificationPermissions;
#endif

    HashMap<WebCore::SessionID, HashMap<unsigned, double>> plugInAutoStartOriginHashes;
    Vector<String> plugInAutoStartOrigins;

    bool memoryCacheDisabled;

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices;
    bool hasSelectionServices;
    bool hasRichContentServices;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, uint8_t>>> pluginLoadClientPolicies;
    HashMap<String, HashMap<String, HashMap<String, uint8_t>>> pluginLoadClientPoliciesForPrivateBrowsing;
#endif

#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    RetainPtr<CFDataRef> networkATSContext;
#endif
};

} // namespace WebKit

#endif // WebProcessCreationParameters_h
