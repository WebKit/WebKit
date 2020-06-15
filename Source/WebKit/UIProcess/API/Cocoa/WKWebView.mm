/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#import "APIFormClient.h"
#import "APIFrameTreeNode.h"
#import "APIPageConfiguration.h"
#import "APISerializedScriptValue.h"
#import "CocoaImage.h"
#import "CompletionHandlerCallChecker.h"
#import "ContentAsStringIncludesChildFrames.h"
#import "DiagnosticLoggingClient.h"
#import "FindClient.h"
#import "FullscreenClient.h"
#import "GlobalFindInPageState.h"
#import "IconLoadingDelegate.h"
#import "InspectorDelegate.h"
#import "LegacySessionStateCoding.h"
#import "Logging.h"
#import "MediaUtilities.h"
#import "NavigationState.h"
#import "ObjCObjectGraph.h"
#import "PageClient.h"
#import "ProvisionalPageProxy.h"
#import "QuickLookThumbnailLoader.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "ResourceLoadDelegate.h"
#import "SafeBrowsingWarning.h"
#import "UIDelegate.h"
#import "VersionChecks.h"
#import "VideoFullscreenManagerProxy.h"
#import "ViewGestureController.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKContentWorldInternal.h"
#import "WKErrorInternal.h"
#import "WKFindConfiguration.h"
#import "WKFindResultInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKLayoutMode.h"
#import "WKNSData.h"
#import "WKNSURLExtras.h"
#import "WKNavigationDelegate.h"
#import "WKNavigationInternal.h"
#import "WKPDFConfiguration.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKSafeBrowsingWarning.h"
#import "WKSharedAPICast.h"
#import "WKSnapshotConfiguration.h"
#import "WKUIDelegate.h"
#import "WKUIDelegatePrivate.h"
#import "WKUserContentControllerInternal.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProvider.h"
#import "WKWebViewPrivateForTestingIOS.h"
#import "WKWebViewIOS.h"
#import "WKWebViewMac.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebBackForwardList.h"
#import "WebCertificateInfo.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebURLSchemeHandlerCocoa.h"
#import "WebViewImpl.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKDiagnosticLoggingDelegate.h"
#import "_WKFindDelegate.h"
#import "_WKFrameHandleInternal.h"
#import "_WKFrameTreeNodeInternal.h"
#import "_WKFullscreenDelegate.h"
#import "_WKHitTestResultInternal.h"
#import "_WKInputDelegate.h"
#import "_WKInspectorDelegate.h"
#import "_WKInspectorInternal.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKSessionStateInternal.h"
#import "_WKTextInputContextInternal.h"
#import "_WKTextManipulationConfiguration.h"
#import "_WKTextManipulationDelegate.h"
#import "_WKTextManipulationExclusionRule.h"
#import "_WKTextManipulationItem.h"
#import "_WKTextManipulationToken.h"
#import "_WKVisitedLinkStoreInternal.h"
#import "_WKWebsitePoliciesInternal.h"
#import <WebCore/AttributedString.h>
#import <WebCore/ElementContext.h>
#import <WebCore/JSDOMBinding.h>
#import <WebCore/JSDOMExceptionHandling.h>
#import <WebCore/LegacySchemeRegistry.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/Settings.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/StringUtilities.h>
#import <WebCore/TextManipulationController.h>
#import <WebCore/ViewportArguments.h>
#import <WebCore/WebViewVisualIdentificationOverlay.h>
#import <WebCore/WritingMode.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/MathExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Optional.h>
#import <wtf/RetainPtr.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/TextStream.h>

#if ENABLE(APPLICATION_MANIFEST)
#import "_WKApplicationManifestInternal.h"
#endif

#if ENABLE(DATA_DETECTION)
#import "WKDataDetectorTypesInternal.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import "WKScrollView.h"
#import "WKWebViewContentProviderRegistry.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIApplication.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/cocoa/Entitlements.h>

#define RELEASE_LOG_IF_ALLOWED(...) RELEASE_LOG_IF(_page && _page->isAlwaysOnLoggingAllowed(), ViewState, __VA_ARGS__)
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
#import "AppKitSPI.h"
#import "WKContentViewMac.h"
#import "WKTextFinderClient.h"
#import "WKViewInternal.h"
#import <WebCore/ColorMac.h>
#endif

#if PLATFORM(IOS_FAMILY)
#if PLATFORM(WATCHOS)
static const BOOL defaultAllowsViewportShrinkToFit = YES;
static const BOOL defaultFastClickingEnabled = YES;
#else
static const BOOL defaultAllowsViewportShrinkToFit = NO;
static const BOOL defaultFastClickingEnabled = NO;
#endif

static const uint32_t firstSDKVersionWithLinkPreviewEnabledByDefault = 0xA0000;
#endif // PLATFORM(IOS_FAMILY)

static HashMap<WebKit::WebPageProxy*, __unsafe_unretained WKWebView *>& pageToViewMap()
{
    static NeverDestroyed<HashMap<WebKit::WebPageProxy*, __unsafe_unretained WKWebView *>> map;
    return map;
}

WKWebView* fromWebPageProxy(WebKit::WebPageProxy& page)
{
    return pageToViewMap().get(&page);
}

@implementation WKWebView

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
    static bool shouldAllowPictureInPictureMediaPlayback = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_9_0;
    return shouldAllowPictureInPictureMediaPlayback;
}

static bool shouldAllowSettingAnyXHRHeaderFromFileURLs()
{
    static bool shouldAllowSettingAnyXHRHeaderFromFileURLs = (WebCore::IOSApplication::isCardiogram() || WebCore::IOSApplication::isNike()) && !linkedOnOrAfter(WebKit::SDKVersion::FirstThatDisallowsSettingAnyXHRHeaderFromFileURLs);
    return shouldAllowSettingAnyXHRHeaderFromFileURLs;
}

#endif // PLATFORM(IOS_FAMILY)

static bool shouldRequireUserGestureToLoadVideo()
{
#if PLATFORM(IOS_FAMILY)
    static bool shouldRequireUserGestureToLoadVideo = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_10_0;
    return shouldRequireUserGestureToLoadVideo;
#else
    return false;
#endif
}

static bool shouldRestrictBaseURLSchemes()
{
    static bool shouldRestrictBaseURLSchemes = linkedOnOrAfter(WebKit::SDKVersion::FirstThatRestrictsBaseURLSchemes);
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

static void validate(WKWebViewConfiguration *configuration)
{
    if (!configuration.processPool)
        [NSException raise:NSInvalidArgumentException format:@"configuration.processPool is nil"];
    
    if (!configuration.preferences)
        [NSException raise:NSInvalidArgumentException format:@"configuration.preferences is nil"];
    
    if (!configuration.userContentController)
        [NSException raise:NSInvalidArgumentException format:@"configuration.userContentController is nil"];
    
    if (!configuration.websiteDataStore)
        [NSException raise:NSInvalidArgumentException format:@"configuration.websiteDataStore is nil"];
    
    if (!configuration._visitedLinkStore)
        [NSException raise:NSInvalidArgumentException format:@"configuration._visitedLinkStore is nil"];
    
#if PLATFORM(IOS_FAMILY)
    if (!configuration._contentProviderRegistry)
        [NSException raise:NSInvalidArgumentException format:@"configuration._contentProviderRegistry is nil"];
#endif
}

#if PLATFORM(IOS_FAMILY)
static void hardwareKeyboardAvailabilityChangedCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT(observer);
    WKWebView *webView = (__bridge WKWebView *)observer;
    [webView->_contentView _hardwareKeyboardAvailabilityChanged];
    webView._page->hardwareKeyboardAvailabilityChanged(GSEventIsHardwareKeyboardAttached());
}
#endif

- (void)_initializeWithConfiguration:(WKWebViewConfiguration *)configuration
{
    if (!configuration)
        [NSException raise:NSInvalidArgumentException format:@"Configuration cannot be nil"];

    _configuration = adoptNS([configuration copy]);

    if (WKWebView *relatedWebView = [_configuration _relatedWebView]) {
        WKProcessPool *processPool = [_configuration processPool];
        WKProcessPool *relatedWebViewProcessPool = [relatedWebView->_configuration processPool];
        if (processPool && processPool != relatedWebViewProcessPool)
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has process pool %@ but configuration specifies a different process pool %@", relatedWebView, relatedWebViewProcessPool, configuration.processPool];
        if ([relatedWebView->_configuration websiteDataStore] != [_configuration websiteDataStore] && linkedOnOrAfter(WebKit::SDKVersion::FirstWithExceptionsForRelatedWebViewsUsingDifferentDataStores, WebKit::AssumeSafariIsAlwaysLinkedOnAfter::No))
            [NSException raise:NSInvalidArgumentException format:@"Related web view %@ has data store %@ but configuration specifies a different data store %@", relatedWebView, [relatedWebView->_configuration websiteDataStore], [_configuration websiteDataStore]];

        [_configuration setProcessPool:relatedWebViewProcessPool];
    }

    validate(_configuration.get());

    WebKit::WebProcessPool& processPool = *[_configuration processPool]->_processPool;

    auto pageConfiguration = [configuration copyPageConfiguration];

    pageConfiguration->setProcessPool(&processPool);
    
    [self _setupPageConfiguration:pageConfiguration];

    _usePlatformFindUI = YES;

#if PLATFORM(IOS_FAMILY)
    _avoidsUnsafeArea = YES;
    _obscuredInsetEdgesAffectedBySafeArea = UIRectEdgeTop | UIRectEdgeLeft | UIRectEdgeRight;
    _viewportMetaTagWidth = WebCore::ViewportArguments::ValueAuto;
    _initialScaleFactor = 1;
    _allowsViewportShrinkToFit = defaultAllowsViewportShrinkToFit;

    static uint32_t programSDKVersion = dyld_get_program_sdk_version();
    _allowsLinkPreview = programSDKVersion >= firstSDKVersionWithLinkPreviewEnabledByDefault;

    auto fastClickingEnabled = []() {
        if (NSNumber *enabledValue = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitFastClickingDisabled"])
            return enabledValue.boolValue;
        return defaultFastClickingEnabled;
    };

    _fastClickingIsDisabled = fastClickingEnabled();
    _dragInteractionPolicy = _WKDragInteractionPolicyDefault;

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:self.bounds processPool:processPool configuration:pageConfiguration.copyRef() webView:self]);
    _page = [_contentView page];
    [[_configuration _contentProviderRegistry] addPage:*_page];

    [self _setupScrollAndContentViews];
    if (!self.opaque || !pageConfiguration->drawsBackground())
        [self _setOpaqueInternal:NO];
    else
        [self _updateScrollViewBackground];

    [self _frameOrBoundsChanged];
    [self _registerForNotifications];

    _page->contentSizeCategoryDidChange([self _contentSizeCategory]);

    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge const void *)(self), hardwareKeyboardAvailabilityChangedCallback, (CFStringRef)[NSString stringWithUTF8String:kGSEventHardwareKeyboardAvailabilityChangedNotification], nullptr, CFNotificationSuspensionBehaviorCoalesce);
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(META_VIEWPORT)
    _page->setForceAlwaysUserScalable([_configuration ignoresViewportScaleLimits]);
