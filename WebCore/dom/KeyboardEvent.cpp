/**
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "EventNames.h"
#include "PlatformKeyboardEvent.h"

namespace WebCore {

using namespace EventNames;

KeyboardEvent::KeyboardEvent()
    : m_keyEvent(0)
    , m_keyLocation(DOM_KEY_LOCATION_STANDARD)
    , m_altGraphKey(false)
{
}

KeyboardEvent::KeyboardEvent(const PlatformKeyboardEvent& key, AbstractView* view)
    : UIEventWithKeyState(key.isKeyUp() ? keyupEvent : key.isAutoRepeat() ? keypressEvent : keydownEvent,
                          true, true, view, 0, key.ctrlKey(), key.altKey(), key.shiftKey(), key.metaKey())
    , m_keyEvent(new PlatformKeyboardEvent(key))
    , m_keyIdentifier(key.keyIdentifier())
    , m_keyLocation(key.isKeypad() ? DOM_KEY_LOCATION_NUMPAD : DOM_KEY_LOCATION_STANDARD)
    , m_altGraphKey(false)
{
}

KeyboardEvent::KeyboardEvent(const AtomicString& eventType, bool canBubble, bool cancelable, AbstractView *view,
                             const String &keyIdentifier,  unsigned keyLocation,
                             bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey)
    : UIEventWithKeyState(eventType, canBubble, cancelable, view, 0, ctrlKey, altKey, shiftKey, metaKey)
    , m_keyEvent(0)
    , m_keyIdentifier(keyIdentifier)
    , m_keyLocation(keyLocation)
    , m_altGraphKey(altGraphKey)
{
}

KeyboardEvent::~KeyboardEvent()
{
    delete m_keyEvent;
}

void KeyboardEvent::initKeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
                                      const String &keyIdentifier, unsigned keyLocation,
                                      bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey)
{
    if (dispatched())
        return;

    initUIEvent(type, canBubble, cancelable, view, 0);

    m_keyIdentifier = keyIdentifier;
    m_keyLocation = keyLocation;
    m_ctrlKey = ctrlKey;
    m_shiftKey = shiftKey;
    m_altKey = altKey;
    m_metaKey = metaKey;
    m_altGraphKey = altGraphKey;
}

bool KeyboardEvent::getModifierState(const String& keyIdentifier) const
{
    if (keyIdentifier == "Control")
        return ctrlKey();
    if (keyIdentifier == "Shift")
        return shiftKey();
    if (keyIdentifier == "Alt")
        return altKey();
    if (keyIdentifier == "Meta")
        return metaKey();
    return false;
}

int KeyboardEvent::keyCode() const
{
    if (!m_keyEvent)
        return 0;
    if (type() == keydownEvent || type() == keyupEvent)
        return m_keyEvent->WindowsKeyCode();
    return charCode();
}

int KeyboardEvent::charCode() const
{
    if (!m_keyEvent)
        return 0;
    String text = m_keyEvent->text();
    if (text.length() != 1)
        return 0;
    return text[0];
}

bool KeyboardEvent::isKeyboardEvent() const
{
    return true;
}

int KeyboardEvent::which() const
{
    // Netscape's "which" returns a virtual key code for keydown and keyup, and a character code for keypress.
    // That's exactly what IE's "keyCode" returns. So they are the same for keyboard events.
    return keyCode();
}

KeyboardEvent* findKeyboardEvent(Event* event)
{
    for (Event* e = event; e; e = e->underlyingEvent())
        if (e->isKeyboardEvent())
            return static_cast<KeyboardEvent*>(e);
    return 0;
}

} // namespace WebCore
