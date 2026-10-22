/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PortalInteractionGesture.h"

#if ENABLE(MODEL_ELEMENT)

#include "ContainerNodeInlines.h"
#include "Document.h"
#include "DocumentView.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "ModelPlayer.h"
#include "MouseEvent.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"

#if ENABLE(TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

namespace WebCore {

std::optional<PortalInteractionGesture::Phase> PortalInteractionGesture::phaseForEvent(const Event& event) const
{
    auto beginPhase = [&] -> std::optional<Phase> {
        if (m_isActive || event.defaultPrevented())
            return std::nullopt;
        return Phase::Began;
    };

    auto activePhase = [&](Phase phase) -> std::optional<Phase> {
        if (!m_isActive)
            return std::nullopt;
        return phase;
    };

    auto& type = event.type();
    auto& names = eventNames();

#if ENABLE(TOUCH_EVENTS)
    if (is<TouchEvent>(event)) {
        if (type == names.touchstartEvent)
            return beginPhase();
        if (type == names.touchmoveEvent)
            return activePhase(Phase::Changed);
        if (type == names.touchendEvent || type == names.touchcancelEvent)
            return activePhase(Phase::Ended);
        return std::nullopt;
    }
#endif

    RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event);
    if (!mouseEvent || mouseEvent->button() != MouseButton::Left)
        return std::nullopt;

    if (type == names.mousedownEvent)
        return beginPhase();
    if (type == names.mousemoveEvent)
        return activePhase(Phase::Changed);
    if (type == names.mouseupEvent)
        return activePhase(Phase::Ended);
    return std::nullopt;
}

bool PortalInteractionGesture::apply(Phase phase, Element& element, ModelPlayer& player, Event& event)
{
    Ref mouseEvent = mouseRelatedEvent(event);

    switch (phase) {
    case Phase::Began: {
        ASSERT(!m_isActive);

        RefPtr frame = element.document().frame();
        if (!frame)
            return false;

        frame->eventHandler().setCapturingMouseEventsElement(&element);
        event.setDefaultHandled();
        m_capturingElement = element;
        m_isActive = true;
        m_lastLocation = flippedLocation(element, mouseEvent);
        player.handleMouseDown(m_lastLocation, event.timeStamp());
        return true;
    }
    case Phase::Changed:
        ASSERT(m_isActive);

        event.setDefaultHandled();
        m_lastLocation = flippedLocation(element, mouseEvent);
        player.handleMouseMove(m_lastLocation, event.timeStamp());
        return true;
    case Phase::Ended:
        ASSERT(m_isActive);

        event.setDefaultHandled();
        m_lastLocation = flippedLocation(element, mouseEvent);
        end(&player, event.timeStamp());
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void PortalInteractionGesture::cancel(ModelPlayer* player)
{
    if (!m_isActive)
        return;

    end(player, MonotonicTime::now());
}

void PortalInteractionGesture::end(ModelPlayer* player, MonotonicTime timeStamp)
{
    m_isActive = false;

    if (RefPtr element = std::exchange(m_capturingElement, nullptr)) {
        if (RefPtr frame = element->document().frame())
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    }

    if (player)
        player->handleMouseUp(m_lastLocation, timeStamp);
}

MouseRelatedEvent& PortalInteractionGesture::mouseRelatedEvent(Event& event)
{
#if ENABLE(TOUCH_EVENTS)
    if (is<TouchEvent>(event))
        return downcast<TouchEvent>(event);
#endif
    return downcast<MouseEvent>(event);
}

FloatPoint PortalInteractionGesture::offsetLocation(Event& event)
{
    Ref mouseEvent = mouseRelatedEvent(event);
    return { static_cast<float>(mouseEvent->offsetX()), static_cast<float>(mouseEvent->offsetY()) };
}

LayoutPoint PortalInteractionGesture::flippedLocation(Element& element, MouseRelatedEvent& event)
{
    Ref document = element.document();
    document->updateLayoutIgnorePendingStylesheets();

    CheckedPtr box = dynamicDowncast<RenderBox>(element.renderer());
    if (!box)
        return { };

    auto location = box->absoluteToLocal(event.absoluteLocation(), MapCoordinatesMode::UseTransforms);
    location.move(-box->borderLeft().toFloat(), -box->borderTop().toFloat());

    if (RefPtr view = document->view()) {
        if (float scaleFactor = view->documentToAbsoluteScaleFactor(); scaleFactor != 1)
            location.scale(1 / scaleFactor);
    }

    return { LayoutUnit(location.x()), box->paddingBoxHeight() - LayoutUnit(location.y()) };
}

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
