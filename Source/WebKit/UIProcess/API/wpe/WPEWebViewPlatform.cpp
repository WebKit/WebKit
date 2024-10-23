/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WPEWebViewPlatform.h"

#if ENABLE(WPE_PLATFORM)
#include "APIPageConfiguration.h"
#include "APIViewClient.h"
#include "AcceleratedBackingStoreDMABuf.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebTouchEvent.h"
#include "NativeWebWheelEvent.h"
#include "ScreenManager.h"
#include "WebPreferences.h"
#include <WebCore/Cursor.h>

#if USE(CAIRO)
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>
#endif

#if USE(SKIA)
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include <skia/core/SkPixmap.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
IGNORE_CLANG_WARNINGS_END
#endif

using namespace WebKit;

namespace WKWPE {

ViewPlatform::ViewPlatform(WPEDisplay* display, const API::PageConfiguration& configuration)
    : m_wpeView(adoptGRef(wpe_view_new(display)))
{
    ASSERT(m_wpeView);

    m_inputMethodFilter.setUseWPEPlatformEvents(true);
    m_size.setWidth(wpe_view_get_width(m_wpeView.get()));
    m_size.setHeight(wpe_view_get_height(m_wpeView.get()));

    if (wpe_view_get_mapped(m_wpeView.get()))
        m_viewStateFlags.add(WebCore::ActivityState::IsVisible);
    if (wpe_view_get_has_focus(m_wpeView.get())) {
        m_viewStateFlags.add(WebCore::ActivityState::IsFocused);
        m_inputMethodFilter.notifyFocusedIn();
    }
    if (auto* toplevel = wpe_view_get_toplevel(m_wpeView.get())) {
        m_viewStateFlags.add(WebCore::ActivityState::IsInWindow);
        if (wpe_toplevel_get_state(toplevel) & WPE_TOPLEVEL_STATE_ACTIVE)
            m_viewStateFlags.add(WebCore::ActivityState::WindowIsActive);
    }

    if (auto* screen = wpe_view_get_screen(m_wpeView.get()))
        m_displayID = wpe_screen_get_id(screen);
    else
        m_displayID = ScreenManager::singleton().primaryDisplayID();

    g_signal_connect(m_wpeView.get(), "notify::mapped", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.activityStateChanged(WebCore::ActivityState::IsVisible, wpe_view_get_mapped(view));
    }), this);
    g_signal_connect(m_wpeView.get(), "resized", G_CALLBACK(+[](WPEView* view, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.setSize(WebCore::IntSize(wpe_view_get_width(view), wpe_view_get_height(view)));
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::scale", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.page().setIntrinsicDeviceScaleFactor(wpe_view_get_scale(view));
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::screen", G_CALLBACK(+[](WPEView*, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.updateDisplayID();
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::toplevel", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.activityStateChanged(WebCore::ActivityState::IsInWindow, !!wpe_view_get_toplevel(view));
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::has-focus", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        auto focused = wpe_view_get_has_focus(view);
        if (!webView.activityStateChanged(WebCore::ActivityState::IsFocused, focused))
            return;

        if (focused)
            webView.m_inputMethodFilter.notifyFocusedIn();
        else
            webView.m_inputMethodFilter.notifyFocusedOut();
    }), this);
    g_signal_connect_after(m_wpeView.get(), "event", G_CALLBACK(+[](WPEView* view, WPEEvent* event, gpointer userData) -> gboolean {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        return webView.handleEvent(event);
    }), this);
    g_signal_connect(m_wpeView.get(), "toplevel-state-changed", G_CALLBACK(+[](WPEView* view, WPEToplevelState previousState, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.toplevelStateChanged(previousState, wpe_view_get_toplevel_state(view));
    }), this);
    g_signal_connect(m_wpeView.get(), "preferred-dma-buf-formats-changed", G_CALLBACK(+[](WPEView*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.page().preferredBufferFormatsDidChange();
    }), this);

    createWebPage(configuration);
    m_pageProxy->setIntrinsicDeviceScaleFactor(wpe_view_get_scale(m_wpeView.get()));
    m_pageProxy->windowScreenDidChange(m_displayID);
    m_backingStore = AcceleratedBackingStoreDMABuf::create(*m_pageProxy, m_wpeView.get());

    auto& pageConfiguration = m_pageProxy->configuration();
    m_pageProxy->initializeWebPage(pageConfiguration.openedSite(), pageConfiguration.initialSandboxFlags());
}

