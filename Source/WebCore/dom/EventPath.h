/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

#ifndef EventPath_h
#define EventPath_h

#include "EventContext.h"
#include "PseudoElement.h"
#include "SVGElement.h"
#include "SVGUseElement.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class EventPath {
public:
    EventPath(Node& origin, Event&);

    bool isEmpty() const { return m_path.isEmpty(); }
    size_t size() const { return m_path.size(); }
    const EventContext& contextAt(size_t i) const { return *m_path[i]; }
    EventContext& contextAt(size_t i) { return *m_path[i]; }

#if ENABLE(TOUCH_EVENTS)
    void retargetTouchLists(const TouchEvent&);
#endif
    void setRelatedTarget(Node& origin, EventTarget&);

    bool hasEventListeners(const AtomicString& eventType) const;

    EventContext* lastContextIfExists() { return m_path.isEmpty() ? nullptr : m_path.last().get(); }

    static EventTarget* eventTargetRespectingTargetRules(Node& referenceNode)
    {
        if (is<PseudoElement>(referenceNode))
            return downcast<PseudoElement>(referenceNode).hostElement();

        // Events sent to elements inside an SVG use element's shadow tree go to the use element.
        if (is<SVGElement>(referenceNode)) {
            if (auto* useElement = downcast<SVGElement>(referenceNode).correspondingUseElement())
                return useElement;
        }

        return &referenceNode;
    }

private:
#if ENABLE(TOUCH_EVENTS)
    void retargetTouch(TouchEventContext::TouchListType, const Touch&);
#endif

    Event& m_event;
    Vector<std::unique_ptr<EventContext>, 32> m_path;
};

}

#endif /* EventPath_h */
