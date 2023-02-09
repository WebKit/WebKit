/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#import "ApplicationStateTracker.h"
#import "DataReference.h"
#import "DrawingAreaProxy.h"
#import "EndowmentStateTracker.h"
#import "FrameInfoData.h"
#import "InteractionInformationAtPosition.h"
#import "NativeWebKeyboardEvent.h"
#import "NavigationState.h"
#import "RunningBoardServicesSPI.h"
#import "StringUtilities.h"
#import "TapHandlingResult.h"
#import "UIKitSPI.h"
#import "UndoOrRedo.h"
#import "ViewSnapshotStore.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKEditCommand.h"
#import "WKGeolocationProviderIOS.h"
#import "WKPasswordView.h"
#import "WKProcessPoolInternal.h"
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

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_webView.get()->_page->process().connection())

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

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return [m_contentView _createDrawingAreaProxy:process];
}

void PageClientImpl::setViewNeedsDisplay(const Region&)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, ScrollIsAnimated animated)
{
    [m_webView _scrollToContentScrollPosition:scrollPosition scrollOrigin:scrollOrigin animated:animated == ScrollIsAnimated::Yes];
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    if (UIScrollView *scroller = [m_contentView _scroller])
        return scroller.contentOffset;

    return { };
}

IntSize PageClientImpl::viewSize()
{
    return IntSize([m_webView bounds].size);
}

bool PageClientImpl::isViewWindowActive()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=133098
    return isViewVisible() || [m_webView _isRetainingActiveFocusedState];
}

bool PageClientImpl::isViewFocused()
{
    return (isViewInWindow() && ![m_webView _isBackground] && [m_webView _contentViewIsFirstResponder]) || [m_webView _isRetainingActiveFocusedState];
}

bool PageClientImpl::isViewVisible()
{
    if (isViewInWindow() && ![m_webView _isBackground])
        return true;
    
    if ([m_webView _isShowingVideoPictureInPicture])
        return true;
    
    if ([m_webView _mayAutomaticallyShowVideoPictureInPicture])
        return true;
    
    return false;
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
    if (auto webView = m_webView.get())
        return [webView window];

    return [m_contentView window];
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
    [m_contentView _processDidExit];
    [m_webView _processDidExit];
}

void PageClientImpl::processWillSwap()
{
    [m_contentView _processWillSwap];
    [m_webView _processWillSwap];
}

void PageClientImpl::didRelaunchProcess()
{
    [m_contentView _didRelaunchProcess];
    [m_webView _didRelaunchProcess];
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void PageClientImpl::didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID)
{
    [m_contentView _webProcessDidCreateContextForVisibilityPropagation];
}

#if ENABLE(GPU_PROCESS)
void PageClientImpl::didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID)
{
    [m_contentView _gpuProcessDidCreateContextForVisibilityPropagation];
}
#endif // ENABLE(GPU_PROCESS)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(GPU_PROCESS)
void PageClientImpl::gpuProcessDidExit()
{
    [m_contentView _gpuProcessDidExit];
    PageClientImplCocoa::gpuProcessDidExit();
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
    [m_contentView _didNotHandleTapAsClick:point];
}

void PageClientImpl::didCompleteSyntheticClick()
{
    [m_contentView _didCompleteSyntheticClick];
}

void PageClientImpl::decidePolicyForGeolocationPermissionRequest(WebFrameProxy& frame, const FrameInfoData& frameInfo, Function<void(bool)>& completionHandler)
{
    [[wrapper(m_webView.get()->_page->process().processPool()) _geolocationProvider] decidePolicyForGeolocationRequestFromOrigin:FrameInfoData { frameInfo } completionHandler:std::exchange(completionHandler, nullptr) view:m_webView.get().get()];
}

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    [m_webView _didStartProvisionalLoadForMainFrame];
    [m_contentView _didStartProvisionalLoadForMainFrame];
    [m_webView _hidePasswordView];
}

