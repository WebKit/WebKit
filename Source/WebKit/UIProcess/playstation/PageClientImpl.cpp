/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "PlayStationWebView.h"
#include "WebPageProxy.h"

namespace WebKit {

PageClientImpl::PageClientImpl(PlayStationWebView& view)
    : m_view(view)
{
}

// PageClient's pure virtual functions
std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& processProxy)
{
    return makeUnique<DrawingAreaProxyCoordinatedGraphics>(*m_view.page(), processProxy);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region& region)
{
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin)
{
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    return WebCore::FloatPoint { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    return WebCore::IntSize { };
}

bool PageClientImpl::isViewWindowActive()
{
    return m_view.isActive();
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
    notImplemented();
    return true;
}

void PageClientImpl::processDidExit()
{
}

void PageClientImpl::didRelaunchProcess()
{
}
void PageClientImpl::pageClosed()
{
}

void PageClientImpl::preferencesDidChange()
{
}

void PageClientImpl::toolTipChanged(const String&, const String&)
{
}

void PageClientImpl::didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider)
{
    notImplemented();
}

void PageClientImpl::handleDownloadRequest(DownloadProxy&)
{
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize& size)
{
    notImplemented();
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    notImplemented();
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool)
{
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes& attributes)
{
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo)
{
}

void PageClientImpl::clearAllEditCommands()
{
}

bool PageClientImpl::canUndoRedo(UndoOrRedo)
{
    return false;
}

void PageClientImpl::executeUndoRedo(UndoOrRedo)
{
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&)
{
}

WebCore::FloatRect PageClientImpl::convertToDeviceSpace(const WebCore::FloatRect& rect)
{
    return rect;
}

WebCore::FloatRect PageClientImpl::convertToUserSpace(const WebCore::FloatRect& rect)
{
    return rect;
}

WebCore::IntPoint PageClientImpl::screenToRootView(const WebCore::IntPoint& point)
{
    return point;
}

WebCore::IntRect PageClientImpl::rootViewToScreen(const WebCore::IntRect& rect)
{
    return rect;
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

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy&  pageProxy)
{
    notImplemented();
    return { };
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& context)
{
    notImplemented();
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext&)
{
    notImplemented();
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *(WebFullScreenManagerProxyClient*)this;
}
#endif

// Custom representations.
void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&)
{
}

void PageClientImpl::navigationGestureDidBegin()
{
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd()
{
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem&)
{
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::didFinishNavigation(API::Navigation*)
{
}

void PageClientImpl::didFailNavigation(API::Navigation*)
{
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
}

void PageClientImpl::didChangeBackgroundColor()
{
}

void PageClientImpl::isPlayingAudioWillChange()
{
}

void PageClientImpl::isPlayingAudioDidChange()
{
}

void PageClientImpl::refView()
{
}

void PageClientImpl::derefView()
{
}

void PageClientImpl::didRestoreScrollPosition()
{
}

WebCore::UserInterfaceLayoutDirection PageClientImpl::userInterfaceLayoutDirection()
{
    return WebCore::UserInterfaceLayoutDirection::LTR;
}

void PageClientImpl::requestDOMPasteAccess(const WebCore::IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

} // namespace WebKit
