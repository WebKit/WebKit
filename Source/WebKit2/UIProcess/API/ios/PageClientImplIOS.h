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

#ifndef PageClientImplIOS_h
#define PageClientImplIOS_h

#import "PageClient.h"
#import "WebFullScreenManagerProxy.h"
#import <wtf/RetainPtr.h>

@class WKContentView;

namespace WebKit {
    
class PageClientImpl : public PageClient
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
public:
    explicit PageClientImpl(WKContentView *);
    virtual ~PageClientImpl();
    
private:
    // PageClient
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() OVERRIDE;
    virtual void setViewNeedsDisplay(const WebCore::IntRect&) OVERRIDE;
    virtual void displayView() OVERRIDE;
    virtual bool canScrollView() OVERRIDE;
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) OVERRIDE;
    virtual WebCore::IntSize viewSize() OVERRIDE;
    virtual bool isViewWindowActive() OVERRIDE;
    virtual bool isViewFocused() OVERRIDE;
    virtual bool isViewVisible() OVERRIDE;
    virtual bool isViewInWindow() OVERRIDE;
    virtual void processDidCrash() OVERRIDE;
    virtual void didRelaunchProcess() OVERRIDE;
    virtual void pageClosed() OVERRIDE;
    virtual void preferencesDidChange() OVERRIDE;
    virtual void toolTipChanged(const String&, const String&) OVERRIDE;
    virtual bool decidePolicyForGeolocationPermissionRequest(WebFrameProxy&, WebSecurityOrigin&, GeolocationPermissionRequestProxy&) OVERRIDE;
    virtual void didCommitLoadForMainFrame() OVERRIDE;
    virtual void didChangeContentSize(const WebCore::IntSize&) OVERRIDE;
    virtual void setCursor(const WebCore::Cursor&) OVERRIDE;
    virtual void setCursorHiddenUntilMouseMoves(bool) OVERRIDE;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) OVERRIDE;
    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) OVERRIDE;
    virtual void clearAllEditCommands() OVERRIDE;
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo) OVERRIDE;
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo) OVERRIDE;
    virtual void accessibilityWebProcessTokenReceived(const IPC::DataReference&) OVERRIDE;
    virtual bool interpretKeyEvent(const NativeWebKeyboardEvent&, Vector<WebCore::KeypressCommand>&) OVERRIDE;
    virtual bool executeSavedCommandBySelector(const String& selector) OVERRIDE;
    virtual void setDragImage(const WebCore::IntPoint& clientPosition, PassRefPtr<ShareableBitmap> dragImage, bool isLinkDrag) OVERRIDE;
    virtual void updateSecureInputState() OVERRIDE;
    virtual void resetSecureInputState() OVERRIDE;
    virtual void notifyInputContextAboutDiscardedComposition() OVERRIDE;
    virtual void makeFirstResponder() OVERRIDE;
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) OVERRIDE;
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) OVERRIDE;
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) OVERRIDE;
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) OVERRIDE;
#endif
    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) OVERRIDE;
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) OVERRIDE;
    virtual void setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut, bool animate) OVERRIDE;
#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) OVERRIDE;
    virtual void exitAcceleratedCompositingMode() OVERRIDE;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) OVERRIDE;
    virtual void setAcceleratedCompositingRootLayer(CALayer *rootLayer) OVERRIDE;
#endif

    virtual void mainDocumentDidReceiveMobileDocType() OVERRIDE;

    virtual void didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius) OVERRIDE;

    void didChangeViewportArguments(const WebCore::ViewportArguments& viewportArguments) OVERRIDE;

    virtual void startAssistingNode(const WebCore::IntRect&, bool hasNextFocusable, bool hasPreviousFocusable) OVERRIDE;
    virtual void stopAssistingNode() OVERRIDE;
    virtual void selectionDidChange() OVERRIDE;
    virtual bool interpretKeyEvent(const NativeWebKeyboardEvent&, bool isCharEvent) OVERRIDE;
    virtual void positionInformationDidChange(const InteractionInformationAtPosition&);

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    virual WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() OVERRIDE;
#endif

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() OVERRIDE;
    virtual bool isFullScreen() OVERRIDE;
    virtual void enterFullScreen() OVERRIDE;
    virtual void exitFullScreen() OVERRIDE;
    virtual void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) OVERRIDE;
    virtual void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) OVERRIDE;
#endif

    WKContentView *m_view;
};
} // namespace WebKit

#endif // PageClientImplIOS_h
