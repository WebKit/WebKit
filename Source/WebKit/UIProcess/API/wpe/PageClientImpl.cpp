/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "ScrollGestureController.h"
#include "WPEView.h"
#include "WebContextMenuProxy.h"
#include "WebContextMenuProxyWPE.h"
#include <WebCore/ActivityState.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {

PageClientImpl::PageClientImpl(WKWPE::View& view)
    : m_view(view)
    , m_scrollGestureController(std::make_unique<ScrollGestureController>())
{
}

PageClientImpl::~PageClientImpl() = default;

struct wpe_view_backend* PageClientImpl::viewBackend()
{
    return m_view.backend();
}

IPC::Attachment PageClientImpl::hostFileDescriptor()
{
    return wpe_view_backend_get_renderer_host_fd(m_view.backend());
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return std::make_unique<DrawingAreaProxyCoordinatedGraphics>(m_view.page(), process);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region&)
{
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, bool)
{
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    return { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    return m_view.size();
}

bool PageClientImpl::isViewWindowActive()
{
    return m_view.viewState().contains(WebCore::ActivityState::WindowIsActive);
}

bool PageClientImpl::isViewFocused()
{
    return m_view.viewState().contains(WebCore::ActivityState::IsFocused);
}

bool PageClientImpl::isViewVisible()
{
    return m_view.viewState().contains(WebCore::ActivityState::IsVisible);
}

bool PageClientImpl::isViewInWindow()
{
    return m_view.viewState().contains(WebCore::ActivityState::IsInWindow);
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

void PageClientImpl::didCommitLoadForMainFrame(const String&, bool)
{
}

void PageClientImpl::handleDownloadRequest(DownloadProxy* download)
{
    ASSERT(download);
    m_view.handleDownloadRequest(*download);
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize&)
{
}

void PageClientImpl::setCursor(const WebCore::Cursor&)
{
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool)
{
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
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

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
{
}

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& touchEvent, bool wasEventHandled)
{
    if (wasEventHandled)
        return;

    const struct wpe_input_touch_event_raw* touchPoint = touchEvent.nativeFallbackTouchPoint();
    if (touchPoint->type == wpe_input_touch_event_type_null)
        return;

    auto& page = m_view.page();

    if (m_scrollGestureController->handleEvent(touchPoint)) {
        struct wpe_input_axis_event* axisEvent = m_scrollGestureController->axisEvent();
        if (axisEvent->type != wpe_input_axis_event_type_null)
            page.handleWheelEvent(WebKit::NativeWebWheelEvent(axisEvent, m_view.page().deviceScaleFactor()));
        return;
    }

    struct wpe_input_pointer_event pointerEvent {
        wpe_input_pointer_event_type_null, touchPoint->time,
        touchPoint->x, touchPoint->y,
        1, 0, 0
    };

    switch (touchPoint->type) {
    case wpe_input_touch_event_type_down:
        pointerEvent.type = wpe_input_pointer_event_type_button;
        pointerEvent.state = 1;
        pointerEvent.modifiers |= wpe_input_pointer_modifier_button1;
        break;
    case wpe_input_touch_event_type_motion:
        pointerEvent.type = wpe_input_pointer_event_type_motion;
        pointerEvent.state = 1;
        break;
    case wpe_input_touch_event_type_up:
        pointerEvent.type = wpe_input_pointer_event_type_button;
        pointerEvent.state = 0;
        pointerEvent.modifiers &= ~wpe_input_pointer_modifier_button1;
        break;
    case wpe_input_pointer_event_type_null:
        ASSERT_NOT_REACHED();
        return;
    }

    page.handleMouseEvent(NativeWebMouseEvent(&pointerEvent, page.deviceScaleFactor()));
}
#endif

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&)
{
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy&)
{
    return nullptr;
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy&, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyWPE::create(WTFMove(context), userData);
}
#endif

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext&)
{
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, const IPC::DataReference&)
{
}

void PageClientImpl::navigationGestureDidBegin()
{
}

void PageClientImpl::navigationGestureWillEnd(bool, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd(bool, WebBackForwardListItem&)
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

void PageClientImpl::didFinishLoadForMainFrame()
{
}

void PageClientImpl::didFailLoadForMainFrame()
{
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
}

void PageClientImpl::didChangeBackgroundColor()
{
}

void PageClientImpl::refView()
{
}

void PageClientImpl::derefView()
{
}

#if ENABLE(VIDEO)
bool PageClientImpl::decidePolicyForInstallMissingMediaPluginsPermissionRequest(InstallMissingMediaPluginsPermissionRequest&)
{
    return false;
}
#endif

void PageClientImpl::didRestoreScrollPosition()
{
}

WebCore::UserInterfaceLayoutDirection PageClientImpl::userInterfaceLayoutDirection()
{
    return WebCore::UserInterfaceLayoutDirection::LTR;
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
    return m_view.isFullScreen();
}

void PageClientImpl::enterFullScreen()
{
    if (isFullScreen())
        return;

    WebFullScreenManagerProxy* fullScreenManagerProxy = m_view.page().fullScreenManager();
    if (fullScreenManagerProxy) {
        fullScreenManagerProxy->willEnterFullScreen();
        m_view.setFullScreen(true);
        fullScreenManagerProxy->didEnterFullScreen();
    }
}

void PageClientImpl::exitFullScreen()
{
    if (!isFullScreen())
        return;

    WebFullScreenManagerProxy* fullScreenManagerProxy = m_view.page().fullScreenManager();
    if (fullScreenManagerProxy) {
        fullScreenManagerProxy->willExitFullScreen();
        m_view.setFullScreen(false);
        fullScreenManagerProxy->didExitFullScreen();
    }
}

void PageClientImpl::beganEnterFullScreen(const WebCore::IntRect& /* initialFrame */, const WebCore::IntRect& /* finalFrame */)
{
    notImplemented();
}

void PageClientImpl::beganExitFullScreen(const WebCore::IntRect& /* initialFrame */, const WebCore::IntRect& /* finalFrame */)
{
    notImplemented();
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::requestDOMPasteAccess(const WebCore::IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

} // namespace WebKit
