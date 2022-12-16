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

#include "config.h"
#include "WebPageCreationParameters.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

void WebPageCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << viewSize;
    encoder << activityState;

    encoder << store;
    encoder << drawingAreaType;
    encoder << drawingAreaIdentifier;
    encoder << webPageProxyIdentifier;
    encoder << pageGroupData;
    encoder << isEditable;
    encoder << underlayColor;
    encoder << useFixedLayout;
    encoder << fixedLayoutSize;
    encoder << defaultUnobscuredSize;
    encoder << minimumUnobscuredSize;
    encoder << maximumUnobscuredSize;
    encoder << viewExposedRect;
    encoder << alwaysShowsHorizontalScroller;
    encoder << alwaysShowsVerticalScroller;
    encoder << paginationMode;
    encoder << paginationBehavesLikeColumns;
    encoder << pageLength;
    encoder << gapBetweenPages;
    encoder << userAgent;
    encoder << itemStatesWereRestoredByAPIRequest;
    encoder << itemStates;
    encoder << visitedLinkTableID;
    encoder << canRunBeforeUnloadConfirmPanel;
    encoder << canRunModal;
    encoder << deviceScaleFactor;
    encoder << viewScaleFactor;
    encoder << textZoomFactor;
    encoder << pageZoomFactor;
    encoder << topContentInset;
    encoder << mediaVolume;
    encoder << muted;
    encoder << openedByDOM;
    encoder << mayStartMediaWhenInWindow;
    encoder << mediaPlaybackIsSuspended;
    encoder << minimumSizeForAutoLayout;
    encoder << sizeToContentAutoSizeMaximumSize;
    encoder << autoSizingShouldExpandToViewHeight;
    encoder << viewportSizeForCSSViewportUnits;
    encoder << scrollPinningBehavior;
    encoder << scrollbarOverlayStyle;
    encoder << backgroundExtendsBeyondPage;
    encoder << layerHostingMode;
    encoder << mimeTypesWithCustomContentProviders;
    encoder << controlledByAutomation;
    encoder << isProcessSwap;
    encoder << useDarkAppearance;
    encoder << useElevatedUserInterfaceLevel;
    encoder << hasResourceLoadClient;

#if PLATFORM(MAC)
    encoder << colorSpace;
    encoder << useSystemAppearance;
    encoder << useFormSemanticContext;
#endif

#if ENABLE(META_VIEWPORT)
    encoder << ignoresViewportScaleLimits;
    encoder << viewportConfigurationViewLayoutSize;
    encoder << viewportConfigurationLayoutSizeScaleFactor;
    encoder << viewportConfigurationMinimumEffectiveDeviceWidth;
    encoder << viewportConfigurationViewSize;
    encoder << overrideViewportArguments;
#endif

#if PLATFORM(IOS_FAMILY)
    encoder << screenSize;
    encoder << availableScreenSize;
    encoder << overrideScreenSize;
    encoder << textAutosizingWidth;
    encoder << deviceOrientation;
    encoder << keyboardIsAttached;
    encoder << canShowWhileLocked;
    encoder << isCapturingScreen;
#endif
#if PLATFORM(COCOA)
    encoder << smartInsertDeleteEnabled;
    encoder << additionalSupportedImageTypes;
    encoder << gpuIOKitExtensionHandles;
    encoder << gpuMachExtensionHandles;
#endif
#if HAVE(STATIC_FONT_REGISTRY)
    encoder << fontMachExtensionHandles;
#endif
#if HAVE(APP_ACCENT_COLORS)
    encoder << accentColor;
#endif
#if USE(WPE_RENDERER)
    encoder << hostFileDescriptor;
#endif
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    encoder << nativeWindowHandle;
#endif
#if USE(GRAPHICS_LAYER_WC)
    encoder << usesOffscreenRendering;
#endif
    encoder << shouldScaleViewToFitDocument;
    encoder << userInterfaceLayoutDirection;
    encoder << observedLayoutMilestones;
    encoder << overrideContentSecurityPolicy;
    encoder << cpuLimit;
    encoder << urlSchemeHandlers;
    encoder << urlSchemesWithLegacyCustomProtocolHandlers;
#if ENABLE(APPLICATION_MANIFEST)
    encoder << applicationManifest;
#endif
    encoder << needsFontAttributes;
    encoder << iceCandidateFilteringEnabled;
    encoder << enumeratingAllNetworkInterfacesEnabled;
    encoder << userContentControllerParameters;
#if ENABLE(WK_WEB_EXTENSIONS)
    encoder << webExtensionControllerParameters;
