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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutContainerBox.h"
#include "LayoutUnit.h"
#include "LayoutUnits.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LayoutSize;
struct Length;

namespace Layout {

class Box;
class BoxGeometry;
class ReplacedBox;
struct ComputedHorizontalMargin;
struct ComputedVerticalMargin;
class ContainerBox;
struct ContentHeightAndMargin;
struct ContentWidthAndMargin;
struct Edges;
class FormattingState;
struct HorizontalGeometry;
class InvalidationState;
class LayoutState;
struct OverrideHorizontalValues;
struct OverrideVerticalValues;
struct VerticalGeometry;

class FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(FormattingContext);
public:
    FormattingContext(const ContainerBox& formattingContextRoot, FormattingState&);
    virtual ~FormattingContext();

    struct ConstraintsForInFlowContent {
        HorizontalConstraints horizontal;
        VerticalConstraints vertical;
    };
    virtual void layoutInFlowContent(InvalidationState&, const ConstraintsForInFlowContent&) = 0;

    struct ConstraintsForOutOfFlowContent {
        HorizontalConstraints horizontal;
        VerticalConstraints vertical;
        // Borders and padding are resolved against the containing block's content box as if the box was an in-flow box.
        LayoutUnit borderAndPaddingConstraints;
    };
    void layoutOutOfFlowContent(InvalidationState&, const ConstraintsForOutOfFlowContent&);

    struct IntrinsicWidthConstraints {
        void expand(LayoutUnit horizontalValue);
        IntrinsicWidthConstraints& operator+=(const IntrinsicWidthConstraints&);
        IntrinsicWidthConstraints& operator+=(LayoutUnit);
        IntrinsicWidthConstraints& operator-=(const IntrinsicWidthConstraints&);
        IntrinsicWidthConstraints& operator-=(LayoutUnit);

        LayoutUnit minimum;
        LayoutUnit maximum;
    };
    virtual IntrinsicWidthConstraints computedIntrinsicWidthConstraints() = 0;

    bool isBlockFormattingContext() const { return root().establishesBlockFormattingContext(); }
    bool isInlineFormattingContext() const { return root().establishesInlineFormattingContext(); }
    bool isTableFormattingContext() const { return root().establishesTableFormattingContext(); }
    bool isTableWrapperBlockFormattingContext() const { return isBlockFormattingContext() && root().isTableWrapperBox(); }
    bool isFlexFormattingContext() const { return root().establishesFlexFormattingContext(); }

    enum class EscapeReason {
        NeedsGeometryFromEstablishedFormattingContext,
        OutOfFlowBoxNeedsInFlowGeometry,
        FloatBoxNeedsToBeInAbsoluteCoordinates,
        FindFixedHeightAncestorQuirk,
        DocumentBoxStretchesToViewportQuirk,
        BodyStretchesToViewportQuirk,
        StrokeOverflowNeedsViewportGeometry,
        TableNeedsAccessToTableWrapper
    };
    const BoxGeometry& geometryForBox(const Box&, Optional<EscapeReason> = WTF::nullopt) const;
    const ContainerBox& root() const { return *m_root; }

    LayoutState& layoutState() const;

protected:
    using LayoutQueue = Vector<const Box*>;

    const FormattingState& formattingState() const { return m_formattingState; }
    FormattingState& formattingState() { return m_formattingState; }

    void computeBorderAndPadding(const Box&, const HorizontalConstraints&);

#ifndef NDEBUG
    virtual void validateGeometryConstraintsAfterLayout() const;
#endif

    // This class implements generic positioning and sizing.
    class Geometry {
    public:
        VerticalGeometry outOfFlowVerticalGeometry(const Box&, const HorizontalConstraints&, const VerticalConstraints&, const OverrideVerticalValues&) const;
        HorizontalGeometry outOfFlowHorizontalGeometry(const Box&, const HorizontalConstraints&, const VerticalConstraints&, const OverrideHorizontalValues&);

        ContentHeightAndMargin floatingHeightAndMargin(const Box&, const HorizontalConstraints&, const OverrideVerticalValues&) const;
        ContentWidthAndMargin floatingWidthAndMargin(const Box&, const HorizontalConstraints&, const OverrideHorizontalValues&);

        ContentHeightAndMargin inlineReplacedHeightAndMargin(const ReplacedBox&, const HorizontalConstraints&, Optional<VerticalConstraints>, const OverrideVerticalValues&) const;
        ContentWidthAndMargin inlineReplacedWidthAndMargin(const ReplacedBox&, const HorizontalConstraints&, Optional<VerticalConstraints>, const OverrideHorizontalValues&);

        LayoutSize inFlowPositionedPositionOffset(const Box&, const HorizontalConstraints&) const;

        ContentHeightAndMargin complicatedCases(const Box&, const HorizontalConstraints&, const OverrideVerticalValues&) const;
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

        FormattingContext::IntrinsicWidthConstraints constrainByMinMaxWidth(const Box&, IntrinsicWidthConstraints) const;

