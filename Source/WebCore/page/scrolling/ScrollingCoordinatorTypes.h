/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ScrollTypes.h"

namespace WebCore {

typedef unsigned SynchronousScrollingReasons;
typedef uint64_t ScrollingNodeID;

enum class ScrollingNodeType : uint8_t {
    MainFrame,
    Subframe,
    FrameHosting,
    Overflow,
    Fixed,
    Sticky
};

enum ScrollingStateTreeAsTextBehaviorFlags {
    ScrollingStateTreeAsTextBehaviorNormal                  = 0,
    ScrollingStateTreeAsTextBehaviorIncludeLayerIDs         = 1 << 0,
    ScrollingStateTreeAsTextBehaviorIncludeNodeIDs          = 1 << 1,
    ScrollingStateTreeAsTextBehaviorIncludeLayerPositions   = 1 << 2,
    ScrollingStateTreeAsTextBehaviorDebug                   = ScrollingStateTreeAsTextBehaviorIncludeLayerIDs | ScrollingStateTreeAsTextBehaviorIncludeNodeIDs | ScrollingStateTreeAsTextBehaviorIncludeLayerPositions
};
typedef unsigned ScrollingStateTreeAsTextBehavior;

enum class ScrollingLayerPositionAction {
    Set,
    SetApproximate,
    Sync
};

struct ScrollableAreaParameters {
    ScrollElasticity horizontalScrollElasticity { ScrollElasticityNone };
    ScrollElasticity verticalScrollElasticity { ScrollElasticityNone };

    ScrollbarMode horizontalScrollbarMode { ScrollbarAuto };
    ScrollbarMode verticalScrollbarMode { ScrollbarAuto };

    bool hasEnabledHorizontalScrollbar { false };
    bool hasEnabledVerticalScrollbar { false };

    bool useDarkAppearanceForScrollbars { false };

    bool operator==(const ScrollableAreaParameters& other) const
    {
        return horizontalScrollElasticity == other.horizontalScrollElasticity
            && verticalScrollElasticity == other.verticalScrollElasticity
            && horizontalScrollbarMode == other.horizontalScrollbarMode
            && verticalScrollbarMode == other.verticalScrollbarMode
            && hasEnabledHorizontalScrollbar == other.hasEnabledHorizontalScrollbar
            && hasEnabledVerticalScrollbar == other.hasEnabledVerticalScrollbar
            && useDarkAppearanceForScrollbars == other.useDarkAppearanceForScrollbars;
    }
};

enum class ViewportRectStability {
    Stable,
    Unstable,
    ChangingObscuredInsetsInteractively // This implies Unstable.
};

}
