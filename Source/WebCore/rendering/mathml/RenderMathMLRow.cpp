/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All right reserved.
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
#include "RenderMathMLRow.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "MathMLRowElement.h"
#include "RenderIterator.h"
#include "RenderMathMLBlockInlines.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLRoot.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLRow);

RenderMathMLRow::RenderMathMLRow(Type type, MathMLRowElement& element, RenderStyle&& style)
    : RenderMathMLBlock(type, element, WTFMove(style))
{
    ASSERT(isRenderMathMLRow());
}

MathMLRowElement& RenderMathMLRow::element() const
{
    return static_cast<MathMLRowElement&>(nodeForNonAnonymous());
}

std::optional<LayoutUnit> RenderMathMLRow::firstLineBaseline() const
{
    auto* baselineChild = firstChildBox();
    if (!baselineChild)
        return std::optional<LayoutUnit>();

    return LayoutUnit { static_cast<int>(lroundf(ascentForChild(*baselineChild) + baselineChild->logicalTop())) };
}

static RenderMathMLOperator* toVerticalStretchyOperator(RenderBox* box)
{
    if (auto* mathMLBlock = dynamicDowncast<RenderMathMLBlock>(box)) {
        auto* renderOperator = mathMLBlock->unembellishedOperator();
        if (renderOperator && renderOperator->isStretchy() && renderOperator->isVertical())
            return renderOperator;
    }
    return nullptr;
}

void RenderMathMLRow::stretchVerticalOperatorsAndLayoutChildren()
{
    // First calculate stretch ascent and descent.
    LayoutUnit stretchAscent, stretchDescent;
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned()) {
            child->containingBlock()->insertPositionedObject(*child);
            continue;
        }
        if (toVerticalStretchyOperator(child))
            continue;
        child->layoutIfNeeded();
        LayoutUnit childAscent = ascentForChild(*child);
        LayoutUnit childDescent = child->logicalHeight() - childAscent;
        stretchAscent = std::max(stretchAscent, childAscent);
        stretchDescent = std::max(stretchDescent, childDescent);
    }
    if (stretchAscent + stretchDescent <= 0) {
        // We ensure a minimal stretch size.
        stretchAscent = style().computedFontSize();
        stretchDescent = 0;
    }

    // Next, we stretch the vertical operators.
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        if (auto renderOperator = toVerticalStretchyOperator(child)) {
            renderOperator->stretchTo(stretchAscent, stretchDescent);
            renderOperator->layoutIfNeeded();
            child->layoutIfNeeded();
        }
    }
}

void RenderMathMLRow::getContentBoundingBox(LayoutUnit& width, LayoutUnit& ascent, LayoutUnit& descent) const
{
    ascent = 0;
    descent = 0;
    width = borderAndPaddingStart();
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        width += child->marginStart() + child->logicalWidth() + child->marginEnd();
        LayoutUnit childAscent = ascentForChild(*child);
        LayoutUnit childDescent = child->logicalHeight() - childAscent;
        ascent = std::max(ascent, childAscent + child->marginTop());
        descent = std::max(descent, childDescent + child->marginBottom());
    }
    width += borderEnd() + paddingEnd();
}

void RenderMathMLRow::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    LayoutUnit preferredWidth;
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        preferredWidth += child->maxPreferredLogicalWidth() + child->marginLogicalWidth();
    }

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = preferredWidth + borderAndPaddingLogicalWidth();

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLRow::layoutRowItems(LayoutUnit width, LayoutUnit ascent)
{
    LayoutUnit horizontalOffset = borderAndPaddingStart();
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        horizontalOffset += child->marginStart();
        LayoutUnit childAscent = ascentForChild(*child);
        LayoutUnit childVerticalOffset = borderTop() + paddingTop() + child->marginTop() + ascent - childAscent;
        LayoutUnit childWidth = child->logicalWidth();
        LayoutUnit childHorizontalOffset = style().isLeftToRightDirection() ? horizontalOffset : width - horizontalOffset - childWidth;
        auto repaintRect = child->checkForRepaintDuringLayout() ? std::make_optional(child->frameRect()) : std::nullopt;
        child->setLocation(LayoutPoint(childHorizontalOffset, childVerticalOffset));
        if (repaintRect) {
            repaintRect->uniteEvenIfEmpty(child->frameRect());
            repaintRectangle(*repaintRect);
        }
        horizontalOffset += childWidth + child->marginEnd();
    }
}

void RenderMathMLRow::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    recomputeLogicalWidth();

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    LayoutUnit width, ascent, descent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(width, ascent, descent);
    layoutRowItems(width, ascent);
    setLogicalWidth(width);
    setLogicalHeight(borderTop() + paddingTop() + ascent + descent + borderBottom() + paddingBottom() + horizontalScrollbarHeight());
    updateLogicalHeight();

    layoutPositionedObjects(relayoutChildren);

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

}

#endif // ENABLE(MATHML)
