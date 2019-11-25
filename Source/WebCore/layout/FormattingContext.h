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

#include "LayoutContainer.h"
#include "LayoutUnit.h"
#include "LayoutUnits.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Display {
class Box;
}
class LayoutSize;
struct Length;

namespace Layout {

class Box;
struct ComputedHorizontalMargin;
struct ComputedVerticalMargin;
class Container;
struct ContentHeightAndMargin;
struct ContentWidthAndMargin;
struct Edges;
class FormattingState;
struct HorizontalGeometry;
class InvalidationState;
class LayoutState;
struct UsedHorizontalValues;
struct UsedVerticalValues;
struct VerticalGeometry;

class FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(FormattingContext);
public:
    FormattingContext(const Container& formattingContextRoot, FormattingState&);
    virtual ~FormattingContext();

    virtual void layoutInFlowContent(InvalidationState&) = 0;
    void layoutOutOfFlowContent(InvalidationState&);

    struct IntrinsicWidthConstraints {
        void expand(LayoutUnit horizontalValue);
        IntrinsicWidthConstraints& operator+=(const IntrinsicWidthConstraints&);

        LayoutUnit minimum;
        LayoutUnit maximum;
    };
    virtual IntrinsicWidthConstraints computedIntrinsicWidthConstraints() = 0;

    LayoutUnit mapTopToFormattingContextRoot(const Box&) const;
    LayoutUnit mapLeftToFormattingContextRoot(const Box&) const;
    LayoutUnit mapRightToFormattingContextRoot(const Box&) const;

    bool isBlockFormattingContext() const { return root().establishesBlockFormattingContext(); }
    bool isInlineFormattingContext() const { return root().establishesInlineFormattingContext(); }
    bool isTableFormattingContext() const { return root().establishesTableFormattingContext(); }

    enum class EscapeType {
        AccessChildFormattingContext,
        AccessParentFormattingContext,
        AccessAncestorFormattingContext,
        TableFormattingContextAccessParentTableWrapperBlockFormattingContext
    };
    const Display::Box& geometryForBox(const Box&, Optional<EscapeType> = WTF::nullopt) const;

protected:
    using LayoutQueue = Vector<const Box*>;

    const Container& root() const { return *m_root; }
    LayoutState& layoutState() const;
    const FormattingState& formattingState() const { return m_formattingState; }
    FormattingState& formattingState() { return m_formattingState; }

    void computeBorderAndPadding(const Box&, Optional<UsedHorizontalValues> = WTF::nullopt);

#ifndef NDEBUG
    virtual void validateGeometryConstraintsAfterLayout() const;
#endif

    // This class implements generic positioning and sizing.
    class Geometry {
    public:
        VerticalGeometry outOfFlowVerticalGeometry(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        HorizontalGeometry outOfFlowHorizontalGeometry(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&);

        ContentHeightAndMargin floatingHeightAndMargin(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        ContentWidthAndMargin floatingWidthAndMargin(const Box&, const UsedHorizontalValues&);

        ContentHeightAndMargin inlineReplacedHeightAndMargin(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        ContentWidthAndMargin inlineReplacedWidthAndMargin(const Box&, const UsedHorizontalValues&, Optional<UsedVerticalValues> = WTF::nullopt) const;

        LayoutSize inFlowPositionedPositionOffset(const Box&, const UsedHorizontalValues&) const;

        ContentHeightAndMargin complicatedCases(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        LayoutUnit shrinkToFitWidth(const Box&, LayoutUnit availableWidth);

        Edges computedBorder(const Box&) const;
        Optional<Edges> computedPadding(const Box&, const UsedHorizontalValues&) const;

        ComputedHorizontalMargin computedHorizontalMargin(const Box&, const UsedHorizontalValues&) const;
        ComputedVerticalMargin computedVerticalMargin(const Box&, const UsedHorizontalValues&) const;

        Optional<LayoutUnit> computedValueIfNotAuto(const Length& geometryProperty, LayoutUnit containingBlockWidth) const;
        Optional<LayoutUnit> fixedValue(const Length& geometryProperty) const;

        Optional<LayoutUnit> computedMinHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;
        Optional<LayoutUnit> computedMaxHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;

        Optional<LayoutUnit> computedMinWidth(const Box&, LayoutUnit containingBlockWidth) const;
        Optional<LayoutUnit> computedMaxWidth(const Box&, LayoutUnit containingBlockWidth) const;

        FormattingContext::IntrinsicWidthConstraints constrainByMinMaxWidth(const Box&, IntrinsicWidthConstraints) const;

        LayoutUnit contentHeightForFormattingContextRoot(const Box&) const;

    protected:
        friend class FormattingContext;
        Geometry(const FormattingContext&);

        enum class HeightType { Min, Max, Normal };
        Optional<LayoutUnit> computedHeightValue(const Box&, HeightType, Optional<LayoutUnit> containingBlockHeight) const;
        Optional<LayoutUnit> computedContentHeight(const Box&, Optional<LayoutUnit> containingBlockHeight = WTF::nullopt) const;
        Optional<LayoutUnit> computedContentWidth(const Box&, LayoutUnit containingBlockWidth) const;

        const LayoutState& layoutState() const { return m_formattingContext.layoutState(); }
        LayoutState& layoutState() { return m_formattingContext.layoutState(); }
        const FormattingContext& formattingContext() const { return m_formattingContext; }

    private:
        VerticalGeometry outOfFlowReplacedVerticalGeometry(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        HorizontalGeometry outOfFlowReplacedHorizontalGeometry(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;

        VerticalGeometry outOfFlowNonReplacedVerticalGeometry(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        HorizontalGeometry outOfFlowNonReplacedHorizontalGeometry(const Box&, const UsedHorizontalValues&);

        ContentHeightAndMargin floatingReplacedHeightAndMargin(const Box&, const UsedHorizontalValues&, const UsedVerticalValues&) const;
        ContentWidthAndMargin floatingReplacedWidthAndMargin(const Box&, const UsedHorizontalValues&) const;

        ContentWidthAndMargin floatingNonReplacedWidthAndMargin(const Box&, const UsedHorizontalValues&);

        LayoutUnit staticVerticalPositionForOutOfFlowPositioned(const Box&, const UsedVerticalValues&) const;
        LayoutUnit staticHorizontalPositionForOutOfFlowPositioned(const Box&, const UsedHorizontalValues&) const;

        const FormattingContext& m_formattingContext;
    };
    FormattingContext::Geometry geometry() const { return Geometry(*this); }

    class Quirks {
    public:
        LayoutUnit heightValueOfNearestContainingBlockWithFixedHeight(const Box&);

    protected:
        friend class FormattingContext;
        Quirks(const FormattingContext&);

        const LayoutState& layoutState() const { return m_formattingContext.layoutState(); }
        LayoutState& layoutState() { return m_formattingContext.layoutState(); }
        const FormattingContext& formattingContext() const { return m_formattingContext; }

        const FormattingContext& m_formattingContext;
    };
    FormattingContext::Quirks quirks() const { return Quirks(*this); }

private:
    void collectOutOfFlowDescendantsIfNeeded();
    void computeOutOfFlowVerticalGeometry(const Box&);
    void computeOutOfFlowHorizontalGeometry(const Box&);

    WeakPtr<const Container> m_root;
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

}
}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::FormattingContext& formattingContext) { return formattingContext.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
