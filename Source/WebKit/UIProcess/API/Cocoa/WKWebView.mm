/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebViewInternal.h"

#import "APIDataTask.h"
#import "APIFormClient.h"
#import "APIFrameTreeNode.h"
#import "APIPageConfiguration.h"
#import "APISecurityOrigin.h"
#import "APISerializedScriptValue.h"
#import "BrowsingWarning.h"
#import "CocoaImage.h"
#import "CompletionHandlerCallChecker.h"
#import "ContentAsStringIncludesChildFrames.h"
#import "DefaultWebBrowserChecks.h"
#import "DiagnosticLoggingClient.h"
#import "FindClient.h"
#import "FullscreenClient.h"
#import "GlobalFindInPageState.h"
#import "IconLoadingDelegate.h"
#import "ImageAnalysisUtilities.h"
#import "LegacySessionStateCoding.h"
#import "Logging.h"
#import "MediaPlaybackState.h"
#import "MediaUtilities.h"
#import "NavigationState.h"
#import "PageClient.h"
#import "PlatformWritingToolsUtilities.h"
#import "ProvisionalPageProxy.h"
#import "QuickLookThumbnailLoader.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "ResourceLoadDelegate.h"
#import "SessionStateCoding.h"
#import "UIDelegate.h"
#import "VideoPresentationManagerProxy.h"
#import "ViewGestureController.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKContentWorldInternal.h"
#import "WKDataDetectorTypesInternal.h"
#import "WKDownloadInternal.h"
#import "WKErrorInternal.h"
#import "WKFindConfiguration.h"
#import "WKFindResultInternal.h"
#import "WKFrameInfoPrivate.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKLayoutMode.h"
#import "WKNSData.h"
#import "WKNSURLExtras.h"
#import "WKNavigationDelegate.h"
#import "WKNavigationInternal.h"
#import "WKPDFConfiguration.h"
#import "WKPDFView.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKSharedAPICast.h"
#import "WKSnapshotConfigurationPrivate.h"
#import "WKTextAnimationType.h"
#import "WKTextExtractionItem.h"
#import "WKTextExtractionUtilities.h"
#import "WKUIDelegate.h"
#import "WKUIDelegatePrivate.h"
#import "WKUserContentControllerInternal.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProvider.h"
#import "WKWebViewMac.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebBackForwardCache.h"
#import "WebBackForwardList.h"
#import "WebFrameProxy.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageGroup.h"
#import "WebPageInspectorController.h"
#import "WebPageProxy.h"
#import "WebPageProxyTesting.h"
#import "WebPreferences.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebURLSchemeHandlerCocoa.h"
#import "WebViewImpl.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKAppHighlightDelegate.h"
#import "_WKAppHighlightInternal.h"
#import "_WKApplicationManifestInternal.h"
#import "_WKArchiveConfiguration.h"
#import "_WKArchiveExclusionRule.h"
#import "_WKDataTaskInternal.h"
#import "_WKDiagnosticLoggingDelegate.h"
#import "_WKFindDelegate.h"
#import "_WKFrameHandleInternal.h"
#import "_WKFrameTreeNodeInternal.h"
#import "_WKFullscreenDelegate.h"
#import "_WKHitTestResultInternal.h"
#import "_WKInputDelegate.h"
#import "_WKInspectorInternal.h"
#import "_WKPageLoadTimingInternal.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKSessionStateInternal.h"
#import "_WKTargetedElementInfoInternal.h"
#import "_WKTargetedElementRequestInternal.h"
#import "_WKTextInputContextInternal.h"
#import "_WKTextManipulationConfiguration.h"
#import "_WKTextManipulationDelegate.h"
#import "_WKTextManipulationExclusionRule.h"
#import "_WKTextManipulationItem.h"
#import "_WKTextManipulationToken.h"
#import "_WKTextPreview.h"
#import "_WKTextRunInternal.h"
#import "_WKVisitedLinkStoreInternal.h"
#import "_WKWarningView.h"
#import <WebCore/AppHighlight.h>
#import <WebCore/ArchiveError.h>
#import <WebCore/AttributedString.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/ContentExtensionsBackend.h>
#import <WebCore/DOMException.h>
#import <WebCore/ElementContext.h>
#import <WebCore/ExceptionCode.h>
#import <WebCore/ImageUtilities.h>
#import <WebCore/JSDOMBinding.h>
#import <WebCore/JSDOMExceptionHandling.h>
#import <WebCore/LegacySchemeRegistry.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MarkupExclusionRule.h>
#import <WebCore/Pagination.h>
#import <WebCore/Permissions.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/RunJavaScriptParameters.h>
#import <WebCore/Settings.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/StringUtilities.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/TextManipulationController.h>
#import <WebCore/TextManipulationItem.h>
#import <WebCore/ViewportArguments.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebViewVisualIdentificationOverlay.h>
#import <WebCore/WritingMode.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/HashMap.h>
#import <wtf/MathExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/SystemTracing.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/TextStream.h>

#if ENABLE(WK_WEB_EXTENSIONS)
#import "WKWebExtensionControllerInternal.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "PointerTouchCompatibilitySimulator.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import "WKScrollView.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewIOS.h"
#import "WKWebViewPrivateForTestingIOS.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIApplication.h>
#import <pal/spi/cf/CFNotificationCenterSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/cocoa/Entitlements.h>

#define WKWEBVIEW_RELEASE_LOG(...) RELEASE_LOG(ViewState, __VA_ARGS__)
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
#import "AppKitSPI.h"
#import "WKTextFinderClient.h"
#import "WKViewInternal.h"
#import <WebCore/ColorMac.h>
#endif

#import "WebKitSwiftSoftLink.h"

#if PLATFORM(WATCHOS)
static const BOOL defaultAllowsViewportShrinkToFit = YES;
static const BOOL defaultFastClickingEnabled = YES;
#elif PLATFORM(IOS_FAMILY)
static const BOOL defaultAllowsViewportShrinkToFit = NO;
static const BOOL defaultFastClickingEnabled = NO;
#endif

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
#import <wtf/SoftLinking.h>
SOFT_LINK_LIBRARY_OPTIONAL(libAccessibility)
SOFT_LINK_OPTIONAL(libAccessibility, _AXSReduceMotionAutoplayAnimatedImagesEnabled, Boolean, (), ());
#endif

#if ENABLE(GAMEPAD) && PLATFORM(VISION) && __has_include(<GameController/GCEventInteraction.h>)
#import <WebCore/GameControllerSoftLink.h>
#endif

#define THROW_IF_SUSPENDED if (UNLIKELY(_page && _page->isSuspended())) \
    [NSException raise:NSInternalInconsistencyException format:@"The WKWebView is suspended"]

RetainPtr<NSError> nsErrorFromExceptionDetails(const WebCore::ExceptionDetails& details)
{
    auto userInfo = adoptNS([[NSMutableDictionary alloc] init]);

    WKErrorCode errorCode;
    switch (details.type) {
    case WebCore::ExceptionDetails::Type::InvalidTargetFrame:
        errorCode = WKErrorJavaScriptInvalidFrameTarget;
        break;
    case WebCore::ExceptionDetails::Type::Script:
        errorCode = WKErrorJavaScriptExceptionOccurred;
        break;
    case WebCore::ExceptionDetails::Type::AppBoundDomain:
        errorCode = WKErrorJavaScriptAppBoundDomain;
        break;
    }

    [userInfo setObject:localizedDescriptionForErrorCode(errorCode) forKey:NSLocalizedDescriptionKey];
    [userInfo setObject:details.message forKey:_WKJavaScriptExceptionMessageErrorKey];
    [userInfo setObject:@(details.lineNumber) forKey:_WKJavaScriptExceptionLineNumberErrorKey];
    [userInfo setObject:@(details.columnNumber) forKey:_WKJavaScriptExceptionColumnNumberErrorKey];

    if (!details.sourceURL.isEmpty())
        [userInfo setObject:[NSURL _web_URLWithWTFString:details.sourceURL] forKey:_WKJavaScriptExceptionSourceURLErrorKey];

    return adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:errorCode userInfo:userInfo.get()]);
}

@implementation WKWebView

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (instancetype)initWithFrame:(CGRect)frame
{
    return [self initWithFrame:frame configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()];
}

- (BOOL)_isValid
{
    return _page && _page->hasRunningProcess();
}

#if PLATFORM(IOS_FAMILY)

static bool shouldAllowPictureInPictureMediaPlayback()
{
    static bool shouldAllowPictureInPictureMediaPlayback = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PictureInPictureMediaPlayback);
    return shouldAllowPictureInPictureMediaPlayback;
}

static bool shouldAllowSettingAnyXHRHeaderFromFileURLs()
{
    static bool shouldAllowSettingAnyXHRHeaderFromFileURLs = (WTF::IOSApplication::isCardiogram() || WTF::IOSApplication::isNike()) && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DisallowsSettingAnyXHRHeaderFromFileURLs);
    return shouldAllowSettingAnyXHRHeaderFromFileURLs;
}

#endif // PLATFORM(IOS_FAMILY)

static bool shouldRequireUserGestureToLoadVideo()
{
#if PLATFORM(IOS_FAMILY)
    static bool shouldRequireUserGestureToLoadVideo = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::RequiresUserGestureToLoadVideo);
    return shouldRequireUserGestureToLoadVideo;
#else
    return false;
#endif
}

static bool shouldRestrictBaseURLSchemes()
{
    static bool shouldRestrictBaseURLSchemes = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::RestrictsBaseURLSchemes);
    return shouldRestrictBaseURLSchemes;
}

#if PLATFORM(MAC)
static uint32_t convertUserInterfaceDirectionPolicy(WKUserInterfaceDirectionPolicy policy)
{
    switch (policy) {
    case WKUserInterfaceDirectionPolicyContent:
        return static_cast<uint32_t>(WebCore::UserInterfaceDirectionPolicy::Content);
    case WKUserInterfaceDirectionPolicySystem:
        return static_cast<uint32_t>(WebCore::UserInterfaceDirectionPolicy::System);
    }
    return static_cast<uint32_t>(WebCore::UserInterfaceDirectionPolicy::Content);
}

static uint32_t convertSystemLayoutDirection(NSUserInterfaceLayoutDirection direction)
{
    switch (direction) {
    case NSUserInterfaceLayoutDirectionLeftToRight:
        return static_cast<uint32_t>(WebCore::UserInterfaceLayoutDirection::LTR);
    case NSUserInterfaceLayoutDirectionRightToLeft:
        return static_cast<uint32_t>(WebCore::UserInterfaceLayoutDirection::RTL);
    }
    return static_cast<uint32_t>(WebCore::UserInterfaceLayoutDirection::LTR);
}

#endif // PLATFORM(MAC)

- (void)_initializeWithConfiguration:(WKWebViewConfiguration *)configuration
{
    if (!configuration)
        [NSException raise:NSInvalidArgumentException format:@"Configuration cannot be nil"];

    _configuration = adoptNS([configuration copy]);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (WKWebView *relatedWebView = [_configuration _relatedWebView]) {
        WKProcessPool *processPool = [_configuration processPool];
        WKProcessPool *relatedWebViewProcessPool = [relatedWebView->_configuration processPool];
        if (processPool && processPool != relatedWebViewProcessPool)
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has process pool %@ but configuration specifies a different process pool %@", relatedWebView, relatedWebViewProcessPool, configuration.processPool];
        if ([relatedWebView->_configuration websiteDataStore] != [_configuration websiteDataStore] && linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ExceptionsForRelatedWebViewsUsingDifferentDataStores))
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has data store %@ but configuration specifies a different data store %@", relatedWebView, [relatedWebView->_configuration websiteDataStore], [_configuration websiteDataStore]];

        [_configuration setProcessPool:relatedWebViewProcessPool];
    }
    ALLOW_DEPRECATED_DECLARATIONS_END

    WebKit::WebProcessPool& processPool = *[_configuration processPool]->_processPool;

    // FIXME: This copy is probably not necessary.
    Ref pageConfiguration = _configuration->_pageConfiguration->copy();
    [self _setupPageConfiguration:pageConfiguration withPool:processPool];

    _usePlatformFindUI = YES;

#if PLATFORM(IOS_FAMILY)
    _obscuredInsetEdgesAffectedBySafeArea = UIRectEdgeTop | UIRectEdgeLeft | UIRectEdgeRight;
    _allowsViewportShrinkToFit = defaultAllowsViewportShrinkToFit;
    _allowsLinkPreview = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::LinkPreviewEnabledByDefault);
    _findInteractionEnabled = NO;
    _needsToPresentLockdownModeMessage = YES;

    auto fastClickingEnabled = []() {
        if (NSNumber *enabledValue = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitFastClickingDisabled"])
            return enabledValue.boolValue;
        return defaultFastClickingEnabled;
    };

    _fastClickingIsDisabled = fastClickingEnabled();
    _dragInteractionPolicy = _WKDragInteractionPolicyDefault;

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:self.bounds processPool:processPool configuration:pageConfiguration.copyRef() webView:self]);
    _page = [_contentView page];

    [self _setupScrollAndContentViews];
    if (!self.opaque || !pageConfiguration->drawsBackground())
        [self _setOpaqueInternal:NO];
    else
        [self _updateScrollViewBackground];

    [self _frameOrBoundsMayHaveChanged];
    [self _registerForNotifications];

    _page->contentSizeCategoryDidChange([self _contentSizeCategory]);
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(META_VIEWPORT)
    _page->setForceAlwaysUserScalable([_configuration ignoresViewportScaleLimits]);
#endif

#if PLATFORM(MAC)
    _impl = makeUnique<WebKit::WebViewImpl>(self, self, processPool, pageConfiguration.copyRef());
    _page = &_impl->page();

    _impl->setAutomaticallyAdjustsContentInsets(true);

#if HAVE(INLINE_PREDICTIONS)
    _impl->setInlinePredictionsEnabled(!![_configuration allowsInlinePredictions]);
#endif
#endif

    if (NSString *applicationNameForUserAgent = configuration.applicationNameForUserAgent)
        _page->setApplicationNameForUserAgent(applicationNameForUserAgent);

    _page->setApplicationNameForDesktopUserAgent(configuration._applicationNameForDesktopUserAgent);

    _navigationState = makeUniqueWithoutRefCountedCheck<WebKit::NavigationState>(self);
    _page->setNavigationClient(_navigationState->createNavigationClient());

    _uiDelegate = makeUniqueWithoutRefCountedCheck<WebKit::UIDelegate>(self);
    _page->setFindClient(makeUnique<WebKit::FindClient>(self));
    _page->setDiagnosticLoggingClient(makeUnique<WebKit::DiagnosticLoggingClient>(self));

    _iconLoadingDelegate = makeUnique<WebKit::IconLoadingDelegate>(self);
    _resourceLoadDelegate = makeUnique<WebKit::ResourceLoadDelegate>(self);

    for (auto& pair : pageConfiguration->urlSchemeHandlers())
        _page->setURLSchemeHandlerForScheme(pair.value.get(), pair.key);

    _page->setCocoaView(self);

    [WebViewVisualIdentificationOverlay installForWebViewIfNeeded:self kind:@"WKWebView" deprecated:NO];

#if PLATFORM(IOS_FAMILY)
    auto timeNow = MonotonicTime::now();
    _timeOfRequestForVisibleContentRectUpdate = timeNow;
    _timeOfLastVisibleContentRectUpdate = timeNow;
    _timeOfFirstVisibleContentRectUpdateWithPendingCommit = timeNow;
#endif

#if ENABLE(WRITING_TOOLS)
    _activeWritingToolsSession = nil;
    _writingToolsTextSuggestions = [NSMapTable strongToWeakObjectsMapTable];
#endif

#if PLATFORM(APPLETV)
    // FIXME: This is a workaround for <rdar://135515434> to prevent the tint color from being set to either solid black or white.
    self.tintColor = UIColor.systemBlueColor;
#endif

#if PLATFORM(IOS_FAMILY)
    if (WebKit::PointerTouchCompatibilitySimulator::requiresPointerTouchCompatibility())
        _pointerTouchCompatibilitySimulator = WTF::makeUnique<WebKit::PointerTouchCompatibilitySimulator>(self);
#endif
}

- (void)_setupPageConfiguration:(Ref<API::PageConfiguration>&)pageConfiguration withPool:(WebKit::WebProcessPool&)pool
{
    pageConfiguration->setPreferences([_configuration preferences]->_preferences.get());
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (WKWebView *relatedWebView = [_configuration _relatedWebView])
        pageConfiguration->setRelatedPage(relatedWebView->_page.get());
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (WKWebView *webViewToCloneSessionStorageFrom = [_configuration _webViewToCloneSessionStorageFrom])
        pageConfiguration->setPageToCloneSessionStorageFrom(webViewToCloneSessionStorageFrom->_page.get());

    pageConfiguration->setUserContentController([_configuration userContentController]->_userContentControllerProxy.get());
    pageConfiguration->setVisitedLinkStore([_configuration _visitedLinkStore]->_visitedLinkStore.get());
    pageConfiguration->setWebsiteDataStore([_configuration websiteDataStore]->_websiteDataStore.get());
    pageConfiguration->setDefaultWebsitePolicies([_configuration defaultWebpagePreferences]->_websitePolicies.get());

#if ENABLE(WK_WEB_EXTENSIONS)
    if (auto *controller = _configuration.get()._strongWebExtensionController)
        pageConfiguration->setWebExtensionController(&controller._webExtensionController);

    if (auto *controller = _configuration.get()._weakWebExtensionController)
        pageConfiguration->setWeakWebExtensionController(&controller._webExtensionController);
#endif

    NSString *groupIdentifier = [_configuration _groupIdentifier];
    if (groupIdentifier.length)
        pageConfiguration->setPageGroup(WebKit::WebPageGroup::create(groupIdentifier).ptr());

    pageConfiguration->setAdditionalSupportedImageTypes(makeVector<String>([_configuration _additionalSupportedImageTypes]));

    pageConfiguration->setWaitsForPaintAfterViewDidMoveToWindow([_configuration _waitsForPaintAfterViewDidMoveToWindow]);
    pageConfiguration->setDrawsBackground([_configuration _drawsBackground]);
    pageConfiguration->setControlledByAutomation([_configuration _isControlledByAutomation]);

    pageConfiguration->preferences().startBatchingUpdates();

    pageConfiguration->preferences().setSuppressesIncrementalRendering(!![_configuration suppressesIncrementalRendering]);
#if !PLATFORM(MAC)
    // FIXME: rdar://99156546. Remove this and WKWebViewConfiguration._printsBackgrounds once all iOS clients adopt the new API.
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToExcludingBackgroundsWhenPrinting))
        pageConfiguration->preferences().setShouldPrintBackgrounds(!![_configuration _printsBackgrounds]);
#endif
    pageConfiguration->preferences().setIncrementalRenderingSuppressionTimeout([_configuration _incrementalRenderingSuppressionTimeout]);
    pageConfiguration->preferences().setJavaScriptMarkupEnabled(!![_configuration _allowsJavaScriptMarkup]);
    pageConfiguration->preferences().setShouldConvertPositionStyleOnCopy(!![_configuration _convertsPositionStyleOnCopy]);
    pageConfiguration->preferences().setHTTPEquivEnabled(!![_configuration _allowsMetaRefresh]);
    pageConfiguration->preferences().setAllowUniversalAccessFromFileURLs(!![_configuration _allowUniversalAccessFromFileURLs]);
    pageConfiguration->preferences().setAllowTopNavigationToDataURLs(!![_configuration _allowTopNavigationToDataURLs]);
    pageConfiguration->preferences().setIncompleteImageBorderEnabled(!![_configuration _incompleteImageBorderEnabled]);
    pageConfiguration->preferences().setShouldDeferAsynchronousScriptsUntilAfterDocumentLoadOrFirstPaint(!![_configuration _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad]);
    pageConfiguration->preferences().setShouldRestrictBaseURLSchemes(shouldRestrictBaseURLSchemes());

#if PLATFORM(MAC)
    pageConfiguration->preferences().setShowsURLsInToolTipsEnabled(!![_configuration _showsURLsInToolTips]);
    pageConfiguration->preferences().setServiceControlsEnabled(!![_configuration _serviceControlsEnabled]);
    pageConfiguration->preferences().setImageControlsEnabled(!![_configuration _imageControlsEnabled]);
    pageConfiguration->preferences().setContextMenuQRCodeDetectionEnabled(!![_configuration _contextMenuQRCodeDetectionEnabled]);

    pageConfiguration->preferences().setUserInterfaceDirectionPolicy(convertUserInterfaceDirectionPolicy([_configuration userInterfaceDirectionPolicy]));
    // We are in the View's initialization routine, so our client hasn't had time to set our user interface direction.
    // Therefore, according to the docs[1], "this property contains the value reported by the app's userInterfaceLayoutDirection property."
    // [1] http://developer.apple.com/library/mac/documentation/Cocoa/Reference/ApplicationKit/Classes/NSView_Class/index.html#//apple_ref/doc/uid/20000014-SW222
    pageConfiguration->preferences().setSystemLayoutDirection(convertSystemLayoutDirection(self.userInterfaceLayoutDirection));
#endif

#if PLATFORM(IOS_FAMILY)
    pageConfiguration->preferences().setAllowsInlineMediaPlayback(!![_configuration allowsInlineMediaPlayback]);
    pageConfiguration->preferences().setAllowsInlineMediaPlaybackAfterFullscreen(!![_configuration _allowsInlineMediaPlaybackAfterFullscreen]);
    pageConfiguration->preferences().setInlineMediaPlaybackRequiresPlaysInlineAttribute(!![_configuration _inlineMediaPlaybackRequiresPlaysInlineAttribute]);
    pageConfiguration->preferences().setAllowsPictureInPictureMediaPlayback(!![_configuration allowsPictureInPictureMediaPlayback] && shouldAllowPictureInPictureMediaPlayback());
    pageConfiguration->preferences().setUserInterfaceDirectionPolicy(static_cast<uint32_t>(WebCore::UserInterfaceDirectionPolicy::Content));
    pageConfiguration->preferences().setSystemLayoutDirection(static_cast<uint32_t>(WebCore::TextDirection::LTR));
    pageConfiguration->preferences().setAllowSettingAnyXHRHeaderFromFileURLs(shouldAllowSettingAnyXHRHeaderFromFileURLs());
    pageConfiguration->preferences().setShouldDecidePolicyBeforeLoadingQuickLookPreview(!![_configuration _shouldDecidePolicyBeforeLoadingQuickLookPreview]);
#if ENABLE(DEVICE_ORIENTATION)
    pageConfiguration->preferences().setDeviceOrientationPermissionAPIEnabled(linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SupportsDeviceOrientationAndMotionPermissionAPI));
#endif
#if USE(SYSTEM_PREVIEW)
    pageConfiguration->preferences().setSystemPreviewEnabled(!![_configuration _systemPreviewEnabled]);
