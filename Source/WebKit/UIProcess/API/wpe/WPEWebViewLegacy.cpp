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
#include "WPEWebViewLegacy.h"

#include "APIViewClient.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebTouchEvent.h"
#include "NativeWebWheelEvent.h"
#include "TouchGestureController.h"
#include "WebPreferences.h"
#include <wpe/wpe.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(GAMEPAD)
#include <WebCore/GamepadProviderLibWPE.h>
#endif

using namespace WebKit;

namespace WKWPE {

static Vector<ViewLegacy*>& viewsVector()
{
    static NeverDestroyed<Vector<ViewLegacy*>> vector;
    return vector;
}

ViewLegacy::ViewLegacy(struct wpe_view_backend* backend, const API::PageConfiguration& configuration)
    : m_backend(backend)
#if ENABLE(TOUCH_EVENTS)
    , m_touchGestureController(makeUnique<TouchGestureController>())
#endif
{
    ASSERT(m_backend);

    m_size = { 800, 600 };
    m_viewStateFlags = { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsFocused, WebCore::ActivityState::IsVisible, WebCore::ActivityState::IsInWindow };

    createWebPage(configuration);

    static struct wpe_view_backend_client s_backendClient = {
        // set_size
        [](void* data, uint32_t width, uint32_t height)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.setSize(WebCore::IntSize(width, height));
        },
        // frame_displayed
        [](void* data)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.frameDisplayed();
        },
        // activity_state_changed
        [](void* data, uint32_t state)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            OptionSet<WebCore::ActivityState> flags;
            if (state & wpe_view_activity_state_visible)
                flags.add(WebCore::ActivityState::IsVisible);
            if (state & wpe_view_activity_state_focused) {
                flags.add(WebCore::ActivityState::IsFocused);
                flags.add(WebCore::ActivityState::WindowIsActive);
            }
            if (state & wpe_view_activity_state_in_window)
                flags.add(WebCore::ActivityState::IsInWindow);
            view.setViewState(flags);
        },
#if WPE_CHECK_VERSION(1, 3, 0)
        // get_accessible
        [](void* data) -> void*
        {
#if USE(ATK)
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            return view.accessible();
#else
            UNUSED_PARAM(data);
            return nullptr;
#endif
        },
        // set_device_scale_factor
        [](void* data, float scale)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.page().setIntrinsicDeviceScaleFactor(scale);
        },
#if WPE_CHECK_VERSION(1, 13, 2)
        // target_refresh_rate_changed
        [](void*, uint32_t)
        {
        },
#else
        // padding
        nullptr,
#endif // WPE_CHECK_VERSION(1, 13, 0)
#else
        // padding
        nullptr,
        nullptr
#endif // WPE_CHECK_VERSION(1, 3, 0)
    };
    wpe_view_backend_set_backend_client(m_backend, &s_backendClient, this);

    static struct wpe_view_backend_input_client s_inputClient = {
        // handle_keyboard_event
        [](void* data, struct wpe_input_keyboard_event* event)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            if (event->pressed
                && event->modifiers & wpe_input_keyboard_modifier_control
                && event->modifiers & wpe_input_keyboard_modifier_shift
                && event->key_code == WPE_KEY_G) {
                auto& preferences = view.page().preferences();
                preferences.setResourceUsageOverlayVisible(!preferences.resourceUsageOverlayVisible());
                return;
            }
            view.handleKeyboardEvent(event);
        },
        // handle_pointer_event
        [](void* data, struct wpe_input_pointer_event* event)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            if (event->type == wpe_input_pointer_event_type_button && event->state == 1)
                view.m_inputMethodFilter.cancelComposition();
            auto& page = view.page();
            page.handleMouseEvent(WebKit::NativeWebMouseEvent(event, page.deviceScaleFactor()));
        },
        // handle_axis_event
        [](void* data, struct wpe_input_axis_event* event)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);

            // FIXME: We shouldn't hard-code this.
            enum Axis {
                Vertical,
                Horizontal
            };

            // We treat an axis motion event with a value of zero to be equivalent
            // to a 'stop' event. Motion events with zero motion don't exist naturally,
            // so this allows a backend to express 'stop' events without changing API.
            // The wheel event phase is adjusted accordingly.
            WebWheelEvent::Phase phase = WebWheelEvent::Phase::PhaseChanged;
            WebWheelEvent::Phase momentumPhase = WebWheelEvent::Phase::PhaseNone;

