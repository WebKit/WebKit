/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef EventTargetSVGElementInstance_h
#define EventTargetSVGElementInstance_h

#if ENABLE(SVG)
#include "EventTarget.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"

namespace WebCore {

class EventTargetSVGElementInstance : public SVGElementInstance,
                                      public EventTarget {
public:
    EventTargetSVGElementInstance(SVGUseElement*, SVGElement* originalElement);
    virtual ~EventTargetSVGElementInstance();

    virtual bool isEventTargetSVGElementInstance() const { return true; }

    virtual Frame* associatedFrame() const;

    virtual EventTargetNode* toNode() { return shadowTreeElement(); }
    virtual EventTargetSVGElementInstance* toSVGElementInstance() { return this; }

    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);

    using SVGElementInstance::ref;
    using SVGElementInstance::deref;

private:
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
};

inline EventTargetSVGElementInstance* EventTargetSVGElementInstanceCast(SVGElementInstance* e) 
{ 
    return static_cast<EventTargetSVGElementInstance*>(e);
}

inline const EventTargetSVGElementInstance* EventTargetSVGElementInstanceCast(const SVGElementInstance* e) 
{ 
    return static_cast<const EventTargetSVGElementInstance*>(e);
}

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // EventTargetSVGElementInstance_h