void PageClientImpl::didFailProvisionalLoadForMainFrame()
{
    [m_webView _hidePasswordView];
}

void PageClientImpl::didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider)
{
    [m_webView _hidePasswordView];
    [m_webView _setHasCustomContentView:useCustomContentProvider loadedMIMEType:mimeType];
    [m_contentView _didCommitLoadForMainFrame];
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize&)
{
    notImplemented();
}

void PageClientImpl::disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier requestID)
{
    [m_contentView _disableDoubleTapGesturesDuringTapIfNecessary:requestID];
}

void PageClientImpl::handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier requestID, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel)
{
    [m_contentView _handleSmartMagnificationInformationForPotentialTap:requestID renderRect:renderRect fitEntireRect:fitEntireRect viewportMinimumScale:viewportMinimumScale viewportMaximumScale:viewportMaximumScale nodeIsRootLevel:nodeIsRootLevel];
}

double PageClientImpl::minimumZoomScale() const
{
    if (UIScrollView *scroller = [m_webView scrollView])
        return scroller.minimumZoomScale;

    return 1;
}

WebCore::FloatRect PageClientImpl::documentRect() const
{
    return [m_contentView bounds];
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
    
    NSUndoManager *undoManager = [m_contentView undoManagerForWebView];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == UndoOrRedo::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:(NSString *)actionName];
}

void PageClientImpl::clearAllEditCommands()
{
    [[m_contentView undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == UndoOrRedo::Undo) ? [[m_contentView undoManager] canUndo] : [[m_contentView undoManager] canRedo];
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == UndoOrRedo::Undo) ? [[m_contentView undoManager] undo] : [[m_contentView undoManager] redo];
}

void PageClientImpl::accessibilityWebProcessTokenReceived(const IPC::DataReference& data)
{
    NSData *remoteToken = [NSData dataWithBytes:data.data() length:data.size()];
    [m_contentView _setAccessibilityWebProcessToken:remoteToken];
}

bool PageClientImpl::interpretKeyEvent(const NativeWebKeyboardEvent& event, bool isCharEvent)
{
    return [m_contentView _interpretKeyEvent:event.nativeEvent() isCharEvent:isCharEvent];
}

void PageClientImpl::positionInformationDidChange(const InteractionInformationAtPosition& info)
{
    [m_contentView _positionInformationDidChange:info];
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
    [m_contentView _selectionChanged];
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
    [m_contentView becomeFirstResponder];
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
    return IntPoint([m_contentView convertPoint:point fromView:nil]);
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    return enclosingIntRect([m_contentView convertRect:rect toView:nil]);
}
    
IntPoint PageClientImpl::accessibilityScreenToRootView(const IntPoint& point)
{
    CGPoint rootViewPoint = point;
    if ([m_contentView respondsToSelector:@selector(accessibilityConvertPointFromSceneReferenceCoordinates:)])
        rootViewPoint = [m_contentView accessibilityConvertPointFromSceneReferenceCoordinates:rootViewPoint];
    return IntPoint(rootViewPoint);
}
    
IntRect PageClientImpl::rootViewToAccessibilityScreen(const IntRect& rect)
{
    CGRect rootViewRect = rect;
    if ([m_contentView respondsToSelector:@selector(accessibilityConvertRectToSceneReferenceCoordinates:)])
        rootViewRect = [m_contentView accessibilityConvertRectToSceneReferenceCoordinates:rootViewRect];
    return enclosingIntRect(rootViewRect);
}
    
void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool eventWasHandled)
{
    [m_contentView _didHandleKeyEvent:event.nativeEvent() eventWasHandled:eventWasHandled];
}

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& nativeWebTouchEvent, bool eventHandled)
{
    [m_contentView _webTouchEvent:nativeWebTouchEvent preventsNativeGestures:eventHandled];
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)