#if WPE_CHECK_VERSION(1, 5, 0)
            if (event->type & wpe_input_axis_event_type_mask_2d) {
                auto* event2D = reinterpret_cast<struct wpe_input_axis_2d_event*>(event);
                view.m_horizontalScrollActive = !!event2D->x_axis;
                view.m_verticalScrollActive = !!event2D->y_axis;
                if (!view.m_horizontalScrollActive && !view.m_verticalScrollActive)
                    phase = WebWheelEvent::Phase::PhaseEnded;

                auto& page = view.page();
                page.handleNativeWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor(), phase, momentumPhase));
                return;
            }
#endif

            switch (event->axis) {
            case Vertical:
                view.m_horizontalScrollActive = !!event->value;
                break;
            case Horizontal:
                view.m_verticalScrollActive = !!event->value;
                break;
            }

            bool shouldDispatch = !!event->value;
            if (!view.m_horizontalScrollActive && !view.m_verticalScrollActive) {
                shouldDispatch = true;
                phase = WebWheelEvent::Phase::PhaseEnded;
            }

            if (shouldDispatch) {
                auto& page = view.page();
                page.handleNativeWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor(), phase, momentumPhase));
            }
        },
        // handle_touch_event
        [](void* data, struct wpe_input_touch_event* event)
        {
#if ENABLE(TOUCH_EVENTS)
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            auto& page = view.page();

            WebKit::NativeWebTouchEvent touchEvent(event, page.deviceScaleFactor());

            // If already gesturing axis events, short-cut directly to the controller,
            // avoiding the usual roundtrip.
            auto& touchGestureController = *view.m_touchGestureController;
            if (touchGestureController.gesturedEvent() == TouchGestureController::GesturedEvent::Axis) {
                bool handledThroughGestureController = false;

                auto generatedEvent = touchGestureController.handleEvent(touchEvent.nativeFallbackTouchPoint());
                WTF::switchOn(generatedEvent,
                    [](TouchGestureController::NoEvent&) { },
                    [](TouchGestureController::ClickEvent&) { },
                    [](TouchGestureController::ContextMenuEvent&) { },
                    [&](TouchGestureController::AxisEvent& axisEvent)
                    {
#if WPE_CHECK_VERSION(1, 5, 0)
                        auto* event = &axisEvent.event.base;
#else
                        auto* event = &axisEvent.event;
#endif
                        if (event->type != wpe_input_axis_event_type_null) {
                            page.handleNativeWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor(),
                                axisEvent.phase, WebWheelEvent::Phase::PhaseNone));
                            handledThroughGestureController = true;
                        }
                    });
            }

            page.handleTouchEvent(touchEvent);
#endif
        },
        // padding
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    wpe_view_backend_set_input_client(m_backend, &s_inputClient, this);

#if ENABLE(FULLSCREEN_API) && WPE_CHECK_VERSION(1, 11, 1)
    static struct wpe_view_backend_fullscreen_client s_fullscreenClient = {
        // did_enter_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.page().fullScreenManager()->didEnterFullScreen();
        },
        // did_exit_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.page().fullScreenManager()->didExitFullScreen();
        },
        // request_enter_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.page().fullScreenManager()->requestRestoreFullScreen([](bool) { });
        },
        // request_exit_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<ViewLegacy*>(data);
            view.page().fullScreenManager()->requestExitFullScreen();
        },
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    wpe_view_backend_set_fullscreen_client(m_backend, &s_fullscreenClient, this);
#endif

    wpe_view_backend_initialize(m_backend);

    m_pageProxy->initializeWebPage();

    viewsVector().append(this);
}

ViewLegacy::~ViewLegacy()
{
    wpe_view_backend_set_backend_client(m_backend, nullptr, nullptr);
    wpe_view_backend_set_input_client(m_backend, nullptr, nullptr);
    // Although the fullscreen client is used for libwpe 1.11.1 and newer, we cannot
    // unregister it prior to 1.14.2 (see https://github.com/WebPlatformForEmbedded/libwpe/pull/129).
#if ENABLE(FULLSCREEN_API) && WPE_CHECK_VERSION(1, 14, 2)
    wpe_view_backend_set_fullscreen_client(m_backend, nullptr, nullptr);
#endif

    viewsVector().removeAll(this);
}

