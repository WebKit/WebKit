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

#include "APIObject.h"
#include "DefaultUndoController.h"
#include "PageClient.h"
#include "WebContext.h"
#include "WebGeometry.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebViewClient.h"
#include <WebCore/TransformationMatrix.h>

class EwkView;

namespace WebCore {
class CoordinatedGraphicsScene;
}

namespace WebKit {

class WebView : public TypedAPIObject<APIObject::TypeView>, public PageClient {
public:
    virtual ~WebView();

    static PassRefPtr<WebView> create(WebContext*, WebPageGroup*);

    // FIXME: Remove when possible.
    void setEwkView(EwkView*);

    void initialize();

    void setUserViewportTranslation(double tx, double ty);
    WebCore::IntPoint userViewportToContents(const WebCore::IntPoint&) const;

    void paintToCurrentGLContext();
    void paintToCairoSurface(cairo_surface_t*);

    WKPageRef pageRef() const { return toAPI(m_page.get()); }

    void setDrawsBackground(bool);
    bool drawsBackground() const;
    void setDrawsTransparentBackground(bool);
    bool drawsTransparentBackground() const;

    void setThemePath(const String&);

    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();

    void setShowsAsSource(bool);
    bool showsAsSource() const;

#if ENABLE(FULLSCREEN_API)
    void exitFullScreen();
#endif

    // View client.
    void initializeClient(const WKViewClient*);

    // FIXME: Remove when possible.
    Evas_Object* evasObject();
    WebPageProxy* page() { return m_page.get(); }

    void didCommitLoad();
    void updateViewportSize();
    void didChangeContentsSize(const WebCore::IntSize&);

    // FIXME: Should become private when Web Events creation is moved to WebView.
    WebCore::AffineTransform transformFromScene() const;
    WebCore::AffineTransform transformToScene() const;

private:
    WebView(WebContext*, WebPageGroup*);
    WebCore::CoordinatedGraphicsScene* coordinatedGraphicsScene();

    // PageClient
    PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy() OVERRIDE;

    void setViewNeedsDisplay(const WebCore::IntRect&) OVERRIDE;

    void displayView() OVERRIDE;

    bool canScrollView() OVERRIDE { return false; }
    void scrollView(const WebCore::IntRect&, const WebCore::IntSize&) OVERRIDE;

    WebCore::IntSize viewSize() OVERRIDE;

    bool isViewWindowActive() OVERRIDE;
    bool isViewFocused() OVERRIDE;
    bool isViewVisible() OVERRIDE;
    bool isViewInWindow() OVERRIDE;

    void processDidCrash() OVERRIDE;
    void didRelaunchProcess() OVERRIDE;
    void pageClosed() OVERRIDE;

    void toolTipChanged(const String&, const String&) OVERRIDE;

    void pageDidRequestScroll(const WebCore::IntPoint&) OVERRIDE;
    void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect) OVERRIDE;
    void pageTransitionViewportReady() OVERRIDE;

    void setCursor(const WebCore::Cursor&) OVERRIDE;
    void setCursorHiddenUntilMouseMoves(bool) OVERRIDE;

    void didChangeViewportProperties(const WebCore::ViewportAttributes&) OVERRIDE;

    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) OVERRIDE;
    void clearAllEditCommands() OVERRIDE;
    bool canUndoRedo(WebPageProxy::UndoOrRedo) OVERRIDE;
    void executeUndoRedo(WebPageProxy::UndoOrRedo) OVERRIDE;

    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) OVERRIDE;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) OVERRIDE;
    WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) OVERRIDE;
    WebCore::IntRect windowToScreen(const WebCore::IntRect&) OVERRIDE;

    void updateTextInputState() OVERRIDE;

    void handleDownloadRequest(DownloadProxy*) OVERRIDE;

    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool) OVERRIDE;
#if ENABLE(TOUCH_EVENTS)
    void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) OVERRIDE;
#endif

    PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) OVERRIDE;
    PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) OVERRIDE;
#if ENABLE(INPUT_TYPE_COLOR)
    PassRefPtr<WebColorChooserProxy> createColorChooserProxy(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&) OVERRIDE;
#endif

    void setFindIndicator(PassRefPtr<FindIndicator>, bool, bool) OVERRIDE;

    void enterAcceleratedCompositingMode(const LayerTreeContext&) OVERRIDE;
    void exitAcceleratedCompositingMode() OVERRIDE;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) OVERRIDE;

    void didCommitLoadForMainFrame(bool) OVERRIDE;
    void didFinishLoadingDataForCustomRepresentation(const String&, const CoreIPC::DataReference&) OVERRIDE;

    double customRepresentationZoomFactor() OVERRIDE;
    void setCustomRepresentationZoomFactor(double) OVERRIDE;

    void flashBackingStoreUpdates(const Vector<WebCore::IntRect>&) OVERRIDE;
    void findStringInCustomRepresentation(const String&, FindOptions, unsigned) OVERRIDE;
    void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned) OVERRIDE;

private:
    WebViewClient m_client;
    EwkView* m_ewkView;
    RefPtr<WebPageProxy> m_page;
    DefaultUndoController m_undoController;
    WebCore::TransformationMatrix m_userViewportTransform;
};

}

#endif
