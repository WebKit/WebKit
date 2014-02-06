/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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


#ifndef WheelEventDeltaTracker_h
#define WheelEventDeltaTracker_h

#include "FloatSize.h"
#include <wtf/Deque.h>

namespace WebCore {

class PlatformWheelEvent;

const size_t recentEventCount = 3;

enum class DominantScrollGestureDirection {
    None,
    Vertical,
    Horizontal
};

class WheelEventDeltaTracker {
public:
    WheelEventDeltaTracker();
    virtual ~WheelEventDeltaTracker();

    void beginTrackingDeltas();
    void endTrackingDeltas();

    bool isTrackingDeltas() const { return m_isTrackingDeltas; }
    bool isFirstWheelEvent() const { return m_recentWheelEventDeltas.size() <= 1; }

    void recordWheelEventDelta(const PlatformWheelEvent&);
    DominantScrollGestureDirection dominantScrollGestureDirection() const;

private:
    Deque<FloatSize, recentEventCount> m_recentWheelEventDeltas;
    bool m_isTrackingDeltas;

};

} // namespace WebCore

#endif // WheelEventDeltaTracker_h
