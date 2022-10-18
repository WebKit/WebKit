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
    encoder << auxiliaryProcessParameters;
    encoder << injectedBundlePath;
    encoder << injectedBundlePathExtensionHandle;
    encoder << additionalSandboxExtensionHandles;
    encoder << initializationUserData;
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    encoder << enableRemoteWebInspectorExtensionHandle;
#endif
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
    encoder << shouldUseFontSmoothingForTesting;
    encoder << fontAllowList;
    encoder << overrideLanguages;
#if USE(GSTREAMER)
    encoder << gstreamerOptions;
#endif
    encoder << textCheckerState;
    encoder << fullKeyboardAccessEnabled;
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    encoder << hasMouseDevice;
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    encoder << hasStylusDevice;
#endif
    encoder << defaultRequestTimeoutInterval;
    encoder << backForwardCacheCapacity;
#if PLATFORM(COCOA)
    encoder << uiProcessBundleIdentifier;
    encoder << latencyQOS;
    encoder << throughputQOS;
    encoder << presentingApplicationBundleIdentifier;
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

    encoder << memoryCacheDisabled;
    encoder << attrStyleEnabled;
    encoder << shouldThrowExceptionForGlobalConstantRedeclaration;
    encoder << crossOriginMode;
    encoder << isLockdownModeEnabled;

#if ENABLE(SERVICE_CONTROLS)
    encoder << hasImageServices;
    encoder << hasSelectionServices;
    encoder << hasRichContentServices;
#endif

#if PLATFORM(COCOA)
    encoder << networkATSContext;
#endif

#if PLATFORM(WAYLAND)
    encoder << waylandCompositorDisplayName;
#endif

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
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

    encoder << mobileGestaltExtensionHandle;
    encoder << launchServicesExtensionHandle;

#if HAVE(VIDEO_RESTRICTED_DECODING)
#if PLATFORM(MAC)
    encoder << trustdExtensionHandle;
#endif
    encoder << enableDecodingHEIC;
    encoder << enableDecodingAVIF;
#endif

#if PLATFORM(IOS_FAMILY)
    encoder << dynamicIOKitExtensionHandles;
#endif

#if PLATFORM(COCOA)
    encoder << systemHasBattery;
    encoder << systemHasAC;
#endif

#if PLATFORM(IOS_FAMILY)
    encoder << currentUserInterfaceIdiomIsSmallScreen;
    encoder << supportsPictureInPicture;
    encoder << cssValueToSystemColorMap;
    encoder << focusRingColor;
    encoder << localizedDeviceModel;
    encoder << contentSizeCategory;
#endif

#if PLATFORM(GTK)
    encoder << useSystemAppearanceForScrollbars;
    encoder << gtkSettings;
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    encoder << overrideUserInterfaceIdiomAndScale;
#endif

#if HAVE(IOSURFACE)
    encoder << maximumIOSurfaceSize;
    encoder << bytesPerRowIOSurfaceAlignment;
#endif

    encoder << accessibilityPreferences;
#if PLATFORM(IOS_FAMILY)
    encoder << applicationAccessibilityEnabled;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    encoder << memoryPressureHandlerConfiguration;
#endif

#if USE(GLIB)
    encoder << applicationID;
    encoder << applicationName;
#if ENABLE(REMOTE_INSPECTOR)
    encoder << inspectorServerAddress;
#endif
#endif

#if USE(ATSPI)
    encoder << accessibilityBusAddress;
#endif

    encoder << timeZoneOverride;
}

