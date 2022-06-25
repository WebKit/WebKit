/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
    WTF_MAKE_ISO_ALLOCATED(HashChangeEvent);
public:
    static Ref<HashChangeEvent> create(const String& oldURL, const String& newURL)
    {
        return adoptRef(*new HashChangeEvent(oldURL, newURL));
    }

    static Ref<HashChangeEvent> createForBindings()
    {
        return adoptRef(*new HashChangeEvent);
    }

    struct Init : EventInit {
        String oldURL;
        String newURL;
    };

    static Ref<HashChangeEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new HashChangeEvent(type, initializer, isTrusted));
    }

    void initHashChangeEvent(const AtomString& eventType, bool canBubble, bool cancelable, const String& oldURL, const String& newURL)
    {
        if (isBeingDispatched())
            return;

        initEvent(eventType, canBubble, cancelable);

        m_oldURL = oldURL;
        m_newURL = newURL;
    }

    const String& oldURL() const { return m_oldURL; }
    const String& newURL() const { return m_newURL; }

    EventInterface eventInterface() const override { return HashChangeEventInterfaceType; }

private:
    HashChangeEvent()
    {
    }

    HashChangeEvent(const String& oldURL, const String& newURL)
        : Event(eventNames().hashchangeEvent, CanBubble::No, IsCancelable::No)
        , m_oldURL(oldURL)
        , m_newURL(newURL)
    {
    }

    HashChangeEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
        : Event(type, initializer, isTrusted)
        , m_oldURL(initializer.oldURL)
        , m_newURL(initializer.newURL)
    {
    }

    String m_oldURL;
    String m_newURL;
};

} // namespace WebCore
