/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import <WebCore/FloatRect.h>
#import <wtf/Vector.h>
#include <wtf/MonotonicTime.h>

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;

class RemoteLayerTreeScrollingPerformanceData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerTreeScrollingPerformanceData(RemoteLayerTreeDrawingAreaProxy&);
    ~RemoteLayerTreeScrollingPerformanceData();

    void didCommitLayerTree(const WebCore::FloatRect& visibleRect);
    void didScroll(const WebCore::FloatRect& visibleRect);
    void didChangeSynchronousScrollingReasons(WTF::MonotonicTime, uint64_t scrollingChangeData);

    NSArray *data(); // Array of [ time, event type, unfilled pixel count ]
    void logData();

private:
    struct ScrollingLogEvent {
        enum EventType { Filled, Exposed, SwitchedScrollingMode };

        WTF::MonotonicTime startTime;
        WTF::MonotonicTime endTime;
        EventType eventType;
        uint64_t value;
        
        ScrollingLogEvent(WTF::MonotonicTime start, WTF::MonotonicTime end, EventType type, uint64_t data)
            : startTime(start)
            , endTime(end)
            , eventType(type)
            , value(data)
        { }
        
        bool canCoalesce(ScrollingLogEvent::EventType, uint64_t blankPixelCount) const;
    };
    
    unsigned blankPixelCount(const WebCore::FloatRect& visibleRect) const;

    void appendBlankPixelCount(ScrollingLogEvent::EventType, uint64_t blankPixelCount);
    void appendSynchronousScrollingChange(WTF::MonotonicTime, uint64_t);

    RemoteLayerTreeDrawingAreaProxy& m_drawingArea;
    Vector<ScrollingLogEvent> m_events;
#if PLATFORM(MAC)
    uint64_t m_lastUnfilledArea;
#endif
};

}