#endif
    encoder << backgroundColor;
    encoder << oldPageID;
    encoder << overriddenMediaType;
    encoder << corsDisablingPatterns;
    encoder << maskedURLSchemes;
    encoder << loadsSubresources;
    encoder << allowedNetworkHosts;
    encoder << userScriptsShouldWaitUntilNotification;
    encoder << crossOriginAccessControlCheckEnabled;
    encoder << processDisplayName;

    encoder << shouldCaptureAudioInUIProcess;
    encoder << shouldCaptureAudioInGPUProcess;
    encoder << shouldCaptureVideoInUIProcess;
    encoder << shouldCaptureVideoInGPUProcess;
    encoder << shouldCaptureDisplayInUIProcess;
    encoder << shouldCaptureDisplayInGPUProcess;
    encoder << shouldRenderCanvasInGPUProcess;
    encoder << shouldRenderDOMInGPUProcess;
    encoder << shouldPlayMediaInGPUProcess;
#if ENABLE(WEBGL)
    encoder << shouldRenderWebGLInGPUProcess;
#endif
    encoder << shouldEnableVP8Decoder;
    encoder << shouldEnableVP9Decoder;
    encoder << shouldEnableVP9SWDecoder;
#if ENABLE(APP_BOUND_DOMAINS)
    encoder << limitsNavigationsToAppBoundDomains;
#endif
    encoder << lastNavigationWasAppInitiated;
    encoder << shouldRelaxThirdPartyCookieBlocking;
    encoder << canUseCredentialStorage;

    encoder << httpsUpgradeEnabled;
#if PLATFORM(IOS)
    encoder << allowsDeprecatedSynchronousXMLHttpRequestDuringUnload;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    encoder << appHighlightsVisible;
#endif

#if HAVE(TOUCH_BAR)
    encoder << requiresUserActionForEditingControlsManager;
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    encoder << hasResizableWindows;
#endif

    encoder << contentSecurityPolicyModeForExtension;
    encoder << mainFrameIdentifier;

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
    encoder << machBootstrapHandle;
#endif
}