void PageClientImpl::doneDeferringTouchStart(bool preventNativeGestures)
{
    [m_contentView _doneDeferringTouchStart:preventNativeGestures];
}

void PageClientImpl::doneDeferringTouchMove(bool preventNativeGestures)
{
    [m_contentView _doneDeferringTouchMove:preventNativeGestures];
}

void PageClientImpl::doneDeferringTouchEnd(bool preventNativeGestures)
{
    [m_contentView _doneDeferringTouchEnd:preventNativeGestures];
}

#endif // ENABLE(IOS_TOUCH_EVENTS)

#if ENABLE(IMAGE_ANALYSIS)

void PageClientImpl::requestTextRecognition(const URL& imageURL, const ShareableBitmapHandle& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completion)
{
    [m_contentView requestTextRecognition:imageURL imageData:imageData sourceLanguageIdentifier:sourceLanguageIdentifier targetLanguageIdentifier:targetLanguageIdentifier completionHandler:WTFMove(completion)];
}

#endif // ENABLE(IMAGE_ANALYSIS)

WebCore::DataOwnerType PageClientImpl::dataOwnerForPasteboard(PasteboardAccessIntent intent) const
{
    return [m_contentView _dataOwnerForPasteboard:intent];
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy&)
{
    return nullptr;
}

void PageClientImpl::setTextIndicator(Ref<TextIndicator> textIndicator, WebCore::TextIndicatorLifetime)
{
    [m_contentView setUpTextIndicator:textIndicator];
}

void PageClientImpl::clearTextIndicator(WebCore::TextIndicatorDismissalAnimation dismissalAnimation)
{
    [m_contentView clearTextIndicator:dismissalAnimation];
}

void PageClientImpl::setTextIndicatorAnimationProgress(float animationProgress)
{
    [m_contentView setTextIndicatorAnimationProgress:animationProgress];
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
}

void PageClientImpl::makeViewBlank(bool makeBlank)
{
    [m_contentView layer].opacity = makeBlank ? 0 : 1;
}

void PageClientImpl::showSafeBrowsingWarning(const SafeBrowsingWarning& warning, CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&& completionHandler)
{
    if (auto webView = m_webView.get())
        [webView _showSafeBrowsingWarning:warning completionHandler:WTFMove(completionHandler)];
    else
        completionHandler(ContinueUnsafeLoad::No);
}

void PageClientImpl::clearSafeBrowsingWarning()
{
    [m_webView _clearSafeBrowsingWarning];
}

void PageClientImpl::clearSafeBrowsingWarningIfForMainFrameNavigation()
{
    [m_webView _clearSafeBrowsingWarningIfForMainFrameNavigation];
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
    return [m_webView _effectiveAppearanceIsDark];
}

bool PageClientImpl::effectiveUserInterfaceLevelIsElevated() const
{
    return [m_webView _effectiveUserInterfaceLevelIsElevated];
}

void PageClientImpl::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    [m_contentView _setAcceleratedCompositingRootView:rootNode ? rootNode->uiView() : nil];
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    notImplemented();
    return nullptr;
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&&)
{
    return [m_webView _takeViewSnapshot];
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    notImplemented();
}

void PageClientImpl::commitPotentialTapFailed()
{
    [m_contentView _commitPotentialTapFailed];
}

void PageClientImpl::didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling)
{
    [m_contentView _didGetTapHighlightForRequest:requestID color:color quads:highlightedQuads topLeftRadius:topLeftRadius topRightRadius:topRightRadius bottomLeftRadius:bottomLeftRadius bottomRightRadius:bottomRightRadius nodeHasBuiltInClickHandling:nodeHasBuiltInClickHandling];
}

void PageClientImpl::didCommitLayerTree(const RemoteLayerTreeTransaction& layerTreeTransaction)
{
    [m_contentView _didCommitLayerTree:layerTreeTransaction];
}

void PageClientImpl::layerTreeCommitComplete()
{
    [m_contentView _layerTreeCommitComplete];
}

