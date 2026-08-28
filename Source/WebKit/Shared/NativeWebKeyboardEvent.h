/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Igalia S.L
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "WebKeyboardEvent.h"

#if USE(APPKIT)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;
#endif

#if PLATFORM(GTK)
#include "GRefPtrGtk.h"
#include "GUniquePtrGtk.h"
#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
typedef struct _WPEEvent WPEEvent;
#endif

#if PLATFORM(IOS_FAMILY)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebEvent;
#endif

#if USE(LIBWPE)
struct wpe_input_keyboard_event;
#endif

#if PLATFORM(WIN)
#include <windows.h>
#endif

namespace WebKit {
struct EditingRange;

class NativeWebKeyboardEvent : public WebKeyboardEvent {
    WTF_MAKE_TZONE_ALLOCATED(NativeWebKeyboardEvent);
public:
#if USE(APPKIT)
    // FIXME: Share iOS's HandledByInputMethod enum here instead of passing a boolean.
    static Ref<NativeWebKeyboardEvent> create(NSEvent *, bool handledByInputMethod, bool replacesSoftSpace, const Vector<WebCore::KeypressCommand>&);
#elif PLATFORM(GTK)
    static Ref<NativeWebKeyboardEvent> create(const NativeWebKeyboardEvent&);
    static Ref<NativeWebKeyboardEvent> create(GdkEvent*, const String&, bool isAutoRepeat, Vector<String>&& commands);
    static Ref<NativeWebKeyboardEvent> create(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<EditingRange>&&);
    static Ref<NativeWebKeyboardEvent> create(WebEventType, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, Vector<String>&& commands, bool isAutoRepeat, bool isKeypad, OptionSet<WebEventModifier>);
#elif PLATFORM(IOS_FAMILY)
    enum class HandledByInputMethod : bool { No, Yes };
    static Ref<NativeWebKeyboardEvent> create(::WebEvent *, HandledByInputMethod);
#elif PLATFORM(WPE)
#if USE(LIBWPE)
    enum class HandledByInputMethod : bool { No, Yes };
    static Ref<NativeWebKeyboardEvent> create(struct wpe_input_keyboard_event*, const String&, bool isAutoRepeat, HandledByInputMethod, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<EditingRange>&&);
#endif
#if ENABLE(WPE_PLATFORM)
    static Ref<NativeWebKeyboardEvent> create(WPEEvent*, const String&, bool isAutoRepeat);
    static Ref<NativeWebKeyboardEvent> create(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<EditingRange>&&);
#endif
#elif PLATFORM(PLAYSTATION)
    enum class HandledByInputMethod : bool { No, Yes };
    static Ref<NativeWebKeyboardEvent> create(struct wpe_input_keyboard_event*, const String&, bool isAutoRepeat, HandledByInputMethod, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<EditingRange>&&);
#elif PLATFORM(WIN)
    static Ref<NativeWebKeyboardEvent> create(HWND, UINT message, WPARAM, LPARAM, Vector<MSG>&& pendingCharEvents);
#endif

#if USE(APPKIT)
    NSEvent *nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(GTK)
    GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(IOS_FAMILY)
    ::WebEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(WIN)
    const MSG* nativeEvent() const LIFETIME_BOUND { return &m_nativeEvent; }
    const Vector<MSG>& pendingCharEvents() const LIFETIME_BOUND { return m_pendingCharEvents; }
#else
    const void* nativeEvent() const { return nullptr; }
#endif

private:
#if USE(APPKIT)
    NativeWebKeyboardEvent(WebKeyboardEventInit&&, NSEvent *);

    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(GTK) && USE(GTK4)
    NativeWebKeyboardEvent(WebKeyboardEventInit&&, GdkEvent*);

    GRefPtr<GdkEvent> m_nativeEvent;
#elif PLATFORM(GTK)
    NativeWebKeyboardEvent(WebKeyboardEventInit&&, GdkEvent*);

    GUniquePtr<GdkEvent> m_nativeEvent;
#elif PLATFORM(IOS_FAMILY)
    NativeWebKeyboardEvent(WebKeyboardEventInit&&, ::WebEvent *);

    RetainPtr<::WebEvent> m_nativeEvent;
#elif PLATFORM(WIN)
    NativeWebKeyboardEvent(WebKeyboardEventInit&&, const MSG&, Vector<MSG>&&);

    MSG m_nativeEvent;
    Vector<MSG> m_pendingCharEvents;
#else
    explicit NativeWebKeyboardEvent(WebKeyboardEventInit&&);
#endif
};

} // namespace WebKit
