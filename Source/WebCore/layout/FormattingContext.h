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

#include "DisplayBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LayoutPoint;
class LayoutUnit;

namespace Layout {

class Box;
class Container;
class FormattingState;
class LayoutState;

class FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(FormattingContext);
public:
    FormattingContext(const Box& formattingContextRoot, FormattingState&);
    virtual ~FormattingContext();

    virtual void layout() = 0;
    void layoutOutOfFlowContent();

    struct IntrinsicWidthConstraints {
        void expand(LayoutUnit horizontalValue);
        IntrinsicWidthConstraints& operator+=(const IntrinsicWidthConstraints&);

        LayoutUnit minimum;
        LayoutUnit maximum;
    };
    virtual IntrinsicWidthConstraints computedIntrinsicWidthConstraints() = 0;

    static Display::Box mapBoxToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static LayoutUnit mapTopToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static LayoutUnit mapLeftToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static LayoutUnit mapRightToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static Point mapPointToAncestor(const LayoutState&, Point, const Container& from, const Container& to);
    static Point mapPointToDescendent(const LayoutState&, Point, const Container& from, const Container& to);

protected:
    using LayoutQueue = Vector<const Box*>;

    LayoutState& layoutState() const;
    FormattingState& formattingState() const { return m_formattingState; }
    const Box& root() const { return *m_root; }

    void computeBorderAndPadding(const Box&, Optional<UsedHorizontalValues> = WTF::nullopt);

#ifndef NDEBUG
    virtual void validateGeometryConstraintsAfterLayout() const;
#endif

    // This class implements generic positioning and sizing.
    class Geometry {
    public:
        Geometry(LayoutState&);

        VerticalGeometry outOfFlowVerticalGeometry(const Box&, UsedVerticalValues) const;
        HorizontalGeometry outOfFlowHorizontalGeometry(const Box&, UsedHorizontalValues);

        HeightAndMargin floatingHeightAndMargin(const Box&, UsedVerticalValues, UsedHorizontalValues) const;
        WidthAndMargin floatingWidthAndMargin(const Box&, UsedHorizontalValues);

        HeightAndMargin inlineReplacedHeightAndMargin(const Box&, UsedVerticalValues) const;
        WidthAndMargin inlineReplacedWidthAndMargin(const Box&, UsedHorizontalValues) const;

        LayoutSize inFlowPositionedPositionOffset(const Box&) const;

        HeightAndMargin complicatedCases(const Box&, UsedVerticalValues, UsedHorizontalValues) const;
        LayoutUnit shrinkToFitWidth(const Box&, UsedHorizontalValues);

        Edges computedBorder(const Box&) const;
        Optional<Edges> computedPadding(const Box&, UsedHorizontalValues) const;

        ComputedHorizontalMargin computedHorizontalMargin(const Box&, UsedHorizontalValues) const;
        ComputedVerticalMargin computedVerticalMargin(const Box&, UsedHorizontalValues) const;

        Optional<LayoutUnit> computedValueIfNotAuto(const Length& geometryProperty, LayoutUnit containingBlockWidth) const;
        Optional<LayoutUnit> fixedValue(const Length& geometryProperty) const;

        Optional<LayoutUnit> computedMinHeight(const Box&) const;
        Optional<LayoutUnit> computedMaxHeight(const Box&) const;

        FormattingContext::IntrinsicWidthConstraints constrainByMinMaxWidth(const Box&, IntrinsicWidthConstraints) const;

        LayoutUnit contentHeightForFormattingContextRoot(const Box&) const;

    protected:
        enum class HeightType { Min, Max, Normal };
        Optional<LayoutUnit> computedHeightValue(const Box&, HeightType) const;

        const LayoutState& layoutState() const { return m_layoutState; }
        LayoutState& layoutState() { return m_layoutState; }

    private:
        VerticalGeometry outOfFlowReplacedVerticalGeometry(const Box&, UsedVerticalValues) const;
        HorizontalGeometry outOfFlowReplacedHorizontalGeometry(const Box&, UsedHorizontalValues) const;

        VerticalGeometry outOfFlowNonReplacedVerticalGeometry(const Box&, UsedVerticalValues) const;
        HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(const Box&, UsedHorizontalValues);

        HeightAndMargin floatingReplacedHeightAndMargin(const Box&, UsedVerticalValues) const;
        WidthAndMargin floatingReplacedWidthAndMargin(const Box&, UsedHorizontalValues) const;

        WidthAndMargin floatingNonReplacedWidthAndMargin(const Box&, UsedHorizontalValues);

        LayoutUnit staticVerticalPositionForOutOfFlowPositioned(const Box&) const;
        LayoutUnit staticHorizontalPositionForOutOfFlowPositioned(const Box&) const;

        LayoutState& m_layoutState;
    };
    FormattingContext::Geometry geometry() const { return Geometry(layoutState()); }

    class Quirks {
    public:
        Quirks(LayoutState&);

        LayoutUnit heightValueOfNearestContainingBlockWithFixedHeight(const Box&);

    protected:
        const LayoutState& layoutState() const { return m_layoutState; }
        LayoutState& layoutState() { return m_layoutState; }

        LayoutState& m_layoutState;
    };
    FormattingContext::Quirks quirks() const { return Quirks(layoutState()); }

private:
    void computeOutOfFlowVerticalGeometry(const Box&);
    void computeOutOfFlowHorizontalGeometry(const Box&);

    WeakPtr<const Box> m_root;
    FormattingState& m_formattingState;
};

inline FormattingContext::Geometry::Geometry(LayoutState& layoutState)
    : m_layoutState(layoutState)
{
}

inline FormattingContext::Quirks::Quirks(LayoutState& layoutState)
    : m_layoutState(layoutState)
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

}
}
#endif
