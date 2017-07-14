/*
 * Copyright (C) 2010, 2011, 2015 Apple Inc. All rights reserved.
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
    encoder.encodeEnum(paginationMode);
    encoder << paginationBehavesLikeColumns;
    encoder << pageLength;
    encoder << gapBetweenPages;
    encoder << paginationLineGridEnabled;
    encoder << userAgent;
    encoder << itemStates;
    encoder << sessionID;
    encoder << highestUsedBackForwardItemID;
    encoder << userContentControllerID;
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
    encoder << minimumLayoutSize;
    encoder << autoSizingShouldExpandToViewHeight;
    encoder << viewportSizeForCSSViewportUnits;
    encoder.encodeEnum(scrollPinningBehavior);
    encoder << scrollbarOverlayStyle;
    encoder << backgroundExtendsBeyondPage;
    encoder.encodeEnum(layerHostingMode);
    encoder << mimeTypesWithCustomContentProviders;
    encoder << controlledByAutomation;

#if ENABLE(REMOTE_INSPECTOR)
    encoder << allowsRemoteInspection;
    encoder << remoteInspectionNameOverride;
#endif
#if PLATFORM(MAC)
    encoder << colorSpace;
#endif
#if PLATFORM(IOS)
    encoder << screenSize;
    encoder << availableScreenSize;
    encoder << textAutosizingWidth;
    encoder << ignoresViewportScaleLimits;
    encoder << allowsBlockSelection;
#endif
#if PLATFORM(COCOA)
    encoder << smartInsertDeleteEnabled;
#endif
    encoder << appleMailPaginationQuirkEnabled;
    encoder << shouldScaleViewToFitDocument;
    encoder.encodeEnum(userInterfaceLayoutDirection);
    encoder.encodeEnum(observedLayoutMilestones);
    encoder << overrideContentSecurityPolicy;
    encoder << cpuLimit;
    encoder << urlSchemeHandlers;
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

bool WebPageCreationParameters::decode(IPC::Decoder& decoder, WebPageCreationParameters& parameters)
{
    if (!decoder.decode(parameters.viewSize))
        return false;
    if (!decoder.decode(parameters.activityState))
        return false;
    if (!decoder.decode(parameters.store))
        return false;
    if (!decoder.decodeEnum(parameters.drawingAreaType))
        return false;
    if (!decoder.decode(parameters.pageGroupData))
        return false;
    if (!decoder.decode(parameters.drawsBackground))
        return false;
    if (!decoder.decode(parameters.isEditable))
        return false;
    if (!decoder.decode(parameters.underlayColor))
        return false;
    if (!decoder.decode(parameters.useFixedLayout))
        return false;
    if (!decoder.decode(parameters.fixedLayoutSize))
        return false;
    if (!decoder.decodeEnum(parameters.paginationMode))
        return false;
    if (!decoder.decode(parameters.paginationBehavesLikeColumns))
        return false;
    if (!decoder.decode(parameters.pageLength))
        return false;
    if (!decoder.decode(parameters.gapBetweenPages))
        return false;
    if (!decoder.decode(parameters.paginationLineGridEnabled))
        return false;
    if (!decoder.decode(parameters.userAgent))
        return false;
    if (!decoder.decode(parameters.itemStates))
        return false;
    if (!decoder.decode(parameters.sessionID))
        return false;
    if (!decoder.decode(parameters.highestUsedBackForwardItemID))
        return false;
    if (!decoder.decode(parameters.userContentControllerID))
        return false;
    if (!decoder.decode(parameters.visitedLinkTableID))
        return false;
    if (!decoder.decode(parameters.websiteDataStoreID))
        return false;
    if (!decoder.decode(parameters.canRunBeforeUnloadConfirmPanel))
        return false;
    if (!decoder.decode(parameters.canRunModal))
        return false;
    if (!decoder.decode(parameters.deviceScaleFactor))
        return false;
    if (!decoder.decode(parameters.viewScaleFactor))
        return false;
    if (!decoder.decode(parameters.topContentInset))
        return false;
    if (!decoder.decode(parameters.mediaVolume))
        return false;
    if (!decoder.decode(parameters.muted))
        return false;
    if (!decoder.decode(parameters.mayStartMediaWhenInWindow))
        return false;
    if (!decoder.decode(parameters.minimumLayoutSize))
        return false;
    if (!decoder.decode(parameters.autoSizingShouldExpandToViewHeight))
        return false;
    if (!decoder.decode(parameters.viewportSizeForCSSViewportUnits))
        return false;
    if (!decoder.decodeEnum(parameters.scrollPinningBehavior))
        return false;
    if (!decoder.decode(parameters.scrollbarOverlayStyle))
        return false;
    if (!decoder.decode(parameters.backgroundExtendsBeyondPage))
        return false;
    if (!decoder.decodeEnum(parameters.layerHostingMode))
        return false;
    if (!decoder.decode(parameters.mimeTypesWithCustomContentProviders))
        return false;
    if (!decoder.decode(parameters.controlledByAutomation))
        return false;

#if ENABLE(REMOTE_INSPECTOR)
    if (!decoder.decode(parameters.allowsRemoteInspection))
        return false;
    if (!decoder.decode(parameters.remoteInspectionNameOverride))
        return false;
#endif

#if PLATFORM(MAC)
    if (!decoder.decode(parameters.colorSpace))
        return false;
#endif

#if PLATFORM(IOS)
    if (!decoder.decode(parameters.screenSize))
        return false;
    if (!decoder.decode(parameters.availableScreenSize))
        return false;
    if (!decoder.decode(parameters.textAutosizingWidth))
        return false;
    if (!decoder.decode(parameters.ignoresViewportScaleLimits))
        return false;
    if (!decoder.decode(parameters.allowsBlockSelection))
        return false;
#endif

#if PLATFORM(COCOA)
    if (!decoder.decode(parameters.smartInsertDeleteEnabled))
        return false;
#endif

    if (!decoder.decode(parameters.appleMailPaginationQuirkEnabled))
        return false;

    if (!decoder.decode(parameters.shouldScaleViewToFitDocument))
        return false;

    if (!decoder.decodeEnum(parameters.userInterfaceLayoutDirection))
        return false;
    if (!decoder.decodeEnum(parameters.observedLayoutMilestones))
        return false;

    if (!decoder.decode(parameters.overrideContentSecurityPolicy))
        return false;

    if (!decoder.decode(parameters.cpuLimit))
        return false;

    if (!decoder.decode(parameters.urlSchemeHandlers))
        return false;

    if (!decoder.decode(parameters.iceCandidateFilteringEnabled))
        return false;

    if (!decoder.decode(parameters.enumeratingAllNetworkInterfacesEnabled))
        return false;

    if (!decoder.decode(parameters.userContentWorlds))
        return false;
    if (!decoder.decode(parameters.userScripts))
        return false;
    if (!decoder.decode(parameters.userStyleSheets))
        return false;
    if (!decoder.decode(parameters.messageHandlers))
        return false;
#if ENABLE(CONTENT_EXTENSIONS)
    if (!decoder.decode(parameters.contentRuleLists))
        return false;
#endif
    return true;
}

} // namespace WebKit
