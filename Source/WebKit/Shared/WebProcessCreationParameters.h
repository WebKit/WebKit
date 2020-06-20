/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "WebProcessDataStoreParameters.h"
#include <wtf/HashMap.h>
#include <wtf/ProcessID.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <WebCore/PlatformScreen.h>
#include <WebCore/ScreenProperties.h>
#include <wtf/MachSendRight.h>
#endif

#if USE(SOUP)
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/SoupNetworkProxySettings.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <WebCore/RenderThemeIOS.h>
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
#include <WebCore/PluginData.h>
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
    WebProcessCreationParameters(WebProcessCreationParameters&&);
    WebProcessCreationParameters& operator=(WebProcessCreationParameters&&);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebProcessCreationParameters&);

    String injectedBundlePath;
    SandboxExtension::Handle injectedBundlePathExtensionHandle;
    SandboxExtension::HandleArray additionalSandboxExtensionHandles;

    UserData initializationUserData;

#if PLATFORM(IOS_FAMILY)
    SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle containerTemporaryDirectoryExtensionHandle;
#endif
#if ENABLE(MEDIA_STREAM)
    SandboxExtension::Handle audioCaptureExtensionHandle;
#endif

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
    Vector<String> urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest;

    Vector<String> fontWhitelist;
    Vector<String> overrideLanguages;
#if USE(GSTREAMER)
    Vector<String> gstreamerOptions;
#endif

    CacheModel cacheModel;

    double defaultRequestTimeoutInterval { INT_MAX };
    unsigned backForwardCacheCapacity { 0 };

    bool shouldAlwaysUseComplexTextCodePath { false };
    bool shouldEnableMemoryPressureReliefLogging { false };
    bool shouldSuppressMemoryPressureHandler { false };
    bool shouldUseFontSmoothing { true };
    bool fullKeyboardAccessEnabled { false };
    bool memoryCacheDisabled { false };
    bool attrStyleEnabled { false };
    bool useGPUProcessForMedia { false };

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices { false };
    bool hasSelectionServices { false };
    bool hasRichContentServices { false };
#endif

    Seconds terminationTimeout;

    TextCheckerState textCheckerState;

#if PLATFORM(COCOA)
    String uiProcessBundleIdentifier;
    uint32_t uiProcessSDKVersion { 0 };
#endif

    ProcessID presentingApplicationPID { 0 };

#if PLATFORM(COCOA)
    WTF::MachSendRight acceleratedCompositingPort;

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

    Vector<String> plugInAutoStartOrigins;

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>> pluginLoadClientPolicies;
#endif

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> networkATSContext;
#endif

#if PLATFORM(WAYLAND)
    String waylandCompositorDisplayName;
#endif

#if USE(SOUP)
    WebCore::SoupNetworkProxySettings proxySettings;
#endif

#if PLATFORM(COCOA)
    Vector<String> mediaMIMETypes;
    WebCore::ScreenProperties screenProperties;
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    bool shouldLogUserInteraction { false };
#endif

#if PLATFORM(MAC)
    bool useOverlayScrollbars { true };
#endif

#if USE(WPE_RENDERER)
    bool isServiceWorkerProcess { false };
    IPC::Attachment hostClientFileDescriptor;
    CString implementationLibraryName;
#endif

    Optional<WebProcessDataStoreParameters> websiteDataStoreParameters;
    
#if PLATFORM(IOS)
    Optional<SandboxExtension::Handle> compilerServiceExtensionHandle;
#endif

#if PLATFORM(IOS_FAMILY)
    SandboxExtension::HandleArray diagnosticsExtensionHandles;
    SandboxExtension::HandleArray dynamicMachExtensionHandles;
    SandboxExtension::HandleArray dynamicIOKitExtensionHandles;
#endif

#if PLATFORM(COCOA)
    Optional<SandboxExtension::Handle> mapDBExtensionHandle;
    bool systemHasBattery { false };
#endif

#if PLATFORM(IOS_FAMILY)
    bool currentUserInterfaceIdiomIsPad { false };
    bool supportsPictureInPicture { false };
    WebCore::RenderThemeIOS::CSSValueToSystemColorMap cssValueToSystemColorMap;
    WebCore::Color focusRingColor;
    String localizedDeviceModel;
#endif

#if PLATFORM(COCOA)
    SandboxExtension::HandleArray mediaExtensionHandles; // FIXME(207716): Remove when GPU process is complete.
#if ENABLE(CFPREFS_DIRECT_MODE)
    Optional<SandboxExtension::HandleArray> preferencesExtensionHandles;
#endif
#endif

#if PLATFORM(GTK)
    bool useSystemAppearanceForScrollbars { false };
#endif
};

} // namespace WebKit
