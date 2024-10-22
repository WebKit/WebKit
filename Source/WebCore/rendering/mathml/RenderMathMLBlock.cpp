/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2012 David Barton (dbarton@mathscribe.com). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMathMLBlock.h"

#if ENABLE(MATHML)

#include "CSSUnits.h"
#include "GraphicsContext.h"
#include "LayoutRepainter.h"
#include "MathMLElement.h"
#include "MathMLNames.h"
#include "MathMLPresentationElement.h"
#include "RenderBoxInlines.h"
#include "RenderTableInlines.h"
#include "RenderView.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderMathMLBlock);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderMathMLTable);

RenderMathMLBlock::RenderMathMLBlock(Type type, MathMLPresentationElement& container, RenderStyle&& style)
    : RenderBlock(type, container, WTFMove(style), { })
    , m_mathMLStyle(MathMLStyle::create())
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderMathMLBlock::RenderMathMLBlock(Type type, Document& document, RenderStyle&& style)
    : RenderBlock(type, document, WTFMove(style), { })
    , m_mathMLStyle(MathMLStyle::create())
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderMathMLBlock::~RenderMathMLBlock() = default;

bool RenderMathMLBlock::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return is<Element>(child.node());
}

static LayoutUnit axisHeight(const RenderStyle& style)
{
    // If we have a MATH table we just return the AxisHeight constant.
    const Ref primaryFont = style.fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData())
        return LayoutUnit(mathData->getMathConstant(primaryFont, OpenTypeMathData::AxisHeight));

    // Otherwise, the idea is to try and use the middle of operators as the math axis which we thus approximate by "half of the x-height".
    // Note that Gecko has a slower but more accurate version that measures half of the height of U+2212 MINUS SIGN.
    return LayoutUnit(style.metricsOfPrimaryFont().xHeight().value_or(0) / 2);
}

LayoutUnit RenderMathMLBlock::mathAxisHeight() const
{
    return axisHeight(style());
}

LayoutUnit RenderMathMLBlock::mirrorIfNeeded(LayoutUnit horizontalOffset, LayoutUnit boxWidth) const
{
    if (writingMode().isBidiRTL())
        return logicalWidth() - boxWidth - horizontalOffset;

    return horizontalOffset;
}

LayoutUnit RenderMathMLBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    if (linePositionMode == PositionOfInteriorLineBoxes)
        return 0;

    return firstLineBaseline().value_or(RenderBlock::baselinePosition(baselineType, firstLine, direction, linePositionMode));
}

LayoutUnit toUserUnits(const MathMLElement::Length& length, const RenderStyle& style, const LayoutUnit& referenceValue)
{
    switch (length.type) {
    // Zoom for physical units needs to be accounted for.
    case MathMLElement::LengthType::Cm:
        return LayoutUnit(style.usedZoom() * length.value * static_cast<float>(CSS::pixelsPerCm));
    case MathMLElement::LengthType::In:
        return LayoutUnit(style.usedZoom() * length.value * static_cast<float>(CSS::pixelsPerInch));
    case MathMLElement::LengthType::Mm:
        return LayoutUnit(style.usedZoom() * length.value * static_cast<float>(CSS::pixelsPerMm));
    case MathMLElement::LengthType::Pc:
        return LayoutUnit(style.usedZoom() * length.value * static_cast<float>(CSS::pixelsPerPc));
    case MathMLElement::LengthType::Pt:
        return LayoutUnit(style.usedZoom() * length.value * static_cast<float>(CSS::pixelsPerPt));
    case MathMLElement::LengthType::Px:
        return LayoutUnit(style.usedZoom() * length.value);

    // Zoom for logical units is accounted for either in the font info or referenceValue.
    case MathMLElement::LengthType::Em:
        return LayoutUnit(length.value * style.fontCascade().size());
    case MathMLElement::LengthType::Ex:
        return LayoutUnit(length.value * style.metricsOfPrimaryFont().xHeight().value_or(0));
    case MathMLElement::LengthType::MathUnit:
        return LayoutUnit(length.value * style.fontCascade().size() / 18);
    case MathMLElement::LengthType::Percentage:
        return LayoutUnit(referenceValue * length.value / 100);
    case MathMLElement::LengthType::UnitLess:
        return LayoutUnit(referenceValue * length.value);
    case MathMLElement::LengthType::ParsingFailed:
        return referenceValue;
    default:
        ASSERT_NOT_REACHED();
        return referenceValue;
    }
}

RenderMathMLTable::~RenderMathMLTable() = default;

std::optional<LayoutUnit> RenderMathMLTable::firstLineBaseline() const
{
    // By default the vertical center of <mtable> is aligned on the math axis.
    // This is different than RenderTable::firstLineBoxBaseline, which returns the baseline of the first row of a <table>.
    return LayoutUnit { (logicalHeight() / 2 + axisHeight(style())).toInt() };
}

