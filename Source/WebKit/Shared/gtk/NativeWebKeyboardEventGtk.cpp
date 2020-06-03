/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011, 2012 Igalia S.L
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
#include "NativeWebKeyboardEvent.h"

#include "WebEventFactory.h"
#include <WebCore/GtkVersioning.h>

namespace WebKit {

NativeWebKeyboardEvent::NativeWebKeyboardEvent(GdkEvent* event, const String& text, Vector<String>&& commands)
    : WebKeyboardEvent(WebEventFactory::createWebKeyboardEvent(event, text, false, WTF::nullopt, WTF::nullopt, WTFMove(commands)))
    , m_nativeEvent(gdk_event_copy(event))
{
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const String& text, Optional<Vector<WebCore::CompositionUnderline>>&& preeditUnderlines, Optional<EditingRange>&& preeditSelectionRange)
    : WebKeyboardEvent(WebEvent::KeyDown, text, "Unidentified"_s, "Unidentified"_s, "U+0000"_s, 229, GDK_KEY_VoidSymbol, true, WTFMove(preeditUnderlines), WTFMove(preeditSelectionRange), { }, false, { }, WallTime::now())
{
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(Type type, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, Vector<String>&& commands, bool isKeypad, OptionSet<Modifier> modifiers)
    : WebKeyboardEvent(type, text, key, code, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, false, WTF::nullopt, WTF::nullopt, WTFMove(commands), isKeypad, modifiers, WallTime::now())
{
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const NativeWebKeyboardEvent& event)
    : WebKeyboardEvent(event.type(), event.text(), event.key(), event.code(), event.keyIdentifier(), event.windowsVirtualKeyCode(), event.nativeVirtualKeyCode(), event.handledByInputMethod(), Optional<Vector<WebCore::CompositionUnderline>>(event.preeditUnderlines()), Optional<EditingRange>(event.preeditSelectionRange()), Vector<String>(event.commands()), event.isKeypad(), event.modifiers(), event.timestamp())
    , m_nativeEvent(event.nativeEvent() ? gdk_event_copy(event.nativeEvent()) : nullptr)
{
}

} // namespace WebKit
