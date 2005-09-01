/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "kdom.h"
#include "EventImpl.h"
#include "UIEventImpl.h"
#include "MouseEventImpl.h"
#include "KeyboardEventImpl.h"
#include "DOMExceptionImpl.h"
#include "MutationEventImpl.h"
#include "DocumentEventImpl.h"

using namespace KDOM;

DocumentEventImpl::DocumentEventImpl()
{
}

DocumentEventImpl::~DocumentEventImpl()
{
}

EventImpl *DocumentEventImpl::createEvent(DOMStringImpl *eventTypeImpl)
{
    EventImpl *i = 0;

    DOMString eventType(eventTypeImpl);
    if(eventType == "UIEvents")
        i = new UIEventImpl(TypeUIEvent);
    else if(eventType == "MouseEvents")
        i = new MouseEventImpl(TypeMouseEvent);
    else if(eventType == "KeyboardEvents")
        i = new KeyboardEventImpl(TypeKeyboardEvent);
    else if(eventType == "MutationEvents")
        i = new MutationEventImpl(TypeMutationEvent);
    else if(eventType == "Events")
        i = new EventImpl(TypeGenericEvent);
    else
        throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

    return i;
}

// vim:ts=4:noet
