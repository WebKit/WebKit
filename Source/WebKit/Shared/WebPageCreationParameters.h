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
#include "FrameTreeCreationParameters.h"
#include "LayerTreeContext.h"
#include "SandboxExtension.h"
#include "SessionState.h"
#include "UserContentControllerParameters.h"
#include "ViewWindowCoordinates.h"
#include "VisitedLinkTableIdentifier.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageGroupData.h"
#include "WebPageProxyIdentifier.h"
#include "WebPreferencesStore.h"
#include "WebURLSchemeHandlerIdentifier.h"
#include "WebsitePoliciesData.h"
#include <WebCore/ActivityState.h>
#include <WebCore/Color.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/FloatSize.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/HighlightVisibility.h>
#include <WebCore/IntDegrees.h>
#include <WebCore/IntSize.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/Pagination.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/WTFString.h>

#if ENABLE(APPLICATION_MANIFEST)
#include <WebCore/ApplicationManifest.h>
#endif

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
#include <WebCore/LinkDecorationFilteringData.h>
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionControllerParameters.h"
#endif

#if (PLATFORM(GTK) || PLATFORM(WPE)) && USE(GBM)
#include "DMABufRendererBufferFormat.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "HardwareKeyboardState.h"
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
#include <WebCore/ShouldRequireExplicitConsentForGamepadAccess.h>
#endif

namespace WebCore {
enum class SandboxFlag : uint16_t;
using SandboxFlags = OptionSet<SandboxFlag>;
}

namespace WebKit {

struct RemotePageParameters {
    URL initialMainDocumentURL;
    FrameTreeCreationParameters frameTreeParameters;
    std::optional<WebsitePoliciesData> websitePoliciesData;
};

struct WebPageCreationParameters {
    WebCore::IntSize viewSize { };

    OptionSet<WebCore::ActivityState> activityState { };
    
    WebPreferencesStore store { };
    DrawingAreaType drawingAreaType { };
    DrawingAreaIdentifier drawingAreaIdentifier;
    WebPageProxyIdentifier webPageProxyIdentifier;
    WebPageGroupData pageGroupData;

    bool isEditable { false };

    WebCore::Color underlayColor { };

    bool useFixedLayout { false };
    WebCore::IntSize fixedLayoutSize { };

    WebCore::FloatSize defaultUnobscuredSize { };
    WebCore::FloatSize minimumUnobscuredSize { };
    WebCore::FloatSize maximumUnobscuredSize { };

    std::optional<WebCore::FloatRect> viewExposedRect { };

    std::optional<uint32_t> displayID { };
    std::optional<unsigned> nominalFramesPerSecond { };

    bool alwaysShowsHorizontalScroller { false };
    bool alwaysShowsVerticalScroller { false };

    bool suppressScrollbarAnimations { false };

    WebCore::Pagination::Mode paginationMode { WebCore::Pagination::Mode::Unpaginated };
    bool paginationBehavesLikeColumns { false };
    double pageLength { 0 };
    double gapBetweenPages { 0 };
    
    String userAgent { };

    VisitedLinkTableIdentifier visitedLinkTableID;
    bool canRunBeforeUnloadConfirmPanel { false };
    bool canRunModal { false };

    float deviceScaleFactor { 0 };
    float viewScaleFactor { 0 };

    double textZoomFactor { 1 };
    double pageZoomFactor { 1 };

    float topContentInset { 0 };
    
    float mediaVolume { 0 };
    WebCore::MediaProducerMutedStateFlags muted { };
    bool openedByDOM { false };
    bool mayStartMediaWhenInWindow { false };
    bool mediaPlaybackIsSuspended { false };

    WebCore::IntSize minimumSizeForAutoLayout { };
    WebCore::IntSize sizeToContentAutoSizeMaximumSize { };
    bool autoSizingShouldExpandToViewHeight { false };
    std::optional<WebCore::FloatSize> viewportSizeForCSSViewportUnits { };
    
    WebCore::ScrollPinningBehavior scrollPinningBehavior { WebCore::ScrollPinningBehavior::DoNotPin };

    // FIXME: This should be std::optional<WebCore::ScrollbarOverlayStyle>, but we would need to
    // correctly handle enums inside Optionals when encoding and decoding. 
    std::optional<uint32_t> scrollbarOverlayStyle { };

    bool backgroundExtendsBeyondPage { false };

    LayerHostingMode layerHostingMode { LayerHostingMode::InProcess };

    bool hasResourceLoadClient { false };

    Vector<String> mimeTypesWithCustomContentProviders { };

    bool controlledByAutomation { false };
    bool isProcessSwap { false };

