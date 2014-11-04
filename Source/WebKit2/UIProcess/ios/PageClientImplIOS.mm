/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "APIData.h"
#import "DataReference.h"
#import "DownloadProxy.h"
#import "InteractionInformationAtPosition.h"
#import "NativeWebKeyboardEvent.h"
#import "NavigationState.h"
#import "TextIndicator.h"
#import "ViewSnapshotStore.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewInternal.h"
#import "WebContextMenuProxy.h"
#import "WebEditCommandProxy.h"
#import "WebProcessProxy.h"
#import "_WKDownloadInternal.h"
#import <UIKit/UIImagePickerController_Private.h>
#import <UIKit/UIWebTouchEventsGestureRecognizer.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/SharedBuffer.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_webView->_page->process().connection())

@interface UIView (IPI)
- (UIScrollView *)_scroller;
- (CGPoint)accessibilityConvertPointFromSceneReferenceCoordinates:(CGPoint)point;
- (CGRect)accessibilityConvertRectToSceneReferenceCoordinates:(CGRect)rect;
@end

using namespace WebCore;
using namespace WebKit;

@interface WKEditCommandObjC : NSObject
{
    RefPtr<WebEditCommandProxy> m_command;
}
- (id)initWithWebEditCommandProxy:(PassRefPtr<WebEditCommandProxy>)command;
- (WebEditCommandProxy*)command;
@end

@interface WKEditorUndoTargetObjC : NSObject
- (void)undoEditing:(id)sender;
- (void)redoEditing:(id)sender;
@end

@implementation WKEditCommandObjC

- (id)initWithWebEditCommandProxy:(PassRefPtr<WebEditCommandProxy>)command
{
    self = [super init];
    if (!self)
        return nil;
    
    m_command = command;
    return self;
}

- (WebEditCommandProxy *)command
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

PageClientImpl::PageClientImpl(WKContentView *contentView, WKWebView *webView)
    : m_contentView(contentView)
    , m_webView(webView)
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

void PageClientImpl::setViewNeedsDisplay(const IntRect& rect)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::displayView()
{
    ASSERT_NOT_REACHED();
}

bool PageClientImpl::canScrollView()
{
    notImplemented();
    return false;
}

void PageClientImpl::scrollView(const IntRect&, const IntSize&)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, bool isProgrammaticScroll)
{
    UNUSED_PARAM(isProgrammaticScroll);
    [m_webView _scrollToContentOffset:scrollPosition];
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
    return isViewVisible();
}

bool PageClientImpl::isViewFocused()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=133098
    return isViewWindowActive();
}

bool PageClientImpl::isViewVisible()
{
    return isViewInWindow() && !m_contentView.isBackground;
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

bool PageClientImpl::decidePolicyForGeolocationPermissionRequest(WebFrameProxy& frame, WebSecurityOrigin& origin, GeolocationPermissionRequestProxy& request)
{
    [m_contentView _decidePolicyForGeolocationRequestFromOrigin:origin frame:frame request:request];
    return true;
}

void PageClientImpl::didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider)
{
    [m_webView _setHasCustomContentView:useCustomContentProvider loadedMIMEType:mimeType];
    [m_contentView _didCommitLoadForMainFrame];
}

void PageClientImpl::handleDownloadRequest(DownloadProxy* download)
{
    ASSERT_ARG(download, download);
    ASSERT([download->wrapper() isKindOfClass:[_WKDownload class]]);
    [static_cast<_WKDownload *>(download->wrapper()) setOriginatingWebView:m_webView];
}

void PageClientImpl::didChangeViewportMetaTagWidth(float newWidth)
{
    [m_webView _setViewportMetaTagWidth:newWidth];
}

void PageClientImpl::setUsesMinimalUI(bool usesMinimalUI)
{
    [m_webView _setUsesMinimalUI:usesMinimalUI];
}

double PageClientImpl::minimumZoomScale() const
{
    if (UIScrollView *scroller = [m_webView scrollView])
        return scroller.minimumZoomScale;

    return 1;
}

WebCore::FloatSize PageClientImpl::contentsSize() const
{
    return FloatSize([m_contentView bounds].size);
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

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy> prpCommand, WebPageProxy::UndoOrRedo undoOrRedo)
{
    RefPtr<WebEditCommandProxy> command = prpCommand;
    
    RetainPtr<WKEditCommandObjC> commandObjC = adoptNS([[WKEditCommandObjC alloc] initWithWebEditCommandProxy:command]);
    String actionName = WebEditCommandProxy::nameForEditAction(command->editAction());
    
    NSUndoManager *undoManager = [m_contentView undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == WebPageProxy::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
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

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == WebPageProxy::Undo) ? [[m_contentView undoManager] canUndo] : [[m_contentView undoManager] canRedo];
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == WebPageProxy::Undo) ? [[m_contentView undoManager] undo] : [[m_contentView undoManager] redo];
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

void PageClientImpl::saveImageToLibrary(PassRefPtr<SharedBuffer> imageBuffer)
{
    RetainPtr<NSData> imageData = imageBuffer->createNSData();
    UIImageDataWriteToSavedPhotosAlbum(imageData.get(), nil, NULL, NULL);
}

bool PageClientImpl::executeSavedCommandBySelector(const String&)
{
    notImplemented();
    return false;
}

void PageClientImpl::setDragImage(const IntPoint&, PassRefPtr<ShareableBitmap>, bool)
{
    notImplemented();
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
    
void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool)
{
    [m_contentView _didHandleKeyEvent:event.nativeEvent()];
}

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& nativeWebtouchEvent, bool eventHandled)
{
    [m_contentView _webTouchEvent:nativeWebtouchEvent preventsNativeGestures:eventHandled];
}
#endif

PassRefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

PassRefPtr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

void PageClientImpl::setTextIndicator(PassRefPtr<TextIndicator> textIndicator, bool fadeOut, bool animate)
{
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

PassRefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot()
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

void PageClientImpl::dynamicViewportUpdateChangedTarget(double newScale, const WebCore::FloatPoint& newScrollPosition, uint64_t nextValidLayerTreeTransactionID)
{
    [m_webView _dynamicViewportUpdateChangedTargetToScale:newScale position:newScrollPosition nextValidLayerTreeTransactionID:nextValidLayerTreeTransactionID];
}

void PageClientImpl::restorePageState(const WebCore::FloatRect& exposedRect, double scale)
{
    [m_webView _restorePageStateToExposedRect:exposedRect scale:scale];
}

void PageClientImpl::restorePageCenterAndScale(const WebCore::FloatPoint& center, double scale)
{
    [m_webView _restorePageStateToUnobscuredCenter:center scale:scale];
}

void PageClientImpl::startAssistingNode(const AssistedNodeInformation& nodeInformation, bool userIsInteracting, bool blurPreviousNode, API::Object* userData)
{
    MESSAGE_CHECK(!userData || userData->type() == API::Object::Type::Data);

    NSObject <NSSecureCoding> *userObject = nil;
    if (API::Data* data = static_cast<API::Data*>(userData)) {
        auto nsData = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(data->bytes())) length:data->size() freeWhenDone:NO]);
        auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:nsData.get()]);
        [unarchiver setRequiresSecureCoding:YES];
        @try {
            userObject = [unarchiver decodeObjectOfClass:[NSObject class] forKey:@"userObject"];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to decode user data: %@", exception);
        }
    }

    [m_contentView _startAssistingNode:nodeInformation userIsInteracting:userIsInteracting blurPreviousNode:blurPreviousNode userObject:userObject];
}

bool PageClientImpl::isAssistingNode()
{
    return [m_contentView isAssistingNode];
}

void PageClientImpl::stopAssistingNode()
{
    [m_contentView _stopAssistingNode];
}

void PageClientImpl::didUpdateBlockSelectionWithTouch(uint32_t touch, uint32_t flags, float growThreshold, float shrinkThreshold)
{
    [m_contentView _didUpdateBlockSelectionWithTouch:(SelectionTouch)touch withFlags:(SelectionFlags)flags growThreshold:growThreshold shrinkThreshold:shrinkThreshold];
}

void PageClientImpl::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect)
{
    [m_contentView _showPlaybackTargetPicker:hasVideo fromRect:elementRect];
}

bool PageClientImpl::handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, WebOpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
{
    [m_contentView _showRunOpenPanel:parameters resultListener:listener];
    return true;
}

#if ENABLE(INSPECTOR)
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
#endif

#if ENABLE(FULLSCREEN_API)

WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

// WebFullScreenManagerProxyClient

void PageClientImpl::closeFullScreenManager()
{
}

bool PageClientImpl::isFullScreen()
{
    return false;
}

void PageClientImpl::enterFullScreen()
{
}

void PageClientImpl::exitFullScreen()
{
}

void PageClientImpl::beganEnterFullScreen(const IntRect&, const IntRect&)
{
}

void PageClientImpl::beganExitFullScreen(const IntRect&, const IntRect&)
{
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
    RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytes:dataReference.data() length:dataReference.size()]);
    [m_webView _didFinishLoadingDataForCustomContentProviderWithSuggestedFilename:suggestedFilename data:data.get()];
}

void PageClientImpl::zoomToRect(FloatRect rect, double minimumScale, double maximumScale)
{
    [m_contentView _zoomToRect:rect withOrigin:rect.center() fitEntireRect:YES minimumScale:minimumScale maximumScale:maximumScale minimumScrollDistance:0];
}

void PageClientImpl::overflowScrollViewWillStartPanGesture()
{
    [m_contentView scrollViewWillStartPanOrPinchGesture];
}

void PageClientImpl::overflowScrollViewDidScroll()
{
    [m_contentView _didScroll];
}

void PageClientImpl::overflowScrollWillStartScroll()
{
    [m_contentView _overflowScrollingWillBegin];
}

void PageClientImpl::overflowScrollDidEndScroll()
{
    [m_contentView _overflowScrollingDidEnd];
}

void PageClientImpl::didFinishDrawingPagesToPDF(const IPC::DataReference& pdfData)
{
    RetainPtr<CFDataRef> data = adoptCF(CFDataCreate(kCFAllocatorDefault, pdfData.data(), pdfData.size()));
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(data.get()));
    m_webView._printedDocument = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get())).get();
}

Vector<String> PageClientImpl::mimeTypesWithCustomContentProviders()
{
    return m_webView.configuration._contentProviderRegistry._mimeTypesWithCustomContentProviders;
}

void PageClientImpl::navigationGestureDidBegin()
{
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureDidBegin();
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureWillEnd(willNavigate, item);
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView->_page).navigationGestureDidEnd(willNavigate, item);
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    NavigationState::fromWebPage(*m_webView->_page).willRecordNavigationSnapshot(item);
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::didFinishLoadForMainFrame()
{
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
}

void PageClientImpl::didPerformActionMenuHitTest(const ActionMenuHitTestResult&)
{
}

} // namespace WebKit

#endif // PLATFORM(IOS)
