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

#include "config.h"
#include "WebProcessCreationParameters.h"

#include "APIData.h"
#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif
#include "WebCoreArgumentCoders.h"

namespace WebKit {

WebProcessCreationParameters::WebProcessCreationParameters(WebProcessCreationParameters&&) = default;
WebProcessCreationParameters& WebProcessCreationParameters::operator=(WebProcessCreationParameters&&) = default;

WebProcessCreationParameters::WebProcessCreationParameters()
{
}

WebProcessCreationParameters::~WebProcessCreationParameters()
{
}

void WebProcessCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << injectedBundlePath;
    encoder << injectedBundlePathExtensionHandle;
    encoder << additionalSandboxExtensionHandles;
    encoder << initializationUserData;
#if PLATFORM(IOS_FAMILY)
    encoder << cookieStorageDirectoryExtensionHandle;
    encoder << containerCachesDirectoryExtensionHandle;
    encoder << containerTemporaryDirectoryExtensionHandle;
#endif
    encoder << webCoreLoggingChannels;
    encoder << webKitLoggingChannels;
#if ENABLE(MEDIA_STREAM)
    encoder << audioCaptureExtensionHandle;
#endif
    encoder << urlSchemesRegisteredAsEmptyDocument;
    encoder << urlSchemesRegisteredAsSecure;
    encoder << urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    encoder << urlSchemesForWhichDomainRelaxationIsForbidden;
    encoder << urlSchemesRegisteredAsLocal;
    encoder << urlSchemesRegisteredAsNoAccess;
    encoder << urlSchemesRegisteredAsDisplayIsolated;
    encoder << urlSchemesRegisteredAsCORSEnabled;
    encoder << urlSchemesRegisteredAsAlwaysRevalidated;
    encoder << urlSchemesRegisteredAsCachePartitioned;
    encoder << urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest;
    encoder << cacheModel;
    encoder << shouldAlwaysUseComplexTextCodePath;
    encoder << shouldEnableMemoryPressureReliefLogging;
    encoder << shouldSuppressMemoryPressureHandler;
    encoder << shouldUseFontSmoothing;
    encoder << fontAllowList;
    encoder << terminationTimeout;
    encoder << overrideLanguages;
#if USE(GSTREAMER)
    encoder << gstreamerOptions;
#endif
    encoder << textCheckerState;
    encoder << fullKeyboardAccessEnabled;
    encoder << defaultRequestTimeoutInterval;
    encoder << backForwardCacheCapacity;
#if PLATFORM(COCOA)
    encoder << uiProcessBundleIdentifier;
    encoder << uiProcessSDKVersion;
#endif
    encoder << presentingApplicationPID;
#if PLATFORM(COCOA)
    encoder << accessibilityEnhancedUserInterfaceEnabled;
    encoder << acceleratedCompositingPort;
    encoder << uiProcessBundleResourcePath;
    encoder << uiProcessBundleResourcePathExtensionHandle;
    encoder << shouldEnableJIT;
    encoder << shouldEnableFTLJIT;
    encoder << !!bundleParameterData;
    if (bundleParameterData)
        encoder << bundleParameterData->dataReference();
#endif

#if ENABLE(NOTIFICATIONS)
    encoder << notificationPermissions;
#endif

    encoder << plugInAutoStartOrigins;
    encoder << memoryCacheDisabled;
    encoder << attrStyleEnabled;

#if ENABLE(SERVICE_CONTROLS)
    encoder << hasImageServices;
    encoder << hasSelectionServices;
    encoder << hasRichContentServices;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    encoder << pluginLoadClientPolicies;
#endif

#if PLATFORM(COCOA)
    IPC::encode(encoder, networkATSContext.get());
#endif

#if PLATFORM(WAYLAND)
    encoder << waylandCompositorDisplayName;
#endif

#if USE(SOUP)
    encoder << proxySettings;
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    encoder << shouldLogUserInteraction;
#endif

#if PLATFORM(COCOA)
    encoder << mediaMIMETypes;
    encoder << screenProperties;
#endif

#if PLATFORM(MAC)
    encoder << useOverlayScrollbars;
#endif

#if USE(WPE_RENDERER)
    encoder << isServiceWorkerProcess;
    encoder << hostClientFileDescriptor;
    encoder << implementationLibraryName;
#endif

    encoder << websiteDataStoreParameters;
    
#if PLATFORM(IOS)
    encoder << compilerServiceExtensionHandles;
#endif

