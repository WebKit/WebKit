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
#include "WPEView.h"

#include "APIPageConfiguration.h"
#include "APIViewClient.h"
#include "DrawingAreaProxy.h"
#include "EditingRange.h"
#include "EditorState.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebTouchEvent.h"
#include "NativeWebWheelEvent.h"
#include "TouchGestureController.h"
#include "WebPageGroup.h"
#include "WebProcessPool.h"
#include <WebCore/CompositionUnderline.h>
#if ENABLE(GAMEPAD)
#include <WebCore/GamepadProviderLibWPE.h>
#endif
#include <wpe/wpe.h>
#include <wtf/NeverDestroyed.h>

using namespace WebKit;

namespace WKWPE {

static Vector<View*>& viewsVector()
{
    static NeverDestroyed<Vector<View*>> vector;
    return vector;
}

View::View(struct wpe_view_backend* backend, const API::PageConfiguration& baseConfiguration)
    : m_client(makeUnique<API::ViewClient>())
    , m_touchGestureController(makeUnique<TouchGestureController>())
    , m_pageClient(makeUnique<PageClientImpl>(*this))
    , m_size { 800, 600 }
    , m_viewStateFlags { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsFocused, WebCore::ActivityState::IsVisible, WebCore::ActivityState::IsInWindow }
    , m_backend(backend)
{
    ASSERT(m_backend);

    auto configuration = baseConfiguration.copy();
    auto* preferences = configuration->preferences();
    if (!preferences && configuration->pageGroup()) {
        preferences = &configuration->pageGroup()->preferences();
        configuration->setPreferences(preferences);
    }
    if (preferences) {
        preferences->setAcceleratedCompositingEnabled(true);
        preferences->setForceCompositingMode(true);
        preferences->setThreadedScrollingEnabled(true);
        preferences->setWebGLEnabled(true);
    }

    auto* pool = configuration->processPool();
    m_pageProxy = pool->createWebPage(*m_pageClient, WTFMove(configuration));

#if ENABLE(MEMORY_SAMPLER)
    if (getenv("WEBKIT_SAMPLE_MEMORY"))
        pool->startMemorySampler(0);
#endif

    static struct wpe_view_backend_client s_backendClient = {
        // set_size
        [](void* data, uint32_t width, uint32_t height)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.setSize(WebCore::IntSize(width, height));
        },
        // frame_displayed
        [](void* data)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.frameDisplayed();
        },
        // activity_state_changed
        [](void* data, uint32_t state)
        {
            auto& view = *reinterpret_cast<View*>(data);
            OptionSet<WebCore::ActivityState::Flag> flags;
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
#if ENABLE(ACCESSIBILITY)
            auto& view = *reinterpret_cast<View*>(data);
            return view.accessible();
#else
            return nullptr;
#endif
        },
        // set_device_scale_factor
        [](void* data, float scale)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.page().setIntrinsicDeviceScaleFactor(scale);
        },
