/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebView_h
#define WebView_h

#if USE(COORDINATED_GRAPHICS)

#include "APIObject.h"
#include "DefaultUndoController.h"
#include "PageClient.h"
#include "WebContext.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebViewClient.h"
#include <WebCore/TransformationMatrix.h>

namespace WebCore {
class CoordinatedGraphicsScene;
}

namespace WebKit {

class WebView : public API::ObjectImpl<API::Object::Type::View>, public PageClient
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
public:
    virtual ~WebView();

    static PassRefPtr<WebView> create(WebContext*, WebPageGroup*);

    void initialize();

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_size; }

    bool isActive() const;
    void setActive(bool);

    bool isFocused() const { return m_focused; }
    void setFocused(bool);

    bool isVisible() const { return m_visible; }
    void setVisible(bool);

    void setContentScaleFactor(float);
    float contentScaleFactor() const { return m_page->pageScaleFactor(); }

    void setContentPosition(const WebCore::FloatPoint& position) { m_contentPosition = position; }
    const WebCore::FloatPoint& contentPosition() const { return m_contentPosition; }

    void setUserViewportTranslation(double tx, double ty);
    WebCore::IntPoint userViewportToContents(const WebCore::IntPoint&) const;
    WebCore::IntPoint userViewportToScene(const WebCore::IntPoint&) const;
    WebCore::IntPoint contentsToUserViewport(const WebCore::IntPoint&) const;

    void paintToCurrentGLContext();

    WKPageRef pageRef() const { return toAPI(m_page.get()); }

    void setDrawsBackground(bool);
    bool drawsBackground() const;
    void setDrawsTransparentBackground(bool);
    bool drawsTransparentBackground() const;

    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();

#if ENABLE(FULLSCREEN_API)
    bool requestExitFullScreen();
#endif

    void findZoomableAreaForPoint(const WebCore::IntPoint&, const WebCore::IntSize&);

    // View client.
    void initializeClient(const WKViewClientBase*);

    WebPageProxy* page() { return m_page.get(); }

    void didChangeContentSize(const WebCore::IntSize&);
    const WebCore::IntSize& contentsSize() const { return m_contentsSize; }
    WebCore::FloatSize visibleContentsSize() const;
    void didFindZoomableArea(const WebCore::IntPoint&, const WebCore::IntRect&);

    // FIXME: Should become private when Web Events creation is moved to WebView.
    WebCore::AffineTransform transformFromScene() const;
    WebCore::AffineTransform transformToScene() const;

    void setOpacity(double opacity) { m_opacity = clampTo(opacity, 0.0, 1.0); }
    double opacity() const { return m_opacity; }

protected:
    WebView(WebContext*, WebPageGroup*);
    WebCore::CoordinatedGraphicsScene* coordinatedGraphicsScene();

    void updateViewportSize();
    WebCore::FloatSize dipSize() const;
    // PageClient
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;

    virtual void setViewNeedsDisplay(const WebCore::IntRect&) override;

    virtual void displayView() override;

    virtual bool canScrollView() override { return false; }
    virtual void scrollView(const WebCore::IntRect&, const WebCore::IntSize&) override;
    virtual void requestScroll(const WebCore::FloatPoint&, bool) override;

    virtual WebCore::IntSize viewSize() override;

    virtual bool isViewWindowActive() override;
    virtual bool isViewFocused() override;
    virtual bool isViewVisible() override;
    virtual bool isViewInWindow() override;

    virtual void processDidExit() override;
    virtual void didRelaunchProcess() override;
    virtual void pageClosed() override;

    virtual void preferencesDidChange() override;

    virtual void toolTipChanged(const String&, const String&) override;

    virtual void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;

    virtual void pageDidRequestScroll(const WebCore::IntPoint&) override;
    virtual void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect) override;
    virtual void pageTransitionViewportReady() override;

    virtual void setCursor(const WebCore::Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;

    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;

    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) override;
    virtual void clearAllEditCommands() override;
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo) override;
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo) override;

    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;

    virtual void updateTextInputState() override;

    virtual void handleDownloadRequest(DownloadProxy*) override;

    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool) override;
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) override;
#endif

    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) override;
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) override;
#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassRefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&) override;
#endif

    virtual void setTextIndicator(PassRefPtr<WebCore::TextIndicator>, bool, bool) override;

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    virtual void exitAcceleratedCompositingMode() override;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) override;

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() override;

    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() override { }
    virtual bool isFullScreen() override { return false; }
    virtual void enterFullScreen() override { }
    virtual void exitFullScreen() override { }
    virtual void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
    virtual void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
#endif
    virtual void navigationGestureDidBegin() override { };
    virtual void navigationGestureWillEnd(bool, WebBackForwardListItem&) override { };
    virtual void navigationGestureDidEnd(bool, WebBackForwardListItem&) override { };
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) override { };

    WebViewClient m_client;
    RefPtr<WebPageProxy> m_page;
    DefaultUndoController m_undoController;
    WebCore::TransformationMatrix m_userViewportTransform;
    WebCore::IntSize m_size; // Size in device units.
    bool m_focused;
    bool m_visible;
    double m_opacity;
    WebCore::FloatPoint m_contentPosition; // Position in UI units.
    WebCore::IntSize m_contentsSize;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // WebView_h