    encoder << containerManagerExtensionHandle;
    encoder << mobileGestaltExtensionHandle;

#if PLATFORM(IOS_FAMILY)
    encoder << diagnosticsExtensionHandles;
    encoder << dynamicMachExtensionHandles;
    encoder << dynamicIOKitExtensionHandles;
#endif

#if PLATFORM(COCOA)
    encoder << systemHasBattery;
    encoder << systemHasAC;
#endif

#if PLATFORM(IOS_FAMILY)
    encoder << currentUserInterfaceIdiomIsPad;
    encoder << supportsPictureInPicture;
    encoder << cssValueToSystemColorMap;
    encoder << focusRingColor;
    encoder << localizedDeviceModel;
#endif

#if PLATFORM(COCOA)
    // FIXME(207716): The following should be removed when the GPU process is complete.
    encoder << mediaExtensionHandles;
#if ENABLE(CFPREFS_DIRECT_MODE)
    encoder << preferencesExtensionHandles;
#endif
#endif

#if PLATFORM(GTK)
    encoder << useSystemAppearanceForScrollbars;
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    encoder << overrideUserInterfaceIdiomAndScale;
#endif
}

bool WebProcessCreationParameters::decode(IPC::Decoder& decoder, WebProcessCreationParameters& parameters)
{
    if (!decoder.decode(parameters.injectedBundlePath))
        return false;
    
    Optional<SandboxExtension::Handle> injectedBundlePathExtensionHandle;
    decoder >> injectedBundlePathExtensionHandle;
    if (!injectedBundlePathExtensionHandle)
        return false;
    parameters.injectedBundlePathExtensionHandle = WTFMove(*injectedBundlePathExtensionHandle);

    Optional<SandboxExtension::HandleArray> additionalSandboxExtensionHandles;
    decoder >> additionalSandboxExtensionHandles;
    if (!additionalSandboxExtensionHandles)
        return false;
    parameters.additionalSandboxExtensionHandles = WTFMove(*additionalSandboxExtensionHandles);
    if (!decoder.decode(parameters.initializationUserData))
        return false;

#if PLATFORM(IOS_FAMILY)
    
    Optional<SandboxExtension::Handle> cookieStorageDirectoryExtensionHandle;
    decoder >> cookieStorageDirectoryExtensionHandle;
    if (!cookieStorageDirectoryExtensionHandle)
        return false;
    parameters.cookieStorageDirectoryExtensionHandle = WTFMove(*cookieStorageDirectoryExtensionHandle);

    Optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    decoder >> containerCachesDirectoryExtensionHandle;
    if (!containerCachesDirectoryExtensionHandle)
        return false;
    parameters.containerCachesDirectoryExtensionHandle = WTFMove(*containerCachesDirectoryExtensionHandle);

    Optional<SandboxExtension::Handle> containerTemporaryDirectoryExtensionHandle;
    decoder >> containerTemporaryDirectoryExtensionHandle;
    if (!containerTemporaryDirectoryExtensionHandle)
        return false;
    parameters.containerTemporaryDirectoryExtensionHandle = WTFMove(*containerTemporaryDirectoryExtensionHandle);

#endif
    if (!decoder.decode(parameters.webCoreLoggingChannels))
        return false;
    if (!decoder.decode(parameters.webKitLoggingChannels))
        return false;

#if ENABLE(MEDIA_STREAM)

    Optional<SandboxExtension::Handle> audioCaptureExtensionHandle;
    decoder >> audioCaptureExtensionHandle;
    if (!audioCaptureExtensionHandle)
        return false;
    parameters.audioCaptureExtensionHandle = WTFMove(*audioCaptureExtensionHandle);
#endif
    if (!decoder.decode(parameters.urlSchemesRegisteredAsEmptyDocument))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsSecure))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy))
        return false;
    if (!decoder.decode(parameters.urlSchemesForWhichDomainRelaxationIsForbidden))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsLocal))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsNoAccess))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsDisplayIsolated))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsCORSEnabled))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsAlwaysRevalidated))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsCachePartitioned))
        return false;
    if (!decoder.decode(parameters.urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest))
        return false;
    if (!decoder.decode(parameters.cacheModel))
        return false;
    if (!decoder.decode(parameters.shouldAlwaysUseComplexTextCodePath))
        return false;
    if (!decoder.decode(parameters.shouldEnableMemoryPressureReliefLogging))
        return false;
    if (!decoder.decode(parameters.shouldSuppressMemoryPressureHandler))
        return false;
    if (!decoder.decode(parameters.shouldUseFontSmoothing))
        return false;
    if (!decoder.decode(parameters.fontAllowList))
        return false;
    if (!decoder.decode(parameters.terminationTimeout))
        return false;
    if (!decoder.decode(parameters.overrideLanguages))
        return false;
