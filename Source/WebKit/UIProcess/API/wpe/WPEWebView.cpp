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
#include "WPEWebView.h"

#include "APIPageConfiguration.h"
#include "APIViewClient.h"
#include "AcceleratedBackingStoreDMABuf.h"
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
#include <WebCore/Cursor.h>
#if ENABLE(GAMEPAD)
#include <WebCore/GamepadProviderLibWPE.h>
#endif
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>
#include <wpe/wpe.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

using namespace WebKit;

namespace WKWPE {

static Vector<View*>& viewsVector()
{
    static NeverDestroyed<Vector<View*>> vector;
    return vector;
}

#if ENABLE(WPE_PLATFORM)
View::View(struct wpe_view_backend* backend, WPEDisplay* display, const API::PageConfiguration& baseConfiguration)
#else
    View::View(struct wpe_view_backend* backend, const API::PageConfiguration& baseConfiguration)
#endif
    : m_client(makeUnique<API::ViewClient>())
#if ENABLE(TOUCH_EVENTS)
    , m_touchGestureController(makeUnique<TouchGestureController>())
#endif
    , m_pageClient(makeUnique<PageClientImpl>(*this))
    , m_size { 800, 600 }
    , m_viewStateFlags { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsFocused, WebCore::ActivityState::IsVisible, WebCore::ActivityState::IsInWindow }
    , m_backend(backend)
{
#if ENABLE(WPE_PLATFORM)
    ASSERT(m_backend || display);
#else
    ASSERT(m_backend);
#endif

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
    }