#endif
#endif // PLATFORM(IOS_FAMILY)
    pageConfiguration->preferences().setScrollToTextFragmentIndicatorEnabled(!![_configuration _scrollToTextFragmentIndicatorEnabled]);
    pageConfiguration->preferences().setScrollToTextFragmentMarkingEnabled(!![_configuration _scrollToTextFragmentMarkingEnabled]);

    WKAudiovisualMediaTypes mediaTypesRequiringUserGesture = [_configuration mediaTypesRequiringUserActionForPlayback];
    pageConfiguration->preferences().setRequiresUserGestureForVideoPlayback((mediaTypesRequiringUserGesture & WKAudiovisualMediaTypeVideo) == WKAudiovisualMediaTypeVideo);
    pageConfiguration->preferences().setRequiresUserGestureForAudioPlayback(((mediaTypesRequiringUserGesture & WKAudiovisualMediaTypeAudio) == WKAudiovisualMediaTypeAudio));
    pageConfiguration->preferences().setRequiresUserGestureToLoadVideo(shouldRequireUserGestureToLoadVideo());
    pageConfiguration->preferences().setMainContentUserGestureOverrideEnabled(!![_configuration _mainContentUserGestureOverrideEnabled]);
    pageConfiguration->preferences().setInvisibleAutoplayNotPermitted(!![_configuration _invisibleAutoplayNotPermitted]);
    pageConfiguration->preferences().setMediaDataLoadsAutomatically(!![_configuration _mediaDataLoadsAutomatically]);
    pageConfiguration->preferences().setAttachmentElementEnabled(!![_configuration _attachmentElementEnabled]);
    pageConfiguration->preferences().setAttachmentWideLayoutEnabled(!![_configuration _attachmentWideLayoutEnabled]);

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    pageConfiguration->preferences().setDataDetectorTypes(fromWKDataDetectorTypes([_configuration dataDetectorTypes]).toRaw());
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    pageConfiguration->preferences().setAllowsAirPlayForMediaPlayback(!![_configuration allowsAirPlayForMediaPlayback]);
#endif

#if ENABLE(APPLE_PAY)
    pageConfiguration->preferences().setApplePayEnabled(!![_configuration _applePayEnabled]);
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    pageConfiguration->preferences().setCSSTransformStyleSeparatedEnabled(!![_configuration _cssTransformStyleSeparatedEnabled]);
#endif

    pageConfiguration->preferences().setNeedsStorageAccessFromFileURLsQuirk(!![_configuration _needsStorageAccessFromFileURLsQuirk]);
    pageConfiguration->preferences().setMediaContentTypesRequiringHardwareSupport(String([_configuration _mediaContentTypesRequiringHardwareSupport]));
    pageConfiguration->preferences().setAllowMediaContentTypesRequiringHardwareSupportAsFallback(!![_configuration _allowMediaContentTypesRequiringHardwareSupportAsFallback]);
    if (!pageConfiguration->preferences().mediaDevicesEnabled())
        pageConfiguration->preferences().setMediaDevicesEnabled(!![_configuration _mediaCaptureEnabled]);

    pageConfiguration->preferences().setColorFilterEnabled(!![_configuration _colorFilterEnabled]);

    pageConfiguration->preferences().setUndoManagerAPIEnabled(!![_configuration _undoManagerAPIEnabled]);
    
#if ENABLE(APP_HIGHLIGHTS)
    pageConfiguration->preferences().setAppHighlightsEnabled(!![_configuration _appHighlightsEnabled]);
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    pageConfiguration->preferences().setLegacyEncryptedMediaAPIEnabled(!![_configuration _legacyEncryptedMediaAPIEnabled]);
#endif

#if PLATFORM(IOS_FAMILY)
    bool hasServiceWorkerEntitlement = (WTF::processHasEntitlement("com.apple.developer.WebKit.ServiceWorkers"_s) || WTF::processHasEntitlement("com.apple.developer.web-browser"_s)) && ![_configuration preferences]._serviceWorkerEntitlementDisabledForTesting;
    if (!hasServiceWorkerEntitlement && ![_configuration limitsNavigationsToAppBoundDomains])
        pageConfiguration->preferences().setServiceWorkersEnabled(false);
    pageConfiguration->preferences().setServiceWorkerEntitlementDisabledForTesting(!![_configuration preferences]._serviceWorkerEntitlementDisabledForTesting);
#endif

    pageConfiguration->preferences().setSampledPageTopColorMaxDifference([_configuration _sampledPageTopColorMaxDifference]);
    pageConfiguration->preferences().setSampledPageTopColorMinHeight([_configuration _sampledPageTopColorMinHeight]);

    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SiteSpecificQuirksAreEnabledByDefault))
        pageConfiguration->preferences().setNeedsSiteSpecificQuirks(false);

#if PLATFORM(IOS_FAMILY)
    pageConfiguration->preferences().setAlternateFormControlDesignEnabled(WebKit::defaultAlternateFormControlDesignEnabled());
#endif

    // For SharedPreferencesForWebProcess
    pageConfiguration->preferences().setAllowTestOnlyIPC(!![_configuration _allowTestOnlyIPC]);
    pageConfiguration->preferences().setUsesSingleWebProcess(pool.usesSingleWebProcess());

    pageConfiguration->preferences().endBatchingUpdates();

#if PLATFORM(APPLETV)
    if (RefPtr dataStore = pageConfiguration->websiteDataStoreIfExists(); !dataStore || dataStore->isPersistent()) {
        RELEASE_LOG_ERROR(API, "Created web view with persistent data store");
        WTFReportBacktraceWithStackDepth(7);
    }
#endif
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    [self _initializeWithConfiguration:configuration];

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [super initWithCoder:coder]))
        return nil;

    WKWebViewConfiguration *configuration = [coder decodeObjectOfClass:[WKWebViewConfiguration class] forKey:@"configuration"];
    [self _initializeWithConfiguration:configuration];

    self.allowsBackForwardNavigationGestures = [coder decodeBoolForKey:@"allowsBackForwardNavigationGestures"];
    self.customUserAgent = [coder decodeObjectOfClass:[NSString class] forKey:@"customUserAgent"];
    self.allowsLinkPreview = [coder decodeBoolForKey:@"allowsLinkPreview"];

#if PLATFORM(MAC)
    self.allowsMagnification = [coder decodeBoolForKey:@"allowsMagnification"];
    self.magnification = [coder decodeDoubleForKey:@"magnification"];
#endif

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)
    self.findInteractionEnabled = [coder decodeBoolForKey:@"findInteractionEnabled"];
#endif

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [super encodeWithCoder:coder];

    [coder encodeObject:_configuration.get() forKey:@"configuration"];

    [coder encodeBool:self.allowsBackForwardNavigationGestures forKey:@"allowsBackForwardNavigationGestures"];
    [coder encodeObject:self.customUserAgent forKey:@"customUserAgent"];
    [coder encodeBool:self.allowsLinkPreview forKey:@"allowsLinkPreview"];

#if PLATFORM(MAC)
    [coder encodeBool:self.allowsMagnification forKey:@"allowsMagnification"];
    [coder encodeDouble:self.magnification forKey:@"magnification"];
#endif

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)
    [coder encodeBool:self.isFindInteractionEnabled forKey:@"findInteractionEnabled"];
#endif
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebView.class, self))
        return;

#if PLATFORM(MAC)
    [_textFinderClient willDestroyView:self];
#endif

    [self _setResourceLoadDelegate:nil];

#if PLATFORM(IOS_FAMILY)
    [_contentView _webViewDestroyed];

    if (_page && _remoteObjectRegistry)
        _page->configuration().processPool().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->identifier());
#endif

    if (_page)
        _page->close();

#if PLATFORM(IOS_FAMILY)
    [_remoteObjectRegistry _invalidate];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_scrollView setInternalDelegate:nil];
#endif

#if PLATFORM(MAC) && HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)
    [self _invalidateWindowSnapshotReadinessHandler];
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    [self _invalidateResizeAssertions];
#endif

    [super dealloc];
}

- (id)valueForUndefinedKey:(NSString *)key
{
    if ([key isEqualToString:@"serverTrust"])
        return (__bridge id)[self serverTrust];

    return [super valueForUndefinedKey:key];
}

- (void)addObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void *)context
{
    if ([keyPath isEqualToString:@"_webProcessState"])
        _page->configuration().processPool().setWebProcessStateUpdatesForPageClientEnabled(true);

    [super addObserver:observer forKeyPath:keyPath options:options context:context];
}

#pragma mark - macOS/iOS API

- (WKWebViewConfiguration *)configuration
{
    return adoptNS([_configuration copy]).autorelease();
}

- (WKBackForwardList *)backForwardList
{
    [self _didAccessBackForwardList];
    return wrapper(_page->backForwardList());
}

- (id <WKNavigationDelegate>)navigationDelegate
{
    return _navigationState->navigationDelegate().autorelease();
}

- (void)setNavigationDelegate:(id <WKNavigationDelegate>)navigationDelegate
{
    _page->setNavigationClient(_navigationState->createNavigationClient());
    _navigationState->setNavigationDelegate(navigationDelegate);
}

- (id <WKUIDelegate>)UIDelegate
{
    return _uiDelegate->delegate().autorelease();
}

- (void)setUIDelegate:(id<WKUIDelegate>)UIDelegate
{
    _uiDelegate->setDelegate(UIDelegate);
#if ENABLE(CONTEXT_MENUS)
    _page->setContextMenuClient(_uiDelegate->createContextMenuClient());
#endif
    _page->setUIClient(_uiDelegate->createUIClient());
}

- (id <_WKIconLoadingDelegate>)_iconLoadingDelegate
{
    return _iconLoadingDelegate->delegate().autorelease();
}

- (void)_setIconLoadingDelegate:(id<_WKIconLoadingDelegate>)iconLoadingDelegate
{
    _page->setIconLoadingClient(_iconLoadingDelegate->createIconLoadingClient());
    _iconLoadingDelegate->setDelegate(iconLoadingDelegate);
}

- (id <_WKResourceLoadDelegate>)_resourceLoadDelegate
{
    return _resourceLoadDelegate->delegate().autorelease();
}

- (void)_setResourceLoadDelegate:(id<_WKResourceLoadDelegate>)delegate
{
    if (!_page || !_resourceLoadDelegate)
        return;

    if (delegate) {
        _page->setResourceLoadClient(_resourceLoadDelegate->createResourceLoadClient());
        _resourceLoadDelegate->setDelegate(delegate);
    } else {
        _page->setResourceLoadClient(nullptr);
        _resourceLoadDelegate->setDelegate(nil);
    }
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request
{
    THROW_IF_SUSPENDED;
    if (_page->isServiceWorkerPage())
        [NSException raise:NSInternalInconsistencyException format:@"The WKWebView was used to load a service worker"];
    return wrapper(_page->loadRequest(request)).autorelease();
}

- (WKNavigation *)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL
{
    THROW_IF_SUSPENDED;
    if (_page->isServiceWorkerPage())
        [NSException raise:NSInternalInconsistencyException format:@"The WKWebView was used to load a service worker"];

    if (![URL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", URL];

    if (![readAccessURL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", readAccessURL];

    return wrapper(_page->loadFile(URL.filePathURL.absoluteString, readAccessURL.absoluteString)).autorelease();
}

- (WKNavigation *)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL
{
    THROW_IF_SUSPENDED;
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];

    return [self loadData:data MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:baseURL];
}

- (WKNavigation *)loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL
{
    THROW_IF_SUSPENDED;
    if (_page->isServiceWorkerPage())
        [NSException raise:NSInternalInconsistencyException format:@"The WKWebView was used to load a service worker"];

    return wrapper(_page->loadData(WebCore::SharedBuffer::create(data), MIMEType, characterEncodingName, baseURL.absoluteString)).autorelease();
}

- (void)startDownloadUsingRequest:(NSURLRequest *)request completionHandler:(void(^)(WKDownload *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->downloadRequest(request, [completionHandler = makeBlockPtr(completionHandler)] (auto* download) {
        if (download)
            completionHandler(wrapper(download));
        else
            ASSERT_NOT_REACHED();
    });
}

- (void)resumeDownloadFromResumeData:(NSData *)resumeData completionHandler:(void(^)(WKDownload *))completionHandler
{
    THROW_IF_SUSPENDED;
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:resumeData error:nil]);
    [unarchiver setDecodingFailurePolicy:NSDecodingFailurePolicyRaiseException];
    NSDictionary *dictionary = [unarchiver decodeObjectOfClasses:[NSSet setWithObjects:[NSDictionary class], [NSArray class], [NSString class], [NSNumber class], [NSData class], [NSURL class], [NSURLRequest class], nil] forKey:@"NSKeyedArchiveRootObjectKey"];
    [unarchiver finishDecoding];
    NSString *path = [dictionary objectForKey:@"NSURLSessionResumeInfoLocalPath"];

    if (!path)
        [NSException raise:NSInvalidArgumentException format:@"Invalid resume data"];

    _page->resumeDownload(API::Data::createWithoutCopying(resumeData), path, [completionHandler = makeBlockPtr(completionHandler)] (auto* download) {
        if (download)
            completionHandler(wrapper(download));
        else
            ASSERT_NOT_REACHED();
    });
}

- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->goToBackForwardItem(item._item)).autorelease();
}

- (NSString *)title
{
    return _page->pageLoadState().title();
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().activeURL()];
}

- (NSURL *)_resourceDirectoryURL
{
    return _page->currentResourceDirectoryURL();
}

- (BOOL)isLoading
{
    return _page->pageLoadState().isLoading();
}

- (double)estimatedProgress
{
    return _page->pageLoadState().estimatedProgress();
}

- (BOOL)hasOnlySecureContent
{
    return _page->pageLoadState().hasOnlySecureContent();
}

- (SecTrustRef)serverTrust
{
    return _page->pageLoadState().certificateInfo().trust().get();
}

- (void)_didAccessBackForwardList
{
    BOOL oldValue = _didAccessBackForwardList;
    _didAccessBackForwardList = YES;

#if ENABLE(PAGE_LOAD_OBSERVER)
    if (!oldValue)
        [self _updatePageLoadObserverState];
#else
    UNUSED_PARAM(oldValue);
#endif
}

- (BOOL)canGoBack
{
    [self _didAccessBackForwardList];
    return _page->pageLoadState().canGoBack();
}

- (BOOL)canGoForward
{
    [self _didAccessBackForwardList];
    return _page->pageLoadState().canGoForward();
}

- (WKNavigation *)goBack
{
    THROW_IF_SUSPENDED;
    [self _didAccessBackForwardList];
    if (self._safeBrowsingWarning)
        return [self reload];
    return wrapper(_page->goBack()).autorelease();
}

- (WKNavigation *)goForward
{
    THROW_IF_SUSPENDED;
    [self _didAccessBackForwardList];
    return wrapper(_page->goForward()).autorelease();
}

- (WKNavigation *)reload
{
    THROW_IF_SUSPENDED;
    OptionSet<WebCore::ReloadOption> reloadOptions;
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ExpiredOnlyReloadBehavior))
        reloadOptions.add(WebCore::ReloadOption::ExpiredOnly);

    return wrapper(_page->reload(reloadOptions)).autorelease();
}

- (WKNavigation *)reloadFromOrigin
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->reload(WebCore::ReloadOption::FromOrigin)).autorelease();
}

- (void)stopLoading
{
    THROW_IF_SUSPENDED;
    _page->stopLoading();
}

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:YES inFrame:nil inWorld:WKContentWorld.pageWorld completionHandler:completionHandler];
}

- (void)evaluateJavaScript:(NSString *)javaScriptString inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)callAsyncJavaScript:(NSString *)javaScriptString arguments:(NSDictionary<NSString *, id> *)arguments inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:YES withSourceURL:nil withArguments:arguments forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

static bool validateArgument(id argument)
{
    if ([argument isKindOfClass:[NSString class]] || [argument isKindOfClass:[NSNumber class]] || [argument isKindOfClass:[NSDate class]] || [argument isKindOfClass:[NSNull class]])
        return true;

    if ([argument isKindOfClass:[NSArray class]]) {
        __block BOOL valid = true;

        [argument enumerateObjectsUsingBlock:^(id object, NSUInteger, BOOL *stop) {
            if (!validateArgument(object)) {
                valid = false;
                *stop = YES;
            }
        }];

        return valid;
    }

    if ([argument isKindOfClass:[NSDictionary class]]) {
        __block bool valid = true;

        [argument enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
            if (!validateArgument(key) || !validateArgument(value)) {
                valid = false;
                *stop = YES;
            }
        }];

        return valid;
    }

    return false;
}

- (void)closeAllMediaPresentations
{
    [self closeAllMediaPresentationsWithCompletionHandler:^{ }];
}

- (void)closeAllMediaPresentations:(void (^)(void))completionHandler
{
    [self closeAllMediaPresentationsWithCompletionHandler:completionHandler];
}

- (void)closeAllMediaPresentationsWithCompletionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

#if ENABLE(FULLSCREEN_API)
    if (auto videoPresentationManager = _page->videoPresentationManager()) {
        videoPresentationManager->forEachSession([callbackAggregator] (auto& model, auto& interface) mutable {
            model.requestCloseAllMediaPresentations(false, [callbackAggregator] { });
        });
    }

    if (auto fullScreenManager = _page->fullScreenManager(); fullScreenManager && fullScreenManager->isFullScreen())
        fullScreenManager->closeWithCallback([callbackAggregator] { });
#endif
}

- (void)pauseAllMediaPlayback:(void (^)(void))completionHandler
{
    [self pauseAllMediaPlaybackWithCompletionHandler:completionHandler];
}

- (void)pauseAllMediaPlaybackWithCompletionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler) {
        _page->pauseAllMediaPlayback([] { });
        return;
    }

    _page->pauseAllMediaPlayback(makeBlockPtr(completionHandler));
}

- (void)resumeAllMediaPlayback:(void (^)(void))completionHandler
{
    [self setAllMediaPlaybackSuspended:NO completionHandler:completionHandler];
}

- (void)suspendAllMediaPlayback:(void (^)(void))completionHandler
{
    [self setAllMediaPlaybackSuspended:YES completionHandler:completionHandler];
}

- (void)setAllMediaPlaybackSuspended:(BOOL)suspended completionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler)
        completionHandler = [] { };

    if (suspended) {
        _page->suspendAllMediaPlayback(makeBlockPtr(completionHandler));
        return;
    }
    _page->resumeAllMediaPlayback(makeBlockPtr(completionHandler));
}

static WKMediaPlaybackState toWKMediaPlaybackState(WebKit::MediaPlaybackState mediaPlaybackState)
{
    switch (mediaPlaybackState) {
    case WebKit::MediaPlaybackState::NoMediaPlayback:
        return WKMediaPlaybackStateNone;
    case WebKit::MediaPlaybackState::MediaPlaybackPlaying:
        return WKMediaPlaybackStatePlaying;
    case WebKit::MediaPlaybackState::MediaPlaybackPaused:
        return WKMediaPlaybackStatePaused;
    case WebKit::MediaPlaybackState::MediaPlaybackSuspended:
        return WKMediaPlaybackStateSuspended;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return WKMediaPlaybackStateNone;
}

- (void)requestMediaPlaybackState:(void (^)(WKMediaPlaybackState))completionHandler
{
    [self requestMediaPlaybackStateWithCompletionHandler:completionHandler];
}

- (void)requestMediaPlaybackStateWithCompletionHandler:(void (^)(WKMediaPlaybackState))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler)
        return;

    return _page->requestMediaPlaybackState([completionHandler = makeBlockPtr(completionHandler)] (auto&& mediaPlaybackState) {
        completionHandler(toWKMediaPlaybackState(mediaPlaybackState));
    });
}

- (WKMediaCaptureState)cameraCaptureState
{
    auto state = _page->reportedMediaState();
    if (state & WebCore::MediaProducerMediaState::HasActiveVideoCaptureDevice)
        return WKMediaCaptureStateActive;
    if (state & WebCore::MediaProducerMediaState::HasMutedVideoCaptureDevice)
        return WKMediaCaptureStateMuted;
    return WKMediaCaptureStateNone;
}

- (WKMediaCaptureState)microphoneCaptureState
{
    auto state = _page->reportedMediaState();
    if (state & WebCore::MediaProducerMediaState::HasActiveAudioCaptureDevice)
        return WKMediaCaptureStateActive;
    if (state & WebCore::MediaProducerMediaState::HasMutedAudioCaptureDevice)
        return WKMediaCaptureStateMuted;
    return WKMediaCaptureStateNone;
}

