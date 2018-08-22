/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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

#include "EventModifierInit.h"
#include "PlatformEvent.h"
#include "UIEvent.h"

namespace WebCore {

class UIEventWithKeyState : public UIEvent {
public:
    using Modifier = PlatformEvent::Modifier;

    bool ctrlKey() const { return m_modifiers.contains(Modifier::CtrlKey); }
    bool shiftKey() const { return m_modifiers.contains(Modifier::ShiftKey); }
    bool altKey() const { return m_modifiers.contains(Modifier::AltKey); }
    bool metaKey() const { return m_modifiers.contains(Modifier::MetaKey); }
    bool altGraphKey() const { return m_modifiers.contains(Modifier::AltGraphKey); }
    bool capsLockKey() const { return m_modifiers.contains(Modifier::CapsLockKey); }

    OptionSet<Modifier> modifierKeys() { return m_modifiers; }

protected:
    UIEventWithKeyState() = default;

    UIEventWithKeyState(const AtomicString& type, CanBubble canBubble, IsCancelable cancelable, RefPtr<WindowProxy>&& view, int detail, OptionSet<Modifier> modifiers)
        : UIEvent(type, canBubble, cancelable, WTFMove(view), detail)
        , m_modifiers(modifiers)
    {
    }

    UIEventWithKeyState(const AtomicString& type, CanBubble canBubble, IsCancelable cancelable, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail, OptionSet<Modifier> modifiers)
        : UIEvent(type, canBubble, cancelable, timestamp, WTFMove(view), detail)
        , m_modifiers(modifiers)
    {
    }

    UIEventWithKeyState(const AtomicString& type, const EventModifierInit& initializer, IsTrusted isTrusted)
        : UIEvent(type, initializer, isTrusted)
        , m_modifiers(modifiersFromInitializer(initializer))
    {
    }

    void setModifierKeys(bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey)
    {
        OptionSet<Modifier> result;
        if (ctrlKey)
            result |= Modifier::CtrlKey;
        if (altKey)
            result |= Modifier::AltKey;
        if (shiftKey)
            result |= Modifier::ShiftKey;
        if (metaKey)
            result |= Modifier::MetaKey;
        if (altGraphKey)
            result |= Modifier::AltGraphKey;
        //  FIXME: Chrome or Firefox don't preserve this state.
        if (m_modifiers & Modifier::CapsLockKey)
            result |= Modifier::CapsLockKey;
        m_modifiers = result;
    }

    void setModifierKeys(bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
    {
        OptionSet<Modifier> result;
        if (ctrlKey)
            result |= Modifier::CtrlKey;
        if (altKey)
            result |= Modifier::AltKey;
        if (shiftKey)
            result |= Modifier::ShiftKey;
        if (metaKey)
            result |= Modifier::MetaKey;
        //  FIXME: Chrome or Firefox don't preserve these states.
        if (m_modifiers & Modifier::AltGraphKey)
            result |= Modifier::AltGraphKey;
        if (m_modifiers & Modifier::CapsLockKey)
            result |= Modifier::CapsLockKey;
        m_modifiers = result;
    }

private:
    OptionSet<Modifier> m_modifiers;

    OptionSet<Modifier> modifiersFromInitializer(const EventModifierInit& initializer)
    {
        OptionSet<Modifier> result;
        if (initializer.ctrlKey)
            result |= Modifier::CtrlKey;
        if (initializer.altKey)
            result |= Modifier::AltKey;
        if (initializer.shiftKey)
            result |= Modifier::ShiftKey;
        if (initializer.metaKey)
            result |= Modifier::MetaKey;
        if (initializer.modifierAltGraph)
            result |= Modifier::AltGraphKey;
        if (initializer.modifierCapsLock)
            result |= Modifier::CapsLockKey;
        return result;
    }
    
};

UIEventWithKeyState* findEventWithKeyState(Event*);

} // namespace WebCore
