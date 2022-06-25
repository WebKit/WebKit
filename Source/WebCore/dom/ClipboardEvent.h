/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

namespace WebCore {

class DataTransfer;

class ClipboardEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(ClipboardEvent);
public:
    virtual ~ClipboardEvent();

    struct Init : EventInit {
        RefPtr<DataTransfer> clipboardData;
    };

    static Ref<ClipboardEvent> create(const AtomString& type, Ref<DataTransfer>&& dataTransfer)
    {
        return adoptRef(*new ClipboardEvent(type, WTFMove(dataTransfer)));
    }

    static Ref<ClipboardEvent> create(const AtomString& type, const Init& init)
    {
        return adoptRef(*new ClipboardEvent(type, init));
    }

    DataTransfer* clipboardData() const { return m_clipboardData.get(); }

private:
    ClipboardEvent(const AtomString& type, Ref<DataTransfer>&&);
    ClipboardEvent(const AtomString& type, const Init&);

    EventInterface eventInterface() const final;
    bool isClipboardEvent() const final;

    RefPtr<DataTransfer> m_clipboardData;
};

} // namespace WebCore