- (void)setMicrophoneCaptureState:(WKMediaCaptureState)state completionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler)
        completionHandler = [] { };

    if (state == WKMediaCaptureStateNone) {
        _page->stopMediaCapture(WebCore::MediaProducerMediaCaptureKind::Microphone, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
        return;
    }
    auto mutedState = _page->mutedStateFlags();
    if (state == WKMediaCaptureStateActive)
        mutedState.remove(WebCore::MediaProducerMutedState::AudioCaptureIsMuted);
    else if (state == WKMediaCaptureStateMuted)
        mutedState.add(WebCore::MediaProducerMutedState::AudioCaptureIsMuted);
    _page->setMuted(mutedState, WebKit::WebPageProxy::FromApplication::Yes, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)setCameraCaptureState:(WKMediaCaptureState)state completionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler)
        completionHandler = [] { };

    if (state == WKMediaCaptureStateNone) {
        _page->stopMediaCapture(WebCore::MediaProducerMediaCaptureKind::Camera, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
        return;
    }
    auto mutedState = _page->mutedStateFlags();
    if (state == WKMediaCaptureStateActive)
        mutedState.remove(WebCore::MediaProducerMutedState::VideoCaptureIsMuted);
    else if (state == WKMediaCaptureStateMuted)
        mutedState.add(WebCore::MediaProducerMutedState::VideoCaptureIsMuted);
    _page->setMuted(mutedState, WebKit::WebPageProxy::FromApplication::Yes, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_evaluateJavaScript:(NSString *)javaScriptString asAsyncFunction:(BOOL)asAsyncFunction withSourceURL:(NSURL *)sourceURL withArguments:(NSDictionary<NSString *, id> *)arguments forceUserGesture:(BOOL)forceUserGesture inFrame:(WKFrameInfo *)frame inWorld:(WKContentWorld *)world completionHandler:(void (^)(id, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    auto handler = adoptNS([completionHandler copy]);

    std::optional<WebCore::ArgumentWireBytesMap> argumentsMap;
    if (asAsyncFunction)
        argumentsMap = WebCore::ArgumentWireBytesMap { };
    NSString *errorMessage = nil;

    for (id key in arguments) {
        id value = [arguments objectForKey:key];
        auto serializedValue = API::SerializedScriptValue::createFromNSObject(value);
        if (!serializedValue) {
            errorMessage = @"Function argument values must be one of the following types, or contain only the following types: NSNumber, NSNull, NSDate, NSString, NSArray, and NSDictionary";
            break;
        }

        argumentsMap->set(key, serializedValue->internalRepresentation().wireBytes());
    }

    if (errorMessage && handler) {
        RetainPtr<NSMutableDictionary> userInfo = adoptNS([[NSMutableDictionary alloc] init]);

        [userInfo setObject:localizedDescriptionForErrorCode(WKErrorJavaScriptExceptionOccurred) forKey:NSLocalizedDescriptionKey];
        [userInfo setObject:errorMessage forKey:_WKJavaScriptExceptionMessageErrorKey];

        auto error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorJavaScriptExceptionOccurred userInfo:userInfo.get()]);
        RunLoop::main().dispatch([handler, error] {
            auto rawHandler = (void (^)(id, NSError *))handler.get();
            rawHandler(nil, error.get());
        });

        return;
    }
    
    std::optional<WebCore::FrameIdentifier> frameID;
    if (frame && frame._handle && frame._handle->_frameHandle->frameID())
        frameID = frame._handle->_frameHandle->frameID();

    auto removeTransientActivation = !_dontResetTransientActivationAfterRunJavaScript && WebKit::shouldEvaluateJavaScriptWithoutTransientActivation() ? WebCore::RemoveTransientActivation::Yes : WebCore::RemoveTransientActivation::No;
    _page->runJavaScriptInFrameInScriptWorld({ javaScriptString, JSC::SourceTaintedOrigin::Untainted, sourceURL, !!asAsyncFunction, WTFMove(argumentsMap), !!forceUserGesture, removeTransientActivation }, frameID, *world->_contentWorld.get(), [handler] (auto&& result) {
        if (!handler)
            return;

        auto rawHandler = (void (^)(id, NSError *))handler.get();
        if (!result.has_value()) {
            rawHandler(nil, nsErrorFromExceptionDetails(result.error()).get());
            return;
        }

        if (!result.value()) {
            rawHandler(nil, createNSError(WKErrorJavaScriptResultTypeIsUnsupported).get());
            return;
        }

        id body = API::SerializedScriptValue::deserialize(result.value()->internalRepresentation());
        rawHandler(body, nil);
    });
}

- (void)takeSnapshotWithConfiguration:(WKSnapshotConfiguration *)snapshotConfiguration completionHandler:(void(^)(CocoaImage *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    constexpr bool snapshotFailedTraceValue = false;
    tracePoint(TakeSnapshotStart);

    CGRect rectInViewCoordinates = snapshotConfiguration && !CGRectIsNull(snapshotConfiguration.rect) ? snapshotConfiguration.rect : self.bounds;
    CGFloat snapshotWidth;
    if (snapshotConfiguration)
        snapshotWidth = snapshotConfiguration.snapshotWidth.doubleValue ?: rectInViewCoordinates.size.width;
    else
        snapshotWidth = self.bounds.size.width;

    auto handler = makeBlockPtr(completionHandler);

    if (CGRectIsEmpty(rectInViewCoordinates) || !snapshotWidth) {
        RunLoop::main().dispatch([handler = WTFMove(handler)] {
#if USE(APPKIT)
            auto image = adoptNS([[NSImage alloc] initWithSize:NSMakeSize(0, 0)]);
#else
            auto image = adoptNS([[UIImage alloc] init]);
#endif
            handler(image.get(), nil);
        });
        return;
    }

#if USE(APPKIT)
    CGFloat imageScale = snapshotWidth / rectInViewCoordinates.size.width;
    CGFloat imageHeight = imageScale * rectInViewCoordinates.size.height;

    // Need to scale by device scale factor or the image will be distorted.
    CGFloat deviceScale = _page->deviceScaleFactor();
    WebCore::IntSize bitmapSize(snapshotWidth, imageHeight);
    bitmapSize.scale(deviceScale, deviceScale);

    WebKit::SnapshotOptions snapshotOptions;
    if (snapshotConfiguration._usesContentsRect) {
        snapshotOptions = WebKit::SnapshotOption::FullContentRect;
        // Let WebPage decide the image size.
        bitmapSize = WebCore::IntSize { };
    } else
        snapshotOptions = WebKit::SnapshotOption::InViewCoordinates;
    if (!snapshotConfiguration._includesSelectionHighlighting)
        snapshotOptions.add(WebKit::SnapshotOption::ExcludeSelectionHighlighting);
    if (snapshotConfiguration._usesTransparentBackground)
        snapshotOptions.add(WebKit::SnapshotOption::TransparentBackground);

    // Software snapshot will not capture elements rendered with hardware acceleration (WebGL, video, etc).
    // This code doesn't consider snapshotConfiguration.afterScreenUpdates since the software snapshot always
    // contains recent updates. If we ever have a UI-side snapshot mechanism on macOS, we will need to factor
    // in snapshotConfiguration.afterScreenUpdates at that time.
    _page->takeSnapshot(WebCore::enclosingIntRect(rectInViewCoordinates), bitmapSize, snapshotOptions, [handler, snapshotWidth, imageHeight, usesContentRect = snapshotOptions.contains(WebKit::SnapshotOption::FullContentRect)](std::optional<WebCore::ShareableBitmap::Handle>&& imageHandle) {
        if (!imageHandle) {
            tracePoint(TakeSnapshotEnd, snapshotFailedTraceValue);
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }
        auto bitmap = WebCore::ShareableBitmap::create(WTFMove(*imageHandle), WebCore::SharedMemory::Protection::ReadOnly);
        RetainPtr<CGImageRef> cgImage = bitmap ? bitmap->makeCGImage() : nullptr;
        auto width = usesContentRect ? (CGFloat)CGImageGetWidth(cgImage.get()) : snapshotWidth;
        auto height = usesContentRect ? (CGFloat)CGImageGetHeight(cgImage.get()) : imageHeight;
        auto image = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:NSMakeSize(width, height)]);
        tracePoint(TakeSnapshotEnd, true);
        handler(image.get(), nil);
    });
#else
    auto useIntrinsicDeviceScaleFactor = [[_customContentView class] web_requiresCustomSnapshotting];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://131638772> UIScreen.mainScreen is deprecated.
    CGFloat deviceScale = useIntrinsicDeviceScaleFactor ? UIScreen.mainScreen.scale : _page->deviceScaleFactor();
ALLOW_DEPRECATED_DECLARATIONS_END
    CGFloat imageWidth = useIntrinsicDeviceScaleFactor ? snapshotWidth : snapshotWidth * deviceScale;
    RetainPtr<WKWebView> strongSelf = self;
    BOOL afterScreenUpdates = snapshotConfiguration && snapshotConfiguration.afterScreenUpdates;
    auto callSnapshotRect = [strongSelf, afterScreenUpdates, rectInViewCoordinates, imageWidth, deviceScale, handler] {
        [strongSelf _snapshotRectAfterScreenUpdates:afterScreenUpdates rectInViewCoordinates:rectInViewCoordinates intoImageOfWidth:imageWidth completionHandler:[handler, deviceScale](CGImageRef snapshotImage) {
            RetainPtr<NSError> error;
            RetainPtr<UIImage> image;
            
            if (!snapshotImage)
                error = createNSError(WKErrorUnknown);
            else
                image = adoptNS([[UIImage alloc] initWithCGImage:snapshotImage scale:deviceScale orientation:UIImageOrientationUp]);

            tracePoint(TakeSnapshotEnd, !!snapshotImage);
            handler(image.get(), error.get());
        }];
    };

    if ((snapshotConfiguration && !snapshotConfiguration.afterScreenUpdates) || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SnapshotAfterScreenUpdates)) {
        callSnapshotRect();
        return;
    }

    if (!_page->hasRunningProcess() || !_page->drawingArea()) {
        tracePoint(TakeSnapshotEnd, snapshotFailedTraceValue);
        handler(nil, createNSError(WKErrorUnknown).get());
        return;
    }

    _page->callAfterNextPresentationUpdate([callSnapshotRect = WTFMove(callSnapshotRect), handler, page = Ref { *_page }] () mutable {

        if (!page->hasRunningProcess()) {
            tracePoint(TakeSnapshotEnd, snapshotFailedTraceValue);
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }

        // Create an implicit transaction to ensure a commit will happen next.
        [CATransaction activate];

        // Wait for the next flush to ensure the latest IOSurfaces are pushed to backboardd before taking the snapshot.
        [CATransaction addCommitHandler:[callSnapshotRect = WTFMove(callSnapshotRect)]() mutable {
            // callSnapshotRect() calls the client callback which may call directly or indirectly addCommitHandler.
            // It is prohibited by CA to add a commit handler while processing a registered commit handler.
            // So postpone calling callSnapshotRect() till CATransaction processes its commit handlers.
            dispatch_async(dispatch_get_main_queue(), [callSnapshotRect = WTFMove(callSnapshotRect)] {
                callSnapshotRect();
            });
        } forPhase:kCATransactionPhasePostCommit];
    });
#endif
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
    THROW_IF_SUSPENDED;
#if PLATFORM(MAC)
    _impl->setAllowsBackForwardNavigationGestures(allowsBackForwardNavigationGestures);
#elif PLATFORM(IOS_FAMILY)
    if (_allowsBackForwardNavigationGestures == allowsBackForwardNavigationGestures)
        return;

    _allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;

    if (allowsBackForwardNavigationGestures && !_gestureController) {
        _gestureController = WebKit::ViewGestureController::create(*_page);
        _gestureController->installSwipeHandler(self, [self scrollView]);
        if (WKWebView *alternateWebView = [_configuration _alternateWebViewForNavigationGestures])
            _gestureController->setAlternateBackForwardListSourcePage(alternateWebView->_page.get());
    }

    if (_gestureController)
        _gestureController->setSwipeGestureEnabled(allowsBackForwardNavigationGestures);

    _page->setShouldRecordNavigationSnapshots(allowsBackForwardNavigationGestures);
#endif
}

- (BOOL)allowsBackForwardNavigationGestures
{
#if PLATFORM(MAC)
    return _impl->allowsBackForwardNavigationGestures();
#elif PLATFORM(IOS_FAMILY)
    return _allowsBackForwardNavigationGestures;
#endif
}

- (NSString *)customUserAgent
{
    return _page->customUserAgent();
}

- (void)setCustomUserAgent:(NSString *)customUserAgent
{
    THROW_IF_SUSPENDED;
    _page->setCustomUserAgent(customUserAgent);
}

- (BOOL)allowsLinkPreview
{
#if PLATFORM(MAC)
    return _impl->allowsLinkPreview();
#elif PLATFORM(IOS_FAMILY)
    return _allowsLinkPreview;
#endif
}

- (void)setAllowsLinkPreview:(BOOL)allowsLinkPreview
{
    THROW_IF_SUSPENDED;
#if PLATFORM(MAC)
    _impl->setAllowsLinkPreview(allowsLinkPreview);
    return;
#elif PLATFORM(IOS_FAMILY)
    if (_allowsLinkPreview == allowsLinkPreview)
        return;

    _allowsLinkPreview = allowsLinkPreview;

#if HAVE(LINK_PREVIEW)
    if (_allowsLinkPreview)
        [_contentView _registerPreview];
    else
        [_contentView _unregisterPreview];
    [_contentView _didChangeLinkPreviewAvailability];
#endif // HAVE(LINK_PREVIEW)
#endif // PLATFORM(IOS_FAMILY)
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    THROW_IF_SUSPENDED;
#if PLATFORM(MAC)
    _impl->suppressContentRelativeChildViews(WebKit::WebViewImpl::ContentRelativeChildViewsSuppressionType::TemporarilyRemove);
#endif
    _page->setPageZoomFactor(pageZoom);
}

- (CGFloat)pageZoom
{
    return _page->pageZoomFactor();
}

inline OptionSet<WebKit::FindOptions> toFindOptions(WKFindConfiguration *configuration)
{
    OptionSet<WebKit::FindOptions> findOptions;

    if (!configuration.caseSensitive)
        findOptions.add(WebKit::FindOptions::CaseInsensitive);
    if (configuration.backwards)
        findOptions.add(WebKit::FindOptions::Backwards);
    if (configuration.wraps)
        findOptions.add(WebKit::FindOptions::WrapAround);

    return findOptions;
}

- (void)findString:(NSString *)string withConfiguration:(WKFindConfiguration *)configuration completionHandler:(void (^)(WKFindResult *result))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!string.length) {
        completionHandler(adoptNS([[WKFindResult alloc] _initWithMatchFound:NO]).get());
        return;
    }

    _page->findString(string, toFindOptions(configuration), 1, [handler = makeBlockPtr(completionHandler)](bool found) {
        handler(adoptNS([[WKFindResult alloc] _initWithMatchFound:found]).get());
    });
}

+ (BOOL)handlesURLScheme:(NSString *)urlScheme
{
    return WebCore::LegacySchemeRegistry::isBuiltinScheme(urlScheme);
}

- (void)setMediaType:(NSString *)mediaStyle
{
    THROW_IF_SUSPENDED;
    _page->setOverriddenMediaType(mediaStyle);
}

- (NSString *)mediaType
{
    return _page->overriddenMediaType().isNull() ? nil : (NSString *)_page->overriddenMediaType();
}

- (id)interactionState
{
    return WebKit::encodeSessionState(_page->sessionState()).autorelease();
}

- (void)setInteractionState:(id)interactionState
{
    THROW_IF_SUSPENDED;
    if (![(id)interactionState isKindOfClass:[NSData class]])
        return;

    WebKit::SessionState sessionState;
    if (!WebKit::decodeSessionState((NSData *)(interactionState), sessionState))
        return;
    _page->restoreFromSessionState(sessionState, true);
}

- (BOOL)isInspectable
{
#if ENABLE(REMOTE_INSPECTOR)
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    return _page->inspectable();
#else
    return NO;
#endif
}

- (void)setInspectable:(BOOL)inspectable
{
    THROW_IF_SUSPENDED;
#if ENABLE(REMOTE_INSPECTOR)
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    _page->setInspectable(inspectable);
#endif
}

#pragma mark - iOS API

#if PLATFORM(IOS_FAMILY)

- (UIScrollView *)scrollView
{
    return _scrollView.get();
}

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

- (BOOL)findInteractionEnabled
{
    return _findInteractionEnabled;
}

- (void)setFindInteractionEnabled:(BOOL)enabled
{
#if HAVE(UIFINDINTERACTION)
    if (_findInteractionEnabled != enabled) {
        _findInteractionEnabled = enabled;

        if (enabled) {
            if (!_findInteraction)
                _findInteraction = adoptNS([[UIFindInteraction alloc] initWithSessionDelegate:self]);

            [self addInteraction:_findInteraction.get()];
        } else {
            [self removeInteraction:_findInteraction.get()];
            _findInteraction = nil;
        }
    }
#else
    UNUSED_PARAM(enabled);
    UNUSED_VARIABLE(_findInteractionEnabled);
#endif
}

- (UIFindInteraction *)findInteraction
{
#if HAVE(UIFINDINTERACTION)
    return _findInteraction.get();
#else
    return nil;
#endif
}

#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

#endif // PLATFORM(IOS_FAMILY)

#pragma mark - macOS API

#if PLATFORM(MAC)

- (void)setAllowsMagnification:(BOOL)allowsMagnification
{
    THROW_IF_SUSPENDED;
    _impl->setAllowsMagnification(allowsMagnification);
}

- (BOOL)allowsMagnification
{
    return _impl->allowsMagnification();
}

- (void)setMagnification:(double)magnification centeredAtPoint:(NSPoint)point
{
    THROW_IF_SUSPENDED;
    _impl->setMagnification(magnification, NSPointToCGPoint(point));
}

- (void)setMagnification:(double)magnification
{
    THROW_IF_SUSPENDED;
    _impl->setMagnification(magnification);
}

- (double)magnification
{
    return _impl->magnification();
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo
{
    THROW_IF_SUSPENDED;
    if (auto webFrameProxy = _page->mainFrame())
        return _impl->printOperationWithPrintInfo(printInfo, *webFrameProxy);
    return nil;
}

#endif // PLATFORM(MAC)

#pragma mark - macOS/iOS internal

- (void)_showWarningView:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    _warningView = adoptNS([[_WKWarningView alloc] initWithFrame:self.bounds browsingWarning:warning completionHandler:[weakSelf = WeakObjCPtr<WKWebView>(self), completionHandler = WTFMove(completionHandler)] (auto&& result) mutable {
        completionHandler(std::forward<decltype(result)>(result));
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;
        bool navigatesFrame = WTF::switchOn(result,
            [] (WebKit::ContinueUnsafeLoad continueUnsafeLoad) { return continueUnsafeLoad == WebKit::ContinueUnsafeLoad::Yes; },
            [] (const URL&) { return true; }
        );
        bool forMainFrameNavigation = [strongSelf->_warningView forMainFrameNavigation];
        if (navigatesFrame && forMainFrameNavigation) {
            // The safe browsing warning will be hidden once the next page is shown.
            return;
        }
        if (!navigatesFrame && strongSelf->_warningView && !forMainFrameNavigation) {
            strongSelf->_page->goBack();
            return;
        }
        [std::exchange(strongSelf->_warningView, nullptr) removeFromSuperview];
    }]);
    [self addSubview:_warningView.get()];
}

- (void)_showBrowsingWarning:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    [self _showWarningView:warning completionHandler:WTFMove(completionHandler)];
}

- (void)_clearWarningView
{
    [std::exchange(_warningView, nullptr) removeFromSuperview];
}

- (void)_clearBrowsingWarning
{
    [self _clearWarningView];
}

- (void)_clearWarningViewIfForMainFrameNavigation
{
    if ([_warningView forMainFrameNavigation])
        [self _clearWarningView];
}

- (void)_clearBrowsingWarningIfForMainFrameNavigation
{
    [self _clearWarningViewIfForMainFrameNavigation];
}

- (void)_internalDoAfterNextPresentationUpdate:(void (^)(void))updateBlock withoutWaitingForPainting:(BOOL)withoutWaitingForPainting withoutWaitingForAnimatedResize:(BOOL)withoutWaitingForAnimatedResize
{
#if PLATFORM(IOS_FAMILY)
    if (![self usesStandardContentView]) {
        RunLoop::main().dispatch([updateBlock = makeBlockPtr(updateBlock)] {
            updateBlock();
        });
        return;
    }
#endif

    if (withoutWaitingForPainting)
        _page->setShouldSkipWaitingForPaintAfterNextViewDidMoveToWindow(true);

    auto updateBlockCopy = makeBlockPtr(updateBlock);

    RetainPtr<WKWebView> strongSelf = self;
    _page->callAfterNextPresentationUpdate([updateBlockCopy, withoutWaitingForAnimatedResize, strongSelf] {
        if (!updateBlockCopy)
            return;

#if PLATFORM(IOS_FAMILY)
        if (!withoutWaitingForAnimatedResize && strongSelf->_perProcessState.dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
            strongSelf->_callbacksDeferredDuringResize.append([updateBlockCopy] {
                updateBlockCopy();
            });
            
            return;
        }
#else
        UNUSED_PARAM(withoutWaitingForAnimatedResize);
#endif

        updateBlockCopy();
    });
}

- (void)_doAfterNextVisibleContentRectAndPresentationUpdate:(void (^)(void))updateBlock
{
    [self _doAfterNextVisibleContentRectUpdate:makeBlockPtr([strongSelf = retainPtr(self), updateBlock = makeBlockPtr(updateBlock)] {
        [strongSelf _doAfterNextPresentationUpdate:updateBlock.get()];
    }).get()];
}

- (void)_recalculateViewportSizesWithMinimumViewportInset:(CocoaEdgeInsets)minimumViewportInset maximumViewportInset:(CocoaEdgeInsets)maximumViewportInset throwOnInvalidInput:(BOOL)throwOnInvalidInput
{
    auto frame = WebCore::FloatSize(self.frame.size);

#if PLATFORM(MAC)
    CGFloat additionalTopInset = self._topContentInset;
#else
    CGFloat additionalTopInset = 0;
#endif

    auto maximumViewportInsetSize = WebCore::FloatSize(maximumViewportInset.left + maximumViewportInset.right, maximumViewportInset.top + additionalTopInset + maximumViewportInset.bottom);
    auto minimumUnobscuredSize = frame - maximumViewportInsetSize;
    if (minimumUnobscuredSize.isEmpty()) {
        if (!maximumViewportInsetSize.isEmpty()) {
            if (throwOnInvalidInput) {
                [NSException raise:NSInvalidArgumentException format:@"maximumViewportInset cannot be larger than frame"];
                return;
            }

            RELEASE_LOG_ERROR(ViewportSizing, "maximumViewportInset cannot be larger than frame");
        }

        minimumUnobscuredSize = frame;
    }

    auto minimumViewportInsetSize = WebCore::FloatSize(minimumViewportInset.left + minimumViewportInset.right, minimumViewportInset.top + additionalTopInset + minimumViewportInset.bottom);
    auto maximumUnobscuredSize = frame - minimumViewportInsetSize;
    if (maximumUnobscuredSize.isEmpty()) {
        if (!minimumViewportInsetSize.isEmpty()) {
            if (throwOnInvalidInput) {
                [NSException raise:NSInvalidArgumentException format:@"minimumViewportInset cannot be larger than frame"];
                return;
            }

            RELEASE_LOG_ERROR(ViewportSizing, "minimumViewportInset cannot be larger than frame");
        }

        maximumUnobscuredSize = frame;
    }

#if PLATFORM(IOS_FAMILY)
    if (_overriddenLayoutParameters)
        return;
#endif

    _page->setMinimumUnobscuredSize(minimumUnobscuredSize);
    _page->setMaximumUnobscuredSize(maximumUnobscuredSize);
}

#if PLATFORM(MAC) && HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)
- (void)_invalidateWindowSnapshotReadinessHandler
{
    if (auto handler = std::exchange(_windowSnapshotReadinessHandler, nil))
        handler();
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

- (void)_didInsertAttachment:(API::Attachment&)attachment withSource:(NSString *)source
{
    id <WKUIDelegatePrivate> uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:didInsertAttachment:withSource:)])
        [uiDelegate _webView:self didInsertAttachment:wrapper(attachment) withSource:source];
}

- (void)_didRemoveAttachment:(API::Attachment&)attachment
{
    id <WKUIDelegatePrivate> uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:didRemoveAttachment:)])
        [uiDelegate _webView:self didRemoveAttachment:wrapper(attachment)];
}

- (void)_didInvalidateDataForAttachment:(API::Attachment&)attachment
{
    id <WKUIDelegatePrivate> uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:didInvalidateDataForAttachment:)])
        [uiDelegate _webView:self didInvalidateDataForAttachment:wrapper(attachment)];
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

- (id <_WKAppHighlightDelegate>)_appHighlightDelegate
{
#if ENABLE(APP_HIGHLIGHTS)
    return _appHighlightDelegate.getAutoreleased();
#else
    return nil;
#endif
}

- (void)_setAppHighlightDelegate:(id <_WKAppHighlightDelegate>)delegate
{
#if ENABLE(APP_HIGHLIGHTS)
    _appHighlightDelegate = delegate;
#endif
}

#if ENABLE(APP_HIGHLIGHTS)
- (void)_storeAppHighlight:(const WebCore::AppHighlight&)highlight
{
    auto delegate = self._appHighlightDelegate;
    if (!delegate)
        return;

    if (![delegate respondsToSelector:@selector(_webView:storeAppHighlight:inNewGroup:requestOriginatedInApp:)])
        return;

    NSString *text = nil;

    if (highlight.text)
        text = highlight.text.value();

    auto wkHighlight = adoptNS([[_WKAppHighlight alloc] initWithHighlight:highlight.highlight->makeContiguous()->createNSData().get() text:text image:nil]);

    if ([delegate respondsToSelector:@selector(_webView:storeAppHighlight:inNewGroup:requestOriginatedInApp:)])
        [delegate _webView:self storeAppHighlight:wkHighlight.get() inNewGroup:highlight.isNewGroup == WebCore::CreateNewGroupForHighlight::Yes requestOriginatedInApp:highlight.requestOriginatedInApp == WebCore::HighlightRequestOriginatedInApp::Yes];
}
#endif

- (WKPageRef)_pageForTesting
{
    return toAPI(_page.get());
}

- (NakedPtr<WebKit::WebPageProxy>)_page
{
    return _page.get();
}

- (RefPtr<WebKit::WebPageProxy>)_protectedPage
{
    return _page.get();
}

- (std::optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting
{
    return _resolutionForShareSheetImmediateCompletionForTesting;
}

- (void)createPDFWithConfiguration:(WKPDFConfiguration *)pdfConfiguration completionHandler:(void (^)(NSData *pdfDocumentData, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    auto frameID = _page->mainFrame() ? std::optional { _page->mainFrame()->frameID() } : std::nullopt;
    if (!frameID) {
        completionHandler(nil, createNSError(WKErrorUnknown).get());
        return;
    }

    std::optional<WebCore::FloatRect> floatRect;
    if (pdfConfiguration && !CGRectIsNull(pdfConfiguration.rect))
        floatRect = WebCore::FloatRect(pdfConfiguration.rect);

    bool allowTransparentBackground = pdfConfiguration && pdfConfiguration.allowTransparentBackground;

    _page->drawToPDF(*frameID, floatRect, allowTransparentBackground, [handler = makeBlockPtr(completionHandler)](RefPtr<WebCore::SharedBuffer>&& pdfData) {
        if (!pdfData || pdfData->isEmpty()) {
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }

        auto data = pdfData->createCFData();
        handler((NSData *)data.get(), nil);
    });
}

- (void)createWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getWebArchiveOfFrame(nullptr, [completionHandler = makeBlockPtr(completionHandler)](API::Data* data) {
        if (data)
            completionHandler(wrapper(data), nil);
        else
            completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);
    });
}