bool WebProcessCreationParameters::decode(IPC::Decoder& decoder, WebProcessCreationParameters& parameters)
{
    if (!decoder.decode(parameters.auxiliaryProcessParameters))
        return false;
    if (!decoder.decode(parameters.injectedBundlePath))
        return false;
    
    std::optional<SandboxExtension::Handle> injectedBundlePathExtensionHandle;
    decoder >> injectedBundlePathExtensionHandle;
    if (!injectedBundlePathExtensionHandle)
        return false;
    parameters.injectedBundlePathExtensionHandle = WTFMove(*injectedBundlePathExtensionHandle);

    std::optional<Vector<SandboxExtension::Handle>> additionalSandboxExtensionHandles;
    decoder >> additionalSandboxExtensionHandles;
    if (!additionalSandboxExtensionHandles)
        return false;
    parameters.additionalSandboxExtensionHandles = WTFMove(*additionalSandboxExtensionHandles);
    if (!decoder.decode(parameters.initializationUserData))
        return false;
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    std::optional<SandboxExtension::Handle> enableRemoteWebInspectorExtensionHandle;
    decoder >> enableRemoteWebInspectorExtensionHandle;
    if (!enableRemoteWebInspectorExtensionHandle)
        return false;
    parameters.enableRemoteWebInspectorExtensionHandle = WTFMove(*enableRemoteWebInspectorExtensionHandle);
#endif
#if ENABLE(MEDIA_STREAM)
    std::optional<SandboxExtension::Handle> audioCaptureExtensionHandle;
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
    if (!decoder.decode(parameters.shouldUseFontSmoothingForTesting))
        return false;
    if (!decoder.decode(parameters.fontAllowList))
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
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    if (!decoder.decode(parameters.hasMouseDevice))
        return false;
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    if (!decoder.decode(parameters.hasStylusDevice))
        return false;
#endif
    if (!decoder.decode(parameters.defaultRequestTimeoutInterval))
        return false;
    if (!decoder.decode(parameters.backForwardCacheCapacity))
        return false;
#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.uiProcessBundleIdentifier))
        return false;
    if (!decoder.decode(parameters.latencyQOS))
        return false;
    if (!decoder.decode(parameters.throughputQOS))
        return false;
    if (!decoder.decode(parameters.presentingApplicationBundleIdentifier))
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
    
    std::optional<SandboxExtension::Handle> uiProcessBundleResourcePathExtensionHandle;
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

    if (!decoder.decode(parameters.memoryCacheDisabled))
        return false;
    if (!decoder.decode(parameters.attrStyleEnabled))
        return false;
    if (!decoder.decode(parameters.shouldThrowExceptionForGlobalConstantRedeclaration))
        return false;
    if (!decoder.decode(parameters.crossOriginMode))
        return false;
    if (!decoder.decode(parameters.isLockdownModeEnabled))
        return false;

#if ENABLE(SERVICE_CONTROLS)
    if (!decoder.decode(parameters.hasImageServices))
        return false;
    if (!decoder.decode(parameters.hasSelectionServices))
        return false;
    if (!decoder.decode(parameters.hasRichContentServices))
        return false;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.networkATSContext))
        return false;
#endif

#if PLATFORM(WAYLAND)
    if (!decoder.decode(parameters.waylandCompositorDisplayName))
        return false;
#endif

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
    if (!decoder.decode(parameters.shouldLogUserInteraction))
        return false;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.mediaMIMETypes))
        return false;

    std::optional<WebCore::ScreenProperties> screenProperties;
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

    std::optional<std::optional<WebProcessDataStoreParameters>> websiteDataStoreParameters;
    decoder >> websiteDataStoreParameters;
    if (!websiteDataStoreParameters)
        return false;
    parameters.websiteDataStoreParameters = WTFMove(*websiteDataStoreParameters);

#if PLATFORM(IOS)
    std::optional<Vector<SandboxExtension::Handle>> compilerServiceExtensionHandles;
    decoder >> compilerServiceExtensionHandles;
    if (!compilerServiceExtensionHandles)
        return false;
    parameters.compilerServiceExtensionHandles = WTFMove(*compilerServiceExtensionHandles);
#endif

    std::optional<std::optional<SandboxExtension::Handle>> mobileGestaltExtensionHandle;
    decoder >> mobileGestaltExtensionHandle;
    if (!mobileGestaltExtensionHandle)
        return false;
    parameters.mobileGestaltExtensionHandle = WTFMove(*mobileGestaltExtensionHandle);

    std::optional<std::optional<SandboxExtension::Handle>> launchServicesExtensionHandle;
    decoder >> launchServicesExtensionHandle;
    if (!launchServicesExtensionHandle)
        return false;
    parameters.launchServicesExtensionHandle = WTFMove(*launchServicesExtensionHandle);

#if HAVE(VIDEO_RESTRICTED_DECODING)
#if PLATFORM(MAC)
    std::optional<SandboxExtension::Handle> trustdExtensionHandle;
    decoder >> trustdExtensionHandle;
    if (!trustdExtensionHandle)
        return false;
    parameters.trustdExtensionHandle = WTFMove(*trustdExtensionHandle);
#endif
    std::optional<bool> enableDecodingHEIC;
    decoder >> enableDecodingHEIC;
    if (!enableDecodingHEIC)
        return false;
    parameters.enableDecodingHEIC = *enableDecodingHEIC;
    
    std::optional<bool> enableDecodingAVIF;
    decoder >> enableDecodingAVIF;
    if (!enableDecodingAVIF)
        return false;
    parameters.enableDecodingAVIF = *enableDecodingAVIF;
