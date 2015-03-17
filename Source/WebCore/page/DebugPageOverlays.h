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

#ifndef DebugPageOverlays_h
#define DebugPageOverlays_h

#include "Frame.h"
#include "Settings.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class MainFrame;
class RegionOverlay;

class DebugPageOverlays {
public:
    static DebugPageOverlays& singleton();

    enum class RegionType {
        WheelEventHandlers,
        NonFastScrollableRegion,
    };
    static const unsigned NumberOfRegionTypes = NonFastScrollableRegion + 1;

    static void didLayout(Frame&);
    static void didChangeEventHandlers(Frame&);

    WEBCORE_EXPORT static void settingsChanged(MainFrame&);

private:
    static bool hasOverlays(MainFrame&);

    void showRegionOverlay(MainFrame&, RegionType);
    void hideRegionOverlay(MainFrame&, RegionType);

    void regionChanged(Frame&, RegionType);

    bool hasOverlaysForFrame(MainFrame& frame) const
    {
        return m_frameRegionOverlays.contains(&frame);
    }
    
    void updateOverlayRegionVisibility(MainFrame&, DebugOverlayRegions);

    RegionOverlay* regionOverlayForFrame(MainFrame&, RegionType) const;
    RegionOverlay& ensureRegionOverlayForFrame(MainFrame&, RegionType);

    HashMap<MainFrame*, Vector<RefPtr<RegionOverlay>>> m_frameRegionOverlays;

    static DebugPageOverlays* sharedDebugOverlays;
};

#define FAST_RETURN_IF_NO_OVERLAYS(frame) if (LIKELY(!hasOverlays(frame))) return;

inline bool DebugPageOverlays::hasOverlays(MainFrame& frame)
{
    if (!sharedDebugOverlays)
        return false;

    return sharedDebugOverlays->hasOverlaysForFrame(frame);
}

inline void DebugPageOverlays::didLayout(Frame& frame)
{
    FAST_RETURN_IF_NO_OVERLAYS(frame.mainFrame());

    sharedDebugOverlays->regionChanged(frame, RegionType::WheelEventHandlers);
    sharedDebugOverlays->regionChanged(frame, RegionType::NonFastScrollableRegion);
}

inline void DebugPageOverlays::didChangeEventHandlers(Frame& frame)
{
    FAST_RETURN_IF_NO_OVERLAYS(frame.mainFrame());

    sharedDebugOverlays->regionChanged(frame, RegionType::WheelEventHandlers);
    sharedDebugOverlays->regionChanged(frame, RegionType::NonFastScrollableRegion);
}

}

#endif
