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

#include "LayoutBoxGeometry.h"

namespace WebCore {
namespace Layout {

InlineLayoutUnit InlineFormattingContext::Quirks::initialLineHeight() const
{
    // Negative lineHeight value means the line-height is not set
    auto& root = formattingContext().root();
    if (layoutState().inStandardsMode() || !root.style().lineHeight().isNegative())
        return root.style().computedLineHeight();
    return root.style().fontMetrics().floatHeight();
}

bool InlineFormattingContext::Quirks::inlineLevelBoxAffectsLineBox(const LineBox::InlineLevelBox& inlineLevelBox, const LineBox& lineBox) const
{
    if (inlineLevelBox.isLineBreakBox()) {
        if (layoutState().inStandardsMode())
            return true;
        // In quirks mode linebreak boxes (<br>) stop affecting the line box when (assume <br> is nested e.g. <span style="font-size: 100px"><br></span>)
        // 1. the root inline box has content <div>content<br>/div>
        // 2. there's at least one atomic inline level box on the line e.g <div><img><br></div> or <div><span><img></span><br></div>
        // 3. there's at least one inline box with content e.g. <div><span>content</span><br></div>
        if (lineBox.rootInlineBox().hasContent())
            return false;
        if (lineBox.hasAtomicInlineLevelBox())
            return false;
        // At this point we either have only the <br> on the line or inline boxes with or without content.
        auto& inlineLevelBoxes = lineBox.nonRootInlineLevelBoxes();
        ASSERT(!inlineLevelBoxes.isEmpty());
        if (inlineLevelBoxes.size() == 1)
            return true;
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            // Filter out empty inline boxes e.g. <div><span></span><span></span><br></div>
            if (inlineLevelBox->isInlineBox() && inlineLevelBox->hasContent())
                return false;
        }
        return true;
    }
    if (inlineLevelBox.isInlineBox()) {
        // Inline boxes (e.g. root inline box or <span>) affects line boxes either through the strut or actual content.
        auto inlineBoxHasImaginaryStrut = layoutState().inStandardsMode();
        if (inlineLevelBox.hasContent() || inlineBoxHasImaginaryStrut)
            return true;
        if (inlineLevelBox.isRootInlineBox()) {
            auto shouldRootInlineBoxWithNoContentStretchLineBox = [&] {
                if (inlineLevelBox.layoutBox().style().lineHeight().isNegative())
                    return false;
                // The root inline box with non-initial line height value stretches the line box even when root has no content
                // but there's at least one inline box with content.
                // e.g. <div style="line-height: 100px;"><span>content</span></div>
                if (!lineBox.hasInlineBox())
                    return false;
                for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
                    if (!inlineLevelBox->isInlineBox())
                        continue;
                    if (inlineLevelBox->hasContent())
                        return true;
                }
                return false;
            };
            return shouldRootInlineBoxWithNoContentStretchLineBox();
        }
        // Non-root inline boxes (e.g. <span>).
        auto& boxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
        if (boxGeometry.horizontalBorder() || boxGeometry.horizontalPadding().valueOr(0_lu)) {
            // Horizontal border and padding make the inline box stretch the line (e.g. <span style="padding: 10px;"></span>).
            return true;
        }
        return false;
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