        LayoutUnit contentHeightForFormattingContextRoot(const Box&) const;

        ConstraintsForOutOfFlowContent constraintsForOutOfFlowContent(const ContainerBox&);
        ConstraintsForInFlowContent constraintsForInFlowContent(const ContainerBox&, Optional<EscapeReason> = WTF::nullopt);

        Optional<LayoutUnit> computedHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;
        Optional<LayoutUnit> computedWidth(const Box&, LayoutUnit containingBlockWidth);

    protected:
        friend class FormattingContext;
        Geometry(const FormattingContext&);

        const LayoutState& layoutState() const { return m_formattingContext.layoutState(); }
        LayoutState& layoutState() { return m_formattingContext.layoutState(); }
        const FormattingContext& formattingContext() const { return m_formattingContext; }

    private:
        VerticalGeometry outOfFlowReplacedVerticalGeometry(const ReplacedBox&, const HorizontalConstraints&, const VerticalConstraints&, const OverrideVerticalValues&) const;
        HorizontalGeometry outOfFlowReplacedHorizontalGeometry(const ReplacedBox&, const HorizontalConstraints&, const VerticalConstraints&, const OverrideHorizontalValues&);

        VerticalGeometry outOfFlowNonReplacedVerticalGeometry(const Box&, const HorizontalConstraints&, const VerticalConstraints&, const OverrideVerticalValues&) const;
        HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(const Box&, const HorizontalConstraints&, const OverrideHorizontalValues&);

        ContentHeightAndMargin floatingReplacedHeightAndMargin(const ReplacedBox&, const HorizontalConstraints&, const OverrideVerticalValues&) const;
        ContentWidthAndMargin floatingReplacedWidthAndMargin(const ReplacedBox&, const HorizontalConstraints&, const OverrideHorizontalValues&);

        ContentWidthAndMargin floatingNonReplacedWidthAndMargin(const Box&, const HorizontalConstraints&, const OverrideHorizontalValues&);

        LayoutUnit staticVerticalPositionForOutOfFlowPositioned(const Box&, const VerticalConstraints&) const;
        LayoutUnit staticHorizontalPositionForOutOfFlowPositioned(const Box&, const HorizontalConstraints&) const;

        enum class HeightType { Min, Max, Normal };
        Optional<LayoutUnit> computedHeightValue(const Box&, HeightType, Optional<LayoutUnit> containingBlockHeight) const;

        enum class WidthType { Min, Max, Normal };
        Optional<LayoutUnit> computedWidthValue(const Box&, WidthType, LayoutUnit containingBlockWidth);

        const FormattingContext& m_formattingContext;
    };
    FormattingContext::Geometry geometry() const { return Geometry(*this); }

    class Quirks {
    public:
        Quirks(const FormattingContext&);

        LayoutUnit heightValueOfNearestContainingBlockWithFixedHeight(const Box&);

    protected:
        const LayoutState& layoutState() const { return m_formattingContext.layoutState(); }
        LayoutState& layoutState() { return m_formattingContext.layoutState(); }
        const FormattingContext& formattingContext() const { return m_formattingContext; }

        const FormattingContext& m_formattingContext;
    };
    FormattingContext::Quirks quirks() const { return Quirks(*this); }

private:
    void collectOutOfFlowDescendantsIfNeeded();
    void computeOutOfFlowVerticalGeometry(const Box&, const ConstraintsForOutOfFlowContent&);
    void computeOutOfFlowHorizontalGeometry(const Box&, const ConstraintsForOutOfFlowContent&);

    WeakPtr<const ContainerBox> m_root;
    FormattingState& m_formattingState;
};

inline FormattingContext::Geometry::Geometry(const FormattingContext& formattingContext)
    : m_formattingContext(formattingContext)
{
}

inline FormattingContext::Quirks::Quirks(const FormattingContext& formattingContext)
    : m_formattingContext(formattingContext)
{
}

inline void FormattingContext::IntrinsicWidthConstraints::expand(LayoutUnit horizontalValue)
{
    minimum += horizontalValue;
    maximum += horizontalValue;
}

inline FormattingContext::IntrinsicWidthConstraints& FormattingContext::IntrinsicWidthConstraints::operator+=(const IntrinsicWidthConstraints& other)
{
    minimum += other.minimum;
    maximum += other.maximum;
    return *this;
}

inline FormattingContext::IntrinsicWidthConstraints& FormattingContext::IntrinsicWidthConstraints::operator+=(LayoutUnit value)
{
    expand(value);
    return *this;
}

inline FormattingContext::IntrinsicWidthConstraints& FormattingContext::IntrinsicWidthConstraints::operator-=(const IntrinsicWidthConstraints& other)
{
    minimum -= other.minimum;
    maximum -= other.maximum;
    return *this;
}

inline FormattingContext::IntrinsicWidthConstraints& FormattingContext::IntrinsicWidthConstraints::operator-=(LayoutUnit value)
{
    expand(-value);
    return *this;
}

}
}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::FormattingContext& formattingContext) { return formattingContext.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
