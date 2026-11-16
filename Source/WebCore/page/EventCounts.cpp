/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "EventCounts.h"

#include "IDLTypes.h"
#include "JSDOMMapLike.h"
#include <algorithm>

namespace WebCore {

EventCounts::EventCounts(Performance* performance)
    : m_performance(*performance)
{ }

void EventCounts::add(EventType type)
{
    size_t index = std::ranges::lower_bound(EventNames::timedEvents, type) - EventNames::timedEvents.begin();
    ASSERT(index < m_counts.size());
    ++m_counts[index];
}

void EventCounts::initializeMapLike(DOMMapAdapter& map)
{
    auto& eventNamesObject = eventNames();
    for (size_t index = 0; index < EventNames::timedEvents.size(); ++index) {
        auto type = EventNames::timedEvents[index];
        map.set<IDLDOMString, IDLUnsignedLongLong>(eventNamesObject.eventNameFromEventType(type), m_counts[index]);
    }
}

} // namespace WebCore
