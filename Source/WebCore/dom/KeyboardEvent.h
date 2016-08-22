/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "KeypressCommand.h"
#include "UIEventWithKeyState.h"
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class Node;
class PlatformKeyboardEvent;

struct KeyboardEventInit : public UIEventWithKeyStateInit {
    String keyIdentifier;
    unsigned location { 0 };
};

class KeyboardEvent final : public UIEventWithKeyState {
public:
    enum KeyLocationCode {
        DOM_KEY_LOCATION_STANDARD   = 0x00,
        DOM_KEY_LOCATION_LEFT       = 0x01,
        DOM_KEY_LOCATION_RIGHT      = 0x02,
        DOM_KEY_LOCATION_NUMPAD     = 0x03
        // FIXME: The following values are not supported yet (crbug.com/265446)
        // DOM_KEY_LOCATION_MOBILE     = 0x04,
        // DOM_KEY_LOCATION_JOYSTICK   = 0x05
    };

    static Ref<KeyboardEvent> create(const PlatformKeyboardEvent& platformEvent, DOMWindow* view)
    {
        return adoptRef(*new KeyboardEvent(platformEvent, view));
    }

    static Ref<KeyboardEvent> createForBindings()
    {
        return adoptRef(*new KeyboardEvent);
    }

    static Ref<KeyboardEvent> createForBindings(const AtomicString& type, const KeyboardEventInit& initializer)
    {
        return adoptRef(*new KeyboardEvent(type, initializer));
    }

    // FIXME: This method should be get ride of in the future.
    // DO NOT USE IT!
    static Ref<KeyboardEvent> createForDummy()
    {
        return adoptRef(*new KeyboardEvent(WTF::HashTableDeletedValue));
    }

    virtual ~KeyboardEvent();
    
    WEBCORE_EXPORT void initKeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, DOMWindow*,
        const String& keyIdentifier, unsigned location,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey = false);
    
    const String& keyIdentifier() const { return m_keyIdentifier; }
    unsigned location() const { return m_location; }

    WEBCORE_EXPORT bool getModifierState(const String& keyIdentifier) const;

    bool altGraphKey() const { return m_altGraphKey; }
    
    const PlatformKeyboardEvent* keyEvent() const { return m_keyEvent.get(); }

    WEBCORE_EXPORT int keyCode() const final; // key code for keydown and keyup, character for keypress
    WEBCORE_EXPORT int charCode() const final; // character code for keypress, 0 for keydown and keyup

    EventInterface eventInterface() const final;
    bool isKeyboardEvent() const final;
    int which() const final;

#if PLATFORM(COCOA)
    bool handledByInputMethod() const { return m_handledByInputMethod; }
    const Vector<KeypressCommand>& keypressCommands() const { return m_keypressCommands; }

    // The non-const version is still needed for WebKit1, which doesn't construct a complete KeyboardEvent with interpreted commands yet.
    Vector<KeypressCommand>& keypressCommands() { return m_keypressCommands; }
#endif

private:
    WEBCORE_EXPORT KeyboardEvent();
    WEBCORE_EXPORT KeyboardEvent(const PlatformKeyboardEvent&, DOMWindow*);
    KeyboardEvent(const AtomicString&, const KeyboardEventInit&);
    // FIXME: This method should be get ride of in the future.
    // DO NOT USE IT!
    KeyboardEvent(WTF::HashTableDeletedValueType);

    std::unique_ptr<PlatformKeyboardEvent> m_keyEvent;
    String m_keyIdentifier;
    unsigned m_location;
    bool m_altGraphKey : 1;

#if PLATFORM(COCOA)
    // Commands that were sent by AppKit when interpreting the event. Doesn't include input method commands.
    bool m_handledByInputMethod;
    Vector<KeypressCommand> m_keypressCommands;
#endif
};

KeyboardEvent* findKeyboardEvent(Event*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(KeyboardEvent)
