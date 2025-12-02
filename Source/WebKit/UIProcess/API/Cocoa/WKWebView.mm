/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#import "AboutSchemeHandler.h"
#import "AccessibilityPreferences.h"
#import "BrowsingWarning.h"
#import "CocoaImage.h"
#import "CompletionHandlerCallChecker.h"
#import "ContentAsStringIncludesChildFrames.h"
#import "DefaultWebBrowserChecks.h"
#import "DiagnosticLoggingClient.h"
#import "EditingRange.h"
#import "FindClient.h"
#import "FullscreenClient.h"
#import "GlobalFindInPageState.h"
#import "IconLoadingDelegate.h"
#import "ImageAnalysisUtilities.h"
#import "JavaScriptEvaluationResult.h"
#import "LegacySessionStateCoding.h"
#import "Logging.h"
#import "MediaPlaybackState.h"
#import "MediaUtilities.h"
#import "MessageSenderInlines.h"
#import "NavigationState.h"
#import "NodeHitTestResult.h"
#import "PDFPluginIdentifier.h"
#import "PageClient.h"
#import "PlatformWritingToolsUtilities.h"
#import "ProcessTerminationReason.h"
#import "ProvisionalPageProxy.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "ResourceLoadDelegate.h"
#import "RunJavaScriptParameters.h"
#import "SessionStateCoding.h"
#import "TextExtractionFilter.h"
#import "TextExtractionToStringConversion.h"
#import "UIDelegate.h"
#import "UIKitUtilities.h"
#import "VideoPresentationManagerProxy.h"
#import "ViewGestureController.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextController.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKColorExtensionView.h"
#import "WKContentWorldInternal.h"
#import "WKDataDetectorTypesInternal.h"
#import "WKDownloadInternal.h"
#import "WKErrorInternal.h"
#import "WKFindConfiguration.h"
#import "WKFindResultInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKIntelligenceReplacementTextEffectCoordinator.h"
#import "WKIntelligenceSmartReplyTextEffectCoordinator.h"
#import "WKIntelligenceTextEffectCoordinator.h"
#import "WKLayoutMode.h"
#import "WKNSData.h"
#import "WKNSURLExtras.h"
#import "WKNavigationDelegate.h"
#import "WKNavigationInternal.h"
#import "WKPDFConfiguration.h"
#import "WKPDFPageNumberIndicator.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKScreenTimeConfigurationObserver.h"
#import "WKScrollGeometry.h"
#import "WKSecurityOriginInternal.h"
#import "WKSharedAPICast.h"
#import "WKSnapshotConfigurationPrivate.h"
#import "WKTextExtractionUtilities.h"
#import "WKUIDelegate.h"
#import "WKUIDelegateInternal.h"
#import "WKUIScrollEdgeEffect.h"
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
#import "WebPopupMenuProxyMac.h"
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
#import "_WKImmersiveEnvironmentDelegate.h"
#import "_WKInputDelegate.h"
#import "_WKInspectorInternal.h"
#import "_WKJSHandleInternal.h"
#import "_WKPageLoadTimingInternal.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKSessionStateInternal.h"
#import "_WKSpatialBackdropSourceInternal.h"
#import "_WKTargetedElementInfoInternal.h"
#import "_WKTargetedElementRequestInternal.h"
#import "_WKTextExtractionInternal.h"
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
#import <WebCore/PageColorSampler.h>
#import <WebCore/Pagination.h>
#import <WebCore/Permissions.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/RunJavaScriptParameters.h>
#import <WebCore/Settings.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SpatialBackdropSource.h>
#import <WebCore/StringUtilities.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/TextExtractionTypes.h>
#import <WebCore/TextManipulationController.h>
#import <WebCore/TextManipulationItem.h>
#import <WebCore/ViewportArguments.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebCorePersistentCoders.h>
#import <WebCore/WebViewVisualIdentificationOverlay.h>
#import <WebCore/WritingMode.h>
#import <wtf/BlockPtr.h>
#import <wtf/Box.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/HashMap.h>
#import <wtf/MainThread.h>
#import <wtf/MathExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/Scope.h>
#import <wtf/StdLibExtras.h>
#import <wtf/SystemTracing.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/persistence/PersistentDecoder.h>
#import <wtf/persistence/PersistentEncoder.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/TextBreakIterator.h>
#import <wtf/text/TextStream.h>

#if ENABLE(WK_WEB_EXTENSIONS)
#import "WKWebExtensionControllerInternal.h"
#endif

#if ENABLE(DATA_DETECTION)
#import "DataDetectionResult.h"
#endif

#if HAVE(DIGITAL_CREDENTIALS_UI)
#import <WebKit/WKDigitalCredentialsPicker.h>
#import <WebCore/DigitalCredentialsRequestData.h>
#import <WebCore/DigitalCredentialsResponseData.h>
#import <WebCore/ExceptionData.h>
#endif

#if ENABLE(SCREEN_TIME)
#import <pal/cocoa/ScreenTimeSoftLink.h>
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
#import <wtf/darwin/DispatchExtras.h>

#endif // PLATFORM(IOS_FAMILY)

#define WKWEBVIEW_RELEASE_LOG(...) RELEASE_LOG(ViewState, __VA_ARGS__)

#if PLATFORM(MAC)
#import "AppKitSPI.h"
#import "WKTextFinderClient.h"
#import "WKViewInternal.h"
#import <WebCore/ColorMac.h>
#import <pal/spi/mac/NSViewSPI.h>
#endif

#import "WebKitSwiftSoftLink.h"

#if PLATFORM(WATCHOS)
static const BOOL defaultAllowsViewportShrinkToFit = YES;
static const BOOL defaultFastClickingEnabled = YES;
#elif PLATFORM(IOS_FAMILY)
static const BOOL defaultAllowsViewportShrinkToFit = NO;
static const BOOL defaultFastClickingEnabled = NO;
#endif

#if ENABLE(SCREEN_TIME)
static void *screenTimeWebpageControllerBlockedKVOContext = &screenTimeWebpageControllerBlockedKVOContext;
#endif

#if ENABLE(GAMEPAD) && PLATFORM(VISION) && __has_include(<GameController/GCEventInteraction.h>)
#import <WebCore/GameControllerSoftLink.h>
#endif

#define THROW_IF_SUSPENDED if (_page && _page->isSuspended()) [[unlikely]] \
    [NSException raise:NSInternalInconsistencyException format:@"The WKWebView is suspended"]

RetainPtr<NSError> nsErrorFromExceptionDetails(const std::optional<WebCore::ExceptionDetails>& details)
{
    if (!details)
        return createNSError(WKErrorJavaScriptResultTypeIsUnsupported);

    auto userInfo = adoptNS([[NSMutableDictionary alloc] init]);

    WKErrorCode errorCode;
    switch (details->type) {
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

    [userInfo setObject:localizedDescriptionForErrorCode(errorCode).get() forKey:NSLocalizedDescriptionKey];
    [userInfo setObject:details->message.createNSString().get() forKey:_WKJavaScriptExceptionMessageErrorKey];
    [userInfo setObject:RetainPtr { @(details->lineNumber) }.get() forKey:_WKJavaScriptExceptionLineNumberErrorKey];
    [userInfo setObject:RetainPtr { @(details->columnNumber) }.get() forKey:_WKJavaScriptExceptionColumnNumberErrorKey];

    if (!details->sourceURL.isEmpty()) {
        if (RetainPtr url = URL(details->sourceURL).createNSURL())
            [userInfo setObject:url.get() forKey:_WKJavaScriptExceptionSourceURLErrorKey];
    }

    return adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:errorCode userInfo:userInfo.get()]);
}

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

@interface WKWebView (WKColorExtensionView) <WKColorExtensionViewDelegate>
@end

#endif

@interface WKWebView (WKTextExtraction)
- (void)_requestTextExtractionInternal:(_WKTextExtractionConfiguration *)configuration completion:(CompletionHandler<void(std::optional<WebCore::TextExtraction::Item>&&)>&&)completion;
#if ENABLE(TEXT_EXTRACTION_FILTER)
- (void)_validateText:(const String&)text inNode:(std::optional<WebCore::NodeIdentifier>&&)nodeIdentifier completionHandler:(CompletionHandler<void(const String&)>&&)completionHandler;
#endif
@end

@implementation WKWebView

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

#if HAVE(DIGITAL_CREDENTIALS_UI)
- (void)_showDigitalCredentialsPicker:(const WebCore::DigitalCredentialsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&)completionHandler
{
    LOG(DigitalCredentials, "Did not show digital credentials picker because it is not implemented.");
    completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotSupportedError, "Digital credentials picker not implemented."_s }));
}

- (void)_dismissDigitalCredentialsPicker:(WTF::CompletionHandler<void(bool)>&&)completionHandler
{
    LOG(DigitalCredentials, "Did not dismiss digital credentials picker because it is not implemented.");
    completionHandler(false);
}
#endif // HAVE(DIGITAL_CREDENTIALS_UI)

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

static WebCore::RectEdges<bool> toRectEdges(_WKRectEdge edges)
{
    return {
        static_cast<bool>(edges & _WKRectEdgeTop),
        static_cast<bool>(edges & _WKRectEdgeRight),
        static_cast<bool>(edges & _WKRectEdgeBottom),
        static_cast<bool>(edges & _WKRectEdgeLeft),
    };
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

#if ENABLE(SCREEN_TIME)

- (void)_installScreenTimeWebpageControllerIfNeeded
{
    if (_screenTimeWebpageController)
        return;

    if (!PAL::isScreenTimeFrameworkAvailable())
        return;

    if (!_page->preferences().screenTimeEnabled() || !_page->mainFrame() || !_page->mainFrame()->url().protocolIsInHTTPFamily())
        return;

    if (!_screenTimeConfigurationObserver) {
        _screenTimeConfigurationObserver = adoptNS([[WKScreenTimeConfigurationObserver alloc] initWithView:self]);
        [_screenTimeConfigurationObserver startObserving];
    }

    if (![_screenTimeConfigurationObserver enforcesChildRestrictions])
        return;

    _screenTimeWebpageController = adoptNS([PAL::allocSTWebpageControllerInstance() init]);
    [_screenTimeWebpageController addObserver:self forKeyPath:@"URLIsBlocked" options:(NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew) context:&screenTimeWebpageControllerBlockedKVOContext];
    _isBlockedByScreenTime = NO;

    [_screenTimeWebpageController setProfileIdentifier:[_configuration websiteDataStore].identifier.UUIDString];

    [_screenTimeWebpageController setSuppressUsageRecording:![_configuration websiteDataStore].isPersistent];

    // Observing changes to URLIsBlocked is set up in STWebpageController's loadView function.
    // Thus, we have to instantiate its view for URLIsBlocked to update properly.
    RetainPtr screenTimeView = [_screenTimeWebpageController view];

    if ([_configuration showsSystemScreenTimeBlockingView]) {
#if PLATFORM(MAC)
        [screenTimeView _setHostsAutolayoutEngine:YES];
#endif
        [screenTimeView setFrame:self.bounds];
        [self addSubview:screenTimeView.get()];
    }

    RELEASE_LOG(ScreenTime, "Screen Time controller was installed.");
}

- (void)_uninstallScreenTimeWebpageController
{
    if (!PAL::isScreenTimeFrameworkAvailable())
        return;

    [_screenTimeConfigurationObserver stopObserving];
    _screenTimeConfigurationObserver = nil;

    if (!_screenTimeWebpageController)
        return;

    [std::exchange(_screenTimeBlurredSnapshot, nil) removeFromSuperview];

    [[_screenTimeWebpageController view] removeFromSuperview];
    [_screenTimeWebpageController removeObserver:self forKeyPath:@"URLIsBlocked" context:&screenTimeWebpageControllerBlockedKVOContext];
    _screenTimeWebpageController = nil;
    RELEASE_LOG(ScreenTime, "Screen Time controller was uninstalled.");
}

- (void)_updateScreenTimeViewGeometry
{
    auto bounds = self.bounds;
    [_screenTimeBlurredSnapshot setFrame:bounds];
    [[_screenTimeWebpageController view] setFrame:bounds];
}

- (void)_updateScreenTimeBasedOnWindowVisibility
{
    BOOL viewIsInWindow = !!self.window;
    BOOL viewIsVisible = viewIsInWindow;
#if PLATFORM(MAC)
    viewIsVisible = viewIsVisible && ((self.window.occlusionState & NSWindowOcclusionStateVisible) == NSWindowOcclusionStateVisible);
#endif

    BOOL showsSystemScreenTimeBlockingView = [_configuration showsSystemScreenTimeBlockingView];

    if (viewIsInWindow) {
        BOOL previouslyInstalledScreenTimeWebpageController = !!_screenTimeWebpageController;
        [self _installScreenTimeWebpageControllerIfNeeded];
        if (!previouslyInstalledScreenTimeWebpageController && _screenTimeWebpageController)
            [_screenTimeWebpageController setURL:[self _mainFrameURL]];

        if (!showsSystemScreenTimeBlockingView && _screenTimeBlurredSnapshot) {
            [_screenTimeBlurredSnapshot setHidden:NO];
            RELEASE_LOG(ScreenTime, "Screen Time has updated to use the blurred view for any blocked URL.");
        } else if (showsSystemScreenTimeBlockingView) {
            [[_screenTimeWebpageController view] setHidden:NO];
            RELEASE_LOG(ScreenTime, "Screen Time has updated to use the system shield for any blocked URL.");
        }
    } else {
        if (_screenTimeBlurredSnapshot) {
            [_screenTimeBlurredSnapshot setHidden:YES];
            RELEASE_LOG(ScreenTime, "Screen Time has updated to hide the blurred view for all URLs.");
        } else if (showsSystemScreenTimeBlockingView) {
            [[_screenTimeWebpageController view] setHidden:YES];
            RELEASE_LOG(ScreenTime, "Screen Time has updated to hide the system shield for all URLs.");
        }
    }

    [_screenTimeWebpageController setSuppressUsageRecording:(![_configuration websiteDataStore].isPersistent || !viewIsVisible)];
}

#endif // ENABLE(SCREEN_TIME)

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
#if ENABLE(SCREEN_TIME)
    if (context == &screenTimeWebpageControllerBlockedKVOContext) {
        BOOL urlWasBlocked = dynamic_objc_cast<NSNumber>(change[NSKeyValueChangeOldKey]).boolValue;
        BOOL urlIsBlocked = dynamic_objc_cast<NSNumber>(change[NSKeyValueChangeNewKey]).boolValue;
        BOOL wasBlockedByScreenTime = _isBlockedByScreenTime;

        if (urlWasBlocked != urlIsBlocked) {
            [self setAllMediaPlaybackSuspended:urlIsBlocked completionHandler:nil];
            [self willChangeValueForKey:@"isBlockedByScreenTime"];
            _isBlockedByScreenTime = urlIsBlocked;
            [self didChangeValueForKey:@"isBlockedByScreenTime"];
            if (urlIsBlocked)
                RELEASE_LOG(ScreenTime, "Screen Time is blocking the URL.");
            else
                RELEASE_LOG(ScreenTime, "Screen Time is not blocking the URL.");
        }
        if (wasBlockedByScreenTime != _isBlockedByScreenTime) {
            if (!_screenTimeBlurredSnapshot && ![_configuration showsSystemScreenTimeBlockingView]) {
#if PLATFORM(MAC)
                _screenTimeBlurredSnapshot = adoptNS([[NSVisualEffectView alloc] init]);
                [_screenTimeBlurredSnapshot setMaterial:NSVisualEffectMaterialUnderWindowBackground];
                [_screenTimeBlurredSnapshot setBlendingMode:NSVisualEffectBlendingModeWithinWindow];
#else
                RetainPtr blurEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterial];
                _screenTimeBlurredSnapshot = adoptNS([[UIVisualEffectView alloc] initWithEffect:blurEffect.get()]);
#endif
                [_screenTimeBlurredSnapshot setFrame:self.bounds];
                [self addSubview:_screenTimeBlurredSnapshot.get()];
            } else if (_screenTimeBlurredSnapshot) {
                [_screenTimeBlurredSnapshot removeFromSuperview];
                _screenTimeBlurredSnapshot = nil;
            }
        }
        return;
    }
#endif
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

#if PLATFORM(IOS_FAMILY)

static id browsingContextControllerMethodStubNonNil(id, SEL)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return adoptNS([WKBrowsingContextController new]).autorelease();
ALLOW_DEPRECATED_DECLARATIONS_END
}

static id browsingContextControllerMethodStubNil(id, SEL)
{
    return nil;
}

static void addBrowsingContextControllerMethodStubsIfNeeded()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::BrowsingContextControllerMethodStubRemoved))
            return;

        class_addMethod(WKWebView.class, NSSelectorFromString(@"browsingContextController"), reinterpret_cast<IMP>(browsingContextControllerMethodStubNil), "@@:");
        class_addMethod(WKContentView.class, NSSelectorFromString(@"browsingContextController"), reinterpret_cast<IMP>(browsingContextControllerMethodStubNonNil), "@@:");
    });
}

#endif // PLATFORM(IOS_FAMILY)

