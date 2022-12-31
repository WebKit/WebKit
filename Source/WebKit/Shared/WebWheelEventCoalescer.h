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

#include "NativeWebWheelEvent.h"
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>

namespace WebKit {

struct NativeWebWheelEventAndSteps {
    NativeWebWheelEvent event;
    OptionSet<WebCore::WheelEventProcessingSteps> processingSteps;
};

struct WebWheelEventAndSteps {
    WebWheelEvent event;
    OptionSet<WebCore::WheelEventProcessingSteps> processingSteps;
    
    WebWheelEventAndSteps(const WebWheelEvent& event, OptionSet<WebCore::WheelEventProcessingSteps> processingSteps)
        : event(event)
        , processingSteps(processingSteps)
    { }

    explicit WebWheelEventAndSteps(const NativeWebWheelEventAndSteps& nativeEventAndSteps)
        : event(nativeEventAndSteps.event)
        , processingSteps(nativeEventAndSteps.processingSteps)
    { }
};

class WebWheelEventCoalescer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // If this returns true, use nextEventToDispatch() to get the event to dispatch.
    bool shouldDispatchEvent(const NativeWebWheelEvent&, OptionSet<WebCore::WheelEventProcessingSteps>);
    std::optional<WebWheelEventAndSteps> nextEventToDispatch();

    NativeWebWheelEvent takeOldestEventBeingProcessed();

    bool hasEventsBeingProcessed() const { return !m_eventsBeingProcessed.isEmpty(); }
    
    void clear();

private:
    using CoalescedEventSequence = Vector<NativeWebWheelEventAndSteps>;

    static bool canCoalesce(const WebWheelEventAndSteps&, const WebWheelEventAndSteps&);
    static WebWheelEventAndSteps coalesce(const WebWheelEventAndSteps&, const WebWheelEventAndSteps&);

    bool shouldDispatchEventNow(const WebWheelEvent&) const;

    Deque<NativeWebWheelEventAndSteps, 2> m_wheelEventQueue;
    Deque<std::unique_ptr<CoalescedEventSequence>> m_eventsBeingProcessed;
};

} // namespace WebKit
