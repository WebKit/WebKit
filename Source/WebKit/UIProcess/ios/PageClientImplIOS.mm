/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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
#import "PageClientImplIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "APIData.h"
#import "APIUIClient.h"
#import "ApplicationStateTracker.h"
#import "DrawingAreaProxy.h"
#import "EndowmentStateTracker.h"
#import "FrameInfoData.h"
#import "InteractionInformationAtPosition.h"
#import "NativeWebKeyboardEvent.h"
#import "NavigationState.h"
#import "PlatformXRSystem.h"
#import "RemoteLayerTreeNode.h"
#import "RunningBoardServicesSPI.h"
#import "TapHandlingResult.h"
#import "UIKitSPI.h"
#import "UndoOrRedo.h"
#import "ViewSnapshotStore.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKEditCommand.h"
#import "WKFullScreenViewController.h"
#import "WKGeolocationProviderIOS.h"
#import "WKPasswordView.h"
#import "WKProcessPoolInternal.h"
#import "WKVisibilityPropagationView.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewIOS.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebContextMenuProxy.h"
#import "WebDataListSuggestionsDropdownIOS.h"
#import "WebEditCommandProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKDownloadInternal.h"
#import <WebCore/Cursor.h>
#import <WebCore/DOMPasteAccess.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/ScreenOrientationType.h>
#import <WebCore/ShareData.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/ValidationBubble.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/SpanCocoa.h>

@interface UIWindow ()
- (BOOL)_isHostedInAnotherProcess;
@end

namespace WebKit {
using namespace WebCore;

PageClientImpl::PageClientImpl(WKContentView *contentView, WKWebView *webView)
    : PageClientImplCocoa(webView)
    , m_contentView(contentView)
    , m_undoTarget(adoptNS([[WKEditorUndoTarget alloc] init]))
{
}

PageClientImpl::~PageClientImpl()
{
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& webProcessProxy)
{
    return [contentView() _createDrawingAreaProxy:webProcessProxy];
}

void PageClientImpl::setViewNeedsDisplay(const Region&)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, ScrollIsAnimated animated)
{
    [webView() _scrollToContentScrollPosition:scrollPosition scrollOrigin:scrollOrigin animated:animated == ScrollIsAnimated::Yes];
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    if (UIScrollView *scroller = [contentView() _scroller])
        return scroller.contentOffset;

    return { };
}

IntSize PageClientImpl::viewSize()
{
    return IntSize([webView() bounds].size);
}

bool PageClientImpl::isViewWindowActive()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=133098
    return isViewVisible() || [webView() _isRetainingActiveFocusedState];
}

bool PageClientImpl::isViewFocused()
{
    auto webView = this->webView();
    return (isViewInWindow() && ![webView _isBackground] && [webView _contentViewIsFirstResponder]) || [webView _isRetainingActiveFocusedState];
}

bool PageClientImpl::isViewVisible()
{
    auto webView = this->webView();
    if (!webView)
        return false;

    if (isViewInWindow() && ![webView _isBackground])
        return true;
    
    if ([webView _isShowingVideoPictureInPicture])
        return true;
    
    if ([webView _mayAutomaticallyShowVideoPictureInPicture])
        return true;

#if ENABLE(WEBXR) && !USE(OPENXR)
    auto page = webView->_page;
    if (page && page->xrSystem() && page->xrSystem()->hasActiveSession())
        return true;
#endif

    return false;
}

void PageClientImpl::viewIsBecomingVisible()
{
#if ENABLE(PAGE_LOAD_OBSERVER)
    if (RetainPtr webView = this->webView())
        [webView _updatePageLoadObserverState];
#endif
}

bool PageClientImpl::canTakeForegroundAssertions()
{
    if (EndowmentStateTracker::singleton().isVisible()) {
        // If the application is visible according to the UIKit visibility endowment then we can take
        // foreground assertions. Note that for view services, the visibility endownment from the host
        // application gets propagated to the view service.
        return true;
    }

    // If there is no run time limitation, then it means that the process is allowed to run for an extended
    // period of time in the background (e.g. a daemon) and we let such processes take foreground assertions.
    return [RBSProcessHandle currentProcess].activeLimitations.runTime == RBSProcessTimeLimitationNone;
}

bool PageClientImpl::isViewInWindow()
{
    // FIXME: in WebKitTestRunner, m_webView is nil, so check the content view instead.
    if (auto webView = this->webView())
        return [webView window];

    return [contentView() window];
}

