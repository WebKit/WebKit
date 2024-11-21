/*
 * Copyright (C) 2024 Keith Cirkel <webkit@keithcirkel.co.uk>. All rights reserved.
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
#include "ToggleEventTask.h"

#include "EventNames.h"
#include "ToggleEvent.h"

namespace WebCore {

Ref<ToggleEventTask> ToggleEventTask::create(Element& element)
{
    return adoptRef(*new ToggleEventTask(element));
}

void ToggleEventTask::queue(ToggleState oldState, ToggleState newState)
{
    if (m_data)
        oldState = m_data->oldState;

    RefPtr element = m_element.get();
    if (!element)
        return;

    m_data = { oldState, newState };
    element->queueTaskKeepingThisNodeAlive(TaskSource::DOMManipulation, [this, newState] {
        if (!m_data || m_data->newState != newState)
            return;

        auto stringForState = [](ToggleState state) {
            return state == ToggleState::Closed ? "closed"_s : "open"_s;
        };

        RefPtr element = m_element.get();
        if (!element)
            return;

        auto data = *std::exchange(m_data, std::nullopt);
        element->dispatchEvent(ToggleEvent::create(eventNames().toggleEvent, { EventInit { }, stringForState(data.oldState), stringForState(data.newState) }, Event::IsCancelable::No));
    });
}

} // namespace WebCore
