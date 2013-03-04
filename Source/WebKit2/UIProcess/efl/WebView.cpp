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

#include "config.h"
#include "WebView.h"

#include "CoordinatedLayerTreeHostProxy.h"
#include "DownloadManagerEfl.h"
#include "DrawingAreaProxyImpl.h"
#include "EwkView.h"
#include "InputMethodContextEfl.h"
#include "NotImplemented.h"
#include "PageViewportController.h"
#include "PageViewportControllerClientEfl.h"
#include "WebContextMenuProxyEfl.h"
#include "WebPageProxy.h"
#include "WebPopupMenuListenerEfl.h"
#include "ewk_context_private.h"
#include <WebCore/CoordinatedGraphicsScene.h>
#include <WebCore/PlatformContextCairo.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

using namespace EwkViewCallbacks;
using namespace WebCore;

namespace WebKit {

WebView::WebView(WebContext* context, WebPageGroup* pageGroup, EwkView* ewkView)
    : m_ewkView(ewkView)
    , m_page(context->createWebPage(this, pageGroup))
{
    m_page->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    m_page->pageGroup()->preferences()->setForceCompositingMode(true);

    char* debugVisualsEnvironment = getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS");
    bool showDebugVisuals = debugVisualsEnvironment && !strcmp(debugVisualsEnvironment, "1");
    m_page->pageGroup()->preferences()->setCompositingBordersVisible(showDebugVisuals);
    m_page->pageGroup()->preferences()->setCompositingRepaintCountersVisible(showDebugVisuals);

#if ENABLE(FULLSCREEN_API)
    m_page->fullScreenManager()->setWebView(evasObject());
#endif
}

WebView::~WebView()
{
    if (m_page->isClosed())
        return;

    m_page->close();
}

void WebView::initialize()
{
    m_page->initializeWebPage();
    if (CoordinatedGraphicsScene* scene = coordinatedGraphicsScene())
        scene->setActive(true);
}

void WebView::setUserViewportTranslation(double tx, double ty)
{
    m_userViewportTransform = TransformationMatrix().translate(tx, ty);
}

IntPoint WebView::userViewportToContents(const IntPoint& point) const
{
    return m_userViewportTransform.mapPoint(point);
}

void WebView::paintToCurrentGLContext()
{
    CoordinatedGraphicsScene* scene = coordinatedGraphicsScene();
    if (!scene)
        return;

    // FIXME: We need to clean up this code as it is split over CoordGfx and Page.
    scene->setDrawsBackground(m_page->drawsBackground());

    FloatRect viewport = m_userViewportTransform.mapRect(IntRect(IntPoint(), m_ewkView->size()));
    scene->paintToCurrentGLContext(transformToScene().toTransformationMatrix(), /* opacity */ 1, viewport);
}

void WebView::paintToCairoSurface(cairo_surface_t* surface)
{
    CoordinatedGraphicsScene* scene = coordinatedGraphicsScene();
    if (!scene)
        return;

    PlatformContextCairo context(cairo_create(surface));

    const FloatPoint& pagePosition = m_ewkView->pagePosition();
    double effectiveScale = m_page->deviceScaleFactor() * m_ewkView->pageScaleFactor();

    cairo_matrix_t transform = { effectiveScale, 0, 0, effectiveScale, - pagePosition.x(), - pagePosition.y() };
    cairo_set_matrix(context.cr(), &transform);

    scene->paintToGraphicsContext(&context);
}

Evas_Object* WebView::evasObject()
{
    return m_ewkView->evasObject();
}

void WebView::setThemePath(const String& theme)
{
    m_page->setThemePath(theme);
}

void WebView::setDrawsBackground(bool drawsBackground)
{
    m_page->setDrawsBackground(drawsBackground);
}

bool WebView::drawsBackground() const
{
    return m_page->drawsBackground();
}

void WebView::setDrawsTransparentBackground(bool transparentBackground)
{
    m_page->setDrawsTransparentBackground(transparentBackground);
}

bool WebView::drawsTransparentBackground() const
{
    return m_page->drawsTransparentBackground();
}

void WebView::suspendActiveDOMObjectsAndAnimations()
{
    m_page->suspendActiveDOMObjectsAndAnimations();
}

void WebView::resumeActiveDOMObjectsAndAnimations()
{
    m_page->resumeActiveDOMObjectsAndAnimations();
}

void WebView::setShowsAsSource(bool showsAsSource)
{
    m_page->setMainFrameInViewSourceMode(showsAsSource);
}

bool WebView::showsAsSource() const
{
    return m_page->mainFrameInViewSourceMode();
}

#if ENABLE(FULLSCREEN_API)
void WebView::exitFullScreen()
{
    m_page->fullScreenManager()->requestExitFullScreen();
}
#endif

void WebView::initializeClient(const WKViewClient* client)
{
    m_client.initialize(client);
}

void WebView::didCommitLoad()
{
    if (m_page->useFixedLayout()) {
        m_ewkView->pageViewportController()->didCommitLoad();
        return;
    }
    m_ewkView->scheduleUpdateDisplay();
}

void WebView::updateViewportSize()
{
    FloatSize size = m_ewkView->size();
    // The viewport controller expects sizes in UI units, and not raw device units.
    size.scale(1 / m_page->deviceScaleFactor());
    if (m_page->useFixedLayout()) {
        m_ewkView->pageViewportController()->didChangeViewportSize(size);
        return;
    }
    m_page->drawingArea()->setVisibleContentsRect(FloatRect(m_ewkView->pagePosition(), size), FloatPoint());
}

void WebView::didChangeContentsSize(const WebCore::IntSize& size)
{
    if (m_page->useFixedLayout()) {
        m_ewkView->pageViewportController()->didChangeContentsSize(size);
        return;
    }
    m_client.didChangeContentsSize(this, size);
}

AffineTransform WebView::transformFromScene() const
{
    return transformToScene().inverse();
}

AffineTransform WebView::transformToScene() const
{
    TransformationMatrix transform = m_userViewportTransform;

    const FloatPoint& pagePosition = m_ewkView->pagePosition();
    transform.translate(-pagePosition.x(), -pagePosition.y());
    transform.scale(m_page->deviceScaleFactor());
    transform.scale(m_ewkView->pageScaleFactor());

    return transform.toAffineTransform();
}

CoordinatedGraphicsScene* WebView::coordinatedGraphicsScene()
{
    DrawingAreaProxy* drawingArea = m_page->drawingArea();
    if (!drawingArea)
        return 0;

    WebKit::CoordinatedLayerTreeHostProxy* layerTreeHostProxy = drawingArea->coordinatedLayerTreeHostProxy();
    if (!layerTreeHostProxy)
        return 0;

    return layerTreeHostProxy->coordinatedGraphicsScene();
}

// Page Client

PassOwnPtr<DrawingAreaProxy> WebView::createDrawingAreaProxy()
{
    OwnPtr<DrawingAreaProxy> drawingArea = DrawingAreaProxyImpl::create(page());
    return drawingArea.release();
}

void WebView::setViewNeedsDisplay(const WebCore::IntRect& area)
{
    m_client.viewNeedsDisplay(this, area);
}

void WebView::displayView()
{
    notImplemented();
}

void WebView::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize&)
{
    setViewNeedsDisplay(scrollRect);
}

