/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
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

#include "GtkVersioning.h"
#include "WebEventFactory.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

#if USE(GTK4)
#define constructNativeEvent(event) event
#else
#define constructNativeEvent(event) gdk_event_copy(event)
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeWebKeyboardEvent);

Ref<NativeWebKeyboardEvent> NativeWebKeyboardEvent::create(GdkEvent* event, const String& text, bool isAutoRepeat, Vector<String>&& commands)
{
    return adoptRef(*new NativeWebKeyboardEvent(WebEventFactory::createWebKeyboardEvent(event, text, isAutoRepeat, false, std::nullopt, std::nullopt, WTF::move(commands)), event));
}

Ref<NativeWebKeyboardEvent> NativeWebKeyboardEvent::create(const String& text, std::optional<Vector<WebCore::CompositionUnderline>>&& preeditUnderlines, std::optional<EditingRange>&& preeditSelectionRange)
{
    return adoptRef(*new NativeWebKeyboardEvent(WebKeyboardEventInit {
        { WebEventType::KeyDown, { }, MonotonicTime::now() },
        {
            .text = text,
            .key = "Unidentified"_s,
            .code = "Unidentified"_s,
            .keyIdentifier = "U+0000"_s,
            .windowsVirtualKeyCode = 229,
            .nativeVirtualKeyCode = GDK_KEY_VoidSymbol,
            .handledByInputMethod = true,
            .preeditUnderlines = WTF::move(preeditUnderlines),
            .preeditSelectionRange = WTF::move(preeditSelectionRange),
            .commands = { },
            .isAutoRepeat = false,
            .isKeypad = false,
        }
    }, nullptr));
}

Ref<NativeWebKeyboardEvent> NativeWebKeyboardEvent::create(WebEventType type, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, Vector<String>&& commands, bool isAutoRepeat, bool isKeypad, OptionSet<WebEventModifier> modifiers)
{
    return adoptRef(*new NativeWebKeyboardEvent(WebKeyboardEventInit {
        { type, modifiers, MonotonicTime::now() },
        {
            .text = text,
            .key = key,
            .code = code,
            .keyIdentifier = keyIdentifier,
            .windowsVirtualKeyCode = windowsVirtualKeyCode,
            .nativeVirtualKeyCode = nativeVirtualKeyCode,
            .handledByInputMethod = false,
            .preeditUnderlines = std::nullopt,
            .preeditSelectionRange = std::nullopt,
            .commands = WTF::move(commands),
            .isAutoRepeat = isAutoRepeat,
            .isKeypad = isKeypad,
        }
    }, nullptr));
}

Ref<NativeWebKeyboardEvent> NativeWebKeyboardEvent::create(const NativeWebKeyboardEvent& event)
{
    return adoptRef(*new NativeWebKeyboardEvent(WebKeyboardEventInit { event.eventData(), event.keyboardData() },
        event.nativeEvent() ? const_cast<GdkEvent*>(event.nativeEvent()) : nullptr));
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(WebKeyboardEventInit&& init, GdkEvent* event)
    : WebKeyboardEvent(WTF::move(init.event), WTF::move(init.keyboard))
    , m_nativeEvent(event ? constructNativeEvent(event) : nullptr)
{
}

} // namespace WebKit

#undef constructNativeEvent