static NSDictionary *dictionaryRepresentationForEditorState(const WebKit::EditorState& state)
{
    if (!state.hasPostLayoutData())
        return @{ @"post-layout-data" : @NO };

    auto& postLayoutData = *state.postLayoutData;
    return @{
        @"post-layout-data" : @YES,
        @"bold": postLayoutData.typingAttributes.contains(WebKit::TypingAttribute::Bold) ? @YES : @NO,
        @"italic": postLayoutData.typingAttributes.contains(WebKit::TypingAttribute::Italics) ? @YES : @NO,
        @"underline": postLayoutData.typingAttributes.contains(WebKit::TypingAttribute::Underline) ? @YES : @NO,
        @"text-alignment": @(nsTextAlignment(static_cast<WebKit::TextAlignment>(postLayoutData.textAlignment))),
        @"text-color": (NSString *)serializationForCSS(postLayoutData.textColor)
    };
}

static NSTextAlignment nsTextAlignment(WebKit::TextAlignment alignment)
{
    switch (alignment) {
    case WebKit::TextAlignment::Natural:
        return NSTextAlignmentNatural;
    case WebKit::TextAlignment::Left:
        return NSTextAlignmentLeft;
    case WebKit::TextAlignment::Right:
        return NSTextAlignmentRight;
    case WebKit::TextAlignment::Center:
        return NSTextAlignmentCenter;
    case WebKit::TextAlignment::Justified:
        return NSTextAlignmentJustified;
    }
    ASSERT_NOT_REACHED();
    return NSTextAlignmentNatural;
}

static _WKSelectionAttributes selectionAttributes(const WebKit::EditorState& editorState, _WKSelectionAttributes previousAttributes)
{
    _WKSelectionAttributes attributes = _WKSelectionAttributeNoSelection;
    if (editorState.selectionIsNone)
        return attributes;

    if (editorState.selectionIsRange)
        attributes |= _WKSelectionAttributeIsRange;
    else
        attributes |= _WKSelectionAttributeIsCaret;

    return attributes;
}

- (void)_didChangeEditorState
{
    auto newSelectionAttributes = selectionAttributes(_page->editorState(), _selectionAttributes);
    if (_selectionAttributes != newSelectionAttributes) {
        NSString *selectionAttributesKey = NSStringFromSelector(@selector(_selectionAttributes));
        [self willChangeValueForKey:selectionAttributesKey];
        _selectionAttributes = newSelectionAttributes;
        [self didChangeValueForKey:selectionAttributesKey];
    }

    // FIXME: We should either rename -_webView:editorStateDidChange: to clarify that it's only intended for use when testing,
    // or remove it entirely and use -_webView:didChangeFontAttributes: instead once text alignment is supported in the set of
    // font attributes.
    id <WKUIDelegatePrivate> uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:editorStateDidChange:)])
        [uiDelegate _webView:self editorStateDidChange:dictionaryRepresentationForEditorState(_page->editorState())];
}

- (WKNavigation *)loadSimulatedRequest:(NSURLRequest *)request response:(NSURLResponse *)response responseData:(NSData *)data
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->loadSimulatedRequest(request, response, WebCore::SharedBuffer::create(data))).autorelease();
}

// FIXME(223658): Remove this once adopters have moved to the final API.
- (WKNavigation *)loadSimulatedRequest:(NSURLRequest *)request withResponse:(NSURLResponse *)response responseData:(NSData *)data
{
    THROW_IF_SUSPENDED;
    return [self loadSimulatedRequest:request response:response responseData:data];
}

- (WKNavigation *)loadSimulatedRequest:(NSURLRequest *)request responseHTMLString:(NSString *)string
{
    THROW_IF_SUSPENDED;
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    auto response = adoptNS([[NSURLResponse alloc] initWithURL:request.URL MIMEType:@"text/html" expectedContentLength:string.length textEncodingName:@"UTF-8"]);

    return [self loadSimulatedRequest:request response:response.get() responseData:data];
}

// FIXME(223658): Remove this once adopters have moved to the final API.
- (WKNavigation *)loadSimulatedRequest:(NSURLRequest *)request withResponseHTMLString:(NSString *)string
{
    THROW_IF_SUSPENDED;
    return [self loadSimulatedRequest:request responseHTMLString:string];
}

- (WKNavigation *)loadFileRequest:(NSURLRequest *)request allowingReadAccessToURL:(NSURL *)readAccessURL
{
    THROW_IF_SUSPENDED;
    auto URL = request.URL;

    if (![URL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", URL];

    if (![readAccessURL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", readAccessURL];

    bool isAppInitiated = true;
#if ENABLE(APP_PRIVACY_REPORT)
    isAppInitiated = request.attribution == NSURLRequestAttributionDeveloper;
#endif

    return wrapper(_page->loadFile(URL.absoluteString, readAccessURL.absoluteString, isAppInitiated)).autorelease();
}

- (WebCore::CocoaColor *)themeColor
{
    return cocoaColorOrNil(_page->themeColor()).autorelease();
}

- (WebCore::CocoaColor *)underPageBackgroundColor
{
    return cocoaColor(_page->underPageBackgroundColor()).autorelease();
}

- (void)setUnderPageBackgroundColor:(WebCore::CocoaColor *)underPageBackgroundColorOverride
{
    _page->setUnderPageBackgroundColorOverride(WebCore::roundAndClampToSRGBALossy(underPageBackgroundColorOverride.CGColor));
}

+ (BOOL)automaticallyNotifiesObserversOfUnderPageBackgroundColor
{
    return NO;
}

- (WKFullscreenState)fullscreenState
{
#if ENABLE(FULLSCREEN_API)
    auto* fullscreenManager = _page->fullScreenManager();
    if (!fullscreenManager)
        return WKFullscreenStateNotInFullscreen;

    WKFullscreenState state = WKFullscreenStateNotInFullscreen;
    switch (fullscreenManager->fullscreenState()) {
    case WebKit::WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen:
        state = WKFullscreenStateEnteringFullscreen;
        break;
    case WebKit::WebFullScreenManagerProxy::FullscreenState::InFullscreen:
        state = WKFullscreenStateInFullscreen;
        break;
    case WebKit::WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen:
        state = WKFullscreenStateExitingFullscreen;
        break;
    default:
        state = WKFullscreenStateNotInFullscreen;
        break;
    }

    return state;
#else
    return WKFullscreenStateNotInFullscreen;
#endif
}

- (void)setMinimumViewportInset:(CocoaEdgeInsets)minimumViewportInset maximumViewportInset:(CocoaEdgeInsets)maximumViewportInset
{
    if (minimumViewportInset.top < 0 || minimumViewportInset.left < 0 || minimumViewportInset.bottom < 0 || minimumViewportInset.right < 0)
        [NSException raise:NSInvalidArgumentException format:@"minimumViewportInset cannot be negative"];

    if (maximumViewportInset.top < 0 || maximumViewportInset.left < 0 || maximumViewportInset.bottom < 0 || maximumViewportInset.right < 0)
        [NSException raise:NSInvalidArgumentException format:@"maximumViewportInset cannot be negative"];

    if (minimumViewportInset.top + minimumViewportInset.bottom > maximumViewportInset.top + maximumViewportInset.bottom || minimumViewportInset.right + minimumViewportInset.left > maximumViewportInset.right + maximumViewportInset.left)
        [NSException raise:NSInvalidArgumentException format:@"minimumViewportInset cannot be larger than maximumViewportInset"];

    [self _recalculateViewportSizesWithMinimumViewportInset:minimumViewportInset maximumViewportInset:maximumViewportInset throwOnInvalidInput:YES];

    _minimumViewportInset = minimumViewportInset;
    _maximumViewportInset = maximumViewportInset;
}

#if ENABLE(WRITING_TOOLS)

#pragma mark - Writing Tools API

- (BOOL)isWritingToolsActive
{
#if ENABLE(WRITING_TOOLS)
    return _page->isWritingToolsActive();
#else
    return NO;
#endif
}

#pragma mark - WTWritingToolsDelegate conformance

- (PlatformWritingToolsResultOptions)allowedWritingToolsResultOptions
{
    auto& editorState = _page->editorState();
    if (editorState.isContentEditable && !editorState.isContentRichlyEditable)
        return PlatformWritingToolsResultPlainText;

    return PlatformWritingToolsResultPlainText | PlatformWritingToolsResultRichText | PlatformWritingToolsResultList | PlatformWritingToolsResultTable;
}

// FIXME: (rdar://130540028) Remove uses of the old WritingToolsAllowedInputOptions API in favor of the new WritingToolsResultOptions API, and remove staging.
- (PlatformWritingToolsResultOptions)writingToolsAllowedInputOptions
{
    return [self allowedWritingToolsResultOptions];
}

- (PlatformWritingToolsBehavior)writingToolsBehavior
{
    return WebKit::convertToPlatformWritingToolsBehavior(_page->writingToolsBehavior());
}

- (void)willBeginWritingToolsSession:(WTSession *)session requestContexts:(void (^)(NSArray<WTContext *> *))completion
{
    auto webSession = WebKit::convertToWebSession(session);

    if (session) {
        _activeWritingToolsSession = session;
        _page->setWritingToolsActive(true);
    }

    _page->willBeginWritingToolsSession(webSession, [completion = makeBlockPtr(completion)](const auto& contextData) {
        auto contexts = [NSMutableArray arrayWithCapacity:contextData.size()];
        for (auto& context : contextData) {
            auto platformContext = WebKit::convertToPlatformContext(context);
            [contexts addObject:platformContext.get()];
        }
        completion(contexts);
    });
}

- (void)didBeginWritingToolsSession:(WTSession *)session contexts:(NSArray<WTContext *> *)contexts
{
    auto webSession = WebKit::convertToWebSession(session);
    if (!webSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<WebCore::WritingTools::Context> contextData;
    for (WTContext *context in contexts) {
        auto webContext = WebKit::convertToWebContext(context);
        if (!webContext) {
            ASSERT_NOT_REACHED();
            return;
        }

        contextData.append(*webContext);
    }

    _page->didBeginWritingToolsSession(*webSession, contextData);

    if (session.type == WTSessionTypeProofreading) {
        _intelligenceTextEffectCoordinator = adoptNS([WebKit::allocWKIntelligenceTextEffectCoordinatorInstance() initWithDelegate:(id<WKIntelligenceTextEffectCoordinatorDelegate>)self]);
        [_intelligenceTextEffectCoordinator startAnimationForRange:contexts.firstObject.range completion:^{ }];
    }
}

- (void)proofreadingSession:(WTSession *)session didReceiveSuggestions:(NSArray<WTTextSuggestion *> *)suggestions processedRange:(NSRange)range inContext:(WTContext *)context finished:(BOOL)finished
{
    auto webSession = WebKit::convertToWebSession(session);
    if (!webSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webContext = WebKit::convertToWebContext(context);
    if (!webContext) {
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<WebCore::WritingTools::TextSuggestion> replacementData;
    for (WTTextSuggestion *suggestion in suggestions) {
        auto replacementDataItem = WebKit::convertToWebTextSuggestion(suggestion);
        if (!replacementDataItem) {
            ASSERT_NOT_REACHED();
            continue;
        }

        replacementData.append(*replacementDataItem);

        [_writingToolsTextSuggestions setObject:suggestion forKey:suggestion.uuid];
    }

    NSInteger delta = [WebKit::getWKIntelligenceTextEffectCoordinatorClass() characterDeltaForReceivedSuggestions:suggestions];

    auto operation = makeBlockPtr([webSession, replacementData, range, webContext, finished, weakSelf = WeakObjCPtr<WKWebView>(self)](void (^completion)(void)) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completion();
            return;
        }

        strongSelf->_page->proofreadingSessionDidReceiveSuggestions(*webSession, replacementData, range, *webContext, finished, [completion = makeBlockPtr(completion)] {
            completion();
        });
    });

    [_intelligenceTextEffectCoordinator requestReplacementWithProcessedRange:range finished:finished characterDelta:delta operation:operation.get() completion:^{ }];
}

- (void)proofreadingSession:(WTSession *)session didUpdateState:(WTTextSuggestionState)state forSuggestionWithUUID:(NSUUID *)suggestionUUID inContext:(WTContext *)context
{
    auto webSession = WebKit::convertToWebSession(session);
    if (!webSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webContext = WebKit::convertToWebContext(context);
    if (!webContext) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webTextSuggestionState = WebKit::convertToWebTextSuggestionState(state);

    WTTextSuggestion *suggestion = [_writingToolsTextSuggestions objectForKey:suggestionUUID];
    if (!suggestion) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webTextSuggestion = WebKit::convertToWebTextSuggestion(suggestion);
    if (!webTextSuggestion) {
        ASSERT_NOT_REACHED();
        return;
    }

    _page->proofreadingSessionDidUpdateStateForSuggestion(*webSession, webTextSuggestionState, *webTextSuggestion, *webContext);
}

- (void)didEndWritingToolsSession:(WTSession *)session accepted:(BOOL)accepted
{
    auto webSession = WebKit::convertToWebSession(session);
    if (!webSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    _activeWritingToolsSession = nil;
    [_writingToolsTextSuggestions removeAllObjects];

    if (session.type != WTSessionTypeProofreading) {
        _page->setWritingToolsActive(false);
        _page->didEndWritingToolsSession(*webSession, accepted);
        return;
    }

    // Flush and invoke all replacement operations, then dismiss the markers (and revert the text if not accepted),
    // then set the selection to the updated context range, and finally clear the state in the web process.
    //
    // It's possible that the selection has already been restored by this point if the entire animation has already
    // finished, but this is not guaranteed.

    [_intelligenceTextEffectCoordinator flushReplacementsWithCompletion:makeBlockPtr([webSession, accepted, weakSelf = WeakObjCPtr<WKWebView>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        strongSelf->_page->setWritingToolsActive(false);

        strongSelf->_page->willEndWritingToolsSession(*webSession, accepted, [webSession, accepted, weakSelf] {
            [weakSelf.get()->_intelligenceTextEffectCoordinator restoreSelectionAcceptedReplacements:accepted completion:makeBlockPtr([webSession, accepted, weakSelf] {
                weakSelf.get()->_page->didEndWritingToolsSession(*webSession, accepted);
            }).get()];
        });
    }).get()];
}

- (void)compositionSession:(WTSession *)session didReceiveText:(NSAttributedString *)attributedText replacementRange:(NSRange)range inContext:(WTContext *)context finished:(BOOL)finished
{
    auto webSession = WebKit::convertToWebSession(session);
    if (!webSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webContext = WebKit::convertToWebContext(context);
    if (!webContext) {
        ASSERT_NOT_REACHED();
        return;
    }

    _writingToolsTextReplacementsFinished = finished;
    _partialIntelligenceTextAnimationCount += 1;

    _page->compositionSessionDidReceiveTextWithReplacementRange(*webSession, WebCore::AttributedString::fromNSAttributedString(attributedText), { range }, *webContext, finished);
}

- (void)writingToolsSession:(WTSession *)session didReceiveAction:(WTAction)action
{
    auto webSession = WebKit::convertToWebSession(session);
    if (!webSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webAction = WebKit::convertToWebAction(action);

    if (webAction == WebCore::WritingTools::Action::Restart) {
        _writingToolsTextReplacementsFinished = false;
        _partialIntelligenceTextAnimationCount = 0;
    }

    _page->writingToolsSessionDidReceiveAction(*webSession, webAction);
}

#pragma mark - WKIntelligenceTextEffectCoordinatorDelegate conformance

#if !PLATFORM(APPLETV) && !PLATFORM(WATCHOS)

#if PLATFORM(IOS_FAMILY)
- (UIView *)viewForIntelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator
{
    return _contentView.get();
}
#else
- (NSView *)viewForIntelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator
{
    return self;
}
#endif

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator rectsForProofreadingSuggestionsInRange:(NSRange)range completion:(void (^)(NSArray<NSValue *> *))completion
{
    _page->proofreadingSessionSuggestionTextRectsInRootViewCoordinates(range, [completion = makeBlockPtr(completion)](auto&& rects) {
        RetainPtr nsArray = createNSArray(rects, [](auto& rect) {
            return [NSValue valueWithRect:rect];
        });

        completion(nsArray.get());
    });
}

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator updateTextVisibilityForRange:(NSRange)range visible:(BOOL)visible identifier:(NSUUID *)identifier completion:(void (^)(void))completion
{
    auto convertedIdentifier = WTF::UUID::fromNSUUID(identifier);
    if (!convertedIdentifier) {
        ASSERT_NOT_REACHED();
        completion();
        return;
    }

    _page->updateTextVisibilityForActiveWritingToolsSession(range, visible, *convertedIdentifier, [completion = makeBlockPtr(completion), weakSelf = WeakObjCPtr<WKWebView>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completion();
            return;
        }

        // Ensure the rendering of the visibility has actually taken effect.
        [strongSelf _doAfterNextPresentationUpdate:completion.get()];
    });
}

#if PLATFORM(IOS_FAMILY)
- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator textPreviewsForRange:(NSRange)range completion:(void (^)(UITargetedPreview *))completion
{
    _page->textPreviewDataForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion), weakSelf = WeakObjCPtr<WKWebView>(self)](auto&& textIndicatorData) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completion(nil);
            return;
        }

        if (!textIndicatorData) {
            completion(nil);
            return;
        }

        RetainPtr preview = [strongSelf->_contentView _createTargetedPreviewFromTextIndicator:*textIndicatorData previewContainer:strongSelf.get()];
        completion(preview.get());
    });
}
#else
- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator textPreviewsForRange:(NSRange)range completion:(void (^)(NSArray<_WKTextPreview *> *))completion
{
    // FIXME: This logic is currently duplicated in a bunch of places; it should be unified.
    _page->textPreviewDataForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion)](auto&& textIndicatorData) {
        if (!textIndicatorData) {
            completion(@[ ]);
            return;
        }

        RefPtr contentImage = textIndicatorData->contentImage;
        if (!contentImage) {
            ASSERT_NOT_REACHED();
            completion(@[ ]);
            return;
        }

        RefPtr nativeImage = contentImage->nativeImage();
        if (!nativeImage) {
            ASSERT_NOT_REACHED();
            completion(@[ ]);
            return;
        }

        RetainPtr platformImage = nativeImage->platformImage();

        auto textBoundingRectInRootViewCoordinates = textIndicatorData->textBoundingRectInRootViewCoordinates;
        auto textRectsInBoundingRectCoordinates = textIndicatorData->textRectsInBoundingRectCoordinates;
        auto contentImageScaleFactor = textIndicatorData->contentImageScaleFactor;

        RetainPtr previews = createNSArray(textRectsInBoundingRectCoordinates, [platformImage, textBoundingRectInRootViewCoordinates, contentImageScaleFactor](auto& textRectInBoundingRectCoordinates) -> _WKTextPreview * {

            auto croppedTextRectInImageCoordinates = textRectInBoundingRectCoordinates;
            croppedTextRectInImageCoordinates.scale(contentImageScaleFactor);

            RetainPtr textImage = adoptCF(CGImageCreateWithImageInRect(platformImage.get(), croppedTextRectInImageCoordinates));

            auto presentationFrame = CGRectOffset(textRectInBoundingRectCoordinates, textBoundingRectInRootViewCoordinates.x(), textBoundingRectInRootViewCoordinates.y());

            RetainPtr textPreview = adoptNS([[_WKTextPreview alloc] initWithSnapshotImage:textImage.get() presentationFrame:presentationFrame]);
            return textPreview.autorelease();
        });

        completion(previews.get());
    });
}
#endif

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator decorateReplacementsForRange:(NSRange)range completion:(void (^)(void))completion
{
    _page->decorateTextReplacementsForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion)] {
        completion();
    });
}

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator setSelectionForRange:(NSRange)range completion:(void (^)(void))completion
{
    _page->setSelectionForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion)] {
        completion();
    });
}

#endif

#pragma mark - WTTextViewDelegate invoking methods

- (void)_proofreadingSessionShowDetailsForSuggestionWithUUID:(NSUUID *)replacementUUID relativeToRect:(CGRect)rect
{
    if (!_activeWritingToolsSession)
        return;

    auto textViewDelegate = (NSObject<WTTextViewDelegate> *)[_activeWritingToolsSession textViewDelegate];

    if (![textViewDelegate respondsToSelector:@selector(proofreadingSessionWithUUID:showDetailsForSuggestionWithUUID:relativeToRect:inView:)])
        return;

#if PLATFORM(MAC)
    RetainPtr view = self;
#else
    RetainPtr view = _contentView;
#endif

    [textViewDelegate proofreadingSessionWithUUID:[_activeWritingToolsSession uuid] showDetailsForSuggestionWithUUID:replacementUUID relativeToRect:rect inView:view.get()];
}

- (void)_proofreadingSessionUpdateState:(WebCore::WritingTools::TextSuggestion::State)state forSuggestionWithUUID:(NSUUID *)replacementUUID
{
    if (!_activeWritingToolsSession)
        return;

    auto textViewDelegate = (NSObject<WTTextViewDelegate> *)[_activeWritingToolsSession textViewDelegate];

    if (![textViewDelegate respondsToSelector:@selector(proofreadingSessionWithUUID:updateState:forSuggestionWithUUID:)])
        return;

    [textViewDelegate proofreadingSessionWithUUID:[_activeWritingToolsSession uuid] updateState:WebKit::convertToPlatformTextSuggestionState(state) forSuggestionWithUUID:replacementUUID];
}

- (void)_didEndPartialIntelligenceTextAnimation
{
    if (!_partialIntelligenceTextAnimationCount) {
        ASSERT_NOT_REACHED();
        return;
    }

    _partialIntelligenceTextAnimationCount -= 1;

    if (!_partialIntelligenceTextAnimationCount && _writingToolsTextReplacementsFinished) {
        // If the entire replacement has already been completed, and this is the end of the last animation,
        // then reveal the selection and end the session if needed.
        _page->intelligenceTextAnimationsDidComplete();
    }
}

- (BOOL)_writingToolsTextReplacementsFinished
{
    return _writingToolsTextReplacementsFinished;
}

- (void)_addTextAnimationForAnimationID:(NSUUID *)nsUUID withData:(const WebCore::TextAnimationData&)data
{
#if PLATFORM(IOS_FAMILY)
    [_contentView addTextAnimationForAnimationID:nsUUID withData:data];
#else
    auto uuid = WTF::UUID::fromNSUUID(nsUUID);
    if (!uuid)
        return;

    _impl->addTextAnimationForAnimationID(*uuid, data);
#endif
}

- (void)_removeTextAnimationForAnimationID:(NSUUID *)nsUUID
{
#if PLATFORM(IOS_FAMILY)
    [_contentView removeTextAnimationForAnimationID:nsUUID];
#else
    auto uuid = WTF::UUID::fromNSUUID(nsUUID);
    if (!uuid)
        return;

    _impl->removeTextAnimationForAnimationID(*uuid);
#endif
}

#endif

#if ENABLE(GAMEPAD)

- (void)_setGamepadsRecentlyAccessed:(BOOL)gamepadsRecentlyAccessed
{
#if PLATFORM(VISION)
    id<WKUIDelegatePrivate> uiDelegate = (id<WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:setRecentlyAccessedGamepads:)])
        [uiDelegate _webView:self setRecentlyAccessedGamepads:gamepadsRecentlyAccessed];
#endif
}

#if PLATFORM(VISION)
- (BOOL)_gamepadsConnected
{
    return _page->gamepadsConnected();
}

- (void)_gamepadsConnectedStateChanged
{
    id<WKUIDelegatePrivate> uiDelegate = (id<WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:gamepadsConnectedStateDidChange:)])
        [uiDelegate _webView:self gamepadsConnectedStateDidChange:_page->gamepadsConnected()];
}

