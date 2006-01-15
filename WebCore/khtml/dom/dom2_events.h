/*
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

#ifndef _DOM_Events_h_
#define _DOM_Events_h_

#include "shared.h"
#include "dom_string.h"

namespace DOM {

class EventListenerImpl;
class EventImpl;

typedef EventImpl *EventListenerEvent;

/**
 * Introduced in DOM Level 2
 *
 * The EventListener interface is the primary method for handling events.
 * Users implement the EventListener interface and register their listener on
 * an EventTarget using the AddEventListener method. The users should also
 * remove their EventListener from its EventTarget after they have completed
 * using the listener.
 *
 * When a Node is copied using the cloneNode method the EventListeners attached
 * to the source Node are not attached to the copied Node. If the user wishes
 * the same EventListeners to be added to the newly created copy the user must
 * add them manually.
 *
 */
class EventListener : public khtml::Shared<EventListener> {
public:
    EventListener();
    virtual ~EventListener();

    /**
     * This method is called whenever an event occurs of the type for which the
     * EventListener interface was registered. Parameters
     *
     * @param evt The Event contains contextual information about the event. It
     * also contains the stopPropagation and preventDefault methods which are
     * used in determining the event's flow and default action.
     *
     * @param isWindowEvent If true, the "this" should be the window, not the current
     * target (there is no DOM node for the window, so it can't be the target).
     *
     */
    virtual void handleEvent(EventListenerEvent evt, bool isWindowEvent);
    void handleEventImpl(EventImpl *evt, bool isWindowEvent);

    /**
     * @internal
     * not part of the DOM
     *
     * Returns a name specifying the type of listener. Useful for checking
     * if an event is of a particular sublass.
     *
     */
    virtual DOMString eventListenerType();

protected:
    /**
     * @internal
     * Reserved. Do not use in your subclasses.
     */
    EventListenerImpl *impl;
};


/**
 * Introduced in DOM Level 2
 *
 * The Event interface is used to provide contextual information about an event
 * to the handler processing the event. An object which implements the Event
 * interface is generally passed as the first parameter to an event handler.
 * More specific context information is passed to event handlers by deriving
 * additional interfaces from Event which contain information directly relating
 * to the type of event they accompany. These derived interfaces are also
 * implemented by the object passed to the event listener.
 *
 */
class Event {

public:

    /**
     * An integer indicating which phase of event flow is being processed.
     *
     * AT_TARGET: The event is currently being evaluated at the target
     * EventTarget.
     *
     * BUBBLING_PHASE: The current event phase is the bubbling phase.
     *
     * CAPTURING_PHASE: The current event phase is the capturing phase.
     *
     */
    enum PhaseType {
        CAPTURING_PHASE = 1,
        AT_TARGET = 2,
        BUBBLING_PHASE = 3
    };
};


/**
 * Introduced in DOM Level 2:
 *
 * Event operations may throw an EventException as specified in their method
 * descriptions.
 *
 */
class EventException
{
public:

    /**
     * An integer indicating the type of error generated.
     *
     * UNSPECIFIED_EVENT_TYPE_ERR: If the Event's type was not specified by
     * initializing the event before the method was called. Specification of
     * the Event's type as null or an empty string will also trigger this
     * exception.
     *
     */
    enum EventExceptionCode {
        UNSPECIFIED_EVENT_TYPE_ERR     = 0,
        _EXCEPTION_OFFSET              = 3000,
        _EXCEPTION_MAX                 = 3999
    };
};

/**
 * Introduced in DOM Level 2
 *
 * The MutationEvent interface provides specific contextual information
 * associated with Mutation events.
 *
 */
class MutationEvent : public Event {
public:

    /**
     * An integer indicating in which way the Attr was changed.
     *
     * ADDITION: The Attr was just added.
     *
     * MODIFICATION: The Attr was modified in place.
     *
     * REMOVAL: The Attr was just removed.
     *
     */
    enum attrChangeType {
        MODIFICATION = 1,
        ADDITION = 2,
        REMOVAL = 3
    };
};


/**
 * Introduced in DOM Level 3
 *
 * The KeyboardEvent interface provides specific contextual information
 * associated with Keyboard events.
 *
 */
class KeyboardEvent

{

public:

    // KeyLocationCode
    static const unsigned DOM_KEY_LOCATION_STANDARD      = 0x00;
    static const unsigned DOM_KEY_LOCATION_LEFT          = 0x01;
    static const unsigned DOM_KEY_LOCATION_RIGHT         = 0x02;
    static const unsigned DOM_KEY_LOCATION_NUMPAD        = 0x03;
    static const unsigned DOM_KEY_LOCATION_UNKNOWN       = 0x04;
};

}; //namespace
#endif
