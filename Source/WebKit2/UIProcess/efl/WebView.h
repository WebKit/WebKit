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
    std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;

    void setViewNeedsDisplay(const WebCore::IntRect&) override;

    void displayView() override;

    bool canScrollView() override { return false; }
    void scrollView(const WebCore::IntRect&, const WebCore::IntSize&) override;
    void requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, bool) override;

    WebCore::IntSize viewSize() override;

    bool isViewWindowActive() override;
    bool isViewFocused() override;
    bool isViewVisible() override;
    bool isViewInWindow() override;

    void processDidExit() override;
    void didRelaunchProcess() override;
    void pageClosed() override;

    void preferencesDidChange() override;

    void toolTipChanged(const String&, const String&) override;

    void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    void pageDidRequestScroll(const WebCore::IntPoint&) override;
    void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect) override;
    void pageTransitionViewportReady() override;
    void didFindZoomableArea(const WebCore::IntPoint&, const WebCore::IntRect&) override;
#endif

    void updateTextInputState() override;

    void handleDownloadRequest(DownloadProxy*) override;

    void didChangeContentSize(const WebCore::IntSize&) override;

    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;

    void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;

    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) override;
    void clearAllEditCommands() override;
    bool canUndoRedo(WebPageProxy::UndoOrRedo) override;
    void executeUndoRedo(WebPageProxy::UndoOrRedo) override;

    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;

    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool) override;
#if ENABLE(TOUCH_EVENTS)
    void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) override;
#endif

    RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
#if ENABLE(CONTEXT_MENUS)
    std::unique_ptr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, const ContextMenuContextData&, const UserData&) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color&, const WebCore::IntRect&) override;
#endif

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode() override;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) override;
    void willEnterAcceleratedCompositingMode() override { }

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() override;
#endif

    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override final { }

    void navigationGestureDidBegin() override { }
    void navigationGestureWillEnd(bool, WebBackForwardListItem&) override { }
    void navigationGestureDidEnd(bool, WebBackForwardListItem&) override { }
    void navigationGestureDidEnd() override { }
    void willRecordNavigationSnapshot(WebBackForwardListItem&) override { }
    void didRemoveNavigationGestureSnapshot() override { }

    void didFirstVisuallyNonEmptyLayoutForMainFrame() override final { }
    void didFinishLoadForMainFrame() override final { }
    void didFailLoadForMainFrame() override { }
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override final { }

    void didChangeBackgroundColor() override { }

    void refView() override final { }
    void derefView() override final { }

#if ENABLE(VIDEO) && USE(GSTREAMER)
    bool decidePolicyForInstallMissingMediaPluginsPermissionRequest(InstallMissingMediaPluginsPermissionRequest&) override final { return false; };
#endif

    void didRestoreScrollPosition() override { }

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    void closeFullScreenManager() override { }
    bool isFullScreen() override final;
    void enterFullScreen() override final;
    void exitFullScreen() override final;
    void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
    void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
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
