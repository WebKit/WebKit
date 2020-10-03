/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#pragma once

#include "EditingRange.h"
#include "WebEvent.h"
#include <WebCore/CompositionUnderline.h>

#if USE(APPKIT)
#include <WebCore/KeypressCommand.h>
#endif

namespace WebKit {

class WebKeyboardEvent : public WebEvent {
public:
    WebKeyboardEvent();
    ~WebKeyboardEvent();

#if USE(APPKIT)
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, const Vector<WebCore::KeypressCommand>&, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier>, WallTime timestamp);
#elif PLATFORM(GTK)
    WebKeyboardEvent(Type, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, Optional<Vector<WebCore::CompositionUnderline>>&&, Optional<EditingRange>&&, Vector<String>&& commands, bool isKeypad, OptionSet<Modifier>, WallTime timestamp);
#elif PLATFORM(IOS_FAMILY)
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier>, WallTime timestamp);
#elif USE(LIBWPE)
    WebKeyboardEvent(Type, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, Optional<Vector<WebCore::CompositionUnderline>>&&, Optional<EditingRange>&&, bool isKeypad, OptionSet<Modifier>, WallTime timestamp);
#else
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier>, WallTime timestamp);
#endif

    const String& text() const { return m_text; }
    const String& unmodifiedText() const { return m_unmodifiedText; }
    const String& key() const { return m_key; }
    const String& code() const { return m_code; }
    const String& keyIdentifier() const { return m_keyIdentifier; }
    int32_t windowsVirtualKeyCode() const { return m_windowsVirtualKeyCode; }
    int32_t nativeVirtualKeyCode() const { return m_nativeVirtualKeyCode; }
    int32_t macCharCode() const { return m_macCharCode; }
#if USE(APPKIT) || USE(UIKIT_KEYBOARD_ADDITIONS) || PLATFORM(GTK) || USE(LIBWPE)
    bool handledByInputMethod() const { return m_handledByInputMethod; }
#endif
#if PLATFORM(GTK) || USE(LIBWPE)
    const Optional<Vector<WebCore::CompositionUnderline>>& preeditUnderlines() const { return m_preeditUnderlines; }
    const Optional<EditingRange>& preeditSelectionRange() const { return m_preeditSelectionRange; }
#endif
#if USE(APPKIT)
    const Vector<WebCore::KeypressCommand>& commands() const { return m_commands; }
#elif PLATFORM(GTK)
    const Vector<String>& commands() const { return m_commands; }
#endif
    bool isAutoRepeat() const { return m_isAutoRepeat; }
    bool isKeypad() const { return m_isKeypad; }
    bool isSystemKey() const { return m_isSystemKey; }

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebKeyboardEvent&);

    static bool isKeyboardEventType(Type);

private:
    String m_text;
    String m_unmodifiedText;
    String m_key;
    String m_code;
    String m_keyIdentifier;
    int32_t m_windowsVirtualKeyCode { 0 };
    int32_t m_nativeVirtualKeyCode { 0 };
    int32_t m_macCharCode { 0 };
#if USE(APPKIT) || USE(UIKIT_KEYBOARD_ADDITIONS) || PLATFORM(GTK) || USE(LIBWPE)
    bool m_handledByInputMethod { false };
#endif
#if PLATFORM(GTK) || USE(LIBWPE)
    Optional<Vector<WebCore::CompositionUnderline>> m_preeditUnderlines;
    Optional<EditingRange> m_preeditSelectionRange;
#endif
#if USE(APPKIT)
    Vector<WebCore::KeypressCommand> m_commands;
#elif PLATFORM(GTK)
    Vector<String> m_commands;
#endif
    bool m_isAutoRepeat { false };
    bool m_isKeypad { false };
    bool m_isSystemKey { false };
};

} // namespace WebKit
