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
#include "FloatingState.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LayoutPoint;
class LayoutUnit;

namespace Layout {

class Box;
class Container;
class FormattingState;
class LayoutContext;

class FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(FormattingContext);
public:
    FormattingContext(const Box& formattingContextRoot);
    virtual ~FormattingContext();

    virtual void layout(LayoutContext&, FormattingState&) const = 0;
    void layoutOutOfFlowDescendants(LayoutContext&, const Box&) const;

    struct InstrinsicWidthConstraints {
        LayoutUnit minimum;
        LayoutUnit maximum;
    };
    virtual InstrinsicWidthConstraints instrinsicWidthConstraints(LayoutContext&, const Box&) const = 0;

    static Display::Box mapBoxToAncestor(const LayoutContext&, const Box&, const Container& ancestor);
    static Position mapTopLeftToAncestor(const LayoutContext&, const Box&, const Container& ancestor);
    static Position mapCoordinateToAncestor(const LayoutContext&, Position, const Container& containingBlock, const Container& ancestor);

protected:
    struct LayoutPair {
        const Box& layoutBox;
        Display::Box& displayBox;
    };
    using LayoutQueue = Vector<std::unique_ptr<LayoutPair>>;

    const Box& root() const { return *m_root; }

    virtual void computeStaticPosition(LayoutContext&, const Box&, Display::Box&) const = 0;
    virtual void computeInFlowPositionedPosition(LayoutContext&, const Box&, Display::Box&) const = 0;

    void computeFloatingHeightAndMargin(LayoutContext&, const Box&, Display::Box&) const;
    void computeFloatingWidthAndMargin(LayoutContext&, const Box&, Display::Box&) const;

    void computeBorderAndPadding(LayoutContext&, const Box&, Display::Box&) const;

    void placeInFlowPositionedChildren(LayoutContext&, const Container&) const;

#ifndef NDEBUG
    virtual void validateGeometryConstraintsAfterLayout(const LayoutContext&) const;
#endif

    // This class implements generic positioning and sizing.
    class Geometry {
    public:
        static VerticalGeometry outOfFlowVerticalGeometry(LayoutContext&, const Box&);
        static HorizontalGeometry outOfFlowHorizontalGeometry(LayoutContext&, const FormattingContext&, const Box&);

        static HeightAndMargin floatingHeightAndMargin(LayoutContext&, const Box&);
        static WidthAndMargin floatingWidthAndMargin(LayoutContext&, const FormattingContext&, const Box&);

        static HeightAndMargin inlineReplacedHeightAndMargin(LayoutContext&, const Box&);
        static WidthAndMargin inlineReplacedWidthAndMargin(LayoutContext&, const Box&, std::optional<LayoutUnit> precomputedMarginLeft = { },
            std::optional<LayoutUnit> precomputedMarginRight = { });

        static HeightAndMargin complicatedCases(LayoutContext&, const Box&);

        static Edges computedBorder(LayoutContext&, const Box&);
        static std::optional<Edges> computedPadding(LayoutContext&, const Box&);

        static HorizontalEdges computedNonCollapsedHorizontalMarginValue(const LayoutContext&, const Box&);
        static VerticalEdges computedNonCollapsedVerticalMarginValue(const LayoutContext&, const Box&);

        static std::optional<LayoutUnit> computedValueIfNotAuto(const Length& geometryProperty, LayoutUnit containingBlockWidth);
        static std::optional<LayoutUnit> fixedValue(const Length& geometryProperty);

    private:
        static VerticalGeometry outOfFlowReplacedVerticalGeometry(LayoutContext&, const Box&);
        static HorizontalGeometry outOfFlowReplacedHorizontalGeometry(LayoutContext&, const Box&);

        static VerticalGeometry outOfFlowNonReplacedVerticalGeometry(LayoutContext&, const Box&);
        static HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(LayoutContext&, const FormattingContext&, const Box&);

        static HeightAndMargin floatingReplacedHeightAndMargin(LayoutContext&, const Box&);
        static WidthAndMargin floatingReplacedWidthAndMargin(LayoutContext&, const Box&);

        static WidthAndMargin floatingNonReplacedWidthAndMargin(LayoutContext&, const FormattingContext&, const Box&);

        static LayoutUnit shrinkToFitWidth(LayoutContext&, const FormattingContext&, const Box&);
    };

private:
    void computeOutOfFlowVerticalGeometry(LayoutContext&, const Box&, Display::Box&) const;
    void computeOutOfFlowHorizontalGeometry(LayoutContext&, const Box&, Display::Box&) const;

    WeakPtr<Box> m_root;
};

}
}
#endif