bool PageClientImpl::isViewVisibleOrOccluded()
{
    return isViewVisible();
}

bool PageClientImpl::isVisuallyIdle()
{
    return !isViewVisible();
}

void PageClientImpl::processDidExit()
{
    [contentView() _processDidExit];
    [webView() _processDidExit];
}

void PageClientImpl::processWillSwap()
{
    [contentView() _processWillSwap];
    [webView() _processWillSwap];
}

void PageClientImpl::didRelaunchProcess()
{
    [contentView() _didRelaunchProcess];
    [webView() _didRelaunchProcess];
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void PageClientImpl::didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID)
{
    [contentView() _webProcessDidCreateContextForVisibilityPropagation];
}

#if ENABLE(GPU_PROCESS)
void PageClientImpl::didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID)
{
    [contentView() _gpuProcessDidCreateContextForVisibilityPropagation];
}
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)
void PageClientImpl::didCreateContextInModelProcessForVisibilityPropagation(LayerHostingContextID)
{
    [m_contentView _modelProcessDidCreateContextForVisibilityPropagation];
}
#endif // ENABLE(MODEL_PROCESS)

#if USE(EXTENSIONKIT)
UIView *PageClientImpl::createVisibilityPropagationView()
{
    return [contentView() _createVisibilityPropagationView];
}
#endif
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(GPU_PROCESS)
void PageClientImpl::gpuProcessDidExit()
{
    [contentView() _gpuProcessDidExit];
    PageClientImplCocoa::gpuProcessDidExit();
}
#endif

#if ENABLE(MODEL_PROCESS)
void PageClientImpl::modelProcessDidExit()
{
    [m_contentView _modelProcessDidExit];
    PageClientImplCocoa::modelProcessDidExit();
}
#endif

void PageClientImpl::preferencesDidChange()
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String&)
{
    notImplemented();
}

void PageClientImpl::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    [contentView() _didNotHandleTapAsClick:point];
}

void PageClientImpl::didHandleTapAsHover()
{
    [contentView() _didHandleTapAsHover];
}

void PageClientImpl::didCompleteSyntheticClick()
{
    [contentView() _didCompleteSyntheticClick];
}

void PageClientImpl::decidePolicyForGeolocationPermissionRequest(WebFrameProxy& frame, const FrameInfoData& frameInfo, Function<void(bool)>& completionHandler)
{
    if (auto webView = this->webView()) {
        auto* geolocationProvider = [wrapper(webView->_page->configuration().processPool()) _geolocationProvider];
        [geolocationProvider decidePolicyForGeolocationRequestFromOrigin:FrameInfoData { frameInfo } completionHandler:std::exchange(completionHandler, nullptr) view:webView.get()];
    }
}

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    auto webView = this->webView();
    [webView _didStartProvisionalLoadForMainFrame];
    [contentView() _didStartProvisionalLoadForMainFrame];
    [webView _hidePasswordView];
}

void PageClientImpl::didFailProvisionalLoadForMainFrame()
{
    [webView() _hidePasswordView];
}

void PageClientImpl::didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider)
{
    auto webView = this->webView();
    [webView _hidePasswordView];
    [webView _setHasCustomContentView:useCustomContentProvider loadedMIMEType:mimeType];
    [contentView() _didCommitLoadForMainFrame];
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize&)
{
    notImplemented();
}

void PageClientImpl::disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier requestID)
{
    [contentView() _disableDoubleTapGesturesDuringTapIfNecessary:requestID];
}

void PageClientImpl::handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier requestID, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel)
{
    [contentView() _handleSmartMagnificationInformationForPotentialTap:requestID renderRect:renderRect fitEntireRect:fitEntireRect viewportMinimumScale:viewportMinimumScale viewportMaximumScale:viewportMaximumScale nodeIsRootLevel:nodeIsRootLevel];
}

double PageClientImpl::minimumZoomScale() const
{
    if (UIScrollView *scroller = [webView() scrollView])
        return scroller.minimumZoomScale;

    return 1;
}

WebCore::FloatRect PageClientImpl::documentRect() const
{
    return [contentView() bounds];
}

