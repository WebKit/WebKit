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
#include "DisplayBox.h"
#include "InlineFormattingContext.h"
#include "InlineLineBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

bool InlineFormattingContext::Quirks::lineDescentNeedsCollapsing(const Line::RunList& runList) const
{
    // Collapse line descent in limited and full quirk mode when there's no baseline aligned content or
    // the baseline aligned content has no descent.
    auto& layoutState = this->layoutState();
    if (!layoutState.inQuirksMode() && !layoutState.inLimitedQuirksMode())
        return false;

    for (auto& run : runList) {
        auto& layoutBox = run.layoutBox();
        if (run.isContainerEnd() || layoutBox.style().verticalAlign() != VerticalAlign::Baseline)
            continue;

        if (run.isLineBreak())
            return false;
        if (run.isText())
            return false;
        if (run.isContainerStart()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            if (boxGeometry.horizontalBorder() || (boxGeometry.horizontalPadding() && boxGeometry.horizontalPadding().value()))
                return false;
            continue;
        }
        if (run.isBox()) {
            if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = layoutState.establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                auto lastLineBox = formattingState.displayInlineContent()->lineBoxes.last();
                if (lastLineBox.height() > lastLineBox.baseline())
                    return false;
            }
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    return true;
}

InlineLayoutUnit InlineFormattingContext::Quirks::initialLineHeight() const
{
    // Negative lineHeight value means the line-height is not set
    auto& root = formattingContext().root();
    if (layoutState().inNoQuirksMode() || !root.style().lineHeight().isNegative())
        return root.style().computedLineHeight();
    return root.style().fontMetrics().floatHeight();
}

}
}

#endif