    auto* pool = configuration->processPool();
    if (!pool) {
        auto processPoolConfiguration = API::ProcessPoolConfiguration::create();
        pool = &WebProcessPool::create(processPoolConfiguration).leakRef();
        configuration->setProcessPool(pool);
    }
    m_pageProxy = pool->createWebPage(*m_pageClient, WTFMove(configuration));

#if ENABLE(WPE_PLATFORM)
    if (display) {
        m_wpeView = adoptGRef(wpe_view_new(display));
        m_size.setWidth(wpe_view_get_width(m_wpeView.get()));
        m_size.setHeight(wpe_view_get_height(m_wpeView.get()));
        g_signal_connect(m_wpeView.get(), "resized", G_CALLBACK(+[](WPEView* view, gpointer userData) {
            auto& webView = *reinterpret_cast<View*>(userData);
            webView.setSize(WebCore::IntSize(wpe_view_get_width(view), wpe_view_get_height(view)));
        }), this);
        page().setIntrinsicDeviceScaleFactor(wpe_view_get_scale(m_wpeView.get()));
        g_signal_connect(m_wpeView.get(), "notify::scale", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer userData) {
            auto& webView = *reinterpret_cast<View*>(userData);
            webView.page().setIntrinsicDeviceScaleFactor(wpe_view_get_scale(view));
        }), this);
        g_signal_connect_after(m_wpeView.get(), "event", G_CALLBACK(+[](WPEView* view, WPEEvent* event, gpointer userData) -> gboolean {
            auto& webView = *reinterpret_cast<View*>(userData);
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
                // FIXME: input methods
                webView.page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, String(), webView.m_keyAutoRepeatHandler.keyPress(wpe_event_keyboard_get_keycode(event))));
                return TRUE;
            }
            case WPE_EVENT_KEYBOARD_KEY_UP:
                // FIXME: input methods
                webView.m_keyAutoRepeatHandler.keyRelease();
                webView.page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event, String(), false));
                return TRUE;
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
            auto& webView = *reinterpret_cast<View*>(userData);
            if (webView.m_viewStateFlags.contains(WebCore::ActivityState::IsFocused))
                return;

            OptionSet<WebCore::ActivityState> flagsToUpdate { WebCore::ActivityState::IsFocused };
            webView.m_viewStateFlags.add(WebCore::ActivityState::IsFocused);
            if (!webView.m_viewStateFlags.contains(WebCore::ActivityState::WindowIsActive)) {
                flagsToUpdate.add(WebCore::ActivityState::WindowIsActive);
                webView.m_viewStateFlags.add(WebCore::ActivityState::WindowIsActive);
            }
            webView.m_inputMethodFilter.notifyFocusedIn();
            webView.page().activityStateDidChange(flagsToUpdate);
        }), this);
        g_signal_connect(m_wpeView.get(), "focus-out", G_CALLBACK(+[](WPEView* view, gpointer userData) {
            auto& webView = *reinterpret_cast<View*>(userData);
            if (!webView.m_viewStateFlags.contains(WebCore::ActivityState::IsFocused))
                return;

            OptionSet<WebCore::ActivityState> flagsToUpdate { WebCore::ActivityState::IsFocused };
            webView.m_viewStateFlags.remove(WebCore::ActivityState::IsFocused);
            if (webView.m_viewStateFlags.contains(WebCore::ActivityState::WindowIsActive)) {
                flagsToUpdate.add(WebCore::ActivityState::WindowIsActive);
                webView.m_viewStateFlags.remove(WebCore::ActivityState::WindowIsActive);
            }
            webView.m_inputMethodFilter.notifyFocusedOut();
            webView.page().activityStateDidChange(flagsToUpdate);
        }), this);
        g_signal_connect(m_wpeView.get(), "state-changed", G_CALLBACK(+[](WPEView* view, WPEViewState previousState, gpointer userData) {
            auto& webView = *reinterpret_cast<View*>(userData);
            auto state = wpe_view_get_state(view);
            uint32_t changedMask = state ^ previousState;
            if (changedMask & WPE_VIEW_STATE_FULLSCREEN) {
                switch (webView.m_fullscreenState) {
                case WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen:
                    if (state & WPE_VIEW_STATE_FULLSCREEN)
                        webView.didEnterFullScreen();
                    break;
                case WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen:
                    if (!(state & WPE_VIEW_STATE_FULLSCREEN))
                        webView.didExitFullScreen();
                    break;
                case WebFullScreenManagerProxy::FullscreenState::InFullscreen:
                    if (!(state & WPE_VIEW_STATE_FULLSCREEN) && webView.isFullScreen())
                        webView.requestExitFullScreen();
                    break;
                case WebFullScreenManagerProxy::FullscreenState::NotInFullscreen:
                    break;
                }
            }
        }), this);
        g_signal_connect(m_wpeView.get(), "preferred-dma-buf-formats-changed", G_CALLBACK(+[](WPEView*, gpointer userData) {
            auto& webView = *reinterpret_cast<View*>(userData);
            webView.page().preferredBufferFormatsDidChange();
        }), this);
        m_backingStore = AcceleratedBackingStoreDMABuf::create(*m_pageProxy, m_wpeView.get());
    }
#endif

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
    if (m_backend)
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
    if (m_backend)
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
    if (m_backend)
        wpe_view_backend_set_fullscreen_client(m_backend, &s_fullscreenClient, this);
#endif

    if (m_backend)
        wpe_view_backend_initialize(m_backend);

    m_pageProxy->initializeWebPage();

    viewsVector().append(this);
}

View::~View()
{
    if (m_backend) {
        wpe_view_backend_set_backend_client(m_backend, nullptr, nullptr);
        wpe_view_backend_set_input_client(m_backend, nullptr, nullptr);
        // Although the fullscreen client is used for libwpe 1.11.1 and newer, we cannot
        // unregister it prior to 1.15.2 (see https://github.com/WebPlatformForEmbedded/libwpe/pull/129).
#if ENABLE(FULLSCREEN_API) && WPE_CHECK_VERSION(1, 15, 2)
        wpe_view_backend_set_fullscreen_client(m_backend, nullptr, nullptr);
#endif
    }

    viewsVector().removeAll(this);

#if ENABLE(WPE_PLATFORM)
    if (m_wpeView)
        g_signal_handlers_disconnect_by_data(m_wpeView.get(), this);
    m_backingStore = nullptr;
#endif

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

void View::setViewState(OptionSet<WebCore::ActivityState> flags)
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

void View::synthesizeCompositionKeyPress(const String& text, std::optional<Vector<WebCore::CompositionUnderline>>&& underlines, std::optional<EditingRange>&& selectionRange)
{
    // The Windows composition key event code is 299 or VK_PROCESSKEY. We need to
    // emit this code for web compatibility reasons when key events trigger
    // composition results. WPE doesn't have an equivalent, so we send VoidSymbol
    // here to WebCore. PlatformKeyEvent converts this code into VK_PROCESSKEY.
    static struct wpe_input_keyboard_event event = { 0, WPE_KEY_VoidSymbol, 0, true, 0 };
    page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(&event, text, false, NativeWebKeyboardEvent::HandledByInputMethod::Yes, WTFMove(underlines), WTFMove(selectionRange)));
}