- (void)_initializeWithConfiguration:(WKWebViewConfiguration *)configuration
{
    if (!configuration)
        [NSException raise:NSInvalidArgumentException format:@"Configuration cannot be nil"];

    _configuration = adoptNS([configuration copy]);

#if PLATFORM(IOS_FAMILY)
    addBrowsingContextControllerMethodStubsIfNeeded();
#endif

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (RetainPtr relatedWebView = [_configuration _relatedWebView]) {
        RetainPtr processPool = [_configuration processPool];
        RetainPtr relatedWebViewProcessPool = [relatedWebView->_configuration processPool];
        if (processPool && processPool != relatedWebViewProcessPool)
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has process pool %@ but configuration specifies a different process pool %@", relatedWebView.get(), relatedWebViewProcessPool.get(), retainPtr(configuration.processPool).get()];
        if ([relatedWebView->_configuration websiteDataStore] != [_configuration websiteDataStore] && linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ExceptionsForRelatedWebViewsUsingDifferentDataStores))
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has data store %@ but configuration specifies a different data store %@", relatedWebView.get(), retainPtr([relatedWebView->_configuration websiteDataStore]).get(), retainPtr([_configuration websiteDataStore]).get()];

        [_configuration setProcessPool:relatedWebViewProcessPool.get()];
    }
    ALLOW_DEPRECATED_DECLARATIONS_END

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    Ref processPool = *[_configuration processPool]->_processPool;
    ALLOW_DEPRECATED_DECLARATIONS_END

    // FIXME: This copy is probably not necessary.
    Ref pageConfiguration = Ref { *_configuration->_pageConfiguration }->copy();
    [self _setupPageConfiguration:pageConfiguration withPool:processPool.get()];

    _usePlatformFindUI = YES;

#if PLATFORM(IOS_FAMILY)
    _obscuredInsetEdgesAffectedBySafeArea = UIRectEdgeTop | UIRectEdgeLeft | UIRectEdgeRight;
    _allowsViewportShrinkToFit = defaultAllowsViewportShrinkToFit;
    _allowsLinkPreview = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::LinkPreviewEnabledByDefault);
    _findInteractionEnabled = NO;
    _needsToPresentLockdownModeMessage = YES;
    _allowsMagnification = YES;
    _avoidsUnsafeArea = YES;

    auto fastClickingEnabled = []() {
        if (NSNumber *enabledValue = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitFastClickingDisabled"])
            return enabledValue.boolValue;
        return defaultFastClickingEnabled;
    };

    _fastClickingIsDisabled = fastClickingEnabled();
    _dragInteractionPolicy = _WKDragInteractionPolicyDefault;

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:self.bounds processPool:processPool.get() configuration:pageConfiguration.copyRef() webView:self]);
    lazyInitialize(_page, Ref { *[_contentView page] });

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
    lazyInitialize(_impl, makeUnique<WebKit::WebViewImpl>(self, processPool.get(), pageConfiguration.copyRef()));
    lazyInitialize(_page, Ref { _impl->page() });

    _impl->setAutomaticallyAdjustsContentInsets(true);

#if HAVE(INLINE_PREDICTIONS)
    _impl->setInlinePredictionsEnabled(!![_configuration allowsInlinePredictions]);
#endif
#endif

    if (RetainPtr<NSString> applicationNameForUserAgent = configuration.applicationNameForUserAgent)
        _page->setApplicationNameForUserAgent(applicationNameForUserAgent.get());

    _page->setApplicationNameForDesktopUserAgent(configuration._applicationNameForDesktopUserAgent);

    lazyInitialize(_navigationState, makeUniqueWithoutRefCountedCheck<WebKit::NavigationState>(self));
    _page->setNavigationClient(_navigationState->createNavigationClient());

    lazyInitialize(_uiDelegate, makeUniqueWithoutRefCountedCheck<WebKit::UIDelegate>(self));
    _page->setFindClient(makeUnique<WebKit::FindClient>(self));
    _page->setDiagnosticLoggingClient(makeUnique<WebKit::DiagnosticLoggingClient>(self));

    lazyInitialize(_iconLoadingDelegate, makeUnique<WebKit::IconLoadingDelegate>(self));
    lazyInitialize(_resourceLoadDelegate, makeUnique<WebKit::ResourceLoadDelegate>(self));

    for (auto& pair : pageConfiguration->urlSchemeHandlers())
        _page->setURLSchemeHandlerForScheme(pair.value.get(), pair.key);
    _page->setURLSchemeHandlerForScheme(_page->protectedAboutSchemeHandler(), WebKit::AboutSchemeHandler::scheme);

    _page->setCocoaView(self);

    [WebViewVisualIdentificationOverlay installForWebViewIfNeeded:self kind:retainPtr(self._nameForVisualIdentificationOverlay).get() deprecated:NO];

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
    _pointerTouchCompatibilitySimulator = WTF::makeUnique<WebKit::PointerTouchCompatibilitySimulator>(self);
#endif
}

- (void)_setupPageConfiguration:(Ref<API::PageConfiguration>&)pageConfiguration withPool:(WebKit::WebProcessPool&)pool
{
    pageConfiguration->setPreferences([_configuration preferences]->_preferences.get());
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (RetainPtr relatedWebView = [_configuration _relatedWebView])
        pageConfiguration->setRelatedPage(relatedWebView->_page.get());
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (RetainPtr webViewToCloneSessionStorageFrom = [_configuration _webViewToCloneSessionStorageFrom])
        pageConfiguration->setPageToCloneSessionStorageFrom(webViewToCloneSessionStorageFrom->_page.get());

    pageConfiguration->setUserContentController([_configuration userContentController]->_userContentControllerProxy.get());
    pageConfiguration->setVisitedLinkStore([_configuration _visitedLinkStore]->_visitedLinkStore.get());
    pageConfiguration->setWebsiteDataStore([_configuration websiteDataStore]->_websiteDataStore.get());
    pageConfiguration->setDefaultWebsitePolicies([_configuration defaultWebpagePreferences]->_websitePolicies.get());

#if ENABLE(WK_WEB_EXTENSIONS)
    if (RetainPtr<WKWebExtensionController> controller = _configuration.get()._strongWebExtensionController)
        pageConfiguration->setWebExtensionController(&controller.get()._webExtensionController);

    if (RetainPtr<WKWebExtensionController> controller = _configuration.get()._weakWebExtensionController)
        pageConfiguration->setWeakWebExtensionController(Ref { controller.get()._webExtensionController }.ptr());
#endif

    RetainPtr groupIdentifier = [_configuration _groupIdentifier];
    if (groupIdentifier.get().length)
        pageConfiguration->setPageGroup(WebKit::WebPageGroup::create(groupIdentifier.get()).ptr());

    pageConfiguration->setAdditionalSupportedImageTypes(makeVector<String>(retainPtr([_configuration _additionalSupportedImageTypes]).get()));

    pageConfiguration->setWaitsForPaintAfterViewDidMoveToWindow([_configuration _waitsForPaintAfterViewDidMoveToWindow]);
    pageConfiguration->setDrawsBackground([_configuration _drawsBackground]);
    pageConfiguration->setControlledByAutomation([_configuration _isControlledByAutomation]);

    Ref preferences = pageConfiguration->preferences();
    preferences->startBatchingUpdates();

    preferences->setSuppressesIncrementalRendering(!![_configuration suppressesIncrementalRendering]);
#if !PLATFORM(MAC)
    // FIXME: rdar://99156546. Remove this and WKWebViewConfiguration._printsBackgrounds once all iOS clients adopt the new API.
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToExcludingBackgroundsWhenPrinting))
        preferences->setShouldPrintBackgrounds(!![_configuration _printsBackgrounds]);
#endif
    preferences->setIncrementalRenderingSuppressionTimeout([_configuration _incrementalRenderingSuppressionTimeout]);
    preferences->setJavaScriptMarkupEnabled(!![_configuration _allowsJavaScriptMarkup]);
    preferences->setShouldConvertPositionStyleOnCopy(!![_configuration _convertsPositionStyleOnCopy]);
    preferences->setHTTPEquivEnabled(!![_configuration _allowsMetaRefresh]);
    preferences->setAllowUniversalAccessFromFileURLs(!![_configuration _allowUniversalAccessFromFileURLs]);
    preferences->setAllowTopNavigationToDataURLs(!![_configuration _allowTopNavigationToDataURLs]);
    preferences->setIncompleteImageBorderEnabled(!![_configuration _incompleteImageBorderEnabled]);
    preferences->setShouldDeferAsynchronousScriptsUntilAfterDocumentLoadOrFirstPaint(!![_configuration _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad]);
    preferences->setShouldRestrictBaseURLSchemes(shouldRestrictBaseURLSchemes());

#if PLATFORM(MAC)
    preferences->setShowsURLsInToolTipsEnabled(!![_configuration _showsURLsInToolTips]);
    preferences->setServiceControlsEnabled(!![_configuration _serviceControlsEnabled]);
    preferences->setImageControlsEnabled(!![_configuration _imageControlsEnabled]);
    preferences->setContextMenuQRCodeDetectionEnabled(!![_configuration _contextMenuQRCodeDetectionEnabled]);

    preferences->setUserInterfaceDirectionPolicy(convertUserInterfaceDirectionPolicy([_configuration userInterfaceDirectionPolicy]));
    // We are in the View's initialization routine, so our client hasn't had time to set our user interface direction.
    // Therefore, according to the docs[1], "this property contains the value reported by the app's userInterfaceLayoutDirection property."
    // [1] http://developer.apple.com/library/mac/documentation/Cocoa/Reference/ApplicationKit/Classes/NSView_Class/index.html#//apple_ref/doc/uid/20000014-SW222
    preferences->setSystemLayoutDirection(convertSystemLayoutDirection(self.userInterfaceLayoutDirection));
#endif

#if PLATFORM(IOS_FAMILY)
    preferences->setAllowsInlineMediaPlayback(!![_configuration allowsInlineMediaPlayback]);
    preferences->setAllowsInlineMediaPlaybackAfterFullscreen(!![_configuration _allowsInlineMediaPlaybackAfterFullscreen]);
    preferences->setInlineMediaPlaybackRequiresPlaysInlineAttribute(!![_configuration _inlineMediaPlaybackRequiresPlaysInlineAttribute]);
    preferences->setAllowsPictureInPictureMediaPlayback(!![_configuration allowsPictureInPictureMediaPlayback] && shouldAllowPictureInPictureMediaPlayback());
    preferences->setUserInterfaceDirectionPolicy(static_cast<uint32_t>(WebCore::UserInterfaceDirectionPolicy::Content));
    preferences->setSystemLayoutDirection(static_cast<uint32_t>(WebCore::TextDirection::LTR));
    preferences->setShouldDecidePolicyBeforeLoadingQuickLookPreview(!![_configuration _shouldDecidePolicyBeforeLoadingQuickLookPreview]);
#if USE(SYSTEM_PREVIEW)
    preferences->setSystemPreviewEnabled(!![_configuration _systemPreviewEnabled]);
#endif
#endif // PLATFORM(IOS_FAMILY)
    preferences->setScrollToTextFragmentIndicatorEnabled(!![_configuration _scrollToTextFragmentIndicatorEnabled]);
    preferences->setScrollToTextFragmentMarkingEnabled(!![_configuration _scrollToTextFragmentMarkingEnabled]);

    WKAudiovisualMediaTypes mediaTypesRequiringUserGesture = [_configuration mediaTypesRequiringUserActionForPlayback];
    preferences->setRequiresUserGestureForVideoPlayback((mediaTypesRequiringUserGesture & WKAudiovisualMediaTypeVideo) == WKAudiovisualMediaTypeVideo);
    preferences->setRequiresUserGestureForAudioPlayback(((mediaTypesRequiringUserGesture & WKAudiovisualMediaTypeAudio) == WKAudiovisualMediaTypeAudio));
    preferences->setRequiresUserGestureToLoadVideo(shouldRequireUserGestureToLoadVideo());
    preferences->setMainContentUserGestureOverrideEnabled(!![_configuration _mainContentUserGestureOverrideEnabled]);
    preferences->setInvisibleAutoplayNotPermitted(!![_configuration _invisibleAutoplayNotPermitted]);
    preferences->setMediaDataLoadsAutomatically(!![_configuration _mediaDataLoadsAutomatically]);
    preferences->setAttachmentElementEnabled(!![_configuration _attachmentElementEnabled]);
    preferences->setAttachmentWideLayoutEnabled(!![_configuration _attachmentWideLayoutEnabled]);

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    preferences->setDataDetectorTypes(fromWKDataDetectorTypes([_configuration dataDetectorTypes]).toRaw());
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    preferences->setAllowsAirPlayForMediaPlayback(!![_configuration allowsAirPlayForMediaPlayback]);
#endif

#if ENABLE(APPLE_PAY)
    preferences->setApplePayEnabled(!![_configuration _applePayEnabled]);
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    preferences->setCSSTransformStyleSeparatedEnabled(!![_configuration _cssTransformStyleSeparatedEnabled]);
#endif

    preferences->setNeedsStorageAccessFromFileURLsQuirk(!![_configuration _needsStorageAccessFromFileURLsQuirk]);
    preferences->setMediaContentTypesRequiringHardwareSupport(String([_configuration _mediaContentTypesRequiringHardwareSupport]));
    preferences->setAllowMediaContentTypesRequiringHardwareSupportAsFallback(!![_configuration _allowMediaContentTypesRequiringHardwareSupportAsFallback]);
    if (!preferences->mediaDevicesEnabled())
        preferences->setMediaDevicesEnabled(!![_configuration _mediaCaptureEnabled]);

    preferences->setColorFilterEnabled(!![_configuration _colorFilterEnabled]);

    preferences->setUndoManagerAPIEnabled(!![_configuration _undoManagerAPIEnabled]);
    
#if ENABLE(APP_HIGHLIGHTS)
    preferences->setAppHighlightsEnabled(!![_configuration _appHighlightsEnabled]);
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    preferences->setLegacyEncryptedMediaAPIEnabled(!![_configuration _legacyEncryptedMediaAPIEnabled]);
#endif

#if PLATFORM(IOS_FAMILY)
    bool hasServiceWorkerEntitlement = (WTF::processHasEntitlement("com.apple.developer.WebKit.ServiceWorkers"_s) || WTF::processHasEntitlement("com.apple.developer.web-browser"_s)) && ![_configuration preferences]._serviceWorkerEntitlementDisabledForTesting;
    if (!hasServiceWorkerEntitlement && ![_configuration limitsNavigationsToAppBoundDomains])
        preferences->setServiceWorkersEnabled(false);
    preferences->setServiceWorkerEntitlementDisabledForTesting(!![_configuration preferences]._serviceWorkerEntitlementDisabledForTesting);
#endif

    preferences->setSampledPageTopColorMaxDifference([_configuration _sampledPageTopColorMaxDifference]);
    preferences->setSampledPageTopColorMinHeight([_configuration _sampledPageTopColorMinHeight]);

    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SiteSpecificQuirksAreEnabledByDefault))
        preferences->setNeedsSiteSpecificQuirks(false);

    // For SharedPreferencesForWebProcess
    preferences->setAllowTestOnlyIPC(!![_configuration _allowTestOnlyIPC]);
    preferences->setUsesSingleWebProcess(pool.usesSingleWebProcess());

    preferences->endBatchingUpdates();

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

    RetainPtr configuration = [coder decodeObjectOfClass:[WKWebViewConfiguration class] forKey:@"configuration"];
    [self _initializeWithConfiguration:configuration.get()];

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
    [coder encodeObject:retainPtr(self.customUserAgent).get() forKey:@"customUserAgent"];
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

#if ENABLE(SCREEN_TIME)
    [self _uninstallScreenTimeWebpageController];
#endif

#if PLATFORM(MAC)
    [self _resetSecureInputState];
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
    if (!isUIThread())
        RELEASE_LOG_FAULT(Loading, "WKWebView APIs must be called from the main thread");

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
    RetainPtr data = [string dataUsingEncoding:NSUTF8StringEncoding];

    return [self loadData:data.get() MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:baseURL];
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
    RetainPtr dictionary = [unarchiver decodeObjectOfClasses:[NSSet setWithObjects:[NSDictionary class], [NSArray class], [NSString class], [NSNumber class], [NSData class], [NSURL class], [NSURLRequest class], nil] forKey:@"NSKeyedArchiveRootObjectKey"];
    [unarchiver finishDecoding];
    RetainPtr path = [dictionary objectForKey:@"NSURLSessionResumeInfoLocalPath"];

    if (!path)
        [NSException raise:NSInvalidArgumentException format:@"Invalid resume data"];

    _page->resumeDownload(API::Data::createWithoutCopying(resumeData), path.get(), [completionHandler = makeBlockPtr(completionHandler)] (auto* download) {
        if (download)
            completionHandler(wrapper(download));
        else
            ASSERT_NOT_REACHED();
    });
}

- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    THROW_IF_SUSPENDED;
    return wrapper(_page->goToBackForwardItem(Ref { item._item })).autorelease();
}

- (NSString *)title
{
    return _page->protectedPageLoadState()->title().createNSString().autorelease();
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:_page->protectedPageLoadState()->activeURL()];
}

- (NSURL *)_resourceDirectoryURL
{
    return _page->currentResourceDirectoryURL().createNSURL().autorelease();
}

- (BOOL)isLoading
{
    return _page->protectedPageLoadState()->isLoading();
}

- (double)estimatedProgress
{
    return _page->protectedPageLoadState()->estimatedProgress();
}

- (BOOL)hasOnlySecureContent
{
    return _page->protectedPageLoadState()->hasOnlySecureContent();
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
    return _page->protectedPageLoadState()->canGoBack();
}

