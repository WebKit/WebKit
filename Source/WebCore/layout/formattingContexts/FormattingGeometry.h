/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "FormattingContext.h"

namespace WebCore {
namespace Layout {

class ReplacedBox;
struct ComputedHorizontalMargin;
struct ComputedVerticalMargin;
class ContainerBox;
struct ContentHeightAndMargin;
struct ContentWidthAndMargin;
struct Edges;
struct HorizontalGeometry;
class LayoutState;
struct OverriddenHorizontalValues;
struct OverriddenVerticalValues;
struct VerticalGeometry;

// This class implements generic positioning and sizing.
class FormattingGeometry {
public:
    FormattingGeometry(const FormattingContext&);

    VerticalGeometry outOfFlowVerticalGeometry(const Box&, const HorizontalConstraints&, const VerticalConstraints&, const OverriddenVerticalValues&) const;
    HorizontalGeometry outOfFlowHorizontalGeometry(const Box&, const HorizontalConstraints&, const VerticalConstraints&, const OverriddenHorizontalValues&);

    ContentHeightAndMargin floatingContentHeightAndMargin(const Box&, const HorizontalConstraints&, const OverriddenVerticalValues&) const;
    ContentWidthAndMargin floatingContentWidthAndMargin(const Box&, const HorizontalConstraints&, const OverriddenHorizontalValues&);

    ContentHeightAndMargin inlineReplacedContentHeightAndMargin(const ReplacedBox&, const HorizontalConstraints&, Optional<VerticalConstraints>, const OverriddenVerticalValues&) const;
    ContentWidthAndMargin inlineReplacedContentWidthAndMargin(const ReplacedBox&, const HorizontalConstraints&, Optional<VerticalConstraints>, const OverriddenHorizontalValues&);

    LayoutSize inFlowPositionedPositionOffset(const Box&, const HorizontalConstraints&) const;

    ContentHeightAndMargin complicatedCases(const Box&, const HorizontalConstraints&, const OverriddenVerticalValues&) const;
    LayoutUnit shrinkToFitWidth(const Box&, LayoutUnit availableWidth);

    Edges computedBorder(const Box&) const;
    Optional<Edges> computedPadding(const Box&, LayoutUnit containingBlockWidth) const;

    ComputedHorizontalMargin computedHorizontalMargin(const Box&, const HorizontalConstraints&) const;
    ComputedVerticalMargin computedVerticalMargin(const Box&, const HorizontalConstraints&) const;

    Optional<LayoutUnit> computedValue(const Length& geometryProperty, LayoutUnit containingBlockWidth) const;
    Optional<LayoutUnit> fixedValue(const Length& geometryProperty) const;

    Optional<LayoutUnit> computedMinHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;
    Optional<LayoutUnit> computedMaxHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;

    Optional<LayoutUnit> computedMinWidth(const Box&, LayoutUnit containingBlockWidth);
    Optional<LayoutUnit> computedMaxWidth(const Box&, LayoutUnit containingBlockWidth);

    FormattingContext::IntrinsicWidthConstraints constrainByMinMaxWidth(const Box&, FormattingContext::IntrinsicWidthConstraints) const;

    LayoutUnit contentHeightForFormattingContextRoot(const ContainerBox&) const;

    FormattingContext::ConstraintsForOutOfFlowContent constraintsForOutOfFlowContent(const ContainerBox&);
    FormattingContext::ConstraintsForInFlowContent constraintsForInFlowContent(const ContainerBox&, Optional<FormattingContext::EscapeReason> = WTF::nullopt);

    Optional<LayoutUnit> computedHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;
    Optional<LayoutUnit> computedWidth(const Box&, LayoutUnit containingBlockWidth);

protected:
    const LayoutState& layoutState() const { return m_formattingContext.layoutState(); }
    LayoutState& layoutState() { return m_formattingContext.layoutState(); }
    const FormattingContext& formattingContext() const { return m_formattingContext; }

private:
    VerticalGeometry outOfFlowReplacedVerticalGeometry(const ReplacedBox&, const HorizontalConstraints&, const VerticalConstraints&, const OverriddenVerticalValues&) const;
    HorizontalGeometry outOfFlowReplacedHorizontalGeometry(const ReplacedBox&, const HorizontalConstraints&, const VerticalConstraints&, const OverriddenHorizontalValues&);

    VerticalGeometry outOfFlowNonReplacedVerticalGeometry(const ContainerBox&, const HorizontalConstraints&, const VerticalConstraints&, const OverriddenVerticalValues&) const;
    HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(const ContainerBox&, const HorizontalConstraints&, const OverriddenHorizontalValues&);

    ContentHeightAndMargin floatingReplacedContentHeightAndMargin(const ReplacedBox&, const HorizontalConstraints&, const OverriddenVerticalValues&) const;
    ContentWidthAndMargin floatingReplacedContentWidthAndMargin(const ReplacedBox&, const HorizontalConstraints&, const OverriddenHorizontalValues&);

    ContentWidthAndMargin floatingNonReplacedContentWidthAndMargin(const Box&, const HorizontalConstraints&, const OverriddenHorizontalValues&);

    LayoutUnit staticVerticalPositionForOutOfFlowPositioned(const Box&, const VerticalConstraints&) const;
    LayoutUnit staticHorizontalPositionForOutOfFlowPositioned(const Box&, const HorizontalConstraints&) const;

    enum class HeightType { Min, Max, Normal };
    Optional<LayoutUnit> computedHeightValue(const Box&, HeightType, Optional<LayoutUnit> containingBlockHeight) const;

    enum class WidthType { Min, Max, Normal };
    Optional<LayoutUnit> computedWidthValue(const Box&, WidthType, LayoutUnit containingBlockWidth);

    const FormattingContext& m_formattingContext;
};

}
}

#endif
