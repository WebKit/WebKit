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
#include "WebViewPrivate.h"

#include "NotImplemented.h"
#include "WebContext.h"
#include "WebPageGroup.h"

using namespace WebCore;

namespace WebKit {

WebView::WebView(WebContext* context, WebPageGroup* pageGroup)
{
    WebPageConfiguration webPageConfiguration;
    webPageConfiguration.pageGroup = pageGroup;

    // Need to call createWebPage after other data members, specifically m_visible, are initialized.
    m_page = context->createWebPage(*this, WTF::move(webPageConfiguration));

    m_page->pageGroup().preferences().setAcceleratedCompositingEnabled(true);
    m_page->pageGroup().preferences().setForceCompositingMode(true);

    char* debugVisualsEnvironment = getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS");
    bool showDebugVisuals = debugVisualsEnvironment && !strcmp(debugVisualsEnvironment, "1");
    m_page->pageGroup().preferences().setCompositingBordersVisible(showDebugVisuals);
    m_page->pageGroup().preferences().setCompositingRepaintCountersVisible(showDebugVisuals);
    
    notImplemented();
}

WebView::~WebView()
{
    if (m_page->isClosed())
        return;

    m_page->close();
}

PassRefPtr<WebView> WebView::create(WebContext* context, WebPageGroup* pageGroup)
{
    return adoptRef(new WebView(context, pageGroup));
}

void WebView::didChangeContentSize(WebCore::IntSize const&)
{
    notImplemented();
}

std::unique_ptr<WebKit::DrawingAreaProxy> WebView::createDrawingAreaProxy()
{
    notImplemented();
}

void WebView::setViewNeedsDisplay(WebCore::IntRect const&)
{
    notImplemented();
}

void WebView::displayView()
{
    notImplemented();
}

void WebView::scrollView(WebCore::IntRect const&, WebCore::IntSize const&)
{
    notImplemented();
}

void WebView::requestScroll(WebCore::FloatPoint const&, bool)
{
    notImplemented();
}

WebCore::IntSize WebView::viewSize()
{
    notImplemented();
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

void WebView::processDidExit()
{
    notImplemented();
}

void WebView::didRelaunchProcess()
{
    notImplemented();
}

void WebView::pageClosed()
{
    notImplemented();
}

void WebView::preferencesDidChange()
{
    notImplemented();
}

void WebView::toolTipChanged(WTF::String const&, WTF::String const&)
{
    notImplemented();
}

void WebView::didCommitLoadForMainFrame(WTF::String const&, bool)
{
    notImplemented();
}

void WebView::setCursor(WebCore::Cursor const&)
{
    notImplemented();
}

void WebView::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void WebView::didChangeViewportProperties(WebCore::ViewportAttributes const&)
{
    notImplemented();
}

bool WebView::isViewInWindow()
{
    notImplemented();
    return true;
}

void WebView::registerEditCommand(WTF::PassRefPtr<WebKit::WebEditCommandProxy>, WebKit::WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

void WebView::clearAllEditCommands()
{
    notImplemented();
}

bool WebView::canUndoRedo(WebKit::WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

void WebView::executeUndoRedo(WebKit::WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

WebCore::FloatRect WebView::convertToDeviceSpace(WebCore::FloatRect const& rect)
{
    notImplemented();
    return rect;
}

WebCore::IntPoint WebView::screenToRootView(WebCore::IntPoint const&)
{
    notImplemented();
}

WebCore::IntRect WebView::rootViewToScreen(WebCore::IntRect const&)
{
    notImplemented();
}

WebCore::FloatRect WebView::convertToUserSpace(WebCore::FloatRect const&)
{
    notImplemented();
}

void WebView::updateTextInputState()
{
    notImplemented();
}

void WebView::handleDownloadRequest(WebKit::DownloadProxy*)
{
    notImplemented();
}

void WebView::doneWithKeyEvent(WebKit::NativeWebKeyboardEvent const&, bool)
{
    notImplemented();
}

WTF::PassRefPtr<WebKit::WebPopupMenuProxy> WebView::createPopupMenuProxy(WebKit::WebPageProxy*)
{
    notImplemented();
}

WTF::PassRefPtr<WebKit::WebContextMenuProxy> WebView::createContextMenuProxy(WebKit::WebPageProxy*)
{
    notImplemented();
}

void WebView::setFindIndicator(WTF::PassRefPtr<WebKit::FindIndicator>, bool, bool)
{
    notImplemented();
}

void WebView::enterAcceleratedCompositingMode(WebKit::LayerTreeContext const&)
{
    notImplemented();
}

void WebView::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void WebView::updateAcceleratedCompositingMode(WebKit::LayerTreeContext const&)
{
    notImplemented();
}

void WebView::didFinishLoadingDataForCustomContentProvider(WTF::String const&, IPC::DataReference const&)
{
    notImplemented();
}

void WebView::didFinishLoadForMainFrame()
{
    notImplemented();
}

void WebView::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    notImplemented();
}


}
