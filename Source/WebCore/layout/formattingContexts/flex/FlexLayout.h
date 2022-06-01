/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FlexFormattingConstraints.h"
#include "FlexFormattingState.h"
#include "FlexRect.h"

namespace WebCore {
namespace Layout {

// This class implements the layout logic for flex formatting contexts.
// https://www.w3.org/TR/css-flexbox-1/
class FlexLayout {
public:
    FlexLayout(const FlexFormattingState&, const RenderStyle& flexBoxStyle);

    struct LogicalFlexItem {
        FlexRect marginRect;
        CheckedPtr<const ContainerBox> layoutBox;        
    };
    using LogicalFlexItems = Vector<LogicalFlexItem>;    
    void layout(const ConstraintsForFlexContent&, LogicalFlexItems&);

private:
    void computeLogicalWidthForFlexItems(LogicalFlexItems&, LayoutUnit availableSpace);
    void computeLogicalWidthForStretchingFlexItems(LogicalFlexItems&, LayoutUnit availableSpace);
    void computeLogicalWidthForShrinkingFlexItems(LogicalFlexItems&, LayoutUnit availableSpace);
    void computeLogicalHeightForFlexItems(LogicalFlexItems&, LayoutUnit availableSpace);
    void alignFlexItems(LogicalFlexItems&, LayoutUnit availableSpace);
    void justifyFlexItems(LogicalFlexItems&, LayoutUnit availableSpace);
    LayoutUnit computeAvailableLogicalVerticalSpace(LogicalFlexItems&, const ConstraintsForFlexContent&) const;
    LayoutUnit computeAvailableLogicalHorizontalSpace(LogicalFlexItems&, const ConstraintsForFlexContent&) const;

    const FlexFormattingState& formattingState() const { return m_formattingState; }
    const RenderStyle& flexBoxStyle() const { return m_flexBoxStyle; }

    const FlexFormattingState& m_formattingState;
    const RenderStyle& m_flexBoxStyle;
};

}
}

#endif