std::optional<WebPageCreationParameters> WebPageCreationParameters::decode(IPC::Decoder& decoder)
{
    WebPageCreationParameters parameters;

    if (!decoder.decode(parameters.viewSize))
        return std::nullopt;
    if (!decoder.decode(parameters.activityState))
        return std::nullopt;
    if (!decoder.decode(parameters.store))
        return std::nullopt;
    if (!decoder.decode(parameters.drawingAreaType))
        return std::nullopt;
    std::optional<DrawingAreaIdentifier> drawingAreaIdentifier;
    decoder >> drawingAreaIdentifier;
    if (!drawingAreaIdentifier)
        return std::nullopt;
    parameters.drawingAreaIdentifier = *drawingAreaIdentifier;
    std::optional<WebPageProxyIdentifier> webPageProxyIdentifier;
    decoder >> webPageProxyIdentifier;
    if (!webPageProxyIdentifier)
        return std::nullopt;
    parameters.webPageProxyIdentifier = WTFMove(*webPageProxyIdentifier);
    std::optional<WebPageGroupData> pageGroupData;
    decoder >> pageGroupData;
    if (!pageGroupData)
        return std::nullopt;
    parameters.pageGroupData = WTFMove(*pageGroupData);
    if (!decoder.decode(parameters.isEditable))
        return std::nullopt;
    if (!decoder.decode(parameters.underlayColor))
        return std::nullopt;
    if (!decoder.decode(parameters.useFixedLayout))
        return std::nullopt;
    if (!decoder.decode(parameters.fixedLayoutSize))
        return std::nullopt;
    if (!decoder.decode(parameters.defaultUnobscuredSize))
        return std::nullopt;
    if (!decoder.decode(parameters.minimumUnobscuredSize))
        return std::nullopt;
    if (!decoder.decode(parameters.maximumUnobscuredSize))
        return std::nullopt;
    if (!decoder.decode(parameters.viewExposedRect))
        return std::nullopt;
    if (!decoder.decode(parameters.alwaysShowsHorizontalScroller))
        return std::nullopt;
    if (!decoder.decode(parameters.alwaysShowsVerticalScroller))
        return std::nullopt;
    if (!decoder.decode(parameters.paginationMode))
        return std::nullopt;
    if (!decoder.decode(parameters.paginationBehavesLikeColumns))
        return std::nullopt;
    if (!decoder.decode(parameters.pageLength))
        return std::nullopt;
    if (!decoder.decode(parameters.gapBetweenPages))
        return std::nullopt;

    std::optional<String> userAgent;
    decoder >> userAgent;
    if (!userAgent)
        return std::nullopt;
    parameters.userAgent = WTFMove(*userAgent);

    std::optional<bool> itemStatesWereRestoredByAPIRequest;
    decoder >> itemStatesWereRestoredByAPIRequest;
    if (!itemStatesWereRestoredByAPIRequest)
        return std::nullopt;
    parameters.itemStatesWereRestoredByAPIRequest = *itemStatesWereRestoredByAPIRequest;

    std::optional<Vector<BackForwardListItemState>> itemStates;
    decoder >> itemStates;
    if (!itemStates)
        return std::nullopt;
    parameters.itemStates = WTFMove(*itemStates);

    if (!decoder.decode(parameters.visitedLinkTableID))
        return std::nullopt;
    if (!decoder.decode(parameters.canRunBeforeUnloadConfirmPanel))
        return std::nullopt;
    if (!decoder.decode(parameters.canRunModal))
        return std::nullopt;
    if (!decoder.decode(parameters.deviceScaleFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.viewScaleFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.textZoomFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.pageZoomFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.topContentInset))
        return std::nullopt;
    if (!decoder.decode(parameters.mediaVolume))
        return std::nullopt;

    std::optional<WebCore::MediaProducerMutedStateFlags> mutedStateFlags;
    decoder >> mutedStateFlags;
    if (!mutedStateFlags)
        return std::nullopt;
    parameters.muted = *mutedStateFlags;

    if (!decoder.decode(parameters.openedByDOM))
        return std::nullopt;
    if (!decoder.decode(parameters.mayStartMediaWhenInWindow))
        return std::nullopt;
    if (!decoder.decode(parameters.mediaPlaybackIsSuspended))
        return std::nullopt;
    if (!decoder.decode(parameters.minimumSizeForAutoLayout))
        return std::nullopt;
    if (!decoder.decode(parameters.sizeToContentAutoSizeMaximumSize))
        return std::nullopt;
    if (!decoder.decode(parameters.autoSizingShouldExpandToViewHeight))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportSizeForCSSViewportUnits))
        return std::nullopt;
    if (!decoder.decode(parameters.scrollPinningBehavior))
        return std::nullopt;

    std::optional<std::optional<uint32_t>> scrollbarOverlayStyle;
    decoder >> scrollbarOverlayStyle;
    if (!scrollbarOverlayStyle)
        return std::nullopt;
    parameters.scrollbarOverlayStyle = WTFMove(*scrollbarOverlayStyle);

    if (!decoder.decode(parameters.backgroundExtendsBeyondPage))
        return std::nullopt;
    if (!decoder.decode(parameters.layerHostingMode))
        return std::nullopt;
    if (!decoder.decode(parameters.mimeTypesWithCustomContentProviders))
        return std::nullopt;
    if (!decoder.decode(parameters.controlledByAutomation))
        return std::nullopt;
    if (!decoder.decode(parameters.isProcessSwap))
        return std::nullopt;
    if (!decoder.decode(parameters.useDarkAppearance))
        return std::nullopt;
    if (!decoder.decode(parameters.useElevatedUserInterfaceLevel))
        return std::nullopt;

    std::optional<bool> hasResourceLoadClient;
    decoder >> hasResourceLoadClient;
    if (!hasResourceLoadClient)
        return std::nullopt;
    parameters.hasResourceLoadClient = WTFMove(*hasResourceLoadClient);

#if PLATFORM(MAC)
    std::optional<std::optional<WebCore::DestinationColorSpace>> colorSpace;
    decoder >> colorSpace;
    if (!colorSpace)
        return std::nullopt;
    parameters.colorSpace = WTFMove(*colorSpace);
    if (!decoder.decode(parameters.useSystemAppearance))
        return std::nullopt;
    if (!decoder.decode(parameters.useFormSemanticContext))
        return std::nullopt;
#endif

#if ENABLE(META_VIEWPORT)
    if (!decoder.decode(parameters.ignoresViewportScaleLimits))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationViewLayoutSize))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationLayoutSizeScaleFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationMinimumEffectiveDeviceWidth))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationViewSize))
        return std::nullopt;
    std::optional<std::optional<WebCore::ViewportArguments>> overrideViewportArguments;
    decoder >> overrideViewportArguments;
    if (!overrideViewportArguments)
        return std::nullopt;
    parameters.overrideViewportArguments = WTFMove(*overrideViewportArguments);
