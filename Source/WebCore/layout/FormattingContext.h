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

    virtual void layout() const = 0;
    void layoutOutOfFlowDescendants(const Box&) const;

    struct IntrinsicWidthConstraints {
        LayoutUnit minimum;
        LayoutUnit maximum;
    };
    virtual void computeIntrinsicWidthConstraints() const = 0;

    static Display::Box mapBoxToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static LayoutUnit mapTopToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static Point mapCoordinateToAncestor(const LayoutState&, Point, const Container& containingBlock, const Container& ancestor);

protected:
    using LayoutQueue = Vector<const Box*>;

    LayoutState& layoutState() const;
    FormattingState& formattingState() const { return m_formattingState; }
    const Box& root() const { return *m_root; }

    void computeBorderAndPadding(const Box&) const;

#ifndef NDEBUG
    virtual void validateGeometryConstraintsAfterLayout() const;
#endif

    // This class implements generic positioning and sizing.
    class Geometry {
    public:
        static VerticalGeometry outOfFlowVerticalGeometry(const LayoutState&, const Box&, UsedVerticalValues);
        static HorizontalGeometry outOfFlowHorizontalGeometry(LayoutState&, const Box&, UsedHorizontalValues);

        static HeightAndMargin floatingHeightAndMargin(const LayoutState&, const Box&, UsedVerticalValues);
        static WidthAndMargin floatingWidthAndMargin(LayoutState&, const Box&, UsedHorizontalValues);

        static HeightAndMargin inlineReplacedHeightAndMargin(const LayoutState&, const Box&, UsedVerticalValues);
        static WidthAndMargin inlineReplacedWidthAndMargin(const LayoutState&, const Box&, UsedHorizontalValues);

        static LayoutSize inFlowPositionedPositionOffset(const LayoutState&, const Box&);

        static HeightAndMargin complicatedCases(const LayoutState&, const Box&, UsedVerticalValues);
        static LayoutUnit shrinkToFitWidth(LayoutState&, const Box&, UsedHorizontalValues);

        static Edges computedBorder(const Box&);
        static Optional<Edges> computedPadding(const Box&, UsedHorizontalValues);

        static ComputedHorizontalMargin computedHorizontalMargin(const Box&, UsedHorizontalValues);
        static ComputedVerticalMargin computedVerticalMargin(const LayoutState&, const Box&);

        static Optional<LayoutUnit> computedValueIfNotAuto(const Length& geometryProperty, LayoutUnit containingBlockWidth);
        static Optional<LayoutUnit> fixedValue(const Length& geometryProperty);

        static Optional<LayoutUnit> computedMinHeight(const LayoutState&, const Box&);
        static Optional<LayoutUnit> computedMaxHeight(const LayoutState&, const Box&);

    protected:
        enum class HeightType { Min, Max, Normal };
        static Optional<LayoutUnit> computedHeightValue(const LayoutState&, const Box&, HeightType);

    private:
        static VerticalGeometry outOfFlowReplacedVerticalGeometry(const LayoutState&, const Box&, UsedVerticalValues);
        static HorizontalGeometry outOfFlowReplacedHorizontalGeometry(const LayoutState&, const Box&, UsedHorizontalValues);

        static VerticalGeometry outOfFlowNonReplacedVerticalGeometry(const LayoutState&, const Box&, UsedVerticalValues);
        static HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(LayoutState&, const Box&, UsedHorizontalValues);

        static HeightAndMargin floatingReplacedHeightAndMargin(const LayoutState&, const Box&, UsedVerticalValues);
        static WidthAndMargin floatingReplacedWidthAndMargin(const LayoutState&, const Box&, UsedHorizontalValues);

        static WidthAndMargin floatingNonReplacedWidthAndMargin(LayoutState&, const Box&, UsedHorizontalValues);
    };

    class Quirks {
    public:
        static LayoutUnit heightValueOfNearestContainingBlockWithFixedHeight(const LayoutState&, const Box&);
    };

private:
    void computeOutOfFlowVerticalGeometry(const Box&) const;
    void computeOutOfFlowHorizontalGeometry(const Box&) const;

    WeakPtr<const Box> m_root;
    FormattingState& m_formattingState;
};

}
}
#endif