- (void)_setAllowGamepadsAccess
{
    _page->allowGamepadAccess();
}

- (void)_setAllowGamepadsInput:(BOOL)allowGamepadsInput
{
#if __has_include(<GameController/GCEventInteraction.h>)
    if (allowGamepadsInput == !!_gamepadsRecentlyAccessedState)
        return;

    if (allowGamepadsInput) {
        _gamepadsRecentlyAccessedState = adoptNS([[WebCore::getGCEventInteractionClass() alloc] init]);
        ((GCEventInteraction *)_gamepadsRecentlyAccessedState.get()).handledEventTypes = GCUIEventTypeGamepad;
        [self addInteraction:(id<UIInteraction>)_gamepadsRecentlyAccessedState.get()];
    } else {
        [self removeInteraction:(id<UIInteraction>)_gamepadsRecentlyAccessedState.get()];
        _gamepadsRecentlyAccessedState = nil;
    }
#endif // __has_include(<GameController/GCEventInteraction.h>)
}

#endif // PLATFORM(VISION)
#endif // ENABLE(GAMEPAD)

@end

#pragma mark -

@implementation WKWebView (WKPrivate)

#pragma mark - macOS WKPrivate

#if PLATFORM(MAC)

#define WEBCORE_PRIVATE_COMMAND(command) - (void)_##command:(id)sender { THROW_IF_SUSPENDED; _page->executeEditCommand(#command ## _s); }

WEBCORE_PRIVATE_COMMAND(alignCenter)
WEBCORE_PRIVATE_COMMAND(alignJustified)
WEBCORE_PRIVATE_COMMAND(alignLeft)
WEBCORE_PRIVATE_COMMAND(alignRight)
WEBCORE_PRIVATE_COMMAND(insertOrderedList)
WEBCORE_PRIVATE_COMMAND(insertUnorderedList)
WEBCORE_PRIVATE_COMMAND(insertNestedOrderedList)
WEBCORE_PRIVATE_COMMAND(insertNestedUnorderedList)
WEBCORE_PRIVATE_COMMAND(indent)
WEBCORE_PRIVATE_COMMAND(outdent)
WEBCORE_PRIVATE_COMMAND(pasteAsQuotation)
WEBCORE_PRIVATE_COMMAND(pasteAndMatchStyle)

#undef WEBCORE_PRIVATE_COMMAND

- (void)_toggleStrikeThrough:(id)sender
{
    THROW_IF_SUSPENDED;
    _page->executeEditCommand("strikethrough"_s);
}

- (void)_increaseListLevel:(id)sender
{
    THROW_IF_SUSPENDED;
    _page->increaseListLevel();
}

- (void)_decreaseListLevel:(id)sender
{
    THROW_IF_SUSPENDED;
    _page->decreaseListLevel();
}

- (void)_changeListType:(id)sender
{
    THROW_IF_SUSPENDED;
    _page->changeListType();
}

#endif // PLATFORM(MAC)

#pragma mark - iOS WKPrivate

#if PLATFORM(IOS_FAMILY)

#define FORWARD_ACTION_TO_WKCONTENTVIEW(_action) \
- (void)_action:(id)sender \
{ \
    if (self.usesStandardContentView) \
        [_contentView _action ## ForWebView:sender]; \
}

FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(FORWARD_ACTION_TO_WKCONTENTVIEW)

#undef FORWARD_ACTION_TO_WKCONTENTVIEW

- (UIView *)inputAccessoryView
{
    return [_contentView inputAccessoryViewForWebView];
}

- (UIView *)inputView
{
    return [_contentView inputViewForWebView];
}

- (UITextInputAssistantItem *)inputAssistantItem
{
    return [_contentView inputAssistantItemForWebView];
}

#endif // PLATFORM(IOS_FAMILY)

#pragma mark - macOS/iOS WKPrivate

- (_WKSelectionAttributes)_selectionAttributes
{
    return _selectionAttributes;
}

- (CGSize)_viewportSizeForCSSViewportUnits
{
    return _page->viewportSizeForCSSViewportUnits();
}

- (void)_setViewportSizeForCSSViewportUnits:(CGSize)viewportSize
{
    THROW_IF_SUSPENDED;
    auto viewportSizeForViewportUnits = WebCore::FloatSize(viewportSize);
    if (viewportSizeForViewportUnits.isEmpty())
        [NSException raise:NSInvalidArgumentException format:@"Viewport size should not be empty"];

    _page->setViewportSizeForCSSViewportUnits(viewportSizeForViewportUnits);
}

- (BOOL)_isBeingInspected
{
    return _page && _page->hasInspectorFrontend();
}

- (_WKInspector *)_inspector
{
    if (auto* inspector = _page->inspector())
        return wrapper(*inspector);
    return nil;
}

- (void)_didEnableBrowserExtensions:(NSDictionary<NSString *, NSString *> *)extensionIDToNameMap
{
    THROW_IF_SUSPENDED;
    HashMap<String, String> transformed;
    transformed.reserveInitialCapacity(extensionIDToNameMap.count);
    [extensionIDToNameMap enumerateKeysAndObjectsUsingBlock:[&](NSString *extensionID, NSString *extensionName, BOOL *) {
        transformed.set(extensionID, extensionName);
    }];
    _page->inspectorController().browserExtensionsEnabled(WTFMove(transformed));
}

- (void)_didDisableBrowserExtensions:(NSSet<NSString *> *)extensionIDs
{
    THROW_IF_SUSPENDED;
    HashSet<String> transformed;
    transformed.reserveInitialCapacity(extensionIDs.count);
    for (NSString *extensionID in extensionIDs)
        transformed.addVoid(extensionID);
    _page->inspectorController().browserExtensionsDisabled(WTFMove(transformed));
}

- (_WKFrameHandle *)_mainFrame
{
    if (auto* frame = _page->mainFrame())
        return wrapper(API::FrameHandle::create(frame->frameID())).autorelease();
    return nil;
}

- (BOOL)_negotiatedLegacyTLS
{
    return _page->pageLoadState().hasNegotiatedLegacyTLS();
}

- (BOOL)_wasPrivateRelayed
{
    return _page->pageLoadState().wasPrivateRelayed();
}

- (void)_frames:(void (^)(_WKFrameTreeNode *))completionHandler
{
    _page->getAllFrames([completionHandler = makeBlockPtr(completionHandler), page = Ref { *_page.get() }] (WebKit::FrameTreeNodeData&& data) {
        completionHandler(wrapper(API::FrameTreeNode::create(WTFMove(data), page.get())).get());
    });
}

- (void)_frameTrees:(void (^)(NSSet<_WKFrameTreeNode *> *))completionHandler
{
    _page->getAllFrameTrees([completionHandler = makeBlockPtr(completionHandler), page = Ref { *_page.get() }] (Vector<WebKit::FrameTreeNodeData>&& vector) {
        auto set = adoptNS([[NSMutableSet alloc] initWithCapacity:vector.size()]);
        for (auto& data : vector)
            [set addObject:wrapper(API::FrameTreeNode::create(WTFMove(data), page.get())).get()];
        completionHandler(set.get());
    });
}

- (BOOL)_isEditable
{
    return _page && _page->isEditable();
}

- (void)_setEditable:(BOOL)editable
{
    THROW_IF_SUSPENDED;
    bool wasEditable = _page->isEditable();
    _page->setEditable(editable);
#if PLATFORM(MAC)
    if (editable)
        _impl->didBecomeEditable();
#endif

    if (wasEditable == editable)
        return;

#if PLATFORM(IOS_FAMILY)
    [_contentView _didChangeWebViewEditability];
#endif
}

- (void)_executeEditCommand:(NSString *)command argument:(NSString *)argument completion:(void (^)(BOOL))completion
{
    THROW_IF_SUSPENDED;
    _page->executeEditCommand(command, argument, [capturedCompletionBlock = makeBlockPtr(completion)] {
        if (capturedCompletionBlock)
            capturedCompletionBlock(YES);
    });
}

- (id <_WKTextManipulationDelegate>)_textManipulationDelegate
{
    return _textManipulationDelegate.getAutoreleased();
}

- (void)_setTextManipulationDelegate:(id <_WKTextManipulationDelegate>)delegate
{
    _textManipulationDelegate = delegate;
}

static RetainPtr<NSDictionary<NSString *, id>> createUserInfo(const std::optional<WebCore::TextManipulationTokenInfo>& info)
{
    if (!info)
        return { };

    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:3]);
    if (!info->documentURL.isNull())
        [result setObject:(NSURL *)info->documentURL forKey:_WKTextManipulationTokenUserInfoDocumentURLKey];
    if (!info->tagName.isNull())
        [result setObject:(NSString *)info->tagName forKey:_WKTextManipulationTokenUserInfoTagNameKey];
    if (!info->roleAttribute.isNull())
        [result setObject:(NSString *)info->roleAttribute forKey:_WKTextManipulationTokenUserInfoRoleAttributeKey];
    [result setObject:@(info->isVisible) forKey:_WKTextManipulationTokenUserInfoVisibilityKey];

    return result;
}

- (void)_startTextManipulationsWithConfiguration:(_WKTextManipulationConfiguration *)configuration completion:(void(^)())completionHandler
{
    THROW_IF_SUSPENDED;
    using ExclusionRule = WebCore::TextManipulationController::ExclusionRule;
    bool includeSubframes = configuration.includeSubframes;

    if (!_textManipulationDelegate || !_page) {
        completionHandler();
        return;
    }

    auto* frame = _page->mainFrame();
    if (!frame) {
        completionHandler();
        return;
    }

    Vector<WebCore::TextManipulationController::ExclusionRule> exclusionRules;
    if (configuration) {
        for (_WKTextManipulationExclusionRule *wkRule in configuration.exclusionRules) {
            auto type = wkRule.isExclusion ? ExclusionRule::Type::Exclude : ExclusionRule::Type::Include;
            if (wkRule.attributeName)
                exclusionRules.append({type, ExclusionRule::AttributeRule { wkRule.attributeName, wkRule.attributeValue } });
            else if (wkRule.className)
                exclusionRules.append({type, ExclusionRule::ClassRule { wkRule.className } });
            else
                exclusionRules.append({type, ExclusionRule::ElementRule { wkRule.elementName } });
        }
    }

    _page->startTextManipulations(exclusionRules, includeSubframes, [weakSelf = WeakObjCPtr<WKWebView>(self)] (const Vector<WebCore::TextManipulationItem>& itemReferences) {
        if (!weakSelf)
            return;

        auto retainedSelf = weakSelf.get();
        auto delegate = [retainedSelf _textManipulationDelegate];
        if (!delegate)
            return;

        auto createWKItem = [] (const WebCore::TextManipulationItem& item) {
            auto tokens = createNSArray(item.tokens, [] (auto& token) {
                auto wkToken = adoptNS([[_WKTextManipulationToken alloc] init]);
                [wkToken setIdentifier:String::number(token.identifier.toUInt64())];
                [wkToken setContent:token.content];
                [wkToken setExcluded:token.isExcluded];
                [wkToken setUserInfo:createUserInfo(token.info).get()];
                return wkToken;
            });
            auto identifier = makeString(item.frameID ? item.frameID->processIdentifier().toUInt64() : 0, '-', item.frameID ? item.frameID->object().toUInt64() : 0, '-', item.identifier ? item.identifier->toUInt64() : 0);
            return adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:identifier tokens:tokens.get() isSubframe:item.isSubframe isCrossSiteSubframe:item.isCrossSiteSubframe]);
        };

        if ([delegate respondsToSelector:@selector(_webView:didFindTextManipulationItems:)])
            [delegate _webView:retainedSelf.get() didFindTextManipulationItems:createNSArray(itemReferences, createWKItem).get()];
        else {
            for (auto& item : itemReferences)
                [delegate _webView:retainedSelf.get() didFindTextManipulationItem:createWKItem(item).get()];
        }
    }, [capturedCompletionBlock = makeBlockPtr(completionHandler)] () {
        capturedCompletionBlock();
    });
}

struct ItemIdentifiers {
    WebCore::FrameIdentifier frameID;
    WebCore::TextManipulationItemIdentifier itemID;
};

static std::optional<ItemIdentifiers> coreTextManipulationItemIdentifierFromString(NSString *identifier)
{
    String identifierString { identifier };
    unsigned index = 0;
    std::optional<uint64_t> processID;
    std::optional<uint64_t> frameID;
    std::optional<uint64_t> itemID;
    for (auto token : StringView(identifierString).split('-')) {
        switch (index) {
        case 0:
            processID = parseInteger<uint64_t>(token);
            break;
        case 1:
            frameID = parseInteger<uint64_t>(token);
            break;
        case 2:
            itemID = parseInteger<uint64_t>(token);
            break;
        default:
            return std::nullopt;
        }
        ++index;
    }
    if (!processID || !*processID || !frameID || !*frameID || !itemID || !*itemID)
        return std::nullopt;

    if (!ObjectIdentifier<WebCore::ProcessIdentifierType>::isValidIdentifier(*processID))
        return std::nullopt;

    if (!ObjectIdentifier<WebCore::TextManipulationItemIdentifierType>::isValidIdentifier(*itemID))
        return std::nullopt;

    return ItemIdentifiers { WebCore::ProcessQualified(ObjectIdentifier<WebCore::FrameIdentifierType>(*frameID),
        ObjectIdentifier<WebCore::ProcessIdentifierType>(*processID)),
        ObjectIdentifier<WebCore::TextManipulationItemIdentifierType>(*itemID) };
}

static WebCore::TextManipulationTokenIdentifier coreTextManipulationTokenIdentifierFromString(NSString *identifier)
{
    return ObjectIdentifier<WebCore::TextManipulationTokenIdentifierType>(identifier.longLongValue);
}

- (void)_completeTextManipulation:(_WKTextManipulationItem *)item completion:(void(^)(BOOL success))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!_page) {
        completionHandler(false);
        return;
    }

    auto identifiers = coreTextManipulationItemIdentifierFromString(item.identifier);
    if (!identifiers) {
        completionHandler(false);
        return;
    }

    Vector<WebCore::TextManipulationToken> tokens;
    for (_WKTextManipulationToken *wkToken in item.tokens)
        tokens.append(WebCore::TextManipulationToken { coreTextManipulationTokenIdentifierFromString(wkToken.identifier), wkToken.content, std::nullopt });

    Vector<WebCore::TextManipulationItem> coreItems({ WebCore::TextManipulationItem { identifiers->frameID, false, false, identifiers->itemID, WTFMove(tokens) } });
    _page->completeTextManipulation(coreItems, [capturedCompletionBlock = makeBlockPtr(completionHandler)] (bool allFailed, auto& failures) {
        capturedCompletionBlock(!allFailed && failures.isEmpty());
    });
}

static RetainPtr<NSMutableArray> makeFailureSetForAllTextManipulationItems(NSArray<_WKTextManipulationItem *> *items)
{
    RetainPtr<NSMutableArray> wkFailures = adoptNS([[NSMutableArray alloc] initWithCapacity:items.count]);
    for (_WKTextManipulationItem *item in items)
        [wkFailures addObject:[NSError errorWithDomain:_WKTextManipulationItemErrorDomain code:_WKTextManipulationItemErrorNotAvailable userInfo:@{_WKTextManipulationItemErrorItemKey: item}]];
    return wkFailures;
};

static RetainPtr<NSArray> wkTextManipulationErrors(NSArray<_WKTextManipulationItem *> *items, const Vector<WebCore::TextManipulationController::ManipulationFailure>& failures)
{
    if (failures.isEmpty())
        return nil;

    return createNSArray(failures, [&] (auto& coreFailure) -> NSError * {
        ASSERT(coreFailure.index < items.count);
        if (coreFailure.index >= items.count)
            return nil;
        auto errorCode = static_cast<NSInteger>(([&coreFailure] {
            using Type = WebCore::TextManipulationController::ManipulationFailure::Type;
            switch (coreFailure.type) {
            case Type::NotAvailable:
                return _WKTextManipulationItemErrorNotAvailable;
            case Type::ContentChanged:
                return _WKTextManipulationItemErrorContentChanged;
            case Type::InvalidItem:
                return _WKTextManipulationItemErrorInvalidItem;
            case Type::InvalidToken:
                return _WKTextManipulationItemErrorInvalidToken;
            case Type::ExclusionViolation:
                return _WKTextManipulationItemErrorExclusionViolation;
            }
        })());
        auto item = items[coreFailure.index];
#if ASSERT_ENABLED
        auto identifiers = coreTextManipulationItemIdentifierFromString(item.identifier);
        ASSERT(identifiers);
        ASSERT(identifiers->frameID == coreFailure.frameID);
        ASSERT(identifiers->itemID == coreFailure.identifier);
#endif
        return [NSError errorWithDomain:_WKTextManipulationItemErrorDomain code:errorCode userInfo:@{_WKTextManipulationItemErrorItemKey: item}];
    });
}

- (void)_completeTextManipulationForItems:(NSArray<_WKTextManipulationItem *> *)items completion:(void(^)(NSArray<NSError *> *errors))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!_page) {
        completionHandler(makeFailureSetForAllTextManipulationItems(items).get());
        return;
    }

    Vector<WebCore::TextManipulationItem> coreItems;
    coreItems.reserveInitialCapacity(items.count);
    for (_WKTextManipulationItem *wkItem in items) {
        Vector<WebCore::TextManipulationToken> coreTokens(wkItem.tokens.count, [&](size_t i) {
            _WKTextManipulationToken *wkToken = wkItem.tokens[i];
            return WebCore::TextManipulationToken { coreTextManipulationTokenIdentifierFromString(wkToken.identifier), wkToken.content, std::nullopt };
        });
        auto identifiers = coreTextManipulationItemIdentifierFromString(wkItem.identifier);
        std::optional<WebCore::FrameIdentifier> frameID;
        std::optional<WebCore::TextManipulationItemIdentifier> itemID;
        if (identifiers) {
            frameID = identifiers->frameID;
            itemID = identifiers->itemID;
        }
        coreItems.append(WebCore::TextManipulationItem { frameID, false, false, itemID, WTFMove(coreTokens) });
    }

    RetainPtr<NSArray<_WKTextManipulationItem *>> retainedItems = items;
    _page->completeTextManipulation(coreItems, [capturedItems = retainedItems, capturedCompletionBlock = makeBlockPtr(completionHandler)](bool allFailed, auto& failures) {
        if (allFailed) {
            capturedCompletionBlock(makeFailureSetForAllTextManipulationItems(capturedItems.get()).get());
            return;
        }
        capturedCompletionBlock(wkTextManipulationErrors(capturedItems.get(), failures).get());
    });
}

- (void)_startImageAnalysis:(NSString *)sourceLanguageIdentifier target:(NSString *)targetLanguageIdentifier
{
#if ENABLE(IMAGE_ANALYSIS)
    THROW_IF_SUSPENDED;

    if (!_page || !_page->preferences().visualTranslationEnabled() || !WebKit::languageIdentifierSupportsLiveText(sourceLanguageIdentifier))
        return;

    _page->startVisualTranslation(sourceLanguageIdentifier, targetLanguageIdentifier);
#endif
}

- (void)_dataTaskWithRequest:(NSURLRequest *)request runAtForegroundPriority:(BOOL)runAtForegroundPriority completionHandler:(void(^)(_WKDataTask *))completionHandler
{
    _page->dataTaskWithRequest(request, std::nullopt, !!runAtForegroundPriority, [completionHandler = makeBlockPtr(completionHandler)] (Ref<API::DataTask>&& task) {
        completionHandler(wrapper(task));
    });
}

- (void)_dataTaskWithRequest:(NSURLRequest *)request completionHandler:(void(^)(_WKDataTask *))completionHandler
{
    [self _dataTaskWithRequest:request runAtForegroundPriority:NO completionHandler:completionHandler];
}

- (void)_takeFindStringFromSelection:(id)sender
{
    THROW_IF_SUSPENDED;
#if PLATFORM(MAC)
    [self _takeFindStringFromSelectionInternal:sender];
#else
    _page->executeEditCommand("TakeFindStringFromSelection"_s);
#endif
}

+ (NSString *)_stringForFind
{
    return WebKit::stringForFind();
}

+ (void)_setStringForFind:(NSString *)findString
{
    WebKit::updateStringForFind(findString);
}

- (_WKRemoteObjectRegistry *)_remoteObjectRegistry
{
#if PLATFORM(MAC)
    return _impl->remoteObjectRegistry();
#else
    if (!_remoteObjectRegistry) {
        _remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithWebPageProxy:*_page]);
        _page->configuration().processPool().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->identifier(), [_remoteObjectRegistry remoteObjectRegistry]);
    }

    return _remoteObjectRegistry.get();
#endif
}

- (WKBrowsingContextHandle *)_handle
{
    return adoptNS([[WKBrowsingContextHandle alloc] _initWithPageProxy:*_page]).autorelease();
}

- (_WKRenderingProgressEvents)_observedRenderingProgressEvents
{
    return _observedRenderingProgressEvents;
}

- (id <WKHistoryDelegatePrivate>)_historyDelegate
{
    return _navigationState->historyDelegate().autorelease();
}

- (void)_setHistoryDelegate:(id <WKHistoryDelegatePrivate>)historyDelegate
{
    _page->setHistoryClient(_navigationState->createHistoryClient());
    _navigationState->setHistoryDelegate(historyDelegate);
}

- (void)_updateMediaPlaybackControlsManager
{
    THROW_IF_SUSPENDED;
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->updateMediaPlaybackControlsManager();
#endif
}

- (BOOL)_isSuspended
{
    return _page->legacyMainFrameProcess().throttler().isSuspended();
}

- (BOOL)_canTogglePictureInPicture
{
#if HAVE(TOUCH_BAR)
    return _impl->canTogglePictureInPicture();
#else
    return NO;
#endif
}

- (BOOL)_canToggleInWindow
{
#if HAVE(TOUCH_BAR)
    return _impl->canTogglePictureInPicture();
#else
    return NO;
#endif
}

- (BOOL)_canEnterFullscreen
{
    return _page->canEnterFullscreen();
}

- (BOOL)_isPictureInPictureActive
{
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    return _impl->isPictureInPictureActive();
#else
    return NO;
#endif
}

- (BOOL)_isInWindowActive
{
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    return _impl->isInWindowFullscreenActive();
#else
    return NO;
#endif
}

- (void)_togglePictureInPicture
{
    THROW_IF_SUSPENDED;
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->togglePictureInPicture();
#endif
}

- (void)_nowPlayingMediaTitleAndArtist:(void (^)(NSString *, NSString *))completionHandler
{
    THROW_IF_SUSPENDED;
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->nowPlayingMediaTitleAndArtist(completionHandler);
#else
    completionHandler(nil, nil);
#endif
}

- (void)_toggleInWindow
{
    THROW_IF_SUSPENDED;
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->isInWindowFullscreenActive() ? _impl->exitInWindowFullscreen() : _impl->enterInWindowFullscreen();
#endif
}

- (void)_enterInWindow
{
    THROW_IF_SUSPENDED;
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->enterInWindowFullscreen();
#endif
}

- (void)_exitInWindow
{
    THROW_IF_SUSPENDED;
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->exitInWindowFullscreen();
#endif
}

