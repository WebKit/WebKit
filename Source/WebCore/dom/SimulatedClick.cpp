/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimulatedClick.h"

#include "DataTransfer.h"
#include "Element.h"
#include "EventDispatcher.h"
#include "MouseEvent.h"
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class SimulatedMouseEvent final : public MouseEvent {
public:
    static Ref<SimulatedMouseEvent> create(const AtomicString& eventType, AbstractView* view, RefPtr<Event>&& underlyingEvent, Element& target)
    {
        return adoptRef(*new SimulatedMouseEvent(eventType, view, WTFMove(underlyingEvent), target));
    }

private:
    SimulatedMouseEvent(const AtomicString& eventType, AbstractView* view, RefPtr<Event>&& underlyingEvent, Element& target)
        : MouseEvent(eventType, true, true, underlyingEvent ? underlyingEvent->timeStamp() : currentTime(), view, 0, 0, 0, 0, 0,
#if ENABLE(POINTER_LOCK)
                     0, 0,
#endif
                     false, false, false, false, 0, 0, 0, 0, true)
    {
        if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
            m_ctrlKey = keyStateEvent->ctrlKey();
            m_altKey = keyStateEvent->altKey();
            m_shiftKey = keyStateEvent->shiftKey();
            m_metaKey = keyStateEvent->metaKey();
        }
        setUnderlyingEvent(underlyingEvent.get());

        if (is<MouseEvent>(this->underlyingEvent())) {
            MouseEvent& mouseEvent = downcast<MouseEvent>(*this->underlyingEvent());
            m_screenLocation = mouseEvent.screenLocation();
            initCoordinates(mouseEvent.clientLocation());
        } else {
            m_screenLocation = target.screenRect().center();
            initCoordinates(LayoutPoint(target.clientRect().center()));
        }
    }

};

static void simulateMouseEvent(const AtomicString& eventType, Element& element, Event* underlyingEvent, SimulatedClickCreationOptions creationOptions)
{
    auto event = SimulatedMouseEvent::create(eventType, element.document().defaultView(), underlyingEvent, element);
    if (creationOptions == SimulatedClickCreationOptions::FromBindings)
        event.get().setUntrusted();
    EventDispatcher::dispatchEvent(&element, event.get());
}

void simulateClick(Element& element, Event* underlyingEvent, SimulatedClickMouseEventOptions mouseEventOptions, SimulatedClickVisualOptions visualOptions, SimulatedClickCreationOptions creationOptions)
{
    if (element.isDisabledFormControl())
        return;

    static NeverDestroyed<HashSet<Element*>> elementsDispatchingSimulatedClicks;
    if (!elementsDispatchingSimulatedClicks.get().add(&element).isNewEntry)
        return;

    if (mouseEventOptions == SendMouseOverUpDownEvents)
        simulateMouseEvent(eventNames().mouseoverEvent, element, underlyingEvent, creationOptions);

    if (mouseEventOptions != SendNoEvents)
        simulateMouseEvent(eventNames().mousedownEvent, element, underlyingEvent, creationOptions);
    element.setActive(true, visualOptions == ShowPressedLook);
    if (mouseEventOptions != SendNoEvents)
        simulateMouseEvent(eventNames().mouseupEvent, element, underlyingEvent, creationOptions);
    element.setActive(false);

    simulateMouseEvent(eventNames().clickEvent, element, underlyingEvent, creationOptions);

    elementsDispatchingSimulatedClicks.get().remove(&element);
}

} // namespace WebCore