void RenderMathMLBlock::layoutItems(bool relayoutChildren)
{
    LayoutUnit verticalOffset = borderAndPaddingBefore();
    LayoutUnit horizontalOffset = borderAndPaddingStart();

    LayoutUnit preferredHorizontalExtent;
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        LayoutUnit childHorizontalExtent = child->maxPreferredLogicalWidth() - child->horizontalBorderAndPaddingExtent();
        LayoutUnit childHorizontalMarginBoxExtent = child->horizontalBorderAndPaddingExtent() + childHorizontalExtent;
        childHorizontalMarginBoxExtent += child->horizontalMarginExtent();

        preferredHorizontalExtent += childHorizontalMarginBoxExtent;
    }

    LayoutUnit currentHorizontalExtent = contentLogicalWidth();
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        auto everHadLayout = child->everHadLayout();
        LayoutUnit childSize = child->maxPreferredLogicalWidth() - child->horizontalBorderAndPaddingExtent();

        if (preferredHorizontalExtent > currentHorizontalExtent)
            childSize = currentHorizontalExtent;

        LayoutUnit childPreferredSize = childSize + child->horizontalBorderAndPaddingExtent();

        if (childPreferredSize != child->width())
            child->setChildNeedsLayout(MarkOnlyThis);

        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *child);
        child->layoutIfNeeded();

        LayoutUnit childVerticalMarginBoxExtent;
        childVerticalMarginBoxExtent = child->height() + child->verticalMarginExtent();

        setLogicalHeight(std::max(logicalHeight(), verticalOffset + borderAndPaddingAfter() + childVerticalMarginBoxExtent + horizontalScrollbarHeight()));

        horizontalOffset += child->marginStart();

        LayoutUnit childHorizontalExtent = child->width();
        LayoutPoint childLocation(writingMode().isBidiLTR() ? horizontalOffset : width() - horizontalOffset - childHorizontalExtent,
            verticalOffset + child->marginBefore());

        child->setLocation(childLocation);
        horizontalOffset += childHorizontalExtent + child->marginEnd();
        if (!everHadLayout && child->checkForRepaintDuringLayout())
            child->repaint();
    }
}

void RenderMathMLBlock::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (!relayoutChildren && simplifiedLayout())
        return;

    layoutFloatingChildren();

    LayoutRepainter repainter(*this);

    if (recomputeLogicalWidth())
        relayoutChildren = true;

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    layoutItems(relayoutChildren);

    updateLogicalHeight();

    layoutPositionedObjects(relayoutChildren);

    repainter.repaintAfterLayout();

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

void RenderMathMLBlock::computeAndSetBlockDirectionMarginsOfChildren()
{
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox())
        child->computeAndSetBlockDirectionMargins(*this);
}

void RenderMathMLBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    // MathML displaystyle changes can affect layout.
    if (oldStyle && style().mathStyle() != oldStyle->mathStyle())
        setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLBlock::insertPositionedChildrenIntoContainingBlock()
{
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            child->containingBlock()->insertPositionedObject(*child);
    }
}

void RenderMathMLBlock::layoutFloatingChildren()
{
    // According to the spec, https://w3c.github.io/mathml-core/#css-styling:
    // > The float property does not create floating of elements whose parent's computed display value is block math or inline math,
    // > and does not take them out-of-flow.
    // However, WebKit does not currently do this since `display: math` is unimplemented. See webkit.org/b/278533.
    // Since this leaves floats as neither positioned nor in-flow, perform dummy layout for floating children.
    // FIXME: Per the spec, there should be no floating children inside MathML renderers.
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isFloating())
            child->layoutIfNeeded();
    }
}

void RenderMathMLBlock::shiftInFlowChildren(LayoutUnit left, LayoutUnit top)
{
    LayoutPoint shift(left, top);
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox())
        child->setLocation(child->location() + shift);
}

void RenderMathMLBlock::adjustPreferredLogicalWidthsForBorderAndPadding()
{
    ASSERT(preferredLogicalWidthsDirty());
    m_minPreferredLogicalWidth += borderAndPaddingLogicalWidth();
    m_maxPreferredLogicalWidth += borderAndPaddingLogicalWidth();
}

void RenderMathMLBlock::adjustLayoutForBorderAndPadding()
{
    setLogicalWidth(logicalWidth() + borderAndPaddingLogicalWidth());
    setLogicalHeight(logicalHeight() + borderAndPaddingLogicalHeight());
    shiftInFlowChildren(style().isLeftToRightDirection() ? borderAndPaddingStart() : borderAndPaddingEnd(), borderAndPaddingBefore());
}

}

#endif
