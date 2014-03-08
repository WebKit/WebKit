/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "config.h"
#include "WebViewEfl.h"

#include "DownloadManagerEfl.h"
#include "EwkView.h"
#include "InputMethodContextEfl.h"
#include "NativeWebMouseEvent.h"
#include "NotImplemented.h"
#include "WebContextMenuProxyEfl.h"
#include "WebPopupMenuListenerEfl.h"
#include "ewk_context_private.h"
#include <WebCore/CoordinatedGraphicsScene.h>
#include <WebCore/PlatformContextCairo.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include "EwkTouchEvent.h"
#endif

#if ENABLE(INPUT_TYPE_COLOR)
#include "WebColorPickerEfl.h"
#endif

using namespace EwkViewCallbacks;
using namespace WebCore;

namespace WebKit {

PassRefPtr<WebView> WebView::create(WebContext* context, WebPageGroup* pageGroup)
{
    return adoptRef(new WebViewEfl(context, pageGroup));
}

WebViewEfl::WebViewEfl(WebContext* context, WebPageGroup* pageGroup)
    : WebView(context, pageGroup)
    , m_ewkView(0)
    , m_hasRequestedFullScreen(false)
{
}

void WebViewEfl::setEwkView(EwkView* ewkView)
{
    m_ewkView = ewkView;
}

void WebViewEfl::paintToCairoSurface(cairo_surface_t* surface)
{
    CoordinatedGraphicsScene* scene = coordinatedGraphicsScene();
    if (!scene)
        return;

    PlatformContextCairo context(cairo_create(surface));

    const FloatPoint& position = contentPosition();
    double effectiveScale = m_page->deviceScaleFactor() * contentScaleFactor();

    cairo_matrix_t transform = { effectiveScale, 0, 0, effectiveScale, - position.x() * m_page->deviceScaleFactor(), - position.y() * m_page->deviceScaleFactor() };
    cairo_set_matrix(context.cr(), &transform);
    scene->paintToGraphicsContext(&context);
}

PassRefPtr<WebPopupMenuProxy> WebViewEfl::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuListenerEfl::create(page);
}

#if ENABLE(CONTEXT_MENUS)
PassRefPtr<WebContextMenuProxy> WebViewEfl::createContextMenuProxy(WebPageProxy* page)
{
    return WebContextMenuProxyEfl::create(m_ewkView, page);
}
#endif

void WebViewEfl::setCursor(const Cursor& cursor)
{
    m_ewkView->setCursor(cursor);
}

void WebViewEfl::updateTextInputState()
{
    if (InputMethodContextEfl* inputMethodContext = m_ewkView->inputMethodContext())
        inputMethodContext->updateTextInputState();
}

void WebViewEfl::handleDownloadRequest(DownloadProxy* download)
{
    EwkContext* context = m_ewkView->ewkContext();
    context->downloadManager()->registerDownloadJob(toAPI(download), m_ewkView);
}

void WebViewEfl::setThemePath(const String& theme)
{
    m_page->setThemePath(theme);
}

#if ENABLE(TOUCH_EVENTS)
void WebViewEfl::sendTouchEvent(EwkTouchEvent* touchEvent)
{
    ASSERT(touchEvent);
    m_page->handleTouchEvent(NativeWebTouchEvent(touchEvent, transformFromScene()));
}
#endif

void WebViewEfl::sendMouseEvent(const Evas_Event_Mouse_Down* event)
{
    ASSERT(event);
    m_page->handleMouseEvent(NativeWebMouseEvent(event, transformFromScene(), m_userViewportTransform.toAffineTransform()));
}

void WebViewEfl::sendMouseEvent(const Evas_Event_Mouse_Up* event)
{
    ASSERT(event);
    m_page->handleMouseEvent(NativeWebMouseEvent(event, transformFromScene(), m_userViewportTransform.toAffineTransform()));
}

void WebViewEfl::sendMouseEvent(const Evas_Event_Mouse_Move* event)
{
    ASSERT(event);
    m_page->handleMouseEvent(NativeWebMouseEvent(event, transformFromScene(), m_userViewportTransform.toAffineTransform()));
}

#if ENABLE(FULLSCREEN_API)

// WebFullScreenManagerProxyClient
bool WebViewEfl::isFullScreen()
{
    return m_hasRequestedFullScreen;
}

void WebViewEfl::enterFullScreen()
{
    if (!m_ewkView || m_hasRequestedFullScreen)
        return;

    m_hasRequestedFullScreen = true;

    WebFullScreenManagerProxy* manager = m_page->fullScreenManager();
    manager->willEnterFullScreen();
    m_ewkView->enterFullScreen();
    manager->didEnterFullScreen();
}

void WebViewEfl::exitFullScreen()
{
    if (!m_ewkView || !m_hasRequestedFullScreen)
        return;

    m_hasRequestedFullScreen = false;

    WebFullScreenManagerProxy* manager = m_page->fullScreenManager();
    manager->willExitFullScreen();
    m_ewkView->exitFullScreen();
    manager->didExitFullScreen();
}
#endif // ENABLE(FULLSCREEN_API)

void WebViewEfl::didFinishLoadingDataForCustomContentProvider(const String&, const IPC::DataReference&)
{
    notImplemented();
}

#if ENABLE(INPUT_TYPE_COLOR)
void WebViewEfl::initializeColorPickerClient(const WKColorPickerClientBase* client)
{
    m_colorPickerClient.initialize(client);
}

PassRefPtr<WebColorPicker> WebViewEfl::createColorPicker(WebPageProxy* page, const WebCore::Color& color, const WebCore::IntRect&)
{
    return WebColorPickerEfl::create(this, page, color);
}
#endif

} // namespace WebKit
