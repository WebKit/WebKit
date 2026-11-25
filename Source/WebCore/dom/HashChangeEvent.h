/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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

#include "Event.h"
#include "EventNames.h"

namespace WebCore {

class HashChangeEvent final : public Event {
    WTF_MAKE_TZONE_ALLOCATED(HashChangeEvent);
public:
    static Ref<HashChangeEvent> create(StringImpl* oldURL, StringImpl* newURL)
    {
        return adoptRef(*new HashChangeEvent(oldURL, newURL));
    }

    static Ref<HashChangeEvent> createForBindings()
    {
        return adoptRef(*new HashChangeEvent);
    }

    struct Init : EventInit {
        RefPtr<StringImpl> oldURL;
        RefPtr<StringImpl> newURL;
    };

    static Ref<HashChangeEvent> create(AtomStringImpl* type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new HashChangeEvent(type, initializer, isTrusted));
    }

    const RefPtr<StringImpl>& oldURL() const LIFETIME_BOUND { return m_oldURL; }
    const RefPtr<StringImpl>& newURL() const LIFETIME_BOUND { return m_newURL; }

private:
    HashChangeEvent()
        : Event(EventInterfaceType::HashChangeEvent)
    {
    }

    HashChangeEvent(StringImpl* oldURL, StringImpl* newURL)
        : Event(EventInterfaceType::HashChangeEvent, eventNames().hashchangeEvent, CanBubble::No, IsCancelable::No)
        , m_oldURL(oldURL)
        , m_newURL(newURL)
    {
    }

    HashChangeEvent(AtomStringImpl* type, const Init& initializer, IsTrusted isTrusted)
        : Event(EventInterfaceType::HashChangeEvent, type, initializer, isTrusted)
        , m_oldURL(initializer.oldURL)
        , m_newURL(initializer.newURL)
    {
    }

    RefPtr<StringImpl> m_oldURL;
    RefPtr<StringImpl> m_newURL;
};

} // namespace WebCore
