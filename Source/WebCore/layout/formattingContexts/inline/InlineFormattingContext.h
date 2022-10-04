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
#include "InlineFormattingGeometry.h"
#include "InlineFormattingQuirks.h"
#include "InlineLineBuilder.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class InlineDamage;
class InlineFormattingState;
class LineBox;

// This class implements the layout logic for inline formatting contexts.
// https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
class InlineFormattingContext final : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingContext);
public:
    InlineFormattingContext(const ElementBox& formattingContextRoot, InlineFormattingState&, const InlineDamage* = nullptr);

    void layoutInFlowContent(const ConstraintsForInFlowContent&) override;
    IntrinsicWidthConstraints computedIntrinsicWidthConstraints() override;
    LayoutUnit usedContentHeight() const override;

    const InlineFormattingState& formattingState() const { return downcast<InlineFormattingState>(FormattingContext::formattingState()); }
    InlineFormattingState& formattingState() { return downcast<InlineFormattingState>(FormattingContext::formattingState()); }
    const InlineFormattingGeometry& formattingGeometry() const final { return m_inlineFormattingGeometry; }
    const InlineFormattingQuirks& formattingQuirks() const final { return m_inlineFormattingQuirks; }

    void layoutInFlowContentForIntegration(const ConstraintsForInFlowContent&);
    IntrinsicWidthConstraints computedIntrinsicWidthConstraintsForIntegration();

private:
    void lineLayout(InlineItems&, const LineBuilder::InlineItemRange&, const ConstraintsForInFlowContent&);
    void computeStaticPositionForOutOfFlowContent(const FormattingState::OutOfFlowBoxList&);

    void computeIntrinsicWidthForFormattingRoot(const Box&);
    InlineLayoutUnit computedIntrinsicWidthForConstraint(IntrinsicWidthMode) const;

    void computeHorizontalMargin(const Box&, const HorizontalConstraints&);
    void computeHeightAndMargin(const Box&, const HorizontalConstraints&);
    void computeWidthAndMargin(const Box&, const HorizontalConstraints&);

    void collectContentIfNeeded();
    InlineRect computeGeometryForLineContent(const LineBuilder::LineContent&);
    void invalidateFormattingState();

    const InlineDamage* m_lineDamage { nullptr };
    const InlineFormattingGeometry m_inlineFormattingGeometry;
    const InlineFormattingQuirks m_inlineFormattingQuirks;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(InlineFormattingContext, isInlineFormattingContext())