ViewPlatform::~ViewPlatform()
{
    g_signal_handlers_disconnect_by_data(m_wpeView.get(), this);
    m_inputMethodFilter.setContext(nullptr);
    m_backingStore = nullptr;
}

#if ENABLE(FULLSCREEN_API)
static bool viewToplevelIsFullScreen(WPEToplevel* toplevel)
{
    return toplevel && (wpe_toplevel_get_state(toplevel) & WPE_TOPLEVEL_STATE_FULLSCREEN);
}

void ViewPlatform::enterFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen);

    if (m_client->enterFullScreen(*this))
        return;

    auto* toplevel = wpe_view_get_toplevel(m_wpeView.get());
    if (viewToplevelIsFullScreen(toplevel)) {
        m_viewWasAlreadyInFullScreen = true;
        didEnterFullScreen();
        return;
    }

    m_viewWasAlreadyInFullScreen = false;
    if (toplevel)
        wpe_toplevel_fullscreen(toplevel);
}

void ViewPlatform::didEnterFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->didEnterFullScreen();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::InFullscreen;
}

void ViewPlatform::exitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen);

    if (m_client->exitFullScreen(*this))
        return;

    auto* toplevel = wpe_view_get_toplevel(m_wpeView.get());
    if (!viewToplevelIsFullScreen(toplevel) || m_viewWasAlreadyInFullScreen) {
        didExitFullScreen();
        return;
    }

    if (toplevel)
        wpe_toplevel_unfullscreen(toplevel);
}

void ViewPlatform::didExitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->didExitFullScreen();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::NotInFullscreen;
}

void ViewPlatform::requestExitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen || m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::InFullscreen);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->requestExitFullScreen();
}
#endif // ENABLE(FULLSCREEN_API)

void ViewPlatform::updateAcceleratedSurface(uint64_t surfaceID)
{
    m_backingStore->updateSurfaceID(surfaceID);
}

RendererBufferFormat ViewPlatform::renderBufferFormat() const
{
    return m_backingStore->bufferFormat();
}

void ViewPlatform::updateDisplayID()
{
    auto* screen = wpe_view_get_screen(m_wpeView.get());
    if (!screen)
        return;

    auto displayID = wpe_screen_get_id(screen);
    if (displayID == m_displayID)
        return;

    m_displayID = displayID;
    m_pageProxy->windowScreenDidChange(m_displayID);
}

bool ViewPlatform::activityStateChanged(WebCore::ActivityState state, bool value)
{
    if (value) {
        if (m_viewStateFlags.contains(state))
            return false;

        m_viewStateFlags.add(state);
    } else {
        if (!m_viewStateFlags.contains(state))
            return false;

        m_viewStateFlags.remove(state);
    }
    page().activityStateDidChange({ state });
    return true;
}

