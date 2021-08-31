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

#include "AccessibilityPreferences.h"
#include "CacheModel.h"
#include "SandboxExtension.h"
#include "TextCheckerState.h"
#include "UserData.h"
#include "WebProcessDataStoreParameters.h"
#include <WebCore/CrossOriginMode.h>
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

#if PLATFORM(IOS_FAMILY)
#include <WebCore/RenderThemeIOS.h>
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
#include <WebCore/PluginData.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include <wtf/MemoryPressureHandler.h>
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
    Vector<SandboxExtension::Handle> additionalSandboxExtensionHandles;

    UserData initializationUserData;

#if PLATFORM(IOS_FAMILY)
    SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle containerTemporaryDirectoryExtensionHandle;
#endif
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    SandboxExtension::Handle enableRemoteWebInspectorExtensionHandle;
#endif
#if ENABLE(MEDIA_STREAM)
    SandboxExtension::Handle audioCaptureExtensionHandle;
#endif

    String wtfLoggingChannels;
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

    Vector<String> fontAllowList;
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
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    bool hasMouseDevice { false };
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    bool hasStylusDevice { false };
#endif
    bool memoryCacheDisabled { false };
    bool attrStyleEnabled { false };
    bool shouldThrowExceptionForGlobalConstantRedeclaration { true };
    WebCore::CrossOriginMode crossOriginMode { WebCore::CrossOriginMode::Shared }; // Cross-origin isolation via COOP+COEP headers.

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices { false };
    bool hasSelectionServices { false };
    bool hasRichContentServices { false };
#endif

    Seconds terminationTimeout;

    TextCheckerState textCheckerState;

#if PLATFORM(COCOA)
    String uiProcessBundleIdentifier;
    int latencyQOS { 0 };
    int throughputQOS { 0 };
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

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, WebCore::PluginLoadClientPolicy>>> pluginLoadClientPolicies;
#endif

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> networkATSContext;
#endif

#if PLATFORM(WAYLAND)
    String waylandCompositorDisplayName;
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

    std::optional<WebProcessDataStoreParameters> websiteDataStoreParameters;
    
#if PLATFORM(IOS)
    Vector<SandboxExtension::Handle> compilerServiceExtensionHandles;
#endif

    std::optional<SandboxExtension::Handle> containerManagerExtensionHandle;
    std::optional<SandboxExtension::Handle> mobileGestaltExtensionHandle;
    std::optional<SandboxExtension::Handle> launchServicesExtensionHandle;
#if HAVE(VIDEO_RESTRICTED_DECODING)
    Vector<SandboxExtension::Handle> videoDecoderExtensionHandles;
#endif

    Vector<SandboxExtension::Handle> diagnosticsExtensionHandles;
#if PLATFORM(IOS_FAMILY)
    Vector<SandboxExtension::Handle> dynamicMachExtensionHandles;
    Vector<SandboxExtension::Handle> dynamicIOKitExtensionHandles;
#endif

#if PLATFORM(COCOA)
    bool systemHasBattery { false };
    bool systemHasAC { false };
#endif

#if PLATFORM(IOS_FAMILY)
    bool currentUserInterfaceIdiomIsPhoneOrWatch { false };
    bool supportsPictureInPicture { false };
    WebCore::RenderThemeIOS::CSSValueToSystemColorMap cssValueToSystemColorMap;
    WebCore::Color focusRingColor;
    String localizedDeviceModel;
    String contentSizeCategory;
#endif

#if PLATFORM(COCOA)
#if ENABLE(CFPREFS_DIRECT_MODE)
    std::optional<Vector<SandboxExtension::Handle>> preferencesExtensionHandles;
#endif
#endif

#if PLATFORM(GTK)
    bool useSystemAppearanceForScrollbars { false };
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    std::pair<int64_t, double> overrideUserInterfaceIdiomAndScale;
#endif

#if HAVE(IOSURFACE)
    WebCore::IntSize maximumIOSurfaceSize;
#endif
    
    AccessibilityPreferences accessibilityPreferences;

#if PLATFORM(GTK) || PLATFORM(WPE)
    std::optional<MemoryPressureHandler::Configuration> memoryPressureHandlerConfiguration;
#endif
};

} // namespace WebKit
