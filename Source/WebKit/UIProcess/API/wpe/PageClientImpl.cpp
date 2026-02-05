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
#include "NativeWebTouchEvent.h"
#include "NativeWebWheelEvent.h"
#include "TouchGestureController.h"
#include "ViewSnapshotStore.h"
#include "WPEWebViewLegacy.h"
#include "WPEWebViewPlatform.h"
#include "WebColorPicker.h"
#include "WebContextMenuProxy.h"
#include "WebContextMenuProxyWPE.h"
#include "WebDataListSuggestionsDropdown.h"
#include "WebDateTimePicker.h"
#include "WebKitPopupMenu.h"
#include <WebCore/ActivityState.h>
#include <WebCore/Cursor.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SystemSettings.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(ATK)
#include <atk/atk.h>
#endif

#if ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

#if USE(LIBWPE)
#include <wpe/wpe.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageClientImpl);

PageClientImpl::PageClientImpl(WKWPE::View& view)
    : m_view(view)
{
}

PageClientImpl::~PageClientImpl() = default;

#if USE(LIBWPE)
struct wpe_view_backend* PageClientImpl::viewBackend()
{
    return m_view.backend();
}
#endif

#if ENABLE(WPE_PLATFORM)
WPEView* PageClientImpl::wpeView() const
{
    return m_view.wpeView();
}
#endif

#if USE(WPE_RENDERER)
UnixFileDescriptor PageClientImpl::hostFileDescriptor()
{
    if (!m_view.backend())
        return { };
    return UnixFileDescriptor { wpe_view_backend_get_renderer_host_fd(m_view.backend()), UnixFileDescriptor::Adopt };
}
#endif

Ref<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& webProcessProxy)
{
    return DrawingAreaProxyCoordinatedGraphics::create(m_view.page(), webProcessProxy);
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

bool PageClientImpl::isActiveViewVisible()
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

void PageClientImpl::didChangeContentSize(const WebCore::IntSize&)
{
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    m_view.setCursor(cursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    if (hiddenUntilMouseMoves)
        setCursor(WebCore::noneCursor());
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(WTF::move(command), undoOrRedo);
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

WebCore::IntPoint PageClientImpl::rootViewToScreen(const WebCore::IntPoint& point)
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
void PageClientImpl::doneWithTouchEvent(const WebTouchEvent& touchEvent, bool wasEventHandled)
{
    if (wasEventHandled) {
#if ENABLE(WPE_PLATFORM)
        // If the touch event was handled, we must interrupt any gesture detection sequence ongoing so that gestures
        // are not detected by engine itself.
        if (!m_view.wpeView())
            return;
        if (auto* gestureController = wpe_view_get_gesture_controller(m_view.wpeView()))
            wpe_gesture_controller_cancel(gestureController);
#endif
        return;
    }

#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView()) {
        if (touchEvent.isNativeWebTouchEvent())
            static_cast<WKWPE::ViewPlatform&>(m_view).handleGesture(static_cast<const NativeWebTouchEvent&>(touchEvent).nativeEvent());
        return;
    }
#endif

#if USE(LIBWPE)
    const struct wpe_input_touch_event_raw* touchPoint = touchEvent.isNativeWebTouchEvent() ? static_cast<const NativeWebTouchEvent&>(touchEvent).nativeFallbackTouchPoint() : nullptr;
    if (!touchPoint || touchPoint->type == wpe_input_touch_event_type_null)
        return;

    auto& page = m_view.page();
    auto& touchGestureController = static_cast<WKWPE::ViewLegacy&>(m_view).touchGestureController();

    auto generatedEvent = touchGestureController.handleEvent(touchPoint);
    WTF::switchOn(generatedEvent,
        [](TouchGestureController::NoEvent&) { },
        [&](TouchGestureController::ClickEvent& clickEvent)
        {
            auto* event = &clickEvent.event;

            // Mouse motion towards the point of the click.
            event->type = wpe_input_pointer_event_type_motion;
            page.handleMouseEvent(NativeWebMouseEvent(event, page.deviceScaleFactor(), WebMouseEventSyntheticClickType::OneFingerTap));

            event->type = wpe_input_pointer_event_type_button;
            event->button = 1;

            // Mouse down on the point of the click.
            event->state = 1;
            event->modifiers |= wpe_input_pointer_modifier_button1;
            page.handleMouseEvent(NativeWebMouseEvent(event, page.deviceScaleFactor(), WebMouseEventSyntheticClickType::OneFingerTap));

            // Mouse up on the same location.
            event->state = 0;
            event->modifiers &= ~wpe_input_pointer_modifier_button1;
            page.handleMouseEvent(NativeWebMouseEvent(event, page.deviceScaleFactor(), WebMouseEventSyntheticClickType::OneFingerTap));
        },
        [&](TouchGestureController::ContextMenuEvent&) {
            // FIXME: Generate contextmenuevent without accidentally generating mouseup/mousedown events
        },
        [](TouchGestureController::AxisEvent&) { });
#endif
}
#endif

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&)
{
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    if (!m_view.client().isGLibBasedAPI())
        return nullptr;
    return WebKitPopupMenu::create(m_view, page.popupMenuClient());
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyWPE::create(page, WTF::move(frameInfo), WTF::move(context), userData);
}
#endif

RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy&, const WebCore::Color& intialColor, const WebCore::IntRect&, ColorControlSupportsAlpha supportsAlpha, Vector<WebCore::Color>&&)
{
    return nullptr;
}

