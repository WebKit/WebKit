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
    encoder.encodeEnum(drawingAreaType);
    encoder << drawingAreaIdentifier;
    encoder << webPageProxyIdentifier;
    encoder << pageGroupData;
    encoder << isEditable;
    encoder << underlayColor;
    encoder << useFixedLayout;
    encoder << fixedLayoutSize;
    encoder << alwaysShowsHorizontalScroller;
    encoder << alwaysShowsVerticalScroller;
    encoder.encodeEnum(paginationMode);
    encoder << paginationBehavesLikeColumns;
    encoder << pageLength;
    encoder << gapBetweenPages;
    encoder << paginationLineGridEnabled;
    encoder << userAgent;
    encoder << itemStates;
    encoder << userContentControllerID;
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
    encoder << mayStartMediaWhenInWindow;
    encoder << mediaPlaybackIsSuspended;
    encoder << minimumSizeForAutoLayout;
    encoder << autoSizingShouldExpandToViewHeight;
    encoder << viewportSizeForCSSViewportUnits;
    encoder.encodeEnum(scrollPinningBehavior);
    encoder << scrollbarOverlayStyle;
    encoder << backgroundExtendsBeyondPage;
    encoder.encodeEnum(layerHostingMode);
    encoder << mimeTypesWithCustomContentProviders;
    encoder << controlledByAutomation;
    encoder << isProcessSwap;
    encoder << useDarkAppearance;
    encoder << useElevatedUserInterfaceLevel;
    encoder << hasResourceLoadClient;

#if PLATFORM(MAC)
    encoder << colorSpace;
    encoder << useSystemAppearance;
#endif

#if ENABLE(META_VIEWPORT)
    encoder << ignoresViewportScaleLimits;
    encoder << viewportConfigurationViewLayoutSize;
    encoder << viewportConfigurationLayoutSizeScaleFactor;
    encoder << viewportConfigurationMinimumEffectiveDeviceWidth;
    encoder << viewportConfigurationViewSize;
    encoder << overrideViewportArguments;
    encoder << frontboardExtensionHandle;
    encoder << iconServicesExtensionHandle;
#endif

#if PLATFORM(IOS_FAMILY)
    encoder << screenSize;
    encoder << availableScreenSize;
    encoder << overrideScreenSize;
    encoder << textAutosizingWidth;
    encoder << maximumUnobscuredSize;
    encoder << deviceOrientation;
    encoder << keyboardIsAttached;
    encoder << canShowWhileLocked;
#endif
#if PLATFORM(COCOA)
    encoder << smartInsertDeleteEnabled;
    encoder << additionalSupportedImageTypes;
#endif
#if USE(WPE_RENDERER)
    encoder << hostFileDescriptor;
#endif
    encoder << appleMailPaginationQuirkEnabled;
    encoder << appleMailLinesClampEnabled;
    encoder << shouldScaleViewToFitDocument;
    encoder.encodeEnum(userInterfaceLayoutDirection);
    encoder << observedLayoutMilestones;
    encoder << overrideContentSecurityPolicy;
    encoder << cpuLimit;
    encoder << urlSchemeHandlers;
#if ENABLE(APPLICATION_MANIFEST)
    encoder << applicationManifest;
#endif
    encoder << needsFontAttributes;
    encoder << iceCandidateFilteringEnabled;
    encoder << enumeratingAllNetworkInterfacesEnabled;
    encoder << userContentWorlds;
    encoder << userScripts;
    encoder << userStyleSheets;
    encoder << messageHandlers;
#if ENABLE(CONTENT_EXTENSIONS)
    encoder << contentRuleLists;
#endif
    encoder << backgroundColor;
    encoder << oldPageID;
    encoder << overriddenMediaType;
    encoder << corsDisablingPatterns;

    encoder << shouldCaptureAudioInUIProcess;
    encoder << shouldCaptureAudioInGPUProcess;
    encoder << shouldCaptureVideoInUIProcess;
    encoder << shouldCaptureVideoInGPUProcess;
    encoder << shouldCaptureDisplayInUIProcess;

#if PLATFORM(GTK)
    encoder << themeName;
#endif
}

