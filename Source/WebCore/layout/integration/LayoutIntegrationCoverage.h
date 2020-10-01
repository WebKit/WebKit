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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "RenderStyleConstants.h"
#include <wtf/Forward.h>

namespace WebCore {

class RenderBlockFlow;

namespace LayoutIntegration {

enum class AvoidanceReason : uint64_t {
    FlowIsInsideANonMultiColumnThread     = 1LLU  << 0,
    FlowHasHorizonalWritingMode           = 1LLU  << 1,
    FlowHasOutline                        = 1LLU  << 2,
    FlowIsRuby                            = 1LLU  << 3,
    FlowIsPaginated                       = 1LLU  << 4,
    FlowHasTextOverflow                   = 1LLU  << 5,
    FlowIsDepricatedFlexBox               = 1LLU  << 6,
    FlowParentIsPlaceholderElement        = 1LLU  << 7,
    FlowParentIsTextAreaWithWrapping      = 1LLU  << 8,
    FlowHasNonSupportedChild              = 1LLU  << 9,
    FlowHasUnsupportedFloat               = 1LLU  << 10,
    FlowHasUnsupportedUnderlineDecoration = 1LLU  << 11,
    FlowHasJustifiedNonLatinText          = 1LLU  << 12,
    FlowHasOverflowNotVisible             = 1LLU  << 13,
    FlowHasWebKitNBSPMode                 = 1LLU  << 14,
    FlowIsNotLTR                          = 1LLU  << 15,
    FlowHasLineBoxContainProperty         = 1LLU  << 16,
    FlowIsNotTopToBottom                  = 1LLU  << 17,
    FlowHasLineBreak                      = 1LLU  << 18,
    FlowHasNonNormalUnicodeBiDi           = 1LLU  << 19,
    FlowHasRTLOrdering                    = 1LLU  << 20,
    FlowHasLineAlignEdges                 = 1LLU  << 21,
    FlowHasLineSnap                       = 1LLU  << 22,
    FlowHasTextEmphasisFillOrMark         = 1LLU  << 23,
    FlowHasTextShadow                     = 1LLU  << 24,
    FlowHasPseudoFirstLine                = 1LLU  << 25,
    FlowHasPseudoFirstLetter              = 1LLU  << 26,
    FlowHasTextCombine                    = 1LLU  << 27,
    FlowHasTextFillBox                    = 1LLU  << 28,
    FlowHasBorderFitLines                 = 1LLU  << 29,
    FlowHasNonAutoLineBreak               = 1LLU  << 30,
    FlowHasTextSecurity                   = 1LLU  << 31,
    FlowHasSVGFont                        = 1LLU  << 32,
    FlowTextIsEmpty                       = 1LLU  << 33,
    FlowTextHasSoftHyphen                 = 1LLU  << 34,
    FlowTextHasDirectionCharacter         = 1LLU  << 35,
    FlowIsMissingPrimaryFont              = 1LLU  << 36,
    FlowPrimaryFontIsInsufficient         = 1LLU  << 37,
    FlowTextIsCombineText                 = 1LLU  << 38,
    FlowTextIsRenderCounter               = 1LLU  << 39,
    FlowTextIsRenderQuote                 = 1LLU  << 40,
    FlowTextIsTextFragment                = 1LLU  << 41,
    FlowTextIsSVGInlineText               = 1LLU  << 42,
    FlowHasComplexFontCodePath            = 1LLU  << 43,
    FeatureIsDisabled                     = 1LLU  << 44,
    FlowHasNoParent                       = 1LLU  << 45,
    FlowHasNoChild                        = 1LLU  << 46,
    FlowChildIsSelected                   = 1LLU  << 47,
    FlowHasHangingPunctuation             = 1LLU  << 48,
    FlowFontHasOverflowGlyph              = 1LLU  << 49,
    FlowTextHasSurrogatePair              = 1LLU  << 50,
    MultiColumnFlowIsNotTopLevel          = 1LLU  << 51,
    MultiColumnFlowHasColumnSpanner       = 1LLU  << 52,
    MultiColumnFlowVerticalAlign          = 1LLU  << 53,
    MultiColumnFlowIsFloating             = 1LLU  << 54,
    FlowIncludesDocumentMarkers           = 1LLU  << 55,
    EndOfReasons                          = 1LLU  << 56
};

bool canUseForLineLayout(const RenderBlockFlow&);
bool canUseForLineLayoutAfterStyleChange(const RenderBlockFlow&, StyleDifference);

enum class IncludeReasons { First , All };
OptionSet<AvoidanceReason> canUseForLineLayoutWithReason(const RenderBlockFlow&, IncludeReasons);

}
}

#endif