void ViewPlatform::toplevelStateChanged(WPEToplevelState previousState, WPEToplevelState state)
{
    uint32_t changedMask = state ^ previousState;

#if ENABLE(FULLSCREEN_API)
    if (changedMask & WPE_TOPLEVEL_STATE_FULLSCREEN) {
        switch (m_fullscreenState) {
        case WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen:
            if (state & WPE_TOPLEVEL_STATE_FULLSCREEN)
                didEnterFullScreen();
            break;
        case WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen:
            if (!(state & WPE_TOPLEVEL_STATE_FULLSCREEN))
                didExitFullScreen();
            break;
        case WebFullScreenManagerProxy::FullscreenState::InFullscreen:
            if (!(state & WPE_TOPLEVEL_STATE_FULLSCREEN) && isFullScreen())
                requestExitFullScreen();
            break;
        case WebFullScreenManagerProxy::FullscreenState::NotInFullscreen:
            break;
        }
    }
#endif

    if (changedMask & WPE_TOPLEVEL_STATE_ACTIVE)
        activityStateChanged(WebCore::ActivityState::WindowIsActive, state & WPE_TOPLEVEL_STATE_ACTIVE);
}

#if ENABLE(TOUCH_EVENTS)
Vector<WebKit::WebPlatformTouchPoint> ViewPlatform::touchPointsForEvent(WPEEvent* event)
{
    auto stateForEvent = [](uint32_t id, WPEEvent* event) -> WebPlatformTouchPoint::State {
        if (wpe_event_touch_get_sequence_id(event) != id)
            return WebPlatformTouchPoint::State::Stationary;

        switch (wpe_event_get_event_type(event)) {
        case WPE_EVENT_TOUCH_DOWN:
            return WebPlatformTouchPoint::State::Pressed;
        case WPE_EVENT_TOUCH_UP:
            return WebPlatformTouchPoint::State::Released;
        case WPE_EVENT_TOUCH_MOVE:
            return WebPlatformTouchPoint::State::Moved;
        case WPE_EVENT_TOUCH_CANCEL:
            return WebPlatformTouchPoint::State::Cancelled;
        default:
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    };

    Vector<WebPlatformTouchPoint> points;
    points.reserveInitialCapacity(m_touchEvents.size());
    for (const auto& it : m_touchEvents) {
        auto* currentEvent = it.value.get();
        double x, y;
        wpe_event_get_position(currentEvent, &x, &y);
        WebCore::IntPoint position(x, y);
        points.append(WebPlatformTouchPoint(it.key, stateForEvent(it.key, currentEvent), position, position));
    }
    return points;
}
#endif // ENABLE(TOUCH_EVENTS)

gboolean ViewPlatform::handleEvent(WPEEvent* event)
{
    switch (wpe_event_get_event_type(event)) {
    case WPE_EVENT_NONE:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case WPE_EVENT_POINTER_DOWN:
        m_inputMethodFilter.cancelComposition();
        FALLTHROUGH;
    case WPE_EVENT_POINTER_UP:
    case WPE_EVENT_POINTER_MOVE:
    case WPE_EVENT_POINTER_ENTER:
    case WPE_EVENT_POINTER_LEAVE:
        page().handleMouseEvent(WebKit::NativeWebMouseEvent(event));
        return TRUE;
    case WPE_EVENT_SCROLL:
        page().handleNativeWheelEvent(WebKit::NativeWebWheelEvent(event));
        return TRUE;
    case WPE_EVENT_KEYBOARD_KEY_DOWN: {
        auto modifiers = wpe_event_get_modifiers(event);
        auto keyval = wpe_event_keyboard_get_keyval(event);
        if (modifiers & WPE_MODIFIER_KEYBOARD_CONTROL && modifiers & WPE_MODIFIER_KEYBOARD_SHIFT && keyval == WPE_KEY_G) {
            auto& preferences = page().preferences();
            preferences.setResourceUsageOverlayVisible(!preferences.resourceUsageOverlayVisible());
            return TRUE;
        }
        auto filterResult = m_inputMethodFilter.filterKeyEvent(event);
        if (!filterResult.handled)
            page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, filterResult.keyText, m_keyAutoRepeatHandler.keyPress(wpe_event_keyboard_get_keycode(event))));
        return TRUE;
    }
    case WPE_EVENT_KEYBOARD_KEY_UP: {
        m_keyAutoRepeatHandler.keyRelease();
        auto filterResult = m_inputMethodFilter.filterKeyEvent(event);
        if (!filterResult.handled)
            page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, String(), false));
        return TRUE;
    }
    case WPE_EVENT_TOUCH_DOWN:
#if ENABLE(TOUCH_EVENTS)
        m_touchEvents.set(wpe_event_touch_get_sequence_id(event), event);
        page().handleTouchEvent(NativeWebTouchEvent(event, touchPointsForEvent(event)));
#endif
        handleGesture(event);
        return TRUE;
    case WPE_EVENT_TOUCH_UP:
    case WPE_EVENT_TOUCH_CANCEL: {
#if ENABLE(TOUCH_EVENTS)
        m_touchEvents.set(wpe_event_touch_get_sequence_id(event), event);
        auto points = touchPointsForEvent(event);
        m_touchEvents.remove(wpe_event_touch_get_sequence_id(event));
        page().handleTouchEvent(NativeWebTouchEvent(event, WTFMove(points)));
#endif
        handleGesture(event);
        return TRUE;
    }
    case WPE_EVENT_TOUCH_MOVE:
#if ENABLE(TOUCH_EVENTS)
        m_touchEvents.set(wpe_event_touch_get_sequence_id(event), event);
        page().handleTouchEvent(NativeWebTouchEvent(event, touchPointsForEvent(event)));
#endif
        handleGesture(event);
        return TRUE;
    };
    return FALSE;
}

void ViewPlatform::handleGesture(WPEEvent* event)
{
    auto* gestureController = wpe_view_get_gesture_controller(m_wpeView.get());
    if (!gestureController)
        return;

    wpe_gesture_controller_handle_event(gestureController, event);

    if (wpe_event_get_event_type(event) == WPE_EVENT_TOUCH_DOWN)
        return;

    switch (wpe_gesture_controller_get_gesture(gestureController)) {
    case WPE_GESTURE_NONE:
        break;
    case WPE_GESTURE_TAP:
        if (wpe_event_get_event_type(event) == WPE_EVENT_TOUCH_MOVE)
            return;
        if (double x, y; wpe_gesture_controller_get_gesture_position(gestureController, &x, &y)) {
            // Mouse motion towards the point of the click.
            {
                GRefPtr<WPEEvent> simulatedEvent = adoptGRef(wpe_event_pointer_move_new(
                    WPE_EVENT_POINTER_MOVE, m_wpeView.get(), WPE_INPUT_SOURCE_TOUCHSCREEN, 0, static_cast<WPEModifiers>(0), x, y, 0, 0
                ));
                page().handleMouseEvent(WebKit::NativeWebMouseEvent(simulatedEvent.get()));
            }

            // Mouse down on the point of the click.
            {
                GRefPtr<WPEEvent> simulatedEvent = adoptGRef(wpe_event_pointer_button_new(
                    WPE_EVENT_POINTER_DOWN, m_wpeView.get(), WPE_INPUT_SOURCE_TOUCHSCREEN, 0, WPE_MODIFIER_POINTER_BUTTON1, 1, x, y, 1
                ));
                page().handleMouseEvent(WebKit::NativeWebMouseEvent(simulatedEvent.get()));
            }

            // Mouse up on the same location.
            {
                GRefPtr<WPEEvent> simulatedEvent = adoptGRef(wpe_event_pointer_button_new(
                    WPE_EVENT_POINTER_UP, m_wpeView.get(), WPE_INPUT_SOURCE_TOUCHSCREEN, 0, static_cast<WPEModifiers>(0), 1, x, y, 0
                ));
                page().handleMouseEvent(WebKit::NativeWebMouseEvent(simulatedEvent.get()));
            }
        }
        break;
    case WPE_GESTURE_DRAG:
        if (double x, y, dx, dy; wpe_gesture_controller_get_gesture_position(gestureController, &x, &y) && wpe_gesture_controller_get_gesture_delta(gestureController, &dx, &dy)) {
            GRefPtr<WPEEvent> simulatedScrollEvent = adoptGRef(wpe_event_scroll_new(
                m_wpeView.get(), WPE_INPUT_SOURCE_MOUSE, 0, static_cast<WPEModifiers>(0), dx, dy, TRUE, FALSE, x, y
            ));
            auto phase = wpe_gesture_controller_is_drag_begin(gestureController)
                ? WebWheelEvent::Phase::PhaseBegan
                : (wpe_event_get_event_type(event) == WPE_EVENT_TOUCH_UP) ? WebWheelEvent::Phase::PhaseEnded : WebWheelEvent::Phase::PhaseChanged;
            page().handleNativeWheelEvent(WebKit::NativeWebWheelEvent(simulatedScrollEvent.get(), phase));
        }
    }
}