#if USE(GSTREAMER)
    if (!decoder.decode(parameters.gstreamerOptions))
        return false;
#endif
    if (!decoder.decode(parameters.textCheckerState))
        return false;
    if (!decoder.decode(parameters.fullKeyboardAccessEnabled))
        return false;
    if (!decoder.decode(parameters.defaultRequestTimeoutInterval))
        return false;
    if (!decoder.decode(parameters.backForwardCacheCapacity))
        return false;
#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.uiProcessBundleIdentifier))
        return false;
    if (!decoder.decode(parameters.uiProcessSDKVersion))
        return false;
#endif
    if (!decoder.decode(parameters.presentingApplicationPID))
        return false;
#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.accessibilityEnhancedUserInterfaceEnabled))
        return false;
    if (!decoder.decode(parameters.acceleratedCompositingPort))
        return false;
    if (!decoder.decode(parameters.uiProcessBundleResourcePath))
        return false;
    
    Optional<SandboxExtension::Handle> uiProcessBundleResourcePathExtensionHandle;
    decoder >> uiProcessBundleResourcePathExtensionHandle;
    if (!uiProcessBundleResourcePathExtensionHandle)
        return false;
    parameters.uiProcessBundleResourcePathExtensionHandle = WTFMove(*uiProcessBundleResourcePathExtensionHandle);

    if (!decoder.decode(parameters.shouldEnableJIT))
        return false;
    if (!decoder.decode(parameters.shouldEnableFTLJIT))
        return false;

    bool hasBundleParameterData;
    if (!decoder.decode(hasBundleParameterData))
        return false;

    if (hasBundleParameterData) {
        IPC::DataReference dataReference;
        if (!decoder.decode(dataReference))
            return false;

        parameters.bundleParameterData = API::Data::create(dataReference.data(), dataReference.size());
    }
#endif

#if ENABLE(NOTIFICATIONS)
    if (!decoder.decode(parameters.notificationPermissions))
        return false;
#endif

    if (!decoder.decode(parameters.plugInAutoStartOrigins))
        return false;
    if (!decoder.decode(parameters.memoryCacheDisabled))
        return false;
    if (!decoder.decode(parameters.attrStyleEnabled))
        return false;

#if ENABLE(SERVICE_CONTROLS)
    if (!decoder.decode(parameters.hasImageServices))
        return false;
    if (!decoder.decode(parameters.hasSelectionServices))
        return false;
    if (!decoder.decode(parameters.hasRichContentServices))
        return false;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (!decoder.decode(parameters.pluginLoadClientPolicies))
        return false;
#endif

#if PLATFORM(COCOA)
    if (!IPC::decode(decoder, parameters.networkATSContext))
        return false;
#endif

#if PLATFORM(WAYLAND)
    if (!decoder.decode(parameters.waylandCompositorDisplayName))
        return false;
#endif

#if USE(SOUP)
    if (!decoder.decode(parameters.proxySettings))
        return false;
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (!decoder.decode(parameters.shouldLogUserInteraction))
        return false;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.mediaMIMETypes))
        return false;

    Optional<WebCore::ScreenProperties> screenProperties;
    decoder >> screenProperties;
    if (!screenProperties)
        return false;
    parameters.screenProperties = WTFMove(*screenProperties);
#endif

#if PLATFORM(MAC)
    if (!decoder.decode(parameters.useOverlayScrollbars))
        return false;
#endif

#if USE(WPE_RENDERER)
    if (!decoder.decode(parameters.isServiceWorkerProcess))
        return false;
    if (!decoder.decode(parameters.hostClientFileDescriptor))
        return false;
    if (!decoder.decode(parameters.implementationLibraryName))
        return false;
#endif

    Optional<Optional<WebProcessDataStoreParameters>> websiteDataStoreParameters;
    decoder >> websiteDataStoreParameters;
    if (!websiteDataStoreParameters)
        return false;
    parameters.websiteDataStoreParameters = WTFMove(*websiteDataStoreParameters);

#if PLATFORM(IOS)
    Optional<SandboxExtension::HandleArray> compilerServiceExtensionHandles;
    decoder >> compilerServiceExtensionHandles;
    if (!compilerServiceExtensionHandles)
        return false;
    parameters.compilerServiceExtensionHandles = WTFMove(*compilerServiceExtensionHandles);
