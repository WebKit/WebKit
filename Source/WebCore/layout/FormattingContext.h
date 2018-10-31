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

    virtual void layout(LayoutState&, FormattingState&) const = 0;
    void layoutOutOfFlowDescendants(LayoutState&, const Box&) const;

    struct InstrinsicWidthConstraints {
        LayoutUnit minimum;
        LayoutUnit maximum;
    };
    virtual InstrinsicWidthConstraints instrinsicWidthConstraints(LayoutState&, const Box&) const = 0;

    static Display::Box mapBoxToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static Position mapTopLeftToAncestor(const LayoutState&, const Box&, const Container& ancestor);
    static Position mapCoordinateToAncestor(const LayoutState&, Position, const Container& containingBlock, const Container& ancestor);

protected:
    using LayoutQueue = Vector<const Box*>;

    FormattingState& formattingState() const;
    LayoutState& layoutState() const;
    const Box& root() const { return *m_root; }

    virtual void computeStaticPosition(const LayoutState&, const Box&) const = 0;
    virtual void computeInFlowPositionedPosition(const LayoutState&, const Box&) const = 0;

    void computeBorderAndPadding(const LayoutState&, const Box&) const;

    void placeInFlowPositionedChildren(const LayoutState&, const Container&) const;

#ifndef NDEBUG
    virtual void validateGeometryConstraintsAfterLayout(const LayoutState&) const;
#endif

    // This class implements generic positioning and sizing.
    class Geometry {
    public:
        static VerticalGeometry outOfFlowVerticalGeometry(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static HorizontalGeometry outOfFlowHorizontalGeometry(LayoutState&, const FormattingContext&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static HeightAndMargin floatingHeightAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static WidthAndMargin floatingWidthAndMargin(LayoutState&, const FormattingContext&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static HeightAndMargin inlineReplacedHeightAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static WidthAndMargin inlineReplacedWidthAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedWidth = { }, 
            std::optional<LayoutUnit> precomputedMarginLeft = { }, std::optional<LayoutUnit> precomputedMarginRight = { });

        static HeightAndMargin complicatedCases(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });

        static Edges computedBorder(const LayoutState&, const Box&);
        static std::optional<Edges> computedPadding(const LayoutState&, const Box&);

        static HorizontalEdges computedNonCollapsedHorizontalMarginValue(const LayoutState&, const Box&);
        static VerticalEdges computedNonCollapsedVerticalMarginValue(const LayoutState&, const Box&);

        static std::optional<LayoutUnit> computedValueIfNotAuto(const Length& geometryProperty, LayoutUnit containingBlockWidth);
        static std::optional<LayoutUnit> fixedValue(const Length& geometryProperty);

        static std::optional<LayoutUnit> computedMinHeight(const LayoutState&, const Box&);
        static std::optional<LayoutUnit> computedMaxHeight(const LayoutState&, const Box&);

    private:
        static VerticalGeometry outOfFlowReplacedVerticalGeometry(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static HorizontalGeometry outOfFlowReplacedHorizontalGeometry(const LayoutState&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static VerticalGeometry outOfFlowNonReplacedVerticalGeometry(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(LayoutState&, const FormattingContext&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static HeightAndMargin floatingReplacedHeightAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static WidthAndMargin floatingReplacedWidthAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static WidthAndMargin floatingNonReplacedWidthAndMargin(LayoutState&, const FormattingContext&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static LayoutUnit shrinkToFitWidth(LayoutState&, const FormattingContext&, const Box&);
    };

private:
    void computeOutOfFlowVerticalGeometry(const LayoutState&, const Box&) const;
    void computeOutOfFlowHorizontalGeometry(LayoutState&, const Box&) const;

    WeakPtr<const Box> m_root;
    FormattingState& m_formattingState;
};

}
}
#endif