void PageClientImpl::couldNotRestorePageState()
{
    [m_webView _couldNotRestorePageState];
}

void PageClientImpl::restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale)
{
    [m_webView _restorePageScrollPosition:scrollPosition scrollOrigin:scrollOrigin previousObscuredInset:obscuredInsetsOnSave scale:scale];
}

void PageClientImpl::restorePageCenterAndScale(std::optional<WebCore::FloatPoint> center, double scale)
{
    [m_webView _restorePageStateToUnobscuredCenter:center scale:scale];
}

void PageClientImpl::elementDidFocus(const FocusedElementInformation& nodeInformation, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState::Flag> activityStateChanges, API::Object* userData)
{
    MESSAGE_CHECK(!userData || userData->type() == API::Object::Type::Data);

    NSObject <NSSecureCoding> *userObject = nil;
    if (API::Data* data = static_cast<API::Data*>(userData)) {
        auto nsData = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<unsigned char*>(data->bytes()) length:data->size() freeWhenDone:NO]);
        auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:nsData.get() error:nullptr]);
        unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
        @try {
            auto* allowedClasses = m_webView.get()->_page->process().processPool().allowedClassesForParameterCoding();
            userObject = [unarchiver decodeObjectOfClasses:allowedClasses forKey:@"userObject"];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to decode user data: %@", exception);
        }
    }

    [m_contentView _elementDidFocus:nodeInformation userIsInteracting:userIsInteracting blurPreviousNode:blurPreviousNode activityStateChanges:activityStateChanges userObject:userObject];
}

void PageClientImpl::updateInputContextAfterBlurringAndRefocusingElement()
{
    [m_contentView _updateInputContextAfterBlurringAndRefocusingElement];
}

bool PageClientImpl::isFocusingElement()
{
    return [m_contentView isFocusingElement];
}

void PageClientImpl::elementDidBlur()
{
    [m_contentView _elementDidBlur];
}

void PageClientImpl::focusedElementDidChangeInputMode(WebCore::InputMode mode)
{
    [m_contentView _didUpdateInputMode:mode];
}

void PageClientImpl::didUpdateEditorState()
{
    [m_contentView _didUpdateEditorState];
}

void PageClientImpl::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect, WebCore::RouteSharingPolicy policy, const String& contextUID)
{
    [m_contentView _showPlaybackTargetPicker:hasVideo fromRect:elementRect routeSharingPolicy:policy routingContextUID:contextUID];
}

bool PageClientImpl::handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, const FrameInfoData& frameInfo, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
{
    [m_contentView _showRunOpenPanel:parameters frameInfo:frameInfo resultListener:listener];
    return true;
}

bool PageClientImpl::showShareSheet(const ShareDataWithParsedURL& shareData, WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    [m_contentView _showShareSheet:shareData inRect:std::nullopt completionHandler:WTFMove(completionHandler)];
    return true;
}

void PageClientImpl::showContactPicker(const WebCore::ContactsRequestData& requestData, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& completionHandler)
{
    [m_contentView _showContactPicker:requestData completionHandler:WTFMove(completionHandler)];
}

void PageClientImpl::showInspectorHighlight(const WebCore::InspectorOverlay::Highlight& highlight)
{
    [m_contentView _showInspectorHighlight:highlight];
}

void PageClientImpl::hideInspectorHighlight()
{
    [m_contentView _hideInspectorHighlight];
}

void PageClientImpl::showInspectorIndication()
{
    [m_contentView setShowingInspectorIndication:YES];
}

void PageClientImpl::hideInspectorIndication()
{
    [m_contentView setShowingInspectorIndication:NO];
}

void PageClientImpl::enableInspectorNodeSearch()
{
    [m_contentView _enableInspectorNodeSearch];
}

void PageClientImpl::disableInspectorNodeSearch()
{
    [m_contentView _disableInspectorNodeSearch];
}