void PageClientImpl::setCursor(const Cursor& cursor)
{
    // The Web process may have asked to change the cursor when the view was in an active window, but
    // if it is no longer in a window or the window is not active, then the cursor should not change.
    if (!isViewWindowActive())
        return;

    cursor.setAsPlatformCursor();
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void PageClientImpl::didChangeViewportProperties(const ViewportAttributes&)
{
    notImplemented();
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    auto actionName = command->label();
    auto commandObjC = adoptNS([[WKEditCommand alloc] initWithWebEditCommandProxy:WTFMove(command)]);
    
    NSUndoManager *undoManager = [contentView() undoManagerForWebView];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == UndoOrRedo::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:(NSString *)actionName];
}

void PageClientImpl::clearAllEditCommands()
{
    [[contentView() undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == UndoOrRedo::Undo) ? [[contentView() undoManager] canUndo] : [[contentView() undoManager] canRedo];
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == UndoOrRedo::Undo) ? [[contentView() undoManager] undo] : [[contentView() undoManager] redo];
}

void PageClientImpl::accessibilityWebProcessTokenReceived(std::span<const uint8_t> data, pid_t)
{
    [contentView() _setAccessibilityWebProcessToken:toNSData(data).get()];
}

bool PageClientImpl::interpretKeyEvent(const NativeWebKeyboardEvent& event, bool isCharEvent)
{
    return [contentView() _interpretKeyEvent:event.nativeEvent() isCharEvent:isCharEvent];
}

void PageClientImpl::positionInformationDidChange(const InteractionInformationAtPosition& info)
{
    [contentView() _positionInformationDidChange:info];
}

void PageClientImpl::saveImageToLibrary(Ref<SharedBuffer>&& imageBuffer)
{
    RetainPtr<NSData> imageData = imageBuffer->createNSData();
    UIImageDataWriteToSavedPhotosAlbum(imageData.get(), nil, NULL, NULL);
}

bool PageClientImpl::executeSavedCommandBySelector(const String&)
{
    notImplemented();
    return false;
}

void PageClientImpl::selectionDidChange()
{
    [contentView() _selectionChanged];
}

void PageClientImpl::updateSecureInputState()
{
    notImplemented();
}

void PageClientImpl::resetSecureInputState()
{
    notImplemented();
}

void PageClientImpl::notifyInputContextAboutDiscardedComposition()
{
    notImplemented();
}

void PageClientImpl::assistiveTechnologyMakeFirstResponder()
{
    [contentView() becomeFirstResponder];
}

void PageClientImpl::makeFirstResponder()
{
    notImplemented();
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& rect)
{
    return rect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& rect)
{
    return rect;
}

IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    return IntPoint([contentView() convertPoint:point fromView:nil]);
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    return enclosingIntRect([contentView() convertRect:rect toView:nil]);
}
    
IntPoint PageClientImpl::accessibilityScreenToRootView(const IntPoint& point)
{
    CGPoint rootViewPoint = point;
    auto contentView = this->contentView();
    if ([contentView respondsToSelector:@selector(accessibilityConvertPointFromSceneReferenceCoordinates:)])
        rootViewPoint = [contentView accessibilityConvertPointFromSceneReferenceCoordinates:rootViewPoint];
    return IntPoint(rootViewPoint);
}

void PageClientImpl::relayAccessibilityNotification(const String& notificationName, const RetainPtr<NSData>& notificationData)
{
    auto contentView = this->contentView();
    if ([contentView respondsToSelector:@selector(accessibilityRelayNotification:notificationData:)])
        [contentView accessibilityRelayNotification:notificationName notificationData:notificationData.get()];
}

IntRect PageClientImpl::rootViewToAccessibilityScreen(const IntRect& rect)
{
    CGRect rootViewRect = rect;
    auto contentView = this->contentView();
    if ([contentView respondsToSelector:@selector(accessibilityConvertRectToSceneReferenceCoordinates:)])
        rootViewRect = [contentView accessibilityConvertRectToSceneReferenceCoordinates:rootViewRect];
    return enclosingIntRect(rootViewRect);
}
    
void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool eventWasHandled)
{
    [contentView() _didHandleKeyEvent:event.nativeEvent() eventWasHandled:eventWasHandled];
}

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& nativeWebTouchEvent, bool eventHandled)
{
    [contentView() _touchEvent:nativeWebTouchEvent preventsNativeGestures:eventHandled];
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)

void PageClientImpl::doneDeferringTouchStart(bool preventNativeGestures)
{
    [contentView() _doneDeferringTouchStart:preventNativeGestures];
}

void PageClientImpl::doneDeferringTouchMove(bool preventNativeGestures)
{
    [contentView() _doneDeferringTouchMove:preventNativeGestures];
}

