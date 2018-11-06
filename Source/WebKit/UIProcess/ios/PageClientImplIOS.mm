/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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
#import "DataReference.h"
#import "DownloadProxy.h"
#import "DrawingAreaProxy.h"
#import "InteractionInformationAtPosition.h"
#import "NativeWebKeyboardEvent.h"
#import "NavigationState.h"
#import "StringUtilities.h"
#import "UIKitSPI.h"
#import "UndoOrRedo.h"
#import "ViewSnapshotStore.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKGeolocationProviderIOS.h"
#import "WKPasswordView.h"
#import "WKProcessPoolInternal.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewInternal.h"
#import "WebContextMenuProxy.h"
#import "WebDataListSuggestionsDropdownIOS.h"
#import "WebEditCommandProxy.h"
#import "WebProcessProxy.h"
#import "_WKDownloadInternal.h"
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/ShareData.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/BlockPtr.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_webView->_page->process().connection())

@interface WKEditCommandObjC : NSObject
{
    RefPtr<WebKit::WebEditCommandProxy> m_command;
}
- (id)initWithWebEditCommandProxy:(Ref<WebKit::WebEditCommandProxy>&&)command;
- (WebKit::WebEditCommandProxy*)command;
@end

@interface WKEditorUndoTargetObjC : NSObject
- (void)undoEditing:(id)sender;
- (void)redoEditing:(id)sender;
@end

@implementation WKEditCommandObjC

- (id)initWithWebEditCommandProxy:(Ref<WebKit::WebEditCommandProxy>&&)command
{
    self = [super init];
    if (!self)
        return nil;
    
    m_command = WTFMove(command);
    return self;
}

- (WebKit::WebEditCommandProxy *)command
{
    return m_command.get();
}

@end

@implementation WKEditorUndoTargetObjC

- (void)undoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->unapply();
}

- (void)redoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->reapply();
}

@end

namespace WebKit {
using namespace WebCore;

PageClientImpl::PageClientImpl(WKContentView *contentView, WKWebView *webView)
    : PageClientImplCocoa(webView)
    , m_contentView(contentView)
    , m_undoTarget(adoptNS([[WKEditorUndoTargetObjC alloc] init]))
{
}

PageClientImpl::~PageClientImpl()
{
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return [m_contentView _createDrawingAreaProxy];
}

void PageClientImpl::setViewNeedsDisplay(const Region&)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, bool isProgrammaticScroll)
{
    UNUSED_PARAM(isProgrammaticScroll);
    [m_webView _scrollToContentScrollPosition:scrollPosition scrollOrigin:scrollOrigin];
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    if (UIScrollView *scroller = [m_contentView _scroller])
        return scroller.contentOffset;

    return { };
}

IntSize PageClientImpl::viewSize()
{
    if (UIScrollView *scroller = [m_contentView _scroller])
        return IntSize(scroller.bounds.size);

    return IntSize(m_contentView.bounds.size);
}

bool PageClientImpl::isViewWindowActive()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=133098
    return isViewVisible() || (m_webView && [m_webView _isRetainingActiveFocusedState]);
}

bool PageClientImpl::isViewFocused()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=133098
    return isViewWindowActive() || (m_webView && [m_webView _isRetainingActiveFocusedState]);
}

bool PageClientImpl::isViewVisible()
{
    if (isViewInWindow() && !m_webView._isBackground)
        return true;
    
    if ([m_webView _isShowingVideoPictureInPicture])
        return true;
    
    if ([m_webView _mayAutomaticallyShowVideoPictureInPicture])
        return true;
    
    return false;
}

