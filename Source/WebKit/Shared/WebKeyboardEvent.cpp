/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebKeyboardEvent.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/KeypressCommand.h>

namespace WebKit {

WebKeyboardEvent::WebKeyboardEvent()
{
}

#if USE(APPKIT)

WebKeyboardEvent::WebKeyboardEvent(WebEvent&& event, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, const Vector<WebCore::KeypressCommand>& commands, bool isAutoRepeat, bool isKeypad, bool isSystemKey)
    : WebEvent(WTFMove(event))
    , m_text(text)
    , m_unmodifiedText(unmodifiedText)
    , m_key(key)
    , m_code(code)
    , m_keyIdentifier(keyIdentifier)
    , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
    , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
    , m_macCharCode(macCharCode)
    , m_handledByInputMethod(handledByInputMethod)
    , m_commands(commands)
    , m_isAutoRepeat(isAutoRepeat)
    , m_isKeypad(isKeypad)
    , m_isSystemKey(isSystemKey)
{
    ASSERT(isKeyboardEventType(type()));
}

#elif PLATFORM(GTK)

WebKeyboardEvent::WebKeyboardEvent(WebEvent&& event, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, std::optional<Vector<WebCore::CompositionUnderline>>&& preeditUnderlines, std::optional<EditingRange>&& preeditSelectionRange, Vector<String>&& commands, bool isKeypad)
    : WebEvent(WTFMove(event))
    , m_text(text)
    , m_unmodifiedText(text)
    , m_key(key)
    , m_code(code)
    , m_keyIdentifier(keyIdentifier)
    , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
    , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
    , m_macCharCode(0)
    , m_handledByInputMethod(handledByInputMethod)
    , m_preeditUnderlines(WTFMove(preeditUnderlines))
    , m_preeditSelectionRange(WTFMove(preeditSelectionRange))
    , m_commands(WTFMove(commands))
    , m_isAutoRepeat(false)
    , m_isKeypad(isKeypad)
    , m_isSystemKey(false)
{
    ASSERT(isKeyboardEventType(type()));
}

#elif PLATFORM(IOS_FAMILY)

WebKeyboardEvent::WebKeyboardEvent(WebEvent&& event, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, bool isAutoRepeat, bool isKeypad, bool isSystemKey)
    : WebEvent(WTFMove(event))
    , m_text(text)
    , m_unmodifiedText(unmodifiedText)
    , m_key(key)
    , m_code(code)
    , m_keyIdentifier(keyIdentifier)
    , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
    , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
    , m_macCharCode(macCharCode)
    , m_handledByInputMethod(handledByInputMethod)
    , m_isAutoRepeat(isAutoRepeat)
    , m_isKeypad(isKeypad)
    , m_isSystemKey(isSystemKey)
{
    ASSERT(isKeyboardEventType(type()));
}

#elif USE(LIBWPE)

WebKeyboardEvent::WebKeyboardEvent(WebEvent&& event, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, std::optional<Vector<WebCore::CompositionUnderline>>&& preeditUnderlines, std::optional<EditingRange>&& preeditSelectionRange, bool isKeypad)
    : WebEvent(WTFMove(event))
    , m_text(text)
    , m_unmodifiedText(text)
    , m_key(key)
    , m_code(code)
    , m_keyIdentifier(keyIdentifier)
    , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
    , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
    , m_macCharCode(0)
    , m_handledByInputMethod(handledByInputMethod)
    , m_preeditUnderlines(WTFMove(preeditUnderlines))
    , m_preeditSelectionRange(WTFMove(preeditSelectionRange))
    , m_isAutoRepeat(false)
    , m_isKeypad(isKeypad)
    , m_isSystemKey(false)
{
    ASSERT(isKeyboardEventType(type()));
}

#else

WebKeyboardEvent::WebKeyboardEvent(WebEvent&& event, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey)
    : WebEvent(WTFMove(event))
    , m_text(text)
    , m_unmodifiedText(unmodifiedText)
    , m_key(key)
    , m_code(code)
    , m_keyIdentifier(keyIdentifier)
    , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
    , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
    , m_macCharCode(macCharCode)
    , m_isAutoRepeat(isAutoRepeat)
    , m_isKeypad(isKeypad)
    , m_isSystemKey(isSystemKey)
{
    ASSERT(isKeyboardEventType(type()));
}

#endif

WebKeyboardEvent::~WebKeyboardEvent()
{
}

bool WebKeyboardEvent::isKeyboardEventType(Type type)
{
    return type == RawKeyDown || type == KeyDown || type == KeyUp || type == Char;
}

} // namespace WebKit
