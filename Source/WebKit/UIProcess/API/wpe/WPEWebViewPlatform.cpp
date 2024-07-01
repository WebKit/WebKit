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
#include "APIViewClient.h"
#include "AcceleratedBackingStoreDMABuf.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebTouchEvent.h"
#include "NativeWebWheelEvent.h"
#include "ScreenManager.h"
#include "WebPreferences.h"
#include <WebCore/Cursor.h>
#include <wpe/wpe-platform.h>

#if USE(CAIRO)
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>
#endif

#if USE(SKIA)
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END
#endif

using namespace WebKit;

namespace WKWPE {

ViewPlatform::ViewPlatform(WPEDisplay* display, const API::PageConfiguration& baseConfiguration)
    : View(baseConfiguration)
    , m_wpeView(adoptGRef(wpe_view_new(display)))
{
    ASSERT(m_wpeView);

    m_inputMethodFilter.setUseWPEPlatformEvents(true);
    m_size.setWidth(wpe_view_get_width(m_wpeView.get()));
    m_size.setHeight(wpe_view_get_height(m_wpeView.get()));
    m_pageProxy->setIntrinsicDeviceScaleFactor(wpe_view_get_scale(m_wpeView.get()));

    if (wpe_view_get_mapped(m_wpeView.get()))
        m_viewStateFlags.add(WebCore::ActivityState::IsVisible);
    if (auto* toplevel = wpe_view_get_toplevel(m_wpeView.get())) {
        m_viewStateFlags.add(WebCore::ActivityState::IsInWindow);
        if (wpe_toplevel_get_state(toplevel) & WPE_TOPLEVEL_STATE_ACTIVE)
            m_viewStateFlags.add(WebCore::ActivityState::WindowIsActive);
    }
    m_pageProxy->activityStateDidChange(m_viewStateFlags);

    if (auto* monitor = wpe_view_get_monitor(m_wpeView.get()))
        m_displayID = wpe_monitor_get_id(monitor);
    else
        m_displayID = ScreenManager::singleton().primaryDisplayID();
    m_pageProxy->windowScreenDidChange(m_displayID);

    g_signal_connect(m_wpeView.get(), "notify::mapped", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);

        OptionSet<WebCore::ActivityState> flagsToUpdate { WebCore::ActivityState::IsVisible };
        if (wpe_view_get_mapped(view)) {
            if (webView.m_viewStateFlags.contains(WebCore::ActivityState::IsVisible))
                return;

            webView.m_viewStateFlags.add(WebCore::ActivityState::IsVisible);
        } else {
            if (!webView.m_viewStateFlags.contains(WebCore::ActivityState::IsVisible))
                return;

            webView.m_viewStateFlags.remove(WebCore::ActivityState::IsVisible);
        }
        webView.page().activityStateDidChange(flagsToUpdate);
    }), this);
    g_signal_connect(m_wpeView.get(), "resized", G_CALLBACK(+[](WPEView* view, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.setSize(WebCore::IntSize(wpe_view_get_width(view), wpe_view_get_height(view)));
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::scale", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.page().setIntrinsicDeviceScaleFactor(wpe_view_get_scale(view));
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::monitor", G_CALLBACK(+[](WPEView*, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.updateDisplayID();
    }), this);
    g_signal_connect(m_wpeView.get(), "notify::toplevel", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);

        OptionSet<WebCore::ActivityState> flagsToUpdate { WebCore::ActivityState::IsInWindow };
        if (wpe_view_get_toplevel(view)) {
            if (webView.m_viewStateFlags.contains(WebCore::ActivityState::IsInWindow))
                return;

            webView.m_viewStateFlags.add(WebCore::ActivityState::IsInWindow);
        } else {
            if (!webView.m_viewStateFlags.contains(WebCore::ActivityState::IsInWindow))
                return;

            webView.m_viewStateFlags.remove(WebCore::ActivityState::IsInWindow);
        }
        webView.page().activityStateDidChange(flagsToUpdate);
    }), this);
    g_signal_connect_after(m_wpeView.get(), "event", G_CALLBACK(+[](WPEView* view, WPEEvent* event, gpointer userData) -> gboolean {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        switch (wpe_event_get_event_type(event)) {
        case WPE_EVENT_NONE:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case WPE_EVENT_POINTER_DOWN:
            webView.m_inputMethodFilter.cancelComposition();
            FALLTHROUGH;
        case WPE_EVENT_POINTER_UP:
        case WPE_EVENT_POINTER_MOVE:
        case WPE_EVENT_POINTER_ENTER:
        case WPE_EVENT_POINTER_LEAVE:
            webView.page().handleMouseEvent(WebKit::NativeWebMouseEvent(event));
            return TRUE;
        case WPE_EVENT_SCROLL:
            webView.page().handleNativeWheelEvent(WebKit::NativeWebWheelEvent(event));
            return TRUE;
        case WPE_EVENT_KEYBOARD_KEY_DOWN: {
            auto modifiers = wpe_event_get_modifiers(event);
            auto keyval = wpe_event_keyboard_get_keyval(event);
            if (modifiers & WPE_MODIFIER_KEYBOARD_CONTROL && modifiers & WPE_MODIFIER_KEYBOARD_SHIFT && keyval == WPE_KEY_G) {
                auto& preferences = webView.page().preferences();
                preferences.setResourceUsageOverlayVisible(!preferences.resourceUsageOverlayVisible());
                return TRUE;
            }
            auto filterResult = webView.m_inputMethodFilter.filterKeyEvent(event);
            if (!filterResult.handled)
                webView.page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, filterResult.keyText, webView.m_keyAutoRepeatHandler.keyPress(wpe_event_keyboard_get_keycode(event))));
            return TRUE;
        }
        case WPE_EVENT_KEYBOARD_KEY_UP: {
            webView.m_keyAutoRepeatHandler.keyRelease();
            auto filterResult = webView.m_inputMethodFilter.filterKeyEvent(event);
            if (!filterResult.handled)
                webView.page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, String(), false));
            return TRUE;
        }
        case WPE_EVENT_TOUCH_DOWN:
            // FIXME: gestures