#endif

#if PLATFORM(MAC)
    _impl = makeUnique<WebKit::WebViewImpl>(self, self, processPool, pageConfiguration.copyRef());
    _page = &_impl->page();

    if (_impl->isUsingUISideCompositing())
        _contentView = adoptNS([[WKContentView alloc] initWithFrame:self.bounds page:_page.copyRef()]);

    _impl->setAutomaticallyAdjustsContentInsets(true);
    _impl->setRequiresUserActionForEditingControlsManager([configuration _requiresUserActionForEditingControlsManager]);

    [self _setupScrollAndContentViews];
#endif

    if (NSString *applicationNameForUserAgent = configuration.applicationNameForUserAgent)
        _page->setApplicationNameForUserAgent(applicationNameForUserAgent);

    _page->setApplicationNameForDesktopUserAgent(configuration._applicationNameForDesktopUserAgent);

    _navigationState = makeUnique<WebKit::NavigationState>(self);
    _page->setNavigationClient(_navigationState->createNavigationClient());

    _uiDelegate = makeUnique<WebKit::UIDelegate>(self);
    _page->setFindClient(makeUnique<WebKit::FindClient>(self));
    _page->setDiagnosticLoggingClient(makeUnique<WebKit::DiagnosticLoggingClient>(self));

    _iconLoadingDelegate = makeUnique<WebKit::IconLoadingDelegate>(self);
    _resourceLoadDelegate = makeUnique<WebKit::ResourceLoadDelegate>(self);
    _inspectorDelegate = makeUnique<WebKit::InspectorDelegate>(self);

    for (auto& pair : pageConfiguration->urlSchemeHandlers())
        _page->setURLSchemeHandlerForScheme(WebKit::WebURLSchemeHandlerCocoa::create(static_cast<WebKit::WebURLSchemeHandlerCocoa&>(pair.value.get()).apiHandler()), pair.key);

    pageToViewMap().add(_page.get(), self);

    [WebViewVisualIdentificationOverlay installForWebViewIfNeeded:self kind:@"WKWebView" deprecated:NO];

#if PLATFORM(IOS_FAMILY)
    auto timeNow = MonotonicTime::now();
    _timeOfRequestForVisibleContentRectUpdate = timeNow;
    _timeOfLastVisibleContentRectUpdate = timeNow;
    _timeOfFirstVisibleContentRectUpdateWithPendingCommit = timeNow;
#endif // PLATFORM(IOS_FAMILY)
}

- (void)_setupPageConfiguration:(Ref<API::PageConfiguration>&)pageConfiguration
{
    pageConfiguration->setPreferences([_configuration preferences]->_preferences.get());
    if (WKWebView *relatedWebView = [_configuration _relatedWebView])
        pageConfiguration->setRelatedPage(relatedWebView->_page.get());

    pageConfiguration->setUserContentController([_configuration userContentController]->_userContentControllerProxy.get());
    pageConfiguration->setVisitedLinkStore([_configuration _visitedLinkStore]->_visitedLinkStore.get());
    pageConfiguration->setWebsiteDataStore([_configuration websiteDataStore]->_websiteDataStore.get());
    pageConfiguration->setDefaultWebsitePolicies([_configuration defaultWebpagePreferences]->_websitePolicies.get());

#if PLATFORM(MAC)
    if (auto pageGroup = WebKit::toImpl([_configuration _pageGroup]))
        pageConfiguration->setPageGroup(pageGroup);
    else
#endif
    {
        NSString *groupIdentifier = [_configuration _groupIdentifier];
        if (groupIdentifier.length)
            pageConfiguration->setPageGroup(WebKit::WebPageGroup::create(groupIdentifier).ptr());
    }

    pageConfiguration->setAdditionalSupportedImageTypes(makeVector<String>([_configuration _additionalSupportedImageTypes]));

    pageConfiguration->preferences()->setSuppressesIncrementalRendering(!![_configuration suppressesIncrementalRendering]);

    pageConfiguration->preferences()->setShouldRespectImageOrientation(!![_configuration _respectsImageOrientation]);
#if !PLATFORM(MAC)
    // FIXME: Expose WKPreferences._shouldPrintBackgrounds on iOS, adopt it in all iOS clients, and remove this and WKWebViewConfiguration._printsBackgrounds.
    pageConfiguration->preferences()->setShouldPrintBackgrounds(!![_configuration _printsBackgrounds]);
#endif
    pageConfiguration->preferences()->setIncrementalRenderingSuppressionTimeout([_configuration _incrementalRenderingSuppressionTimeout]);
    pageConfiguration->preferences()->setJavaScriptMarkupEnabled(!![_configuration _allowsJavaScriptMarkup]);
    pageConfiguration->preferences()->setShouldConvertPositionStyleOnCopy(!![_configuration _convertsPositionStyleOnCopy]);
    pageConfiguration->preferences()->setHTTPEquivEnabled(!![_configuration _allowsMetaRefresh]);
    pageConfiguration->preferences()->setAllowUniversalAccessFromFileURLs(!![_configuration _allowUniversalAccessFromFileURLs]);
    pageConfiguration->preferences()->setAllowTopNavigationToDataURLs(!![_configuration _allowTopNavigationToDataURLs]);
    pageConfiguration->setWaitsForPaintAfterViewDidMoveToWindow([_configuration _waitsForPaintAfterViewDidMoveToWindow]);
    pageConfiguration->setDrawsBackground([_configuration _drawsBackground]);
    pageConfiguration->setControlledByAutomation([_configuration _isControlledByAutomation]);
    pageConfiguration->preferences()->setIncompleteImageBorderEnabled(!![_configuration _incompleteImageBorderEnabled]);
    pageConfiguration->preferences()->setShouldDeferAsynchronousScriptsUntilAfterDocumentLoadOrFirstPaint(!![_configuration _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad]);
    pageConfiguration->preferences()->setShouldRestrictBaseURLSchemes(shouldRestrictBaseURLSchemes());

#if PLATFORM(MAC)
    pageConfiguration->preferences()->setShowsURLsInToolTipsEnabled(!![_configuration _showsURLsInToolTips]);
    pageConfiguration->preferences()->setServiceControlsEnabled(!![_configuration _serviceControlsEnabled]);
    pageConfiguration->preferences()->setImageControlsEnabled(!![_configuration _imageControlsEnabled]);

    pageConfiguration->preferences()->setUserInterfaceDirectionPolicy(convertUserInterfaceDirectionPolicy([_configuration userInterfaceDirectionPolicy]));
    // We are in the View's initialization routine, so our client hasn't had time to set our user interface direction.
    // Therefore, according to the docs[1], "this property contains the value reported by the app's userInterfaceLayoutDirection property."
    // [1] http://developer.apple.com/library/mac/documentation/Cocoa/Reference/ApplicationKit/Classes/NSView_Class/index.html#//apple_ref/doc/uid/20000014-SW222
    pageConfiguration->preferences()->setSystemLayoutDirection(convertSystemLayoutDirection(self.userInterfaceLayoutDirection));
#endif

#if PLATFORM(IOS_FAMILY)
    pageConfiguration->preferences()->setAllowsInlineMediaPlayback(!![_configuration allowsInlineMediaPlayback]);
    pageConfiguration->preferences()->setAllowsInlineMediaPlaybackAfterFullscreen(!![_configuration _allowsInlineMediaPlaybackAfterFullscreen]);
    pageConfiguration->preferences()->setInlineMediaPlaybackRequiresPlaysInlineAttribute(!![_configuration _inlineMediaPlaybackRequiresPlaysInlineAttribute]);
    pageConfiguration->preferences()->setAllowsPictureInPictureMediaPlayback(!![_configuration allowsPictureInPictureMediaPlayback] && shouldAllowPictureInPictureMediaPlayback());
    pageConfiguration->preferences()->setUserInterfaceDirectionPolicy(static_cast<uint32_t>(WebCore::UserInterfaceDirectionPolicy::Content));
    pageConfiguration->preferences()->setSystemLayoutDirection(static_cast<uint32_t>(WebCore::TextDirection::LTR));
    pageConfiguration->preferences()->setAllowSettingAnyXHRHeaderFromFileURLs(shouldAllowSettingAnyXHRHeaderFromFileURLs());
    pageConfiguration->preferences()->setShouldDecidePolicyBeforeLoadingQuickLookPreview(!![_configuration _shouldDecidePolicyBeforeLoadingQuickLookPreview]);
#if ENABLE(DEVICE_ORIENTATION)
    pageConfiguration->preferences()->setDeviceOrientationPermissionAPIEnabled(linkedOnOrAfter(WebKit::SDKVersion::FirstWithDeviceOrientationAndMotionPermissionAPI));
#endif
#if USE(SYSTEM_PREVIEW)
    pageConfiguration->preferences()->setSystemPreviewEnabled(!![_configuration _systemPreviewEnabled]);
#endif
#endif // PLATFORM(IOS_FAMILY)

    WKAudiovisualMediaTypes mediaTypesRequiringUserGesture = [_configuration mediaTypesRequiringUserActionForPlayback];
    pageConfiguration->preferences()->setRequiresUserGestureForVideoPlayback((mediaTypesRequiringUserGesture & WKAudiovisualMediaTypeVideo) == WKAudiovisualMediaTypeVideo);
    pageConfiguration->preferences()->setRequiresUserGestureForAudioPlayback(((mediaTypesRequiringUserGesture & WKAudiovisualMediaTypeAudio) == WKAudiovisualMediaTypeAudio));
    pageConfiguration->preferences()->setRequiresUserGestureToLoadVideo(shouldRequireUserGestureToLoadVideo());
    pageConfiguration->preferences()->setMainContentUserGestureOverrideEnabled(!![_configuration _mainContentUserGestureOverrideEnabled]);
    pageConfiguration->preferences()->setInvisibleAutoplayNotPermitted(!![_configuration _invisibleAutoplayNotPermitted]);
    pageConfiguration->preferences()->setMediaDataLoadsAutomatically(!![_configuration _mediaDataLoadsAutomatically]);
    pageConfiguration->preferences()->setAttachmentElementEnabled(!![_configuration _attachmentElementEnabled]);

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    pageConfiguration->preferences()->setDataDetectorTypes(static_cast<uint32_t>(fromWKDataDetectorTypes([_configuration dataDetectorTypes])));
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    pageConfiguration->preferences()->setAllowsAirPlayForMediaPlayback(!![_configuration allowsAirPlayForMediaPlayback]);
#endif

#if ENABLE(APPLE_PAY)
    pageConfiguration->preferences()->setApplePayEnabled(!![_configuration _applePayEnabled]);
#endif

    pageConfiguration->preferences()->setNeedsStorageAccessFromFileURLsQuirk(!![_configuration _needsStorageAccessFromFileURLsQuirk]);
    pageConfiguration->preferences()->setMediaContentTypesRequiringHardwareSupport(String([_configuration _mediaContentTypesRequiringHardwareSupport]));
    pageConfiguration->preferences()->setAllowMediaContentTypesRequiringHardwareSupportAsFallback(!![_configuration _allowMediaContentTypesRequiringHardwareSupportAsFallback]);

    pageConfiguration->preferences()->setColorFilterEnabled(!![_configuration _colorFilterEnabled]);

    pageConfiguration->preferences()->setEditableImagesEnabled(!![_configuration _editableImagesEnabled]);
    pageConfiguration->preferences()->setUndoManagerAPIEnabled(!![_configuration _undoManagerAPIEnabled]);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    pageConfiguration->preferences()->setLegacyEncryptedMediaAPIEnabled(!![_configuration _legacyEncryptedMediaAPIEnabled]);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(SERVICE_WORKER)
    if ((!WTF::processHasEntitlement("com.apple.developer.WebKit.ServiceWorkers") || !![_configuration preferences]._serviceWorkerEntitlementDisabledForTesting) && ![_configuration limitsNavigationsToAppBoundDomains])
        pageConfiguration->preferences()->setServiceWorkersEnabled(false);
    pageConfiguration->preferences()->setServiceWorkerEntitlementDisabledForTesting(!![_configuration preferences]._serviceWorkerEntitlementDisabledForTesting);
#endif

    if (!linkedOnOrAfter(WebKit::SDKVersion::FirstWhereSiteSpecificQuirksAreEnabledByDefault))
        pageConfiguration->preferences()->setNeedsSiteSpecificQuirks(false);
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
}

