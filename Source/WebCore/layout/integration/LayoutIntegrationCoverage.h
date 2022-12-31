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

#include "RenderStyleConstants.h"
#include <wtf/Forward.h>

namespace WebCore {

class RenderBlockFlow;
class RenderFlexibleBox;
class RenderInline;

namespace LayoutIntegration {

enum class AvoidanceReason : uint64_t {
    FlowIsInsideANonMultiColumnThread            = 1LLU  << 0,
    FlowHasInitialLetter                         = 1LLU  << 1,
    // Unused                                    = 1LLU  << 2,
    ContentIsRuby                                = 1LLU  << 3,
    FlowIsPaginated                              = 1LLU  << 4,
    // Unused                                    = 1LLU  << 5,
    FlowHasLineClamp                             = 1LLU  << 6,
    // Unused                                    = 1LLU  << 7,
    // Unused                                    = 1LLU  << 8,
    FlowHasNonSupportedChild                     = 1LLU  << 9,
    FloatIsShapeOutside                          = 1LLU  << 10,
    // Unused                                    = 1LLU  << 11,
    // Unused                                    = 1LLU  << 12,
    // Unused                                    = 1LLU  << 13,
    // Unused                                    = 1LLU  << 14,
    // Unused                                    = 1LLU  << 15,
    // Unused                                    = 1LLU  << 16,
    FlowHasUnsupportedWritingMode                = 1LLU  << 17,
    // Unused                                    = 1LLU  << 18,
    // Unused                                    = 1LLU  << 19,
    FlowHasLineAlignEdges                        = 1LLU  << 20,
    FlowHasLineSnap                              = 1LLU  << 21,
    // Unused                                    = 1LLU  << 22,
    // Unused                                    = 1LLU  << 23,
    // Unused                                    = 1LLU  << 24,
    // Unused                                    = 1LLU  << 25,
    // Unused                                    = 1LLU  << 26,
    // Unused                                    = 1LLU  << 27,
    // Unused                                    = 1LLU  << 28,
    // Unused                                    = 1LLU  << 29,
    // Unused                                    = 1LLU  << 30,
    // Unused                                    = 1LLU  << 31,
    // Unused                                    = 1LLU  << 32,
    // Unused                                    = 1LLU  << 33,
    // Unused                                    = 1LLU  << 34,
    // Unused                                    = 1LLU  << 35,
    // Unused                                    = 1LLU  << 36,
    // Unused                                    = 1LLU  << 37,
    // Unused                                    = 1LLU  << 38,
    FlowTextIsSVGInlineText                      = 1LLU  << 39,
    // Unused                                    = 1LLU  << 40,
    FeatureIsDisabled                            = 1LLU  << 41,
    FlowDoesNotEstablishInlineFormattingContext  = 1LLU  << 42,
    // Unused                                    = 1LLU  << 43,
    // Unused                                    = 1LLU  << 44,
    // Unused                                    = 1LLU  << 45,
    // Unused                                    = 1LLU  << 46,
    MultiColumnFlowIsNotTopLevel                 = 1LLU  << 47,
    MultiColumnFlowHasColumnSpanner              = 1LLU  << 48,
    MultiColumnFlowVerticalAlign                 = 1LLU  << 49,
    MultiColumnFlowIsFloating                    = 1LLU  << 50,
    // Unused                                    = 1LLU  << 51,
    // Unused                                    = 1LLU  << 52,
    // Unused                                    = 1LLU  << 53,
    // Unused                                    = 1LLU  << 54,
    ChildBoxIsFloatingOrPositioned               = 1LLU  << 55,
    ContentIsSVG                                 = 1LLU  << 56,
    // Unused                                    = 1LLU  << 57,
    // Unused                                    = 1LLU  << 58,
    // Unused                                    = 1LLU  << 59,
    // Unused                                    = 1LLU  << 60,
    ChildIsUnsupportedListItem                   = 1LLU  << 61,
    EndOfReasons                                 = 1LLU  << 62
};

bool canUseForLineLayout(const RenderBlockFlow&);
bool canUseForLineLayoutAfterStyleChange(const RenderBlockFlow&, StyleDifference);
bool canUseForLineLayoutAfterInlineBoxStyleChange(const RenderInline&, StyleDifference);

bool canUseForFlexLayout(const RenderFlexibleBox&);

enum class IncludeReasons { First , All };
OptionSet<AvoidanceReason> canUseForLineLayoutWithReason(const RenderBlockFlow&, IncludeReasons);

}
}