#if ENABLE(TOUCH_EVENTS)
            webView.m_touchEvents.add(wpe_event_touch_get_sequence_id(event), event);
            webView.page().handleTouchEvent(NativeWebTouchEvent(event, webView.touchPointsForEvent(event)));
#endif
            return TRUE;
        case WPE_EVENT_TOUCH_UP:
        case WPE_EVENT_TOUCH_CANCEL: {
            // FIXME: gestures
#if ENABLE(TOUCH_EVENTS)
            webView.m_touchEvents.set(wpe_event_touch_get_sequence_id(event), event);
            auto points = webView.touchPointsForEvent(event);
            webView.m_touchEvents.remove(wpe_event_touch_get_sequence_id(event));
            webView.page().handleTouchEvent(NativeWebTouchEvent(event, WTFMove(points)));
#endif
            return TRUE;
        }
        case WPE_EVENT_TOUCH_MOVE:
            // FIXME: gestures
#if ENABLE(TOUCH_EVENTS)
            webView.m_touchEvents.set(wpe_event_touch_get_sequence_id(event), event);
            webView.page().handleTouchEvent(NativeWebTouchEvent(event, webView.touchPointsForEvent(event)));
#endif
            return TRUE;
        };
        return FALSE;
    }), this);
    g_signal_connect(m_wpeView.get(), "focus-in", G_CALLBACK(+[](WPEView* view, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        if (webView.m_viewStateFlags.contains(WebCore::ActivityState::IsFocused))
            return;

        OptionSet<WebCore::ActivityState> flagsToUpdate { WebCore::ActivityState::IsFocused };
        webView.m_viewStateFlags.add(WebCore::ActivityState::IsFocused);
        webView.m_inputMethodFilter.notifyFocusedIn();
        webView.page().activityStateDidChange(flagsToUpdate);
    }), this);
    g_signal_connect(m_wpeView.get(), "focus-out", G_CALLBACK(+[](WPEView* view, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        if (!webView.m_viewStateFlags.contains(WebCore::ActivityState::IsFocused))
            return;

        OptionSet<WebCore::ActivityState> flagsToUpdate { WebCore::ActivityState::IsFocused };
        webView.m_viewStateFlags.remove(WebCore::ActivityState::IsFocused);
        webView.m_inputMethodFilter.notifyFocusedOut();
        webView.page().activityStateDidChange(flagsToUpdate);
    }), this);
    g_signal_connect(m_wpeView.get(), "toplevel-state-changed", G_CALLBACK(+[](WPEView* view, WPEToplevelState previousState, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        auto state = wpe_view_get_toplevel_state(view);
        uint32_t changedMask = state ^ previousState;
        if (changedMask & WPE_TOPLEVEL_STATE_FULLSCREEN) {
            switch (webView.m_fullscreenState) {
            case WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen:
                if (state & WPE_TOPLEVEL_STATE_FULLSCREEN)
                    webView.didEnterFullScreen();
                break;
            case WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen:
                if (!(state & WPE_TOPLEVEL_STATE_FULLSCREEN))
                    webView.didExitFullScreen();
                break;
            case WebFullScreenManagerProxy::FullscreenState::InFullscreen:
                if (!(state & WPE_TOPLEVEL_STATE_FULLSCREEN) && webView.isFullScreen())
                    webView.requestExitFullScreen();
                break;
            case WebFullScreenManagerProxy::FullscreenState::NotInFullscreen:
                break;
            }
        }
        if (changedMask & WPE_TOPLEVEL_STATE_ACTIVE) {
            OptionSet<WebCore::ActivityState> flagsToUpdate;
            constexpr auto flagToCheck { WebCore::ActivityState::WindowIsActive };

            if (state & WPE_TOPLEVEL_STATE_ACTIVE) {
                if (!webView.m_viewStateFlags.contains(flagToCheck)) {
                    flagsToUpdate.add(flagToCheck);
                    webView.m_viewStateFlags.add(flagToCheck);
                }
            } else {
                if (webView.m_viewStateFlags.contains(flagToCheck)) {
                    flagsToUpdate.add(flagToCheck);
                    webView.m_viewStateFlags.remove(flagToCheck);
                }
            }
            if (!flagsToUpdate.isEmpty())
                webView.page().activityStateDidChange(flagsToUpdate);
        }
    }), this);
    g_signal_connect(m_wpeView.get(), "preferred-dma-buf-formats-changed", G_CALLBACK(+[](WPEView*, gpointer userData) {
        auto& webView = *reinterpret_cast<ViewPlatform*>(userData);
        webView.page().preferredBufferFormatsDidChange();
    }), this);
    m_backingStore = AcceleratedBackingStoreDMABuf::create(*m_pageProxy, m_wpeView.get());

    m_pageProxy->initializeWebPage();
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
    auto* monitor = wpe_view_get_monitor(m_wpeView.get());
    if (!monitor)
        return;

    auto displayID = wpe_monitor_get_id(monitor);
    if (displayID == m_displayID)
        return;

    m_displayID = displayID;
    m_pageProxy->windowScreenDidChange(m_displayID);
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

void ViewPlatform::synthesizeCompositionKeyPress(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<EditingRange>&&)
{
    // FIXME: implement.
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