Optional<WebPageCreationParameters> WebPageCreationParameters::decode(IPC::Decoder& decoder)
{
    WebPageCreationParameters parameters;

    if (!decoder.decode(parameters.viewSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.activityState))
        return WTF::nullopt;
    if (!decoder.decode(parameters.store))
        return WTF::nullopt;
    if (!decoder.decodeEnum(parameters.drawingAreaType))
        return WTF::nullopt;
    Optional<DrawingAreaIdentifier> drawingAreaIdentifier;
    decoder >> drawingAreaIdentifier;
    if (!drawingAreaIdentifier)
        return WTF::nullopt;
    parameters.drawingAreaIdentifier = *drawingAreaIdentifier;
    Optional<WebPageProxyIdentifier> webPageProxyIdentifier;
    decoder >> webPageProxyIdentifier;
    if (!webPageProxyIdentifier)
        return WTF::nullopt;
    parameters.webPageProxyIdentifier = WTFMove(*webPageProxyIdentifier);
    Optional<WebPageGroupData> pageGroupData;
    decoder >> pageGroupData;
    if (!pageGroupData)
        return WTF::nullopt;
    parameters.pageGroupData = WTFMove(*pageGroupData);
    if (!decoder.decode(parameters.isEditable))
        return WTF::nullopt;
    if (!decoder.decode(parameters.underlayColor))
        return WTF::nullopt;
    if (!decoder.decode(parameters.useFixedLayout))
        return WTF::nullopt;
    if (!decoder.decode(parameters.fixedLayoutSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.alwaysShowsHorizontalScroller))
        return WTF::nullopt;
    if (!decoder.decode(parameters.alwaysShowsVerticalScroller))
        return WTF::nullopt;
    if (!decoder.decodeEnum(parameters.paginationMode))
        return WTF::nullopt;
    if (!decoder.decode(parameters.paginationBehavesLikeColumns))
        return WTF::nullopt;
    if (!decoder.decode(parameters.pageLength))
        return WTF::nullopt;
    if (!decoder.decode(parameters.gapBetweenPages))
        return WTF::nullopt;
    if (!decoder.decode(parameters.paginationLineGridEnabled))
        return WTF::nullopt;

    Optional<String> userAgent;
    decoder >> userAgent;
    if (!userAgent)
        return WTF::nullopt;
    parameters.userAgent = WTFMove(*userAgent);

    Optional<Vector<BackForwardListItemState>> itemStates;
    decoder >> itemStates;
    if (!itemStates)
        return WTF::nullopt;
    parameters.itemStates = WTFMove(*itemStates);

    Optional<UserContentControllerIdentifier> userContentControllerIdentifier;
    decoder >> userContentControllerIdentifier;
    if (!userContentControllerIdentifier)
        return WTF::nullopt;
    parameters.userContentControllerID = *userContentControllerIdentifier;

    if (!decoder.decode(parameters.visitedLinkTableID))
        return WTF::nullopt;
    if (!decoder.decode(parameters.canRunBeforeUnloadConfirmPanel))
        return WTF::nullopt;
    if (!decoder.decode(parameters.canRunModal))
        return WTF::nullopt;
    if (!decoder.decode(parameters.deviceScaleFactor))
        return WTF::nullopt;
    if (!decoder.decode(parameters.viewScaleFactor))
        return WTF::nullopt;
    if (!decoder.decode(parameters.textZoomFactor))
        return WTF::nullopt;
    if (!decoder.decode(parameters.pageZoomFactor))
        return WTF::nullopt;
    if (!decoder.decode(parameters.topContentInset))
        return WTF::nullopt;
    if (!decoder.decode(parameters.mediaVolume))
        return WTF::nullopt;
    if (!decoder.decode(parameters.muted))
        return WTF::nullopt;
    if (!decoder.decode(parameters.mayStartMediaWhenInWindow))
        return WTF::nullopt;
    if (!decoder.decode(parameters.mediaPlaybackIsSuspended))
        return WTF::nullopt;
    if (!decoder.decode(parameters.minimumSizeForAutoLayout))
        return WTF::nullopt;
    if (!decoder.decode(parameters.autoSizingShouldExpandToViewHeight))
        return WTF::nullopt;
    if (!decoder.decode(parameters.viewportSizeForCSSViewportUnits))
        return WTF::nullopt;
    if (!decoder.decodeEnum(parameters.scrollPinningBehavior))
        return WTF::nullopt;

    Optional<Optional<uint32_t>> scrollbarOverlayStyle;
    decoder >> scrollbarOverlayStyle;
    if (!scrollbarOverlayStyle)
        return WTF::nullopt;
    parameters.scrollbarOverlayStyle = WTFMove(*scrollbarOverlayStyle);

    if (!decoder.decode(parameters.backgroundExtendsBeyondPage))
        return WTF::nullopt;
    if (!decoder.decodeEnum(parameters.layerHostingMode))
        return WTF::nullopt;
    if (!decoder.decode(parameters.mimeTypesWithCustomContentProviders))
        return WTF::nullopt;
    if (!decoder.decode(parameters.controlledByAutomation))
        return WTF::nullopt;
    if (!decoder.decode(parameters.isProcessSwap))
        return WTF::nullopt;
    if (!decoder.decode(parameters.useDarkAppearance))
        return WTF::nullopt;
    if (!decoder.decode(parameters.useElevatedUserInterfaceLevel))
        return WTF::nullopt;

    Optional<bool> hasResourceLoadClient;
    decoder >> hasResourceLoadClient;
    if (!hasResourceLoadClient)
        return WTF::nullopt;
    parameters.hasResourceLoadClient = WTFMove(*hasResourceLoadClient);

#if PLATFORM(MAC)
    if (!decoder.decode(parameters.colorSpace))
        return WTF::nullopt;
    if (!decoder.decode(parameters.useSystemAppearance))
        return WTF::nullopt;
#endif

#if ENABLE(META_VIEWPORT)
    if (!decoder.decode(parameters.ignoresViewportScaleLimits))
        return WTF::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationViewLayoutSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationLayoutSizeScaleFactor))
        return WTF::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationMinimumEffectiveDeviceWidth))
        return WTF::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationViewSize))
        return WTF::nullopt;
    Optional<Optional<WebCore::ViewportArguments>> overrideViewportArguments;
    decoder >> overrideViewportArguments;
    if (!overrideViewportArguments)
        return WTF::nullopt;
    parameters.overrideViewportArguments = WTFMove(*overrideViewportArguments);

    Optional<Optional<SandboxExtension::Handle>> frontboardExtensionHandle;
    decoder >> frontboardExtensionHandle;
    if (!frontboardExtensionHandle)
        return WTF::nullopt;
    parameters.frontboardExtensionHandle = WTFMove(*frontboardExtensionHandle);

    Optional<Optional<SandboxExtension::Handle>> iconServicesExtensionHandle;
    decoder >> iconServicesExtensionHandle;
    if (!iconServicesExtensionHandle)
        return WTF::nullopt;
    parameters.iconServicesExtensionHandle = WTFMove(*iconServicesExtensionHandle);
