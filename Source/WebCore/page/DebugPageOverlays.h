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

#pragma once

#include "Frame.h"
#include "Settings.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class Page;
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

    WEBCORE_EXPORT static void settingsChanged(Page&);

private:
    static bool hasOverlays(Page&);

    void showRegionOverlay(Page&, RegionType);
    void hideRegionOverlay(Page&, RegionType);

    void regionChanged(Frame&, RegionType);

    bool hasOverlaysForPage(Page& page) const
    {
        return m_pageRegionOverlays.contains(&page);
    }
    
    void updateOverlayRegionVisibility(Page&, DebugOverlayRegions);

    RegionOverlay* regionOverlayForPage(Page&, RegionType) const;
    RegionOverlay& ensureRegionOverlayForPage(Page&, RegionType);

    HashMap<Page*, Vector<RefPtr<RegionOverlay>>> m_pageRegionOverlays;

    static DebugPageOverlays* sharedDebugOverlays;
};

#define FAST_RETURN_IF_NO_OVERLAYS(page) if (LIKELY(!page || !hasOverlays(*page))) return;

inline bool DebugPageOverlays::hasOverlays(Page& page)
{
    if (!sharedDebugOverlays)
        return false;

    return sharedDebugOverlays->hasOverlaysForPage(page);
}

inline void DebugPageOverlays::didLayout(Frame& frame)
{
    FAST_RETURN_IF_NO_OVERLAYS(frame.page());

    sharedDebugOverlays->regionChanged(frame, RegionType::WheelEventHandlers);
    sharedDebugOverlays->regionChanged(frame, RegionType::NonFastScrollableRegion);
}

inline void DebugPageOverlays::didChangeEventHandlers(Frame& frame)
{
    FAST_RETURN_IF_NO_OVERLAYS(frame.page());

    sharedDebugOverlays->regionChanged(frame, RegionType::WheelEventHandlers);
    sharedDebugOverlays->regionChanged(frame, RegionType::NonFastScrollableRegion);
}

} // namespace WebCore
