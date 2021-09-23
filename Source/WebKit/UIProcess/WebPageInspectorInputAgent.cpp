/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPageInspectorInputAgent.h"

#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "WebWheelEvent.h"
#include "WebPageProxy.h"
#include <wtf/MathExtras.h>
#include <wtf/HexNumber.h>

#include "WebPageMessages.h"

namespace WebKit {

using namespace Inspector;

namespace {

template<class T>
class CallbackList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~CallbackList()
    {
        for (const auto& callback : m_callbacks)
            callback->sendFailure("Page closed");
    }

    void append(Ref<T>&& callback)
    {
        m_callbacks.append(WTFMove(callback));
    }

    void sendSuccess()
    {
        for (const auto& callback : m_callbacks)
            callback->sendSuccess();
        m_callbacks.clear();
    }

private:
    Vector<Ref<T>> m_callbacks;
};

} // namespace

class WebPageInspectorInputAgent::KeyboardCallbacks : public CallbackList<Inspector::InputBackendDispatcherHandler::DispatchKeyEventCallback> {
};

class WebPageInspectorInputAgent::MouseCallbacks : public CallbackList<Inspector::InputBackendDispatcherHandler::DispatchMouseEventCallback> {
};

class WebPageInspectorInputAgent::WheelCallbacks : public CallbackList<Inspector::InputBackendDispatcherHandler::DispatchWheelEventCallback> {
};

WebPageInspectorInputAgent::WebPageInspectorInputAgent(Inspector::BackendDispatcher& backendDispatcher, WebPageProxy& page)
    : InspectorAgentBase("Input"_s)
    , m_backendDispatcher(InputBackendDispatcher::create(backendDispatcher, this))
    , m_page(page)
{
}

WebPageInspectorInputAgent::~WebPageInspectorInputAgent() = default;

void WebPageInspectorInputAgent::didProcessAllPendingKeyboardEvents()
{
    m_keyboardCallbacks->sendSuccess();
}

void WebPageInspectorInputAgent::didProcessAllPendingMouseEvents()
{
    m_page.setInterceptDrags(false);
    m_mouseCallbacks->sendSuccess();
}

void WebPageInspectorInputAgent::didProcessAllPendingWheelEvents()
{
    m_wheelCallbacks->sendSuccess();
}

void WebPageInspectorInputAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
    m_keyboardCallbacks = makeUnique<KeyboardCallbacks>();
    m_mouseCallbacks = makeUnique<MouseCallbacks>();
    m_wheelCallbacks = makeUnique<WheelCallbacks>();
}

void WebPageInspectorInputAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_keyboardCallbacks = nullptr;
    m_mouseCallbacks = nullptr;
    m_wheelCallbacks = nullptr;
}

static String keyIdentifierForKey(const String& key)
{
    if (key.length() == 1)
        return makeString("U+", hex(toASCIIUpper(key.characterAt(0)), 4));
    if (key == "Delete")
        return "U+007F";
    if (key == "Backspace")
        return "U+0008";
    if (key == "ArrowUp")
        return "Up";
    if (key == "ArrowDown")
        return "Down";
    if (key == "ArrowLeft")
        return "Left";
    if (key == "ArrowRight")
        return "Right";
    if (key == "Tab")
        return "U+0009";
    if (key == "Pause")
        return "Pause";
    if (key == "ScrollLock")
        return "Scroll";
    return key;
}

void WebPageInspectorInputAgent::dispatchKeyEvent(const String& type, std::optional<int>&& modifiers, const String& text, const String& unmodifiedText, const String& code, const String& key, std::optional<int>&& windowsVirtualKeyCode, std::optional<int>&& nativeVirtualKeyCode, std::optional<bool>&& autoRepeat, std::optional<bool>&& isKeypad, std::optional<bool>&& isSystemKey, RefPtr<JSON::Array>&& commands, Ref<Inspector::InputBackendDispatcherHandler::DispatchKeyEventCallback>&& callback)
{
    WebKit::WebEvent::Type eventType;
    if (type == "keyDown") {
        eventType = WebKit::WebEvent::KeyDown;
    } else if (type == "keyUp") {
        eventType = WebKit::WebEvent::KeyUp;
    } else {
        callback->sendFailure("Unsupported event type.");
        return;
    }
    OptionSet<WebEvent::Modifier> eventModifiers;
    if (modifiers)
        eventModifiers = eventModifiers.fromRaw(*modifiers);
    int eventWindowsVirtualKeyCode = 0;
    if (windowsVirtualKeyCode)
        eventWindowsVirtualKeyCode = *windowsVirtualKeyCode;
    int eventNativeVirtualKeyCode = 0;
    if (nativeVirtualKeyCode)
        eventNativeVirtualKeyCode = *nativeVirtualKeyCode;
    Vector<String> eventCommands;
    if (commands) {
      for (const auto& value : *commands) {
        String command;
        if (!value->asString(command)) {
          callback->sendFailure("Command must be string");
          return;
        }
        eventCommands.append(command);
      }
    }

    String keyIdentifier = keyIdentifierForKey(key);

    bool eventIsAutoRepeat = false;
    if (autoRepeat)
        eventIsAutoRepeat = *autoRepeat;
    bool eventIsKeypad = false;
    if (isKeypad)
        eventIsKeypad = *isKeypad;
    bool eventIsSystemKey = false;
    if (isSystemKey)
        eventIsSystemKey = *isSystemKey;
    WallTime timestamp = WallTime::now();

    // cancel any active drag on Escape
    if (eventType == WebKit::WebEvent::KeyDown && key == "Escape" && m_page.cancelDragIfNeeded()) {
        callback->sendSuccess();
        return;
    }

    m_keyboardCallbacks->append(WTFMove(callback));
    platformDispatchKeyEvent(
        eventType,
        text,
        unmodifiedText,
        key,
        code,
        keyIdentifier,
        eventWindowsVirtualKeyCode,
        eventNativeVirtualKeyCode,
        eventIsAutoRepeat,
        eventIsKeypad,
        eventIsSystemKey,
        eventModifiers,
        eventCommands,
        timestamp);
}

