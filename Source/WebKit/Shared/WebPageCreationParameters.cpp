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
    encoder << pageGroupData;
    encoder << drawsBackground;
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
    encoder << sessionID;
    encoder << userContentControllerID.toUInt64();
    encoder << visitedLinkTableID;
    encoder << websiteDataStoreID;
    encoder << canRunBeforeUnloadConfirmPanel;
    encoder << canRunModal;
    encoder << deviceScaleFactor;
    encoder << viewScaleFactor;
    encoder << topContentInset;
    encoder << mediaVolume;
    encoder << muted;
    encoder << mayStartMediaWhenInWindow;
    encoder << viewLayoutSize;
    encoder << autoSizingShouldExpandToViewHeight;
    encoder << viewportSizeForCSSViewportUnits;
    encoder.encodeEnum(scrollPinningBehavior);
    encoder << scrollbarOverlayStyle;
    encoder << backgroundExtendsBeyondPage;
    encoder.encodeEnum(layerHostingMode);
    encoder << mimeTypesWithCustomContentProviders;
    encoder << controlledByAutomation;

#if PLATFORM(MAC)
    encoder << colorSpace;
    encoder << useSystemAppearance;
    encoder << useDarkAppearance;
    encoder << shouldDelayAttachingDrawingArea;
#endif
#if PLATFORM(IOS_FAMILY)
    encoder << screenSize;
    encoder << availableScreenSize;
    encoder << overrideScreenSize;
    encoder << textAutosizingWidth;
    encoder << ignoresViewportScaleLimits;
    encoder << viewportConfigurationViewLayoutSize;
    encoder << viewportConfigurationLayoutSizeScaleFactor;
    encoder << viewportConfigurationViewSize;
    encoder << maximumUnobscuredSize;
#endif
#if PLATFORM(COCOA)
    encoder << smartInsertDeleteEnabled;
    encoder << additionalSupportedImageTypes;
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
#if ENABLE(SERVICE_WORKER)
    encoder << hasRegisteredServiceWorkers;
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
    if (!decoder.decodeEnum(parameters.drawingAreaType))
        return std::nullopt;
    std::optional<WebPageGroupData> pageGroupData;
    decoder >> pageGroupData;
    if (!pageGroupData)
        return std::nullopt;
    parameters.pageGroupData = WTFMove(*pageGroupData);
    if (!decoder.decode(parameters.drawsBackground))
        return std::nullopt;
    if (!decoder.decode(parameters.isEditable))
        return std::nullopt;
    if (!decoder.decode(parameters.underlayColor))
        return std::nullopt;
    if (!decoder.decode(parameters.useFixedLayout))
        return std::nullopt;
    if (!decoder.decode(parameters.fixedLayoutSize))
        return std::nullopt;
    if (!decoder.decode(parameters.alwaysShowsHorizontalScroller))
        return std::nullopt;
    if (!decoder.decode(parameters.alwaysShowsVerticalScroller))
        return std::nullopt;
    if (!decoder.decodeEnum(parameters.paginationMode))
        return std::nullopt;
    if (!decoder.decode(parameters.paginationBehavesLikeColumns))
        return std::nullopt;
    if (!decoder.decode(parameters.pageLength))
        return std::nullopt;
    if (!decoder.decode(parameters.gapBetweenPages))
        return std::nullopt;
    if (!decoder.decode(parameters.paginationLineGridEnabled))
        return std::nullopt;

    std::optional<String> userAgent;
    decoder >> userAgent;
    if (!userAgent)
        return std::nullopt;
    parameters.userAgent = WTFMove(*userAgent);

    std::optional<Vector<BackForwardListItemState>> itemStates;
    decoder >> itemStates;
    if (!itemStates)
        return std::nullopt;
    parameters.itemStates = WTFMove(*itemStates);

    if (!decoder.decode(parameters.sessionID))
        return std::nullopt;

    std::optional<uint64_t> userContentControllerIdentifier;
    decoder >> userContentControllerIdentifier;
    if (!userContentControllerIdentifier)
        return std::nullopt;
    parameters.userContentControllerID = makeObjectIdentifier<UserContentControllerIdentifierType>(*userContentControllerIdentifier);

    if (!decoder.decode(parameters.visitedLinkTableID))
        return std::nullopt;
    if (!decoder.decode(parameters.websiteDataStoreID))
        return std::nullopt;
    if (!decoder.decode(parameters.canRunBeforeUnloadConfirmPanel))
        return std::nullopt;
    if (!decoder.decode(parameters.canRunModal))
        return std::nullopt;
    if (!decoder.decode(parameters.deviceScaleFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.viewScaleFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.topContentInset))
        return std::nullopt;
    if (!decoder.decode(parameters.mediaVolume))
        return std::nullopt;
    if (!decoder.decode(parameters.muted))
        return std::nullopt;
    if (!decoder.decode(parameters.mayStartMediaWhenInWindow))
        return std::nullopt;
    if (!decoder.decode(parameters.viewLayoutSize))
        return std::nullopt;
    if (!decoder.decode(parameters.autoSizingShouldExpandToViewHeight))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportSizeForCSSViewportUnits))
        return std::nullopt;
    if (!decoder.decodeEnum(parameters.scrollPinningBehavior))
        return std::nullopt;

    std::optional<std::optional<uint32_t>> scrollbarOverlayStyle;
    decoder >> scrollbarOverlayStyle;
    if (!scrollbarOverlayStyle)
        return std::nullopt;
    parameters.scrollbarOverlayStyle = WTFMove(*scrollbarOverlayStyle);

    if (!decoder.decode(parameters.backgroundExtendsBeyondPage))
        return std::nullopt;
    if (!decoder.decodeEnum(parameters.layerHostingMode))
        return std::nullopt;
    if (!decoder.decode(parameters.mimeTypesWithCustomContentProviders))
        return std::nullopt;
    if (!decoder.decode(parameters.controlledByAutomation))
        return std::nullopt;

