/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FormattingContext.h"
#include "FormattingState.h"
#include "InlineDisplayContent.h"
#include "InlineFormattingConstraints.h"
#include "InlineFormattingGeometry.h"
#include "InlineFormattingQuirks.h"
#include "InlineLineBuilder.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class InlineLayoutState;
class InlineDamage;
class InlineFormattingState;
class LineBox;

struct InlineLayoutResult {
    InlineDisplay::Content displayContent;
    enum class Range : uint8_t {
        Full, // Display content represents the complete inline content -result of full layout
        FullFromDamage, // Display content represents part of the inline content starting from damaged line until the end of inline content -result of partial layout with continuous damage all the way to the end of the inline content
        PartialFromDamage // Display content represents part of the inline content starting from damaged line until damage stops -result of partial layout with damage that does not cover the entire inline content
    };
    Range range { Range::Full };
};

// This class implements the layout logic for inline formatting contexts.
// https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
class InlineFormattingContext final : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingContext);
public:
    InlineFormattingContext(const ElementBox& formattingContextRoot, InlineFormattingState&, const InlineDamage* = nullptr);

    InlineLayoutResult layoutInFlowAndFloatContent(const ConstraintsForInlineContent&, InlineLayoutState&);
    void layoutOutOfFlowContent(const ConstraintsForInlineContent&, InlineLayoutState&, const InlineDisplay::Content&);
    IntrinsicWidthConstraints computedIntrinsicWidthConstraints() override;
    LayoutUnit maximumContentSize();

    const InlineFormattingGeometry& formattingGeometry() const final { return m_inlineFormattingGeometry; }
    const InlineFormattingQuirks& formattingQuirks() const final { return m_inlineFormattingQuirks; }
    const InlineFormattingState& formattingState() const { return downcast<InlineFormattingState>(FormattingContext::formattingState()); }

private:
    InlineLayoutResult lineLayout(const InlineItems&, InlineItemRange, std::optional<PreviousLine>, const ConstraintsForInlineContent&, InlineLayoutState&);
    void layoutFloatContentOnly(const ConstraintsForInlineContent&, FloatingState&);
    void computeStaticPositionForOutOfFlowContent(const FormattingState::OutOfFlowBoxList&, const ConstraintsForInFlowContent&, const InlineDisplay::Content&, const FloatingState&);

    InlineLayoutUnit computedIntrinsicWidthForConstraint(IntrinsicWidthMode) const;

    void collectContentIfNeeded();
    InlineRect createDisplayContentForLine(size_t lineIndex, const LineBuilder::LayoutResult&, const ConstraintsForInlineContent&, const InlineLayoutState&, InlineDisplay::Content&);
    void resetGeometryForClampedContent(const InlineItemRange& needsDisplayContentRange, const LineBuilder::SuspendedFloatList& suspendedFloats, LayoutPoint topleft);

    InlineFormattingState& formattingState() { return downcast<InlineFormattingState>(FormattingContext::formattingState()); }

    const InlineDamage* m_lineDamage { nullptr };
    const InlineFormattingGeometry m_inlineFormattingGeometry;
    const InlineFormattingQuirks m_inlineFormattingQuirks;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(InlineFormattingContext, isInlineFormattingContext())