#if ENABLE(FULLSCREEN_API)

WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

// WebFullScreenManagerProxyClient

void PageClientImpl::closeFullScreenManager()
{
    [m_webView closeFullScreenWindowController];
}

bool PageClientImpl::isFullScreen()
{
    if (![m_webView hasFullScreenWindowController])
        return false;

    return [m_webView fullScreenWindowController].isFullScreen;
}

void PageClientImpl::enterFullScreen(FloatSize videoDimensions)
{
    [[m_webView fullScreenWindowController] enterFullScreen:videoDimensions];
}

void PageClientImpl::exitFullScreen()
{
    [[m_webView fullScreenWindowController] exitFullScreen];
}

static UIInterfaceOrientationMask toUIInterfaceOrientationMask(WebCore::ScreenOrientationType orientation)
{
    switch (orientation) {
    case WebCore::ScreenOrientationType::PortraitPrimary:
        return UIInterfaceOrientationMaskPortrait;
    case WebCore::ScreenOrientationType::PortraitSecondary:
        return UIInterfaceOrientationMaskPortraitUpsideDown;
    case WebCore::ScreenOrientationType::LandscapePrimary:
        return UIInterfaceOrientationMaskLandscapeLeft;
    case WebCore::ScreenOrientationType::LandscapeSecondary:
        return UIInterfaceOrientationMaskLandscapeRight;
    }
    ASSERT_NOT_REACHED();
    return UIInterfaceOrientationMaskPortrait;
}

bool PageClientImpl::lockFullscreenOrientation(WebCore::ScreenOrientationType orientation)
{
    [[m_webView fullScreenWindowController] setSupportedOrientations:toUIInterfaceOrientationMask(orientation)];
    return true;
}

void PageClientImpl::unlockFullscreenOrientation()
{
    [[m_webView fullScreenWindowController] resetSupportedOrientations];
}

void PageClientImpl::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [[m_webView fullScreenWindowController] beganEnterFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
}

void PageClientImpl::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [[m_webView fullScreenWindowController] beganExitFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
    RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytes:dataReference.data() length:dataReference.size()]);
    [m_webView _didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:suggestedFilename data:data.get()];
}

void PageClientImpl::scrollingNodeScrollViewWillStartPanGesture(ScrollingNodeID)
{
    [m_contentView scrollViewWillStartPanOrPinchGesture];
}

void PageClientImpl::scrollingNodeScrollViewDidScroll(ScrollingNodeID)
{
    [m_contentView _didScroll];
}

void PageClientImpl::scrollingNodeScrollWillStartScroll(ScrollingNodeID nodeID)
{
    [m_contentView _scrollingNodeScrollingWillBegin:nodeID];
}

void PageClientImpl::scrollingNodeScrollDidEndScroll(ScrollingNodeID nodeID)
{
    [m_contentView _scrollingNodeScrollingDidEnd:nodeID];
}

Vector<String> PageClientImpl::mimeTypesWithCustomContentProviders()
{
    return [m_webView _contentProviderRegistry]._mimeTypesWithCustomContentProviders;
}

void PageClientImpl::navigationGestureDidBegin()
{
    [m_webView _navigationGestureDidBegin];
    NavigationState::fromWebPage(*m_webView.get()->_page).navigationGestureDidBegin();
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView.get()->_page).navigationGestureWillEnd(willNavigate, item);
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView.get()->_page).navigationGestureDidEnd(willNavigate, item);
    [m_webView _navigationGestureDidEnd];
}

void PageClientImpl::navigationGestureDidEnd()
{
    [m_webView _navigationGestureDidEnd];
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView.get()->_page).willRecordNavigationSnapshot(item);
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    NavigationState::fromWebPage(*m_webView.get()->_page).navigationGestureSnapshotWasRemoved();
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::didFinishNavigation(API::Navigation* navigation)
{
    [m_webView _didFinishNavigation:navigation];
}