- (void)_enterFullscreen
{
    if (RefPtr page = _page)
        page->enterFullscreen();
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
- (void)_pauseAllAnimationsWithCompletionHandler:(void(^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler) {
        _page->pauseAllAnimations([] { });
        return;
    }

    _page->pauseAllAnimations(makeBlockPtr(completionHandler));
}

- (void)_playAllAnimationsWithCompletionHandler:(void(^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler) {
        _page->playAllAnimations([] { });
        return;
    }

    _page->playAllAnimations(makeBlockPtr(completionHandler));
}

- (BOOL)_allowsAnyAnimationToPlay
{
    THROW_IF_SUSPENDED;
    return _page->allowsAnyAnimationToPlay();
}

- (BOOL)_allowAnimationControls
{
    THROW_IF_SUSPENDED;

    // Only show animation controls if autoplay of animated images has been disabled.
    auto* autoplayAnimatedImagesFunction = _AXSReduceMotionAutoplayAnimatedImagesEnabledPtr();
    return autoplayAnimatedImagesFunction && !autoplayAnimatedImagesFunction();
}
#else // !ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
- (void)_pauseAllAnimationsWithCompletionHandler:(void(^)(void))completionHandler
{
    if (completionHandler)
        completionHandler();
}

- (void)_playAllAnimationsWithCompletionHandler:(void(^)(void))completionHandler
{
    if (completionHandler)
        completionHandler();
}

- (BOOL)_allowsAnyAnimationToPlay
{
    return YES;
}

- (BOOL)_allowAnimationControls
{
    return NO;
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

- (void)_setStatisticsCrossSiteLoadWithLinkDecorationForTesting:(NSString *)fromHost withToHost:(NSString *)toHost withWasFiltered:(BOOL)wasFiltered withCompletionHandler:(void(^)(void))completionHandler
{
    if (RefPtr pageForTesting = _page->pageForTesting())
        pageForTesting->setCrossSiteLoadWithLinkDecorationForTesting(URL { fromHost }, URL { toHost }, wasFiltered, makeBlockPtr(completionHandler));
}

- (_WKMediaMutedState)_mediaMutedState
{
    return WebKit::toWKMediaMutedState(_page->mutedStateFlags());
}

- (void)_closeAllMediaPresentations
{
    THROW_IF_SUSPENDED;
    [self closeAllMediaPresentationsWithCompletionHandler:^{ }];
}

- (void)_stopMediaCapture
{
    THROW_IF_SUSPENDED;
    _page->stopMediaCapture(WebCore::MediaProducerMediaCaptureKind::EveryKind);
}

- (void)_stopAllMediaPlayback
{
    THROW_IF_SUSPENDED;
    [self pauseAllMediaPlaybackWithCompletionHandler:nil];
}

- (void)_suspendAllMediaPlayback
{
    THROW_IF_SUSPENDED;
    [self setAllMediaPlaybackSuspended:YES completionHandler:nil];
}

- (void)_resumeAllMediaPlayback
{
    THROW_IF_SUSPENDED;
    [self setAllMediaPlaybackSuspended:NO completionHandler:nil];
}

#if ENABLE(APP_HIGHLIGHTS)
static void convertAndAddHighlight(Vector<Ref<WebCore::SharedMemory>>& buffers, NSData *highlight)
{
    if (auto sharedMemory = WebCore::SharedMemory::allocate(highlight.length)) {
        [highlight getBytes:sharedMemory->mutableSpan().data() length:highlight.length];
        buffers.append(sharedMemory.releaseNonNull());
    }
}
#endif

- (void)_restoreAppHighlights:(NSArray<NSData *> *)highlights
{
    THROW_IF_SUSPENDED;
#if ENABLE(APP_HIGHLIGHTS)
    Vector<Ref<WebCore::SharedMemory>> buffers;

    for (NSData *highlight in highlights)
        convertAndAddHighlight(buffers, highlight);
    
    _page->restoreAppHighlightsAndScrollToIndex(buffers, std::nullopt);
#else
    UNUSED_PARAM(highlights);
#endif
}

- (void)_restoreAndScrollToAppHighlight:(NSData *)highlight
{
    THROW_IF_SUSPENDED;
#if ENABLE(APP_HIGHLIGHTS)
    Vector<Ref<WebCore::SharedMemory>> buffers;
    
    convertAndAddHighlight(buffers, highlight);
    _page->restoreAppHighlightsAndScrollToIndex(buffers, 0);
#else
    UNUSED_PARAM(highlight);
#endif
}

- (void)_addAppHighlight
{
    THROW_IF_SUSPENDED;
    [self _addAppHighlightInNewGroup:NO originatedInApp:YES];
}

- (void)_addAppHighlightInNewGroup:(BOOL)newGroup originatedInApp:(BOOL)originatedInApp
{
    THROW_IF_SUSPENDED;
#if ENABLE(APP_HIGHLIGHTS)
    _page->createAppHighlightInSelectedRange(newGroup ? WebCore::CreateNewGroupForHighlight::Yes : WebCore::CreateNewGroupForHighlight::No, originatedInApp ? WebCore::HighlightRequestOriginatedInApp::Yes : WebCore::HighlightRequestOriginatedInApp::No);
#endif
}

- (void)_requestAllTextWithCompletionHandler:(void(^)(NSArray<_WKTextRun *> *))completionHandler
{
    _page->requestAllTextAndRects([weakSelf = WeakObjCPtr<WKWebView>(self), completion = makeBlockPtr(completionHandler)](auto&& textRuns) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return completion(@[ ]);

        completion(createNSArray(WTFMove(textRuns), [](auto&& textRun) {
            return wrapper(textRun);
        }).get());
    });
}

- (void)_requestTargetedElementInfo:(_WKTargetedElementRequest *)request completionHandler:(void(^)(NSArray<_WKTargetedElementInfo *> *))completion
{
    _page->requestTargetedElement(*request->_request, [completion = makeBlockPtr(completion)](auto& elements) {
        completion(createNSArray(elements, [](auto& element) {
            return wrapper(element);
        }).get());
    });
}

- (void)_requestAllTargetableElementsInfo:(CGFloat)hitTestInterval completionHandler:(void(^)(NSArray<NSArray<_WKTargetedElementInfo *> *> *))completionHandler
{
    _page->requestAllTargetableElements(float(hitTestInterval), [completion = makeBlockPtr(completionHandler)](auto&& elements) {
        completion(createNSArray(elements, [](auto& subelements) {
            return createNSArray(subelements, [](auto& subelement) {
                return wrapper(subelement);
            });
        }).get());
    });
}

- (NSURL *)_unreachableURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().unreachableURL()];
}

- (NSURL *)_mainFrameURL
{
    if (auto* frame = _page->mainFrame())
        return frame->url();
    return nil;
}

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    THROW_IF_SUSPENDED;
    RetainPtr data = bridge_cast([string dataUsingEncoding:NSUTF8StringEncoding] ?: NSData.data);
    _page->loadAlternateHTML(WebCore::DataSegment::create(WTFMove(data)), "UTF-8"_s, baseURL, unreachableURL);
}

- (WKNavigation *)_loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL userData:(id)userData
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->loadData(WebCore::SharedBuffer::create(data), MIMEType, characterEncodingName, baseURL.absoluteString, [userData isKindOfClass:NSData.class] ? API::Data::createWithoutCopying((NSData *)userData).ptr() : nullptr)).autorelease();
}

- (WKNavigation *)_loadRequest:(NSURLRequest *)request shouldOpenExternalURLs:(BOOL)shouldOpenExternalURLs
{
    THROW_IF_SUSPENDED;
    _WKShouldOpenExternalURLsPolicy policy = shouldOpenExternalURLs ? _WKShouldOpenExternalURLsPolicyAllow : _WKShouldOpenExternalURLsPolicyNotAllow;
    return [self _loadRequest:request shouldOpenExternalURLsPolicy:policy];
}

- (WKNavigation *)_loadRequest:(NSURLRequest *)request shouldOpenExternalURLsPolicy:(_WKShouldOpenExternalURLsPolicy)shouldOpenExternalURLsPolicy
{
    THROW_IF_SUSPENDED;
    WebCore::ShouldOpenExternalURLsPolicy policy;
    switch (shouldOpenExternalURLsPolicy) {
    case _WKShouldOpenExternalURLsPolicyNotAllow:
        policy = WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow;
        break;
    case _WKShouldOpenExternalURLsPolicyAllow:
        policy = WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow;
        break;
    case _WKShouldOpenExternalURLsPolicyAllowExternalSchemesButNotAppLinks:
        policy = WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks;
        break;
    }
    return wrapper(_page->loadRequest(request, policy)).autorelease();
}

- (void)_loadAndDecodeImage:(NSURLRequest *)request constrainedToSize:(CGSize)maxSize maximumBytesFromNetwork:(size_t)maximumBytesFromNetwork completionHandler:(void (^)(CocoaImage *, NSError *))completionHandler
{
    auto sizeConstraint = (maxSize.height || maxSize.width) ? std::optional(WebCore::FloatSize(maxSize)) : std::nullopt;
    WebCore::ResourceRequest resourceRequest(request);
    auto url = resourceRequest.url();
    _page->loadAndDecodeImage(request, sizeConstraint, maximumBytesFromNetwork, [completionHandler = makeBlockPtr(completionHandler), url](std::variant<WebCore::ResourceError, Ref<WebCore::ShareableBitmap>>&& result) mutable {
        WTF::switchOn(WTFMove(result), [&] (WebCore::ResourceError&& error) {
            if (error.isNull())
                return completionHandler(nil, WebCore::internalError(url)); // This can happen if IPC fails.
            completionHandler(nil, error.nsError());
        }, [&] (Ref<WebCore::ShareableBitmap>&& bitmap) {
#if PLATFORM(MAC)
            completionHandler(adoptNS([[NSImage alloc] initWithCGImage:bitmap->makeCGImageCopy().get() size:bitmap->size()]).get(), nil);
#else
            completionHandler(adoptNS([[UIImage alloc] initWithCGImage:bitmap->makeCGImageCopy().get()]).get(), nil);
#endif
        });
    });
}

- (void)_getInformationFromImageData:(NSData *)imageData completionHandler:(void (^)(NSString * typeIdentifier, NSArray<NSValue *> *availableSizes, NSError *error))completionHandler
{
    _page->getInformationFromImageData(makeVector(imageData), [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result) {
            NSError *error = [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: WebCore::descriptionString(result.error()) }];
            return completionHandler(nil, nil, error);
        }

        auto [typeIdentifier, sizes] = result.value();
        RetainPtr availableSizes = createNSArray(sizes, [](auto size) {
#if PLATFORM(MAC)
            return [NSValue valueWithSize:size];
#else
            CGSize cgSize = size;
            return [NSValue valueWithCGSize:cgSize];
#endif
        });
        return completionHandler(typeIdentifier, availableSizes.autorelease(), nil);
    });
}

- (void)_createIconDataFromImageData:(NSData *)imageData withLengths:(NSArray<NSNumber *> *)lengths completionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    Vector<unsigned> targetLengths;
    targetLengths.reserveInitialCapacity(lengths.count);
    for (NSNumber *length in lengths) {
        if (unsigned lengthValue = length.unsignedIntValue)
            targetLengths.append(lengthValue);
    }

    auto buffer = WebCore::SharedBuffer::create(imageData);
    _page->createIconDataFromImageData(WTFMove(buffer), targetLengths, [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result) {
            NSError *error = [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: @"Failed to decode data" }];
            return completionHandler(nil, error);
        }

        completionHandler(result->createNSData().autorelease(), nil);
    });
}

- (void)_decodeImageData:(NSData *)imageData preferredSize:(NSValue *)preferredSize completionHandler:(void (^)(CocoaImage *, NSError *))completionHandler
{
    std::optional<WebCore::FloatSize> size;
    if (preferredSize) {
#if PLATFORM(MAC)
        size = WebCore::FloatSize([preferredSize sizeValue]);
#else
        size = WebCore::FloatSize([preferredSize CGSizeValue]);
#endif
    }

    auto buffer = WebCore::SharedBuffer::create(imageData);
    _page->decodeImageData(WTFMove(buffer), size, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<WebCore::ShareableBitmap>&& bitmap) mutable {
        if (!bitmap) {
            NSError *error = [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: @"Failed to decode data" }];
            return completionHandler(nil, error);
        }

#if PLATFORM(MAC)
        RetainPtr cocoaImage = adoptNS([[NSImage alloc] initWithCGImage:bitmap->makeCGImageCopy().get() size:bitmap->size()]);
#else
        RetainPtr cocoaImage = adoptNS([[UIImage alloc] initWithCGImage:bitmap->makeCGImageCopy().get()]);
#endif
        completionHandler(cocoaImage.autorelease(), nil);
    });
}

- (void)_loadServiceWorker:(NSURL *)url usingModules:(BOOL)usingModules completionHandler:(void (^)(BOOL success))completionHandler
{
    THROW_IF_SUSPENDED;
    if (_page->isServiceWorkerPage())
        [NSException raise:NSInternalInconsistencyException format:@"The WKWebView was already used to load a service worker"];

    _page->loadServiceWorker(url, usingModules, [completionHandler = makeBlockPtr(completionHandler)](bool success) mutable {
        completionHandler(success);
    });
}

- (void)_loadServiceWorker:(NSURL *)url completionHandler:(void (^)(BOOL success))completionHandler
{
    [self _loadServiceWorker:url usingModules:NO completionHandler:completionHandler];
}

- (void)_grantAccessToAssetServices
{
    THROW_IF_SUSPENDED;
    if (_page)
        _page->grantAccessToAssetServices();
}

- (void)_revokeAccessToAssetServices
{
    THROW_IF_SUSPENDED;
    if (_page)
        _page->revokeAccessToAssetServices();
}

- (void)_disableURLSchemeCheckInDataDetectors
{
    THROW_IF_SUSPENDED;
    if (_page)
        _page->disableURLSchemeCheckInDataDetectors();
}

- (void)_switchFromStaticFontRegistryToUserFontRegistry
{
    THROW_IF_SUSPENDED;
    if (_page)
        _page->switchFromStaticFontRegistryToUserFontRegistry();
}

- (void)_didLoadAppInitiatedRequest:(void (^)(BOOL result))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->appPrivacyReportTestingData([completionHandler = makeBlockPtr(completionHandler)] (auto&& appPrivacyReportTestingData) mutable {
        completionHandler(appPrivacyReportTestingData.hasLoadedAppInitiatedRequestTesting);
    });
}

- (void)_didLoadNonAppInitiatedRequest:(void (^)(BOOL result))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->appPrivacyReportTestingData([completionHandler = makeBlockPtr(completionHandler)] (auto&& appPrivacyReportTestingData) mutable {
        completionHandler(appPrivacyReportTestingData.hasLoadedNonAppInitiatedRequestTesting);
    });
}

- (void)_suspendPage:(void (^)(BOOL))completionHandler
{
    if (!_page) {
        completionHandler(NO);
        return;
    }
    _page->suspend([completionHandler = makeBlockPtr(completionHandler)](bool success) {
        completionHandler(success);
    });
}

- (void)_resumePage:(void (^)(BOOL))completionHandler
{
    if (!_page) {
        completionHandler(NO);
        return;
    }
    _page->resume([completionHandler = makeBlockPtr(completionHandler)](bool success) {
        completionHandler(success);
    });
}

- (NSArray *)_certificateChain
{
    if (WebKit::WebFrameProxy* mainFrame = _page->mainFrame())
        return (__bridge NSArray *)WebCore::CertificateInfo::certificateChainFromSecTrust(mainFrame->certificateInfo().trust().get()).autorelease();

    return nil;
}

- (NSURL *)_committedURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().url()];
}

- (NSString *)_MIMEType
{
    if (_page->mainFrame())
        return _page->mainFrame()->mimeType();

    return nil;
}

- (NSString *)_userAgent
{
    return _page->userAgent();
}

- (NSString *)_applicationNameForUserAgent
{
    return _page->applicationNameForUserAgent();
}

- (void)_setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    THROW_IF_SUSPENDED;
    _page->setApplicationNameForUserAgent(applicationNameForUserAgent);
    _page->setApplicationNameForDesktopUserAgent(applicationNameForUserAgent);
}

- (NSString *)_customUserAgent
{
    return self.customUserAgent;
}

- (void)_setCustomUserAgent:(NSString *)customUserAgent
{
    self.customUserAgent = customUserAgent;
}

- (void)_setUserContentExtensionsEnabled:(BOOL)userContentExtensionsEnabled
{
    // This is kept for binary compatibility with iOS 9.
}

- (BOOL)_userContentExtensionsEnabled
{
    // This is kept for binary compatibility with iOS 9.
    return true;
}

- (pid_t)_webProcessIdentifier
{
    if (![self _isValid])
        return 0;

    return _page->legacyMainFrameProcessID();
}

- (pid_t)_provisionalWebProcessIdentifier
{
    if (![self _isValid])
        return 0;

    auto* provisionalPage = _page->provisionalPageProxy();
    if (!provisionalPage)
        return 0;

    return provisionalPage->process().processID();
}

- (pid_t)_gpuProcessIdentifier
{
    if (![self _isValid])
        return 0;

    return _page->gpuProcessID();
}

- (pid_t)_modelProcessIdentifier
{
    if (![self _isValid])
        return 0;

    return _page->modelProcessID();
}


- (BOOL)_webProcessIsResponsive
{
    return _page->legacyMainFrameProcess().isResponsive();
}

- (void)_killWebContentProcess
{
    THROW_IF_SUSPENDED;
    if (![self _isValid])
        return;

    _page->legacyMainFrameProcess().terminate();
}

- (WKNavigation *)_reloadWithoutContentBlockers
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->reload(WebCore::ReloadOption::DisableContentBlockers)).autorelease();
}

- (WKNavigation *)_reloadExpiredOnly
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->reload(WebCore::ReloadOption::ExpiredOnly)).autorelease();
}

- (void)_killWebContentProcessAndResetState
{
    THROW_IF_SUSPENDED;
    Ref<WebKit::WebProcessProxy> protectedProcessProxy(_page->legacyMainFrameProcess());
    protectedProcessProxy->requestTermination(WebKit::ProcessTerminationReason::RequestedByClient);

    if (auto* provisionalPageProxy = _page->provisionalPageProxy()) {
        Ref<WebKit::WebProcessProxy> protectedProcessProxy(provisionalPageProxy->process());
        protectedProcessProxy->requestTermination(WebKit::ProcessTerminationReason::RequestedByClient);
    }
}

- (void)_takePDFSnapshotWithConfiguration:(WKSnapshotConfiguration *)snapshotConfiguration completionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    RetainPtr<WKPDFConfiguration> pdfConfiguration;
    if (snapshotConfiguration) {
        pdfConfiguration = adoptNS([[WKPDFConfiguration alloc] init]);
        [pdfConfiguration setRect:snapshotConfiguration.rect];
    }

    [self createPDFWithConfiguration:pdfConfiguration.get() completionHandler:completionHandler];
}

- (void)_getPDFFirstPageSizeInFrame:(_WKFrameHandle *)frame completionHandler:(void(^)(CGSize))completionHandler
{
    THROW_IF_SUSPENDED;
    auto frameID = frame->_frameHandle->frameID();
    if (!frameID)
        return completionHandler({ });
    _page->getPDFFirstPageSize(*frameID, [completionHandler = makeBlockPtr(completionHandler)](WebCore::FloatSize size) {
        completionHandler(static_cast<CGSize>(size));
    });
}

- (NSData *)_sessionStateData
{
    // FIXME: This should not use the legacy session state encoder.
    return wrapper(WebKit::encodeLegacySessionState(_page->sessionState())).autorelease();
}

- (_WKSessionState *)_sessionState
{
    return adoptNS([[_WKSessionState alloc] _initWithSessionState:_page->sessionState()]).autorelease();
}

- (_WKSessionState *)_sessionStateWithFilter:(BOOL (^)(WKBackForwardListItem *item))filter
{
    WebKit::SessionState sessionState = _page->sessionState([filter](WebKit::WebBackForwardListItem& item) {
        if (!filter)
            return true;

        return (bool)filter(wrapper(item));
    });

    return adoptNS([[_WKSessionState alloc] _initWithSessionState:sessionState]).autorelease();
}

- (void)_restoreFromSessionStateData:(NSData *)sessionStateData
{
    THROW_IF_SUSPENDED;

    // FIXME: This should not use the legacy session state decoder.
    WebKit::SessionState sessionState;
    if (!WebKit::decodeLegacySessionState(span(sessionStateData), sessionState))
        return;

    _page->restoreFromSessionState(WTFMove(sessionState), true);
}

- (WKNavigation *)_restoreSessionState:(_WKSessionState *)sessionState andNavigate:(BOOL)navigate
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->restoreFromSessionState(sessionState ? sessionState->_sessionState : WebKit::SessionState { }, navigate)).autorelease();
}

- (void)_close
{
    _page->close();
}

- (BOOL)_tryClose
{
    THROW_IF_SUSPENDED;
    return _page->tryClose();
}

- (BOOL)_isClosed
{
    return _page->isClosed();
}

- (_WKAttachment *)_insertAttachmentWithFilename:(NSString *)filename contentType:(NSString *)contentType data:(NSData *)data options:(_WKAttachmentDisplayOptions *)options completion:(void(^)(BOOL success))completionHandler
{
    THROW_IF_SUSPENDED;
    UNUSED_PARAM(options);
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    if (filename)
        [fileWrapper setPreferredFilename:filename];
    return [self _insertAttachmentWithFileWrapper:fileWrapper.get() contentType:contentType completion:completionHandler];
}

- (_WKAttachment *)_insertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType options:(_WKAttachmentDisplayOptions *)options completion:(void(^)(BOOL success))completionHandler
{
    THROW_IF_SUSPENDED;
    UNUSED_PARAM(options);
    return [self _insertAttachmentWithFileWrapper:fileWrapper contentType:contentType completion:completionHandler];
}

- (_WKAttachment *)_insertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType completion:(void(^)(BOOL success))completionHandler
{
    THROW_IF_SUSPENDED;
#if ENABLE(ATTACHMENT_ELEMENT)
    auto identifier = createVersion4UUIDString();
    auto attachment = API::Attachment::create(identifier, *_page);
    attachment->setFileWrapperAndUpdateContentType(fileWrapper, contentType);
    _page->insertAttachment(attachment.copyRef(), [capturedHandler = makeBlockPtr(completionHandler)] {
        if (capturedHandler)
            capturedHandler(true);
    });
#if HAVE(QUICKLOOK_THUMBNAILING)
    _page->requestThumbnail(attachment, identifier);
#endif
    return wrapper(attachment);
#else
    return nil;
#endif
}

- (_WKAttachment *)_attachmentForIdentifier:(NSString *)identifier
{
    THROW_IF_SUSPENDED;
#if ENABLE(ATTACHMENT_ELEMENT)
    if (auto attachment = _page->attachmentForIdentifier(identifier))
        return wrapper(attachment);
#endif
    return nil;
}

- (void)_simulateDeviceOrientationChangeWithAlpha:(double)alpha beta:(double)beta gamma:(double)gamma
{
    THROW_IF_SUSPENDED;
    _page->simulateDeviceOrientationChange(alpha, beta, gamma);
}

- (void)_launchInitialProcessIfNecessary
{
    THROW_IF_SUSPENDED;
    _page->launchInitialProcessIfNecessary();
}

- (void)_clearBackForwardCache
{
    THROW_IF_SUSPENDED;
    _page->configuration().processPool().backForwardCache().removeEntriesForPage(*_page);
}

+ (BOOL)_handlesSafeBrowsing
{
    return true;
}

+ (BOOL)_willUpgradeToHTTPS:(NSURL *)url
{
#if ENABLE(CONTENT_EXTENSIONS)
    return WebCore::ContentExtensions::ContentExtensionsBackend::shouldBeMadeSecure(url);
#else
    return NO;
#endif
}

- (void)_showWarningViewWithTitle:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _showWarningViewWithURL:nil title:title warning:warning detailsWithLinks:details completionHandler:^(BOOL continueUnsafeLoad, NSURL *url) {
        ASSERT(!url);
        completionHandler(continueUnsafeLoad);
    }];
}

- (void)_showSafeBrowsingWarningWithTitle:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler
{
    [self _showWarningViewWithTitle:title warning:warning details:details completionHandler:completionHandler];
}

