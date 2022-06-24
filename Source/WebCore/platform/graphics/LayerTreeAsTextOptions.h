/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/OptionSet.h>

namespace WebCore {

enum class LayerTreeAsTextOptions : uint16_t {
    Debug                        = 1 << 0, // Dump extra debugging info like layer addresses.
    IncludeVisibleRects          = 1 << 1,
    IncludeTileCaches            = 1 << 2,
    IncludeRepaintRects          = 1 << 3,
    IncludePaintingPhases        = 1 << 4,
    IncludeContentLayers         = 1 << 5,
    IncludePageOverlayLayers     = 1 << 6,
    IncludeAcceleratesDrawing    = 1 << 7,
    IncludeClipping              = 1 << 8,
    IncludeBackingStoreAttached  = 1 << 9,
    IncludeRootLayerProperties   = 1 << 10,
    IncludeEventRegion           = 1 << 11,
    IncludeDeepColor             = 1 << 12,
    IncludeDeviceScale           = 1 << 13,
};

static constexpr OptionSet<LayerTreeAsTextOptions> AllLayerTreeAsTextOptions = {
    LayerTreeAsTextOptions::Debug,
    LayerTreeAsTextOptions::IncludeVisibleRects,
    LayerTreeAsTextOptions::IncludeTileCaches,
    LayerTreeAsTextOptions::IncludeRepaintRects,
    LayerTreeAsTextOptions::IncludePaintingPhases,
    LayerTreeAsTextOptions::IncludeContentLayers,
    LayerTreeAsTextOptions::IncludePageOverlayLayers,
    LayerTreeAsTextOptions::IncludeAcceleratesDrawing,
    LayerTreeAsTextOptions::IncludeClipping,
    LayerTreeAsTextOptions::IncludeBackingStoreAttached,
    LayerTreeAsTextOptions::IncludeRootLayerProperties,
    LayerTreeAsTextOptions::IncludeEventRegion,
    LayerTreeAsTextOptions::IncludeDeepColor,
};

} // namespace WebCore