void PageClientImpl::doneDeferringTouchEnd(bool preventNativeGestures)
{
    [contentView() _doneDeferringTouchEnd:preventNativeGestures];
}

#endif // ENABLE(IOS_TOUCH_EVENTS)

#if ENABLE(IMAGE_ANALYSIS)

void PageClientImpl::requestTextRecognition(const URL& imageURL, ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completion)
{
    [contentView() requestTextRecognition:imageURL imageData:WTFMove(imageData) sourceLanguageIdentifier:sourceLanguageIdentifier targetLanguageIdentifier:targetLanguageIdentifier completionHandler:WTFMove(completion)];
}

#endif // ENABLE(IMAGE_ANALYSIS)

WebCore::DataOwnerType PageClientImpl::dataOwnerForPasteboard(PasteboardAccessIntent intent) const
{
    return [contentView() _dataOwnerForPasteboard:intent];
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy&)
{
    return nullptr;
}

void PageClientImpl::setTextIndicator(Ref<TextIndicator> textIndicator, WebCore::TextIndicatorLifetime)
{
    [contentView() setUpTextIndicator:textIndicator];
}

void PageClientImpl::clearTextIndicator(WebCore::TextIndicatorDismissalAnimation dismissalAnimation)
{
    [contentView() clearTextIndicator:dismissalAnimation];
}

void PageClientImpl::setTextIndicatorAnimationProgress(float animationProgress)
{
    [contentView() setTextIndicatorAnimationProgress:animationProgress];
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
}

void PageClientImpl::makeViewBlank(bool makeBlank)
{
    [contentView() layer].opacity = makeBlank ? 0 : 1;
}

void PageClientImpl::showBrowsingWarning(const BrowsingWarning& warning, CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&& completionHandler)
{
    if (auto webView = this->webView())
        [webView _showBrowsingWarning:warning completionHandler:WTFMove(completionHandler)];
    else
        completionHandler(ContinueUnsafeLoad::No);
}

void PageClientImpl::clearBrowsingWarning()
{
    [webView() _clearBrowsingWarning];
}

void PageClientImpl::clearBrowsingWarningIfForMainFrameNavigation()
{
    [webView() _clearBrowsingWarningIfForMainFrameNavigation];
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext&)
{
}

void PageClientImpl::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
#if ENABLE(REVEAL)
    DictionaryLookup::showPopup(dictionaryPopupInfo, m_contentView.getAutoreleased(), nullptr);
#else
    UNUSED_PARAM(dictionaryPopupInfo);
#endif // ENABLE(REVEAL)
}

bool PageClientImpl::effectiveAppearanceIsDark() const
{
    return [webView() _effectiveAppearanceIsDark];
}

bool PageClientImpl::effectiveUserInterfaceLevelIsElevated() const
{
    return [webView() _effectiveUserInterfaceLevelIsElevated];
}

void PageClientImpl::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    [contentView() _setAcceleratedCompositingRootView:rootNode ? rootNode->uiView() : nil];
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    notImplemented();
    return nullptr;
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&&)
{
    return [webView() _takeViewSnapshot];
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    notImplemented();
}

void PageClientImpl::commitPotentialTapFailed()
{
    [contentView() _commitPotentialTapFailed];
}

void PageClientImpl::didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling)
{
    [contentView() _didGetTapHighlightForRequest:requestID color:color quads:highlightedQuads topLeftRadius:topLeftRadius topRightRadius:topRightRadius bottomLeftRadius:bottomLeftRadius bottomRightRadius:bottomRightRadius nodeHasBuiltInClickHandling:nodeHasBuiltInClickHandling];
}

void PageClientImpl::didCommitLayerTree(const RemoteLayerTreeTransaction& layerTreeTransaction)
{
    [contentView() _didCommitLayerTree:layerTreeTransaction];
}

void PageClientImpl::layerTreeCommitComplete()
{
    [contentView() _layerTreeCommitComplete];
}

void PageClientImpl::couldNotRestorePageState()
{
    [webView() _couldNotRestorePageState];
}

void PageClientImpl::restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale)
{
    [webView() _restorePageScrollPosition:scrollPosition scrollOrigin:scrollOrigin previousObscuredInset:obscuredInsetsOnSave scale:scale];
}

void PageClientImpl::restorePageCenterAndScale(std::optional<WebCore::FloatPoint> center, double scale)
{
    [webView() _restorePageStateToUnobscuredCenter:center scale:scale];
}