- (void)dealloc
{
#if PLATFORM(MAC)
    [_textFinderClient willDestroyView:self];
#endif

#if PLATFORM(IOS_FAMILY)
    [_contentView _webViewDestroyed];

    if (_page && _remoteObjectRegistry)
        _page->process().processPool().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->identifier());
#endif

    if (_page)
        _page->close();

#if PLATFORM(IOS_FAMILY)
    [_remoteObjectRegistry _invalidate];
    [[_configuration _contentProviderRegistry] removePage:*_page];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_scrollView setInternalDelegate:nil];

    CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge const void *)(self), (CFStringRef)[NSString stringWithUTF8String:kGSEventHardwareKeyboardAvailabilityChangedNotification], nullptr);
#endif

    if (_page)
        pageToViewMap().remove(_page.get());

    [super dealloc];
}

- (id)valueForUndefinedKey:(NSString *)key
{
    if ([key isEqualToString:@"serverTrust"])
        return (__bridge id)[self serverTrust];

    return [super valueForUndefinedKey:key];
}

#pragma mark - macOS/iOS API

- (WKWebViewConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (WKBackForwardList *)backForwardList
{
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
    return wrapper(_page->loadRequest(request));
}

- (WKNavigation *)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL
{
    if (![URL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", URL];

    if (![readAccessURL isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", readAccessURL];

    return wrapper(_page->loadFile(URL.absoluteString, readAccessURL.absoluteString));
}

- (WKNavigation *)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL
{
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];

    return [self loadData:data MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:baseURL];
}

- (WKNavigation *)loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL
{
    return wrapper(_page->loadData({ static_cast<const uint8_t*>(data.bytes), data.length }, MIMEType, characterEncodingName, baseURL.absoluteString));
}

- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    return wrapper(_page->goToBackForwardItem(item._item));
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
#if HAVE(SEC_TRUST_SERIALIZATION)
    auto certificateInfo = _page->pageLoadState().certificateInfo();
    if (!certificateInfo)
        return nil;

    return certificateInfo->certificateInfo().trust();
#else
    return nil;
#endif
}

- (BOOL)canGoBack
{
    return _page->pageLoadState().canGoBack();
}

- (BOOL)canGoForward
{
    return _page->pageLoadState().canGoForward();
}

- (WKNavigation *)goBack
{
    if (self._safeBrowsingWarning)
        return [self reload];
    return wrapper(_page->goBack());
}

- (WKNavigation *)goForward
{
    return wrapper(_page->goForward());
}

- (WKNavigation *)reload
{
    OptionSet<WebCore::ReloadOption> reloadOptions;
    if (linkedOnOrAfter(WebKit::SDKVersion::FirstWithExpiredOnlyReloadBehavior))
        reloadOptions.add(WebCore::ReloadOption::ExpiredOnly);

    return wrapper(_page->reload(reloadOptions));
}

- (WKNavigation *)reloadFromOrigin
{
    return wrapper(_page->reload(WebCore::ReloadOption::FromOrigin));
}

- (void)stopLoading
{
    _page->stopLoading();
}

static WKErrorCode callbackErrorCode(WebKit::CallbackBase::Error error)
{
    switch (error) {
    case WebKit::CallbackBase::Error::None:
        ASSERT_NOT_REACHED();
        return WKErrorUnknown;

    case WebKit::CallbackBase::Error::Unknown:
        return WKErrorUnknown;

    case WebKit::CallbackBase::Error::ProcessExited:
        return WKErrorWebContentProcessTerminated;

    case WebKit::CallbackBase::Error::OwnerWasInvalidated:
        return WKErrorWebViewInvalidated;
    }
}

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler
{
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:YES inFrame:nil inWorld:WKContentWorld.pageWorld completionHandler:completionHandler];
}

- (void)evaluateJavaScript:(NSString *)javaScriptString inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *))completionHandler
{
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:YES inFrame:nil inWorld:contentWorld completionHandler:completionHandler];
}

- (void)callAsyncJavaScript:(NSString *)javaScriptString arguments:(NSDictionary<NSString *, id> *)arguments inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:YES withSourceURL:nil withArguments:arguments forceUserGesture:YES inFrame:nil inWorld:contentWorld completionHandler:completionHandler];
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

- (void)_evaluateJavaScript:(NSString *)javaScriptString asAsyncFunction:(BOOL)asAsyncFunction withSourceURL:(NSURL *)sourceURL withArguments:(NSDictionary<NSString *, id> *)arguments forceUserGesture:(BOOL)forceUserGesture inFrame:(WKFrameInfo *)frame inWorld:(WKContentWorld *)world completionHandler:(void (^)(id, NSError *))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);

    Optional<WebCore::ArgumentWireBytesMap> argumentsMap;
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

        argumentsMap->set(key, serializedValue->internalRepresentation().toWireBytes());
    }

    if (errorMessage && handler) {
        RetainPtr<NSMutableDictionary> userInfo = adoptNS([[NSMutableDictionary alloc] init]);

        [userInfo setObject:localizedDescriptionForErrorCode(WKErrorJavaScriptExceptionOccurred) forKey:NSLocalizedDescriptionKey];
        [userInfo setObject:errorMessage forKey:_WKJavaScriptExceptionMessageErrorKey];

        auto error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorJavaScriptExceptionOccurred userInfo:userInfo.get()]);
        dispatch_async(dispatch_get_main_queue(), [handler, error] {
            auto rawHandler = (void (^)(id, NSError *))handler.get();
            rawHandler(nil, error.get());
        });

        return;
    }
    
    Optional<WebCore::FrameIdentifier> frameID;
    if (frame) {
        if (uint64_t identifier = frame._handle.frameID)
            frameID = makeObjectIdentifier<WebCore::FrameIdentifierType>(identifier);
    }

    _page->runJavaScriptInFrameInScriptWorld({ javaScriptString, sourceURL, !!asAsyncFunction, WTFMove(argumentsMap), !!forceUserGesture }, frameID, *world->_contentWorld.get(), [handler](API::SerializedScriptValue* serializedScriptValue, Optional<WebCore::ExceptionDetails> details, WebKit::ScriptValueCallback::Error errorCode) {
        if (!handler)
            return;

        if (errorCode != WebKit::ScriptValueCallback::Error::None) {
            auto error = createNSError(callbackErrorCode(errorCode));
            if (errorCode == WebKit::ScriptValueCallback::Error::OwnerWasInvalidated) {
                // The OwnerWasInvalidated callback is synchronous. We don't want to call the block from within it
                // because that can trigger re-entrancy bugs in WebKit.
                // FIXME: It would be even better if GenericCallback did this for us.
                dispatch_async(dispatch_get_main_queue(), [handler, error] {
                    auto rawHandler = (void (^)(id, NSError *))handler.get();
                    rawHandler(nil, error.get());
                });
                return;
            }

            auto rawHandler = (void (^)(id, NSError *))handler.get();
            rawHandler(nil, error.get());
            return;
        }

        auto rawHandler = (void (^)(id, NSError *))handler.get();
        if (details) {
            ASSERT(!serializedScriptValue);

            RetainPtr<NSMutableDictionary> userInfo = adoptNS([[NSMutableDictionary alloc] init]);

            [userInfo setObject:localizedDescriptionForErrorCode(WKErrorJavaScriptExceptionOccurred) forKey:NSLocalizedDescriptionKey];
            [userInfo setObject:static_cast<NSString *>(details->message) forKey:_WKJavaScriptExceptionMessageErrorKey];
            [userInfo setObject:@(details->lineNumber) forKey:_WKJavaScriptExceptionLineNumberErrorKey];
            [userInfo setObject:@(details->columnNumber) forKey:_WKJavaScriptExceptionColumnNumberErrorKey];

            if (!details->sourceURL.isEmpty())
                [userInfo setObject:[NSURL _web_URLWithWTFString:details->sourceURL] forKey:_WKJavaScriptExceptionSourceURLErrorKey];

            rawHandler(nil, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorJavaScriptExceptionOccurred userInfo:userInfo.get()]).get());
            return;
        }

        if (!serializedScriptValue) {
            rawHandler(nil, createNSError(WKErrorJavaScriptResultTypeIsUnsupported).get());
            return;
        }

        id body = API::SerializedScriptValue::deserialize(serializedScriptValue->internalRepresentation(), 0);
        rawHandler(body, nil);
    });
}

