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

#pragma once

#if ENABLE(MODEL_ELEMENT)

#include <WebCore/FloatPoint.h>
#include <WebCore/LayoutPoint.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Element;
class Event;
class ModelPlayer;
class MouseRelatedEvent;
class WeakPtrImplWithEventTargetData;

class PortalInteractionGesture {
    WTF_MAKE_NONCOPYABLE(PortalInteractionGesture);
public:
    enum class Phase : uint8_t { Began, Changed, Ended };

    PortalInteractionGesture() = default;

    std::optional<Phase> handleEvent(Element& element, ModelPlayer& player, Event& event)
    {
        return handleEvent(element, player, event, [](FloatPoint) {
            return true;
        });
    }

    std::optional<Phase> handleEvent(Element& element, ModelPlayer& player, Event& event, NOESCAPE const Invocable<bool(FloatPoint)> auto& canBegin)
    {
        auto phase = phaseForEvent(event);
        if (!phase)
            return std::nullopt;

        if (*phase == Phase::Began && !canBegin(offsetLocation(event)))
            return std::nullopt;

        if (!apply(*phase, element, player, event))
            return std::nullopt;

        return phase;
    }

    void cancel(ModelPlayer*);

private:
    std::optional<Phase> phaseForEvent(const Event&) const;
    bool apply(Phase, Element&, ModelPlayer&, Event&);
    void end(ModelPlayer*, MonotonicTime);

    static MouseRelatedEvent& mouseRelatedEvent(Event&);
    static FloatPoint offsetLocation(Event&);
    static LayoutPoint flippedLocation(Element&, MouseRelatedEvent&);

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_capturingElement;
    LayoutPoint m_lastLocation;
    bool m_isActive { false };
};

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
