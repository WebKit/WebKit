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

#include "config.h"

#if ENABLE(SVG)
#include "EventTargetSVGElementInstance.h"

#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventException.h"
#include "FrameView.h"

namespace WebCore {

EventTargetSVGElementInstance::EventTargetSVGElementInstance(SVGUseElement* useElement, SVGElement* originalElement)
    : SVGElementInstance(useElement, originalElement)
{
}

EventTargetSVGElementInstance::~EventTargetSVGElementInstance()
{
}

Frame* EventTargetSVGElementInstance::associatedFrame() const
{
    if (SVGElement* element = correspondingElement())
        return element->associatedFrame();
    return 0;
}

void EventTargetSVGElementInstance::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (SVGElement* element = correspondingElement())
        element->addEventListener(eventType, listener, useCapture);
}

void EventTargetSVGElementInstance::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (SVGElement* element = correspondingElement())
        element->removeEventListener(eventType, listener, useCapture);
}

bool EventTargetSVGElementInstance::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    // The event has to be dispatched to the shadowTreeElement(), not the correspondingElement()!
    SVGElement* element = shadowTreeElement();
    if (!element)
        return false;

    evt->setTarget(this);

    RefPtr<FrameView> view = element->document()->view();
    return element->dispatchGenericEvent(evt.release(), ec, tempEvent);
}

} // namespace WebCore

#endif // ENABLE(SVG)