- (void)_showWarningViewWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _showWarningViewWithURL:nil title:title warning:warning detailsWithLinks:details completionHandler:^(BOOL continueUnsafeLoad, NSURL *url) {
        ASSERT(!url);
        completionHandler(continueUnsafeLoad);
    }];
}

- (void)_showSafeBrowsingWarningWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler
{
    [self _showWarningViewWithURL:url title:title warning:warning details:details completionHandler:completionHandler];
}

- (void)_showWarningViewWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning detailsWithLinks:(NSAttributedString *)details completionHandler:(void(^)(BOOL, NSURL *))completionHandler
{
    THROW_IF_SUSPENDED;
    auto safeBrowsingWarning = WebKit::BrowsingWarning::create(url, title, warning, details, WebKit::BrowsingWarning::SafeBrowsingWarningData { });
    auto wrapper = [completionHandler = makeBlockPtr(completionHandler)] (std::variant<WebKit::ContinueUnsafeLoad, URL>&& variant) {
        switchOn(variant, [&] (WebKit::ContinueUnsafeLoad continueUnsafeLoad) {
            switch (continueUnsafeLoad) {
            case WebKit::ContinueUnsafeLoad::Yes:
                return completionHandler(YES, nil);
            case WebKit::ContinueUnsafeLoad::No:
                return completionHandler(NO, nil);
            }
        }, [&] (URL url) {
            completionHandler(NO, url);
        });
    };
#if PLATFORM(MAC)
    _impl->showWarningView(safeBrowsingWarning, WTFMove(wrapper));
#else
    [self _showWarningView:safeBrowsingWarning completionHandler:WTFMove(wrapper)];
#endif
}

- (void)_showSafeBrowsingWarningWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning detailsWithLinks:(NSAttributedString *)details completionHandler:(void(^)(BOOL, NSURL *))completionHandler
{
    [self _showWarningViewWithURL:url title:title warning:warning detailsWithLinks:details completionHandler:completionHandler];
}

+ (NSURL *)_confirmMalwareSentinel
{
    return WebKit::BrowsingWarning::confirmMalwareSentinel();
}

+ (NSURL *)_visitUnsafeWebsiteSentinel
{
    return WebKit::BrowsingWarning::visitUnsafeWebsiteSentinel();
}

- (void)_isJITEnabled:(void(^)(BOOL))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->isJITEnabled([completionHandler = makeBlockPtr(completionHandler)] (bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_evaluateJavaScriptWithoutUserGesture:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:NO inFrame:nil inWorld:WKContentWorld.pageWorld completionHandler:completionHandler];
}

- (void)_callAsyncJavaScript:(NSString *)functionBody arguments:(NSDictionary<NSString *, id> *)arguments inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:functionBody asAsyncFunction:YES withSourceURL:nil withArguments:arguments forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}


- (BOOL)_allMediaPresentationsClosed
{
#if ENABLE(FULLSCREEN_API)
    bool hasOpenMediaPresentations = false;
    if (auto videoPresentationManager = _page->videoPresentationManager()) {
        hasOpenMediaPresentations = videoPresentationManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
            || videoPresentationManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard);
    }

    if (!hasOpenMediaPresentations && _page->fullScreenManager() && _page->fullScreenManager()->isFullScreen())
        hasOpenMediaPresentations = true;

    return !hasOpenMediaPresentations;
#else
    return true;
#endif
}

- (void)_evaluateJavaScript:(NSString *)javaScriptString inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)_evaluateJavaScript:(NSString *)javaScriptString withSourceURL:(NSURL *)url inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:url withArguments:nil forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)_evaluateJavaScript:(NSString *)javaScriptString withSourceURL:(NSURL *)url inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld withUserGesture:(BOOL)withUserGesture completionHandler:(void (^)(id, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:url withArguments:nil forceUserGesture:withUserGesture inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)_updateWebpagePreferences:(WKWebpagePreferences *)webpagePreferences
{
    THROW_IF_SUSPENDED;
    if (webpagePreferences._websiteDataStore)
        [NSException raise:NSInvalidArgumentException format:@"Updating WKWebsiteDataStore is only supported during decidePolicyForNavigationAction."];
    if (webpagePreferences._userContentController)
        [NSException raise:NSInvalidArgumentException format:@"Updating WKUserContentController is only supported during decidePolicyForNavigationAction."];
    auto data = webpagePreferences->_websitePolicies->data();
    _page->updateWebsitePolicies(WTFMove(data));
}

- (void)_notifyUserScripts
{
    THROW_IF_SUSPENDED;
    _page->notifyUserScripts();
}

- (BOOL)_deferrableUserScriptsNeedNotification
{
    THROW_IF_SUSPENDED;
    return _page->userScriptsNeedNotification();
}

- (BOOL)_allowsRemoteInspection
{
    return self.inspectable;
}

- (void)_setAllowsRemoteInspection:(BOOL)allow
{
    self.inspectable = allow;
}

- (NSString *)_remoteInspectionNameOverride
{
#if ENABLE(REMOTE_INSPECTOR)
    return _page->remoteInspectionNameOverride();
#else
    return nil;
#endif
}

- (void)_setRemoteInspectionNameOverride:(NSString *)name
{
    THROW_IF_SUSPENDED;
#if ENABLE(REMOTE_INSPECTOR)
    _page->setRemoteInspectionNameOverride(name);
#endif
}

- (BOOL)_addsVisitedLinks
{
    return _page->addsVisitedLinks();
}

- (void)_setAddsVisitedLinks:(BOOL)addsVisitedLinks
{
    THROW_IF_SUSPENDED;
    _page->setAddsVisitedLinks(addsVisitedLinks);
}

- (NSArray<NSString *> *)_corsDisablingPatterns
{
    return createNSArray(_page->corsDisablingPatterns()).autorelease();
}

- (void)_setCORSDisablingPatterns:(NSArray<NSString *> *)patterns
{
    _page->setCORSDisablingPatterns(makeVector<String>(patterns));
}

- (BOOL)_networkRequestsInProgress
{
    return _page->pageLoadState().networkRequestsInProgress();
}

static inline OptionSet<WebCore::LayoutMilestone> layoutMilestones(_WKRenderingProgressEvents events)
{
    OptionSet<WebCore::LayoutMilestone> milestones;

    if (events & _WKRenderingProgressEventFirstLayout)
        milestones.add(WebCore::LayoutMilestone::DidFirstLayout);

    if (events & _WKRenderingProgressEventFirstVisuallyNonEmptyLayout)
        milestones.add(WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout);

    if (events & _WKRenderingProgressEventFirstPaintWithSignificantArea)
        milestones.add(WebCore::LayoutMilestone::DidHitRelevantRepaintedObjectsAreaThreshold);

    if (events & _WKRenderingProgressEventReachedSessionRestorationRenderTreeSizeThreshold)
        milestones.add(WebCore::LayoutMilestone::ReachedSessionRestorationRenderTreeSizeThreshold);

    if (events & _WKRenderingProgressEventFirstLayoutAfterSuppressedIncrementalRendering)
        milestones.add(WebCore::LayoutMilestone::DidFirstLayoutAfterSuppressedIncrementalRendering);

    if (events & _WKRenderingProgressEventFirstPaintAfterSuppressedIncrementalRendering)
        milestones.add(WebCore::LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering);

    if (events & _WKRenderingProgressEventDidRenderSignificantAmountOfText)
        milestones.add(WebCore::LayoutMilestone::DidRenderSignificantAmountOfText);

    if (events & _WKRenderingProgressEventFirstMeaningfulPaint)
        milestones.add(WebCore::LayoutMilestone::DidFirstMeaningfulPaint);

    return milestones;
}

- (void)_setObservedRenderingProgressEvents:(_WKRenderingProgressEvents)observedRenderingProgressEvents
{
    _observedRenderingProgressEvents = observedRenderingProgressEvents;
    _page->listenForLayoutMilestones(layoutMilestones(observedRenderingProgressEvents));
}

- (void)_saveResources:(NSURL *)directory suggestedFileName:(NSString *)name completionHandler:(void (^)(NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->saveResources(_page->mainFrame(), { }, directory.path, name, [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler([NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: WebCore::errorDescription(result.error()) }]);

        completionHandler(nil);
    });
}

- (void)_archiveWithConfiguration:(_WKArchiveConfiguration*)configuration completionHandler:(void (^)(NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;

    if (!configuration)
        [NSException raise:NSInvalidArgumentException format:@"Configuration cannot be nil"];

    Vector<WebCore::MarkupExclusionRule> markupExclusionRules;
    for (_WKArchiveExclusionRule *rule in configuration.exclusionRules) {
        if (!rule.elementLocalName && (!rule.attributeLocalNames || !rule.attributeLocalNames.count))
            continue;
        Vector<std::pair<AtomString, AtomString>> attibutes;
        for (unsigned index = 0; index < rule.attributeLocalNames.count; ++index) {
            NSString *attributeLocalName = [rule.attributeLocalNames objectAtIndex:index];
            NSString *attributeValue = [rule.attributeValues objectAtIndex:index];
            if (attributeLocalName && !attributeLocalName.length)
                attibutes.append({ AtomString { attributeLocalName }, attributeValue });
        }
        markupExclusionRules.append(WebCore::MarkupExclusionRule { AtomString { rule.elementLocalName }, WTFMove(attibutes) });
    }

    _page->saveResources(_page->mainFrame(), WTFMove(markupExclusionRules), configuration.directory.path, configuration.suggestedFileName, [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler([NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: WebCore::errorDescription(result.error()) }]);

        completionHandler(nil);
    });
}

- (void)_getMainResourceDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getMainResourceDataOfFrame(_page->mainFrame(), [completionHandler = makeBlockPtr(completionHandler)](API::Data* data) {
        completionHandler(wrapper(data), nil);
    });
}

- (void)_getWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    [self createWebArchiveDataWithCompletionHandler:completionHandler];
}

- (void)_getContentsAsStringWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::No, [handler = makeBlockPtr(completionHandler)](String string) {
        handler(string, nil);
    });
}

- (void)_getContentsAsStringWithCompletionHandlerKeepIPCConnectionAliveForTesting:(void (^)(NSString *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::No, [handler = makeBlockPtr(completionHandler), connection = _page->legacyMainFrameProcess().protectedConnection()](String string) {
        handler(string, nil);
    });
}

- (void)_getContentsOfAllFramesAsStringWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::Yes, [handler = makeBlockPtr(completionHandler)](String string) {
        handler(string);
    });
}

- (void)_getContentsAsAttributedStringWithCompletionHandler:(void (^)(NSAttributedString *, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsAttributedString([handler = makeBlockPtr(completionHandler)](auto& attributedString) {
        if (auto string = attributedString.nsAttributedString())
            handler(string.get(), attributedString.documentAttributesAsNSDictionary().get(), nil);
        else
            handler(nil, nil, createNSError(WKErrorUnknown).get());
    });
}

- (void)_getApplicationManifestWithCompletionHandler:(void (^)(_WKApplicationManifest *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getApplicationManifest([completionHandler = makeBlockPtr(completionHandler)](const std::optional<WebCore::ApplicationManifest>& manifest) {
        if (completionHandler) {
            if (manifest) {
                auto apiManifest = API::ApplicationManifest::create(*manifest);
                completionHandler(wrapper(apiManifest));
            } else
                completionHandler(nil);
        }
    });
}

- (void)_getTextFragmentMatchWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getTextFragmentMatch([completionHandler = makeBlockPtr(completionHandler)](const String& textFragmentMatch) {
        completionHandler(nsStringNilIfNull(textFragmentMatch));
    });
}

- (_WKPaginationMode)_paginationMode
{
    switch (_page->paginationMode()) {
    case WebCore::PaginationMode::Unpaginated:
        return _WKPaginationModeUnpaginated;
    case WebCore::PaginationMode::LeftToRightPaginated:
        return _WKPaginationModeLeftToRight;
    case WebCore::PaginationMode::RightToLeftPaginated:
        return _WKPaginationModeRightToLeft;
    case WebCore::PaginationMode::TopToBottomPaginated:
        return _WKPaginationModeTopToBottom;
    case WebCore::PaginationMode::BottomToTopPaginated:
        return _WKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return _WKPaginationModeUnpaginated;
}

- (void)_setPaginationMode:(_WKPaginationMode)paginationMode
{
    THROW_IF_SUSPENDED;
    WebCore::Pagination::Mode mode;
    switch (paginationMode) {
    case _WKPaginationModeUnpaginated:
        mode = WebCore::PaginationMode::Unpaginated;
        break;
    case _WKPaginationModeLeftToRight:
        mode = WebCore::PaginationMode::LeftToRightPaginated;
        break;
    case _WKPaginationModeRightToLeft:
        mode = WebCore::PaginationMode::RightToLeftPaginated;
        break;
    case _WKPaginationModeTopToBottom:
        mode = WebCore::PaginationMode::TopToBottomPaginated;
        break;
    case _WKPaginationModeBottomToTop:
        mode = WebCore::PaginationMode::BottomToTopPaginated;
        break;
    default:
        return;
    }

    _page->setPaginationMode(mode);
}

- (BOOL)_paginationBehavesLikeColumns
{
    return _page->paginationBehavesLikeColumns();
}

- (void)_setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns
{
    THROW_IF_SUSPENDED;
    _page->setPaginationBehavesLikeColumns(behavesLikeColumns);
}

- (CGFloat)_pageLength
{
    return _page->pageLength();
}

- (void)_setPageLength:(CGFloat)pageLength
{
    THROW_IF_SUSPENDED;
    _page->setPageLength(pageLength);
}

- (CGFloat)_gapBetweenPages
{
    return _page->gapBetweenPages();
}

- (void)_setGapBetweenPages:(CGFloat)gapBetweenPages
{
    THROW_IF_SUSPENDED;
    _page->setGapBetweenPages(gapBetweenPages);
}

- (BOOL)_paginationLineGridEnabled
{
    return NO;
}

- (void)_setPaginationLineGridEnabled:(BOOL)lineGridEnabled
{
}

- (NSUInteger)_pageCount
{
    return _page->pageCount();
}

- (BOOL)_supportsTextZoom
{
    return _page->supportsTextZoom();
}

- (double)_textZoomFactor
{
    return _page->textZoomFactor();
}

- (void)_setTextZoomFactor:(double)zoomFactor
{
    THROW_IF_SUSPENDED;
#if PLATFORM(MAC)
    _impl->suppressContentRelativeChildViews(WebKit::WebViewImpl::ContentRelativeChildViewsSuppressionType::TemporarilyRemove);
#endif
    _page->setTextZoomFactor(zoomFactor);
}

- (double)_pageZoomFactor
{
    return [self pageZoom];
}

- (void)_setPageZoomFactor:(double)zoomFactor
{
    THROW_IF_SUSPENDED;
    [self setPageZoom:zoomFactor];
}

- (id <_WKDiagnosticLoggingDelegate>)_diagnosticLoggingDelegate
{
    auto* diagnosticLoggingClient = _page->diagnosticLoggingClient();
    if (!diagnosticLoggingClient)
        return nil;

    return static_cast<WebKit::DiagnosticLoggingClient&>(*diagnosticLoggingClient).delegate().autorelease();
}

- (void)_setDiagnosticLoggingDelegate:(id<_WKDiagnosticLoggingDelegate>)diagnosticLoggingDelegate
{
    auto* diagnosticLoggingClient = _page->diagnosticLoggingClient();
    if (!diagnosticLoggingClient)
        return;

    static_cast<WebKit::DiagnosticLoggingClient&>(*diagnosticLoggingClient).setDelegate(diagnosticLoggingDelegate);
}

- (id <_WKFindDelegate>)_findDelegate
{
    return static_cast<WebKit::FindClient&>(_page->findClient()).delegate().autorelease();
}

- (void)_setFindDelegate:(id<_WKFindDelegate>)findDelegate
{
    static_cast<WebKit::FindClient&>(_page->findClient()).setDelegate(findDelegate);
}

static inline OptionSet<WebKit::FindOptions> toFindOptions(_WKFindOptions wkFindOptions)
{
    OptionSet<WebKit::FindOptions> findOptions;

    if (wkFindOptions & _WKFindOptionsCaseInsensitive)
        findOptions.add(WebKit::FindOptions::CaseInsensitive);
    if (wkFindOptions & _WKFindOptionsAtWordStarts)
        findOptions.add(WebKit::FindOptions::AtWordStarts);
    if (wkFindOptions & _WKFindOptionsTreatMedialCapitalAsWordStart)
        findOptions.add(WebKit::FindOptions::TreatMedialCapitalAsWordStart);
    if (wkFindOptions & _WKFindOptionsBackwards)
        findOptions.add(WebKit::FindOptions::Backwards);
    if (wkFindOptions & _WKFindOptionsWrapAround)
        findOptions.add(WebKit::FindOptions::WrapAround);
    if (wkFindOptions & _WKFindOptionsShowOverlay)
        findOptions.add(WebKit::FindOptions::ShowOverlay);
    if (wkFindOptions & _WKFindOptionsShowFindIndicator)
        findOptions.add(WebKit::FindOptions::ShowFindIndicator);
    if (wkFindOptions & _WKFindOptionsShowHighlight)
        findOptions.add(WebKit::FindOptions::ShowHighlight);
    if (wkFindOptions & _WKFindOptionsNoIndexChange)
        findOptions.add(WebKit::FindOptions::NoIndexChange);
    if (wkFindOptions & _WKFindOptionsDetermineMatchIndex)
        findOptions.add(WebKit::FindOptions::DetermineMatchIndex);

    return findOptions;
}

- (void)_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    THROW_IF_SUSPENDED;
#if PLATFORM(IOS_FAMILY)
    if (_customContentView) {
        [_customContentView web_countStringMatches:string options:options maxCount:maxCount];
        return;
    }
#endif
    _page->countStringMatches(string, toFindOptions(options), maxCount);
}

- (void)_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    THROW_IF_SUSPENDED;
#if PLATFORM(IOS_FAMILY)
    // While AppKit contains logic in NSBarTextFinder to automatically update the find pasteboard
    // when the find string changes, this (along with the find pasteboard itself) are both missing
    // from iOS; thus, on iOS, we update the current find-in-page string here.
    WebKit::updateStringForFind(string);

    if (_customContentView) {
        [_customContentView web_findString:string options:options maxCount:maxCount];
        return;
    }
#endif
    _page->findString(string, toFindOptions(options), maxCount);
}

- (void)_hideFindUI
{
    THROW_IF_SUSPENDED;
#if PLATFORM(IOS_FAMILY)
    if (_customContentView) {
        [_customContentView web_hideFindUI];
        return;
    }
#endif
    _page->hideFindUI();
}

- (void)_saveBackForwardSnapshotForItem:(WKBackForwardListItem *)item
{
    THROW_IF_SUSPENDED;
    if (!item)
        return;
    _page->recordNavigationSnapshot(item._item);
}

- (void)_serviceWorkersEnabled:(void(^)(BOOL))completionHandler
{
    auto enabled = [_configuration preferences]->_preferences.get()->serviceWorkersEnabled();
    completionHandler(enabled);
}

- (void)_clearServiceWorkerEntitlementOverride:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->clearServiceWorkerEntitlementOverride([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_preconnectToServer:(NSURL *)url
{
    THROW_IF_SUSPENDED;

    auto request = WebCore::ResourceRequest { url };
    if (auto userAgent = _page->userAgent(); !userAgent.isEmpty())
        request.setHTTPUserAgent(WTFMove(userAgent));
    _page->preconnectTo(WTFMove(request));
}

- (BOOL)_canUseCredentialStorage
{
    return _page->canUseCredentialStorage();
}

- (void)_setCanUseCredentialStorage:(BOOL)canUseCredentialStorage
{
    THROW_IF_SUSPENDED;
    _page->setCanUseCredentialStorage(canUseCredentialStorage);
}

// FIXME: Remove old `-[WKWebView _themeColor]` SPI <rdar://76662644>
- (WebCore::CocoaColor *)_themeColor
{
    return [self themeColor];
}

// FIXME: Remove old `-[WKWebView _pageExtendedBackgroundColor]` SPI <rdar://77789732>
- (WebCore::CocoaColor *)_pageExtendedBackgroundColor
{
    return cocoaColorOrNil(_page->pageExtendedBackgroundColor()).autorelease();
}

- (WebCore::CocoaColor *)_sampledPageTopColor
{
    return cocoaColorOrNil(_page->sampledPageTopColor()).autorelease();
}

- (id <_WKInputDelegate>)_inputDelegate
{
    return _inputDelegate.getAutoreleased();
}

- (void)_setInputDelegate:(id <_WKInputDelegate>)inputDelegate
{
    _inputDelegate = inputDelegate;

    class FormClient : public API::FormClient {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(FormClient);
    public:
        explicit FormClient(WKWebView *webView)
            : m_webView(webView)
        {
        }

        virtual ~FormClient() { }

        void willSubmitForm(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebKit::WebFrameProxy& sourceFrame, const Vector<std::pair<WTF::String, WTF::String>>& textFieldValues, API::Object* userData, CompletionHandler<void()>&& completionHandler) override
        {
            auto webView = m_webView.get();
            if (!webView)
                return completionHandler();

            auto inputDelegate = webView->_inputDelegate.get();

            if (![inputDelegate respondsToSelector:@selector(_webView:willSubmitFormValues:userObject:submissionHandler:)]) {
                completionHandler();
                return;
            }

            auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:textFieldValues.size()]);
            for (const auto& pair : textFieldValues)
                [valueMap setObject:pair.second forKey:pair.first];

            auto userObject = userData ? userData->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>();

            auto checker = WebKit::CompletionHandlerCallChecker::create(inputDelegate.get(), @selector(_webView:willSubmitFormValues:userObject:submissionHandler:));
            [inputDelegate _webView:webView.get() willSubmitFormValues:valueMap.get() userObject:userObject.get() submissionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] () mutable {
                if (checker->completionHandlerHasBeenCalled())
                    return;
                checker->didCallCompletionHandler();
                completionHandler();
            }).get()];
        }

    private:
        WeakObjCPtr<WKWebView> m_webView;
    };

    if (inputDelegate)
        _page->setFormClient(makeUnique<FormClient>(self));
    else
        _page->setFormClient(nullptr);
}

- (BOOL)_isDisplayingPDF
{
    auto mainFrameIsDisplayingPDFDocument = [page = WeakPtr { _page.get() }] {
        if (!page)
            return false;
        if (RefPtr mainFrame = page->mainFrame())
            return mainFrame->isDisplayingPDFDocument();
        return false;
    };

    bool unifiedPDFEnabled = false;
#if ENABLE(UNIFIED_PDF)
    unifiedPDFEnabled = _page->preferences().unifiedPDFEnabled();
#endif

    bool hasCustomContentViewForPDFType = false;
#if ENABLE(WKPDFVIEW)
    hasCustomContentViewForPDFType = [weakSelf = WeakObjCPtr<WKWebView>(self)] {
        if (![WKPDFView platformSupportsPDFView])
            return false;

        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return false;

        for (auto& type : WebCore::MIMETypeRegistry::pdfMIMETypes()) {
            Class providerClass = [strongSelf->_contentProviderRegistry providerForMIMEType:@(type.characters())];
            if ([strongSelf->_customContentView isKindOfClass:providerClass])
                return true;
        }

        return false;
    }();
#endif

    return static_cast<BOOL>(!unifiedPDFEnabled && hasCustomContentViewForPDFType ?: mainFrameIsDisplayingPDFDocument());
}

- (BOOL)_isDisplayingStandaloneImageDocument
{
    if (auto* mainFrame = _page->mainFrame())
        return mainFrame->isDisplayingStandaloneImageDocument();
    return NO;
}

- (BOOL)_isDisplayingStandaloneMediaDocument
{
    if (auto* mainFrame = _page->mainFrame())
        return mainFrame->isDisplayingStandaloneMediaDocument();
    return NO;
}

- (BOOL)_isPlayingAudio
{
    return _page->isPlayingAudio();
}