#if PLATFORM(MAC)
    if (!decoder.decode(parameters.colorSpace))
        return std::nullopt;
    if (!decoder.decode(parameters.useSystemAppearance))
        return std::nullopt;
    if (!decoder.decode(parameters.useDarkAppearance))
        return std::nullopt;
    if (!decoder.decode(parameters.shouldDelayAttachingDrawingArea))
        return std::nullopt;
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
    if (!decoder.decode(parameters.ignoresViewportScaleLimits))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationViewLayoutSize))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationLayoutSizeScaleFactor))
        return std::nullopt;
    if (!decoder.decode(parameters.viewportConfigurationViewSize))
        return std::nullopt;
    if (!decoder.decode(parameters.maximumUnobscuredSize))
        return std::nullopt;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.smartInsertDeleteEnabled))
        return std::nullopt;
    if (!decoder.decode(parameters.additionalSupportedImageTypes))
        return std::nullopt;
#endif

    if (!decoder.decode(parameters.appleMailPaginationQuirkEnabled))
        return std::nullopt;

    if (!decoder.decode(parameters.appleMailLinesClampEnabled))
        return std::nullopt;

    if (!decoder.decode(parameters.shouldScaleViewToFitDocument))
        return std::nullopt;

    if (!decoder.decodeEnum(parameters.userInterfaceLayoutDirection))
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

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<std::optional<WebCore::ApplicationManifest>> applicationManifest;
    decoder >> applicationManifest;
    if (!applicationManifest)
        return std::nullopt;
    parameters.applicationManifest = WTFMove(*applicationManifest);
#endif
#if ENABLE(SERVICE_WORKER)
    if (!decoder.decode(parameters.hasRegisteredServiceWorkers))
        return std::nullopt;
#endif

    if (!decoder.decode(parameters.needsFontAttributes))
        return std::nullopt;

    if (!decoder.decode(parameters.iceCandidateFilteringEnabled))
        return std::nullopt;

    if (!decoder.decode(parameters.enumeratingAllNetworkInterfacesEnabled))
        return std::nullopt;

    std::optional<Vector<std::pair<uint64_t, String>>> userContentWorlds;
    decoder >> userContentWorlds;
    if (!userContentWorlds)
        return std::nullopt;
    parameters.userContentWorlds = WTFMove(*userContentWorlds);

    std::optional<Vector<WebUserScriptData>> userScripts;
    decoder >> userScripts;
    if (!userScripts)
        return std::nullopt;
    parameters.userScripts = WTFMove(*userScripts);
    
    std::optional<Vector<WebUserStyleSheetData>> userStyleSheets;
    decoder >> userStyleSheets;
    if (!userStyleSheets)
        return std::nullopt;
    parameters.userStyleSheets = WTFMove(*userStyleSheets);
    
    std::optional<Vector<WebScriptMessageHandlerData>> messageHandlers;
    decoder >> messageHandlers;
    if (!messageHandlers)
        return std::nullopt;
    parameters.messageHandlers = WTFMove(*messageHandlers);
    
#if ENABLE(CONTENT_EXTENSIONS)
    std::optional<Vector<std::pair<String, WebCompiledContentRuleListData>>> contentRuleLists;
    decoder >> contentRuleLists;
    if (!contentRuleLists)
        return std::nullopt;
    parameters.contentRuleLists = WTFMove(*contentRuleLists);
#endif

    return WTFMove(parameters);
}

} // namespace WebKit
