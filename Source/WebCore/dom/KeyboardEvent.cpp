/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "KeyboardEvent.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "PlatformKeyboardEvent.h"
#include "WindowsKeyboardCodes.h"

namespace WebCore {

static inline const AtomString& eventTypeForKeyboardEventType(PlatformEvent::Type type)
{
    switch (type) {
        case PlatformEvent::KeyUp:
            return eventNames().keyupEvent;
        case PlatformEvent::RawKeyDown:
            return eventNames().keydownEvent;
        case PlatformEvent::Char:
            return eventNames().keypressEvent;
        case PlatformEvent::KeyDown:
            // The caller should disambiguate the combined event into RawKeyDown or Char events.
            break;
        default:
            break;
    }
    ASSERT_NOT_REACHED();
    return eventNames().keydownEvent;
}

static inline int windowsVirtualKeyCodeWithoutLocation(int keycode)
{
    switch (keycode) {
    case VK_LCONTROL:
    case VK_RCONTROL:
        return VK_CONTROL;
    case VK_LSHIFT:
    case VK_RSHIFT:
        return VK_SHIFT;
    case VK_LMENU:
    case VK_RMENU:
        return VK_MENU;
    default:
        return keycode;
    }
}

static inline KeyboardEvent::KeyLocationCode keyLocationCode(const PlatformKeyboardEvent& key)
{
    if (key.isKeypad())
        return KeyboardEvent::DOM_KEY_LOCATION_NUMPAD;

    switch (key.windowsVirtualKeyCode()) {
    case VK_LCONTROL:
    case VK_LSHIFT:
    case VK_LMENU:
    case VK_LWIN:
        return KeyboardEvent::DOM_KEY_LOCATION_LEFT;
    case VK_RCONTROL:
    case VK_RSHIFT:
    case VK_RMENU:
    case VK_RWIN:
        return KeyboardEvent::DOM_KEY_LOCATION_RIGHT;
    default:
        return KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
    }
}

inline KeyboardEvent::KeyboardEvent() = default;

inline KeyboardEvent::KeyboardEvent(const PlatformKeyboardEvent& key, RefPtr<WindowProxy>&& view)
    : UIEventWithKeyState(eventTypeForKeyboardEventType(key.type()), CanBubble::Yes, IsCancelable::Yes, IsComposed::Yes,
        key.timestamp().approximateMonotonicTime(), view.copyRef(), 0, key.modifiers(), IsTrusted::Yes)
    , m_underlyingPlatformEvent(makeUnique<PlatformKeyboardEvent>(key))
#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
    , m_key(key.key())
#endif
#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
    , m_code(key.code())
#endif
    , m_keyIdentifier(key.keyIdentifier())
    , m_location(keyLocationCode(key))
    , m_repeat(key.isAutoRepeat())
    , m_isComposing(view && is<DOMWindow>(view->window()) && downcast<DOMWindow>(*view->window()).frame() && downcast<DOMWindow>(*view->window()).frame()->editor().hasComposition())
#if USE(APPKIT) || USE(UIKIT_KEYBOARD_ADDITIONS)
    , m_handledByInputMethod(key.handledByInputMethod())
#endif
#if USE(APPKIT)
    , m_keypressCommands(key.commands())
#endif
{
}

inline KeyboardEvent::KeyboardEvent(const AtomString& eventType, const Init& initializer)
    : UIEventWithKeyState(eventType, initializer)
#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
    , m_key(initializer.key)
#endif
#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
    , m_code(initializer.code)
#endif
    , m_keyIdentifier(initializer.keyIdentifier)
    , m_location(initializer.keyLocation ? *initializer.keyLocation : initializer.location)
    , m_repeat(initializer.repeat)
    , m_isComposing(initializer.isComposing)
    , m_charCode(initializer.charCode)
    , m_keyCode(initializer.keyCode)
    , m_which(initializer.which)
{
}

KeyboardEvent::~KeyboardEvent() = default;

Ref<KeyboardEvent> KeyboardEvent::create(const PlatformKeyboardEvent& platformEvent, RefPtr<WindowProxy>&& view)
{
    return adoptRef(*new KeyboardEvent(platformEvent, WTFMove(view)));
}

Ref<KeyboardEvent> KeyboardEvent::createForBindings()
{
    return adoptRef(*new KeyboardEvent);
}

Ref<KeyboardEvent> KeyboardEvent::create(const AtomString& type, const Init& initializer)
{
    return adoptRef(*new KeyboardEvent(type, initializer));
}

void KeyboardEvent::initKeyboardEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view,
    const String& keyIdentifier, unsigned location, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, canBubble, cancelable, WTFMove(view), 0);

    m_keyIdentifier = keyIdentifier;
    m_location = location;

    setModifierKeys(ctrlKey, altKey, shiftKey, metaKey, altGraphKey);

    m_charCode = WTF::nullopt;
    m_isComposing = false;
    m_keyCode = WTF::nullopt;
    m_repeat = false;
    m_underlyingPlatformEvent = nullptr;
    m_which = WTF::nullopt;

#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
    m_code = { };
#endif

#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
    m_key = { };
#endif

#if PLATFORM(COCOA)
    m_handledByInputMethod = false;
    m_keypressCommands = { };
#endif
}

int KeyboardEvent::keyCode() const
{
    if (m_keyCode)
        return m_keyCode.value();

    // IE: virtual key code for keyup/keydown, character code for keypress
    // Firefox: virtual key code for keyup/keydown, zero for keypress
    // We match IE.
    if (!m_underlyingPlatformEvent)
        return 0;
    if (type() == eventNames().keydownEvent || type() == eventNames().keyupEvent)
        return windowsVirtualKeyCodeWithoutLocation(m_underlyingPlatformEvent->windowsVirtualKeyCode());

    return charCode();
}

int KeyboardEvent::charCode() const
{
    if (m_charCode)
        return m_charCode.value();

    // IE: not supported
    // Firefox: 0 for keydown/keyup events, character code for keypress
    // We match Firefox, unless in backward compatibility mode, where we always return the character code.
    bool backwardCompatibilityMode = false;
    auto* window = view() ? view()->window() : nullptr;
    if (is<DOMWindow>(window) && downcast<DOMWindow>(*window).frame())
        backwardCompatibilityMode = downcast<DOMWindow>(*window).frame()->eventHandler().needsKeyboardEventDisambiguationQuirks();

    if (!m_underlyingPlatformEvent || (type() != eventNames().keypressEvent && !backwardCompatibilityMode))
        return 0;
    return m_underlyingPlatformEvent->text().characterStartingAt(0);
}

EventInterface KeyboardEvent::eventInterface() const
{
    return KeyboardEventInterfaceType;
}

bool KeyboardEvent::isKeyboardEvent() const
{
    return true;
}

int KeyboardEvent::which() const
{
    // Netscape's "which" returns a virtual key code for keydown and keyup, and a character code for keypress.
    // That's exactly what IE's "keyCode" returns. So they are the same for keyboard events.
    if (m_which)
        return m_which.value();
    return keyCode();
}

} // namespace WebCore