- (BOOL)canGoForward
{
    [self _didAccessBackForwardList];
    return _page->protectedPageLoadState()->canGoForward();
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
    auto callbackAggregator = CallbackAggregator::create([completionHandler = makeBlockPtr(completionHandler)] {
        if (completionHandler)
            completionHandler();
    });

#if ENABLE(FULLSCREEN_API)
    if (RefPtr videoPresentationManager = _page->videoPresentationManager()) {
        videoPresentationManager->forEachSession([callbackAggregator] (auto& model, auto& interface) mutable {
            model.requestCloseAllMediaPresentations(false, [callbackAggregator] { });
        });
    }

    if (RefPtr fullScreenManager = _page->fullScreenManager(); fullScreenManager && fullScreenManager->isFullScreen())
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

    std::optional<Vector<std::pair<String, WebKit::JavaScriptEvaluationResult>>> argumentsMap;
    if (asAsyncFunction)
        argumentsMap = Vector<std::pair<String, WebKit::JavaScriptEvaluationResult>> { };
    NSString *errorMessage = nil;

    for (id key in arguments) {
        RetainPtr keyString = dynamic_objc_cast<NSString>(key);
        if (!keyString) {
            errorMessage = @"Key value must be NSString";
            break;
        }

        RetainPtr<id> value = [arguments objectForKey:keyString.get()];
        auto serializedValue = WebKit::JavaScriptEvaluationResult::extract(value.get());
        if (!serializedValue) {
            errorMessage = @"Function argument values must be one of the following types, or contain only the following types: NSNumber, NSNull, NSDate, NSString, NSArray, and NSDictionary";
            break;
        }

        argumentsMap->append({ keyString.get(), WTFMove(*serializedValue) });
    }

    if (errorMessage && handler) {
        RetainPtr<NSMutableDictionary> userInfo = adoptNS([[NSMutableDictionary alloc] init]);

        [userInfo setObject:localizedDescriptionForErrorCode(WKErrorJavaScriptExceptionOccurred).get() forKey:NSLocalizedDescriptionKey];
        [userInfo setObject:errorMessage forKey:_WKJavaScriptExceptionMessageErrorKey];

        auto error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorJavaScriptExceptionOccurred userInfo:userInfo.get()]);
        RunLoop::mainSingleton().dispatch([handler, error] {
            auto rawHandler = (void (^)(id, NSError *))handler.get();
            rawHandler(nil, error.get());
        });

        return;
    }
    
    std::optional<WebCore::FrameIdentifier> frameID;
    if (frame && frame._handle && frame._handle->_frameHandle->frameID())
        frameID = frame._handle->_frameHandle->frameID();

    auto removeTransientActivation = !_dontResetTransientActivationAfterRunJavaScript && WebKit::shouldEvaluateJavaScriptWithoutTransientActivation() ? WebCore::RemoveTransientActivation::Yes : WebCore::RemoveTransientActivation::No;
    _page->runJavaScriptInFrameInScriptWorld(WebKit::RunJavaScriptParameters {
        javaScriptString,
        JSC::SourceTaintedOrigin::Untainted,
        sourceURL,
        asAsyncFunction ? WebCore::RunAsAsyncFunction::Yes : WebCore::RunAsAsyncFunction::No,
        WTFMove(argumentsMap),
        forceUserGesture ? WebCore::ForceUserGesture::Yes : WebCore::ForceUserGesture::No,
        removeTransientActivation
    }, frameID, Ref { *world->_contentWorld }, !!handler, [handler] (auto&& result) {
        if (!handler)
            return;

        auto rawHandler = (void (^)(id, NSError *))handler.get();
        if (!result)
            return rawHandler(nil, nsErrorFromExceptionDetails(result.error()).get());
        rawHandler(result->toID().get(), nil);
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
        RunLoop::mainSingleton().dispatch([handler = WTFMove(handler)] {
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
    _page->takeSnapshot(WebCore::enclosingIntRect(rectInViewCoordinates), bitmapSize, snapshotOptions, [handler, snapshotWidth, imageHeight, usesContentRect = snapshotOptions.contains(WebKit::SnapshotOption::FullContentRect)](CGImageRef cgImage) {
        if (!cgImage) {
            tracePoint(TakeSnapshotEnd, snapshotFailedTraceValue);
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }
        auto width = usesContentRect ? (CGFloat)CGImageGetWidth(cgImage) : snapshotWidth;
        auto height = usesContentRect ? (CGFloat)CGImageGetHeight(cgImage) : imageHeight;
        auto image = adoptNS([[NSImage alloc] initWithCGImage:cgImage size:NSMakeSize(width, height)]);
        tracePoint(TakeSnapshotEnd, true);
        handler(image.get(), nil);
    });
#else

    CGFloat deviceScale = _page->deviceScaleFactor();
    CGFloat imageWidth = snapshotWidth * deviceScale;
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
            dispatch_async(mainDispatchQueueSingleton(), [callSnapshotRect = WTFMove(callSnapshotRect)] {
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
    return _page->customUserAgent().createNSString().autorelease();
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
    return _page->overriddenMediaType().isNull() ? nil : _page->overriddenMediaType().createNSString().autorelease();
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
    if (RefPtr webFrameProxy = _page->mainFrame())
        return _impl->printOperationWithPrintInfo(printInfo, *webFrameProxy).autorelease();
    return nil;
}

#endif // PLATFORM(MAC)

#pragma mark - macOS/iOS internal

- (NSString *)_nameForVisualIdentificationOverlay
{
    return @"WKWebView";
}

- (void)_showWarningView:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
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

- (void)_showBrowsingWarning:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
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
        RunLoop::mainSingleton().dispatch([updateBlock = makeBlockPtr(updateBlock)] {
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
    auto additionalInsets = _impl->obscuredContentInsets();
#else
    WebCore::FloatBoxExtent additionalInsets;
#endif

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    [self _updateFixedColorExtensionViews];
#if PLATFORM(MAC)
    _impl->updateScrollPocket();
#endif
#endif

    auto maximumViewportInsetSize = WebCore::FloatSize(maximumViewportInset.left + additionalInsets.left() + maximumViewportInset.right, maximumViewportInset.top + additionalInsets.top() + maximumViewportInset.bottom);
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

    auto minimumViewportInsetSize = WebCore::FloatSize(minimumViewportInset.left + additionalInsets.left() + minimumViewportInset.right, minimumViewportInset.top + additionalInsets.top() + minimumViewportInset.bottom);
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
    auto handler = std::exchange(_windowSnapshotReadinessHandler, nil);
    if (!handler)
        return;

    handler();

    RefPtr page = _page;
    if (!page) {
        RELEASE_LOG(ViewState, "%p - Stopped holding window resize snapshot for window full screen (null page)", self);
        return;
    }

    RELEASE_LOG(ViewState, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] Stopped holding window resize snapshot for window full screen",
        self, page->identifier().toUInt64(), page->webPageIDInMainFrameProcess().toUInt64(), page->legacyMainFrameProcessID());
}

#endif // PLATFORM(MAC) && HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
- (void)_spatialBackdropSourceDidChange
{
    if (auto spatialBackdropSource = _page->spatialBackdropSource())
        _cachedSpatialBackdropSource = adoptNS([[_WKSpatialBackdropSource alloc] initWithSpatialBackdropSource:spatialBackdropSource.value()]);
    else
        _cachedSpatialBackdropSource = nil;
}
#endif

- (id<_WKImmersiveEnvironmentDelegate>)_immersiveEnvironmentDelegate
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    return _immersiveEnvironmentDelegate.getAutoreleased();
#else
    return nil;
#endif
}

- (void)_setImmersiveEnvironmentDelegate:(id<_WKImmersiveEnvironmentDelegate>)delegate
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    _immersiveEnvironmentDelegate = delegate;
#endif
}

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
- (void)_canEnterImmersiveElementFromURL:(const URL&)url completion:(CompletionHandler<void(bool)>&&)completion
{
    id<_WKImmersiveEnvironmentDelegate> immersiveEnvironmentDelegate = self._immersiveEnvironmentDelegate;
    if (!immersiveEnvironmentDelegate) {
        completion(false);
        return;
    }

    auto completionBlock = makeBlockPtr(WTFMove(completion));
    auto nsURL = url.createNSURL();

    [immersiveEnvironmentDelegate webView:self canPresentImmersiveEnvironmentFromURL:nsURL.get() completion:^(bool canPresentImmersive) {
        completionBlock(canPresentImmersive);
    }];
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

- (void)_didInsertAttachment:(API::Attachment&)attachment withSource:(NSString *)source
{
    RetainPtr uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:didInsertAttachment:withSource:)])
        [uiDelegate _webView:self didInsertAttachment:protectedWrapper(attachment).get() withSource:source];
}

- (void)_didRemoveAttachment:(API::Attachment&)attachment
{
    RetainPtr uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:didRemoveAttachment:)])
        [uiDelegate _webView:self didRemoveAttachment:protectedWrapper(attachment).get()];
}

- (void)_didInvalidateDataForAttachment:(API::Attachment&)attachment
{
    RetainPtr uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:didInvalidateDataForAttachment:)])
        [uiDelegate _webView:self didInvalidateDataForAttachment:protectedWrapper(attachment).get()];
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
    RetainPtr<id <_WKAppHighlightDelegate>> delegate = self._appHighlightDelegate;
    if (!delegate)
        return;

    if (![delegate respondsToSelector:@selector(_webView:storeAppHighlight:inNewGroup:requestOriginatedInApp:)])
        return;

    RetainPtr<NSString> text;

    if (highlight.text)
        text = highlight.text.value().createNSString();

    RetainPtr wkHighlight = adoptNS([[_WKAppHighlight alloc] initWithHighlight:Ref { highlight.highlight }->makeContiguous()->createNSData().get() text:text.get() image:nil]);

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

#if ENABLE(SCREEN_TIME)
- (STWebpageController *)_screenTimeWebpageController
{
    return _screenTimeWebpageController.get();
}

#if PLATFORM(MAC)
- (NSVisualEffectView *) _screenTimeBlurredSnapshot
#else
- (UIVisualEffectView *) _screenTimeBlurredSnapshot
#endif
{
    return _screenTimeBlurredSnapshot.get();
}
#endif

- (std::optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting
{
    return _resolutionForShareSheetImmediateCompletionForTesting;
}

- (void)createPDFWithConfiguration:(WKPDFConfiguration *)pdfConfiguration completionHandler:(void (^)(NSData *pdfDocumentData, NSError *error))completionHandler
{
    THROW_IF_SUSPENDED;
    if (!_page->mainFrame()) {
        completionHandler(nil, createNSError(WKErrorUnknown).get());
        return;
    }

    std::optional<WebCore::FloatRect> floatRect;
    if (pdfConfiguration && !CGRectIsNull(pdfConfiguration.rect))
        floatRect = WebCore::FloatRect(pdfConfiguration.rect);

    bool allowTransparentBackground = pdfConfiguration && pdfConfiguration.allowTransparentBackground;

    _page->drawToPDF(floatRect, allowTransparentBackground, [handler = makeBlockPtr(completionHandler)](RefPtr<WebCore::SharedBuffer>&& pdfData) {
        if (!pdfData || pdfData->isEmpty()) {
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }

        auto data = pdfData->createCFData();
        handler((NSData *)data.get(), nil);
    });
}

static RetainPtr<NSError> unknownError()
{
    return adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);
}

- (void)createWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getWebArchiveData([completionHandler = makeBlockPtr(completionHandler)](API::Data* data) {
        if (data)
            completionHandler(protectedWrapper(data).get(), nil);
        else
            completionHandler(nil, unknownError().get());
    });
}

static RetainPtr<NSDictionary> dictionaryRepresentationForEditorState(const WebKit::EditorState& state)
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
        @"text-color": serializationForCSS(postLayoutData.textColor).createNSString().get()
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
        RetainPtr selectionAttributesKey = NSStringFromSelector(@selector(_selectionAttributes));
        [self willChangeValueForKey:selectionAttributesKey.get()];
        _selectionAttributes = newSelectionAttributes;
        [self didChangeValueForKey:selectionAttributesKey.get()];
    }

    // FIXME: We should either rename -_webView:editorStateDidChange: to clarify that it's only intended for use when testing,
    // or remove it entirely and use -_webView:didChangeFontAttributes: instead once text alignment is supported in the set of
    // font attributes.
    RetainPtr uiDelegate = (id <WKUIDelegatePrivate>)self.UIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:editorStateDidChange:)])
        [uiDelegate _webView:self editorStateDidChange:dictionaryRepresentationForEditorState(_page->editorState()).get()];
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
    RetainPtr data = [string dataUsingEncoding:NSUTF8StringEncoding];
    auto response = adoptNS([[NSURLResponse alloc] initWithURL:retainPtr(request.URL).get() MIMEType:@"text/html" expectedContentLength:string.length textEncodingName:@"UTF-8"]);

    return [self loadSimulatedRequest:request response:response.get() responseData:data.get()];
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
    RetainPtr<NSURL> URL = request.URL;

    if (![URL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", URL.get()];

    if (![readAccessURL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", readAccessURL];

    bool isAppInitiated = true;
#if ENABLE(APP_PRIVACY_REPORT)
    isAppInitiated = request.attribution == NSURLRequestAttributionDeveloper;
#endif

    return wrapper(_page->loadFile(URL.get().absoluteString, readAccessURL.absoluteString, isAppInitiated)).autorelease();
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
    _page->setUnderPageBackgroundColorOverride(WebCore::roundAndClampToSRGBALossy(RetainPtr { underPageBackgroundColorOverride.CGColor }.get()));
}

+ (BOOL)automaticallyNotifiesObserversOfUnderPageBackgroundColor
{
    return NO;
}

- (WKFullscreenState)fullscreenState
{
#if ENABLE(FULLSCREEN_API)
    RefPtr fullscreenManager = _page->fullScreenManager();
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

- (void)_setNeedsScrollGeometryUpdates:(BOOL)needsScrollGeometryUpdates
{
    _page->setNeedsScrollGeometryUpdates(needsScrollGeometryUpdates);
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

- (CocoaWritingToolsResultOptions)allowedWritingToolsResultOptions
{
    auto& editorState = _page->editorState();
    if (editorState.isContentEditable && !editorState.isContentRichlyEditable)
        return CocoaWritingToolsResultPlainText;

    return CocoaWritingToolsResultPlainText | CocoaWritingToolsResultRichText | CocoaWritingToolsResultList | CocoaWritingToolsResultTable;
}

- (CocoaWritingToolsBehavior)writingToolsBehavior
{
    return WebKit::convertToCocoaWritingToolsBehavior(_page->writingToolsBehavior());
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

    if (session.type == WTSessionTypeProofreading)
        _intelligenceTextEffectCoordinator = adoptNS([WebKit::allocWKIntelligenceReplacementTextEffectCoordinatorInstance() initWithDelegate:(id<WKIntelligenceTextEffectCoordinatorDelegate>)self]);
    else if (session.type == WTSessionTypeComposition && session.compositionSessionType == WTCompositionSessionTypeSmartReply)
        _intelligenceTextEffectCoordinator = adoptNS([WebKit::allocWKIntelligenceSmartReplyTextEffectCoordinatorInstance() initWithDelegate:(id<WKIntelligenceTextEffectCoordinatorDelegate>)self]);
    else
        _intelligenceTextEffectCoordinator = nil;

    [_intelligenceTextEffectCoordinator startAnimationForRange:contexts.firstObject.range completion:^{ }];
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

        [_writingToolsTextSuggestions setObject:suggestion forKey:retainPtr(suggestion.uuid).get()];
    }

    NSInteger delta = [WebKit::getWKIntelligenceReplacementTextEffectCoordinatorClassSingleton() characterDeltaForReceivedSuggestions:suggestions];

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

    RetainPtr suggestion = [_writingToolsTextSuggestions objectForKey:suggestionUUID];
    if (!suggestion) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webTextSuggestion = WebKit::convertToWebTextSuggestion(suggestion.get());
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

    if (!_intelligenceTextEffectCoordinator) {
        _page->setWritingToolsActive(false);
        _page->didEndWritingToolsSession(*webSession, accepted);
        return;
    }

    // Flush and invoke all replacement operations, then dismiss the markers (and revert the text if not accepted),
    // then set the selection to the updated context range, and finally clear the state in the web process.
    //
    // It's possible that the selection has already been restored by this point if the entire animation has already
    // finished, but this is not guaranteed.

    [_intelligenceTextEffectCoordinator flushReplacementsWithCompletionHandler:makeBlockPtr([webSession, accepted, weakSelf = WeakObjCPtr<WKWebView>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        strongSelf->_page->setWritingToolsActive(false);

        strongSelf->_page->willEndWritingToolsSession(*webSession, accepted, [webSession, accepted, weakSelf] {
            // At this point, the selection will have been restored by the intelligence effects coordinator
            // assuming that the replacements have been accepted. If this is not the case, the selection should
            // be updated accordingly.
            //
            // If the user ends a session before the animation ends, and they have accepted the changes, then the
            // selection should not show up (this case is handled within the intelligence effects coordinator).

            if (accepted)
                weakSelf.get()->_page->didEndWritingToolsSession(*webSession, accepted);
            else {
                [weakSelf.get()->_intelligenceTextEffectCoordinator restoreSelectionAcceptedReplacements:accepted completionHandler:makeBlockPtr([webSession, accepted, weakSelf] {
                    weakSelf.get()->_page->didEndWritingToolsSession(*webSession, accepted);
                }).get()];
            }
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

    // FIXME: This branch can be removed once all composition types use the new effects system.
    if (!_intelligenceTextEffectCoordinator) {
        _writingToolsTextReplacementsFinished = finished;
        _partialIntelligenceTextAnimationCount += 1;

        _page->compositionSessionDidReceiveTextWithReplacementRange(*webSession, WebCore::AttributedString::fromNSAttributedString(attributedText), { range }, *webContext, finished, [] { });
        return;
    }

    auto convertedAttributedText = WebCore::AttributedString::fromNSAttributedString(attributedText);

    auto operation = makeBlockPtr([webSession, convertedAttributedText, range, webContext, finished, weakSelf = WeakObjCPtr<WKWebView>(self)](void (^completion)(void)) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completion();
            return;
        }

        strongSelf->_page->compositionSessionDidReceiveTextWithReplacementRange(*webSession, convertedAttributedText, { range }, *webContext, finished, [completion = makeBlockPtr(completion)] {
            completion();
        });
    });

    // With Smart Replies, the `range` parameter will always be `(0, 0)` instead of the actual replacement range,
    // which is in fact just the current selection and always starts at the beginning, and so this range is used instead.

    auto& editorState = _page->editorState();
    auto selectedTextCharacterCount = editorState.postLayoutData
        .transform([](auto& postLayoutData) { return postLayoutData.selectedTextLength; })
        .value_or(0);

    // The character delta is the difference between the existing text and the text after the replacement.

    auto characterDelta = attributedText.length - selectedTextCharacterCount;

    [_intelligenceTextEffectCoordinator requestReplacementWithProcessedRange:NSMakeRange(0, selectedTextCharacterCount) finished:finished characterDelta:characterDelta operation:operation.get() completion:^{ }];
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
- (UIView *)viewForIntelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator
{
    return _contentView.get();
}
#else
- (NSView *)viewForIntelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator
{
    return self;
}
#endif

- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator rectsForProofreadingSuggestionsInRange:(NSRange)range completion:(void (^)(NSArray<NSValue *> *))completion
{
    _page->proofreadingSessionSuggestionTextRectsInRootViewCoordinates(range, [completion = makeBlockPtr(completion)](auto&& rects) {
        RetainPtr nsArray = createNSArray(rects);

        completion(nsArray.get());
    });
}

- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator updateTextVisibilityForRange:(NSRange)range visible:(BOOL)visible identifier:(NSUUID *)identifier completion:(void (^)(void))completion
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
- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator textPreviewsForRange:(NSRange)range completion:(void (^)(UITargetedPreview *))completion
{
    _page->textPreviewDataForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion), weakSelf = WeakObjCPtr<WKWebView>(self)](RefPtr<WebCore::TextIndicator>&& textIndicator) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completion(nil);
            return;
        }

        if (!textIndicator) {
            completion(nil);
            return;
        }

        RetainPtr preview = [strongSelf->_contentView _createTargetedPreviewFromTextIndicator:WTFMove(textIndicator) previewContainer:strongSelf.get()];
        completion(preview.get());
    });
}
#else
- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator textPreviewsForRange:(NSRange)range completion:(void (^)(NSArray<_WKTextPreview *> *))completion
{
    // FIXME: This logic is currently duplicated in a bunch of places; it should be unified.
    _page->textPreviewDataForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion)](RefPtr<WebCore::TextIndicator>&& textIndicator) {
        if (!textIndicator) {
            completion(@[ ]);
            return;
        }

        RefPtr contentImage = textIndicator->contentImage();
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

        auto textBoundingRectInRootViewCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();
        auto textRectsInBoundingRectCoordinates = textIndicator->textRectsInBoundingRectCoordinates();
        auto contentImageScaleFactor = textIndicator->contentImageScaleFactor();

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

- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator contentPreviewForRange:(NSRange)range completion:(void (^)(_WKTextPreview *))completion
{
    // FIXME: This logic is currently duplicated in a bunch of places; it should be unified.
    _page->textPreviewDataForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion)](RefPtr<WebCore::TextIndicator>&& textIndicator) {
        if (!textIndicator) {
            completion(nil);
            return;
        }

        RefPtr contentImage = textIndicator->contentImage();
        if (!contentImage) {
            ASSERT_NOT_REACHED();
            completion(nil);
            return;
        }

        RefPtr nativeImage = contentImage->nativeImage();
        if (!nativeImage) {
            ASSERT_NOT_REACHED();
            completion(nil);
            return;
        }

        RetainPtr platformImage = nativeImage->platformImage();
        if (!platformImage) {
            ASSERT_NOT_REACHED();
            completion(nil);
            return;
        }

        auto textBoundingRectInRootViewCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();
        auto textRectsInBoundingRectCoordinates = textIndicator->textRectsInBoundingRectCoordinates();

        RetainPtr textPreview = adoptNS([[_WKTextPreview alloc] initWithSnapshotImage:platformImage.get() presentationFrame:textBoundingRectInRootViewCoordinates]);

        completion(textPreview.get());
    });
}

- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator decorateReplacementsForRange:(NSRange)range completion:(void (^)(void))completion
{
    _page->decorateTextReplacementsForActiveWritingToolsSession(range, [completion = makeBlockPtr(completion)] {
        completion();
    });
}

- (void)intelligenceTextEffectCoordinator:(id<WKIntelligenceTextEffectCoordinating>)coordinator setSelectionForRange:(NSRange)range completion:(void (^)(void))completion
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

    RetainPtr textViewDelegate = (NSObject<WTTextViewDelegate> *)[_activeWritingToolsSession textViewDelegate];

    if (![textViewDelegate respondsToSelector:@selector(proofreadingSessionWithUUID:showDetailsForSuggestionWithUUID:relativeToRect:inView:)])
        return;

#if PLATFORM(MAC)
    RetainPtr view = self;
#else
    RetainPtr view = _contentView;
#endif

    [textViewDelegate proofreadingSessionWithUUID:retainPtr([_activeWritingToolsSession uuid]).get() showDetailsForSuggestionWithUUID:replacementUUID relativeToRect:rect inView:view.get()];
}

- (void)_proofreadingSessionUpdateState:(WebCore::WritingTools::TextSuggestion::State)state forSuggestionWithUUID:(NSUUID *)replacementUUID
{
    if (!_activeWritingToolsSession)
        return;

    RetainPtr textViewDelegate = (NSObject<WTTextViewDelegate> *)[_activeWritingToolsSession textViewDelegate];

    if (![textViewDelegate respondsToSelector:@selector(proofreadingSessionWithUUID:updateState:forSuggestionWithUUID:)])
        return;

    [textViewDelegate proofreadingSessionWithUUID:retainPtr([_activeWritingToolsSession uuid]).get() updateState:WebKit::convertToPlatformTextSuggestionState(state) forSuggestionWithUUID:replacementUUID];
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
    if (_gamepadsRecentlyAccessed == gamepadsRecentlyAccessed)
        return;

    _gamepadsRecentlyAccessed = gamepadsRecentlyAccessed;

#if PLATFORM(VISION)
    if (self._gamepadAccessRequiresExplicitConsent) {
        id<WKUIDelegatePrivate> uiDelegate = (id<WKUIDelegatePrivate>)self.UIDelegate;
        if ([uiDelegate respondsToSelector:@selector(_webView:setRecentlyAccessedGamepads:)])
            [uiDelegate _webView:self setRecentlyAccessedGamepads:gamepadsRecentlyAccessed];
        return;
    }

    [self _tryUpdatingGamepadsAccessStateForImplicitConsentCase];
#endif
}

#if PLATFORM(VISION)
- (BOOL)_gamepadsConnected
{
    return _page->gamepadsConnected();
}

- (void)_gamepadsConnectedStateChanged
{
    if (self._gamepadAccessRequiresExplicitConsent) {
        id<WKUIDelegatePrivate> uiDelegate = (id<WKUIDelegatePrivate>)self.UIDelegate;
        if ([uiDelegate respondsToSelector:@selector(_webView:gamepadsConnectedStateDidChange:)])
            [uiDelegate _webView:self gamepadsConnectedStateDidChange:_page->gamepadsConnected()];
        return;
    }

    [self _tryUpdatingGamepadsAccessStateForImplicitConsentCase];
}

- (void)_setAllowGamepadsAccess
{
    _page->allowGamepadAccess();
}

- (BOOL)_gamepadAccessRequiresExplicitConsent
{
    return [_configuration _gamepadAccessRequiresExplicitConsent];
}

- (BOOL)_supportsGameControllerEventInteractionAPI
{
    static bool supportsGameControllerEventInteractionAPI = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SupportGameControllerEventInteractionAPI);
    return supportsGameControllerEventInteractionAPI;
}

- (void)_tryUpdatingGamepadsAccessStateForImplicitConsentCase
{
    // We only automatically switch to receiving game controller events over
    // the Game Controller framework on user interactions on game controller
    // if the app is linked before visionOS 2.0. On visionOS 2.0 and later,
    // apps are responsible for adding the GCEventInteraction themselves.
    if (self._supportsGameControllerEventInteractionAPI)
        return;

    [self _setAllowGamepadsInput:_gamepadsRecentlyAccessed && self._gamepadsConnected];
}