#endif

    Optional<Optional<SandboxExtension::Handle>> containerManagerExtensionHandle;
    decoder >> containerManagerExtensionHandle;
    if (!containerManagerExtensionHandle)
        return false;
    parameters.containerManagerExtensionHandle = WTFMove(*containerManagerExtensionHandle);

    Optional<Optional<SandboxExtension::Handle>> mobileGestaltExtensionHandle;
    decoder >> mobileGestaltExtensionHandle;
    if (!mobileGestaltExtensionHandle)
        return false;
    parameters.mobileGestaltExtensionHandle = WTFMove(*mobileGestaltExtensionHandle);

#if PLATFORM(IOS_FAMILY)
    Optional<SandboxExtension::HandleArray> diagnosticsExtensionHandles;
    decoder >> diagnosticsExtensionHandles;
    if (!diagnosticsExtensionHandles)
        return false;
    parameters.diagnosticsExtensionHandles = WTFMove(*diagnosticsExtensionHandles);

    Optional<SandboxExtension::HandleArray> dynamicMachExtensionHandles;
    decoder >> dynamicMachExtensionHandles;
    if (!dynamicMachExtensionHandles)
        return false;
    parameters.dynamicMachExtensionHandles = WTFMove(*dynamicMachExtensionHandles);

    Optional<SandboxExtension::HandleArray> dynamicIOKitExtensionHandles;
    decoder >> dynamicIOKitExtensionHandles;
    if (!dynamicIOKitExtensionHandles)
        return false;
    parameters.dynamicIOKitExtensionHandles = WTFMove(*dynamicIOKitExtensionHandles);
#endif

#if PLATFORM(COCOA)
    Optional<bool> systemHasBattery;
    decoder >> systemHasBattery;
    if (!systemHasBattery)
        return false;
    parameters.systemHasBattery = WTFMove(*systemHasBattery);

    Optional<bool> systemHasAC;
    decoder >> systemHasAC;
    if (!systemHasAC)
        return false;
    parameters.systemHasAC = WTFMove(*systemHasAC);
#endif

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(parameters.currentUserInterfaceIdiomIsPad))
        return false;

    if (!decoder.decode(parameters.supportsPictureInPicture))
        return false;

    Optional<WebCore::RenderThemeIOS::CSSValueToSystemColorMap> cssValueToSystemColorMap;
    decoder >> cssValueToSystemColorMap;
    if (!cssValueToSystemColorMap)
        return false;
    parameters.cssValueToSystemColorMap = WTFMove(*cssValueToSystemColorMap);

    Optional<WebCore::Color> focusRingColor;
    decoder >> focusRingColor;
    if (!focusRingColor)
        return false;
    parameters.focusRingColor = WTFMove(*focusRingColor);
    
    if (!decoder.decode(parameters.localizedDeviceModel))
        return false;
#endif

#if PLATFORM(COCOA)
    // FIXME(207716): The following should be removed when the GPU process is complete.
    Optional<SandboxExtension::HandleArray> mediaExtensionHandles;
    decoder >> mediaExtensionHandles;
    if (!mediaExtensionHandles)
        return false;
    parameters.mediaExtensionHandles = WTFMove(*mediaExtensionHandles);
    // FIXME(207716): End region to remove.

#if ENABLE(CFPREFS_DIRECT_MODE)
    Optional<Optional<SandboxExtension::HandleArray>> preferencesExtensionHandles;
    decoder >> preferencesExtensionHandles;
    if (!preferencesExtensionHandles)
        return false;
    parameters.preferencesExtensionHandles = WTFMove(*preferencesExtensionHandles);
#endif
#endif

#if PLATFORM(GTK)
    Optional<bool> useSystemAppearanceForScrollbars;
    decoder >> useSystemAppearanceForScrollbars;
    if (!useSystemAppearanceForScrollbars)
        return false;
    parameters.useSystemAppearanceForScrollbars = WTFMove(*useSystemAppearanceForScrollbars);
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    Optional<std::pair<int64_t, double>> overrideUserInterfaceIdiomAndScale;
    decoder >> overrideUserInterfaceIdiomAndScale;
    if (!overrideUserInterfaceIdiomAndScale)
        return false;
    parameters.overrideUserInterfaceIdiomAndScale = WTFMove(*overrideUserInterfaceIdiomAndScale);
#endif

    return true;
}

} // namespace WebKit