void PageClientImpl::elementDidFocus(const FocusedElementInformation& nodeInformation, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState> activityStateChanges, API::Object* userData)
{
    auto userObject = userData ? userData->toNSObject() : RetainPtr<NSObject<NSSecureCoding>>();
    [contentView() _elementDidFocus:nodeInformation userIsInteracting:userIsInteracting blurPreviousNode:blurPreviousNode activityStateChanges:activityStateChanges userObject:userObject.get()];
}

void PageClientImpl::updateInputContextAfterBlurringAndRefocusingElement()
{
    [contentView() _updateInputContextAfterBlurringAndRefocusingElement];
}

void PageClientImpl::updateFocusedElementInformation(const FocusedElementInformation& information)
{
    [contentView() _updateFocusedElementInformation:information];
}

bool PageClientImpl::isFocusingElement()
{
    return [contentView() isFocusingElement];
}

void PageClientImpl::elementDidBlur()
{
    [contentView() _elementDidBlur];
}

void PageClientImpl::focusedElementDidChangeInputMode(WebCore::InputMode mode)
{
    [contentView() _didUpdateInputMode:mode];
}

void PageClientImpl::didUpdateEditorState()
{
    [contentView() _didUpdateEditorState];
}

void PageClientImpl::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect, WebCore::RouteSharingPolicy policy, const String& contextUID)
{
    [contentView() _showPlaybackTargetPicker:hasVideo fromRect:elementRect routeSharingPolicy:policy routingContextUID:contextUID];
}

bool PageClientImpl::handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, const FrameInfoData& frameInfo, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
{
    [contentView() _showRunOpenPanel:parameters frameInfo:frameInfo resultListener:listener];
    return true;
}

bool PageClientImpl::showShareSheet(const ShareDataWithParsedURL& shareData, WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    [contentView() _showShareSheet:shareData inRect:std::nullopt completionHandler:WTFMove(completionHandler)];
    return true;
}

void PageClientImpl::showContactPicker(const WebCore::ContactsRequestData& requestData, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& completionHandler)
{
    [contentView() _showContactPicker:requestData completionHandler:WTFMove(completionHandler)];
}

void PageClientImpl::showInspectorHighlight(const WebCore::InspectorOverlay::Highlight& highlight)
{
    [contentView() _showInspectorHighlight:highlight];
}

void PageClientImpl::hideInspectorHighlight()
{
    [contentView() _hideInspectorHighlight];
}

void PageClientImpl::showInspectorIndication()
{
    [contentView() setShowingInspectorIndication:YES];
}

void PageClientImpl::hideInspectorIndication()
{
    [contentView() setShowingInspectorIndication:NO];
}

void PageClientImpl::enableInspectorNodeSearch()
{
    [contentView() _enableInspectorNodeSearch];
}

void PageClientImpl::disableInspectorNodeSearch()
{
    [contentView() _disableInspectorNodeSearch];
}

#if ENABLE(FULLSCREEN_API)

WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

// WebFullScreenManagerProxyClient

void PageClientImpl::closeFullScreenManager()
{
    [webView() closeFullScreenWindowController];
}

bool PageClientImpl::isFullScreen()
{
    auto webView = this->webView();
    if (![webView hasFullScreenWindowController])
        return false;

    return [webView fullScreenWindowController].isFullScreen;
}