WebCore::IntSize WebView::viewSize()
{
    return m_ewkView->size();
}

bool WebView::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool WebView::isViewFocused()
{
    return m_ewkView->isFocused();
}

bool WebView::isViewVisible()
{
    return m_ewkView->isVisible();
}

bool WebView::isViewInWindow()
{
    notImplemented();
    return true;
}

void WebView::processDidCrash()
{
    m_client.webProcessCrashed(this, m_page->urlAtProcessExit());
}

void WebView::didRelaunchProcess()
{
    m_client.webProcessDidRelaunch(this);
}

void WebView::pageClosed()
{
    notImplemented();
}

void WebView::toolTipChanged(const String&, const String& newToolTip)
{
    if (newToolTip.isEmpty())
        m_ewkView->smartCallback<TooltipTextUnset>().call();
    else
        m_ewkView->smartCallback<TooltipTextSet>().call(newToolTip);
}

void WebView::setCursor(const Cursor& cursor)
{
    m_ewkView->setCursor(cursor);
}

void WebView::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void WebView::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(command, undoOrRedo);
}

void WebView::clearAllEditCommands()
{
    m_undoController.clearAllEditCommands();
}

bool WebView::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return m_undoController.canUndoRedo(undoOrRedo);
}

void WebView::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoController.executeUndoRedo(undoOrRedo);
}

