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
#import "FindIndicator.h"
#import "NativeWebKeyboardEvent.h"
#import "InteractionInformationAtPosition.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
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
@end

using namespace WebCore;

namespace WebKit {

PageClientImpl::PageClientImpl(WKContentView *contentView, WKWebView *webView)
    : m_contentView(contentView)
    , m_webView(webView)
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

void PageClientImpl::scrollView(const IntRect&, const IntSize&)
{
    ASSERT_NOT_REACHED();
}

bool PageClientImpl::canScrollView()
{
    notImplemented();
    return false;
}

IntSize PageClientImpl::viewSize()
{
    if (UIScrollView *scroller = [m_contentView _scroller])
        return IntSize(scroller.bounds.size);

    return IntSize(m_contentView.bounds.size);
}

bool PageClientImpl::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewFocused()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewVisible()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewInWindow()
{
    return [m_contentView window];
}

void PageClientImpl::processDidExit()
{
    [m_contentView _processDidExit];
}

void PageClientImpl::didRelaunchProcess()
{
    [m_contentView _didRelaunchProcess];
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

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

void PageClientImpl::clearAllEditCommands()
{
    notImplemented();
}

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo)
{
    notImplemented();
    return false;
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo)
{
    notImplemented();
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

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
{
    notImplemented();
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

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator> findIndicator, bool fadeOut, bool animate)
{
    [m_contentView _setFindIndicator:findIndicator fadeOut:fadeOut animate:animate];
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

RetainPtr<CGImageRef> PageClientImpl::takeViewSnapshot()
{
    return [m_contentView _takeViewSnapshot];
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    notImplemented();
}

void PageClientImpl::clearCustomSwipeViews()
{
    notImplemented();
}

void PageClientImpl::didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius)
{
    [m_contentView _didGetTapHighlightForRequest:requestID color:color quads:highlightedQuads topLeftRadius:topLeftRadius topRightRadius:topRightRadius bottomLeftRadius:bottomLeftRadius bottomRightRadius:bottomRightRadius];
}

void PageClientImpl::didCommitLayerTree(const RemoteLayerTreeTransaction& layerTreeTransaction)
{
    [m_contentView _didCommitLayerTree:layerTreeTransaction];
}

void PageClientImpl::dynamicViewportUpdateChangedTarget(double newScale, const WebCore::FloatPoint& newScrollPosition)
{
    [m_webView _dynamicViewportUpdateChangedTargetToScale:newScale position:newScrollPosition];
}

void PageClientImpl::startAssistingNode(const AssistedNodeInformation& nodeInformation, API::Object* userData)
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

    [m_contentView _startAssistingNode:nodeInformation userObject:userObject];
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

#if ENABLE(INSPECTOR)
void PageClientImpl::showInspectorIndication()
{
    [m_webView _showInspectorIndication];
}

void PageClientImpl::hideInspectorIndication()
{
    [m_webView _hideInspectorIndication];
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

} // namespace WebKit

#endif // PLATFORM(IOS)
