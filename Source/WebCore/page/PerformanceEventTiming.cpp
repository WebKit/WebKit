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
#include "PerformanceEventTiming.h"

#include "Document.h"
#include "DocumentInlines.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "Node.h"
#include "PerformanceEventTimingCandidate.h"
#include <cmath>

namespace WebCore {

Ref<PerformanceEventTiming> PerformanceEventTiming::create(const PerformanceEventTimingCandidate& candidate, bool isFirst)
{
    return adoptRef(*new PerformanceEventTiming(candidate, isFirst));
}

PerformanceEventTiming::PerformanceEventTiming(const PerformanceEventTimingCandidate& candidate, bool isFirst)
    : PerformanceEntry(eventNames().eventNameFromEventType(candidate.type), candidate.startTime.milliseconds(), candidate.startTime.milliseconds() + durationResolutionInMilliseconds*std::round(candidate.duration.milliseconds() / durationResolutionInMilliseconds))
    , m_isFirst(isFirst)
    , m_cancelable(candidate.cancelable)
    , m_processingStart(candidate.processingStart)
    , m_processingEnd(candidate.processingEnd)
    , m_interactionID(candidate.interactionID)
    , m_target(candidate.target)
{ }

PerformanceEventTiming::~PerformanceEventTiming() = default;

Node* PerformanceEventTiming::target() const
{
    if (!m_target || !m_target->isNode())
        return nullptr;

    RefPtr<Node> node = downcast<Node>((m_target.get()));
    if (!node)
        return nullptr;

    Ref document = node->document();
    if (!node->isConnected() || !document->isFullyActive())
        return nullptr;

    return node.get();
}

PerformanceEntry::Type PerformanceEventTiming::performanceEntryType() const
{
    return m_isFirst ? Type::FirstInput : Type::Event;
}

ASCIILiteral PerformanceEventTiming::entryType() const
{
    return m_isFirst ? "first-input"_s : "event"_s;
}

uint64_t PerformanceEventTiming::interactionId() const
{
    return m_interactionID.value;
}

} // namespace WebCore
