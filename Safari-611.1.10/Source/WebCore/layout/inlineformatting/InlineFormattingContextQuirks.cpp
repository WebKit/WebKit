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

#include "config.h"
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

InlineLayoutUnit InlineFormattingContext::Quirks::initialLineHeight() const
{
    // Negative lineHeight value means the line-height is not set
    auto& root = formattingContext().root();
    if (layoutState().inNoQuirksMode() || !root.style().lineHeight().isNegative())
        return root.style().computedLineHeight();
    return root.style().fontMetrics().floatHeight();
}

bool InlineFormattingContext::Quirks::inlineLevelBoxAffectsLineBox(const LineBox::InlineLevelBox& inlineLevelBox, const LineBox& lineBox) const
{
    if (inlineLevelBox.isInlineBox())
        return inlineLevelBox.hasContent();
    if (inlineLevelBox.isLineBreakBox()) {
        // <br> in non-standard mode stretches the line box only when the line is empty.
        // e.g. <div><span><br></span></div> will stretch but <div>this will not stretch to 200px<span style="font-size: 200px;"><br></span></div>
        return layoutState().inNoQuirksMode() ? true : lineBox.isConsideredEmpty();
    }
    if (inlineLevelBox.isAtomicInlineLevelBox()) {
        if (inlineLevelBox.layoutBounds().height())
            return true;
        // While in practice when the negative vertical margin makes the layout bounds empty (e.g: height: 100px; margin-top: -100px;), and this inline
        // level box contributes 0px to the line box height, it still needs to be taken into account while computing line box geometries.
        auto& boxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
        return boxGeometry.marginBefore() || boxGeometry.marginAfter();
    }
    return false;
}

bool InlineFormattingContext::Quirks::hasSoftWrapOpportunityAtImage() const
{
    return !formattingContext().root().isTableCell();
}

}
}

#endif