#if WPE_CHECK_VERSION(1, 13, 2)
        // target_refresh_rate_changed
        [](void* data, uint32_t rate)
        {
            auto& view = *reinterpret_cast<View*>(data);
            if (view.page().drawingArea())
                view.page().drawingArea()->targetRefreshRateDidChange(rate);
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
            auto& view = *reinterpret_cast<View*>(data);
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
            auto& view = *reinterpret_cast<View*>(data);
            if (event->type == wpe_input_pointer_event_type_button && event->state == 1)
                view.m_inputMethodFilter.cancelComposition();
            auto& page = view.page();
            page.handleMouseEvent(WebKit::NativeWebMouseEvent(event, page.deviceScaleFactor()));
        },
        // handle_axis_event
        [](void* data, struct wpe_input_axis_event* event)
        {
            auto& view = *reinterpret_cast<View*>(data);

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
                page.handleWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor(), phase, momentumPhase));
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
                page.handleWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor(), phase, momentumPhase));
            }
        },
        // handle_touch_event
        [](void* data, struct wpe_input_touch_event* event)
        {
            auto& view = *reinterpret_cast<View*>(data);
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
                            handledThroughGestureController = true;
                        }
                    });

                // In case of the axis event gesturing, the generic touch event handling should be skipped.
                // Exception to this are touch-up events that should still be handled just like the corresponding
                // touch-down events were.
                if (handledThroughGestureController && event->type != wpe_input_touch_event_type_up)
                    return;
            }

            page.handleTouchEvent(touchEvent);
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
            auto& view = *reinterpret_cast<View*>(data);
            view.page().fullScreenManager()->didEnterFullScreen();
        },
        // did_exit_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.page().fullScreenManager()->didExitFullScreen();
        },
        // request_enter_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.page().fullScreenManager()->requestRestoreFullScreen();
        },
        // request_exit_fullscreen
        [](void* data)
        {
            auto& view = *reinterpret_cast<View*>(data);
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

View::~View()
{
    viewsVector().removeAll(this);
#if ENABLE(ACCESSIBILITY)
    if (m_accessible)
        webkitWebViewAccessibleSetWebView(m_accessible.get(), nullptr);
#endif
}

void View::setClient(std::unique_ptr<API::ViewClient>&& client)
{
    if (!client)
        m_client = makeUnique<API::ViewClient>();
    else
        m_client = WTFMove(client);
}

void View::frameDisplayed()
{
    m_client->frameDisplayed(*this);
}

void View::handleDownloadRequest(DownloadProxy& download)
{
    m_client->handleDownloadRequest(*this, download);
}

void View::willStartLoad()
{
    m_client->willStartLoad(*this);
}

void View::didChangePageID()
{
    m_client->didChangePageID(*this);
}

void View::didReceiveUserMessage(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    m_client->didReceiveUserMessage(*this, WTFMove(message), WTFMove(completionHandler));
}

WebKitWebResourceLoadManager* View::webResourceLoadManager()
{
    return m_client->webResourceLoadManager();
}

void View::setInputMethodContext(WebKitInputMethodContext* context)
{
    m_inputMethodFilter.setContext(context);
}

WebKitInputMethodContext* View::inputMethodContext() const
{
    return m_inputMethodFilter.context();
}

void View::setInputMethodState(std::optional<InputMethodState>&& state)
{
    m_inputMethodFilter.setState(WTFMove(state));
}

void View::selectionDidChange()
{
    const auto& editorState = m_pageProxy->editorState();
    if (editorState.hasPostLayoutAndVisualData()) {
        m_inputMethodFilter.notifyCursorRect(editorState.visualData->caretRectAtStart);
        m_inputMethodFilter.notifySurrounding(editorState.postLayoutData->surroundingContext, editorState.postLayoutData->surroundingContextCursorPosition,
            editorState.postLayoutData->surroundingContextSelectionPosition);
    }
}

void View::setSize(const WebCore::IntSize& size)
{
    m_size = size;
    if (m_pageProxy->drawingArea())
        m_pageProxy->drawingArea()->setSize(size);
}

void View::setViewState(OptionSet<WebCore::ActivityState::Flag> flags)
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

void View::handleKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    auto filterResult = m_inputMethodFilter.filterKeyEvent(event);
    if (filterResult.handled)
        return;

    page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, event->pressed ? filterResult.keyText : String(), NativeWebKeyboardEvent::HandledByInputMethod::No, std::nullopt, std::nullopt));
}

void View::synthesizeCompositionKeyPress(const String& text, std::optional<Vector<WebCore::CompositionUnderline>>&& underlines, std::optional<EditingRange>&& selectionRange)
{
    // The Windows composition key event code is 299 or VK_PROCESSKEY. We need to
    // emit this code for web compatibility reasons when key events trigger
    // composition results. WPE doesn't have an equivalent, so we send VoidSymbol
    // here to WebCore. PlatformKeyEvent converts this code into VK_PROCESSKEY.
    static struct wpe_input_keyboard_event event = { 0, WPE_KEY_VoidSymbol, 0, true, 0 };
    page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(&event, text, NativeWebKeyboardEvent::HandledByInputMethod::Yes, WTFMove(underlines), WTFMove(selectionRange)));
}

void View::close()
{
    m_pageProxy->close();
}

#if ENABLE(FULLSCREEN_API)
bool View::setFullScreen(bool fullScreenState)
{
#if WPE_CHECK_VERSION(1, 11, 1)
    if (!wpe_view_backend_platform_set_fullscreen(m_backend, fullScreenState))
        return false;
#endif
    m_fullScreenModeActive = fullScreenState;
    return true;
};
#endif

#if ENABLE(ACCESSIBILITY)
WebKitWebViewAccessible* View::accessible() const
{
    if (!m_accessible)
        m_accessible = webkitWebViewAccessibleNew(const_cast<View*>(this));
    return m_accessible.get();
}
#endif

#if ENABLE(GAMEPAD)
WebKit::WebPageProxy* View::platformWebPageProxyForGamepadInput()
{
    const auto& views = viewsVector();
    if (views.isEmpty())
        return nullptr;

    struct wpe_view_backend* viewBackend = WebCore::GamepadProviderLibWPE::singleton().inputView();

    size_t index = notFound;

    if (viewBackend) {
        index = views.findIf([&](View* view) {
            return view->backend() == viewBackend
                && view->viewState().contains(WebCore::ActivityState::IsVisible)
                && view->viewState().contains(WebCore::ActivityState::IsFocused);
        });
    } else {
        index = views.findIf([](View* view) {
            return view->viewState().contains(WebCore::ActivityState::IsVisible)
                && view->viewState().contains(WebCore::ActivityState::IsFocused);
        });
    }

    if (index != notFound)
        return &(views[index]->page());

    return nullptr;
}
#endif

} // namespace WKWPE