void PageClientImpl::didFailNavigation(API::Navigation* navigation)
{
    [m_webView _didFailNavigation:navigation];
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType navigationType)
{
    [m_webView _didSameDocumentNavigationForMainFrame:navigationType];
}

void PageClientImpl::didChangeBackgroundColor()
{
    [m_webView _updateScrollViewBackground];
}

void PageClientImpl::videoControlsManagerDidChange()
{
    [m_webView _videoControlsManagerDidChange];
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
    if (!m_webView)
        return WebCore::UserInterfaceLayoutDirection::LTR;
    return ([UIView userInterfaceLayoutDirectionForSemanticContentAttribute:[m_webView semanticContentAttribute]] == UIUserInterfaceLayoutDirectionLeftToRight) ? WebCore::UserInterfaceLayoutDirection::LTR : WebCore::UserInterfaceLayoutDirection::RTL;
}

Ref<ValidationBubble> PageClientImpl::createValidationBubble(const String& message, const ValidationBubble::Settings& settings)
{
    return ValidationBubble::create(m_contentView.getAutoreleased(), message, settings);
}

#if ENABLE(INPUT_TYPE_COLOR)
RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&, Vector<WebCore::Color>&&)
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
    [m_contentView _didPerformDragOperation:handled];
}

void PageClientImpl::didHandleDragStartRequest(bool started)
{
    [m_contentView _didHandleDragStartRequest:started];
}

void PageClientImpl::didHandleAdditionalDragItemsRequest(bool added)
{
    [m_contentView _didHandleAdditionalDragItemsRequest:added];
}

void PageClientImpl::startDrag(const DragItem& item, const ShareableBitmapHandle& image)
{
    auto bitmap = ShareableBitmap::create(image);
    if (!bitmap)
        return;
    [m_contentView _startDrag:bitmap->makeCGImageCopy() item:item];
}

void PageClientImpl::willReceiveEditDragSnapshot()
{
    [m_contentView _willReceiveEditDragSnapshot];
}

void PageClientImpl::didReceiveEditDragSnapshot(std::optional<TextIndicatorData> data)
{
    [m_contentView _didReceiveEditDragSnapshot:data];
}

void PageClientImpl::didChangeDragCaretRect(const IntRect& previousCaretRect, const IntRect& caretRect)
{
    [m_contentView _didChangeDragCaretRect:previousCaretRect currentRect:caretRect];
}
#endif

#if USE(QUICK_LOOK)
void PageClientImpl::requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&& completionHandler)
{
    auto passwordHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSString *password) {
        completionHandler(password);
    });

    if (WKPasswordView *passwordView = [m_webView _passwordView]) {
        ASSERT(fileName == String { passwordView.documentName });
        [passwordView showPasswordFailureAlert];
        passwordView.userDidEnterPassword = passwordHandler.get();
        return;
    }

    [m_webView _showPasswordViewWithDocumentName:fileName passwordHandler:passwordHandler.get()];
    NavigationState::fromWebPage(*m_webView.get()->_page).didRequestPasswordForQuickLookDocument();
}
#endif

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    [m_contentView _requestDOMPasteAccessForCategory:pasteAccessCategory elementRect:elementRect originIdentifier:originIdentifier completionHandler:WTFMove(completionHandler)];
}

void PageClientImpl::cancelPointersForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    [m_contentView cancelPointersForGestureRecognizer:gestureRecognizer];
}

std::optional<unsigned> PageClientImpl::activeTouchIdentifierForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    return [m_contentView activeTouchIdentifierForGestureRecognizer:gestureRecognizer];
}

void PageClientImpl::handleAutocorrectionContext(const WebAutocorrectionContext& context)
{
    [m_contentView _handleAutocorrectionContext:context];
}

void PageClientImpl::showDictationAlternativeUI(const WebCore::FloatRect&, WebCore::DictationContext)
{
    notImplemented();
}