void PageClientImpl::enterFullScreen(FloatSize mediaDimensions)
{
    [[webView() fullScreenWindowController] enterFullScreen:mediaDimensions];
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void PageClientImpl::updateImageSource()
{
    [[webView() fullScreenWindowController] updateImageSource];
}
#endif

void PageClientImpl::exitFullScreen()
{
    [[webView() fullScreenWindowController] exitFullScreen];
}

static UIInterfaceOrientationMask toUIInterfaceOrientationMask(WebCore::ScreenOrientationType orientation)
{
    switch (orientation) {
    case WebCore::ScreenOrientationType::PortraitPrimary:
        return UIInterfaceOrientationMaskPortrait;
    case WebCore::ScreenOrientationType::PortraitSecondary:
        return UIInterfaceOrientationMaskPortraitUpsideDown;
    case WebCore::ScreenOrientationType::LandscapePrimary:
        return UIInterfaceOrientationMaskLandscapeRight;
    case WebCore::ScreenOrientationType::LandscapeSecondary:
        return UIInterfaceOrientationMaskLandscapeLeft;
    }
    ASSERT_NOT_REACHED();
    return UIInterfaceOrientationMaskPortrait;
}

bool PageClientImpl::lockFullscreenOrientation(WebCore::ScreenOrientationType orientation)
{
    [[webView() fullScreenWindowController] setSupportedOrientations:toUIInterfaceOrientationMask(orientation)];
    return true;
}

void PageClientImpl::unlockFullscreenOrientation()
{
    [[webView() fullScreenWindowController] resetSupportedOrientations];
}

void PageClientImpl::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [[webView() fullScreenWindowController] beganEnterFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
}

void PageClientImpl::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [[webView() fullScreenWindowController] beganExitFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, std::span<const uint8_t> dataReference)
{
    [webView() _didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:suggestedFilename data:toNSData(dataReference).get()];
}

void PageClientImpl::scrollingNodeScrollViewWillStartPanGesture(ScrollingNodeID)
{
    [contentView() scrollViewWillStartPanOrPinchGesture];
}

void PageClientImpl::scrollingNodeScrollViewDidScroll(ScrollingNodeID)
{
    [contentView() _didScroll];
}

void PageClientImpl::scrollingNodeScrollWillStartScroll(std::optional<ScrollingNodeID> nodeID)
{
    [contentView() _scrollingNodeScrollingWillBegin:nodeID];
}

void PageClientImpl::scrollingNodeScrollDidEndScroll(std::optional<ScrollingNodeID> nodeID)
{
    [contentView() _scrollingNodeScrollingDidEnd:nodeID];
}

Vector<String> PageClientImpl::mimeTypesWithCustomContentProviders()
{
    return [webView() _contentProviderRegistry]._mimeTypesWithCustomContentProviders;
}

void PageClientImpl::navigationGestureDidBegin()
{
    if (auto webView = this->webView()) {
        [webView _navigationGestureDidBegin];
        if (auto* navigationState = NavigationState::fromWebPage(*webView->_page))
            navigationState->navigationGestureDidBegin();
    }
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (auto webView = this->webView()) {
        if (auto* navigationState = NavigationState::fromWebPage(*webView->_page))
            navigationState->navigationGestureWillEnd(willNavigate, item);
    }
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (auto webView = this->webView()) {
        if (auto* navigationState = NavigationState::fromWebPage(*webView->_page))
            navigationState->navigationGestureDidEnd(willNavigate, item);
        [webView _navigationGestureDidEnd];
    }
}

void PageClientImpl::navigationGestureDidEnd()
{
    [webView() _navigationGestureDidEnd];
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (auto webView = this->webView()) {
        if (auto* navigationState = NavigationState::fromWebPage(*webView->_page))
            navigationState->willRecordNavigationSnapshot(item);
    }
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    if (auto webView = this->webView()) {
        if (auto* navigationState = NavigationState::fromWebPage(*webView->_page))
            navigationState->navigationGestureSnapshotWasRemoved();
    }
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::didFinishNavigation(API::Navigation* navigation)
{
    [webView() _didFinishNavigation:navigation];
}

void PageClientImpl::didFailNavigation(API::Navigation* navigation)
{
    [webView() _didFailNavigation:navigation];
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType navigationType)
{
    [webView() _didSameDocumentNavigationForMainFrame:navigationType];
}

void PageClientImpl::didChangeBackgroundColor()
{
    [webView() _updateScrollViewBackground];
}

void PageClientImpl::videoControlsManagerDidChange()
{
    PageClientImplCocoa::videoControlsManagerDidChange();
    [webView() _videoControlsManagerDidChange];
}

void PageClientImpl::refView()
{
    [m_contentView retain];
    [m_webView retain];
}

void PageClientImpl::derefView()
{
    [m_contentView release];
    [m_webView release];
}

void PageClientImpl::didRestoreScrollPosition()
{
}

WebCore::UserInterfaceLayoutDirection PageClientImpl::userInterfaceLayoutDirection()
{
    if (auto webView = this->webView())
        return ([UIView userInterfaceLayoutDirectionForSemanticContentAttribute:[webView semanticContentAttribute]] == UIUserInterfaceLayoutDirectionLeftToRight) ? WebCore::UserInterfaceLayoutDirection::LTR : WebCore::UserInterfaceLayoutDirection::RTL;
    return WebCore::UserInterfaceLayoutDirection::LTR;
}

Ref<ValidationBubble> PageClientImpl::createValidationBubble(const String& message, const ValidationBubble::Settings& settings)
{
    return ValidationBubble::create(m_contentView.getAutoreleased(), message, settings);
}

#if ENABLE(INPUT_TYPE_COLOR)
RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&, ColorControlSupportsAlpha, Vector<WebCore::Color>&&)
{
    return nullptr;
}
#endif

#if ENABLE(DATALIST_ELEMENT)
RefPtr<WebDataListSuggestionsDropdown> PageClientImpl::createDataListSuggestionsDropdown(WebPageProxy& page)
{
    return WebDataListSuggestionsDropdownIOS::create(page, m_contentView.getAutoreleased());
}
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
RefPtr<WebDateTimePicker> PageClientImpl::createDateTimePicker(WebPageProxy&)
{
    return nullptr;
}
#endif

#if ENABLE(DRAG_SUPPORT)
void PageClientImpl::didPerformDragOperation(bool handled)
{
    [contentView() _didPerformDragOperation:handled];
}

void PageClientImpl::didHandleDragStartRequest(bool started)
{
    [contentView() _didHandleDragStartRequest:started];
}

void PageClientImpl::didHandleAdditionalDragItemsRequest(bool added)
{
    [contentView() _didHandleAdditionalDragItemsRequest:added];
}

void PageClientImpl::startDrag(const DragItem& item, ShareableBitmap::Handle&& image)
{
    auto bitmap = ShareableBitmap::create(WTFMove(image));
    if (!bitmap)
        return;
    [contentView() _startDrag:bitmap->makeCGImageCopy() item:item];
}

void PageClientImpl::willReceiveEditDragSnapshot()
{
    [contentView() _willReceiveEditDragSnapshot];
}

void PageClientImpl::didReceiveEditDragSnapshot(std::optional<TextIndicatorData> data)
{
    [contentView() _didReceiveEditDragSnapshot:data];
}

void PageClientImpl::didChangeDragCaretRect(const IntRect& previousCaretRect, const IntRect& caretRect)
{
    [contentView() _didChangeDragCaretRect:previousCaretRect currentRect:caretRect];
}
#endif

void PageClientImpl::performSwitchHapticFeedback()
{
#if HAVE(UI_IMPACT_FEEDBACK_GENERATOR)
    auto feedbackGenerator = adoptNS([[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight]);
    [feedbackGenerator impactOccurred];
#endif
}

#if USE(QUICK_LOOK)
void PageClientImpl::requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&& completionHandler)
{
    auto passwordHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSString *password) {
        completionHandler(password);
    });

    auto webView = this->webView();
    if (WKPasswordView *passwordView = [webView _passwordView]) {
        ASSERT(fileName == String { passwordView.documentName });
        [passwordView showPasswordFailureAlert];
        passwordView.userDidEnterPassword = passwordHandler.get();
        return;
    }

    [webView _showPasswordViewWithDocumentName:fileName passwordHandler:passwordHandler.get()];
}
#endif

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, WebCore::DOMPasteRequiresInteraction requiresInteraction, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    [contentView() _requestDOMPasteAccessForCategory:pasteAccessCategory requiresInteraction:requiresInteraction elementRect:elementRect originIdentifier:originIdentifier completionHandler:WTFMove(completionHandler)];
}