- (void)_setAllowGamepadsInput:(BOOL)allowGamepadsInput
{
#if __has_include(<GameController/GCEventInteraction.h>)
    if (allowGamepadsInput == !!_gamepadsRecentlyAccessedState)
        return;

    if (allowGamepadsInput) {
        _gamepadsRecentlyAccessedState = adoptNS([[WebCore::getGCEventInteractionClassSingleton() alloc] init]);
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

WebCore::CocoaColor *sampledFixedPositionContentColor(const WebCore::FixedContainerEdges& edges, WebCore::BoxSide side)
{
    if (!edges.hasFixedEdge(side))
        return nil;

    return cocoaColorOrNil(edges.predominantColor(side)).autorelease();
}

- (WebCore::CocoaColor *)_sampledTopFixedPositionContentColor
{
    return sampledFixedPositionContentColor(_fixedContainerEdges, WebCore::BoxSide::Top);
}

- (void)_updateScrollGeometryWithContentOffset:(CGPoint)contentOffset contentSize:(CGSize)contentSize
{
#if ENABLE(SWIFTUI)
    CGSize containerSize = self.frame.size;
    auto contentInsets = _page->obscuredContentInsets();
#if PLATFORM(IOS_FAMILY)
    UIEdgeInsets cocoaInsets = UIEdgeInsetsMake(contentInsets.top(), contentInsets.left(), contentInsets.bottom(), contentInsets.right());
#else
    NSEdgeInsets cocoaInsets = NSEdgeInsetsMake(contentInsets.top(), contentInsets.left(), contentInsets.bottom(), contentInsets.right());
#endif

    RetainPtr oldScrollGeometry = _currentScrollGeometry;
    RetainPtr newScrollGeometry = adoptNS([[WKScrollGeometry alloc] initWithContainerSize:containerSize contentInsets:cocoaInsets contentOffset:contentOffset contentSize:contentSize]);

    if (oldScrollGeometry && [oldScrollGeometry isEqual:newScrollGeometry.get()])
        return;

    id<WKUIDelegateInternal> uiDelegate = (id<WKUIDelegateInternal>)self.UIDelegate;
    if (![uiDelegate respondsToSelector:@selector(_webView:geometryDidChange:)])
        return;

    _currentScrollGeometry = newScrollGeometry;
    [uiDelegate _webView:self geometryDidChange:newScrollGeometry.get()];
#endif // ENABLE(SWIFTUI)
}

- (void)_updateFixedContainerEdges:(const WebCore::FixedContainerEdges&)edges
{
    if (_fixedContainerEdges == edges)
        return;

    RetainPtr oldTopColor = [self _sampledTopFixedPositionContentColor];
    RetainPtr newTopColor = sampledFixedPositionContentColor(edges, WebCore::BoxSide::Top);
    bool isTopColorChanging = oldTopColor != newTopColor || ![oldTopColor isEqual:newTopColor.get()];
    bool isTopFixedEdgeChanging = isTopColorChanging || _fixedContainerEdges.hasFixedEdge(WebCore::BoxSide::Top) != edges.hasFixedEdge(WebCore::BoxSide::Top);

    if (isTopColorChanging)
        [self willChangeValueForKey:RetainPtr { NSStringFromSelector(@selector(_sampledTopFixedPositionContentColor)) }.get()];

    _fixedContainerEdges = edges;

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    [self _updateFixedColorExtensionViews];
    [self _updateHiddenScrollPocketEdges];
    [self _updateTopScrollPocketCaptureColor];
#endif

#if PLATFORM(MAC) && ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    if (isTopFixedEdgeChanging)
        _impl->updateScrollPocket();
#else
    UNUSED_VARIABLE(isTopFixedEdgeChanging);
#endif

    if (isTopColorChanging)
        [self didChangeValueForKey:RetainPtr { NSStringFromSelector(@selector(_sampledTopFixedPositionContentColor)) }.get()];
}

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)

- (void)_createPDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier withFrame:(CGRect)rect pageCount:(size_t)pageCount
{
    [self _removePDFPageNumberIndicator:identifier];
    RetainPtr indicator = adoptNS([[WKPDFPageNumberIndicator alloc] initWithFrame:rect view:self pageCount:pageCount]);
    [self addSubview:indicator.get()];
    _pdfPageNumberIndicator = std::make_pair(identifier, WTFMove(indicator));
}

- (void)_removePDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier
{
    if (_pdfPageNumberIndicator.first == identifier) {
        RetainPtr indicator = std::exchange(_pdfPageNumberIndicator, std::make_pair(Markable<WebKit::PDFPluginIdentifier> { }, nullptr)).second;
        [indicator removeFromSuperview];
    }
}

- (void)_updatePDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier withFrame:(CGRect)rect
{
    if (_pdfPageNumberIndicator.first == identifier)
        [_pdfPageNumberIndicator.second updatePosition:rect];
}

- (void)_updatePDFPageNumberIndicator:(WebKit::PDFPluginIdentifier)identifier currentPage:(size_t)pageIndex
{
    if (_pdfPageNumberIndicator.first == identifier)
        [_pdfPageNumberIndicator.second setCurrentPageNumber:pageIndex];
}

- (void)_updatePDFPageNumberIndicatorIfNeeded
{
    if (_pdfPageNumberIndicator.first)
        [_pdfPageNumberIndicator.second updatePosition:self.bounds];
}

- (void)_removeAnyPDFPageNumberIndicator
{
    if (auto pluginIdentifier = _pdfPageNumberIndicator.first)
        [self _removePDFPageNumberIndicator:*pluginIdentifier];
}

#endif // ENABLE(PDF_PAGE_NUMBER_INDICATOR)

- (RetainPtr<WKWebView>)_horizontallyAttachedInspectorWebView
{
    RefPtr inspector = _page->inspector();
    if (!inspector)
        return nil;

    if (!inspector->isAttached())
        return nil;

    switch (inspector->attachmentSide()) {
    case WebKit::AttachmentSide::Bottom:
        return nil;
    case WebKit::AttachmentSide::Right:
    case WebKit::AttachmentSide::Left:
        break;
    }

    RefPtr inspectorPage = inspector->inspectorPage();
    if (!inspectorPage)
        return nil;

    return inspectorPage->cocoaView();
}

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

- (void)_updateTopScrollPocketCaptureColor
{
#if PLATFORM(MAC)
    _impl->updateTopScrollPocketCaptureColor();
#else
    if (!_needsTopScrollPocketDueToVisibleContentInset && ![_scrollView _usesHardTopScrollEdgeEffect]) {
        // When using a soft pocket (iPhone), overriding the top scroll pocket capture color is only
        // necessary when:
        //   1. The top content inset area is visible.
        //   2. There's an element with a top fixed-position color.
        // If either condition is false, the scroll pocket is either not visible in the first place,
        // or it should match the scroll view background color anyways.
        // When using a hard pocket (iPad), the top scroll pocket capture color must be set to ensure
        // that glass elements overlaying the pocket adapt correctly.
        [_scrollView _setInternalTopPocketColor:nil];
        return;
    }

    if (RetainPtr color = [self _sampledTopFixedPositionContentColor] ?: [self underPageBackgroundColor])
        [_scrollView _setInternalTopPocketColor:color.get()];
#endif
}

- (WebCore::FloatBoxExtent)_obscuredInsetsForFixedColorExtension
{
#if PLATFORM(MAC)
    return _impl->obscuredContentInsets();
#else
    auto obscuredInsets = [self _obscuredInsets];
    auto additionalTopInset = [&] -> CGFloat {
        if (![_scrollView _wk_isScrolledBeyondTopExtent])
            return 0;

        auto topFixedColor = _fixedContainerEdges.predominantColor(WebCore::BoxSide::Top);
        if (!topFixedColor.isVisible())
            return 0;

        if (!WebCore::PageColorSampler::colorsAreSimilar(_page->sampledPageTopColor(), topFixedColor))
            return 0;

        if (WebCore::PageColorSampler::colorsAreSimilar(_page->underPageBackgroundColor(), topFixedColor))
            return 0;

        return std::max<CGFloat>(-obscuredInsets.top - [_scrollView contentOffset].y, 0);
    }();

    return WebCore::FloatBoxExtent {
        static_cast<float>(obscuredInsets.top + additionalTopInset),
        static_cast<float>(obscuredInsets.right),
        static_cast<float>(obscuredInsets.bottom),
        static_cast<float>(obscuredInsets.left)
    };
#endif
}

- (CocoaView *)_containerForFixedColorExtension
{
#if PLATFORM(MAC)
    return self;
#else
    return _scrollView.get();
#endif
}

- (void)_updateFixedColorExtensionViews
{
    if (!_page || !_page->protectedPreferences()->contentInsetBackgroundFillEnabled())
        return;

    RetainPtr parentView = [self _containerForFixedColorExtension];
    auto addColorExtensionView = [&](CocoaView *extensionView) {
#if PLATFORM(MAC)
        if (RetainPtr scrollPocket = _impl->topScrollPocket())
            [parentView addSubview:extensionView positioned:NSWindowBelow relativeTo:scrollPocket.get()];
        else
            [parentView addSubview:extensionView];
#else
        [parentView insertSubview:extensionView aboveSubview:_contentView.get()];
#endif
    };

    auto insets = [self _obscuredInsetsForFixedColorExtension];
    auto updateExtensionView = [&](WebCore::BoxSide side) {
        RetainPtr extensionView = _fixedColorExtensionViews.at(side);
        if (insets.at(side) <= 0 || !_fixedContainerEdges.hasFixedEdge(side)) {
            [extensionView fadeOut];
            return;
        }

        RetainPtr edgeColor = cocoaColorOrNil(_fixedContainerEdges.predominantColor(side)) ?: self.underPageBackgroundColor;
        if (side == WebCore::BoxSide::Top) {
#if PLATFORM(MAC)
            edgeColor = [self _adjustedColorForTopContentInsetColorFromUIDelegate:edgeColor.get()];
#endif
            if (_shouldSuppressTopColorExtensionView) {
                [extensionView fadeOut];
                return;
            }
        }

        if (!extensionView) {
            extensionView = adoptNS([[WKColorExtensionView alloc] initWithFrame:CGRectZero delegate:self]);
#if PLATFORM(MAC)
            [extensionView setWantsLayer:YES];
#endif
            [extensionView layer].name = adoptNS([[NSString alloc] initWithFormat:@"Fixed color extension fill (%s)", [side] {
                switch (side) {
                case WebCore::BoxSide::Top:
                    return "Top";
                case WebCore::BoxSide::Right:
                    return "Right";
                case WebCore::BoxSide::Bottom:
                    return "Bottom";
                case WebCore::BoxSide::Left:
                    return "Left";
                default:
                    ASSERT_NOT_REACHED();
                    return "";
                }
            }()]).get();
            addColorExtensionView(extensionView.get());
            _fixedColorExtensionViews.setAt(side, extensionView);
        }

        [extensionView updateColor:edgeColor.get()];
        return;
    };

    for (auto side : WebCore::allBoxSides)
        updateExtensionView(side);

    [self _updateFixedColorExtensionViewFrames];
}

- (void)_updateFixedColorExtensionViewFrames
{
    if (!_page || !_page->protectedPreferences()->contentInsetBackgroundFillEnabled())
        return;

    RetainPtr parentView = [self _containerForFixedColorExtension];
    auto insets = [self _obscuredInsetsForFixedColorExtension];
    WebCore::FloatRect bounds = self.bounds;
#if PLATFORM(IOS_FAMILY)
    auto contentOffset = [_scrollView contentOffset];
    auto contentWidth = [_scrollView contentSize].width;
    if (_perProcessState.liveResizeParameters)
        contentWidth *= self.bounds.size.width / _perProcessState.liveResizeParameters->viewWidth;
#endif

    if (RetainPtr view = _fixedColorExtensionViews.top(); view && ![view isHidden]) {
#if PLATFORM(IOS_FAMILY)
        auto targetRect = CGRectMake(-contentOffset.x, 0, contentWidth, insets.top());
#else
        auto targetRect = NSMakeRect(insets.left(), 0, bounds.width() - insets.left() - insets.right(), insets.top());
#endif
        [view setFrame:[parentView convertRect:targetRect fromView:self]];
    }

    if (RetainPtr view = _fixedColorExtensionViews.left(); view && ![view isHidden])
        [view setFrame:[parentView convertRect:CGRectMake(0, 0, insets.left(), bounds.height()) fromView:self]];

    if (RetainPtr view = _fixedColorExtensionViews.right(); view && ![view isHidden])
        [view setFrame:[parentView convertRect:CGRectMake(bounds.width() - insets.right(), 0, insets.right(), bounds.height()) fromView:self]];

    if (RetainPtr view = _fixedColorExtensionViews.bottom(); view && ![view isHidden]) {
#if PLATFORM(IOS_FAMILY)
        auto targetRect = CGRectMake(-contentOffset.x, bounds.height() - insets.bottom(), contentWidth, insets.bottom());
#else
        auto targetRect = NSMakeRect(insets.left(), bounds.height() - insets.bottom(), bounds.width() - insets.left() - insets.right(), insets.bottom());
#endif
        [view setFrame:[parentView convertRect:targetRect fromView:self]];
    }
}

- (void)_updatePrefersSolidColorHardPocket
{
#if PLATFORM(MAC)
    _impl->updatePrefersSolidColorHardPocket();
#else
    BOOL useSolidColor = [_scrollView _usesHardTopScrollEdgeEffect] && [self _hasVisibleColorExtensionView:WebCore::BoxSide::Top];
    [_scrollView _setPrefersSolidColorHardPocket:useSolidColor forEdge:UIRectEdgeTop];
#endif
}

- (void)_updateHiddenScrollPocketEdges
{
#if PLATFORM(IOS_FAMILY)
    RetainPtr scrollView = _scrollView;
    [scrollView _wk_topEdgeEffect].internallyHidden = [self _shouldHideTopScrollPocket];
    [scrollView _wk_rightEdgeEffect].internallyHidden = [self _hasVisibleColorExtensionView:WebCore::BoxSide::Right];
    [scrollView _wk_leftEdgeEffect].internallyHidden = [self _hasVisibleColorExtensionView:WebCore::BoxSide::Left];
    [scrollView _wk_bottomEdgeEffect].internallyHidden = [self _hasVisibleColorExtensionView:WebCore::BoxSide::Bottom];
#else
    _impl->updateScrollPocket();
#endif
}

- (void)_doAfterAdjustingColorForTopContentInsetFromUIDelegate:(Function<void()>&&)callback
{
#if PLATFORM(MAC)
    if (_isGettingAdjustedColorForTopContentInsetColorFromDelegate) {
        RunLoop::mainSingleton().dispatch(WTFMove(callback));
        return;
    }
#endif

    callback();
}

#if PLATFORM(MAC)

- (NSColor *)_adjustedColorForTopContentInsetColorFromUIDelegate:(NSColor *)color
{
    if (_overrideTopScrollEdgeEffectColor)
        return _overrideTopScrollEdgeEffectColor.get();

    RetainPtr delegate = static_cast<id<WKUIDelegatePrivate>>([self UIDelegate]);
    if (![delegate respondsToSelector:@selector(_webView:adjustedColorForTopContentInsetColor:)])
        return color;

    SetForScope delegateCallScope { _isGettingAdjustedColorForTopContentInsetColorFromDelegate, YES };

    RetainPtr adjustedColor = [delegate _webView:self adjustedColorForTopContentInsetColor:color];
    return adjustedColor.get() ?: color;
}

- (RetainPtr<NSScrollPocket>)_copyTopScrollPocket
{
    RetainPtr clonedScrollPocket = adoptNS([NSScrollPocket new]);
    RetainPtr currentScrollPocket = _impl->topScrollPocket();
    [clonedScrollPocket setEdge:[currentScrollPocket edge]];
    [clonedScrollPocket setStyle:[currentScrollPocket style]];
    [clonedScrollPocket setPrefersSolidColorHardPocket:[currentScrollPocket prefersSolidColorHardPocket]];
    [clonedScrollPocket setCaptureColor:[currentScrollPocket captureColor]];
    return clonedScrollPocket;
}

- (void)_addReasonToPreferSolidColorHardPocket:(WebKit::PreferSolidColorHardPocketReason)reason
{
    if (_preferSolidColorHardPocketReasons.contains(reason))
        return;

    _preferSolidColorHardPocketReasons.add(WebKit::PreferSolidColorHardPocketReason::RequestedByClient);

    if (_preferSolidColorHardPocketReasons.hasExactlyOneBitSet())
        _impl->updatePrefersSolidColorHardPocket();
}

- (void)_removeReasonToPreferSolidColorHardPocket:(WebKit::PreferSolidColorHardPocketReason)reason
{
    if (!_preferSolidColorHardPocketReasons.contains(reason))
        return;

    _preferSolidColorHardPocketReasons.remove(WebKit::PreferSolidColorHardPocketReason::RequestedByClient);

    if (!_preferSolidColorHardPocketReasons)
        _impl->updatePrefersSolidColorHardPocket();
}

#endif // PLATFORM(MAC)

- (BOOL)_hasVisibleColorExtensionView:(WebCore::BoxSide)side
{
    RetainPtr view = _fixedColorExtensionViews.at(side);
    return view && ![view isHiddenOrFadingOut];
}

static ASCIILiteral descriptionForReason(WebKit::HideScrollPocketReason reason)
{
    switch (reason) {
    case WebKit::HideScrollPocketReason::FullScreen:
        return "FullScreen"_s;
    case WebKit::HideScrollPocketReason::ScrolledToTop:
        return "ScrolledToTop"_s;
    case WebKit::HideScrollPocketReason::SiteSpecificQuirk:
        return "SiteSpecificQuirk"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

- (void)_addReasonToHideTopScrollPocket:(WebKit::HideScrollPocketReason)reason
{
    if (_reasonsToHideTopScrollPocket.contains(reason))
        return;

    WKWEBVIEW_RELEASE_LOG("%p Hide top scroll pocket (%s)", self, descriptionForReason(reason).characters());
    _reasonsToHideTopScrollPocket.add(reason);

    [self _updateHiddenScrollPocketEdges];
}

- (void)_removeReasonToHideTopScrollPocket:(WebKit::HideScrollPocketReason)reason
{
    if (!_reasonsToHideTopScrollPocket.contains(reason))
        return;

    WKWEBVIEW_RELEASE_LOG("%p Unhide top scroll pocket (%s)", self, descriptionForReason(reason).characters());
    _reasonsToHideTopScrollPocket.remove(reason);

    [self _updateHiddenScrollPocketEdges];
}

#pragma mark - WKColorExtensionViewDelegate

- (void)colorExtensionViewWillDisappear:(WKColorExtensionView *)view
{
    if (view == _fixedColorExtensionViews.at(WebCore::BoxSide::Top))
        [self _updatePrefersSolidColorHardPocket];

#if PLATFORM(IOS_FAMILY)
    [self _updateHiddenScrollPocketEdges];
#endif
}

- (void)colorExtensionViewDidAppear:(WKColorExtensionView *)view
{
    if (view == _fixedColorExtensionViews.at(WebCore::BoxSide::Top))
        [self _updatePrefersSolidColorHardPocket];

#if PLATFORM(IOS_FAMILY)
    [self _updateHiddenScrollPocketEdges];
#endif
}

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

- (CocoaEdgeInsets)obscuredContentInsets
{
#if PLATFORM(IOS_FAMILY)
    return self._obscuredInsets;
#else
    return self._obscuredContentInsets;
#endif
}

- (void)setObscuredContentInsets:(CocoaEdgeInsets)insets
{
    if (insets.top < 0 || insets.left < 0 || insets.right < 0 || insets.bottom < 0) {
#if PLATFORM(IOS_FAMILY)
        [NSException raise:NSInvalidArgumentException format:@"-obscuredContentInsets cannot be negative: %@", NSStringFromUIEdgeInsets(insets)];
#else
        [NSException raise:NSInvalidArgumentException format:@"-obscuredContentInsets cannot be negative: { top=%f, left=%f, bottom=%f, right=%f }"
            , insets.top, insets.left, insets.bottom, insets.right];
#endif
    }

#if PLATFORM(IOS_FAMILY)
    if (UIEdgeInsetsEqualToEdgeInsets(_obscuredInsets, insets))
        return;

    [self _setObscuredInsetsInternal:insets];
    _automaticallyAdjustsViewLayoutSizesWithObscuredInset = !UIEdgeInsetsEqualToEdgeInsets(insets, UIEdgeInsetsZero);

    [self _frameOrBoundsMayHaveChanged];
#else
    if (NSEdgeInsetsEqual(self._obscuredContentInsets, insets))
        return;

    self._automaticallyAdjustsContentInsets = NO;
    [self _setObscuredContentInsets:insets immediate:NO];
#endif
}

namespace WebKit {
enum class WebViewDataType : uint32_t {
    SessionStorage
};
}

namespace WTF {
template<> struct EnumTraitsForPersistence<WebKit::WebViewDataType> {
    using values = EnumValues<
        WebKit::WebViewDataType,
        WebKit::WebViewDataType::SessionStorage
    >;
};
}

struct WKWebViewData {
    std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>> sessionStorage;
};

- (void)fetchDataOfTypes:(WKWebViewDataType)dataTypes completionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    Vector<WebKit::WebViewDataType> dataTypesToEncode;
    if (dataTypes & WKWebViewDataTypeSessionStorage)
        dataTypesToEncode.append(WebKit::WebViewDataType::SessionStorage);

    auto data = Box<WKWebViewData>::create();

    Ref callbackAggregator = CallbackAggregator::create([completionHandler = makeBlockPtr(completionHandler), dataTypesToEncode = WTFMove(dataTypesToEncode), data] {
        WTF::Persistence::Encoder encoder;
        constexpr unsigned currentWKWebViewDataSerializationVersion = 1;
        encoder << currentWKWebViewDataSerializationVersion;
        encoder << dataTypesToEncode;

        for (auto& dataTypeToEncode : dataTypesToEncode) {
            switch (dataTypeToEncode) {
            case WebKit::WebViewDataType::SessionStorage:
                if (!data->sessionStorage) {
                    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Unknown error occurred while fetching data.", };
                    completionHandler(nullptr, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());
                    return;
                }

                encoder << data->sessionStorage.value();
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }

        completionHandler(toNSData(encoder.span()).get(), nullptr);
    });

    if (dataTypes & WKWebViewDataTypeSessionStorage) {
        RefPtr page = [self _protectedPage];
        page->fetchSessionStorage([callbackAggregator, protectedPage = page, data](auto&& sessionStorage) {
            data->sessionStorage = WTFMove(sessionStorage);
        });
    }
}

- (void)restoreData:(NSData *)data completionHandler:(void(^)(NSError *))completionHandler
{
    WTF::Persistence::Decoder decoder(span(data));

    std::optional<unsigned> currentWKWebViewDataSerializationVersion;
    decoder >> currentWKWebViewDataSerializationVersion;

    if (!currentWKWebViewDataSerializationVersion) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Version number is missing.", };
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());

        return;
    }

    std::optional<Vector<WebKit::WebViewDataType>> encodedDataTypes;
    decoder >> encodedDataTypes;

    if (!encodedDataTypes) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"List of encoded data types is missing.", };
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());

        return;
    }

    auto error = Box<RetainPtr<NSError>>::create(nil);

    Ref callbackAggregator = CallbackAggregator::create([completionHandler = makeBlockPtr(completionHandler), error] {
        if (*error)
            completionHandler(error->get());
        else
            completionHandler(nullptr);
    });

    for (auto& encodedDataType : *encodedDataTypes) {
        switch (encodedDataType) {
        case WebKit::WebViewDataType::SessionStorage: {
            std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>> sessionStorage;
            decoder >> sessionStorage;

            if (!sessionStorage) {
                NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Encoded session storage data is missing.", };
                *error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get();

                return;
            }

            if (!sessionStorage->isEmpty()) {
                RefPtr page = [self _protectedPage];
                page->restoreSessionStorage(WTFMove(*sessionStorage), [callbackAggregator, error](bool restoreSucceeded) {
                    if (!restoreSucceeded) {
                        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Unknown error occurred while restoring data.", };
                        *error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get();
                    }
                });
            }

            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

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

- (IBAction)_alignCenter:(id)sender
{
    [self alignCenter:sender];
}

- (IBAction)_alignJustified:(id)sender
{
    [self alignJustified:sender];
}

- (IBAction)_alignLeft:(id)sender
{
    [self alignLeft:sender];
}

- (IBAction)_alignRight:(id)sender
{
    [self alignRight:sender];
}

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
    if (RefPtr inspector = _page->inspector())
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
    if (RefPtr frame = _page->mainFrame())
        return wrapper(API::FrameHandle::create(frame->frameID())).autorelease();
    return nil;
}

- (BOOL)_negotiatedLegacyTLS
{
    return _page->protectedPageLoadState()->hasNegotiatedLegacyTLS();
}

- (BOOL)_wasPrivateRelayed
{
    return _page->pageLoadState().wasPrivateRelayed();
}

- (NSString *)_proxyName
{
    return _page->pageLoadState().proxyName().createNSString().autorelease();
}

- (BOOL)_isContentFromNetwork
{
    return _page->pageLoadState().source() == WebCore::ResourceResponseBase::Source::Network;
}

- (void)_frames:(void (^)(_WKFrameTreeNode *))completionHandler
{
    _page->getAllFrames([completionHandler = makeBlockPtr(completionHandler), page = Ref { *_page.get() }] (std::optional<WebKit::FrameTreeNodeData>&& data) {
        if (!data)
            return completionHandler(nil);
        completionHandler(wrapper(API::FrameTreeNode::create(WTFMove(*data), page.get())).get());
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

- (void)_frameInfoFromHandle:(_WKFrameHandle *)handle completionHandler:(void (^)(WKFrameInfo *))completionHandler
{
    auto frameID = handle->_frameHandle->frameID();
    if (!frameID)
        return completionHandler(nil);
    RefPtr frame = WebKit::WebFrameProxy::webFrame(*frameID);
    if (!frame)
        return completionHandler(nil);
    frame->getFrameInfo([completionHandler = makeBlockPtr(completionHandler)] (auto&& data) mutable {
        if (!data)
            return completionHandler(nil);
        completionHandler(wrapper(API::FrameInfo::create(WTFMove(*data))).get());
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
        [result setObject:info->documentURL.createNSURL().get() forKey:_WKTextManipulationTokenUserInfoDocumentURLKey];
    if (!info->tagName.isNull())
        [result setObject:info->tagName.createNSString().get() forKey:_WKTextManipulationTokenUserInfoTagNameKey];
    if (!info->roleAttribute.isNull())
        [result setObject:info->roleAttribute.createNSString().get() forKey:_WKTextManipulationTokenUserInfoRoleAttributeKey];
    [result setObject:RetainPtr { @(info->isVisible) }.get() forKey:_WKTextManipulationTokenUserInfoVisibilityKey];

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

    RefPtr frame = _page->mainFrame();
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

        RetainPtr retainedSelf = weakSelf.get();
        RetainPtr delegate = [retainedSelf _textManipulationDelegate];
        if (!delegate)
            return;

        auto createWKItem = [] (const WebCore::TextManipulationItem& item) {
            auto tokens = createNSArray(item.tokens, [] (auto& token) {
                auto wkToken = adoptNS([[_WKTextManipulationToken alloc] init]);
                [wkToken setIdentifier:String::number(token.identifier.toUInt64()).createNSString().get()];
                [wkToken setContent:token.content.createNSString().get()];
                [wkToken setExcluded:token.isExcluded];
                [wkToken setUserInfo:createUserInfo(token.info).get()];
                return wkToken;
            });
            auto identifier = makeString(item.frameID ? item.frameID->toUInt64() : 0, '-', item.identifier ? item.identifier->toUInt64() : 0);
            return adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:identifier.createNSString().get() tokens:tokens.get() isSubframe:item.isSubframe isCrossSiteSubframe:item.isCrossSiteSubframe]);
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
    std::optional<uint64_t> frameID;
    std::optional<uint64_t> itemID;
    for (auto token : StringView(identifierString).split('-')) {
        switch (index) {
        case 0:
            frameID = parseInteger<uint64_t>(token);
            break;
        case 1:
            itemID = parseInteger<uint64_t>(token);
            break;
        default:
            return std::nullopt;
        }
        ++index;
    }
    if (!frameID || !*frameID || !itemID || !*itemID)
        return std::nullopt;

    if (!WebCore::FrameIdentifier::isValidIdentifier(*frameID))
        return std::nullopt;

    if (!ObjectIdentifier<WebCore::TextManipulationItemIdentifierType>::isValidIdentifier(*itemID))
        return std::nullopt;

    return ItemIdentifiers { WebCore::FrameIdentifier(*frameID),
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

    auto identifiers = coreTextManipulationItemIdentifierFromString(retainPtr(item.identifier).get());
    if (!identifiers) {
        completionHandler(false);
        return;
    }

    Vector<WebCore::TextManipulationToken> tokens;
    for (_WKTextManipulationToken *wkToken in item.tokens)
        tokens.append(WebCore::TextManipulationToken { coreTextManipulationTokenIdentifierFromString(wkToken.identifier), wkToken.content, std::nullopt });

    Vector<WebCore::TextManipulationItem> coreItems({ WebCore::TextManipulationItem { identifiers->frameID, false, false, identifiers->itemID, WTFMove(tokens) } });
    _page->completeTextManipulation(coreItems, [capturedCompletionBlock = makeBlockPtr(completionHandler)] (auto failures) {
        capturedCompletionBlock(failures.isEmpty());
    });
}

static RetainPtr<NSMutableArray> makeFailureSetForAllTextManipulationItems(NSArray<_WKTextManipulationItem *> *items)
{
    RetainPtr<NSMutableArray> wkFailures = adoptNS([[NSMutableArray alloc] initWithCapacity:items.count]);
    for (_WKTextManipulationItem *item in items)
        [wkFailures addObject:adoptNS([[NSError alloc] initWithDomain:_WKTextManipulationItemErrorDomain code:_WKTextManipulationItemErrorNotAvailable userInfo:@{_WKTextManipulationItemErrorItemKey: item}]).get()];
    return wkFailures;
};

static RetainPtr<NSArray> wkTextManipulationErrors(NSArray<_WKTextManipulationItem *> *items, const Vector<WebCore::TextManipulationController::ManipulationFailure>& failures)
{
    if (failures.isEmpty())
        return nil;

    return createNSArray(failures, [&] (auto& coreFailure) -> RetainPtr<NSError> {
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
        return adoptNS([[NSError alloc] initWithDomain:_WKTextManipulationItemErrorDomain code:errorCode userInfo:@{_WKTextManipulationItemErrorItemKey: item}]);
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
            RetainPtr<_WKTextManipulationToken> wkToken = wkItem.tokens[i];
            return WebCore::TextManipulationToken { coreTextManipulationTokenIdentifierFromString(wkToken.get().identifier), wkToken.get().content, std::nullopt };
        });
        auto identifiers = coreTextManipulationItemIdentifierFromString(retainPtr(wkItem.identifier).get());
        std::optional<WebCore::FrameIdentifier> frameID;
        std::optional<WebCore::TextManipulationItemIdentifier> itemID;
        if (identifiers) {
            frameID = identifiers->frameID;
            itemID = identifiers->itemID;
        }
        coreItems.append(WebCore::TextManipulationItem { frameID, false, false, itemID, WTFMove(coreTokens) });
    }

    RetainPtr<NSArray<_WKTextManipulationItem *>> retainedItems = items;
    _page->completeTextManipulation(coreItems, [capturedItems = retainedItems, capturedCompletionBlock = makeBlockPtr(completionHandler)](auto failures) {
        capturedCompletionBlock(wkTextManipulationErrors(capturedItems.get(), failures).get());
    });
}

- (void)_startImageAnalysis:(NSString *)sourceLanguageIdentifier target:(NSString *)targetLanguageIdentifier
{
#if ENABLE(IMAGE_ANALYSIS)
    THROW_IF_SUSPENDED;

    if (!_page || !_page->protectedPreferences()->visualTranslationEnabled() || !WebKit::languageIdentifierSupportsLiveText(sourceLanguageIdentifier))
        return;

    _page->startVisualTranslation(sourceLanguageIdentifier, targetLanguageIdentifier);
#endif
}

- (void)_dataTaskWithRequest:(NSURLRequest *)request runAtForegroundPriority:(BOOL)runAtForegroundPriority completionHandler:(void(^)(_WKDataTask *))completionHandler
{
    _page->dataTaskWithRequest(request, std::nullopt, !!runAtForegroundPriority, [completionHandler = makeBlockPtr(completionHandler)] (Ref<API::DataTask>&& task) {
        completionHandler(protectedWrapper(task.get()).get());
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
    return WebKit::stringForFind().createNSString().autorelease();
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

- (void)_convertPoint:(CGPoint)point fromFrame:(WKFrameInfo *)frame toMainFrameCoordinates:(void (^)(CGPoint, NSError *error))completionHandler
{
    if (!frame)
        [NSException raise:NSInternalInconsistencyException format:@"frame must be non-null"];

    _page->convertPointToMainFrameCoordinates(point, frame->_frameInfo->frameInfoData().frameID, [completionHandler = makeBlockPtr(completionHandler)] (std::optional<WebCore::FloatPoint> result) {
        if (result)
            completionHandler(*result, nil);
        else
            completionHandler({ }, unknownError().get());
    });
}

- (void)_convertRect:(CGRect)rect fromFrame:(WKFrameInfo *)frame toMainFrameCoordinates:(void (^)(CGRect, NSError *error))completionHandler
{
    if (!frame)
        [NSException raise:NSInternalInconsistencyException format:@"frame must be non-null"];

    _page->convertRectToMainFrameCoordinates(rect, frame->_frameInfo->frameInfoData().frameID, [completionHandler = makeBlockPtr(completionHandler)] (std::optional<WebCore::FloatRect> result) {
        if (result)
            completionHandler(*result, nil);
        else
            completionHandler({ }, unknownError().get());
    });
}

- (void)_hitTestAtPoint:(CGPoint)point inFrameCoordinateSpace:(WKFrameInfo *)frame completionHandler:(void (^)(_WKJSHandle *, NSError *))completionHandler
{
    RefPtr mainFrame = _page->mainFrame();
    if (!frame && !mainFrame)
        return completionHandler(nil, unknownError().get());
    _page->hitTestAtPoint(frame ? frame->_frameInfo->frameInfoData().frameID : mainFrame->frameID(), point, [completionHandler = makeBlockPtr(completionHandler)] (auto&& result) mutable {
        if (!result)
            return completionHandler(nil, unknownError().get());
        completionHandler(wrapper(API::JSHandle::create(WTFMove(*result))).get(), nil);
    });
}

- (void)_setStatusBarIsVisible:(BOOL)visible
{
    _page->setStatusBarIsVisible(visible);
}

- (BOOL)_statusBarIsVisible
{
    return _page->statusBarIsVisible();
}

- (void)_setMenuBarIsVisible:(BOOL)visible
{
    _page->setMenuBarIsVisible(visible);
}

- (BOOL)_menuBarIsVisible
{
    return _page->menuBarIsVisible();
}

- (void)_setToolbarsAreVisible:(BOOL)visible
{
    _page->setToolbarsAreVisible(visible);
}

- (BOOL)_toolbarsAreVisible
{
    return _page->toolbarsAreVisible();
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
    return !AXPreferenceHelpers::imageAnimationEnabled();
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

- (void)_textFragmentDirectiveFromSelectionWithCompletionHandler:(void(^)(NSURL *))completionHandler
{
    _page->createTextFragmentDirectiveFromSelection([completion = makeBlockPtr(completionHandler)](URL&& url) {
        completion(url.createNSURL().get());
    });
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
    _page->requestTargetedElement(Ref { *request->_request }, [completion = makeBlockPtr(completion)](auto& elements) {
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
    if (RefPtr frame = _page->mainFrame())
        return frame->url().createNSURL().autorelease();
    return nil;
}

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    [self _loadAlternateHTMLString:string baseURL:baseURL forUnreachableURL:unreachableURL withWebpagePreferences:nil];
}

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL withWebpagePreferences:(WKWebpagePreferences *)preferences
{
    THROW_IF_SUSPENDED;
    RetainPtr data = bridge_cast([string dataUsingEncoding:NSUTF8StringEncoding] ?: NSData.data);
    _page->loadAlternateHTML(WebCore::DataSegment::create(WTFMove(data)), "UTF-8"_s, baseURL, unreachableURL, preferences ? preferences->_websitePolicies.get() : nullptr);
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
    _page->loadAndDecodeImage(request, sizeConstraint, maximumBytesFromNetwork, [completionHandler = makeBlockPtr(completionHandler), url](Expected<Ref<WebCore::ShareableBitmap>, WebCore::ResourceError>&& result) mutable {
        if (!result) {
            if (result.error().isNull())
                return completionHandler(nil, WebCore::internalError(url).protectedNSError().get()); // This can happen if IPC fails.
            return completionHandler(nil, result.error().protectedNSError().get());
        }
        Ref bitmap = result.value();
#if PLATFORM(MAC)
        completionHandler(adoptNS([[NSImage alloc] initWithCGImage:bitmap->createPlatformImage().get() size:bitmap->size()]).get(), nil);
#else
        completionHandler(adoptNS([[UIImage alloc] initWithCGImage:bitmap->createPlatformImage().get()]).get(), nil);
#endif
    });
}

- (void)_getInformationFromImageData:(NSData *)imageData completionHandler:(void (^)(NSString * typeIdentifier, NSArray<NSValue *> *availableSizes, NSError *error))completionHandler
{
    _page->getInformationFromImageData(makeVector(imageData), [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result) {
            RetainPtr error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: WebCore::descriptionString(result.error()).createNSString().get() }]);
            return completionHandler(nil, nil, error.get());
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
        return completionHandler(typeIdentifier.createNSString().get(), availableSizes.autorelease(), nil);
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
            RetainPtr error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: @"Failed to decode data" }]);
            return completionHandler(nil, error.get());
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
            RetainPtr error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: @"Failed to decode data" }]);
            return completionHandler(nil, error.get());
        }

#if PLATFORM(MAC)
        RetainPtr cocoaImage = adoptNS([[NSImage alloc] initWithCGImage:bitmap->createPlatformImage().get() size:bitmap->size()]);
#else
        RetainPtr cocoaImage = adoptNS([[UIImage alloc] initWithCGImage:bitmap->createPlatformImage().get()]);
#endif
        completionHandler(cocoaImage.get(), nil);
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
    if (RefPtr mainFrame = _page->mainFrame())
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
        return _page->mainFrame()->mimeType().createNSString().autorelease();

    return nil;
}

- (NSString *)_userAgent
{
    return _page->userAgent().createNSString().autorelease();
}

- (NSString *)_applicationNameForUserAgent
{
    return _page->applicationNameForUserAgent().createNSString().autorelease();
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

    RefPtr provisionalPage = _page->provisionalPageProxy();
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
    return _page->protectedLegacyMainFrameProcess()->isResponsive();
}

- (void)_killWebContentProcess
{
    THROW_IF_SUSPENDED;
    if (![self _isValid])
        return;

    _page->protectedLegacyMainFrameProcess()->terminate();
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

    if (RefPtr provisionalPageProxy = _page->provisionalPageProxy()) {
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
    _page->configuration().protectedProcessPool()->backForwardCache().removeEntriesForPage(*_page);
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
    auto wrapper = [completionHandler = makeBlockPtr(completionHandler)] (Variant<WebKit::ContinueUnsafeLoad, URL>&& variant) {
        switchOn(variant, [&] (WebKit::ContinueUnsafeLoad continueUnsafeLoad) {
            switch (continueUnsafeLoad) {
            case WebKit::ContinueUnsafeLoad::Yes:
                return completionHandler(YES, nil);
            case WebKit::ContinueUnsafeLoad::No:
                return completionHandler(NO, nil);
            }
        }, [&] (URL url) {
            completionHandler(NO, url.createNSURL().get());
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
    return WebKit::BrowsingWarning::confirmMalwareSentinel().autorelease();
}

+ (NSURL *)_visitUnsafeWebsiteSentinel
{
    return WebKit::BrowsingWarning::visitUnsafeWebsiteSentinel().autorelease();
}

- (void)_isJITEnabled:(void(^)(BOOL))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->isJITEnabled([completionHandler = makeBlockPtr(completionHandler)] (bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_isEnhancedSecurityEnabled:(void(^)(BOOL))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->isEnhancedSecurityEnabled([completionHandler = makeBlockPtr(completionHandler)] (bool enabled) {
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
    if (RefPtr videoPresentationManager = _page->videoPresentationManager()) {
        hasOpenMediaPresentations = videoPresentationManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
            || videoPresentationManager->hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard);
    }

    if (!hasOpenMediaPresentations) {
        RefPtr fullScreenManager = _page->fullScreenManager();
        if (fullScreenManager && fullScreenManager->isFullScreen())
            hasOpenMediaPresentations = true;
    }

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
    _page->updateWebsitePolicies(Ref { *webpagePreferences->_websitePolicies });
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
    return _page->remoteInspectionNameOverride().createNSString().autorelease();
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
    _page->saveResources(_page->protectedMainFrame().get(), { }, directory.path, name, [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: WebCore::errorDescription(result.error()).createNSString().get() }]).get());

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
            RetainPtr<NSString> attributeLocalName = [retainPtr(rule.attributeLocalNames) objectAtIndex:index];
            RetainPtr<NSString> attributeValue = [retainPtr(rule.attributeValues) objectAtIndex:index];
            if (attributeLocalName && !attributeLocalName.get().length)
                attibutes.append({ AtomString { attributeLocalName.get() }, attributeValue.get() });
        }
        markupExclusionRules.append(WebCore::MarkupExclusionRule { AtomString { rule.elementLocalName }, WTFMove(attibutes) });
    }

    _page->saveResources(_page->protectedMainFrame().get(), WTFMove(markupExclusionRules), configuration.directory.path, configuration.suggestedFileName, [completionHandler = makeBlockPtr(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey: WebCore::errorDescription(result.error()).createNSString().get() }]).get());

        completionHandler(nil);
    });
}

- (void)_getMainResourceDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getMainResourceDataOfFrame(_page->protectedMainFrame().get(), [completionHandler = makeBlockPtr(completionHandler)](API::Data* data) {
        completionHandler(protectedWrapper(data).get(), nil);
    });
}

- (void)_getWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    [self createWebArchiveDataWithCompletionHandler:completionHandler];
}