- (BOOL)_isShowingNavigationGestureSnapshot
{
    return _page->isShowingNavigationGestureSnapshot();
}

- (_WKLayoutMode)_layoutMode
{
#if PLATFORM(MAC)
    switch (_impl->layoutMode()) {
    case kWKLayoutModeFixedSize:
        return _WKLayoutModeFixedSize;
    case kWKLayoutModeDynamicSizeComputedFromViewScale:
        return _WKLayoutModeDynamicSizeComputedFromViewScale;
    case kWKLayoutModeDynamicSizeComputedFromMinimumDocumentSize:
        return _WKLayoutModeDynamicSizeComputedFromMinimumDocumentSize;
    case kWKLayoutModeViewSize:
    default:
        return _WKLayoutModeViewSize;
    }
#else
    return _page->useFixedLayout() ? _WKLayoutModeFixedSize : _WKLayoutModeViewSize;
#endif
}

- (void)_setLayoutMode:(_WKLayoutMode)layoutMode
{
    THROW_IF_SUSPENDED;
#if PLATFORM(MAC)
    WKLayoutMode wkViewLayoutMode;
    switch (layoutMode) {
    case _WKLayoutModeFixedSize:
        wkViewLayoutMode = kWKLayoutModeFixedSize;
        break;
    case _WKLayoutModeDynamicSizeComputedFromViewScale:
        wkViewLayoutMode = kWKLayoutModeDynamicSizeComputedFromViewScale;
        break;
    case _WKLayoutModeDynamicSizeComputedFromMinimumDocumentSize:
        wkViewLayoutMode = kWKLayoutModeDynamicSizeComputedFromMinimumDocumentSize;
        break;
    case _WKLayoutModeViewSize:
    default:
        wkViewLayoutMode = kWKLayoutModeViewSize;
        break;
    }
    _impl->setLayoutMode(wkViewLayoutMode);
#else
    _page->setUseFixedLayout(layoutMode == _WKLayoutModeFixedSize || layoutMode == _WKLayoutModeDynamicSizeComputedFromViewScale);
#endif
}

- (CGSize)_fixedLayoutSize
{
    return _page->fixedLayoutSize();
}

- (void)_setFixedLayoutSize:(CGSize)fixedLayoutSize
{
    THROW_IF_SUSPENDED;
    _page->setFixedLayoutSize(WebCore::expandedIntSize(WebCore::FloatSize(fixedLayoutSize)));
}

- (void)_setBackgroundExtendsBeyondPage:(BOOL)backgroundExtends
{
    THROW_IF_SUSPENDED;
    _page->setBackgroundExtendsBeyondPage(backgroundExtends);
}

- (BOOL)_backgroundExtendsBeyondPage
{
    return _page->backgroundExtendsBeyondPage();
}

- (CGFloat)_viewScale
{
#if PLATFORM(MAC)
    return _page->viewScaleFactor();
#else
    return _page->layoutSizeScaleFactorFromClient();
#endif
}

- (void)_setViewScale:(CGFloat)viewScale
{
    THROW_IF_SUSPENDED;
    if (viewScale <= 0 || isnan(viewScale) || isinf(viewScale))
        [NSException raise:NSInvalidArgumentException format:@"View scale should be a positive number"];

#if PLATFORM(MAC)
    _impl->setViewScale(viewScale);
#else
    if (_page->layoutSizeScaleFactorFromClient() == viewScale)
        return;

    _page->setViewportConfigurationViewLayoutSize(_page->viewLayoutSize(), viewScale, _page->minimumEffectiveDeviceWidth());
#endif
}

- (void)_getProcessDisplayNameWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getProcessDisplayName([handler = makeBlockPtr(completionHandler)](auto&& name) {
        handler(name);
    });
}

- (void)_setMinimumEffectiveDeviceWidth:(CGFloat)minimumEffectiveDeviceWidth
{
    THROW_IF_SUSPENDED;
#if PLATFORM(IOS_FAMILY)
    if (_page->minimumEffectiveDeviceWidth() == minimumEffectiveDeviceWidth)
        return;

    if (!self._shouldDeferGeometryUpdates)
        _page->setViewportConfigurationViewLayoutSize(_page->viewLayoutSize(), _page->layoutSizeScaleFactorFromClient(), minimumEffectiveDeviceWidth);
    else
        _page->setMinimumEffectiveDeviceWidthWithoutViewportConfigurationUpdate(minimumEffectiveDeviceWidth);
#endif
}

- (CGFloat)_minimumEffectiveDeviceWidth
{
#if PLATFORM(IOS_FAMILY)
    return _page->minimumEffectiveDeviceWidth();
#else
    return 0;
#endif
}

#pragma mark - scrollPerformanceData

- (void)_setScrollPerformanceDataCollectionEnabled:(BOOL)enabled
{
    THROW_IF_SUSPENDED;
    _page->setScrollPerformanceDataCollectionEnabled(enabled);
}

- (BOOL)_scrollPerformanceDataCollectionEnabled
{
    return _page->scrollPerformanceDataCollectionEnabled();
}

- (NSArray *)_scrollPerformanceData
{
#if PLATFORM(IOS_FAMILY)
    if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = _page->scrollingPerformanceData())
        return scrollPerfData->data();
#endif
    return nil;
}

#pragma mark - Media playback restrictions

- (BOOL)_allowsMediaDocumentInlinePlayback
{
#if PLATFORM(IOS_FAMILY)
    return _page->allowsMediaDocumentInlinePlayback();
#else
    return NO;
#endif
}

- (void)_setAllowsMediaDocumentInlinePlayback:(BOOL)flag
{
    THROW_IF_SUSPENDED;
#if PLATFORM(IOS_FAMILY)
    _page->setAllowsMediaDocumentInlinePlayback(flag);
#endif
}

// FIXME: Remove this after Safari adopts the new API
- (void)_setFullscreenDelegate:(id<_WKFullscreenDelegate>)delegate
{
#if ENABLE(FULLSCREEN_API)
    if (auto* client = dynamicDowncast<WebKit::FullscreenClient>(_page->fullscreenClient()))
        client->setDelegate(delegate);
#endif
}

// FIXME: Remove this after Safari adopts the new API
- (id<_WKFullscreenDelegate>)_fullscreenDelegate
{
#if ENABLE(FULLSCREEN_API)
    if (auto* client = dynamicDowncast<WebKit::FullscreenClient>(_page->fullscreenClient()))
        return client->delegate().autorelease();
#endif
    return nil;
}

// FIXME: Remove this after Safari adopts the new API
- (BOOL)_isInFullscreen
{
#if ENABLE(FULLSCREEN_API)
    return _page->fullScreenManager() && _page->fullScreenManager()->isFullScreen();
#else
    return false;
#endif
}

- (_WKMediaCaptureStateDeprecated)_mediaCaptureState
{
    return WebKit::toWKMediaCaptureStateDeprecated(_page->reportedMediaState());
}

- (void)_setMediaCaptureEnabled:(BOOL)enabled
{
    THROW_IF_SUSPENDED;
    _page->setMediaCaptureEnabled(enabled);
}

- (BOOL)_mediaCaptureEnabled
{
    return _page->mediaCaptureEnabled();
}

- (void)_setPageMuted:(_WKMediaMutedState)mutedState
{
    THROW_IF_SUSPENDED;
    WebCore::MediaProducerMutedStateFlags coreState;

    if (mutedState & _WKMediaAudioMuted)
        coreState.add(WebCore::MediaProducerMutedState::AudioIsMuted);
    if (mutedState & _WKMediaCaptureDevicesMuted)
        coreState.add(WebCore::MediaProducer::AudioAndVideoCaptureIsMuted);
    if (mutedState & _WKMediaScreenCaptureMuted)
        coreState.add(WebCore::MediaProducerMutedState::ScreenCaptureIsMuted);

    _page->setMuted(coreState, WebKit::WebPageProxy::FromApplication::Yes);
}

- (void)_removeDataDetectedLinks:(dispatch_block_t)completion
{
    THROW_IF_SUSPENDED;
#if ENABLE(DATA_DETECTION)
    _page->removeDataDetectedLinks([completion = makeBlockPtr(completion), page = WeakPtr { _page.get() }] (auto& result) {
        if (page)
            page->setDataDetectionResult(result);
        if (completion)
            completion();
    });
#else
    UNUSED_PARAM(completion);
#endif
}

// Execute the supplied block after the next transaction from the WebProcess.
- (void)_doAfterNextPresentationUpdate:(void (^)(void))updateBlock
{
    THROW_IF_SUSPENDED;
    [self _internalDoAfterNextPresentationUpdate:updateBlock withoutWaitingForPainting:NO withoutWaitingForAnimatedResize:NO];
}

- (void)_doAfterNextPresentationUpdateWithoutWaitingForPainting:(void (^)(void))updateBlock
{
    THROW_IF_SUSPENDED;
    [self _internalDoAfterNextPresentationUpdate:updateBlock withoutWaitingForPainting:YES withoutWaitingForAnimatedResize:NO];
}

- (void)_doAfterNextVisibleContentRectUpdate:(void (^)(void))updateBlock
{
#if PLATFORM(IOS_FAMILY)
    _visibleContentRectUpdateCallbacks.append(makeBlockPtr(updateBlock));
    [self _scheduleVisibleContentRectUpdate];
#else
    RunLoop::main().dispatch([updateBlock = makeBlockPtr(updateBlock)] {
        updateBlock();
    });
#endif
}

- (WKDisplayCaptureSurfaces) _displayCaptureSurfaces
{
    auto pageState = _page->reportedMediaState();
    WKDisplayCaptureSurfaces state = WKDisplayCaptureSurfaceNone;
    if (pageState.containsAny(WebCore::MediaProducer::ScreenCaptureMask))
        state |= WKDisplayCaptureSurfaceScreen;
    if (pageState.containsAny(WebCore::MediaProducer::WindowCaptureMask))
        state |= WKDisplayCaptureSurfaceWindow;
    return state;
}

- (WKDisplayCaptureState) _displayCaptureState
{
    auto state = _page->reportedMediaState();
    if (state & WebCore::MediaProducer::ActiveDisplayCaptureMask)
        return WKDisplayCaptureStateActive;
    if (state & WebCore::MediaProducer::MutedDisplayCaptureMask)
        return WKDisplayCaptureStateMuted;
    return WKDisplayCaptureStateNone;
}

- (WKSystemAudioCaptureState)_systemAudioCaptureState
{
    auto state = _page->reportedMediaState();
    if (state & WebCore::MediaProducerMediaState::HasActiveSystemAudioCaptureDevice)
        return WKSystemAudioCaptureStateActive;
    if (state & WebCore::MediaProducerMediaState::HasMutedSystemAudioCaptureDevice)
        return WKSystemAudioCaptureStateMuted;
    return WKSystemAudioCaptureStateNone;
}

- (void)_setDisplayCaptureState:(WKDisplayCaptureState)state completionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler)
        completionHandler = [] { };

    if (state == WKDisplayCaptureStateNone) {
        _page->stopMediaCapture(WebCore::MediaProducerMediaCaptureKind::Display, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
        return;
    }

    constexpr WebCore::MediaProducer::MutedStateFlags displayMutedFlags = { WebCore::MediaProducer::MutedState::ScreenCaptureIsMuted, WebCore::MediaProducer::MutedState::WindowCaptureIsMuted };
    auto mutedState = _page->mutedStateFlags();
    if (state == WKDisplayCaptureStateActive)
        mutedState.remove(displayMutedFlags);
    else if (state == WKDisplayCaptureStateMuted)
        mutedState.add(displayMutedFlags);
    _page->setMuted(mutedState, WebKit::WebPageProxy::FromApplication::Yes, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setSystemAudioCaptureState:(WKSystemAudioCaptureState)state completionHandler:(void (^)(void))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!completionHandler)
        completionHandler = [] { };

    if (state == WKSystemAudioCaptureStateNone) {
        _page->stopMediaCapture(WebCore::MediaProducerMediaCaptureKind::SystemAudio, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
        return;
    }
    auto mutedState = _page->mutedStateFlags();
    if (state == WKSystemAudioCaptureStateActive)
        mutedState.remove(WebCore::MediaProducerMutedState::WindowCaptureIsMuted);
    else if (state == WKSystemAudioCaptureStateMuted)
        mutedState.add(WebCore::MediaProducerMutedState::WindowCaptureIsMuted);
    _page->setMuted(mutedState, WebKit::WebPageProxy::FromApplication::Yes, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setOverrideDeviceScaleFactor:(CGFloat)deviceScaleFactor
{
    _page->setCustomDeviceScaleFactor(deviceScaleFactor, []() { });
}

- (CGFloat)_overrideDeviceScaleFactor
{
    return _page->customDeviceScaleFactor().value_or(0);
}

+ (void)_permissionChanged:(NSString *)permissionName forOrigin:(WKSecurityOrigin *)origin
{
    auto name = WebCore::Permissions::toPermissionName(permissionName);
    if (!name)
        return;

    WebKit::WebProcessProxy::permissionChanged(*name, origin->_securityOrigin->securityOrigin());
}

- (NSURL *)_requiredWebExtensionBaseURL
{
    return [_configuration _requiredWebExtensionBaseURL];
}

static Vector<Ref<API::TargetedElementInfo>> elementsFromWKElements(NSArray<_WKTargetedElementInfo *> *wkElements)
{
    Vector<Ref<API::TargetedElementInfo>> elements;
    elements.reserveInitialCapacity(wkElements.count);
    for (_WKTargetedElementInfo *element in wkElements)
        elements.append(*element->_info);
    return elements;
}

- (void)_resetVisibilityAdjustmentsForTargetedElements:(NSArray<_WKTargetedElementInfo *> *)elements completionHandler:(void(^)(BOOL success))completion
{
    _page->resetVisibilityAdjustmentsForTargetedElements(elementsFromWKElements(elements), [completion = makeBlockPtr(completion)](bool success) {
        completion(static_cast<BOOL>(success));
    });
}

- (void)_adjustVisibilityForTargetedElements:(NSArray<_WKTargetedElementInfo *> *)elements completionHandler:(void(^)(BOOL success))completion
{
    _page->adjustVisibilityForTargetedElements(elementsFromWKElements(elements), [completion = makeBlockPtr(completion)](bool success) {
        completion(static_cast<BOOL>(success));
    });
}

- (void)_numberOfVisibilityAdjustmentRectsWithCompletionHandler:(void(^)(NSUInteger))completion
{
    _page->numberOfVisibilityAdjustmentRects([completion = makeBlockPtr(completion)](uint64_t count) {
        completion(static_cast<NSUInteger>(count));
    });
}

- (void)_playPredominantOrNowPlayingMediaSession:(void(^)(BOOL))completionHandler
{
    if (!self._isValid)
        return completionHandler(NO);

    _page->playPredominantOrNowPlayingMediaSession([completionHandler = makeBlockPtr(completionHandler)](bool success) {
        completionHandler(static_cast<BOOL>(success));
    });
}

- (void)_pauseNowPlayingMediaSession:(void(^)(BOOL))completionHandler
{
    if (!self._isValid)
        return completionHandler(NO);

    _page->pauseNowPlayingMediaSession([completionHandler = makeBlockPtr(completionHandler)](bool success) {
        completionHandler(static_cast<BOOL>(success));
    });
}

- (void)_simulateClickOverFirstMatchingTextInViewportWithUserInteraction:(NSString *)targetText completionHandler:(void(^)(BOOL))completionHandler
{
    if (!targetText.length)
        [NSException raise:NSInvalidArgumentException format:@"The target text must be non-empty."];

    if (!self._isValid)
        return completionHandler(NO);

    _page->simulateClickOverFirstMatchingTextInViewportWithUserInteraction(targetText, [completionHandler = makeBlockPtr(completionHandler)](bool success) {
        completionHandler(static_cast<BOOL>(success));
    });
}

- (BOOL)_dontResetTransientActivationAfterRunJavaScript
{
    return _dontResetTransientActivationAfterRunJavaScript;
}

- (void)_setDontResetTransientActivationAfterRunJavaScript:(BOOL)value
{
    _dontResetTransientActivationAfterRunJavaScript = value;
}

#if USE(UICONTEXTMENU)
- (void)_targetedPreviewForElementWithID:(NSString *)elementID completionHandler:(void (^)(UITargetedPreview *))completionHandler
{
    _page->createTextIndicatorForElementWithID(elementID, [completionHandler = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKWebView>(self)](auto&& textIndicatorData) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completionHandler(nil);
            return;
        }

        if (!textIndicatorData) {
            completionHandler(nil);
            return;
        }

        RetainPtr preview = [strongSelf->_contentView _createTargetedPreviewFromTextIndicator:*textIndicatorData previewContainer:[strongSelf scrollView]];
        completionHandler(preview.get());
    });
}
#elif PLATFORM(MAC)
- (void)_textPreviewsForElementWithID:(NSString *)elementID completionHandler:(void (^)(NSArray<_WKTextPreview *> *))completionHandler
{
    _page->createTextIndicatorForElementWithID(elementID, [completionHandler = makeBlockPtr(completionHandler)](auto&& textIndicatorData) {
        if (!textIndicatorData) {
            completionHandler(@[ ]);
            return;
        }

        RefPtr contentImage = textIndicatorData->contentImage;
        if (!contentImage) {
            ASSERT_NOT_REACHED();
            completionHandler(@[ ]);
            return;
        }

        RefPtr nativeImage = contentImage->nativeImage();
        if (!nativeImage) {
            ASSERT_NOT_REACHED();
            completionHandler(@[ ]);
            return;
        }

        RetainPtr platformImage = nativeImage->platformImage();

        auto textBoundingRectInRootViewCoordinates = textIndicatorData->textBoundingRectInRootViewCoordinates;
        auto textRectsInBoundingRectCoordinates = textIndicatorData->textRectsInBoundingRectCoordinates;
        auto contentImageScaleFactor = textIndicatorData->contentImageScaleFactor;

        RetainPtr previews = createNSArray(textRectsInBoundingRectCoordinates, [platformImage, textBoundingRectInRootViewCoordinates, contentImageScaleFactor](auto textRectInBoundingRectCoordinates) -> _WKTextPreview * {
            auto croppedTextRectInImageCoordinates = textRectInBoundingRectCoordinates;
            croppedTextRectInImageCoordinates.scale(contentImageScaleFactor);

            RetainPtr textImage = adoptCF(CGImageCreateWithImageInRect(platformImage.get(), croppedTextRectInImageCoordinates));

            auto presentationFrame = CGRectOffset(textRectInBoundingRectCoordinates, textBoundingRectInRootViewCoordinates.x(), textBoundingRectInRootViewCoordinates.y());

            RetainPtr textPreview = adoptNS([[_WKTextPreview alloc] initWithSnapshotImage:textImage.get() presentationFrame:presentationFrame]);
            return textPreview.autorelease();
        });

        completionHandler(previews.get());
    });
}
#endif

- (NSUUID *)_enableSourceTextAnimationAfterElementWithID:(NSString *)elementID
{
#if ENABLE(WRITING_TOOLS)
    RetainPtr nsUUID = [NSUUID UUID];

    auto uuid = WTF::UUID::fromNSUUID(nsUUID.get());
    if (!uuid)
        return nil;

    _page->enableSourceTextAnimationAfterElementWithID(elementID);

#if PLATFORM(IOS_FAMILY)
    [_contentView addTextAnimationForAnimationID:nsUUID.get() withData:{ WebCore::TextAnimationType::Initial, WebCore::TextAnimationRunMode::RunAnimation }];
#else
    _impl->addTextAnimationForAnimationID(*uuid, { WebCore::TextAnimationType::Initial, WebCore::TextAnimationRunMode::RunAnimation });
#endif

    return nsUUID.get();
#else
    return nil;
#endif
}

- (NSUUID *)_enableFinalTextAnimationForElementWithID:(NSString *)elementID
{
#if ENABLE(WRITING_TOOLS)
    RetainPtr nsUUID = [NSUUID UUID];

    auto uuid = WTF::UUID::fromNSUUID(nsUUID.get());
    if (!uuid)
        return nil;

    _page->enableTextAnimationTypeForElementWithID(elementID);

#if PLATFORM(IOS_FAMILY)
    [_contentView addTextAnimationForAnimationID:nsUUID.get() withData:{ WebCore::TextAnimationType::Final, WebCore::TextAnimationRunMode::RunAnimation }];
#else
    _impl->addTextAnimationForAnimationID(*uuid, { WebCore::TextAnimationType::Final, WebCore::TextAnimationRunMode::RunAnimation });
#endif

    return nsUUID.get();
#else
    return nil;
#endif
}

- (void)_disableTextAnimationWithUUID:(NSUUID *)nsUUID
{
#if ENABLE(WRITING_TOOLS)
#if PLATFORM(IOS_FAMILY)
    [_contentView removeTextAnimationForAnimationID:nsUUID];
#else
    auto uuid = WTF::UUID::fromNSUUID(nsUUID);
    if (!uuid)
        return;
    _impl->removeTextAnimationForAnimationID(*uuid);
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(WRITING_TOOLS)
}

- (_WKWebProcessState)_webProcessState
{
    if (!_page || !_page->hasRunningProcess())
        return _WKWebProcessStateNotRunning;

    switch (_page->protectedLegacyMainFrameProcess()->throttleStateForStatistics()) {
    case WebKit::ProcessThrottleState::Foreground:
        return _WKWebProcessStateForeground;
    case WebKit::ProcessThrottleState::Background:
        return _WKWebProcessStateBackground;
    case WebKit::ProcessThrottleState::Suspended:
        return _WKWebProcessStateSuspended;
    default:
        return _WKWebProcessStateNotRunning;
    }
}

@end

@implementation WKWebView (WKDeprecated)

- (NSArray *)certificateChain
{
    return (__bridge NSArray *)WebCore::CertificateInfo::certificateChainFromSecTrust(_page->pageLoadState().certificateInfo().trust().get()).autorelease() ?: @[ ];
}

@end

@implementation WKWebView (WKBinaryCompatibilityWithIOS10)

- (id <_WKInputDelegate>)_formDelegate
{
    return self._inputDelegate;
}

- (void)_setFormDelegate:(id <_WKInputDelegate>)formDelegate
{
    self._inputDelegate = formDelegate;
}

@end

@implementation WKWebView (WKTextExtraction)

- (void)_requestTextExtractionForSwift:(WKTextExtractionRequest *)context
{
    [self _requestTextExtraction:context.rectInWebView completionHandler:makeBlockPtr([context = retainPtr(context)](WKTextExtractionItem *result) {
        [context fulfill:result];
    }).get()];
}

- (void)_requestTextExtraction:(CGRect)rectInWebView completionHandler:(void(^)(WKTextExtractionItem *))completionHandler
{
    if (!self._isValid || !_page->preferences().textExtractionEnabled())
        return completionHandler(nil);

    auto rectInRootView = [&]() -> std::optional<WebCore::FloatRect> {
        if (CGRectIsNull(rectInWebView))
            return std::nullopt;

#if PLATFORM(IOS_FAMILY)
        return WebCore::FloatRect { [self convertRect:rectInWebView toView:_contentView.get()] };
#else
        return WebCore::FloatRect { rectInWebView };
#endif
    }();

    _page->requestTextExtraction(WTFMove(rectInRootView), [completionHandler = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKWebView>(self)](auto&& item) {
        completionHandler(WebKit::createItem(item, [strongSelf = weakSelf.get()](auto& rectInRootView) -> WebCore::FloatRect {
#if PLATFORM(IOS_FAMILY)
            if (RetainPtr contentView = strongSelf ? strongSelf->_contentView : nil)
                return { [strongSelf convertRect:rectInRootView fromView:contentView.get()] };
#endif
            return rectInRootView;
        }).get());
    });
}

@end

#undef WKWEBVIEW_RELEASE_LOG
