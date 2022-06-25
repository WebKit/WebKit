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

#include "APIViewClient.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "TouchGestureController.h"
#include "WPEView.h"
#include "WebContextMenuProxy.h"
#include "WebContextMenuProxyWPE.h"
#include "WebKitPopupMenu.h"
#include <WebCore/ActivityState.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/NotImplemented.h>

#if ENABLE(ACCESSIBILITY)
#include <atk/atk.h>
#endif

namespace WebKit {

PageClientImpl::PageClientImpl(WKWPE::View& view)
    : m_view(view)
{
}

PageClientImpl::~PageClientImpl() = default;

struct wpe_view_backend* PageClientImpl::viewBackend()
{
    return m_view.backend();
}

IPC::Attachment PageClientImpl::hostFileDescriptor()
{
    return IPC::Attachment(UnixFileDescriptor(wpe_view_backend_get_renderer_host_fd(m_view.backend()), UnixFileDescriptor::Adopt));
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return makeUnique<DrawingAreaProxyCoordinatedGraphics>(m_view.page(), process);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region&)
{
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, WebCore::ScrollIsAnimated)
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

void PageClientImpl::handleDownloadRequest(DownloadProxy& download)
{
    m_view.handleDownloadRequest(download);
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
    auto& touchGestureController = m_view.touchGestureController();

    auto generatedEvent = touchGestureController.handleEvent(touchPoint);
    WTF::switchOn(generatedEvent,
        [](TouchGestureController::NoEvent&) { },
        [&](TouchGestureController::ClickEvent& clickEvent)
        {
            auto* event = &clickEvent.event;

            // Mouse motion towards the point of the click.
            event->type = wpe_input_pointer_event_type_motion;
            page.handleMouseEvent(NativeWebMouseEvent(event, page.deviceScaleFactor()));

            event->type = wpe_input_pointer_event_type_button;
            event->button = 1;

            // Mouse down on the point of the click.
            event->state = 1;
            event->modifiers |= wpe_input_pointer_modifier_button1;
            page.handleMouseEvent(NativeWebMouseEvent(event, page.deviceScaleFactor()));

            // Mouse up on the same location.
            event->state = 0;
            event->modifiers &= ~wpe_input_pointer_modifier_button1;
            page.handleMouseEvent(NativeWebMouseEvent(event, page.deviceScaleFactor()));
        },
        [&](TouchGestureController::AxisEvent& axisEvent)
        {
#if WPE_CHECK_VERSION(1, 5, 0)
            auto* event = &axisEvent.event.base;
#else
            auto* event = &axisEvent.event;
#endif
            if (event->type != wpe_input_axis_event_type_null) {
                page.handleWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor(),
                    axisEvent.phase, WebWheelEvent::Phase::PhaseNone));
            }
        });
}
#endif

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&)
{
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    if (!m_view.client().isGLibBasedAPI())
        return nullptr;
    return WebKitPopupMenu::create(m_view, page);
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyWPE::create(page, WTFMove(context), userData);
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

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    m_view.willStartLoad();
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

void PageClientImpl::refView()
{
}

void PageClientImpl::derefView()
{
}

#if ENABLE(VIDEO) && USE(GSTREAMER)
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
        if (!m_view.setFullScreen(true))
            fullScreenManagerProxy->didExitFullScreen();
    }
}

void PageClientImpl::exitFullScreen()
{
    if (!isFullScreen())
        return;

    WebFullScreenManagerProxy* fullScreenManagerProxy = m_view.page().fullScreenManager();
    if (fullScreenManagerProxy) {
        fullScreenManagerProxy->willExitFullScreen();
        if (!m_view.setFullScreen(false))
            fullScreenManagerProxy->didEnterFullScreen();

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

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, const WebCore::IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

#if ENABLE(ACCESSIBILITY)
AtkObject* PageClientImpl::accessible()
{
    return ATK_OBJECT(m_view.accessible());
}
#endif

void PageClientImpl::didChangeWebPageID() const
{
    m_view.didChangePageID();
}

void PageClientImpl::sendMessageToWebView(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    m_view.didReceiveUserMessage(WTFMove(message), WTFMove(completionHandler));
}

void PageClientImpl::setInputMethodState(std::optional<InputMethodState>&& state)
{
    m_view.setInputMethodState(WTFMove(state));
}

void PageClientImpl::selectionDidChange()
{
    m_view.selectionDidChange();
}

} // namespace WebKit
