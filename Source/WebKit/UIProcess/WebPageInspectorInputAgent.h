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

#pragma once

#include "WebEvent.h"
#include "WebKeyboardEvent.h"
#include "WebMouseEvent.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
}

namespace WebKit {

class NativeWebKeyboardEvent;
class WebPageProxy;

class WebPageInspectorInputAgent : public Inspector::InspectorAgentBase, public Inspector::InputBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(WebPageInspectorInputAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebPageInspectorInputAgent(Inspector::BackendDispatcher& backendDispatcher, WebPageProxy& page);
    ~WebPageInspectorInputAgent() override;

    void didProcessAllPendingKeyboardEvents();
    void didProcessAllPendingMouseEvents();
    void didProcessAllPendingWheelEvents();

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // Protocol handler
    void dispatchKeyEvent(const String& type, std::optional<int>&& modifiers, const String& text, const String& unmodifiedText, const String& code, const String& key, std::optional<int>&& windowsVirtualKeyCode, std::optional<int>&& nativeVirtualKeyCode, std::optional<bool>&& autoRepeat, std::optional<bool>&& isKeypad, std::optional<bool>&& isSystemKey, RefPtr<JSON::Array>&&, Ref<DispatchKeyEventCallback>&& callback) override;
    void dispatchMouseEvent(const String& type, int x, int y, std::optional<int>&& modifiers, const String& button, std::optional<int>&& buttons, std::optional<int>&& clickCount, std::optional<int>&& deltaX, std::optional<int>&& deltaY, Ref<DispatchMouseEventCallback>&& callback) override;
    void dispatchTapEvent(int x, int y, std::optional<int>&& modifiers, Ref<DispatchTapEventCallback>&& callback) override;
    void dispatchWheelEvent(int x, int y, std::optional<int>&& modifiers, std::optional<int>&& deltaX, std::optional<int>&& deltaY, Ref<DispatchWheelEventCallback>&& callback) override;

private:
    void platformDispatchKeyEvent(WebKeyboardEvent::Type type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<WebEvent::Modifier> modifiers, Vector<String>& commands, WallTime timestamp);
#if PLATFORM(MAC)
    void platformDispatchMouseEvent(const String& type, int x, int y, std::optional<int>&& modifier, const String& button, std::optional<int>&& clickCount, unsigned short buttons);
#endif

    Ref<Inspector::InputBackendDispatcher> m_backendDispatcher;
    WebPageProxy& m_page;
    // Keep track of currently active modifiers across multiple keystrokes.
    // Most platforms do not track current modifiers from synthesized events.
    unsigned m_currentModifiers { 0 };
    class KeyboardCallbacks;
    std::unique_ptr<KeyboardCallbacks> m_keyboardCallbacks;
    class MouseCallbacks;
    std::unique_ptr<MouseCallbacks> m_mouseCallbacks;
    class WheelCallbacks;
    std::unique_ptr<WheelCallbacks> m_wheelCallbacks;
};

} // namespace WebKit