void View::close()
{
    m_pageProxy->close();
}

#if ENABLE(FULLSCREEN_API)
bool View::isFullScreen() const
{
    return m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen || m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::InFullscreen;
}

void View::willEnterFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::NotInFullscreen);
    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->willEnterFullScreen();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen;
}

#if ENABLE(WPE_PLATFORM)
void View::enterFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen);
    ASSERT(m_wpeView);

    if (m_client->enterFullScreen(*this))
        return;

    if (wpe_view_get_state(m_wpeView.get()) & WPE_VIEW_STATE_FULLSCREEN) {
        m_viewWasAlreadyInFullScreen = true;
        didEnterFullScreen();
        return;
    }

    m_viewWasAlreadyInFullScreen = false;
    wpe_view_fullscreen(m_wpeView.get());
}

void View::didEnterFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen);
    ASSERT(m_wpeView);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->didEnterFullScreen();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::InFullscreen;
}
#endif

void View::willExitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen || m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::InFullscreen);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->willExitFullScreen();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen;
}

#if ENABLE(WPE_PLATFORM)
void View::exitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen);
    ASSERT(m_wpeView);

    if (m_client->exitFullScreen(*this))
        return;

    if (!(wpe_view_get_state(m_wpeView.get()) & WPE_VIEW_STATE_FULLSCREEN) || m_viewWasAlreadyInFullScreen) {
        didExitFullScreen();
        return;
    }

    wpe_view_unfullscreen(m_wpeView.get());
}

void View::didExitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen);
    ASSERT(m_wpeView);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->didExitFullScreen();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::NotInFullscreen;
}

void View::requestExitFullScreen()
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen || m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::InFullscreen);
    ASSERT(m_wpeView);

    if (auto* fullScreenManagerProxy = page().fullScreenManager())
        fullScreenManagerProxy->requestExitFullScreen();
}
#endif

bool View::setFullScreen(bool fullScreenState)
{
#if ENABLE(WPE_PLATFORM)
    ASSERT(!m_wpeView);
#endif
#if WPE_CHECK_VERSION(1, 11, 1)
    if (m_backend && !wpe_view_backend_platform_set_fullscreen(m_backend, fullScreenState))
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

#if ENABLE(WPE_PLATFORM)
void View::updateAcceleratedSurface(uint64_t surfaceID)
{
    if (m_backingStore)
        m_backingStore->updateSurfaceID(surfaceID);
}

#if ENABLE(TOUCH_EVENTS)
Vector<WebKit::WebPlatformTouchPoint> View::touchPointsForEvent(WPEEvent* event)
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
#endif
#endif

void View::setCursor(const WebCore::Cursor& cursor)
{
#if ENABLE(WPE_PLATFORM)
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

    ASSERT(cursor.type() == WebCore::Cursor::Type::Custom);
    auto image = cursor.image();
    auto nativeImage = image->nativeImageForCurrentFrame();
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
    wpe_view_set_cursor_from_bytes(m_wpeView.get(), bytes.get(), width, height, hotspot.x(), hotspot.y());
#else
    UNUSED_PARAM(cursor);
#endif
}

} // namespace WKWPE