- (void)takeSnapshotWithConfiguration:(WKSnapshotConfiguration *)snapshotConfiguration completionHandler:(void(^)(CocoaImage *, NSError *))completionHandler
{
    CGRect rectInViewCoordinates = snapshotConfiguration && !CGRectIsNull(snapshotConfiguration.rect) ? snapshotConfiguration.rect : self.bounds;
    CGFloat snapshotWidth;
    if (snapshotConfiguration)
        snapshotWidth = snapshotConfiguration.snapshotWidth.doubleValue ?: rectInViewCoordinates.size.width;
    else
        snapshotWidth = self.bounds.size.width;

    auto handler = makeBlockPtr(completionHandler);

#if USE(APPKIT)
    CGFloat imageScale = snapshotWidth / rectInViewCoordinates.size.width;
    CGFloat imageHeight = imageScale * rectInViewCoordinates.size.height;

    // Need to scale by device scale factor or the image will be distorted.
    CGFloat deviceScale = _page->deviceScaleFactor();
    WebCore::IntSize bitmapSize(snapshotWidth, imageHeight);
    bitmapSize.scale(deviceScale, deviceScale);

    // Software snapshot will not capture elements rendered with hardware acceleration (WebGL, video, etc).
    // This code doesn't consider snapshotConfiguration.afterScreenUpdates since the software snapshot always
    // contains recent updates. If we ever have a UI-side snapshot mechanism on macOS, we will need to factor
    // in snapshotConfiguration.afterScreenUpdates at that time.
    _page->takeSnapshot(WebCore::enclosingIntRect(rectInViewCoordinates), bitmapSize, WebKit::SnapshotOptionsInViewCoordinates, [handler, snapshotWidth, imageHeight](const WebKit::ShareableBitmap::Handle& imageHandle, WebKit::CallbackBase::Error errorCode) {
        if (errorCode != WebKit::ScriptValueCallback::Error::None) {
            auto error = createNSError(callbackErrorCode(errorCode));
            handler(nil, error.get());
            return;
        }

        auto bitmap = WebKit::ShareableBitmap::create(imageHandle, WebKit::SharedMemory::Protection::ReadOnly);
        RetainPtr<CGImageRef> cgImage = bitmap ? bitmap->makeCGImage() : nullptr;
        auto image = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:NSMakeSize(snapshotWidth, imageHeight)]);
        handler(image.get(), nil);
    });
#else
    CGFloat deviceScale = _page->deviceScaleFactor();
    RetainPtr<WKWebView> strongSelf = self;
    auto callSnapshotRect = [strongSelf, rectInViewCoordinates, snapshotWidth, deviceScale, handler] {
        [strongSelf _snapshotRect:rectInViewCoordinates intoImageOfWidth:(snapshotWidth * deviceScale) completionHandler:[strongSelf, handler, deviceScale](CGImageRef snapshotImage) {
            RetainPtr<NSError> error;
            RetainPtr<UIImage> image;
            
            if (!snapshotImage)
                error = createNSError(WKErrorUnknown);
            else
                image = adoptNS([[UIImage alloc] initWithCGImage:snapshotImage scale:deviceScale orientation:UIImageOrientationUp]);
            
            handler(image.get(), error.get());
        }];
    };

    if ((snapshotConfiguration && !snapshotConfiguration.afterScreenUpdates) || !linkedOnOrAfter(WebKit::SDKVersion::FirstWithSnapshotAfterScreenUpdates)) {
        callSnapshotRect();
        return;
    }

    _page->callAfterNextPresentationUpdate([callSnapshotRect = WTFMove(callSnapshotRect), handler](WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None) {
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }
        callSnapshotRect();
    });
#endif
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
#if PLATFORM(MAC)
    _impl->setAllowsBackForwardNavigationGestures(allowsBackForwardNavigationGestures);
#elif PLATFORM(IOS_FAMILY)
    if (_allowsBackForwardNavigationGestures == allowsBackForwardNavigationGestures)
        return;

    _allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;

    if (allowsBackForwardNavigationGestures && !_gestureController) {
        _gestureController = makeUnique<WebKit::ViewGestureController>(*_page);
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
    _page->setPageZoomFactor(pageZoom);
}

- (CGFloat)pageZoom
{
    return _page->pageZoomFactor();
}

inline WebKit::FindOptions toFindOptions(WKFindConfiguration *configuration)
{
    unsigned findOptions = 0;

    if (!configuration.caseSensitive)
        findOptions |= WebKit::FindOptionsCaseInsensitive;
    if (configuration.backwards)
        findOptions |= WebKit::FindOptionsBackwards;
    if (configuration.wraps)
        findOptions |= WebKit::FindOptionsWrapAround;

    return static_cast<WebKit::FindOptions>(findOptions);
}

- (void)findString:(NSString *)string withConfiguration:(WKFindConfiguration *)configuration completionHandler:(void (^)(WKFindResult *result))completionHandler
{
    if (!string.length) {
        completionHandler([[[WKFindResult alloc] _initWithMatchFound:NO] autorelease]);
        return;
    }

    _page->findString(string, toFindOptions(configuration), 1, [handler = makeBlockPtr(completionHandler)](bool found, WebKit::CallbackBase::Error error) {
        handler([[[WKFindResult alloc] _initWithMatchFound:(error == WebKit::CallbackBase::Error::None && found)] autorelease]);
    });
}

+ (BOOL)handlesURLScheme:(NSString *)urlScheme
{
    return WebCore::LegacySchemeRegistry::isBuiltinScheme(urlScheme);
}

- (void)setMediaType:(NSString *)mediaStyle
{
    _page->setOverriddenMediaType(mediaStyle);
}

- (NSString *)mediaType
{
    return _page->overriddenMediaType().isNull() ? nil : (NSString *)_page->overriddenMediaType();
}

#pragma mark - iOS API

#if PLATFORM(IOS_FAMILY)
- (UIScrollView *)scrollView
{
    return _scrollView.get();
}
#endif // PLATFORM(IOS_FAMILY)

#pragma mark - macOS API

#if PLATFORM(MAC)

- (void)setAllowsMagnification:(BOOL)allowsMagnification
{
    _impl->setAllowsMagnification(allowsMagnification);
}

- (BOOL)allowsMagnification
{
    return _impl->allowsMagnification();
}

- (void)setMagnification:(double)magnification centeredAtPoint:(NSPoint)point
{
    _impl->setMagnification(magnification, NSPointToCGPoint(point));
}

- (void)setMagnification:(double)magnification
{
    _impl->setMagnification(magnification);
}

