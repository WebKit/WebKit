/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "InjectedBundlePageFullScreenClient.h"
#include "WebPage.h"
#include <WebCore/AppHighlight.h>
#include <WebCore/TextManipulationController.h>
#include <WebCore/UserActivity.h>

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "InputMethodState.h"
#endif

#if ENABLE(META_VIEWPORT)
#include <WebCore/ViewportConfiguration.h>
#endif

namespace WebKit {

using DynamicViewportSizeUpdateID = uint64_t;

struct WebPage::Internals {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    WebCore::PageIdentifier identifier;

    WebCore::IntSize viewSize;
    HashMap<TextCheckerRequestID, RefPtr<WebCore::TextCheckingRequest>> pendingTextCheckingRequestMap;

    WebCore::FloatSize defaultUnobscuredSize;
    WebCore::FloatSize minimumUnobscuredSize;
    WebCore::FloatSize maximumUnobscuredSize;

    WebCore::Color underlayColor;

#if ENABLE(PDFKIT_PLUGIN)
    HashMap<PDFPluginIdentifier, WeakPtr<PDFPlugin>> pdfPlugInsWithHUD;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    WebCore::CreateNewGroupForHighlight highlightIsNewGroup { WebCore::CreateNewGroupForHighlight::No };
    WebCore::HighlightRequestOriginatedInApp highlightRequestOriginatedInApp { WebCore::HighlightRequestOriginatedInApp::No };
#endif

#if PLATFORM(COCOA)
    WebCore::FloatRect windowFrameInScreenCoordinates;
    WebCore::FloatRect windowFrameInUnflippedScreenCoordinates;
    WebCore::FloatRect viewFrameInWindowCoordinates;
    WebCore::FloatPoint accessibilityPosition;
#endif

    RunLoop::Timer setCanStartMediaTimer;

    HashMap<WebUndoStepID, RefPtr<WebUndoStep>> undoStepMap;

#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient fullScreenClient;
#endif

#if PLATFORM(IOS_FAMILY)
    std::optional<WebCore::SimpleRange> startingGestureRange;
#endif

    PAL::HysteresisActivity pageScrolledHysteresis;

#if ENABLE(DRAG_SUPPORT)
    OptionSet<WebCore::DragSourceAction> allowedDragSourceActions;
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    HashSet<RefPtr<WebCore::HTMLImageElement>> pendingImageElementsForDropSnapshot;
    std::optional<WebCore::SimpleRange> rangeForDropSnapshot;
#endif

    WebCore::RectEdges<bool> cachedMainFramePinnedState { true, true, true, true };

    HashSet<WebCore::ResourceLoaderIdentifier> trackedNetworkResourceRequestIdentifiers;

    WebCore::IntSize minimumSizeForAutoLayout;
    WebCore::IntSize sizeToContentAutoSizeMaximumSize;
    std::optional<WebCore::FloatSize> viewportSizeForCSSViewportUnits;

    OptionSet<WebCore::ActivityState> lastActivityStateChanges;

#if ENABLE(META_VIEWPORT)
    WebCore::ViewportConfiguration viewportConfiguration;
#endif

#if PLATFORM(IOS_FAMILY)
    std::optional<WebCore::SimpleRange> currentWordRange;
    WebCore::IntPoint lastInteractionLocation;

    WebCore::FloatPoint potentialTapLocation;
    MonotonicTime oldestNonStableUpdateVisibleContentRectsTimestamp;
    WebCore::FloatSize screenSize;
    WebCore::FloatSize availableScreenSize;
    WebCore::FloatSize overrideScreenSize;

    std::optional<WebCore::SimpleRange> initialSelection;
    WebCore::VisibleSelection storedSelectionForAccessibility;
    HashMap<std::pair<WebCore::IntSize, double>, WebCore::IntPoint> dynamicSizeUpdateHistory;
    WebCore::FloatPoint pendingSyntheticClickLocation;
    WebCore::FloatRect previousExposedContentRect;
    OptionSet<WebEventModifier> pendingSyntheticClickModifiers;
    FocusedElementInformationIdentifier lastFocusedElementInformationIdentifier;
    std::optional<DynamicViewportSizeUpdateID> pendingDynamicViewportSizeUpdateID;
    TransactionID lastTransactionIDWithScaleChange;

    CompletionHandler<void(InteractionInformationAtPosition&&)> pendingSynchronousPositionInformationReply;
    std::optional<std::pair<TransactionID, double>> lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage;
#endif // PLATFORM(IOS_FAMILY)

    WebCore::Timer layerVolatilityTimer;
    std::optional<WebCore::Color> backgroundColor { WebCore::Color::white };

    OptionSet<WebCore::ActivityState> activityState;

    UserActivity userActivity { "App nap disabled for page due to user activity"_s };

    std::optional<WebsitePoliciesData> pendingWebsitePolicies;

    mutable EditorStateIdentifier lastEditorStateIdentifier;

#if PLATFORM(GTK) || PLATFORM(WPE)
    std::optional<InputMethodState> inputMethodState;
#endif

#if USE(WPE_RENDERER)
    UnixFileDescriptor hostFileDescriptor;
#endif

    HashMap<WebURLSchemeHandlerIdentifier, WebURLSchemeHandlerProxy*> identifierToURLSchemeHandlerProxyMap;

    OptionSet<LayerTreeFreezeReason> layerTreeFreezeReasons;
    WebPageProxyIdentifier webPageProxyIdentifier;
    std::optional<WebCore::IntSize> pendingIntrinsicContentSize;
    WebCore::IntSize lastSentIntrinsicContentSize;
#if ENABLE(TEXT_AUTOSIZING)
    WebCore::Timer textAutoSizingAdjustmentTimer;
#endif
#if ENABLE(TRACKING_PREVENTION)
    HashMap<WebCore::RegistrableDomain, WebCore::RegistrableDomain> domainsWithPageLevelStorageAccess;
    HashSet<WebCore::RegistrableDomain> loadedSubresourceDomains;
#endif

    AtomString overriddenMediaType;
    std::optional<Vector<WebCore::TextManipulationController::ExclusionRule>> textManipulationExclusionRules;

#if ENABLE(NETWORK_CONNECTION_INTEGRITY)
    HashSet<String> lookalikeCharacterStrings;
    HashMap<WebCore::RegistrableDomain, HashSet<String>> domainScopedLookalikeCharacterStrings;
    HashMap<WebCore::RegistrableDomain, HashSet<String>> allowedLookalikeCharacterStrings;
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    WeakHashSet<WebCore::HTMLImageElement, WebCore::WeakPtrImplWithEventTargetData> elementsToExcludeFromRemoveBackground;
#endif

    Internals(WebPage&, WebCore::PageIdentifier, WebPageCreationParameters&&);
};

} // namespace WebKit
