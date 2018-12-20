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

#include "DrawingAreaInfo.h"
#include "LayerTreeContext.h"
#include "SessionState.h"
#include "WebCompiledContentRuleListData.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageGroupData.h"
#include "WebPreferencesStore.h"
#include "WebUserContentControllerDataTypes.h"
#include <WebCore/ActivityState.h>
#include <WebCore/Color.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntSize.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/Pagination.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include "ColorSpaceData.h"
#endif

#if ENABLE(APPLICATION_MANIFEST)
#include <WebCore/ApplicationManifest.h>
#endif

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct WebPageCreationParameters {
    void encode(IPC::Encoder&) const;
    static Optional<WebPageCreationParameters> decode(IPC::Decoder&);

    WebCore::IntSize viewSize;

    OptionSet<WebCore::ActivityState::Flag> activityState;
    
    WebPreferencesStore store;
    DrawingAreaType drawingAreaType;
    WebPageGroupData pageGroupData;

    bool drawsBackground;
    bool isEditable;

    WebCore::Color underlayColor;

    bool useFixedLayout;
    WebCore::IntSize fixedLayoutSize;

    bool alwaysShowsHorizontalScroller;
    bool alwaysShowsVerticalScroller;

    bool suppressScrollbarAnimations;

    WebCore::Pagination::Mode paginationMode;
    bool paginationBehavesLikeColumns;
    double pageLength;
    double gapBetweenPages;
    bool paginationLineGridEnabled;
    
    String userAgent;

    Vector<BackForwardListItemState> itemStates;
    PAL::SessionID sessionID;

    UserContentControllerIdentifier userContentControllerID;
    uint64_t visitedLinkTableID;
    uint64_t websiteDataStoreID;
    bool canRunBeforeUnloadConfirmPanel;
    bool canRunModal;

    float deviceScaleFactor;
    float viewScaleFactor;

    float topContentInset;
    
    float mediaVolume;
    WebCore::MediaProducer::MutedStateFlags muted;
    bool mayStartMediaWhenInWindow;

    WebCore::IntSize viewLayoutSize;
    bool autoSizingShouldExpandToViewHeight;
    Optional<WebCore::IntSize> viewportSizeForCSSViewportUnits;
    
    WebCore::ScrollPinningBehavior scrollPinningBehavior;

    // FIXME: This should be Optional<WebCore::ScrollbarOverlayStyle>, but we would need to
    // correctly handle enums inside Optionals when encoding and decoding. 
    Optional<uint32_t> scrollbarOverlayStyle;

    bool backgroundExtendsBeyondPage;

    LayerHostingMode layerHostingMode;

    Vector<String> mimeTypesWithCustomContentProviders;

    bool controlledByAutomation;

#if PLATFORM(MAC)
    ColorSpaceData colorSpace;
    bool useSystemAppearance;
    bool useDarkAppearance;
    bool shouldDelayAttachingDrawingArea { false };
#endif
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatSize screenSize;
    WebCore::FloatSize availableScreenSize;
    WebCore::FloatSize overrideScreenSize;
    float textAutosizingWidth;
    bool ignoresViewportScaleLimits;
    WebCore::FloatSize viewportConfigurationViewLayoutSize;
    double viewportConfigurationLayoutSizeScaleFactor;
    WebCore::FloatSize viewportConfigurationViewSize;
    WebCore::FloatSize maximumUnobscuredSize;
    int32_t deviceOrientation { 0 };
#endif
#if PLATFORM(COCOA)
    bool smartInsertDeleteEnabled;
    Vector<String> additionalSupportedImageTypes;
#endif
    bool appleMailPaginationQuirkEnabled;
    bool appleMailLinesClampEnabled;
    bool shouldScaleViewToFitDocument;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection;
    OptionSet<WebCore::LayoutMilestone> observedLayoutMilestones;

    String overrideContentSecurityPolicy;
    Optional<double> cpuLimit;

    HashMap<String, uint64_t> urlSchemeHandlers;

#if ENABLE(APPLICATION_MANIFEST)
    Optional<WebCore::ApplicationManifest> applicationManifest;
#endif

#if ENABLE(SERVICE_WORKER)
    bool hasRegisteredServiceWorkers { true };
#endif

    bool needsFontAttributes { false };

    // WebRTC members.
    bool iceCandidateFilteringEnabled { true };
    bool enumeratingAllNetworkInterfacesEnabled { false };

    // UserContentController members
    Vector<std::pair<uint64_t, String>> userContentWorlds;
    Vector<WebUserScriptData> userScripts;
    Vector<WebUserStyleSheetData> userStyleSheets;
    Vector<WebScriptMessageHandlerData> messageHandlers;
#if ENABLE(CONTENT_EXTENSIONS)
    Vector<std::pair<String, WebCompiledContentRuleListData>> contentRuleLists;
#endif
};

} // namespace WebKit
