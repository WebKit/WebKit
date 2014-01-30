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

#import "NativeWebKeyboardEvent.h"
#import "InteractionInformationAtPosition.h"
#import "WKContentViewInternal.h"
#import "WebContextMenuProxy.h"
#import "WebEditCommandProxy.h"
#import <UIKit/UIWebTouchEventsGestureRecognizer.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>

@interface UIView (IPI)
- (UIScrollView *)_scroller;
@end

using namespace WebCore;

namespace WebKit {

PageClientImpl::PageClientImpl(WKContentView *view)
    : m_view(view)
{
}

PageClientImpl::~PageClientImpl()
{
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return [m_view _createDrawingAreaProxy];
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
    if (UIScrollView *scroller = [m_view _scroller])
        return IntSize(scroller.bounds.size);

    return IntSize(m_view.bounds.size);
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
    return [m_view window];
}

void PageClientImpl::processDidCrash()
{
    [m_view _processDidCrash];
}

void PageClientImpl::didRelaunchProcess()
{
    [m_view _didRelaunchProcess];
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
    [m_view _decidePolicyForGeolocationRequestFromOrigin:origin frame:frame request:request];
    return true;
}

void PageClientImpl::didCommitLoadForMainFrame()
{
    [m_view _didCommitLoadForMainFrame];
}

void PageClientImpl::didChangeContentSize(const IntSize& contentsSize)
{
    [m_view _didChangeContentSize:contentsSize];
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

void PageClientImpl::accessibilityWebProcessTokenReceived(const IPC::DataReference&)
{
    notImplemented();
}

bool PageClientImpl::interpretKeyEvent(const NativeWebKeyboardEvent&, Vector<KeypressCommand>&)
{
    notImplemented();
    return false;
}

bool PageClientImpl::interpretKeyEvent(const NativeWebKeyboardEvent& event, bool isCharEvent)
{
    return [m_view _interpretKeyEvent:event.nativeEvent() isCharEvent:isCharEvent];
}

void PageClientImpl::positionInformationDidChange(const InteractionInformationAtPosition& info)
{
    [m_view _positionInformationDidChange:info];
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
    [m_view _selectionChanged];
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

IntPoint PageClientImpl::screenToWindow(const IntPoint& point)
{
    notImplemented();
    return IntPoint();
}

IntRect PageClientImpl::windowToScreen(const IntRect& rect)
{
    notImplemented();
    return IntRect();
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
{
    notImplemented();
}

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& nativeWebtouchEvent, bool eventHandled)
{
    [nativeWebtouchEvent.nativeEvent() setDefaultPrevented:eventHandled];
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

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator>, bool, bool)
{
    notImplemented();
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

void PageClientImpl::setAcceleratedCompositingRootLayer(CALayer *rootLayer)
{
    [m_view _setAcceleratedCompositingRootLayer:rootLayer];
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    notImplemented();
    return nullptr;
}

RetainPtr<CGImageRef> PageClientImpl::takeViewSnapshot()
{
    notImplemented();
    return nullptr;
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    notImplemented();
}

void PageClientImpl::mainDocumentDidReceiveMobileDocType()
{
    [m_view _didReceiveMobileDocTypeForMainFrame];
}

void PageClientImpl::didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius)
{
    [m_view _didGetTapHighlightForRequest:requestID color:color quads:highlightedQuads topLeftRadius:topLeftRadius topRightRadius:topRightRadius bottomLeftRadius:bottomLeftRadius bottomRightRadius:bottomRightRadius];
}

void PageClientImpl::didChangeViewportArguments(const WebCore::ViewportArguments& viewportArguments)
{
    [m_view _didChangeViewportArguments:viewportArguments];
}

void PageClientImpl::startAssistingNode(const WebCore::IntRect&, bool, bool)
{
    [m_view _startAssistingNode];
}

void PageClientImpl::stopAssistingNode()
{
    [m_view _stopAssistingNode];
}

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

} // namespace WebKit