    bool useDarkAppearance { false };
    bool useElevatedUserInterfaceLevel { false };

#if PLATFORM(MAC)
    std::optional<WebCore::DestinationColorSpace> colorSpace { };
    bool useSystemAppearance { false };
    bool useFormSemanticContext { false };
    int headerBannerHeight { 0 };
    int footerBannerHeight { 0 };
    std::optional<ViewWindowCoordinates> viewWindowCoordinates { };
#endif
#if ENABLE(META_VIEWPORT)
    bool ignoresViewportScaleLimits { false };
    WebCore::FloatSize viewportConfigurationViewLayoutSize { };
    double viewportConfigurationLayoutSizeScaleFactorFromClient { 0 };
    double viewportConfigurationMinimumEffectiveDeviceWidth { 0 };
    WebCore::FloatSize viewportConfigurationViewSize { };
    std::optional<WebCore::ViewportArguments> overrideViewportArguments { };
#endif
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatSize screenSize { };
    WebCore::FloatSize availableScreenSize { };
    WebCore::FloatSize overrideScreenSize { };
    WebCore::FloatSize overrideAvailableScreenSize { };
    float textAutosizingWidth { 0 };
    WebCore::IntDegrees deviceOrientation { 0 };
    HardwareKeyboardState hardwareKeyboardState { };
    bool canShowWhileLocked { false };
    bool isCapturingScreen { false };
    WebCore::Color insertionPointColor { };
#endif
#if PLATFORM(COCOA)
    bool smartInsertDeleteEnabled { false };
    Vector<String> additionalSupportedImageTypes { };
    Vector<SandboxExtension::Handle> gpuIOKitExtensionHandles { };
    Vector<SandboxExtension::Handle> gpuMachExtensionHandles { };
#endif
#if PLATFORM(MAC)
    SandboxExtension::Handle renderServerMachExtensionHandle { };
#endif
#if HAVE(STATIC_FONT_REGISTRY)
    Vector<SandboxExtension::Handle> fontMachExtensionHandles { };
#endif
#if HAVE(HOSTED_CORE_ANIMATION)
    WTF::MachSendRight acceleratedCompositingPort { };
#endif
#if HAVE(APP_ACCENT_COLORS)
    WebCore::Color accentColor { };
#if PLATFORM(MAC)
    bool appUsesCustomAccentColor { false };
#endif
#endif
#if USE(WPE_RENDERER)
    UnixFileDescriptor hostFileDescriptor { };
#endif
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    uint64_t nativeWindowHandle { 0 };
#endif
#if USE(GRAPHICS_LAYER_WC)
    bool usesOffscreenRendering { false };
#endif
    bool shouldScaleViewToFitDocument { false };

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection { WebCore::UserInterfaceLayoutDirection::LTR };
    OptionSet<WebCore::LayoutMilestone> observedLayoutMilestones { };

    String overrideContentSecurityPolicy { };
    std::optional<double> cpuLimit { };

    HashMap<String, WebURLSchemeHandlerIdentifier> urlSchemeHandlers { };
    Vector<String> urlSchemesWithLegacyCustomProtocolHandlers { };

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<WebCore::ApplicationManifest> applicationManifest { };
#endif

    bool needsFontAttributes { false };

    // WebRTC members.
    bool iceCandidateFilteringEnabled { true };
    bool enumeratingAllNetworkInterfacesEnabled { false };

    UserContentControllerParameters userContentControllerParameters;

#if ENABLE(WK_WEB_EXTENSIONS)
    std::optional<WebExtensionControllerParameters> webExtensionControllerParameters { };
#endif

    std::optional<WebCore::Color> backgroundColor { };

    std::optional<WebCore::PageIdentifier> oldPageID { };

    String overriddenMediaType { };
    Vector<String> corsDisablingPatterns { };
    HashSet<String> maskedURLSchemes { };
    bool userScriptsShouldWaitUntilNotification { true };
    bool loadsSubresources { true };
    std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<String>> allowedNetworkHosts { };
    std::optional<std::pair<uint16_t, uint16_t>> portsForUpgradingInsecureSchemeForTesting { };

    bool crossOriginAccessControlCheckEnabled { true };
    String processDisplayName { };

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
#if ENABLE(APP_BOUND_DOMAINS)
    bool limitsNavigationsToAppBoundDomains { false };
#endif
    bool lastNavigationWasAppInitiated { true };
    bool canUseCredentialStorage { true };

    WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking { WebCore::ShouldRelaxThirdPartyCookieBlocking::No };
    
    bool httpsUpgradeEnabled { true };

#if PLATFORM(IOS) || PLATFORM(VISION)
    bool allowsDeprecatedSynchronousXMLHttpRequestDuringUnload { false };
#endif
    
#if ENABLE(APP_HIGHLIGHTS)
    WebCore::HighlightVisibility appHighlightsVisible { WebCore::HighlightVisibility::Hidden };
#endif

#if HAVE(TOUCH_BAR)
    bool requiresUserActionForEditingControlsManager { false };
#endif

    bool hasResizableWindows { false };

    WebCore::ContentSecurityPolicyModeForExtension contentSecurityPolicyModeForExtension { WebCore::ContentSecurityPolicyModeForExtension::None };

    std::optional<RemotePageParameters> remotePageParameters { };
    WebCore::FrameIdentifier mainFrameIdentifier;
    String openedMainFrameName;
    std::optional<WebCore::FrameIdentifier> mainFrameOpenerIdentifier { };
    WebCore::SandboxFlags initialSandboxFlags;
    std::optional<WebCore::WindowFeatures> windowFeatures { };

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    Vector<WebCore::LinkDecorationFilteringData> linkDecorationFilteringData { };
    Vector<WebCore::LinkDecorationFilteringData> allowedQueryParametersForAdvancedPrivacyProtections { };
#endif

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
    SandboxExtension::Handle machBootstrapHandle { };
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#if USE(GBM)
    Vector<DMABufRendererBufferFormat> preferredBufferFormats { };
#endif
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    WebCore::ShouldRequireExplicitConsentForGamepadAccess gamepadAccessRequiresExplicitConsent { WebCore::ShouldRequireExplicitConsentForGamepadAccess::No };
#endif
};

} // namespace WebKit
