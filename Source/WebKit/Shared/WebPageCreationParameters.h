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

#include "DrawingAreaInfo.h"
#include "LayerTreeContext.h"
#include "SandboxExtension.h"
#include "SessionState.h"
#include "UserContentControllerParameters.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageGroupData.h"
#include "WebPageProxyIdentifier.h"
#include "WebPreferencesStore.h"
#include "WebURLSchemeHandlerIdentifier.h"
#include <WebCore/ActivityState.h>
#include <WebCore/Color.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/FloatSize.h>
#include <WebCore/HighlightVisibility.h>
#include <WebCore/IntSize.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/Pagination.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <WebCore/ViewportArguments.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

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
    static std::optional<WebPageCreationParameters> decode(IPC::Decoder&);

    WebCore::IntSize viewSize;

    OptionSet<WebCore::ActivityState::Flag> activityState;
    
    WebPreferencesStore store;
    DrawingAreaType drawingAreaType;
    DrawingAreaIdentifier drawingAreaIdentifier;
    WebPageProxyIdentifier webPageProxyIdentifier;
    WebPageGroupData pageGroupData;

    bool isEditable;

    WebCore::Color underlayColor;

    bool useFixedLayout;
    WebCore::IntSize fixedLayoutSize;

    std::optional<WebCore::FloatRect> viewExposedRect;

    bool alwaysShowsHorizontalScroller;
    bool alwaysShowsVerticalScroller;

    bool suppressScrollbarAnimations;

    WebCore::Pagination::Mode paginationMode;
    bool paginationBehavesLikeColumns;
    double pageLength;
    double gapBetweenPages;
    bool paginationLineGridEnabled;
    
    String userAgent;

    bool itemStatesWereRestoredByAPIRequest { false };
    Vector<BackForwardListItemState> itemStates;

    uint64_t visitedLinkTableID;
    bool canRunBeforeUnloadConfirmPanel;
    bool canRunModal;

    float deviceScaleFactor;
    float viewScaleFactor;

    double textZoomFactor { 1 };
    double pageZoomFactor { 1 };

    float topContentInset;
    
    float mediaVolume;
    WebCore::MediaProducerMutedStateFlags muted;
    bool openedByDOM { false };
    bool mayStartMediaWhenInWindow;
    bool mediaPlaybackIsSuspended { false };

    WebCore::IntSize minimumSizeForAutoLayout;
    WebCore::IntSize sizeToContentAutoSizeMaximumSize;
    bool autoSizingShouldExpandToViewHeight;
    std::optional<WebCore::FloatSize> viewportSizeForCSSViewportUnits;
    
    WebCore::ScrollPinningBehavior scrollPinningBehavior;

    // FIXME: This should be std::optional<WebCore::ScrollbarOverlayStyle>, but we would need to
    // correctly handle enums inside Optionals when encoding and decoding. 
    std::optional<uint32_t> scrollbarOverlayStyle;

    bool backgroundExtendsBeyondPage;

    LayerHostingMode layerHostingMode;

    bool hasResourceLoadClient { false };

    Vector<String> mimeTypesWithCustomContentProviders;

    bool controlledByAutomation;
    bool isProcessSwap { false };

    bool useDarkAppearance { false };
    bool useElevatedUserInterfaceLevel { false };

#if PLATFORM(MAC)
    std::optional<WebCore::DestinationColorSpace> colorSpace;
    bool useSystemAppearance;
#endif
#if ENABLE(META_VIEWPORT)
    bool ignoresViewportScaleLimits;
    WebCore::FloatSize viewportConfigurationViewLayoutSize;
    double viewportConfigurationLayoutSizeScaleFactor;
    double viewportConfigurationMinimumEffectiveDeviceWidth;
    WebCore::FloatSize viewportConfigurationViewSize;
    std::optional<WebCore::ViewportArguments> overrideViewportArguments;