void ViewPlatform::synthesizeCompositionKeyPress(const String& text, std::optional<Vector<WebCore::CompositionUnderline>>&& underlines, std::optional<EditingRange>&& selectionRange)
{
    page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(text, WTFMove(underlines), WTFMove(selectionRange)));
}

void ViewPlatform::setCursor(const WebCore::Cursor& cursor)
{
    if (!m_wpeView)
        return;

    if (cursor.type() == WebCore::Cursor::Type::Invalid)
        return;

    auto cursorName = [](const WebCore::Cursor& cursor) -> const char* {
        switch (cursor.type()) {
        case WebCore::Cursor::Type::Pointer:
            return "default";
        case WebCore::Cursor::Type::Cross:
            return "crosshair";
        case WebCore::Cursor::Type::Hand:
            return "pointer";
        case WebCore::Cursor::Type::IBeam:
            return "text";
        case WebCore::Cursor::Type::Wait:
            return "wait";
        case WebCore::Cursor::Type::Help:
            return "help";
        case WebCore::Cursor::Type::Move:
        case WebCore::Cursor::Type::MiddlePanning:
            return "move";
        case WebCore::Cursor::Type::EastResize:
        case WebCore::Cursor::Type::EastPanning:
            return "e-resize";
        case WebCore::Cursor::Type::NorthResize:
        case WebCore::Cursor::Type::NorthPanning:
            return "n-resize";
        case WebCore::Cursor::Type::NorthEastResize:
        case WebCore::Cursor::Type::NorthEastPanning:
            return "ne-resize";
        case WebCore::Cursor::Type::NorthWestResize:
        case WebCore::Cursor::Type::NorthWestPanning:
            return "nw-resize";
        case WebCore::Cursor::Type::SouthResize:
        case WebCore::Cursor::Type::SouthPanning:
            return "s-resize";
        case WebCore::Cursor::Type::SouthEastResize:
        case WebCore::Cursor::Type::SouthEastPanning:
            return "se-resize";
        case WebCore::Cursor::Type::SouthWestResize:
        case WebCore::Cursor::Type::SouthWestPanning:
            return "sw-resize";
        case WebCore::Cursor::Type::WestResize:
        case WebCore::Cursor::Type::WestPanning:
            return "w-resize";
        case WebCore::Cursor::Type::NorthSouthResize:
            return "ns-resize";
        case WebCore::Cursor::Type::EastWestResize:
            return "ew-resize";
        case WebCore::Cursor::Type::NorthEastSouthWestResize:
            return "nesw-resize";
        case WebCore::Cursor::Type::NorthWestSouthEastResize:
            return "nwse-resize";
        case WebCore::Cursor::Type::ColumnResize:
            return "col-resize";
        case WebCore::Cursor::Type::RowResize:
            return "row-resize";
        case WebCore::Cursor::Type::VerticalText:
            return "vertical-text";
        case WebCore::Cursor::Type::Cell:
            return "cell";
        case WebCore::Cursor::Type::ContextMenu:
            return "context-menu";
        case WebCore::Cursor::Type::Alias:
            return "alias";
        case WebCore::Cursor::Type::Progress:
            return "progress";
        case WebCore::Cursor::Type::NoDrop:
            return "no-drop";
        case WebCore::Cursor::Type::NotAllowed:
            return "not-allowed";
        case WebCore::Cursor::Type::Copy:
            return "copy";
        case WebCore::Cursor::Type::None:
            return "none";
        case WebCore::Cursor::Type::ZoomIn:
            return "zoom-in";
        case WebCore::Cursor::Type::ZoomOut:
            return "zoom-out";
        case WebCore::Cursor::Type::Grab:
            return "grab";
        case WebCore::Cursor::Type::Grabbing:
            return "grabbing";
        case WebCore::Cursor::Type::Custom:
            return nullptr;
        case WebCore::Cursor::Type::Invalid:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    if (const char* name = cursorName(cursor)) {
        wpe_view_set_cursor_from_name(m_wpeView.get(), name);
        return;
    }

#if USE(CAIRO)
    ASSERT(cursor.type() == WebCore::Cursor::Type::Custom);
    auto image = cursor.image();
    auto nativeImage = image->currentNativeImage();
    if (!nativeImage)
        return;

    auto surface = nativeImage->platformImage();
    auto width = cairo_image_surface_get_width(surface.get());
    auto height = cairo_image_surface_get_height(surface.get());
    auto stride = cairo_image_surface_get_stride(surface.get());
    auto* data = cairo_image_surface_get_data(surface.get());
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(data, height * stride, [](gpointer data) {
        cairo_surface_destroy(static_cast<cairo_surface_t*>(data));
    }, surface.leakRef()));

    WebCore::IntPoint hotspot = WebCore::determineHotSpot(image.get(), cursor.hotSpot());
    wpe_view_set_cursor_from_bytes(m_wpeView.get(), bytes.get(), width, height, stride, hotspot.x(), hotspot.y());
#elif USE(SKIA)
    auto nativeImage = cursor.image()->currentNativeImage();
    if (!nativeImage)
        return;

    SkPixmap pixmap;
    auto platformImage = nativeImage->platformImage();
    ASSERT(platformImage->peekPixels(&pixmap));

    platformImage->ref();
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(pixmap.addr(), pixmap.computeByteSize(), [](gpointer data) {
        static_cast<SkImage*>(data)->unref();
    }, platformImage.get()));

    WebCore::IntPoint hotspot = WebCore::determineHotSpot(cursor.image().get(), cursor.hotSpot());
    wpe_view_set_cursor_from_bytes(m_wpeView.get(), bytes.get(), pixmap.width(), pixmap.height(), pixmap.rowBytes(), hotspot.x(), hotspot.y());
#endif
}

#if ENABLE(POINTER_LOCK)
void ViewPlatform::requestPointerLock()
{
    if (wpe_view_lock_pointer(m_wpeView.get()))
        setCursor(WebCore::noneCursor());
}

void ViewPlatform::didLosePointerLock()
{
    if (wpe_view_unlock_pointer(m_wpeView.get()))
        setCursor(WebCore::pointerCursor());
}
#endif

void ViewPlatform::callAfterNextPresentationUpdate(CompletionHandler<void()>&& callback)
{
    RELEASE_ASSERT(!m_nextPresentationUpdateCallback);
    m_nextPresentationUpdateCallback = WTFMove(callback);
    if (!m_bufferRenderedID) {
        m_bufferRenderedID = g_signal_connect_after(m_wpeView.get(), "buffer-rendered", G_CALLBACK(+[](WPEView* view, WPEBuffer*, gpointer userData) {
            auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
            if (webView.m_nextPresentationUpdateCallback)
                webView.m_nextPresentationUpdateCallback();
        }), this);
    }
}

} // namespace WKWPE

#endif // ENABLE(WPE_PLATFORM)