void WebPageInspectorInputAgent::dispatchMouseEvent(const String& type, int x, int y, std::optional<int>&& modifiers, const String& button, std::optional<int>&& buttons, std::optional<int>&& clickCount, std::optional<int>&& deltaX, std::optional<int>&& deltaY, Ref<DispatchMouseEventCallback>&& callback)
{
    WebEvent::Type eventType = WebEvent::NoType;
    if (type == "down")
        eventType = WebEvent::MouseDown;
    else if (type == "up")
        eventType = WebEvent::MouseUp;
    else if (type == "move")
        eventType = WebEvent::MouseMove;
    else {
        callback->sendFailure("Unsupported event type");
        return;
    }

    OptionSet<WebEvent::Modifier> eventModifiers;
    if (modifiers)
        eventModifiers = eventModifiers.fromRaw(*modifiers);

    WebMouseEvent::Button eventButton = WebMouseEvent::NoButton;
    if (!!button) {
        if (button == "left")
            eventButton = WebMouseEvent::LeftButton;
        else if (button == "middle")
            eventButton = WebMouseEvent::MiddleButton;
        else if (button == "right")
            eventButton = WebMouseEvent::RightButton;
        else if (button == "none")
            eventButton = WebMouseEvent::NoButton;
        else {
            callback->sendFailure("Unsupported eventButton");
            return;
        }
    }

    unsigned short eventButtons = 0;
    if (buttons)
        eventButtons = *buttons;

    int eventClickCount = 0;
    if (clickCount)
        eventClickCount = *clickCount;
    int eventDeltaX = 0;
    if (deltaX)
        eventDeltaX = *deltaX;
    int eventDeltaY = 0;
    if (deltaY)
        eventDeltaY = *deltaY;
    m_mouseCallbacks->append(WTFMove(callback));

    // Convert css coordinates to view coordinates (dip).
    double totalScale = m_page.pageScaleFactor() * m_page.viewScaleFactor();
    x = clampToInteger(roundf(x * totalScale));
    y = clampToInteger(roundf(y * totalScale));
    eventDeltaX = clampToInteger(roundf(eventDeltaX * totalScale));
    eventDeltaY = clampToInteger(roundf(eventDeltaY * totalScale));

    // We intercept any drags generated by this mouse event
    // to prevent them from creating actual drags in the host
    // operating system. This is turned off in the callback.
    m_page.setInterceptDrags(true);
#if PLATFORM(MAC)
    platformDispatchMouseEvent(type, x, y, WTFMove(modifiers), button, WTFMove(clickCount), eventButtons);
#elif PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)
    WallTime timestamp = WallTime::now();
    NativeWebMouseEvent event(
        eventType,
        eventButton,
        eventButtons,
        {x, y},
        WebCore::IntPoint(),
        eventDeltaX,
        eventDeltaY,
        0,
        eventClickCount,
        eventModifiers,
        timestamp);
    m_page.handleMouseEvent(event);
#endif
}

void WebPageInspectorInputAgent::dispatchTapEvent(int x, int y, std::optional<int>&& modifiers, Ref<DispatchTapEventCallback>&& callback) {
    m_page.sendWithAsyncReply(Messages::WebPage::FakeTouchTap(WebCore::IntPoint(x, y), modifiers ? *modifiers : 0), [callback]() {
        callback->sendSuccess();
    });
}

void WebPageInspectorInputAgent::dispatchWheelEvent(int x, int y, std::optional<int>&& modifiers, std::optional<int>&& deltaX, std::optional<int>&& deltaY, Ref<DispatchWheelEventCallback>&& callback)
{
    OptionSet<WebEvent::Modifier> eventModifiers;
    if (modifiers)
        eventModifiers = eventModifiers.fromRaw(*modifiers);

    float eventDeltaX = 0.0f;
    if (deltaX)
        eventDeltaX = *deltaX;
    float eventDeltaY = 0.0f;
    if (deltaY)
        eventDeltaY = *deltaY;
    m_wheelCallbacks->append(WTFMove(callback));

    // Convert css coordinates to view coordinates (dip).
    double totalScale = m_page.pageScaleFactor() * m_page.viewScaleFactor();
    x = clampToInteger(roundf(x * totalScale));
    y = clampToInteger(roundf(y * totalScale));

    WallTime timestamp = WallTime::now();
    WebCore::FloatSize delta = {-eventDeltaX, -eventDeltaY};
    WebCore::FloatSize wheelTicks = delta;
    wheelTicks.scale(1.0f / WebCore::Scrollbar::pixelsPerLineStep());
    WebWheelEvent webEvent(WebEvent::Wheel, {x, y}, {x, y}, delta, wheelTicks, WebWheelEvent::ScrollByPixelWheelEvent, eventModifiers, timestamp);
    NativeWebWheelEvent event(webEvent);
    m_page.handleWheelEvent(event);
}

} // namespace WebKit