#endif

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(parameters.screenSize))
        return std::nullopt;
    if (!decoder.decode(parameters.availableScreenSize))
        return std::nullopt;
    if (!decoder.decode(parameters.overrideScreenSize))
        return std::nullopt;
    if (!decoder.decode(parameters.textAutosizingWidth))
        return std::nullopt;
    if (!decoder.decode(parameters.deviceOrientation))
        return std::nullopt;
    if (!decoder.decode(parameters.keyboardIsAttached))
        return std::nullopt;
    if (!decoder.decode(parameters.canShowWhileLocked))
        return std::nullopt;
    if (!decoder.decode(parameters.isCapturingScreen))
        return std::nullopt;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.smartInsertDeleteEnabled))
        return std::nullopt;
    if (!decoder.decode(parameters.additionalSupportedImageTypes))
        return std::nullopt;

    std::optional<Vector<SandboxExtension::Handle>> gpuIOKitExtensionHandles;
    decoder >> gpuIOKitExtensionHandles;
    if (!gpuIOKitExtensionHandles)
        return std::nullopt;
    parameters.gpuIOKitExtensionHandles = WTFMove(*gpuIOKitExtensionHandles);

    std::optional<Vector<SandboxExtension::Handle>> gpuMachExtensionHandles;
    decoder >> gpuMachExtensionHandles;
    if (!gpuMachExtensionHandles)
        return std::nullopt;
    parameters.gpuMachExtensionHandles = WTFMove(*gpuMachExtensionHandles);
#endif

#if HAVE(STATIC_FONT_REGISTRY)
    std::optional<Vector<SandboxExtension::Handle>> fontMachExtensionHandles;
    decoder >> fontMachExtensionHandles;
    if (!fontMachExtensionHandles)
        return std::nullopt;
    parameters.fontMachExtensionHandles = WTFMove(*fontMachExtensionHandles);
#endif

#if HAVE(APP_ACCENT_COLORS)
    if (!decoder.decode(parameters.accentColor))
        return std::nullopt;
#endif

#if USE(WPE_RENDERER)
    if (!decoder.decode(parameters.hostFileDescriptor))
        return std::nullopt;
#endif

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    if (!decoder.decode(parameters.nativeWindowHandle))
        return std::nullopt;
#endif
#if USE(GRAPHICS_LAYER_WC)
    if (!decoder.decode(parameters.usesOffscreenRendering))
        return std::nullopt;
#endif

    if (!decoder.decode(parameters.shouldScaleViewToFitDocument))
        return std::nullopt;

    if (!decoder.decode(parameters.userInterfaceLayoutDirection))
        return std::nullopt;
    if (!decoder.decode(parameters.observedLayoutMilestones))
        return std::nullopt;

    if (!decoder.decode(parameters.overrideContentSecurityPolicy))
        return std::nullopt;

    std::optional<std::optional<double>> cpuLimit;
    decoder >> cpuLimit;
    if (!cpuLimit)
        return std::nullopt;
    parameters.cpuLimit = WTFMove(*cpuLimit);

    if (!decoder.decode(parameters.urlSchemeHandlers))
        return std::nullopt;
    
    std::optional<Vector<String>> urlSchemesWithLegacyCustomProtocolHandlers;
    decoder >> urlSchemesWithLegacyCustomProtocolHandlers;
    if (!urlSchemesWithLegacyCustomProtocolHandlers)
        return std::nullopt;
    parameters.urlSchemesWithLegacyCustomProtocolHandlers = WTFMove(*urlSchemesWithLegacyCustomProtocolHandlers);

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<std::optional<WebCore::ApplicationManifest>> applicationManifest;
    decoder >> applicationManifest;
    if (!applicationManifest)
        return std::nullopt;
    parameters.applicationManifest = WTFMove(*applicationManifest);
#endif
    if (!decoder.decode(parameters.needsFontAttributes))
        return std::nullopt;

    if (!decoder.decode(parameters.iceCandidateFilteringEnabled))
        return std::nullopt;

    if (!decoder.decode(parameters.enumeratingAllNetworkInterfacesEnabled))
        return std::nullopt;

    std::optional<UserContentControllerParameters> userContentControllerParameters;
    decoder >> userContentControllerParameters;
    if (!userContentControllerParameters)
        return std::nullopt;
    parameters.userContentControllerParameters = WTFMove(*userContentControllerParameters);

#if ENABLE(WK_WEB_EXTENSIONS)
    std::optional<std::optional<WebExtensionControllerParameters>> webExtensionControllerParameters;
    decoder >> webExtensionControllerParameters;
    if (!webExtensionControllerParameters)
        return std::nullopt;
    parameters.webExtensionControllerParameters = WTFMove(*webExtensionControllerParameters);