#endif
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatSize screenSize;
    WebCore::FloatSize availableScreenSize;
    WebCore::FloatSize overrideScreenSize;
    float textAutosizingWidth;
    WebCore::FloatSize minimumUnobscuredSize;
    WebCore::FloatSize maximumUnobscuredSize;
    int32_t deviceOrientation { 0 };
    bool keyboardIsAttached { false };
    bool canShowWhileLocked { false };
    bool isCapturingScreen { false };
#endif
#if PLATFORM(COCOA)
    bool smartInsertDeleteEnabled;
    Vector<String> additionalSupportedImageTypes;
    Vector<SandboxExtension::Handle> gpuIOKitExtensionHandles;
    Vector<SandboxExtension::Handle> gpuMachExtensionHandles;
#endif
#if HAVE(STATIC_FONT_REGISTRY)
    std::optional<SandboxExtension::Handle> fontMachExtensionHandle;
#endif
#if HAVE(APP_ACCENT_COLORS)
    WebCore::Color accentColor;
#endif
#if USE(WPE_RENDERER)
    IPC::Attachment hostFileDescriptor;
#endif
#if PLATFORM(WIN)
    uint64_t nativeWindowHandle;
#endif
#if USE(GRAPHICS_LAYER_WC)
    bool usesOffscreenRendering { false };
#endif
    bool shouldScaleViewToFitDocument;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection;
    OptionSet<WebCore::LayoutMilestone> observedLayoutMilestones;

    String overrideContentSecurityPolicy;
    std::optional<double> cpuLimit;

    HashMap<String, WebURLSchemeHandlerIdentifier> urlSchemeHandlers;
    Vector<String> urlSchemesWithLegacyCustomProtocolHandlers;

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<WebCore::ApplicationManifest> applicationManifest;
#endif

    bool needsFontAttributes { false };

    // WebRTC members.
    bool iceCandidateFilteringEnabled { true };
    bool enumeratingAllNetworkInterfacesEnabled { false };

    UserContentControllerParameters userContentControllerParameters;

    std::optional<WebCore::Color> backgroundColor;

    std::optional<WebCore::PageIdentifier> oldPageID;

    String overriddenMediaType;
    Vector<String> corsDisablingPatterns;
    bool userScriptsShouldWaitUntilNotification { true };
    bool loadsSubresources { true };
    std::optional<HashSet<String>> allowedNetworkHosts;

    bool crossOriginAccessControlCheckEnabled { true };
    String processDisplayName;

    bool shouldCaptureAudioInUIProcess { false };
    bool shouldCaptureAudioInGPUProcess { false };
    bool shouldCaptureVideoInUIProcess { false };
    bool shouldCaptureVideoInGPUProcess { false };
    bool shouldCaptureDisplayInUIProcess { false };
    bool shouldCaptureDisplayInGPUProcess { false };
    bool shouldRenderCanvasInGPUProcess { false };
    bool shouldRenderDOMInGPUProcess { false };
    bool shouldPlayMediaInGPUProcess { false };
#if ENABLE(WEBGL)
    bool shouldRenderWebGLInGPUProcess { false };
#endif
    bool shouldEnableVP8Decoder { false };
    bool shouldEnableVP9Decoder { false };
    bool shouldEnableVP9SWDecoder { false };
#if ENABLE(APP_BOUND_DOMAINS)
    bool limitsNavigationsToAppBoundDomains { false };
#endif
    bool lastNavigationWasAppInitiated { true };
    bool canUseCredentialStorage { true };

    WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking { WebCore::ShouldRelaxThirdPartyCookieBlocking::No };
    
    bool httpsUpgradeEnabled { true };

#if PLATFORM(IOS)
    bool allowsDeprecatedSynchronousXMLHttpRequestDuringUnload { false };
#endif
    
#if ENABLE(APP_HIGHLIGHTS)
    WebCore::HighlightVisibility appHighlightsVisible { WebCore::HighlightVisibility::Hidden };
#endif

#if HAVE(TOUCH_BAR)
    bool requiresUserActionForEditingControlsManager { false };
#endif
};

} // namespace WebKit