bool PageClientImpl::isViewInWindow()
{
    // FIXME: in WebKitTestRunner, m_webView is nil, so check the content view instead.
    if (m_webView)
        return [m_webView window];

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

void PageClientImpl::pageClosed()
{
    notImplemented();
}

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

void PageClientImpl::decidePolicyForGeolocationPermissionRequest(WebFrameProxy& frame, API::SecurityOrigin& origin, Function<void(bool)>& completionHandler)
{
    [[wrapper(m_webView->_page->process().processPool()) _geolocationProvider] decidePolicyForGeolocationRequestFromOrigin:origin.securityOrigin() frame:frame completionHandler:std::exchange(completionHandler, nullptr) view:m_webView];
}

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    [m_webView _didStartProvisionalLoadForMainFrame];
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

void PageClientImpl::handleDownloadRequest(DownloadProxy*)
{
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize&)
{
    notImplemented();
}

void PageClientImpl::disableDoubleTapGesturesDuringTapIfNecessary(uint64_t requestID)
{
    [m_contentView _disableDoubleTapGesturesDuringTapIfNecessary:requestID];
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

void PageClientImpl::setCursor(const Cursor&)
{
    notImplemented();
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
    RetainPtr<WKEditCommandObjC> commandObjC = adoptNS([[WKEditCommandObjC alloc] initWithWebEditCommandProxy:command.copyRef()]);
    String actionName = WebEditCommandProxy::nameForEditAction(command->editAction());
    
    NSUndoManager *undoManager = [m_contentView undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == UndoOrRedo::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:(NSString *)actionName];
}

#if USE(INSERTION_UNDO_GROUPING)
void PageClientImpl::registerInsertionUndoGrouping()
{
    notImplemented();
}
#endif

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
    notImplemented();
}

void PageClientImpl::makeFirstResponder()
{
    notImplemented();
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& rect)
{
    notImplemented();
    return FloatRect();
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& rect)
{
    notImplemented();
    return FloatRect();
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
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& nativeWebtouchEvent, bool eventHandled)
{
    [m_contentView _webTouchEvent:nativeWebtouchEvent preventsNativeGestures:eventHandled];
}
#endif

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy&)
{
    return nullptr;
}

void PageClientImpl::setTextIndicator(Ref<TextIndicator> textIndicator, TextIndicatorWindowLifetime)
{
}

void PageClientImpl::clearTextIndicator(TextIndicatorWindowDismissalAnimation)
{
}

void PageClientImpl::setTextIndicatorAnimationProgress(float)
{
}

void PageClientImpl::showSafeBrowsingWarning(const SafeBrowsingResult&, CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, WebCore::URL>&&)>&& completionHandler)
{
    completionHandler(WebKit::ContinueUnsafeLoad::Yes); // FIXME: Implement.
}

void PageClientImpl::clearSafeBrowsingWarning()
{
    // FIXME: Implement.
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext&)
{
}

void PageClientImpl::setAcceleratedCompositingRootLayer(LayerOrView *rootLayer)
{
    [m_contentView _setAcceleratedCompositingRootView:rootLayer];
}

LayerOrView *PageClientImpl::acceleratedCompositingRootLayer() const
{
    notImplemented();
    return nullptr;
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot()
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

void PageClientImpl::didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius)
{
    [m_contentView _didGetTapHighlightForRequest:requestID color:color quads:highlightedQuads topLeftRadius:topLeftRadius topRightRadius:topRightRadius bottomLeftRadius:bottomLeftRadius bottomRightRadius:bottomRightRadius];
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

void PageClientImpl::startAssistingNode(const AssistedNodeInformation& nodeInformation, bool userIsInteracting, bool blurPreviousNode, bool changingActivityState, API::Object* userData)
{
    MESSAGE_CHECK(!userData || userData->type() == API::Object::Type::Data);

    NSObject <NSSecureCoding> *userObject = nil;
    if (API::Data* data = static_cast<API::Data*>(userData)) {
        auto nsData = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(data->bytes())) length:data->size() freeWhenDone:NO]);
        auto unarchiver = secureUnarchiverFromData(nsData.get());
        @try {
            userObject = [unarchiver decodeObjectOfClass:[NSObject class] forKey:@"userObject"];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to decode user data: %@", exception);
        }
    }

    [m_contentView _startAssistingNode:nodeInformation userIsInteracting:userIsInteracting blurPreviousNode:blurPreviousNode changingActivityState:changingActivityState userObject:userObject];
}

bool PageClientImpl::isAssistingNode()
{
    return [m_contentView isAssistingNode];
}

void PageClientImpl::stopAssistingNode()
{
    [m_contentView _stopAssistingNode];
}

void PageClientImpl::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect, WebCore::RouteSharingPolicy policy, const String& contextUID)
{
    [m_contentView _showPlaybackTargetPicker:hasVideo fromRect:elementRect routeSharingPolicy:policy routingContextUID:contextUID];
}

