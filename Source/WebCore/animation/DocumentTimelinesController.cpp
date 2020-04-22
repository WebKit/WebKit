/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DocumentTimelinesController.h"

#include "DocumentTimeline.h"

namespace WebCore {

DocumentTimelinesController::DocumentTimelinesController()
{
}

DocumentTimelinesController::~DocumentTimelinesController()
{
}

void DocumentTimelinesController::addTimeline(DocumentTimeline& timeline)
{
    m_timelines.add(timeline);
}

void DocumentTimelinesController::removeTimeline(DocumentTimeline& timeline)
{
    m_timelines.remove(timeline);
}

void DocumentTimelinesController::detachFromDocument()
{
    while (!m_timelines.computesEmpty())
        m_timelines.begin()->detachFromDocument();
}

void DocumentTimelinesController::updateAnimationsAndSendEvents(DOMHighResTimeStamp timestamp)
{
    ASSERT(!m_timelines.hasNullReferences());

    // We need to copy m_timelines before iterating over its members since calling updateAnimationsAndSendEvents() may mutate m_timelines.
    Vector<RefPtr<DocumentTimeline>> timelines;
    bool shouldUpdateAnimations = false;
    for (auto& timeline : m_timelines) {
        if (timeline.scheduledUpdate())
            shouldUpdateAnimations = true;
        timelines.append(&timeline);
    }

    for (auto& timeline : timelines) {
        timeline->updateCurrentTime(timestamp);
        if (shouldUpdateAnimations)
            timeline->updateAnimationsAndSendEvents();
    }
}

} // namespace WebCore

