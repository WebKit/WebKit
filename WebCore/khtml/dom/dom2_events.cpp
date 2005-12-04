/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "dom2_events.h"

#include "xml/dom2_eventsimpl.h"

namespace DOM {

const unsigned KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
const unsigned KeyboardEvent::DOM_KEY_LOCATION_LEFT;
const unsigned KeyboardEvent::DOM_KEY_LOCATION_RIGHT;
const unsigned KeyboardEvent::DOM_KEY_LOCATION_NUMPAD;
const unsigned KeyboardEvent::DOM_KEY_LOCATION_UNKNOWN;

EventListener::EventListener()
{
}

EventListener::~EventListener()
{
}

void EventListener::handleEventImpl(EventImpl *evt, bool isWindowEvent)
{
    handleEvent(evt, isWindowEvent);
}

void EventListener::handleEvent(EventListenerEvent, bool)
{
}

DOMString EventListener::eventListenerType()
{
    return "";
}

} // namespace
