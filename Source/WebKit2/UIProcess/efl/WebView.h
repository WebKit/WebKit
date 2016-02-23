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
#include "WebColorPickerClient.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebViewClient.h"
#include <WebCore/TransformationMatrix.h>
#include <WebKit/WKViewEfl.h>

namespace API {
class PageConfiguration;
}

class EwkView;

namespace WebKit {
class CoordinatedGraphicsScene;

#if ENABLE(TOUCH_EVENTS)
class EwkTouchEvent;
#endif

class WebView : public API::ObjectImpl<API::Object::Type::View>, public PageClient
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
public:
    virtual ~WebView();

    static Ref<WebView> create(WebProcessPool*, API::PageConfiguration&);

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

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    void findZoomableAreaForPoint(const WebCore::IntPoint&, const WebCore::IntSize&);
#endif

    // View client.
    void initializeClient(const WKViewClientBase*);

#if ENABLE(INPUT_TYPE_COLOR)
    void initializeColorPickerClient(const WKColorPickerClientBase*);
    WebColorPickerClient& colorPickerClient() { return m_colorPickerClient; }
#endif

    WebPageProxy* page() { return m_page.get(); }

    const WebCore::IntSize& contentsSize() const { return m_contentsSize; }
    WebCore::FloatSize visibleContentsSize() const;

    // FIXME: Should become private when Web Events creation is moved to WebView.
    WebCore::AffineTransform transformFromScene() const;
    WebCore::AffineTransform transformToScene() const;

    void setOpacity(double opacity) { m_opacity = clampTo(opacity, 0.0, 1.0); }
    double opacity() const { return m_opacity; }

    void setEwkView(EwkView*);
    EwkView* ewkView() { return m_ewkView; }

    void paintToCairoSurface(cairo_surface_t*);
    void setThemePath(const String&);

#if ENABLE(TOUCH_EVENTS)
    void sendTouchEvent(EwkTouchEvent*);
#endif
    void sendMouseEvent(const Evas_Event_Mouse_Down*);
    void sendMouseEvent(const Evas_Event_Mouse_Up*);
    void sendMouseEvent(const Evas_Event_Mouse_Move*);

    void setViewBackgroundColor(const WebCore::Color&);
    WebCore::Color viewBackgroundColor();

private:
    WebView(WebProcessPool*, API::PageConfiguration&);

    CoordinatedGraphicsScene* coordinatedGraphicsScene();

    void updateViewportSize();
    WebCore::FloatSize dipSize() const;

    // PageClient
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;

    virtual void setViewNeedsDisplay(const WebCore::IntRect&) override;

    virtual void displayView() override;

    virtual bool canScrollView() override { return false; }
    virtual void scrollView(const WebCore::IntRect&, const WebCore::IntSize&) override;
    virtual void requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, bool) override;

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

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    virtual void pageDidRequestScroll(const WebCore::IntPoint&) override;
    virtual void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect) override;
    virtual void pageTransitionViewportReady() override;
    void didFindZoomableArea(const WebCore::IntPoint&, const WebCore::IntRect&) override;
#endif

    virtual void updateTextInputState() override;

    virtual void handleDownloadRequest(DownloadProxy*) override;

    void didChangeContentSize(const WebCore::IntSize&) override;

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

    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool) override;
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) override;
#endif

    virtual RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
#if ENABLE(CONTEXT_MENUS)
    virtual std::unique_ptr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, const ContextMenuContextData&, const UserData&) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color&, const WebCore::IntRect&) override;
#endif

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    virtual void exitAcceleratedCompositingMode() override;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) override;
    virtual void willEnterAcceleratedCompositingMode() override { }

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() override;
#endif

    virtual void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override final { }

    virtual void navigationGestureDidBegin() override { }
    virtual void navigationGestureWillEnd(bool, WebBackForwardListItem&) override { }
    virtual void navigationGestureDidEnd(bool, WebBackForwardListItem&) override { }
    virtual void navigationGestureDidEnd() override { }
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) override { }
    virtual void didRemoveNavigationGestureSnapshot() override { }

    virtual void didFirstVisuallyNonEmptyLayoutForMainFrame() override final { }
    virtual void didFinishLoadForMainFrame() override final { }
    virtual void didFailLoadForMainFrame() override { }
    virtual void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override final { }

    virtual void didChangeBackgroundColor() override { }

    virtual void refView() override final { }
    virtual void derefView() override final { }

#if ENABLE(VIDEO) && USE(GSTREAMER)
    virtual bool decidePolicyForInstallMissingMediaPluginsPermissionRequest(InstallMissingMediaPluginsPermissionRequest&) override final { return false; };
#endif

    virtual void didRestoreScrollPosition() override { }

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() override { }
    virtual bool isFullScreen() override final;
    virtual void enterFullScreen() override final;
    virtual void exitFullScreen() override final;
    virtual void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
    virtual void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
#endif

    EwkView* m_ewkView;

    WebViewClient m_client;
    RefPtr<WebPageProxy> m_page;
    DefaultUndoController m_undoController;
    WebCore::TransformationMatrix m_userViewportTransform;
    WebCore::IntSize m_size; // Size in device units.
    bool m_focused;
    bool m_visible;
    bool m_hasRequestedFullScreen;
    double m_opacity;
    WebCore::FloatPoint m_contentPosition; // Position in UI units.
    WebCore::IntSize m_contentsSize;

#if ENABLE(INPUT_TYPE_COLOR)
    WebColorPickerClient m_colorPickerClient;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // WebView_h
