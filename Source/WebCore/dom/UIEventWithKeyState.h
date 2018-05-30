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
#include "UIEvent.h"

namespace WebCore {

class UIEventWithKeyState : public UIEvent {
public:
    bool ctrlKey() const { return m_ctrlKey; }
    bool shiftKey() const { return m_shiftKey; }
    bool altKey() const { return m_altKey; }
    bool metaKey() const { return m_metaKey; }
    bool altGraphKey() const { return m_altGraphKey; }
    bool capsLockKey() const { return m_capsLockKey; }

protected:
    UIEventWithKeyState() = default;

    UIEventWithKeyState(const AtomicString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, int detail, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
        : UIEvent(type, canBubble, cancelable, WTFMove(view), detail)
        , m_ctrlKey(ctrlKey)
        , m_altKey(altKey)
        , m_shiftKey(shiftKey)
        , m_metaKey(metaKey)
    {
    }

    UIEventWithKeyState(const AtomicString& type, bool canBubble, bool cancelable, MonotonicTime timestamp, RefPtr<WindowProxy>&& view,
        int detail, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey, bool capsLockKey)
            : UIEvent(type, canBubble, cancelable, timestamp, WTFMove(view), detail)
            , m_ctrlKey(ctrlKey)
            , m_altKey(altKey)
            , m_shiftKey(shiftKey)
            , m_metaKey(metaKey)
            , m_altGraphKey(altGraphKey)
            , m_capsLockKey(capsLockKey)
    {
    }

    UIEventWithKeyState(const AtomicString& type, const EventModifierInit& initializer, IsTrusted isTrusted)
        : UIEvent(type, initializer, isTrusted)
        , m_ctrlKey(initializer.ctrlKey)
        , m_altKey(initializer.altKey)
        , m_shiftKey(initializer.shiftKey)
        , m_metaKey(initializer.metaKey)
        , m_altGraphKey(initializer.modifierAltGraph)
        , m_capsLockKey(initializer.modifierCapsLock)
    {
    }

    // Expose these so init functions can set them.
    bool m_ctrlKey { false };
    bool m_altKey { false };
    bool m_shiftKey { false };
    bool m_metaKey { false };
    bool m_altGraphKey { false };
    bool m_capsLockKey { false };
};

UIEventWithKeyState* findEventWithKeyState(Event*);

} // namespace WebCore