void PageClientImpl::cancelPointersForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    [contentView() cancelPointersForGestureRecognizer:gestureRecognizer];
}

std::optional<unsigned> PageClientImpl::activeTouchIdentifierForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    return [contentView() activeTouchIdentifierForGestureRecognizer:gestureRecognizer];
}

void PageClientImpl::handleAutocorrectionContext(const WebAutocorrectionContext& context)
{
    [contentView() _handleAutocorrectionContext:context];
}

void PageClientImpl::showDictationAlternativeUI(const WebCore::FloatRect&, WebCore::DictationContext)
{
    notImplemented();
}

void PageClientImpl::showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition& positionInformation)
{
    [contentView() _showDataDetectorsUIForPositionInformation:positionInformation];
}

void PageClientImpl::hardwareKeyboardAvailabilityChanged()
{
    [contentView() _hardwareKeyboardAvailabilityChanged];
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void PageClientImpl::didCleanupFullscreen()
{
#if ENABLE(FULLSCREEN_API)
    [[webView() fullScreenWindowController] didCleanupFullscreen];
#endif
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#if ENABLE(ATTACHMENT_ELEMENT)

void PageClientImpl::writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&& info)
{
    [contentView() _writePromisedAttachmentToPasteboard:WTFMove(info)];
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

void PageClientImpl::setMouseEventPolicy(WebCore::MouseEventPolicy policy)
{
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [contentView() _setMouseEventPolicy:policy];
#endif
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
void PageClientImpl::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    [contentView() _showMediaControlsContextMenu:WTFMove(targetFrame) items:WTFMove(items) completionHandler:WTFMove(completionHandler)];
}
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
void PageClientImpl::handleAsynchronousCancelableScrollEvent(WKBaseScrollView *scrollView, WKBEScrollViewScrollUpdate *update, void (^completion)(BOOL handled))
{
    [webView() scrollView:scrollView handleScrollUpdate:update completion:completion];
}
#endif

void PageClientImpl::runModalJavaScriptDialog(CompletionHandler<void()>&& callback)
{
    [contentView() runModalJavaScriptDialog:WTFMove(callback)];
}

WebCore::Color PageClientImpl::contentViewBackgroundColor()
{
    WebCore::Color color;
    [[webView() traitCollection] performAsCurrentTraitCollection:[&]() {
        color = WebCore::roundAndClampToSRGBALossy([contentView() backgroundColor].CGColor);
        if (color.isValid())
            return;
        color = WebCore::roundAndClampToSRGBALossy(UIColor.systemBackgroundColor.CGColor);
    }];

    return color;
}

Color PageClientImpl::insertionPointColor()
{
    return roundAndClampToSRGBALossy([webView() _insertionPointColor].CGColor);
}

bool PageClientImpl::isScreenBeingCaptured()
{
    return [contentView() screenIsBeingCaptured];
}

void PageClientImpl::requestScrollToRect(const FloatRect& targetRect, const FloatPoint& origin)
{
    [contentView() _scrollToRect:targetRect withOrigin:origin minimumScrollDistance:0];
}

String PageClientImpl::sceneID()
{
    return [contentView() window].windowScene._sceneIdentifier;
}

void PageClientImpl::beginTextRecognitionForFullscreenVideo(ShareableBitmap::Handle&& imageHandle, AVPlayerViewController *playerViewController)
{
    [contentView() beginTextRecognitionForFullscreenVideo:WTFMove(imageHandle) playerViewController:playerViewController];
}

void PageClientImpl::cancelTextRecognitionForFullscreenVideo(AVPlayerViewController *controller)
{
    [contentView() cancelTextRecognitionForFullscreenVideo:controller];
}

bool PageClientImpl::isTextRecognitionInFullscreenVideoEnabled() const
{
    return [contentView() isTextRecognitionInFullscreenVideoEnabled];
}

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)
void PageClientImpl::beginTextRecognitionForVideoInElementFullscreen(ShareableBitmap::Handle&& bitmapHandle, FloatRect bounds)
{
    [contentView() beginTextRecognitionForVideoInElementFullscreen:WTFMove(bitmapHandle) bounds:bounds];
}

void PageClientImpl::cancelTextRecognitionForVideoInElementFullscreen()
{
    [contentView() cancelTextRecognitionForVideoInElementFullscreen];
}
#endif

bool PageClientImpl::hasResizableWindows() const
{
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    return [webView() _isWindowResizingEnabled];
#else
    return false;
#endif
}

UIViewController *PageClientImpl::presentingViewController() const
{
    RetainPtr webView = this->webView();

#if ENABLE(FULLSCREEN_API)
    if ([webView fullScreenWindowController].isFullScreen)
        return [webView fullScreenWindowController].fullScreenViewController;
#endif

    if (auto page = webView->_page)
        return page->uiClient().presentingViewController();

    return nil;
}

FloatRect PageClientImpl::rootViewToWebView(const FloatRect& rect) const
{
    return [webView() convertRect:rect fromView:contentView().get()];
}

FloatPoint PageClientImpl::webViewToRootView(const FloatPoint& point) const
{
    return [webView() convertPoint:point toView:contentView().get()];
}

#if HAVE(SPATIAL_TRACKING_LABEL)
const String& PageClientImpl::spatialTrackingLabel() const
{
    return [contentView() spatialTrackingLabel];
}
#endif

void PageClientImpl::scheduleVisibleContentRectUpdate()
{
    [webView() _scheduleVisibleContentRectUpdate];
}

#if ENABLE(PDF_PLUGIN)
void PageClientImpl::pluginDidInstallPDFDocument(double initialScale)
{
    [webView() _pluginDidInstallPDFDocument:initialScale];
}
#endif

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