- (double)magnification
{
    return _impl->magnification();
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo
{
    if (auto webFrameProxy = _page->mainFrame())
        return _impl->printOperationWithPrintInfo(printInfo, *webFrameProxy);
    return nil;
}

#endif // PLATFORM(MAC)

#pragma mark - macOS/iOS internal

- (void)_showSafeBrowsingWarning:(const WebKit::SafeBrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    _safeBrowsingWarning = adoptNS([[WKSafeBrowsingWarning alloc] initWithFrame:self.bounds safeBrowsingWarning:warning completionHandler:[weakSelf = WeakObjCPtr<WKWebView>(self), completionHandler = WTFMove(completionHandler)] (auto&& result) mutable {
        completionHandler(WTFMove(result));
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;
        bool navigatesFrame = WTF::switchOn(result,
            [] (WebKit::ContinueUnsafeLoad continueUnsafeLoad) { return continueUnsafeLoad == WebKit::ContinueUnsafeLoad::Yes; },
            [] (const URL&) { return true; }
        );
        bool forMainFrameNavigation = [strongSelf->_safeBrowsingWarning forMainFrameNavigation];
        if (navigatesFrame && forMainFrameNavigation) {
            // The safe browsing warning will be hidden once the next page is shown.
            return;
        }
        if (!navigatesFrame && strongSelf->_safeBrowsingWarning && !forMainFrameNavigation) {
            strongSelf->_page->goBack();
            return;
        }
        [std::exchange(strongSelf->_safeBrowsingWarning, nullptr) removeFromSuperview];
    }]);
    [self addSubview:_safeBrowsingWarning.get()];
}

- (void)_clearSafeBrowsingWarning
{
    [std::exchange(_safeBrowsingWarning, nullptr) removeFromSuperview];
}

- (void)_clearSafeBrowsingWarningIfForMainFrameNavigation
{
    if ([_safeBrowsingWarning forMainFrameNavigation])
        [self _clearSafeBrowsingWarning];
}

- (void)_internalDoAfterNextPresentationUpdate:(void (^)(void))updateBlock withoutWaitingForPainting:(BOOL)withoutWaitingForPainting withoutWaitingForAnimatedResize:(BOOL)withoutWaitingForAnimatedResize
{
#if PLATFORM(IOS_FAMILY)
    if (![self usesStandardContentView]) {
        dispatch_async(dispatch_get_main_queue(), updateBlock);
        return;
    }
#endif

    if (withoutWaitingForPainting)
        _page->setShouldSkipWaitingForPaintAfterNextViewDidMoveToWindow(true);

    auto updateBlockCopy = makeBlockPtr(updateBlock);

    RetainPtr<WKWebView> strongSelf = self;
    _page->callAfterNextPresentationUpdate([updateBlockCopy, withoutWaitingForAnimatedResize, strongSelf](WebKit::CallbackBase::Error error) {
        if (!updateBlockCopy)
            return;

#if PLATFORM(IOS_FAMILY)
        if (!withoutWaitingForAnimatedResize && strongSelf->_dynamicViewportUpdateMode != WebKit::DynamicViewportUpdateMode::NotResizing) {
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

- (WKPageRef)_pageForTesting
{
    return toAPI(_page.get());
}

- (NakedPtr<WebKit::WebPageProxy>)_page
{
    return _page.get();
}

- (id <WKURLSchemeHandler>)urlSchemeHandlerForURLScheme:(NSString *)urlScheme
{
    auto* handler = static_cast<WebKit::WebURLSchemeHandlerCocoa*>(_page->urlSchemeHandlerForScheme(urlScheme));
    return handler ? handler->apiHandler() : nil;
}

- (Optional<BOOL>)_resolutionForShareSheetImmediateCompletionForTesting
{
    return _resolutionForShareSheetImmediateCompletionForTesting;
}

- (void)createPDFWithConfiguration:(WKPDFConfiguration *)pdfConfiguration completionHandler:(void (^)(NSData *pdfDocumentData, NSError *error))completionHandler
{
    WebCore::FrameIdentifier frameID;
    if (auto mainFrame = _page->mainFrame())
        frameID = mainFrame->frameID();
    else {
        completionHandler(nil, createNSError(WKErrorUnknown).get());
        return;
    }

    Optional<WebCore::FloatRect> floatRect;
    if (pdfConfiguration && !CGRectIsNull(pdfConfiguration.rect))
        floatRect = WebCore::FloatRect(pdfConfiguration.rect);

    auto handler = makeBlockPtr(completionHandler);
    _page->drawToPDF(frameID, floatRect, [retainedSelf = retainPtr(self), handler = WTFMove(handler)](const IPC::DataReference& pdfData, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None) {
            handler(nil, createNSError(WKErrorUnknown).get());
            return;
        }

        auto data = adoptCF(CFDataCreate(kCFAllocatorDefault, pdfData.data(), pdfData.size()));
        handler((NSData *)data.get(), nil);
    });
}

- (void)createWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);

    _page->getWebArchiveOfFrame(_page->mainFrame(), [handler](API::Data* data, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(NSData *, NSError *) = (void (^)(NSData *, NSError *))handler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            // FIXME: Pipe a proper error in from the WebPageProxy.
            completionHandlerBlock(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<int>(error) userInfo:nil]);
        } else
            completionHandlerBlock(wrapper(*data), nil);
    });
}

static NSDictionary *dictionaryRepresentationForEditorState(const WebKit::EditorState& state)
{
    if (state.isMissingPostLayoutData)
        return @{ @"post-layout-data" : @NO };

    auto& postLayoutData = state.postLayoutData();
    return @{
        @"post-layout-data" : @YES,
        @"bold": postLayoutData.typingAttributes & WebKit::AttributeBold ? @YES : @NO,
        @"italic": postLayoutData.typingAttributes & WebKit::AttributeItalics ? @YES : @NO,
        @"underline": postLayoutData.typingAttributes & WebKit::AttributeUnderline ? @YES : @NO,
        @"text-alignment": @(nsTextAlignment(static_cast<WebKit::TextAlignment>(postLayoutData.textAlignment))),
        @"text-color": (NSString *)postLayoutData.textColor.cssText()
    };
}