RefPtr<WebDataListSuggestionsDropdown> PageClientImpl::createDataListSuggestionsDropdown(WebPageProxy&)
{
    return nullptr;
}

RefPtr<WebDateTimePicker> PageClientImpl::createDateTimePicker(WebPageProxy& page)
{
    return nullptr;
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& context)
{
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView())
        static_cast<WKWPE::ViewPlatform&>(m_view).updateAcceleratedSurface(context.contextID);
#else
    UNUSED_PARAM(context);
#endif
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView())
        static_cast<WKWPE::ViewPlatform&>(m_view).updateAcceleratedSurface(0);
#endif
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& context)
{
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView())
        static_cast<WKWPE::ViewPlatform&>(m_view).updateAcceleratedSurface(context.contextID);
#endif
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, std::span<const uint8_t>)
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

void PageClientImpl::themeColorDidChange()
{
    m_view.themeColorDidChange();
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

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    if (m_fullscreenClientForTesting)
        return *m_fullscreenClientForTesting;

    return *this;
}

void PageClientImpl::setFullScreenClientForTesting(std::unique_ptr<WebFullScreenManagerProxyClient>&& client)
{
    m_fullscreenClientForTesting = WTF::move(client);
}

void PageClientImpl::closeFullScreenManager()
{
    notImplemented();
}

bool PageClientImpl::isFullScreen()
{
    return m_view.isFullScreen();
}

void PageClientImpl::enterFullScreen(WebCore::FloatSize, CompletionHandler<void(bool)>&& completionHandler)
{
    if (isFullScreen())
        return completionHandler(false);

    m_view.willEnterFullScreen(WTF::move(completionHandler));
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView()) {
        static_cast<WKWPE::ViewPlatform&>(m_view).enterFullScreen();
        return;
    }
#endif

#if USE(LIBWPE)
    WebFullScreenManagerProxy* fullScreenManagerProxy = m_view.page().fullScreenManager();
    if (fullScreenManagerProxy) {
        if (!static_cast<WKWPE::ViewLegacy&>(m_view).setFullScreen(true))
            fullScreenManagerProxy->requestExitFullScreen();
    }
#endif
}

void PageClientImpl::exitFullScreen(CompletionHandler<void()>&& completionHandler)
{
    if (!isFullScreen())
        return completionHandler();

    m_view.willExitFullScreen(WTF::move(completionHandler));
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView()) {
        static_cast<WKWPE::ViewPlatform&>(m_view).exitFullScreen();
        return;
    }
#endif

#if USE(LIBWPE)
    if (m_view.page().fullScreenManager()) {
        bool success = static_cast<WKWPE::ViewLegacy&>(m_view).setFullScreen(false);
        ASSERT_UNUSED(success, success);
    }
#endif
}

void PageClientImpl::beganEnterFullScreen(const WebCore::IntRect& /* initialFrame */, const WebCore::IntRect& /* finalFrame */, CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
    completionHandler(true);
}

void PageClientImpl::beganExitFullScreen(const WebCore::IntRect& /* initialFrame */, const WebCore::IntRect& /* finalFrame */, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteRequiresInteraction requiresInteraction, const WebCore::IntRect&, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (auto* view = m_view.wpeView()) {
        if (requiresInteraction == WebCore::DOMPasteRequiresInteraction::No) {
            auto* clipboard = wpe_display_get_clipboard(wpe_view_get_display(view));
            if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, WebCore::PasteboardCustomData::wpeType().characters()))) {
                Ref buffer = WebCore::SharedBuffer::create(bytes.get());
                if (WebCore::PasteboardCustomData::fromSharedBuffer(buffer.get()).origin() == originIdentifier) {
                    completionHandler(WebCore::DOMPasteAccessResponse::GrantedForGesture);
                    return;
                }
            }
        }
        // FIXME: add WebKitClipboardPermissionRequest support.
    }
#endif
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

#if USE(ATK)
AtkObject* PageClientImpl::accessible()
{
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView())
        return nullptr;
#endif

#if USE(LIBWPE)
    return ATK_OBJECT(static_cast<WKWPE::ViewLegacy&>(m_view).accessible());
#endif

    return nullptr;
}
#endif

bool PageClientImpl::effectiveAppearanceIsDark() const
{
    return WebCore::SystemSettings::singleton().darkMode().value_or(false);
}

void PageClientImpl::didChangeWebPageID() const
{
    m_view.didChangePageID();
}

void PageClientImpl::sendMessageToWebView(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    m_view.didReceiveUserMessage(WTF::move(message), WTF::move(completionHandler));
}

void PageClientImpl::setInputMethodState(std::optional<InputMethodState>&& state)
{
    m_view.setInputMethodState(WTF::move(state));
}

void PageClientImpl::selectionDidChange()
{
    m_view.selectionDidChange();
}

WebKitWebResourceLoadManager* PageClientImpl::webResourceLoadManager()
{
    return m_view.webResourceLoadManager();
}

void PageClientImpl::callAfterNextPresentationUpdate(CompletionHandler<void()>&& callback)
{
    m_view.callAfterNextPresentationUpdate(WTF::move(callback));
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&& clipRect)
{
#if ENABLE(WPE_PLATFORM)
    if (m_view.wpeView()) {
        auto snapshot = static_cast<WKWPE::ViewPlatform&>(m_view).takeViewSnapshot(WTF::move(clipRect));
        // FIXME Forward the Expected in https://webkit.org/b/300271
        if (snapshot)
            return WTF::move(snapshot.value());
    }
#else
    UNUSED_PARAM(clipRect);
#endif
    return nullptr;
}

} // namespace WebKit
