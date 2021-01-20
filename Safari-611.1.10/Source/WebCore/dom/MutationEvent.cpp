/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "MutationEvent.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MutationEvent);

MutationEvent::MutationEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Node* relatedNode, const String& prevValue, const String& newValue)
    : Event(type, canBubble, cancelable)
    , m_relatedNode(relatedNode)
    , m_prevValue(prevValue)
    , m_newValue(newValue)
{
}

void MutationEvent::initMutationEvent(const AtomString& type, bool canBubble, bool cancelable, Node* relatedNode, const String& prevValue, const String& newValue, const String& attrName, unsigned short attrChange)
{
    if (isBeingDispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_relatedNode = relatedNode;
    m_prevValue = prevValue;
    m_newValue = newValue;
    m_attrName = attrName;
    m_attrChange = attrChange;
}

EventInterface MutationEvent::eventInterface() const
{
    return MutationEventInterfaceType;
}

} // namespace WebCore