#endif

#if PLATFORM(IOS_FAMILY)
    std::optional<Vector<SandboxExtension::Handle>> dynamicIOKitExtensionHandles;
    decoder >> dynamicIOKitExtensionHandles;
    if (!dynamicIOKitExtensionHandles)
        return false;
    parameters.dynamicIOKitExtensionHandles = WTFMove(*dynamicIOKitExtensionHandles);
#endif

#if PLATFORM(COCOA)
    std::optional<bool> systemHasBattery;
    decoder >> systemHasBattery;
    if (!systemHasBattery)
        return false;
    parameters.systemHasBattery = WTFMove(*systemHasBattery);

    std::optional<bool> systemHasAC;
    decoder >> systemHasAC;
    if (!systemHasAC)
        return false;
    parameters.systemHasAC = WTFMove(*systemHasAC);
#endif

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(parameters.currentUserInterfaceIdiomIsSmallScreen))
        return false;

    if (!decoder.decode(parameters.supportsPictureInPicture))
        return false;

    std::optional<WebCore::RenderThemeIOS::CSSValueToSystemColorMap> cssValueToSystemColorMap;
    decoder >> cssValueToSystemColorMap;
    if (!cssValueToSystemColorMap)
        return false;
    parameters.cssValueToSystemColorMap = WTFMove(*cssValueToSystemColorMap);

    std::optional<WebCore::Color> focusRingColor;
    decoder >> focusRingColor;
    if (!focusRingColor)
        return false;
    parameters.focusRingColor = WTFMove(*focusRingColor);
    
    if (!decoder.decode(parameters.localizedDeviceModel))
        return false;

    if (!decoder.decode(parameters.contentSizeCategory))
        return false;
#endif

#if PLATFORM(GTK)
    std::optional<bool> useSystemAppearanceForScrollbars;
    decoder >> useSystemAppearanceForScrollbars;
    if (!useSystemAppearanceForScrollbars)
        return false;
    parameters.useSystemAppearanceForScrollbars = WTFMove(*useSystemAppearanceForScrollbars);
    std::optional<GtkSettingsState> gtkSettings;
    decoder >> gtkSettings;
    if (!gtkSettings)
        return false;
    parameters.gtkSettings = WTFMove(*gtkSettings);
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    std::optional<std::pair<int64_t, double>> overrideUserInterfaceIdiomAndScale;
    decoder >> overrideUserInterfaceIdiomAndScale;
    if (!overrideUserInterfaceIdiomAndScale)
        return false;
    parameters.overrideUserInterfaceIdiomAndScale = WTFMove(*overrideUserInterfaceIdiomAndScale);
#endif

#if HAVE(IOSURFACE)
    if (!decoder.decode(parameters.maximumIOSurfaceSize))
        return false;
    if (!decoder.decode(parameters.bytesPerRowIOSurfaceAlignment))
        return false;
#endif

    std::optional<AccessibilityPreferences> accessibilityPreferences;
    decoder >> accessibilityPreferences;
    if (!accessibilityPreferences)
        return false;
    parameters.accessibilityPreferences = WTFMove(*accessibilityPreferences);

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(parameters.applicationAccessibilityEnabled))
        return false;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    std::optional<std::optional<MemoryPressureHandler::Configuration>> memoryPressureHandlerConfiguration;
    decoder >> memoryPressureHandlerConfiguration;
    if (!memoryPressureHandlerConfiguration)
        return false;
    parameters.memoryPressureHandlerConfiguration = WTFMove(*memoryPressureHandlerConfiguration);
#endif

#if USE(GLIB)
    if (!decoder.decode(parameters.applicationID))
        return false;
    if (!decoder.decode(parameters.applicationName))
        return false;

#if ENABLE(REMOTE_INSPECTOR)
    std::optional<CString> inspectorServerAddress;
    decoder >> inspectorServerAddress;
    if (!inspectorServerAddress)
        return false;
    parameters.inspectorServerAddress = WTFMove(*inspectorServerAddress);
#endif
#endif

#if USE(ATSPI)
    std::optional<String> accessibilityBusAddress;
    decoder >> accessibilityBusAddress;
    if (!accessibilityBusAddress)
        return false;
    parameters.accessibilityBusAddress = WTFMove(*accessibilityBusAddress);
#endif

    std::optional<String> timeZoneOverride;
    decoder >> timeZoneOverride;
    if (!timeZoneOverride)
        return false;
    parameters.timeZoneOverride = WTFMove(*timeZoneOverride);

    return true;
}

} // namespace WebKit