#endif

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(parameters.screenSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.availableScreenSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.overrideScreenSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.textAutosizingWidth))
        return WTF::nullopt;
    if (!decoder.decode(parameters.maximumUnobscuredSize))
        return WTF::nullopt;
    if (!decoder.decode(parameters.deviceOrientation))
        return WTF::nullopt;
    if (!decoder.decode(parameters.keyboardIsAttached))
        return WTF::nullopt;
    if (!decoder.decode(parameters.canShowWhileLocked))
        return WTF::nullopt;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.smartInsertDeleteEnabled))
        return WTF::nullopt;
    if (!decoder.decode(parameters.additionalSupportedImageTypes))
        return WTF::nullopt;
#endif

#if USE(WPE_RENDERER)
    if (!decoder.decode(parameters.hostFileDescriptor))
        return WTF::nullopt;
#endif

    if (!decoder.decode(parameters.appleMailPaginationQuirkEnabled))
        return WTF::nullopt;

    if (!decoder.decode(parameters.appleMailLinesClampEnabled))
        return WTF::nullopt;

    if (!decoder.decode(parameters.shouldScaleViewToFitDocument))
        return WTF::nullopt;

    if (!decoder.decodeEnum(parameters.userInterfaceLayoutDirection))
        return WTF::nullopt;
    if (!decoder.decode(parameters.observedLayoutMilestones))
        return WTF::nullopt;

    if (!decoder.decode(parameters.overrideContentSecurityPolicy))
        return WTF::nullopt;

    Optional<Optional<double>> cpuLimit;
    decoder >> cpuLimit;
    if (!cpuLimit)
        return WTF::nullopt;
    parameters.cpuLimit = WTFMove(*cpuLimit);

    if (!decoder.decode(parameters.urlSchemeHandlers))
        return WTF::nullopt;