bool PageClientImpl::handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
{
    [m_contentView _showRunOpenPanel:parameters resultListener:listener];
    return true;
}

bool PageClientImpl::showShareSheet(const ShareDataWithParsedURL& shareData, WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    [m_contentView _showShareSheet:shareData completionHandler:WTFMove(completionHandler)];
    return true;
}

void PageClientImpl::showInspectorHighlight(const WebCore::Highlight& highlight)
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

void PageClientImpl::enterFullScreen()
{
    [[m_webView fullScreenWindowController] enterFullScreen];
}

void PageClientImpl::exitFullScreen()
{
    [[m_webView fullScreenWindowController] exitFullScreen];
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

void PageClientImpl::scrollingNodeScrollViewWillStartPanGesture()
{
    [m_contentView scrollViewWillStartPanOrPinchGesture];
}

void PageClientImpl::scrollingNodeScrollViewDidScroll()
{
    [m_contentView _didScroll];
}

void PageClientImpl::scrollingNodeScrollWillStartScroll()
{
    [m_contentView _scrollingNodeScrollingWillBegin];
}

void PageClientImpl::scrollingNodeScrollDidEndScroll()
{
    [m_contentView _scrollingNodeScrollingDidEnd];
}

Vector<String> PageClientImpl::mimeTypesWithCustomContentProviders()
{
    return m_webView._contentProviderRegistry._mimeTypesWithCustomContentProviders;
}

void PageClientImpl::navigationGestureDidBegin()
{
    [m_webView _navigationGestureDidBegin];
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureDidBegin();
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureWillEnd(willNavigate, item);
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureDidEnd(willNavigate, item);
    [m_webView _navigationGestureDidEnd];
}

void PageClientImpl::navigationGestureDidEnd()
{
    [m_webView _navigationGestureDidEnd];
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView->_page).willRecordNavigationSnapshot(item);
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureSnapshotWasRemoved();
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::didFinishLoadForMainFrame()
{
    [m_webView _didFinishLoadForMainFrame];
}

void PageClientImpl::didFailLoadForMainFrame()
{
    [m_webView _didFailLoadForMainFrame];
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
    return ValidationBubble::create(m_contentView, message, settings);
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
    return WebDataListSuggestionsDropdownIOS::create(page, m_contentView);
}
#endif

#if ENABLE(DATA_INTERACTION)
void PageClientImpl::didPerformDragOperation(bool handled)
{
    [m_contentView _didPerformDragOperation:handled];
}

void PageClientImpl::didHandleStartDataInteractionRequest(bool started)
{
    [m_contentView _didHandleStartDataInteractionRequest:started];
}

void PageClientImpl::didHandleAdditionalDragItemsRequest(bool added)
{
    [m_contentView _didHandleAdditionalDragItemsRequest:added];
}

void PageClientImpl::startDrag(const DragItem& item, const ShareableBitmap::Handle& image)
{
    [m_contentView _startDrag:ShareableBitmap::create(image)->makeCGImageCopy() item:item];
}

void PageClientImpl::didConcludeEditDataInteraction(std::optional<TextIndicatorData> data)
{
    [m_contentView _didConcludeEditDataInteraction:data];
}

void PageClientImpl::didChangeDataInteractionCaretRect(const IntRect& previousCaretRect, const IntRect& caretRect)
{
    [m_contentView _didChangeDataInteractionCaretRect:previousCaretRect currentRect:caretRect];
}
#endif

#if USE(QUICK_LOOK)
void PageClientImpl::requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&& completionHandler)
{
    auto passwordHandler = BlockPtr<void (NSString *)>::fromCallable([completionHandler = WTFMove(completionHandler)](NSString *password) {
        completionHandler(password);
    });

    if (WKPasswordView *passwordView = m_webView._passwordView) {
        ASSERT(fileName == String { passwordView.documentName });
        [passwordView showPasswordFailureAlert];
        passwordView.userDidEnterPassword = passwordHandler.get();
        return;
    }

    [m_webView _showPasswordViewWithDocumentName:fileName passwordHandler:passwordHandler.get()];
    NavigationState::fromWebPage(*m_webView->_page).didRequestPasswordForQuickLookDocument();
}
#endif

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)

#undef MESSAGE_CHECK