void ViewLegacy::setViewState(OptionSet<WebCore::ActivityState> flags)
{
    auto changedFlags = m_viewStateFlags ^ flags;
    m_viewStateFlags = flags;

    if (changedFlags.contains(WebCore::ActivityState::IsFocused)) {
        if (m_viewStateFlags.contains(WebCore::ActivityState::IsFocused))
            m_inputMethodFilter.notifyFocusedIn();
        else
            m_inputMethodFilter.notifyFocusedOut();
    }

    if (changedFlags)
        m_pageProxy->activityStateDidChange(changedFlags);

    if (!viewsVector().isEmpty() && viewState().contains(WebCore::ActivityState::IsVisible)) {
        if (viewsVector().first() != this) {
            viewsVector().removeAll(this);
            viewsVector().insert(0, this);
        }
    }
}

void ViewLegacy::handleKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    auto isAutoRepeat = false;
    if (event->pressed)
        isAutoRepeat = m_keyAutoRepeatHandler.keyPress(event->key_code);
    else
        m_keyAutoRepeatHandler.keyRelease();

    auto filterResult = m_inputMethodFilter.filterKeyEvent(event);
    if (filterResult.handled)
        return;

    page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, event->pressed ? filterResult.keyText : String(), isAutoRepeat, NativeWebKeyboardEvent::HandledByInputMethod::No, std::nullopt, std::nullopt));
}

void ViewLegacy::synthesizeCompositionKeyPress(const String& text, std::optional<Vector<WebCore::CompositionUnderline>>&& underlines, std::optional<EditingRange>&& selectionRange)
{
    // The Windows composition key event code is 299 or VK_PROCESSKEY. We need to
    // emit this code for web compatibility reasons when key events trigger
    // composition results. WPE doesn't have an equivalent, so we send VoidSymbol
    // here to WebCore. PlatformKeyEvent converts this code into VK_PROCESSKEY.
    static struct wpe_input_keyboard_event event = { 0, WPE_KEY_VoidSymbol, 0, true, 0 };
    page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(&event, text, false, NativeWebKeyboardEvent::HandledByInputMethod::Yes, WTFMove(underlines), WTFMove(selectionRange)));
}

#if ENABLE(FULLSCREEN_API)
bool ViewLegacy::setFullScreen(bool fullScreenState)
{
#if WPE_CHECK_VERSION(1, 11, 1)
    if (!wpe_view_backend_platform_set_fullscreen(m_backend, fullScreenState))
        return false;
#endif
    if (fullScreenState) {
        m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::InFullscreen;
        m_client->enterFullScreen(*this);
    } else {
        m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::NotInFullscreen;
        m_client->exitFullScreen(*this);
    }
    return true;
};
#endif

#if ENABLE(GAMEPAD)
WebKit::WebPageProxy* ViewLegacy::platformWebPageProxyForGamepadInput()
{
    const auto& views = viewsVector();
    if (views.isEmpty())
        return nullptr;

    struct wpe_view_backend* viewBackend = WebCore::GamepadProviderLibWPE::singleton().inputView();

    size_t index = notFound;

    if (viewBackend) {
        index = views.findIf([&](ViewLegacy* view) {
            return view->backend() == viewBackend
                && view->viewState().contains(WebCore::ActivityState::IsVisible)
                && view->viewState().contains(WebCore::ActivityState::IsFocused);
        });
    } else {
        index = views.findIf([](ViewLegacy* view) {
            return view->viewState().contains(WebCore::ActivityState::IsVisible)
                && view->viewState().contains(WebCore::ActivityState::IsFocused);
        });
    }

    if (index != notFound)
        return &(views[index]->page());

    return nullptr;
}
#endif

void ViewLegacy::callAfterNextPresentationUpdate(CompletionHandler<void()>&& callback)
{
    RELEASE_ASSERT(m_pageProxy->drawingArea());
    downcast<DrawingAreaProxyCoordinatedGraphics>(*m_pageProxy->drawingArea()).dispatchAfterEnsuringDrawing(WTFMove(callback));
}

} // namespace WKWPE