IntPoint WebView::screenToWindow(const IntPoint& point)
{
    notImplemented();
    return point;
}

IntRect WebView::windowToScreen(const IntRect&)
{
    notImplemented();
    return IntRect();
}

void WebView::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
{
    notImplemented();
}

#if ENABLE(TOUCH_EVENTS)
void WebView::doneWithTouchEvent(const NativeWebTouchEvent&, bool /*wasEventHandled*/)
{
    notImplemented();
}
#endif

PassRefPtr<WebPopupMenuProxy> WebView::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuListenerEfl::create(page);
}

PassRefPtr<WebContextMenuProxy> WebView::createContextMenuProxy(WebPageProxy* page)
{
    return WebContextMenuProxyEfl::create(m_ewkView, page);
}

#if ENABLE(INPUT_TYPE_COLOR)
PassRefPtr<WebColorChooserProxy> WebView::createColorChooserProxy(WebPageProxy*, const WebCore::Color&, const WebCore::IntRect&)
{
    notImplemented();
    return 0;
}
#endif

void WebView::setFindIndicator(PassRefPtr<FindIndicator>, bool, bool)
{
    notImplemented();
}

void WebView::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    if (CoordinatedGraphicsScene* scene = coordinatedGraphicsScene())
        scene->setActive(true);
}

void WebView::exitAcceleratedCompositingMode()
{
    if (CoordinatedGraphicsScene* scene = coordinatedGraphicsScene())
        scene->setActive(false);
}

void WebView::updateAcceleratedCompositingMode(const LayerTreeContext&)
{
    notImplemented();
}

void WebView::didCommitLoadForMainFrame(bool)
{
    notImplemented();
}

void WebView::didFinishLoadingDataForCustomRepresentation(const String&, const CoreIPC::DataReference&)
{
    notImplemented();
}

double WebView::customRepresentationZoomFactor()
{
    notImplemented();
    return 0;
}

void WebView::setCustomRepresentationZoomFactor(double)
{
    notImplemented();
}

void WebView::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}

void WebView::findStringInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

void WebView::countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

void WebView::updateTextInputState()
{
    InputMethodContextEfl* inputMethodContext = m_ewkView->inputMethodContext();
    if (inputMethodContext)
        inputMethodContext->updateTextInputState();
}

void WebView::handleDownloadRequest(DownloadProxy* download)
{
    EwkContext* context = m_ewkView->ewkContext();
    context->downloadManager()->registerDownloadJob(toAPI(download), m_ewkView);
}

FloatRect WebView::convertToDeviceSpace(const FloatRect& userRect)
{
    if (m_page->useFixedLayout()) {
        FloatRect result = userRect;
        result.scale(m_page->deviceScaleFactor());
        return result;
    }
    // Legacy mode.
    notImplemented();
    return userRect;
}

FloatRect WebView::convertToUserSpace(const FloatRect& deviceRect)
{
    if (m_page->useFixedLayout()) {
        FloatRect result = deviceRect;
        result.scale(1 / m_page->deviceScaleFactor());
        return result;
    }
    // Legacy mode.
    notImplemented();
    return deviceRect;
}

void WebView::didChangeViewportProperties(const WebCore::ViewportAttributes& attr)
{
    if (m_page->useFixedLayout()) {
        m_ewkView->pageViewportController()->didChangeViewportAttributes(attr);
        return;
    }
    m_ewkView->scheduleUpdateDisplay();
}

void WebView::pageDidRequestScroll(const IntPoint& position)
{
    if (m_page->useFixedLayout()) {        
        m_ewkView->pageViewportController()->pageDidRequestScroll(position);
        return;
    }
    m_ewkView->setPagePosition(FloatPoint(position));
    m_ewkView->scheduleUpdateDisplay();
}

void WebView::didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect)
{
    if (m_page->useFixedLayout()) {
        m_ewkView->pageViewportController()->didRenderFrame(contentsSize, coveredRect);
        return;
    }
    m_ewkView->scheduleUpdateDisplay();
}

void WebView::pageTransitionViewportReady()
{
    if (m_page->useFixedLayout()) {
        m_ewkView->pageViewportController()->pageTransitionViewportReady();
        return;
    }
    m_ewkView->scheduleUpdateDisplay();
}

} // namespace WebKit
