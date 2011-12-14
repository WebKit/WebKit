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
#include "WebEvent.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

WebKeyboardEvent::WebKeyboardEvent(Type type, const String& text, const String& unmodifiedText, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, Modifiers modifiers, double timestamp)
    : WebEvent(type, modifiers, timestamp)
    , m_text(text)
    , m_unmodifiedText(unmodifiedText)
    , m_keyIdentifier(keyIdentifier)
    , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
    , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
    , m_macCharCode(macCharCode)
    , m_isAutoRepeat(isAutoRepeat)
    , m_isKeypad(isKeypad)
    , m_isSystemKey(isSystemKey)
{
    ASSERT(isKeyboardEventType(type));
}

void WebKeyboardEvent::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    WebEvent::encode(encoder);

    encoder->encode(CoreIPC::In(m_text, m_unmodifiedText, m_keyIdentifier, m_windowsVirtualKeyCode, m_nativeVirtualKeyCode, m_macCharCode));
    encoder->encode(CoreIPC::In(m_isAutoRepeat, m_isKeypad, m_isSystemKey));
}

bool WebKeyboardEvent::decode(CoreIPC::ArgumentDecoder* decoder, WebKeyboardEvent& t)
{
    if (!WebEvent::decode(decoder, t))
        return false;

    if (!decoder->decode(CoreIPC::Out(t.m_text, t.m_unmodifiedText, t.m_keyIdentifier, t.m_windowsVirtualKeyCode, t.m_nativeVirtualKeyCode, t.m_macCharCode)))
        return false;

    return decoder->decode(CoreIPC::Out(t.m_isAutoRepeat, t.m_isKeypad, t.m_isSystemKey));
}

bool WebKeyboardEvent::isKeyboardEventType(Type type)
{
    return type == RawKeyDown || type == KeyDown || type == KeyUp || type == Char;
}

} // namespace WebKit
