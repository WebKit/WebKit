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
#include "PlatformWheelEvent.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

#if ENABLE(KINETIC_SCROLLING)

TextStream& operator<<(TextStream& ts, PlatformWheelEventPhase phase)
{
    switch (phase) {
    case PlatformWheelEventPhase::None: ts << "none"; break;
    case PlatformWheelEventPhase::Began: ts << "began"; break;
    case PlatformWheelEventPhase::Stationary: ts << "stationary"; break;
    case PlatformWheelEventPhase::Changed: ts << "changed"; break;
    case PlatformWheelEventPhase::Ended: ts << "ended"; break;
    case PlatformWheelEventPhase::Cancelled: ts << "cancelled"; break;
    case PlatformWheelEventPhase::MayBegin: ts << "mayBegin"; break;
    }
    return ts;
}

#endif

TextStream& operator<<(TextStream& ts, const PlatformWheelEvent& event)
{
    ts << "PlatformWheelEvent " << &event << " at " << event.position() << " deltaX " << event.deltaX() << " deltaY " << event.deltaY();

#if ENABLE(KINETIC_SCROLLING)
    ts << " phase \"" << event.phase() << "\" momentum phase \"" << event.momentumPhase() << "\"";
#endif

    return ts;
}

TextStream& operator<<(TextStream& ts, WheelEventProcessingSteps steps)
{
    switch (steps) {
    case WheelEventProcessingSteps::ScrollingThread: ts << "scrolling thread"; break;
    case WheelEventProcessingSteps::MainThreadForScrolling: ts << "main thread scrolling"; break;
    case WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch: ts << "main thread non-blocking DOM event dispatch"; break;
    case WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch: ts << "main thread blocking DOM event dispatch"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EventHandling steps)
{
    switch (steps) {
    case EventHandling::DispatchedToDOM: ts << "dispatched to DOM"; break;
    case EventHandling::DefaultPrevented: ts << "default prevented"; break;
    case EventHandling::DefaultHandled: ts << "default handled"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WheelScrollGestureState state)
{
    switch (state) {
    case WheelScrollGestureState::Blocking: ts << "blocking"; break;
    case WheelScrollGestureState::NonBlocking: ts << "non-blocking"; break;
    }
    return ts;
}

} // namespace WebCore
