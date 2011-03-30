/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef EventDispatcher_h
#define EventDispatcher_h

#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class Event;
class EventContext;
class EventTarget;
class FrameView;
class Node;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;

enum EventDispatchBehavior {
    RetargetEvent,
    StayInsideShadowDOM
};

class EventDispatcher {
public:
    static bool dispatchEvent(Node*, PassRefPtr<Event>);
    static void dispatchScopedEvent(Node*, PassRefPtr<Event>);

    static bool dispatchMouseEvent(Node*, const PlatformMouseEvent&, const AtomicString& eventType, int clickCount = 0, Node* relatedTarget = 0);
    static void dispatchSimulatedClick(Node*, PassRefPtr<Event> underlyingEvent, bool sendMouseEvents, bool showPressedLook);
    static void dispatchWheelEvent(Node*, PlatformWheelEvent&);

    bool dispatchEvent(PassRefPtr<Event>);
private:
    EventDispatcher(Node*);

    EventDispatchBehavior determineDispatchBehavior(Event*);
    void getEventAncestors(EventTarget* originalTarget, EventDispatchBehavior);
    const EventContext* topEventContext();
    bool ancestorsInitialized() const;

    bool dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
        int pageX, int pageY, int screenX, int screenY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        bool isSimulated, Node* relatedTargetArg, PassRefPtr<Event> underlyingEvent);

    Vector<EventContext> m_ancestors;
    RefPtr<Node> m_node;
    RefPtr<EventTarget> m_originalTarget;
    RefPtr<FrameView> m_view;
};

}

#endif