static NSTextAlignment nsTextAlignment(WebKit::TextAlignment alignment)
{
    switch (alignment) {
    case WebKit::NoAlignment:
        return NSTextAlignmentNatural;
    case WebKit::LeftAlignment:
        return NSTextAlignmentLeft;
    case WebKit::RightAlignment:
        return NSTextAlignmentRight;
    case WebKit::CenterAlignment:
        return NSTextAlignmentCenter;
    case WebKit::JustifiedAlignment:
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

    if (!editorState.isMissingPostLayoutData) {
#if PLATFORM(IOS_FAMILY)
        if (editorState.postLayoutData().atStartOfSentence)
            attributes |= _WKSelectionAttributeAtStartOfSentence;
#endif
    } else if (previousAttributes & _WKSelectionAttributeAtStartOfSentence)
        attributes |= _WKSelectionAttributeAtStartOfSentence;

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

@end

#pragma mark -

@implementation WKWebView (WKPrivate)

#pragma mark - macOS WKPrivate

#if PLATFORM(MAC)

#define WEBCORE_PRIVATE_COMMAND(command) - (void)_##command:(id)sender { _page->executeEditCommand(#command ## _s); }

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
    _page->executeEditCommand("strikethrough"_s);
}

- (void)_increaseListLevel:(id)sender
{
    _page->increaseListLevel();
}

- (void)_decreaseListLevel:(id)sender
{
    _page->decreaseListLevel();
}

- (void)_changeListType:(id)sender
{
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
    auto viewportSizeForViewportUnits = WebCore::IntSize(viewportSize);
    if (viewportSizeForViewportUnits.isEmpty())
        [NSException raise:NSInvalidArgumentException format:@"Viewport size should not be empty"];

    _page->setViewportSizeForCSSViewportUnits(viewportSizeForViewportUnits);
}

- (_WKInspector *)_inspector
{
    if (auto* inspector = _page->inspector())
        return wrapper(*inspector);
    return nil;
}

- (id <_WKInspectorDelegate>)_inspectorDelegate
{
    return _inspectorDelegate->delegate().autorelease();
}

- (void)_setInspectorDelegate:(id<_WKInspectorDelegate>)delegate
{
    _page->setInspectorClient(_inspectorDelegate->createInspectorClient());
    _inspectorDelegate->setDelegate(delegate);
}

- (_WKFrameHandle *)_mainFrame
{
    if (auto* frame = _page->mainFrame())
        return wrapper(API::FrameHandle::create(frame->frameID()));
    return nil;
}

- (BOOL)_negotiatedLegacyTLS
{
    return _page->pageLoadState().hasNegotiatedLegacyTLS();
}

- (void)_frames:(void (^)(_WKFrameTreeNode *))completionHandler
{
    _page->getAllFrames([completionHandler = makeBlockPtr(completionHandler), page = makeRef(*_page.get())] (WebKit::FrameTreeNodeData&& data) {
        _WKFrameTreeNode *node = [[wrapper(API::FrameTreeNode::create(WTFMove(data), page.get())) retain] autorelease];
        completionHandler(node);
    });
}

- (BOOL)_isEditable
{
    return _page && _page->isEditable();
}

- (void)_setEditable:(BOOL)editable
{
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

static RetainPtr<NSDictionary<NSString *, id>> createUserInfo(const Optional<WebCore::TextManipulationController::ManipulationTokenInfo>& info)
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

    return result;
}

- (void)_startTextManipulationsWithConfiguration:(_WKTextManipulationConfiguration *)configuration completion:(void(^)())completionHandler
{
    using ExclusionRule = WebCore::TextManipulationController::ExclusionRule;

    if (!_textManipulationDelegate || !_page) {
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

    _page->startTextManipulations(exclusionRules, [weakSelf = WeakObjCPtr<WKWebView>(self)] (const Vector<WebCore::TextManipulationController::ManipulationItem>& itemReferences) {
        if (!weakSelf)
            return;

        auto retainedSelf = weakSelf.get();
        auto delegate = [retainedSelf _textManipulationDelegate];
        if (!delegate)
            return;

        auto createWKItem = [] (const WebCore::TextManipulationController::ManipulationItem& item) {
            auto tokens = createNSArray(item.tokens, [] (auto& token) {
                auto wkToken = adoptNS([[_WKTextManipulationToken alloc] init]);
                [wkToken setIdentifier:String::number(token.identifier.toUInt64())];
                [wkToken setContent:token.content];
                [wkToken setExcluded:token.isExcluded];
                [wkToken setUserInfo:createUserInfo(token.info).get()];
                return wkToken;
            });
            return adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:String::number(item.identifier.toUInt64()) tokens:tokens.get()]);
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

static WebCore::TextManipulationController::ItemIdentifier coreTextManipulationItemIdentifierFromString(NSString* identifier)
{
    return makeObjectIdentifier<WebCore::TextManipulationController::ItemIdentifierType>(String(identifier).toUInt64());
}

static WebCore::TextManipulationController::TokenIdentifier coreTextManipulationTokenIdentifierFromString(NSString* identifier)
{
    return makeObjectIdentifier<WebCore::TextManipulationController::TokenIdentifierType>(String(identifier).toUInt64());
}

- (void)_completeTextManipulation:(_WKTextManipulationItem *)item completion:(void(^)(BOOL success))completionHandler
{
    if (!_page) {
        completionHandler(false);
        return;
    }

    auto itemID = coreTextManipulationItemIdentifierFromString(item.identifier);

    Vector<WebCore::TextManipulationController::ManipulationToken> tokens;
    for (_WKTextManipulationToken *wkToken in item.tokens)
        tokens.append(WebCore::TextManipulationController::ManipulationToken { coreTextManipulationTokenIdentifierFromString(wkToken.identifier), wkToken.content, WTF::nullopt });

    Vector<WebCore::TextManipulationController::ManipulationItem> coreItems;
    coreItems.reserveInitialCapacity(1);
    coreItems.uncheckedAppend(WebCore::TextManipulationController::ManipulationItem { itemID, WTFMove(tokens) });
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
            using Type = WebCore::TextManipulationController::ManipulationFailureType;
            switch (coreFailure.type) {
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
        ASSERT(coreTextManipulationItemIdentifierFromString(item.identifier) == coreFailure.identifier);
        return [NSError errorWithDomain:_WKTextManipulationItemErrorDomain code:errorCode userInfo:@{_WKTextManipulationItemErrorItemKey: item}];
    });
}

- (void)_completeTextManipulationForItems:(NSArray<_WKTextManipulationItem *> *)items completion:(void(^)(NSArray<NSError *> *errors))completionHandler
{
    if (!_page) {
        completionHandler(makeFailureSetForAllTextManipulationItems(items).get());
        return;
    }

    Vector<WebCore::TextManipulationController::ManipulationItem> coreItems;
    coreItems.reserveInitialCapacity(items.count);
    for (_WKTextManipulationItem *wkItem in items) {
        Vector<WebCore::TextManipulationController::ManipulationToken> coreTokens;
        coreTokens.reserveInitialCapacity(wkItem.tokens.count);
        for (_WKTextManipulationToken *wkToken in wkItem.tokens)
            coreTokens.uncheckedAppend(WebCore::TextManipulationController::ManipulationToken { coreTextManipulationTokenIdentifierFromString(wkToken.identifier), wkToken.content, WTF::nullopt });
        coreItems.uncheckedAppend(WebCore::TextManipulationController::ManipulationItem { coreTextManipulationItemIdentifierFromString(wkItem.identifier), WTFMove(coreTokens) });
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

- (void)_takeFindStringFromSelection:(id)sender
{
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
        _page->process().processPool().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->identifier(), [_remoteObjectRegistry remoteObjectRegistry]);
    }

    return _remoteObjectRegistry.get();
#endif
}

- (WKBrowsingContextHandle *)_handle
{
    return [[[WKBrowsingContextHandle alloc] _initWithPageProxy:*_page] autorelease];
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
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->updateMediaPlaybackControlsManager();
#endif
}

- (BOOL)_canTogglePictureInPicture
{
#if HAVE(TOUCH_BAR)
    return _impl->canTogglePictureInPicture();
#else
    return NO;
#endif
}

- (BOOL)_isPictureInPictureActive
{
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    return _impl->isPictureInPictureActive();
#else
    return NO;
#endif
}

- (void)_togglePictureInPicture
{
#if HAVE(TOUCH_BAR) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    _impl->togglePictureInPicture();
#endif
}

- (_WKMediaMutedState)_mediaMutedState
{
    return WebKit::toWKMediaMutedState(_page->mutedStateFlags());
}

- (void)_closeAllMediaPresentations
{
#if ENABLE(FULLSCREEN_API)
    if (auto videoFullscreenManager = _page->videoFullscreenManager()) {
        videoFullscreenManager->forEachSession([] (auto& model, auto& interface) {
            model.requestFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone);
        });
    }

    if (auto fullScreenManager = _page->fullScreenManager(); fullScreenManager && fullScreenManager->isFullScreen())
        fullScreenManager->close();
#endif
}

- (void)_stopMediaCapture
{
    _page->stopMediaCapture();
}

- (void)_stopAllMediaPlayback
{
    _page->stopAllMediaPlayback();
}

- (void)_suspendAllMediaPlayback
{
    _page->suspendAllMediaPlayback();
}

- (void)_resumeAllMediaPlayback
{
    _page->resumeAllMediaPlayback();
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
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    _page->loadAlternateHTML({ static_cast<const uint8_t*>(data.bytes), data.length }, "UTF-8"_s, baseURL, unreachableURL);
}

- (WKNavigation *)_loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL userData:(id)userData
{
    return wrapper(_page->loadData({ static_cast<const uint8_t*>(data.bytes), data.length }, MIMEType, characterEncodingName, baseURL.absoluteString, WebKit::ObjCObjectGraph::create(userData).ptr()));
}

- (WKNavigation *)_loadRequest:(NSURLRequest *)request shouldOpenExternalURLs:(BOOL)shouldOpenExternalURLs
{
    return wrapper(_page->loadRequest(request, shouldOpenExternalURLs ? WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow : WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow));
}

- (NSArray *)_certificateChain
{
    if (WebKit::WebFrameProxy* mainFrame = _page->mainFrame())
        return mainFrame->certificateInfo() ? (__bridge NSArray *)mainFrame->certificateInfo()->certificateInfo().certificateChain() : nil;

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

    return _page->processIdentifier();
}

- (pid_t)_provisionalWebProcessIdentifier
{
    if (![self _isValid])
        return 0;

    auto* provisionalPage = _page->provisionalPageProxy();
    if (!provisionalPage)
        return 0;

    return provisionalPage->process().processIdentifier();
}

- (BOOL)_webProcessIsResponsive
{
    return _page->process().isResponsive();
}

- (void)_killWebContentProcess
{
    if (![self _isValid])
        return;

    _page->process().terminate();
}

- (WKNavigation *)_reloadWithoutContentBlockers
{
    return wrapper(_page->reload(WebCore::ReloadOption::DisableContentBlockers));
}

- (WKNavigation *)_reloadExpiredOnly
{
    return wrapper(_page->reload(WebCore::ReloadOption::ExpiredOnly));
}

- (void)_killWebContentProcessAndResetState
{
    Ref<WebKit::WebProcessProxy> protectedProcessProxy(_page->process());
    protectedProcessProxy->requestTermination(WebKit::ProcessTerminationReason::RequestedByClient);

    if (auto* provisionalPageProxy = _page->provisionalPageProxy()) {
        Ref<WebKit::WebProcessProxy> protectedProcessProxy(provisionalPageProxy->process());
        protectedProcessProxy->requestTermination(WebKit::ProcessTerminationReason::RequestedByClient);
    }
}

- (void)_takePDFSnapshotWithConfiguration:(WKSnapshotConfiguration *)snapshotConfiguration completionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    WKPDFConfiguration *pdfConfiguration = nil;
    if (snapshotConfiguration) {
        pdfConfiguration = [[[WKPDFConfiguration alloc] init] autorelease];
        pdfConfiguration.rect = snapshotConfiguration.rect;
    }

    [self createPDFWithConfiguration:pdfConfiguration completionHandler:completionHandler];
}

- (NSData *)_sessionStateData
{
    // FIXME: This should not use the legacy session state encoder.
    return wrapper(WebKit::encodeLegacySessionState(_page->sessionState()));
}

- (_WKSessionState *)_sessionState
{
    return [[[_WKSessionState alloc] _initWithSessionState:_page->sessionState()] autorelease];
}

- (_WKSessionState *)_sessionStateWithFilter:(BOOL (^)(WKBackForwardListItem *item))filter
{
    WebKit::SessionState sessionState = _page->sessionState([filter](WebKit::WebBackForwardListItem& item) {
        if (!filter)
            return true;

        return (bool)filter(wrapper(item));
    });

    return [[[_WKSessionState alloc] _initWithSessionState:sessionState] autorelease];
}

- (void)_restoreFromSessionStateData:(NSData *)sessionStateData
{
    // FIXME: This should not use the legacy session state decoder.
    WebKit::SessionState sessionState;
    if (!WebKit::decodeLegacySessionState(static_cast<const uint8_t*>(sessionStateData.bytes), sessionStateData.length, sessionState))
        return;

    _page->restoreFromSessionState(WTFMove(sessionState), true);
}

- (WKNavigation *)_restoreSessionState:(_WKSessionState *)sessionState andNavigate:(BOOL)navigate
{
    return wrapper(_page->restoreFromSessionState(sessionState ? sessionState->_sessionState : WebKit::SessionState { }, navigate));
}

- (void)_close
{
    _page->close();
}

- (void)_tryClose
{
    _page->tryClose();
}

- (BOOL)_isClosed
{
    return _page->isClosed();
}

- (_WKAttachment *)_insertAttachmentWithFilename:(NSString *)filename contentType:(NSString *)contentType data:(NSData *)data options:(_WKAttachmentDisplayOptions *)options completion:(void(^)(BOOL success))completionHandler
{
    UNUSED_PARAM(options);
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    if (filename)
        [fileWrapper setPreferredFilename:filename];
    return [self _insertAttachmentWithFileWrapper:fileWrapper.get() contentType:contentType completion:completionHandler];
}

- (_WKAttachment *)_insertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType options:(_WKAttachmentDisplayOptions *)options completion:(void(^)(BOOL success))completionHandler
{
    UNUSED_PARAM(options);
    return [self _insertAttachmentWithFileWrapper:fileWrapper contentType:contentType completion:completionHandler];
}

- (_WKAttachment *)_insertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType completion:(void(^)(BOOL success))completionHandler
{
#if ENABLE(ATTACHMENT_ELEMENT)
    auto identifier = createCanonicalUUIDString();
    auto attachment = API::Attachment::create(identifier, *_page);
    attachment->setFileWrapperAndUpdateContentType(fileWrapper, contentType);
    _page->insertAttachment(attachment.copyRef(), [capturedHandler = makeBlockPtr(completionHandler)] (WebKit::CallbackBase::Error error) {
        if (capturedHandler)
            capturedHandler(error == WebKit::CallbackBase::Error::None);
    });
#if HAVE(QUICKLOOK_THUMBNAILING)
    _page->requestThumbnailWithFileWrapper(fileWrapper, identifier);
#endif
    return wrapper(attachment);
#else
    return nil;
#endif
}

- (_WKAttachment *)_attachmentForIdentifier:(NSString *)identifier
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (auto attachment = _page->attachmentForIdentifier(identifier))
        return wrapper(attachment);
#endif
    return nil;
}

- (void)_simulateDeviceOrientationChangeWithAlpha:(double)alpha beta:(double)beta gamma:(double)gamma
{
    _page->simulateDeviceOrientationChange(alpha, beta, gamma);
}

+ (BOOL)_handlesSafeBrowsing
{
    return true;
}

- (void)_showSafeBrowsingWarningWithTitle:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler
{
    [self _showSafeBrowsingWarningWithURL:nil title:title warning:warning detailsWithLinks:details completionHandler:^(BOOL continueUnsafeLoad, NSURL *url) {
        ASSERT(!url);
        completionHandler(continueUnsafeLoad);
    }];
}

- (void)_showSafeBrowsingWarningWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler
{
    [self _showSafeBrowsingWarningWithURL:nil title:title warning:warning detailsWithLinks:details completionHandler:^(BOOL continueUnsafeLoad, NSURL *url) {
        ASSERT(!url);
        completionHandler(continueUnsafeLoad);
    }];
}

- (void)_showSafeBrowsingWarningWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning detailsWithLinks:(NSAttributedString *)details completionHandler:(void(^)(BOOL, NSURL *))completionHandler
{
    auto safeBrowsingWarning = WebKit::SafeBrowsingWarning::create(url, title, warning, details);
    auto wrapper = [completionHandler = makeBlockPtr(completionHandler)] (Variant<WebKit::ContinueUnsafeLoad, URL>&& variant) {
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
    _impl->showSafeBrowsingWarning(safeBrowsingWarning, WTFMove(wrapper));
#else
    [self _showSafeBrowsingWarning:safeBrowsingWarning completionHandler:WTFMove(wrapper)];
#endif
}

+ (NSURL *)_confirmMalwareSentinel
{
    return WebKit::SafeBrowsingWarning::confirmMalwareSentinel();
}

+ (NSURL *)_visitUnsafeWebsiteSentinel
{
    return WebKit::SafeBrowsingWarning::visitUnsafeWebsiteSentinel();
}

- (void)_isJITEnabled:(void(^)(BOOL))completionHandler
{
    _page->isJITEnabled([completionHandler = makeBlockPtr(completionHandler)] (bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_evaluateJavaScriptWithoutUserGesture:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler
{
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:NO inFrame:nil inWorld:WKContentWorld.pageWorld completionHandler:completionHandler];
}

- (void)_callAsyncJavaScript:(NSString *)functionBody arguments:(NSDictionary<NSString *, id> *)arguments inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    [self _evaluateJavaScript:functionBody asAsyncFunction:YES withSourceURL:nil withArguments:arguments forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)_evaluateJavaScript:(NSString *)javaScriptString inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:nil withArguments:nil forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)_evaluateJavaScript:(NSString *)javaScriptString withSourceURL:(NSURL *)url inFrame:(WKFrameInfo *)frame inContentWorld:(WKContentWorld *)contentWorld completionHandler:(void (^)(id, NSError *error))completionHandler
{
    [self _evaluateJavaScript:javaScriptString asAsyncFunction:NO withSourceURL:url withArguments:nil forceUserGesture:YES inFrame:frame inWorld:contentWorld completionHandler:completionHandler];
}

- (void)_updateWebsitePolicies:(_WKWebsitePolicies *)websitePolicies
{
}

- (void)_updateWebpagePreferences:(WKWebpagePreferences *)webpagePreferences
{
    if (webpagePreferences._websiteDataStore)
        [NSException raise:NSInvalidArgumentException format:@"Updating WKWebsiteDataStore is only supported during decidePolicyForNavigationAction."];
    if (webpagePreferences._userContentController)
        [NSException raise:NSInvalidArgumentException format:@"Updating WKUserContentController is only supported during decidePolicyForNavigationAction."];
    auto data = webpagePreferences->_websitePolicies->data();
    _page->updateWebsitePolicies(WTFMove(data));
}

- (void)_notifyUserScripts
{
    _page->notifyUserScripts();
}

- (BOOL)_deferrableUserScriptsNeedNotification
{
    return _page->userScriptsNeedNotification();
}

- (BOOL)_allowsRemoteInspection
{
#if ENABLE(REMOTE_INSPECTOR)
    return _page->allowsRemoteInspection();
#else
    return NO;
#endif
}

- (void)_setAllowsRemoteInspection:(BOOL)allow
{
#if ENABLE(REMOTE_INSPECTOR)
    _page->setAllowsRemoteInspection(allow);
#endif
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
    _page->setAddsVisitedLinks(addsVisitedLinks);
}

- (BOOL)_networkRequestsInProgress
{
    return _page->pageLoadState().networkRequestsInProgress();
}

static inline OptionSet<WebCore::LayoutMilestone> layoutMilestones(_WKRenderingProgressEvents events)
{
    OptionSet<WebCore::LayoutMilestone> milestones;

    if (events & _WKRenderingProgressEventFirstLayout)
        milestones.add(WebCore::DidFirstLayout);

    if (events & _WKRenderingProgressEventFirstVisuallyNonEmptyLayout)
        milestones.add(WebCore::DidFirstVisuallyNonEmptyLayout);

    if (events & _WKRenderingProgressEventFirstPaintWithSignificantArea)
        milestones.add(WebCore::DidHitRelevantRepaintedObjectsAreaThreshold);

    if (events & _WKRenderingProgressEventReachedSessionRestorationRenderTreeSizeThreshold)
        milestones.add(WebCore::ReachedSessionRestorationRenderTreeSizeThreshold);

    if (events & _WKRenderingProgressEventFirstLayoutAfterSuppressedIncrementalRendering)
        milestones.add(WebCore::DidFirstLayoutAfterSuppressedIncrementalRendering);

    if (events & _WKRenderingProgressEventFirstPaintAfterSuppressedIncrementalRendering)
        milestones.add(WebCore::DidFirstPaintAfterSuppressedIncrementalRendering);

    if (events & _WKRenderingProgressEventDidRenderSignificantAmountOfText)
        milestones.add(WebCore::DidRenderSignificantAmountOfText);

    if (events & _WKRenderingProgressEventFirstMeaningfulPaint)
        milestones.add(WebCore::DidFirstMeaningfulPaint);

    return milestones;
}

- (void)_setObservedRenderingProgressEvents:(_WKRenderingProgressEvents)observedRenderingProgressEvents
{
    _observedRenderingProgressEvents = observedRenderingProgressEvents;
    _page->listenForLayoutMilestones(layoutMilestones(observedRenderingProgressEvents));
}

- (void)_getMainResourceDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);

    _page->getMainResourceDataOfFrame(_page->mainFrame(), [handler](API::Data* data, WebKit::CallbackBase::Error error) {
        void (^completionHandlerBlock)(NSData *, NSError *) = (void (^)(NSData *, NSError *))handler.get();
        if (error != WebKit::CallbackBase::Error::None) {
            // FIXME: Pipe a proper error in from the WebPageProxy.
            completionHandlerBlock(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<int>(error) userInfo:nil]);
        } else
            completionHandlerBlock(wrapper(*data), nil);
    });
}

- (void)_getWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler
{
    [self createWebArchiveDataWithCompletionHandler:completionHandler];
}

- (void)_getContentsAsStringWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    auto handler = makeBlockPtr(completionHandler);

    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::No, [handler](String string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None) {
            // FIXME: Pipe a proper error in from the WebPageProxy.
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<int>(error) userInfo:nil]);
        } else
            handler(string, nil);
    });
}

- (void)_getContentsOfAllFramesAsStringWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    auto handler = makeBlockPtr(completionHandler);
    _page->getContentsAsString(WebKit::ContentAsStringIncludesChildFrames::Yes, [handler](String string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            handler(nil);
        else
            handler(string);
    });
}

- (void)_getContentsAsAttributedStringWithCompletionHandler:(void (^)(NSAttributedString *, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *, NSError *))completionHandler
{
    _page->getContentsAsAttributedString([handler = makeBlockPtr(completionHandler)](auto& attributedString) {
        if (attributedString.string)
            handler([[attributedString.string.get() retain] autorelease], [[attributedString.documentAttributes.get() retain] autorelease], nil);
        else
            handler(nil, nil, createNSError(WKErrorUnknown).get());
    });
}

- (void)_getApplicationManifestWithCompletionHandler:(void (^)(_WKApplicationManifest *))completionHandler
{
#if ENABLE(APPLICATION_MANIFEST)
    _page->getApplicationManifest([completionHandler = makeBlockPtr(completionHandler)](const Optional<WebCore::ApplicationManifest>& manifest, WebKit::CallbackBase::Error error) {
        UNUSED_PARAM(error);
        if (completionHandler) {
            if (manifest) {
                auto apiManifest = API::ApplicationManifest::create(*manifest);
                completionHandler(wrapper(apiManifest));
            } else
                completionHandler(nil);
        }
    });
#else
    if (completionHandler)
        completionHandler(nil);
#endif
}