#endif

    std::optional<std::optional<WebCore::Color>> backgroundColor;
    decoder >> backgroundColor;
    if (!backgroundColor)
        return std::nullopt;
    parameters.backgroundColor = WTFMove(*backgroundColor);

    std::optional<std::optional<WebCore::PageIdentifier>> oldPageID;
    decoder >> oldPageID;
    if (!oldPageID)
        return std::nullopt;
    parameters.oldPageID = WTFMove(*oldPageID);

    if (!decoder.decode(parameters.overriddenMediaType))
        return std::nullopt;

    std::optional<Vector<String>> corsDisablingPatterns;
    decoder >> corsDisablingPatterns;
    if (!corsDisablingPatterns)
        return std::nullopt;
    parameters.corsDisablingPatterns = WTFMove(*corsDisablingPatterns);

    std::optional<HashSet<String>> maskedURLSchemes;
    decoder >> maskedURLSchemes;
    if (!maskedURLSchemes)
        return std::nullopt;
    parameters.maskedURLSchemes = WTFMove(*maskedURLSchemes);

    std::optional<bool> loadsSubresources;
    decoder >> loadsSubresources;
    if (!loadsSubresources)
        return std::nullopt;
    parameters.loadsSubresources = *loadsSubresources;

    std::optional<std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<String>>> allowedNetworkHosts;
    decoder >> allowedNetworkHosts;
    if (!allowedNetworkHosts)
        return std::nullopt;
    parameters.allowedNetworkHosts = *allowedNetworkHosts;

    std::optional<bool> userScriptsShouldWaitUntilNotification;
    decoder >> userScriptsShouldWaitUntilNotification;
    if (!userScriptsShouldWaitUntilNotification)
        return std::nullopt;
    parameters.userScriptsShouldWaitUntilNotification = *userScriptsShouldWaitUntilNotification;
    
    std::optional<bool> crossOriginAccessControlCheckEnabled;
    decoder >> crossOriginAccessControlCheckEnabled;
    if (!crossOriginAccessControlCheckEnabled)
        return std::nullopt;
    parameters.crossOriginAccessControlCheckEnabled = *crossOriginAccessControlCheckEnabled;
    
    std::optional<String> processDisplayName;
    decoder >> processDisplayName;
    if (!processDisplayName)
        return std::nullopt;
    parameters.processDisplayName = WTFMove(*processDisplayName);
    
    if (!decoder.decode(parameters.shouldCaptureAudioInUIProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldCaptureAudioInGPUProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldCaptureVideoInUIProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldCaptureVideoInGPUProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldCaptureDisplayInUIProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldCaptureDisplayInGPUProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldRenderCanvasInGPUProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldRenderDOMInGPUProcess))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldPlayMediaInGPUProcess))
        return std::nullopt;
#if ENABLE(WEBGL)
    if (!decoder.decode(parameters.shouldRenderWebGLInGPUProcess))
        return std::nullopt;
#endif

    if (!decoder.decode(parameters.shouldEnableVP8Decoder))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldEnableVP9Decoder))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldEnableVP9SWDecoder))
        return std::nullopt;

#if ENABLE(APP_BOUND_DOMAINS)
    if (!decoder.decode(parameters.limitsNavigationsToAppBoundDomains))
        return std::nullopt;
#endif
    if (!decoder.decode(parameters.lastNavigationWasAppInitiated))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldRelaxThirdPartyCookieBlocking))
        return std::nullopt;

    if (!decoder.decode(parameters.canUseCredentialStorage))
        return std::nullopt;

    if (!decoder.decode(parameters.httpsUpgradeEnabled))
        return std::nullopt;

#if PLATFORM(IOS)
    if (!decoder.decode(parameters.allowsDeprecatedSynchronousXMLHttpRequestDuringUnload))
        return std::nullopt;
#endif
    
#if ENABLE(APP_HIGHLIGHTS)
    if (!decoder.decode(parameters.appHighlightsVisible))
        return std::nullopt;
#endif

#if HAVE(TOUCH_BAR)
    if (!decoder.decode(parameters.requiresUserActionForEditingControlsManager))
        return std::nullopt;
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    if (!decoder.decode(parameters.hasResizableWindows))
        return std::nullopt;
#endif

    if (!decoder.decode(parameters.contentSecurityPolicyModeForExtension))
        return std::nullopt;

    if (!decoder.decode(parameters.mainFrameIdentifier))
        return std::nullopt;

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
    std::optional<SandboxExtension::Handle> machBootstrapHandle;
    decoder >> machBootstrapHandle;
    if (!machBootstrapHandle)
        return std::nullopt;
    parameters.machBootstrapHandle = WTFMove(*machBootstrapHandle);
#endif

    return { WTFMove(parameters) };
}

} // namespace WebKit
