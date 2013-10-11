/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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

#include "SimulatedClickOptions.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Element;
class Event;
class EventContext;
class EventTarget;
class FrameView;
class Node;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class ShadowRoot;
class TouchEvent;
class TreeScope;
struct InputElementClickState;

enum EventDispatchContinuation {
    ContinueDispatching,
    DoneDispatching
};

class EventPath {
public:
    EventPath(Node& origin, Event&);

    bool isEmpty() const { return m_path.isEmpty(); }
    size_t size() const { return m_path.size(); }
    const EventContext& contextAt(size_t i) const { return *m_path[i]; }
    EventContext& contextAt(size_t i) { return *m_path[i]; }

#if ENABLE(TOUCH_EVENTS)
    void updateTouchLists(const TouchEvent&);
#endif
    void setRelatedTarget(EventTarget&);

    bool hasEventListeners(const AtomicString& eventType) const;

    EventContext* lastContextIfExists() { return m_path.isEmpty() ? 0 : m_path.last().get(); }

private:
    Vector<std::unique_ptr<EventContext>, 32> m_path;
    RefPtr<Node> m_origin;
};

class EventDispatcher {
public:
    static bool dispatchEvent(Node*, PassRefPtr<Event>);
    static void dispatchScopedEvent(Node&, PassRefPtr<Event>);
    static void dispatchSimulatedClick(Element*, Event* underlyingEvent, SimulatedClickMouseEventOptions, SimulatedClickVisualOptions);
};

}

#endif
