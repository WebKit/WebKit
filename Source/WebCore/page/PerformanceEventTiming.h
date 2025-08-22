/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#include "DOMHighResTimeStamp.h"
#include "EventTarget.h"
#include "EventTimingInteractionID.h"
#include "PerformanceEntry.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Node;
struct PerformanceEventTimingCandidate;

class PerformanceEventTiming final : public PerformanceEntry {
public:
    static Ref<PerformanceEventTiming> create(const PerformanceEventTimingCandidate&, bool isFirst = false);
    ~PerformanceEventTiming();

    DOMHighResTimeStamp processingStart() const { return m_processingStart.milliseconds(); }
    DOMHighResTimeStamp processingEnd() const { return m_processingEnd.milliseconds(); }
    bool cancelable() const { return m_cancelable; }
    Node* target() const;
    uint64_t interactionId() const;

    Type performanceEntryType() const final;
    ASCIILiteral entryType() const final;

    static constexpr DOMHighResTimeStamp durationResolutionInMilliseconds = 8;
    static constexpr Seconds durationResolution = 8_ms;
    static constexpr Seconds minimumDurationThreshold = 16_ms;
    static constexpr Seconds defaultDurationThreshold = 104_ms;

private:
    PerformanceEventTiming(const PerformanceEventTimingCandidate&, bool isFirst);

    bool m_isFirst;
    bool m_cancelable;
    Seconds m_processingStart;
    Seconds m_processingEnd;
    EventTimingInteractionID m_interactionID;
    WeakPtr<EventTarget, EventTarget::WeakPtrImplType> m_target;
};

} // namespace WebCore