void PageClientImpl::showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition& positionInformation)
{
    [m_contentView _showDataDetectorsUIForPositionInformation:positionInformation];
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void PageClientImpl::didEnterFullscreen()
{
    [m_contentView _didEnterFullscreen];
}

void PageClientImpl::didExitFullscreen()
{
    [m_contentView _didExitFullscreen];
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#if ENABLE(ATTACHMENT_ELEMENT)

void PageClientImpl::writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&& info)
{
    [m_contentView _writePromisedAttachmentToPasteboard:WTFMove(info)];
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

void PageClientImpl::setMouseEventPolicy(WebCore::MouseEventPolicy policy)
{
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [m_contentView _setMouseEventPolicy:policy];
#endif
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
void PageClientImpl::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    [m_contentView _showMediaControlsContextMenu:WTFMove(targetFrame) items:WTFMove(items) completionHandler:WTFMove(completionHandler)];
}
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
void PageClientImpl::handleAsynchronousCancelableScrollEvent(UIScrollView *scrollView, UIScrollEvent *scrollEvent, void (^completion)(BOOL handled))
{
    [m_webView _scrollView:scrollView asynchronouslyHandleScrollEvent:scrollEvent completion:completion];
}
#endif

void PageClientImpl::runModalJavaScriptDialog(CompletionHandler<void()>&& callback)
{
    [m_contentView runModalJavaScriptDialog:WTFMove(callback)];
}

WebCore::Color PageClientImpl::contentViewBackgroundColor()
{
    WebCore::Color color;
    auto computeContentViewBackgroundColor = [&]() {
        color = WebCore::roundAndClampToSRGBALossy([m_contentView backgroundColor].CGColor);
        if (color.isValid())
            return;

#if HAVE(OS_DARK_MODE_SUPPORT)
        color = WebCore::roundAndClampToSRGBALossy(UIColor.systemBackgroundColor.CGColor);
#else
        color = { };
#endif
    };

#if HAVE(OS_DARK_MODE_SUPPORT)
    [[m_webView traitCollection] performAsCurrentTraitCollection:computeContentViewBackgroundColor];
#else
    computeContentViewBackgroundColor();
#endif

    return color;
}

void PageClientImpl::requestScrollToRect(const FloatRect& targetRect, const FloatPoint& origin)
{
    [m_contentView _scrollToRect:targetRect withOrigin:origin minimumScrollDistance:0];
}

String PageClientImpl::sceneID()
{
    return [m_contentView window].windowScene._sceneIdentifier;
}

void PageClientImpl::beginTextRecognitionForFullscreenVideo(const ShareableBitmapHandle& imageHandle, AVPlayerViewController *playerViewController)
{
    [m_contentView beginTextRecognitionForFullscreenVideo:imageHandle playerViewController:playerViewController];
}

void PageClientImpl::cancelTextRecognitionForFullscreenVideo(AVPlayerViewController *controller)
{
    [m_contentView cancelTextRecognitionForFullscreenVideo:controller];
}

bool PageClientImpl::isTextRecognitionInFullscreenVideoEnabled() const
{
    return [m_contentView isTextRecognitionInFullscreenVideoEnabled];
}

#if ENABLE(VIDEO)
void PageClientImpl::beginTextRecognitionForVideoInElementFullscreen(const ShareableBitmapHandle& bitmapHandle, FloatRect bounds)
{
    [m_contentView beginTextRecognitionForVideoInElementFullscreen:bitmapHandle bounds:bounds];
}

void PageClientImpl::cancelTextRecognitionForVideoInElementFullscreen()
{
    [m_contentView cancelTextRecognitionForVideoInElementFullscreen];
}
#endif

bool PageClientImpl::hasResizableWindows() const
{
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    return [m_webView _isWindowResizingEnabled];
#else
    return false;
#endif
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)

#undef MESSAGE_CHECK