- (void)_createWebArchiveForFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;

    std::optional<WebCore::FrameIdentifier> frameID;
    if (frame && frame._handle && frame._handle->_frameHandle->frameID())
        frameID = frame._handle->_frameHandle->frameID();

    if (!frameID) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Frame no longer exists." };
        completionHandler(nil, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());
        return;
    }

    RefPtr webFrame = WebKit::WebFrameProxy::webFrame(*frameID);
    if (!webFrame) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Frame with specified identifier no longer exists.", };
        completionHandler(nil, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());
        return;
    }

    _page->getWebArchiveDataWithFrame(*webFrame, [completionHandler = makeBlockPtr(completionHandler)](API::Data* data) {
        if (data)
            completionHandler(protectedWrapper(data).get(), nil);
        else
            completionHandler(nil, unknownError().get());
    });
}

- (void)_createWebArchiveForFrames:(NSArray<WKFrameInfo *> *)frames rootFrame:(WKFrameInfo *)rootFrame completionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;

    std::optional<WebCore::FrameIdentifier> rootFrameID;
    if (rootFrame && rootFrame._handle && rootFrame._handle->_frameHandle->frameID())
        rootFrameID = rootFrame._handle->_frameHandle->frameID();

    if (!rootFrameID) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Root frame no longer exists." };
        completionHandler(nil, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());
        return;
    }

    RefPtr webRootFrame = WebKit::WebFrameProxy::webFrame(*rootFrameID);
    if (!webRootFrame) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Root frame with specified identifier no longer exists.", };
        completionHandler(nil, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());
        return;
    }

    HashSet<WebCore::FrameIdentifier> targetFrameIDs;
    for (WKFrameInfo *frame in frames) {
        if (!frame)
            continue;

        RetainPtr<_WKFrameHandle> handle = frame._handle;
        if (!handle)
            continue;

        if (auto frameID = handle->_frameHandle->frameID())
            targetFrameIDs.add(*frameID);
    }

    _page->getWebArchiveDataWithSelectedFrames(*webRootFrame, targetFrameIDs, [completionHandler = makeBlockPtr(completionHandler)](API::Data* data) {
        if (data)
            completionHandler(protectedWrapper(data).get(), nil);
        else
            completionHandler(nil, unknownError().get());
    });
}

- (void)_getContentsAsStringWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::No, [handler = makeBlockPtr(completionHandler)](String string) {
        handler(string.createNSString().get(), nil);
    });
}

- (void)_getContentsAsStringWithCompletionHandlerKeepIPCConnectionAliveForTesting:(void (^)(NSString *, NSError *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::No, [handler = makeBlockPtr(completionHandler), connection = _page->legacyMainFrameProcess().protectedConnection()](String string) {
        handler(string.createNSString().get(), nil);
    });
}

- (void)_getContentsOfAllFramesAsStringWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::Yes, [handler = makeBlockPtr(completionHandler)](String string) {
        handler(string.createNSString().get());
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
                Ref apiManifest = API::ApplicationManifest::create(*manifest);
                completionHandler(protectedWrapper(apiManifest.get()).get());
            } else
                completionHandler(nil);
        }
    });
}

