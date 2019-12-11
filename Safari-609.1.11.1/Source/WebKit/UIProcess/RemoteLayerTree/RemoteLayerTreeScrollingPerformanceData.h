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

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;

class RemoteLayerTreeScrollingPerformanceData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerTreeScrollingPerformanceData(RemoteLayerTreeDrawingAreaProxy&);
    ~RemoteLayerTreeScrollingPerformanceData();

    void didCommitLayerTree(const WebCore::FloatRect& visibleRect);
    void didScroll(const WebCore::FloatRect& visibleRect);

    NSArray *data(); // Array of [ time, event type, unfilled pixel count ]

private:
    unsigned blankPixelCount(const WebCore::FloatRect& visibleRect) const;
    
    struct BlankPixelCount {
        enum EventType { Filled, Exposed };

        double startTime;
        double endTime;
        EventType eventType;
        unsigned blankPixelCount;
        
        BlankPixelCount(double start, double end, EventType type, unsigned count)
            : startTime(start)
            , endTime(end)
            , eventType(type)
            , blankPixelCount(count)
        { }
        
        bool canCoalesce(BlankPixelCount::EventType, unsigned blankPixelCount) const;
    };
    
    void appendBlankPixelCount(BlankPixelCount::EventType, unsigned blankPixelCount);

    RemoteLayerTreeDrawingAreaProxy& m_drawingArea;
    Vector<BlankPixelCount> m_blankPixelCounts;
};

}
