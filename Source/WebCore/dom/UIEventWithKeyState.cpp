/*
 * Copyright (C) 2006 Apple Inc.
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

#include "config.h"
#include "UIEventWithKeyState.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(UIEventWithKeyState);

auto UIEventWithKeyState::modifiersFromInitializer(const EventModifierInit& initializer) -> OptionSet<Modifier>
{
    OptionSet<Modifier> result;
    if (initializer.ctrlKey)
        result.add(Modifier::ControlKey);
    if (initializer.altKey)
        result.add(Modifier::AltKey);
    if (initializer.shiftKey)
        result.add(Modifier::ShiftKey);
    if (initializer.metaKey)
        result.add(Modifier::MetaKey);
    if (initializer.modifierAltGraph)
        result.add(Modifier::AltGraphKey);
    if (initializer.modifierCapsLock)
        result.add(Modifier::CapsLockKey);
    return result;
}

bool UIEventWithKeyState::getModifierState(const String& keyIdentifier) const
{
    if (keyIdentifier == "Control"_s)
        return ctrlKey();
    if (keyIdentifier == "Shift"_s)
        return shiftKey();
    if (keyIdentifier == "Alt"_s)
        return altKey();
    if (keyIdentifier == "Meta"_s)
        return metaKey();
    if (keyIdentifier == "CapsLock"_s)
        return capsLockKey();
    // FIXME: The specification also has Fn, FnLock, Hyper, NumLock, Super, ScrollLock, Symbol, SymbolLock.
    return false;
}

void UIEventWithKeyState::setModifierKeys(bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    OptionSet<Modifier> result;
    if (ctrlKey)
        result.add(Modifier::ControlKey);
    if (altKey)
        result.add(Modifier::AltKey);
    if (shiftKey)
        result.add(Modifier::ShiftKey);
    if (metaKey)
        result.add(Modifier::MetaKey);
    m_modifiers = result;
}

UIEventWithKeyState* findEventWithKeyState(Event* event)
{
    for (Event* e = event; e; e = e->underlyingEvent())
        if (e->isKeyboardEvent() || e->isMouseEvent())
            return static_cast<UIEventWithKeyState*>(e);
    return 0;
}

} // namespace WebCore
