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

class WebKeyboardEvent;

// Field order matches WebEvent.serialization.in. On GTK/WPE, unmodifiedText, macCharCode and
// isSystemKey are not stored because no constructor on those platforms sets them.
struct WebKeyboardEventData {
    String text;
#if !PLATFORM(GTK) && !USE(LIBWPE) && !ENABLE(WPE_PLATFORM)
    String unmodifiedText;
#endif
    String key;
    String code;
    String keyIdentifier;
    int32_t windowsVirtualKeyCode { 0 };
    int32_t nativeVirtualKeyCode { 0 };
#if !PLATFORM(GTK) && !USE(LIBWPE) && !ENABLE(WPE_PLATFORM)
    int32_t macCharCode { 0 };
#endif
#if USE(APPKIT) || PLATFORM(IOS_FAMILY) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    bool handledByInputMethod { false };
#endif
#if PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    std::optional<Vector<WebCore::CompositionUnderline>> preeditUnderlines;
    std::optional<EditingRange> preeditSelectionRange;
#endif
#if USE(APPKIT)
    Vector<WebCore::KeypressCommand> commands;
#elif PLATFORM(GTK)
    Vector<String> commands;
#endif
    bool isAutoRepeat { false };
    bool isKeypad { false };
#if !PLATFORM(GTK) && !USE(LIBWPE) && !ENABLE(WPE_PLATFORM)
    bool isSystemKey { false };
#endif
};

struct WebKeyboardEventInit {
    WebEventData event;
    WebKeyboardEventData keyboard;
};

class WebKeyboardEvent : public WebEvent {
    WTF_MAKE_TZONE_ALLOCATED(WebKeyboardEvent);
public:
    static Ref<WebKeyboardEvent> create(WebEventData&&, WebKeyboardEventData&&);
    static Ref<WebKeyboardEvent> create(WebKeyboardEventInit&&);

    ~WebKeyboardEvent();

    const String& text() const LIFETIME_BOUND { return m_data.text; }
#if !PLATFORM(GTK) && !USE(LIBWPE) && !ENABLE(WPE_PLATFORM)
    const String& unmodifiedText() const LIFETIME_BOUND { return m_data.unmodifiedText; }
#else
    // Always identical to text() on this platform.
    const String& unmodifiedText() const LIFETIME_BOUND { return m_data.text; }
#endif
    const String& key() const LIFETIME_BOUND { return m_data.key; }
    const String& code() const LIFETIME_BOUND { return m_data.code; }
    const String& keyIdentifier() const LIFETIME_BOUND { return m_data.keyIdentifier; }
    int32_t windowsVirtualKeyCode() const { return m_data.windowsVirtualKeyCode; }
#if PLATFORM(WIN)
    void setWindowsVirtualKeyCode(int32_t keyCode) { m_data.windowsVirtualKeyCode = keyCode; }
#endif
    int32_t nativeVirtualKeyCode() const { return m_data.nativeVirtualKeyCode; }
#if !PLATFORM(GTK) && !USE(LIBWPE) && !ENABLE(WPE_PLATFORM)
    int32_t macCharCode() const { return m_data.macCharCode; }
#else
    int32_t macCharCode() const { return 0; }
#endif
#if USE(APPKIT) || PLATFORM(IOS_FAMILY) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    bool handledByInputMethod() const { return m_data.handledByInputMethod; }
#endif
#if PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    const std::optional<Vector<WebCore::CompositionUnderline>>& preeditUnderlines() const LIFETIME_BOUND { return m_data.preeditUnderlines; }
    const std::optional<EditingRange>& preeditSelectionRange() const LIFETIME_BOUND { return m_data.preeditSelectionRange; }
#endif
#if USE(APPKIT)
    const Vector<WebCore::KeypressCommand>& commands() const LIFETIME_BOUND { return m_data.commands; }
#elif PLATFORM(GTK)
    const Vector<String>& commands() const LIFETIME_BOUND { return m_data.commands; }
#endif
    bool isAutoRepeat() const { return m_data.isAutoRepeat; }
    bool isKeypad() const { return m_data.isKeypad; }
#if !PLATFORM(GTK) && !USE(LIBWPE) && !ENABLE(WPE_PLATFORM)
    bool isSystemKey() const { return m_data.isSystemKey; }
#else
    bool isSystemKey() const { return false; }
#endif

    const WebKeyboardEventData& keyboardData() const LIFETIME_BOUND { return m_data; }

    static bool NODELETE isKeyboardEventType(WebEventType);

#if PLATFORM(WPE)
    static String keyValueStringForWPEKeyval(unsigned);
    static String keyCodeStringForWPEKeycode(unsigned);
    static String keyIdentifierForWPEKeyval(unsigned);
    static int32_t windowsKeyCodeForWPEKeyval(unsigned);
    static String singleCharacterStringForWPEKeyval(unsigned);
#endif
#if PLATFORM(GTK)
    static String keyValueStringForGdkKeyval(unsigned);
    static String keyCodeStringForGdkKeycode(unsigned);
    static String keyIdentifierForGdkKeyval(unsigned);
    static int windowsKeyCodeForGdkKeyval(unsigned);
    static String singleCharacterStringForGdkKeyval(unsigned);
#endif

protected:
    WebKeyboardEvent(WebEventData&&, WebKeyboardEventData&&);

private:
    WebKeyboardEventData m_data;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebKeyboardEvent)
static bool isType(const WebKit::WebEvent& event) { return WebKit::WebKeyboardEvent::isKeyboardEventType(event.type()); }
SPECIALIZE_TYPE_TRAITS_END()