#if ENABLE(APPLICATION_MANIFEST)
    Optional<Optional<WebCore::ApplicationManifest>> applicationManifest;
    decoder >> applicationManifest;
    if (!applicationManifest)
        return WTF::nullopt;
    parameters.applicationManifest = WTFMove(*applicationManifest);
#endif
    if (!decoder.decode(parameters.needsFontAttributes))
        return WTF::nullopt;

    if (!decoder.decode(parameters.iceCandidateFilteringEnabled))
        return WTF::nullopt;

    if (!decoder.decode(parameters.enumeratingAllNetworkInterfacesEnabled))
        return WTF::nullopt;

    Optional<Vector<std::pair<ContentWorldIdentifier, String>>> userContentWorlds;
    decoder >> userContentWorlds;
    if (!userContentWorlds)
        return WTF::nullopt;
    parameters.userContentWorlds = WTFMove(*userContentWorlds);

    Optional<Vector<WebUserScriptData>> userScripts;
    decoder >> userScripts;
    if (!userScripts)
        return WTF::nullopt;
    parameters.userScripts = WTFMove(*userScripts);
    
    Optional<Vector<WebUserStyleSheetData>> userStyleSheets;
    decoder >> userStyleSheets;
    if (!userStyleSheets)
        return WTF::nullopt;
    parameters.userStyleSheets = WTFMove(*userStyleSheets);
    
    Optional<Vector<WebScriptMessageHandlerData>> messageHandlers;
    decoder >> messageHandlers;
    if (!messageHandlers)
        return WTF::nullopt;
    parameters.messageHandlers = WTFMove(*messageHandlers);
    
#if ENABLE(CONTENT_EXTENSIONS)
    Optional<Vector<std::pair<String, WebCompiledContentRuleListData>>> contentRuleLists;
    decoder >> contentRuleLists;
    if (!contentRuleLists)
        return WTF::nullopt;
    parameters.contentRuleLists = WTFMove(*contentRuleLists);
#endif

    Optional<Optional<WebCore::Color>> backgroundColor;
    decoder >> backgroundColor;
    if (!backgroundColor)
        return WTF::nullopt;
    parameters.backgroundColor = WTFMove(*backgroundColor);

    Optional<Optional<WebCore::PageIdentifier>> oldPageID;
    decoder >> oldPageID;
    if (!oldPageID)
        return WTF::nullopt;
    parameters.oldPageID = WTFMove(*oldPageID);

    if (!decoder.decode(parameters.overriddenMediaType))
        return WTF::nullopt;

    Optional<Vector<String>> corsDisablingPatterns;
    decoder >> corsDisablingPatterns;
    if (!corsDisablingPatterns)
        return WTF::nullopt;
    parameters.corsDisablingPatterns = WTFMove(*corsDisablingPatterns);

    if (!decoder.decode(parameters.shouldCaptureAudioInUIProcess))
        return WTF::nullopt;

    if (!decoder.decode(parameters.shouldCaptureAudioInGPUProcess))
        return WTF::nullopt;

    if (!decoder.decode(parameters.shouldCaptureVideoInUIProcess))
        return WTF::nullopt;

    if (!decoder.decode(parameters.shouldCaptureVideoInGPUProcess))
        return WTF::nullopt;

    if (!decoder.decode(parameters.shouldCaptureDisplayInUIProcess))
        return WTF::nullopt;

#if PLATFORM(GTK)
    if (!decoder.decode(parameters.themeName))
        return WTF::nullopt;
#endif

    return parameters;
}

} // namespace WebKit