- (void)_getTextFragmentMatchWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    THROW_IF_SUSPENDED;
    _page->getTextFragmentMatch([completionHandler = makeBlockPtr(completionHandler)](const String& textFragmentMatch) {
        completionHandler(nsStringNilIfNull(textFragmentMatch).get());
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

    return downcast<WebKit::DiagnosticLoggingClient>(*diagnosticLoggingClient).delegate().autorelease();
}

- (void)_setDiagnosticLoggingDelegate:(id<_WKDiagnosticLoggingDelegate>)diagnosticLoggingDelegate
{
    auto* diagnosticLoggingClient = _page->diagnosticLoggingClient();
    if (!diagnosticLoggingClient)
        return;

    downcast<WebKit::DiagnosticLoggingClient>(*diagnosticLoggingClient).setDelegate(diagnosticLoggingDelegate);
}

- (id <_WKFindDelegate>)_findDelegate
{
    return downcast<WebKit::FindClient>(_page->findClient()).delegate().autorelease();
}

- (void)_setFindDelegate:(id<_WKFindDelegate>)findDelegate
{
    downcast<WebKit::FindClient>(_page->findClient()).setDelegate(findDelegate);
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
    _page->recordNavigationSnapshot(Ref { item._item });
}

- (void)_serviceWorkersEnabled:(void(^)(BOOL))completionHandler
{
    auto enabled = Ref { *[_configuration preferences]->_preferences }->serviceWorkersEnabled();
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

- (WebCore::CocoaColor *)_sampledPageTopColor
{
    return cocoaColorOrNil(_page->sampledPageTopColor()).autorelease();
}

- (_WKSpatialBackdropSource *)_spatialBackdropSource
{
#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    return _cachedSpatialBackdropSource.get();
#else
    return nil;
#endif
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

        void willSubmitForm(WebKit::WebPageProxy& page, WebKit::WebFrameProxy&, WebKit::WebFrameProxy&, WebKit::FrameInfoData&& frameInfoData, WebKit::FrameInfoData&& sourceFrameInfoData, const Vector<std::pair<WTF::String, WTF::String>>& textFieldValues, API::Object* userData, const WTF::URL& requestURL, const WTF::String& method, CompletionHandler<void()>&& completionHandler) override
        {
            auto webView = m_webView.get();
            if (!webView)
                return completionHandler();

            auto inputDelegate = webView->_inputDelegate.get();

            SEL willSubmitFormValuesSelector = @selector(_webView:willSubmitFormValues:frameInfo:sourceFrameInfo:userObject:requestURL:method:submissionHandler:);
            SEL willSubmitFormValuesWithoutRequestURLSelector = @selector(_webView:willSubmitFormValues:frameInfo:sourceFrameInfo:userObject:submissionHandler:);
            SEL willSubmitFormValuesLegacySelector = @selector(_webView:willSubmitFormValues:userObject:submissionHandler:);

            bool inputDelegateRespondsToWillSubmitFormValues = [inputDelegate respondsToSelector:willSubmitFormValuesSelector];
            bool inputDelegateRespondsToWillSubmitFormValuesWithoutRequestURL = [inputDelegate respondsToSelector:willSubmitFormValuesWithoutRequestURLSelector];
            bool inputDelegateRespondsToWillSubmitFormValuesLegacy = [inputDelegate respondsToSelector:willSubmitFormValuesLegacySelector];

            if (!inputDelegateRespondsToWillSubmitFormValues && !inputDelegateRespondsToWillSubmitFormValuesWithoutRequestURL && !inputDelegateRespondsToWillSubmitFormValuesLegacy) {
                completionHandler();
                return;
            }

            auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:textFieldValues.size()]);
            for (const auto& pair : textFieldValues)
                [valueMap setObject:pair.second.createNSString().get() forKey:pair.first.createNSString().get()];

            auto userObject = userData ? userData->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>();

            if (inputDelegateRespondsToWillSubmitFormValuesLegacy) {
                auto checker = WebKit::CompletionHandlerCallChecker::create(inputDelegate.get(), willSubmitFormValuesLegacySelector);
                [inputDelegate _webView:webView.get() willSubmitFormValues:valueMap.get() userObject:userObject.get() submissionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] () mutable {
                    if (checker->completionHandlerHasBeenCalled())
                        return;
                    checker->didCallCompletionHandler();
                    completionHandler();
                }).get()];
                return;
            }

            if (inputDelegateRespondsToWillSubmitFormValuesWithoutRequestURL) {
                auto checker = WebKit::CompletionHandlerCallChecker::create(inputDelegate.get(), willSubmitFormValuesWithoutRequestURLSelector);
                auto frameInfo = wrapper(API::FrameInfo::create(WTFMove(frameInfoData)));
                auto sourceFrameInfo = wrapper(API::FrameInfo::create(WTFMove(sourceFrameInfoData)));
                [inputDelegate _webView:webView.get() willSubmitFormValues:valueMap.get() frameInfo:frameInfo.get() sourceFrameInfo:sourceFrameInfo.get() userObject:userObject.get() submissionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] () mutable {
                    if (checker->completionHandlerHasBeenCalled())
                        return;
                    checker->didCallCompletionHandler();
                    completionHandler();
                }).get()];
                return;
            }

            auto checker = WebKit::CompletionHandlerCallChecker::create(inputDelegate.get(), willSubmitFormValuesSelector);
            auto frameInfo = wrapper(API::FrameInfo::create(WTFMove(frameInfoData)));
            auto sourceFrameInfo = wrapper(API::FrameInfo::create(WTFMove(sourceFrameInfoData)));
            [inputDelegate _webView:webView.get() willSubmitFormValues:valueMap.get() frameInfo:frameInfo.get() sourceFrameInfo:sourceFrameInfo.get() userObject:userObject.get() requestURL:requestURL.createNSURL().get() method:method.createNSString().get() submissionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] () mutable {
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
    if (!_page)
        return NO;

    RefPtr mainFrame = _page->mainFrame();
    if (!mainFrame)
        return NO;

    return static_cast<BOOL>(mainFrame->isDisplayingPDFDocument());
}

- (BOOL)_isDisplayingStandaloneImageDocument
{
    if (RefPtr mainFrame = _page->mainFrame())
        return mainFrame->isDisplayingStandaloneImageDocument();
    return NO;
}

- (BOOL)_isDisplayingStandaloneMediaDocument
{
    if (RefPtr mainFrame = _page->mainFrame())
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
        handler(name.createNSString().get());
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
    RefPtr fullScreenManager = _page->fullScreenManager();
    return fullScreenManager && fullScreenManager->isFullScreen();
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
    if (mutedState & _WKMediaScreenCaptureMuted) {
        coreState.add(WebCore::MediaProducerMutedState::ScreenCaptureIsMuted);
        coreState.add(WebCore::MediaProducerMutedState::WindowCaptureIsMuted);
        coreState.add(WebCore::MediaProducerMutedState::SystemAudioCaptureIsMuted);
    }

    _page->setMuted(coreState, WebKit::WebPageProxy::FromApplication::Yes);
}

- (void)_removeDataDetectedLinks:(dispatch_block_t)completion
{
    THROW_IF_SUSPENDED;
#if ENABLE(DATA_DETECTION)
    _page->removeDataDetectedLinks([completion = makeBlockPtr(completion), page = WeakPtr { _page.get() }] (auto&& result) {
        if (page)
            page->setDataDetectionResult(WTFMove(result));
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
    RunLoop::mainSingleton().dispatch([updateBlock = makeBlockPtr(updateBlock)] {
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
        elements.append(Ref { *element->_info });
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
    _page->createTextIndicatorForElementWithID(elementID, [completionHandler = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKWebView>(self)](RefPtr<WebCore::TextIndicator>&& textIndicator) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completionHandler(nil);
            return;
        }

        if (!textIndicator) {
            completionHandler(nil);
            return;
        }

        RetainPtr preview = [strongSelf->_contentView _createTargetedPreviewFromTextIndicator:WTFMove(textIndicator) previewContainer:strongSelf.get()];
        completionHandler(preview.get());
    });
}
#elif PLATFORM(MAC)
- (void)_textPreviewsForElementWithID:(NSString *)elementID completionHandler:(void (^)(NSArray<_WKTextPreview *> *))completionHandler
{
    _page->createTextIndicatorForElementWithID(elementID, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<WebCore::TextIndicator>&& textIndicator) {
        if (!textIndicator) {
            completionHandler(@[ ]);
            return;
        }

        RefPtr contentImage = textIndicator->contentImage();
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

        auto textBoundingRectInRootViewCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();
        auto textRectsInBoundingRectCoordinates = textIndicator->textRectsInBoundingRectCoordinates();
        auto contentImageScaleFactor = textIndicator->contentImageScaleFactor();

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

- (void)_fetchDataOfTypes:(_WKWebViewDataType)dataTypes completionHandler:(void (^)(NSData *))completionHandler
{
    [self fetchDataOfTypes:dataTypes completionHandler:makeBlockPtr([completionHandler = makeBlockPtr(completionHandler)](NSData *data, NSError *error) {
        UNUSED_PARAM(error);
        completionHandler(data);
    }).get()];
}

- (void)_restoreData:(NSData *)data completionHandler:(void(^)(BOOL))completionHandler
{
    [self restoreData:data completionHandler:makeBlockPtr([completionHandler = makeBlockPtr(completionHandler)](NSError *error) {
        completionHandler(!error);
    }).get()];
}

- (audit_token_t)presentingApplicationAuditToken
{
    return self._protectedPage->presentingApplicationAuditToken().value_or(audit_token_t { });
}

- (void)setPresentingApplicationAuditToken:(audit_token_t)presentingApplicationAuditToken
{
    self._protectedPage->setPresentingApplicationAuditToken(presentingApplicationAuditToken);
}

- (BOOL)_useSystemAppearance
{
    return [[_configuration preferences] _useSystemAppearance];
}

- (void)_setUseSystemAppearance:(BOOL)useSystemAppearance
{
    [[_configuration preferences] _setUseSystemAppearance:useSystemAppearance];
}

- (void)_scrollToEdge:(_WKRectEdge)edge animated:(BOOL)animated
{
    self._protectedPage->scrollToEdge(toRectEdges(edge), animated ? WebCore::ScrollIsAnimated::Yes : WebCore::ScrollIsAnimated::No);
}

- (BOOL)_shouldSuppressTopColorExtensionView
{
    return _shouldSuppressTopColorExtensionView;
}

- (void)_setShouldSuppressTopColorExtensionView:(BOOL)value
{
    if (_shouldSuppressTopColorExtensionView == value)
        return;

    _shouldSuppressTopColorExtensionView = value;

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    [self _doAfterAdjustingColorForTopContentInsetFromUIDelegate:[strongSelf = RetainPtr { self }] {
        [strongSelf _updateFixedColorExtensionViews];
        [strongSelf _updateTopScrollPocketCaptureColor];
    }];
#endif
}

- (void)_takeSnapshotOfNode:(_WKJSHandle *)node completionHandler:(void (^)(CocoaImage *image, NSError *))completionHandler
{
    if (!node)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

    auto info = node->_ref->info();
    RefPtr webFrame = WebKit::WebFrameProxy::webFrame(info.frameInfo.frameID);
    if (!webFrame)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorWebViewInvalidated userInfo:nil]);

    webFrame->takeSnapshotOfNode(info.identifier, [completionHandler = makeBlockPtr(completionHandler)](auto&& handle) {
        auto makeUnknownError = [] {
            return [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil];
        };

        if (!handle)
            return completionHandler(nil, makeUnknownError());

        RefPtr bitmap = WebCore::ShareableBitmap::create(WTFMove(*handle), WebCore::SharedMemory::Protection::ReadOnly);
        if (!bitmap)
            return completionHandler(nil, makeUnknownError());

        RetainPtr cgImage = bitmap->createPlatformImage();
#if PLATFORM(MAC)
        RetainPtr image = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:bitmap->size()]);
#else
        RetainPtr image = adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
#endif
        completionHandler(image.get(), nil);
    });
}

#if PLATFORM(MAC)
- (NSUInteger)accessibilityRemoteChildTokenHash
{
    return _impl->accessibilityRemoteChildTokenHash();
}

- (NSUInteger)accessibilityUIProcessLocalTokenHash
{
    return _impl->accessibilityUIProcessLocalTokenHash();
}

- (NSArray<NSNumber *> *)registeredRemoteAccessibilityPids
{
    return _impl->registeredRemoteAccessibilityPids();
}

- (bool)hasRemoteAccessibilityChild
{
    return _impl->hasRemoteAccessibilityChild();
}

- (RetainPtr<NSPopUpButtonCell>)_activePopupButtonCell
{
    RefPtr popupMenu = dynamicDowncast<WebKit::WebPopupMenuProxyMac>(_page->activePopupMenu());
    if (!popupMenu)
        return nil;

    if (!popupMenu->isVisible())
        return nil;

    return popupMenu->protectedPopup();
}

#endif // PLATFORM(MAC)

- (Vector<String>)_activeNativeMenuItemTitles
{
#if PLATFORM(MAC)
    RetainPtr allItems = [[self _activePopupButtonCell] itemArray];
    Vector<String> itemTitles;
    itemTitles.reserveInitialCapacity([allItems count]);
    for (NSMenuItem *item in allItems.get()) {
        if (!item.enabled)
            continue;

        if (RetainPtr title = [item title]; [title length])
            itemTitles.append({ title.get() });
    }

    return itemTitles;
#else
    // FIXME: Bridge this into UIKit's context menu interaction.
    return { };
#endif
}

- (void)_debugTextWithConfiguration:(_WKTextExtractionConfiguration *)configuration completionHandler:(void(^)(NSString *))completionHandler
{
#if ENABLE(TEXT_EXTRACTION)
    [self _extractDebugTextWithConfiguration:configuration completionHandler:makeBlockPtr([completionHandler = makeBlockPtr(completionHandler)](_WKTextExtractionResult *result) {
        completionHandler(result.textContent);
    }).get()];
#endif
}

#if ENABLE(TEXT_EXTRACTION)
static HashMap<String, String> extractReplacementStrings(_WKTextExtractionConfiguration *configuration)
{
    HashMap<String, String> result;
    RetainPtr replacementStrings = [configuration replacementStrings];
    for (NSString *replacement in replacementStrings.get()) {
        if (!replacement.length)
            continue;

        result.set(String { replacement }, String { [replacementStrings objectForKey:replacement] });
    }
    return result;
}

static WebKit::TextExtractionOutputFormat textExtractionOutputFormat(_WKTextExtractionConfiguration *configuration)
{
    switch (configuration.outputFormat) {
    case _WKTextExtractionOutputFormatTextTree:
        return WebKit::TextExtractionOutputFormat::TextTree;
    case _WKTextExtractionOutputFormatHTML:
        return WebKit::TextExtractionOutputFormat::HTMLMarkup;
    case _WKTextExtractionOutputFormatMarkdown:
        return WebKit::TextExtractionOutputFormat::Markdown;
    default:
        ASSERT_NOT_REACHED();
        return WebKit::TextExtractionOutputFormat::TextTree;
    }
}
#endif // ENABLE(TEXT_EXTRACTION)

