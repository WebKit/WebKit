/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "PageClientImpl.h"

#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "NotImplemented.h"
#include "WebContextMenuProxyWin.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyWin.h"
#include "WebView.h"
#include <WebCore/DOMPasteAccess.h>

namespace WebKit {
using namespace WebCore;

PageClientImpl::PageClientImpl(WebView& view)
    : m_view(view)
{
}

// PageClient's pure virtual functions
std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return std::make_unique<DrawingAreaProxyCoordinatedGraphics>(*m_view.page(), process);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region& region)
{
    m_view.setViewNeedsDisplay(region);
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, bool)
{
    notImplemented();
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    notImplemented();
    return { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    RECT clientRect;
    GetClientRect(m_view.window(), &clientRect);

    return IntRect(clientRect).size();
}

bool PageClientImpl::isViewWindowActive()
{
    return m_view.isWindowActive();
}

bool PageClientImpl::isViewFocused()
{
    return m_view.isFocused();
}

bool PageClientImpl::isViewVisible()
{
    return m_view.isVisible();
}

bool PageClientImpl::isViewInWindow()
{
    return m_view.isInWindow();
}

void PageClientImpl::PageClientImpl::processDidExit()
{
    notImplemented();
}

void PageClientImpl::didRelaunchProcess()
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String& newToolTip)
{
    m_view.setToolTip(newToolTip);
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    m_view.setCursor(cursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool /* hiddenUntilMouseMoves */)
{
    notImplemented();
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
{
    notImplemented();
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(WTFMove(command), undoOrRedo);
}

void PageClientImpl::clearAllEditCommands()
{
    m_undoController.clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return m_undoController.canUndoRedo(undoOrRedo);
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    m_undoController.executeUndoRedo(undoOrRedo);
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    return IntPoint();
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    return IntRect();
}

WebCore::IntPoint PageClientImpl::accessibilityScreenToRootView(const WebCore::IntPoint& point)
{
    return screenToRootView(point);
}

WebCore::IntRect PageClientImpl::rootViewToAccessibilityScreen(const WebCore::IntRect& rect)    
{
    return rootViewToScreen(rect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool wasEventHandled)
{
    notImplemented();
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyWin::create(&m_view, page);
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyWin::create(page, WTFMove(context), userData);
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& intialColor, const WebCore::IntRect&)
{
    return nullptr;
}
#endif

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    notImplemented();
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    notImplemented();
}

void PageClientImpl::pageClosed()
{
    notImplemented();
}

void PageClientImpl::preferencesDidChange()
{
    notImplemented();
}

void PageClientImpl::didChangeContentSize(const IntSize& size)
{
    notImplemented();
}

void PageClientImpl::handleDownloadRequest(DownloadProxy* download)
{
    notImplemented();
}

void PageClientImpl::didCommitLoadForMainFrame(const String& /* mimeType */, bool /* useCustomContentProvider */ )
{
    notImplemented();
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

void PageClientImpl::closeFullScreenManager()
{
    notImplemented();
}

bool PageClientImpl::isFullScreen()
{
    notImplemented();
    return false;
}

void PageClientImpl::enterFullScreen()
{
    notImplemented();
}

void PageClientImpl::exitFullScreen()
{
    notImplemented();
}

void PageClientImpl::beganEnterFullScreen(const IntRect& /* initialFrame */, const IntRect& /* finalFrame */)
{
    notImplemented();
}

void PageClientImpl::beganExitFullScreen(const IntRect& /* initialFrame */, const IntRect& /* finalFrame */)
{
    notImplemented();
}
#endif // ENABLE(FULLSCREEN_API)

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    notImplemented();
}
#endif // ENABLE(TOUCH_EVENTS)

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    notImplemented();
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, const IPC::DataReference&)
{
    notImplemented();
}

void PageClientImpl::navigationGestureDidBegin()
{
    notImplemented();
}

void PageClientImpl::navigationGestureWillEnd(bool, WebBackForwardListItem&)
{
    notImplemented();
}

void PageClientImpl::navigationGestureDidEnd(bool, WebBackForwardListItem&)
{
    notImplemented();
}

void PageClientImpl::navigationGestureDidEnd()
{
    notImplemented();
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem&)
{
    notImplemented();
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    notImplemented();
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    notImplemented();
}

void PageClientImpl::didFinishLoadForMainFrame()
{
    notImplemented();
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
    notImplemented();
}

void PageClientImpl::didChangeBackgroundColor()
{
    notImplemented();
}

void PageClientImpl::isPlayingAudioWillChange()
{
    notImplemented();
}

void PageClientImpl::isPlayingAudioDidChange()
{
    notImplemented();
}

void PageClientImpl::refView()
{
    notImplemented();
}

void PageClientImpl::derefView()
{
    notImplemented();
}

HWND PageClientImpl::viewWidget()
{
    return m_view.window();
}

void PageClientImpl::requestDOMPasteAccess(const IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

} // namespace WebKit