- (_WKPaginationMode)_paginationMode
{
    switch (_page->paginationMode()) {
    case WebCore::Pagination::Unpaginated:
        return _WKPaginationModeUnpaginated;
    case WebCore::Pagination::LeftToRightPaginated:
        return _WKPaginationModeLeftToRight;
    case WebCore::Pagination::RightToLeftPaginated:
        return _WKPaginationModeRightToLeft;
    case WebCore::Pagination::TopToBottomPaginated:
        return _WKPaginationModeTopToBottom;
    case WebCore::Pagination::BottomToTopPaginated:
        return _WKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return _WKPaginationModeUnpaginated;
}

- (void)_setPaginationMode:(_WKPaginationMode)paginationMode
{
    WebCore::Pagination::Mode mode;
    switch (paginationMode) {
    case _WKPaginationModeUnpaginated:
        mode = WebCore::Pagination::Unpaginated;
        break;
    case _WKPaginationModeLeftToRight:
        mode = WebCore::Pagination::LeftToRightPaginated;
        break;
    case _WKPaginationModeRightToLeft:
        mode = WebCore::Pagination::RightToLeftPaginated;
        break;
    case _WKPaginationModeTopToBottom:
        mode = WebCore::Pagination::TopToBottomPaginated;
        break;
    case _WKPaginationModeBottomToTop:
        mode = WebCore::Pagination::BottomToTopPaginated;
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
    _page->setPaginationBehavesLikeColumns(behavesLikeColumns);
}

- (CGFloat)_pageLength
{
    return _page->pageLength();
}

- (void)_setPageLength:(CGFloat)pageLength
{
    _page->setPageLength(pageLength);
}

- (CGFloat)_gapBetweenPages
{
    return _page->gapBetweenPages();
}

- (void)_setGapBetweenPages:(CGFloat)gapBetweenPages
{
    _page->setGapBetweenPages(gapBetweenPages);
}

- (BOOL)_paginationLineGridEnabled
{
    return _page->paginationLineGridEnabled();
}

- (void)_setPaginationLineGridEnabled:(BOOL)lineGridEnabled
{
    _page->setPaginationLineGridEnabled(lineGridEnabled);
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
    _page->setTextZoomFactor(zoomFactor);
}

- (double)_pageZoomFactor
{
    return [self pageZoom];
}

- (void)_setPageZoomFactor:(double)zoomFactor
{
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

static inline WebKit::FindOptions toFindOptions(_WKFindOptions wkFindOptions)
{
    unsigned findOptions = 0;

    if (wkFindOptions & _WKFindOptionsCaseInsensitive)
        findOptions |= WebKit::FindOptionsCaseInsensitive;
    if (wkFindOptions & _WKFindOptionsAtWordStarts)
        findOptions |= WebKit::FindOptionsAtWordStarts;
    if (wkFindOptions & _WKFindOptionsTreatMedialCapitalAsWordStart)
        findOptions |= WebKit::FindOptionsTreatMedialCapitalAsWordStart;
    if (wkFindOptions & _WKFindOptionsBackwards)
        findOptions |= WebKit::FindOptionsBackwards;
    if (wkFindOptions & _WKFindOptionsWrapAround)
        findOptions |= WebKit::FindOptionsWrapAround;
    if (wkFindOptions & _WKFindOptionsShowOverlay)
        findOptions |= WebKit::FindOptionsShowOverlay;
    if (wkFindOptions & _WKFindOptionsShowFindIndicator)
        findOptions |= WebKit::FindOptionsShowFindIndicator;
    if (wkFindOptions & _WKFindOptionsShowHighlight)
        findOptions |= WebKit::FindOptionsShowHighlight;
    if (wkFindOptions & _WKFindOptionsNoIndexChange)
        findOptions |= WebKit::FindOptionsNoIndexChange;
    if (wkFindOptions & _WKFindOptionsDetermineMatchIndex)
        findOptions |= WebKit::FindOptionsDetermineMatchIndex;

    return static_cast<WebKit::FindOptions>(findOptions);
}

- (void)_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
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
    if (!item)
        return;
    _page->recordNavigationSnapshot(item._item);
}

- (void)_isNavigatingToAppBoundDomain:(void(^)(BOOL))completionHandler
{
    _page->isNavigatingToAppBoundDomainTesting([completionHandler = makeBlockPtr(completionHandler)] (bool isAppBound) {
        completionHandler(isAppBound);
    });
}

- (void)_isForcedIntoAppBoundMode:(void(^)(BOOL))completionHandler
{
    _page->isForcedIntoAppBoundModeTesting([completionHandler = makeBlockPtr(completionHandler)] (bool isForcedIntoAppBoundMode) {
        completionHandler(isForcedIntoAppBoundMode);
    });
}

- (void)_serviceWorkersEnabled:(void(^)(BOOL))completionHandler
{
    auto enabled = [_configuration preferences]->_preferences.get()->serviceWorkersEnabled() || WebCore::RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled();
    completionHandler(enabled);
}

- (void)_clearServiceWorkerEntitlementOverride:(void (^)(void))completionHandler
{
    _page->clearServiceWorkerEntitlementOverride([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_preconnectToServer:(NSURL *)url
{
    _page->preconnectTo(url);
}

- (id <_WKInputDelegate>)_inputDelegate
{
    return _inputDelegate.getAutoreleased();
}

- (void)_setInputDelegate:(id <_WKInputDelegate>)inputDelegate
{
    _inputDelegate = inputDelegate;

    class FormClient : public API::FormClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit FormClient(WKWebView *webView)
            : m_webView(webView)
        {
        }

        virtual ~FormClient() { }

        void willSubmitForm(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebKit::WebFrameProxy& sourceFrame, const Vector<std::pair<WTF::String, WTF::String>>& textFieldValues, API::Object* userData, WTF::Function<void(void)>&& completionHandler) override
        {
            if (userData && userData->type() != API::Object::Type::Data) {
                ASSERT(!userData || userData->type() == API::Object::Type::Data);
                m_webView->_page->process().connection()->markCurrentlyDispatchedMessageAsInvalid();
                completionHandler();
                return;
            }

            auto inputDelegate = m_webView->_inputDelegate.get();

            if (![inputDelegate respondsToSelector:@selector(_webView:willSubmitFormValues:userObject:submissionHandler:)]) {
                completionHandler();
                return;
            }

            auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:textFieldValues.size()]);
            for (const auto& pair : textFieldValues)
                [valueMap setObject:pair.second forKey:pair.first];

            NSObject <NSSecureCoding> *userObject = nil;
            if (API::Data* data = static_cast<API::Data*>(userData)) {
                auto nsData = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<unsigned char*>(data->bytes()) length:data->size() freeWhenDone:NO]);
                auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:nsData.get() error:nullptr]);
                unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
                @try {
                    auto* allowedClasses = m_webView->_page->process().processPool().allowedClassesForParameterCoding();
                    userObject = [unarchiver decodeObjectOfClasses:allowedClasses forKey:@"userObject"];
                } @catch (NSException *exception) {
                    LOG_ERROR("Failed to decode user data: %@", exception);
                }
            }

            auto checker = WebKit::CompletionHandlerCallChecker::create(inputDelegate.get(), @selector(_webView:willSubmitFormValues:userObject:submissionHandler:));
            [inputDelegate _webView:m_webView willSubmitFormValues:valueMap.get() userObject:userObject submissionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] {
                if (checker->completionHandlerHasBeenCalled())
                    return;
                checker->didCallCompletionHandler();
                completionHandler();
            }).get()];
        }

    private:
        WKWebView *m_webView;
    };

    if (inputDelegate)
        _page->setFormClient(makeUnique<FormClient>(self));
    else
        _page->setFormClient(nullptr);
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
    _page->setFixedLayoutSize(WebCore::expandedIntSize(WebCore::FloatSize(fixedLayoutSize)));
}

- (void)_setBackgroundExtendsBeyondPage:(BOOL)backgroundExtends
{
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
    return _page->layoutSizeScaleFactor();
#endif
}

- (void)_setViewScale:(CGFloat)viewScale
{
    if (viewScale <= 0 || isnan(viewScale) || isinf(viewScale))
        [NSException raise:NSInvalidArgumentException format:@"View scale should be a positive number"];

#if PLATFORM(MAC)
    _impl->setViewScale(viewScale);
#else
    if (_page->layoutSizeScaleFactor() == viewScale)
        return;

    _page->setViewportConfigurationViewLayoutSize(_page->viewLayoutSize(), viewScale, _page->minimumEffectiveDeviceWidth());
#endif
}

- (NSArray<NSString *> *)_corsDisablingPatterns
{
    return createNSArray(_page->corsDisablingPatterns()).autorelease();
}

- (void)_setCORSDisablingPatterns:(NSArray<NSString *> *)patterns
{
    _page->setCORSDisablingPatterns(makeVector<String>(patterns));
}

- (void)_getProcessDisplayNameWithCompletionHandler:(void (^)(NSString *))completionHandler
{
    _page->getProcessDisplayName([handler = makeBlockPtr(completionHandler)](auto&& name) {
        handler(name);
    });
}

- (void)_setMinimumEffectiveDeviceWidth:(CGFloat)minimumEffectiveDeviceWidth
{
#if PLATFORM(IOS_FAMILY)
    if (_page->minimumEffectiveDeviceWidth() == minimumEffectiveDeviceWidth)
        return;

    if (_dynamicViewportUpdateMode == WebKit::DynamicViewportUpdateMode::NotResizing)
        _page->setViewportConfigurationViewLayoutSize(_page->viewLayoutSize(), _page->layoutSizeScaleFactor(), minimumEffectiveDeviceWidth);
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

- (void)_grantAccessToPreferenceService
{
    if (_page)
        _page->grantAccessToPreferenceService();
}

#pragma mark - scrollPerformanceData

- (void)_setScrollPerformanceDataCollectionEnabled:(BOOL)enabled
{
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
#if PLATFORM(IOS_FAMILY)
    _page->setAllowsMediaDocumentInlinePlayback(flag);
#endif
}

- (void)_setFullscreenDelegate:(id<_WKFullscreenDelegate>)delegate
{
#if ENABLE(FULLSCREEN_API)
    if (is<WebKit::FullscreenClient>(_page->fullscreenClient()))
        downcast<WebKit::FullscreenClient>(_page->fullscreenClient()).setDelegate(delegate);
#endif
}

- (id<_WKFullscreenDelegate>)_fullscreenDelegate
{
#if ENABLE(FULLSCREEN_API)
    if (is<WebKit::FullscreenClient>(_page->fullscreenClient()))
        return downcast<WebKit::FullscreenClient>(_page->fullscreenClient()).delegate().autorelease();
#endif
    return nil;
}

- (BOOL)_isInFullscreen
{
#if ENABLE(FULLSCREEN_API)
    return _page->fullScreenManager() && _page->fullScreenManager()->isFullScreen();
#else
    return false;
#endif
}

- (_WKMediaCaptureState)_mediaCaptureState
{
    return WebKit::toWKMediaCaptureState(_page->mediaStateFlags());
}

- (void)_setMediaCaptureEnabled:(BOOL)enabled
{
    _page->setMediaCaptureEnabled(enabled);
}

- (BOOL)_mediaCaptureEnabled
{
    return _page->mediaCaptureEnabled();
}

- (void)_setPageMuted:(_WKMediaMutedState)mutedState
{
    WebCore::MediaProducer::MutedStateFlags coreState = WebCore::MediaProducer::NoneMuted;

    if (mutedState & _WKMediaAudioMuted)
        coreState |= WebCore::MediaProducer::AudioIsMuted;
    if (mutedState & _WKMediaCaptureDevicesMuted)
        coreState |= WebCore::MediaProducer::AudioAndVideoCaptureIsMuted;
    if (mutedState & _WKMediaScreenCaptureMuted)
        coreState |= WebCore::MediaProducer::ScreenCaptureIsMuted;

    _page->setMuted(coreState);
}

- (void)_removeDataDetectedLinks:(dispatch_block_t)completion
{
#if ENABLE(DATA_DETECTION)
    _page->removeDataDetectedLinks([completion = makeBlockPtr(completion), page = makeWeakPtr(_page.get())] (auto& result) {
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
    [self _internalDoAfterNextPresentationUpdate:updateBlock withoutWaitingForPainting:NO withoutWaitingForAnimatedResize:NO];
}

- (void)_doAfterNextPresentationUpdateWithoutWaitingForPainting:(void (^)(void))updateBlock
{
    [self _internalDoAfterNextPresentationUpdate:updateBlock withoutWaitingForPainting:YES withoutWaitingForAnimatedResize:NO];
}

@end

@implementation WKWebView (WKDeprecated)

- (NSArray *)certificateChain
{
    auto certificateInfo = _page->pageLoadState().certificateInfo();
    if (!certificateInfo)
        return @[ ];

    return (__bridge NSArray *)certificateInfo->certificateInfo().certificateChain() ?: @[ ];
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

#undef RELEASE_LOG_IF_ALLOWED