- (void)_extractDebugTextWithConfiguration:(_WKTextExtractionConfiguration *)configuration completionHandler:(void(^)(_WKTextExtractionResult *))completionHandler
{
#if ENABLE(TEXT_EXTRACTION)
    bool allowFiltering = _page->protectedPreferences()->textExtractionFilterEnabled();
    bool filterUsingClassifier = allowFiltering && configuration.filterOptions & _WKTextExtractionFilterClassifier;
    bool filterHiddenText = allowFiltering && configuration.filterOptions & _WKTextExtractionFilterTextRecognition;
    bool filterUsingRules = allowFiltering && configuration.filterOptions & _WKTextExtractionFilterRules;

#if ENABLE(TEXT_EXTRACTION_FILTER)
    if (filterUsingClassifier)
        WebKit::TextExtractionFilter::singleton().prewarm();
#endif

    std::optional<WebKit::TextExtractionVersion> version;
    if (RetainPtr overrideVersion = dynamic_objc_cast<NSNumber>([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2TextExtractionOutputVersion"]))
        version = [overrideVersion unsignedIntValue];

    std::optional<uint64_t> maxWordsPerParagraph;
    if (configuration.maxWordsPerParagraph < NSUIntegerMax)
        maxWordsPerParagraph = { static_cast<uint64_t>(configuration.maxWordsPerParagraph) };

    [self _requestTextExtractionInternal:configuration completion:[
        completionHandler = makeBlockPtr(completionHandler),
        weakSelf = WeakObjCPtr<WKWebView>(self),
        filterUsingClassifier,
        filterHiddenText,
        filterUsingRules,
        includeURLs = configuration.includeURLs,
        includeRects = configuration.includeRects,
        onlyIncludeText = configuration.onlyIncludeVisibleText,
        maxWordsPerParagraph = WTFMove(maxWordsPerParagraph),
        version,
        replacementStrings = extractReplacementStrings(configuration),
        outputFormat = textExtractionOutputFormat(configuration)
    ](auto&& item) mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(nil);

        if (!item)
            return completionHandler(nil);

        Vector<WebKit::TextExtractionFilterCallback> filterCallbacks;

        if (filterUsingClassifier) {
#if ENABLE(TEXT_EXTRACTION_FILTER)
            filterCallbacks.append([](auto& text, auto&&) mutable {
                WebKit::TextExtractionFilterPromise::Producer producer;
                Ref promise = producer.promise();

                WebKit::TextExtractionFilter::singleton().shouldFilter(text, [producer = WTFMove(producer), text](bool shouldFilterOut) mutable {
                    if (shouldFilterOut)
                        producer.settle(emptyString());
                    else
                        producer.settle(text);
                });
                return promise;
            });
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
        }

        if (filterHiddenText) {
#if ENABLE(TEXT_EXTRACTION_FILTER)
            filterCallbacks.append([strongSelf](auto& text, auto&& enclosingNodeID) mutable {
                WebKit::TextExtractionFilterPromise::Producer producer;
                Ref promise = producer.promise();

                auto lines = text.splitAllowingEmptyEntries('\n');
                auto components = Box<Vector<String>>::create();
                components->resizeToFit(lines.size());

                Ref aggregator = MainRunLoopCallbackAggregator::create([producer = WTFMove(producer), components] mutable {
                    producer.settle(makeStringByJoining(WTFMove(*components), "\n"_s));
                });

                for (size_t index = 0; index < lines.size(); ++index) {
                    static constexpr auto minimumLengthForTextDetection = 100;
                    auto line = lines[index];
                    if (line.length() < minimumLengthForTextDetection) {
                        components->at(index) = WTFMove(line);
                        continue;
                    }

                    [strongSelf _validateText:line inNode:std::optional { enclosingNodeID } completionHandler:[aggregator, components, index](auto& result) mutable {
                        components->at(index) = result;
                    }];
                }

                return promise;
            });
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
        }

        if (filterUsingRules) {
#if ENABLE(TEXT_EXTRACTION_FILTER)
            filterCallbacks.append([page = strongSelf->_page](auto& text, auto&& enclosingNodeID) mutable {
                WebKit::TextExtractionFilterPromise::Producer producer;
                Ref promise = producer.promise();

                page->applyTextExtractionFilter(text, WTFMove(enclosingNodeID), [producer = WTFMove(producer)](auto&& output) mutable {
                    producer.settle(WTFMove(output));
                });

                return promise;
            });
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
        }

        if (maxWordsPerParagraph) {
            filterCallbacks.append([wordLimit = WTFMove(maxWordsPerParagraph)](auto& text, auto&&) mutable {
                auto truncatedComponents = text.splitAllowingEmptyEntries('\n').map([wordLimit](auto&& component) {
                    if (component.isEmpty())
                        return emptyString();

                    auto* iterator = WTF::wordBreakIterator(component);
                    if (!iterator)
                        return component;

                    uint64_t wordCount = 0;
                    int position = 0;
                    int stringLength = component.length();

                    while (position < stringLength) {
                        position = ubrk_following(iterator, position);
                        if (position == UBRK_DONE)
                            break;

                        if (!position || !u_isalnum(component[position - 1]))
                            continue;

                        wordCount++;
                        if (wordCount != wordLimit)
                            continue;

                        return position < stringLength ? makeString(component.left(position), u"…") : component;
                    }

                    return component;
                });

                auto truncatedString = makeStringByJoining(WTFMove(truncatedComponents), "\n"_s);
                return WebKit::TextExtractionFilterPromise::createAndResolve(WTFMove(truncatedString));
            });
        }

        using enum WebKit::TextExtractionOptionFlag;
        WebKit::TextExtractionOptionFlags optionFlags;
        if (includeURLs)
            optionFlags.add(IncludeURLs);
        if (includeRects)
            optionFlags.add(IncludeRects);
        if (onlyIncludeText)
            optionFlags.add(OnlyIncludeText);
        WebKit::TextExtractionOptions options {
            WTFMove(filterCallbacks),
            [strongSelf _activeNativeMenuItemTitles],
            WTFMove(replacementStrings),
            version,
            optionFlags,
            outputFormat
        };
        WebKit::convertToText(WTFMove(*item), WTFMove(options), [completionHandler = WTFMove(completionHandler)](auto&& result) {
            auto [text, filteredOutAnyText] = result;
            completionHandler(adoptNS([[_WKTextExtractionResult alloc] initWithTextContent:text.createNSString().get() filteredOutAnyText:filteredOutAnyText]).get());
        });
    }];
#endif // ENABLE(TEXT_EXTRACTION)
}

static inline std::optional<WebCore::NodeIdentifier> toNodeIdentifier(const String& nodeIdentifier)
{
    auto rawValue = parseInteger<uint64_t>(nodeIdentifier);
    if (!rawValue || !WebCore::NodeIdentifier::isValidIdentifier(*rawValue))
        return std::nullopt;
    return WebCore::NodeIdentifier { *rawValue };
}

#if ENABLE(TEXT_EXTRACTION)
- (WebCore::TextExtraction::Interaction)_convertToWebCoreInteraction:(_WKTextExtractionInteraction *)wkInteraction
{
    WebCore::TextExtraction::Interaction interaction;
    interaction.action = [&] {
        switch (wkInteraction.action) {
        case _WKTextExtractionActionClick:
            return WebCore::TextExtraction::Action::Click;
        case _WKTextExtractionActionSelectText:
            return WebCore::TextExtraction::Action::SelectText;
        case _WKTextExtractionActionSelectMenuItem:
            return WebCore::TextExtraction::Action::SelectMenuItem;
        case _WKTextExtractionActionTextInput:
            return WebCore::TextExtraction::Action::TextInput;
        case _WKTextExtractionActionKeyPress:
            return WebCore::TextExtraction::Action::KeyPress;
        case _WKTextExtractionActionHighlightText:
            return WebCore::TextExtraction::Action::HighlightText;
        case _WKTextExtractionActionScrollBy:
            return WebCore::TextExtraction::Action::ScrollBy;
        default:
            ASSERT_NOT_REACHED();
            return WebCore::TextExtraction::Action::Click;
        }
    }();

    interaction.nodeIdentifier = toNodeIdentifier(wkInteraction.nodeIdentifier);
    if (wkInteraction.hasSetLocation) {
#if PLATFORM(IOS_FAMILY)
        interaction.locationInRootView = [self convertPoint:wkInteraction.location toView:_contentView.get()];
#else
        interaction.locationInRootView = wkInteraction.location;
#endif
    }
    interaction.text = wkInteraction.text;
    interaction.replaceAll = wkInteraction.replaceAll;
    interaction.scrollToVisible = wkInteraction.scrollToVisible;
    interaction.scrollDelta = WebCore::FloatSize { wkInteraction.scrollDelta };
    return interaction;
}
#endif // ENABLE(TEXT_EXTRACTION)

- (void)_performInteraction:(_WKTextExtractionInteraction *)wkInteraction completionHandler:(void(^)(_WKTextExtractionInteractionResult *))completionHandler
{
#if ENABLE(TEXT_EXTRACTION)
    if (!self._isValid)
        return completionHandler(adoptNS([[_WKTextExtractionInteractionResult alloc] initWithErrorDescription:@"Web view is invalid"]).get());

    if (!_page->protectedPreferences()->textExtractionEnabled())
        return completionHandler(adoptNS([[_WKTextExtractionInteractionResult alloc] initWithErrorDescription:@"Text extraction is unavailable"]).get());

    auto interaction = [self _convertToWebCoreInteraction:wkInteraction];

    RefPtr page = _page;
#if PLATFORM(MAC)
    RetainPtr nativePopup = [self _activePopupButtonCell];
    if (nativePopup && interaction.action == WebCore::TextExtraction::Action::SelectMenuItem && !interaction.text.isEmpty()) {
        RetainPtr title = interaction.text.createNSString();
        BOOL foundItem = [nativePopup indexOfItemWithTitle:title.get()] != -1;
        if (foundItem) {
            [nativePopup selectItemWithTitle:title.get()];
            [retainPtr([nativePopup menu]) cancelTrackingWithoutAnimation];
        }
        page->callAfterNextPresentationUpdate([title, foundItem, completionHandler = makeBlockPtr(WTFMove(completionHandler))] {
            RetainPtr<NSString> errorDescription = foundItem ? nil : [NSString stringWithFormat:@"No popup menu item with title '%@'", title.get()];
            RetainPtr result = adoptNS([[_WKTextExtractionInteractionResult alloc] initWithErrorDescription:errorDescription.get()]);
            completionHandler(result.get());
        });
        return;
    }
#endif // PLATFORM(MAC)

    page->handleTextExtractionInteraction(WTFMove(interaction), [weakSelf = WeakObjCPtr<WKWebView>(self), weakPage = WeakPtr { *page }, completionHandler = makeBlockPtr(WTFMove(completionHandler))](bool success, String&& description) mutable {
        RetainPtr<NSString> errorDescription;
        if (!success)
            errorDescription = description.createNSString();
        RetainPtr result = adoptNS([[_WKTextExtractionInteractionResult alloc] initWithErrorDescription:errorDescription.get()]);
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(result.get());

        RefPtr strongPage = weakPage.get();
        if (!strongPage)
            return completionHandler(result.get());

        strongPage->callAfterNextPresentationUpdate([completionHandler = WTFMove(completionHandler), result] {
            completionHandler(result.get());
        });
    });
#endif // ENABLE(TEXT_EXTRACTION)
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

#if ENABLE(TEXT_EXTRACTION)

static std::optional<WebCore::JSHandleIdentifier> mainFrameJSHandleIdentifier(_WKJSHandle *nodeHandle)
{
    if (!nodeHandle)
        return std::nullopt;

    auto info = nodeHandle->_ref->info();
    if (!info.frameInfo.isMainFrame) {
        // FIXME: Blocked on support for text extraction in subframes.
        return std::nullopt;
    }

    return info.identifier;
}

static HashMap<String, HashMap<WebCore::JSHandleIdentifier, String>> extractClientNodeAttributes(_WKTextExtractionConfiguration *configuration)
{
    __block HashMap<String, HashMap<WebCore::JSHandleIdentifier, String>> result;

    [configuration forEachClientNodeAttribute:^(NSString *attribute, NSString *value, _WKJSHandle *nodeHandle) {
        auto handleIdentifier = mainFrameJSHandleIdentifier(nodeHandle);
        if (!handleIdentifier)
            return;

        result.ensure(String { attribute }, [] {
            return HashMap<WebCore::JSHandleIdentifier, String> { };
        }).iterator->value.add(WTFMove(*handleIdentifier), String { value });
    }];

    return result;
}

#endif // ENABLE(TEXT_EXTRACTION)

- (void)_requestTextExtractionInternal:(_WKTextExtractionConfiguration *)configuration completion:(CompletionHandler<void(std::optional<WebCore::TextExtraction::Item>&&)>&&)completion
{
#if ENABLE(TEXT_EXTRACTION)
    Ref preferences = _page->preferences();
    if (!self._isValid || !preferences->textExtractionEnabled())
        return completion({ });

    auto rectInWebView = configuration.targetRect;
    bool mergeParagraphs = configuration.mergeParagraphs;
    auto nodeIdentifierInclusion = [&] {
        switch (configuration.nodeIdentifierInclusion) {
        case _WKTextExtractionNodeIdentifierInclusionNone:
            return WebCore::TextExtraction::NodeIdentifierInclusion::None;
        case _WKTextExtractionNodeIdentifierInclusionEditableOnly:
            return WebCore::TextExtraction::NodeIdentifierInclusion::EditableOnly;
        case _WKTextExtractionNodeIdentifierInclusionInteractive:
            return WebCore::TextExtraction::NodeIdentifierInclusion::Interactive;
        }
        return WebCore::TextExtraction::NodeIdentifierInclusion::None;
    }();
    bool skipNearlyTransparentContent = configuration.skipNearlyTransparentContent;
    auto rectInRootView = [&] -> std::optional<WebCore::FloatRect> {
        if (CGRectIsNull(rectInWebView))
            return std::nullopt;

#if PLATFORM(IOS_FAMILY)
        return WebCore::FloatRect { [self convertRect:rectInWebView toView:_contentView.get()] };
#else
        return WebCore::FloatRect { rectInWebView };
#endif
    }();

    WebCore::TextExtraction::Request request {
        .clientNodeAttributes = extractClientNodeAttributes(configuration),
        .collectionRectInRootView = WTFMove(rectInRootView),
        .targetNodeHandleIdentifier = mainFrameJSHandleIdentifier(configuration.targetNode),
        .mergeParagraphs = mergeParagraphs,
        .skipNearlyTransparentContent = skipNearlyTransparentContent,
        .nodeIdentifierInclusion = nodeIdentifierInclusion,
        .includeEventListeners = !!configuration.includeEventListeners,
        .includeAccessibilityAttributes = !!configuration.includeAccessibilityAttributes,
        .includeTextInAutoFilledControls = !!configuration.includeTextInAutoFilledControls,
    };

    _page->requestTextExtraction(WTFMove(request), WTFMove(completion));
#endif // ENABLE(TEXT_EXTRACTION)
}

- (void)_requestTextExtraction:(_WKTextExtractionConfiguration *)configuration completionHandler:(void(^)(WKTextExtractionItem *))completionHandler
{
#if ENABLE(TEXT_EXTRACTION)
    [self _requestTextExtractionInternal:configuration completion:[completionHandler = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKWebView>(self)](auto&& item) {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(nil);

        if (!item)
            return completionHandler(nil);

        RetainPtr rootItem = WebKit::createItem(WTFMove(*item), [strongSelf](auto& rectInRootView) -> WebCore::FloatRect {
#if PLATFORM(IOS_FAMILY)
            if (RetainPtr contentView = strongSelf ? strongSelf->_contentView : nil)
                return { [strongSelf convertRect:rectInRootView fromView:contentView.get()] };
#endif
            return rectInRootView;
        });
        completionHandler(rootItem.get());
    }];
#endif // ENABLE(TEXT_EXTRACTION)
}

- (void)_describeInteraction:(_WKTextExtractionInteraction *)wkInteraction completionHandler:(void (^)(NSString *, NSError *))completionHandler
{
#if ENABLE(TEXT_EXTRACTION)
    if (!self._isValid)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorWebViewInvalidated userInfo:nil]);

    RefPtr page = _page;
    if (!page->protectedPreferences()->textExtractionEnabled())
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

    auto interaction = [self _convertToWebCoreInteraction:wkInteraction];
#if PLATFORM(MAC)
    if ([self _activePopupButtonCell] && interaction.action == WebCore::TextExtraction::Action::SelectMenuItem && !interaction.text.isEmpty())
        return completionHandler([NSString stringWithFormat:@"Select popup menu item labeled '%s'", interaction.text.utf8().data()], nil);
#endif

    page->describeTextExtractionInteraction(WTFMove(interaction), [weakSelf = WeakObjCPtr<WKWebView>(self), weakPage = WeakPtr { *page }, completionHandler = makeBlockPtr(WTFMove(completionHandler))](auto&& result) mutable {
        auto [description, stringsToValidate] = WTFMove(result);
        auto valid = Box<bool>::create(true);
        Ref aggregator = MainRunLoopCallbackAggregator::create([completionHandler = WTFMove(completionHandler), description, valid] {
            if (!valid.get()) {
                completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{
                    NSDebugDescriptionErrorKey: @"One or more strings failed validation."
                }]);
                return;
            }

            completionHandler(description.createNSString().get(), nil);
        });

        RefPtr page = weakPage.get();
        if (!page || !page->protectedPreferences()->textExtractionFilterEnabled())
            return;

#if ENABLE(TEXT_EXTRACTION_FILTER)
        for (auto& string : stringsToValidate) {
            WebKit::TextExtractionFilter::singleton().shouldFilter(string, [aggregator = aggregator.copyRef(), valid](bool result) {
                if (result)
                    *valid = false;
            });
        }
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
    });
#endif // ENABLE(TEXT_EXTRACTION)
}

#if ENABLE(TEXT_EXTRACTION_FILTER)

- (void)_validateText:(const String&)text inNode:(std::optional<WebCore::NodeIdentifier>&&)nodeIdentifier completionHandler:(CompletionHandler<void(const String&)>&&)completionHandler
{
#if ENABLE(TEXT_EXTRACTION)
    if (text.isEmpty())
        return completionHandler(text);

    auto textHash = text.hash();
    if (auto cachedResult = _textValidationCache.getOptional(textHash)) {
        return WTF::switchOn(*cachedResult, [&](const String& stringResult) {
            completionHandler(stringResult);
        }, [&](SimilarToOriginalTextTag) {
            completionHandler(text);
        });
    }

    _page->takeSnapshotOfExtractedText({ text, WTFMove(nodeIdentifier) }, [text, completionHandler = WTFMove(completionHandler), view = retainPtr(self), textHash](auto textIndicator) mutable {
        if (!textIndicator)
            return completionHandler(text);

        RefPtr contentImage = textIndicator->contentImage();
        if (!contentImage)
            return completionHandler(text);

        RefPtr nativeImage = contentImage->nativeImage();
        if (!nativeImage)
            return completionHandler(text);

        RetainPtr cgImage = nativeImage->platformImage();
        if (!cgImage)
            return completionHandler(text);

        WebKit::recognizeText(cgImage.get(), [text = WTFMove(text), completionHandler = WTFMove(completionHandler), view = WTFMove(view), textHash](NSString *recognizedText, NSError *error) mutable {
            if (error)
                return completionHandler(text);

            // FIXME: This similarity threshold seems low, but in practice, dense but visible text sometimes
            // gets partially ignored in text recognition results. In the future, we could consider raising
            // this threshold if we get a more reliable way to recognize dense text.
            static constexpr auto minimumSimilarity = 0.5;
            static constexpr auto minimumLength = 10;

            auto similarity = WebKit::computeSimilarity(text.createNSString().get(), recognizedText, minimumLength);
            if (similarity < minimumSimilarity) {
                view->_textValidationCache.add(textHash, TextValidationMapValue { String { recognizedText } });
                completionHandler(recognizedText);
            } else {
                view->_textValidationCache.add(textHash, TextValidationMapValue { SimilarToOriginalTextTag::Value });
                return completionHandler(text);
            }
        });
    });
#endif // ENABLE(TEXT_EXTRACTION)
}

- (void)_clearTextExtractionFilterCache
{
    if (!self.window)
        return;

    if (RefPtr filter = WebKit::TextExtractionFilter::singletonIfCreated())
        filter->resetCache();

    _textValidationCache.clear();
}

#endif // ENABLE(TEXT_EXTRACTION_FILTER)

@end

#undef WKWEBVIEW_RELEASE_LOG
