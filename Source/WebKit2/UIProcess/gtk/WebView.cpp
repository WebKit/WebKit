/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebView.h"

#include "ChunkedUpdateDrawingAreaProxy.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebEventFactory.h"
#include "WebViewWidget.h"
#include "WebPageProxy.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {


void WebView::handleFocusInEvent(GtkWidget* widget)
{
    if (!(m_isPageActive)) {
        m_isPageActive = true;
        m_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
    }

    m_page->viewStateDidChange(WebPageProxy::ViewIsFocused);
}

void WebView::handleFocusOutEvent(GtkWidget* widget)
{
    m_isPageActive = false;
    m_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
}

WebView::WebView(WebContext* context, WebPageGroup* pageGroup)
    : m_isPageActive(true)
{
    m_page = context->createWebPage(this, pageGroup);

    m_viewWidget = static_cast<GtkWidget*>(g_object_new(WEB_VIEW_TYPE_WIDGET, NULL));
    ASSERT(m_viewWidget);

    m_page->initializeWebPage();

    WebViewWidget* webViewWidget = WEB_VIEW_WIDGET(m_viewWidget);
    webViewWidgetSetWebViewInstance(webViewWidget, this);
}

WebView::~WebView()
{
}

GdkWindow* WebView::getWebViewWindow()
{
    return gtk_widget_get_window(m_viewWidget);
}

void WebView::paint(GtkWidget* widget, GdkRectangle rect, cairo_t* cr)
{
    m_page->drawingArea()->paint(IntRect(rect), cr);
}

void WebView::setSize(GtkWidget*, IntSize windowSize)
{
    m_page->drawingArea()->setSize(windowSize, IntSize());
}

void WebView::handleKeyboardEvent(GdkEventKey* event)
{
    m_page->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event)));
}

void WebView::handleMouseEvent(GdkEvent* event, int currentClickCount)
{
    m_page->handleMouseEvent(WebEventFactory::createWebMouseEvent(event, currentClickCount));
}

void WebView::handleWheelEvent(GdkEventScroll* event)
{
    m_page->handleWheelEvent(WebEventFactory::createWebWheelEvent(event));
}

bool WebView::isActive()
{
    return m_isPageActive;
}

void WebView::close()
{
    m_page->close();
}

// PageClient's pure virtual functions
PassOwnPtr<DrawingAreaProxy> WebView::createDrawingAreaProxy()
{
    return ChunkedUpdateDrawingAreaProxy::create(this, m_page.get());
}

void WebView::setViewNeedsDisplay(const WebCore::IntRect&)
{
    notImplemented();
}

void WebView::displayView()
{
    notImplemented();
}

void WebView::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    notImplemented();
}

WebCore::IntSize WebView::viewSize()
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(m_viewWidget, &allocation);
    return IntSize(allocation.width, allocation.height);
}

bool WebView::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool WebView::isViewFocused()
{
    notImplemented();
    return true;
}

bool WebView::isViewVisible()
{
    notImplemented();
    return true;
}

bool WebView::isViewInWindow()
{
    notImplemented();
    return true;
}

void WebView::WebView::processDidCrash()
{
    notImplemented();
}

void WebView::didRelaunchProcess()
{
    notImplemented();
}

void WebView::takeFocus(bool)
{
    notImplemented();
}

void WebView::toolTipChanged(const String&, const String&)
{
    notImplemented();
}

void WebView::setCursor(const Cursor&)
{
    notImplemented();
}

void WebView::setViewportArguments(const WebCore::ViewportArguments&)
{
    notImplemented();
}

void WebView::registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

void WebView::clearAllEditCommands()
{
    notImplemented();
}

FloatRect WebView::convertToDeviceSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

FloatRect WebView::convertToUserSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

void WebView::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled)
{
    notImplemented();
}

void WebView::didNotHandleKeyEvent(const NativeWebKeyboardEvent& event)
{
    notImplemented();
}

PassRefPtr<WebPopupMenuProxy> WebView::createPopupMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

PassRefPtr<WebContextMenuProxy> WebView::createContextMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

void WebView::setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void WebView::pageDidEnterAcceleratedCompositing()
{
    notImplemented();
}

void WebView::pageDidLeaveAcceleratedCompositing()
{
    notImplemented();
}
#endif // USE(ACCELERATED_COMPOSITING)

void WebView::didCommitLoadForMainFrame(bool useCustomRepresentation)
{
}

void WebView::didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&)
{
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

void WebView::pageClosed()
{
    notImplemented();
}

void WebView::didChangeScrollbarsForMainFrame() const
{
}

void WebView::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}


} // namespace WebKit
