/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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
#include "RenderMathMLMath.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "MathMLRowElement.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMathMLMath);

RenderMathMLMath::RenderMathMLMath(MathMLRowElement& element, UniqueRef<Style::ComputedStyle>&& style)
    : RenderMathMLRow(Type::MathMLMath, element, WTF::move(style))
{
    ASSERT(isRenderMathMLMath());
}

RenderMathMLMath::~RenderMathMLMath() = default;

void RenderMathMLMath::moveInFlowChildrenHorizontally(LayoutUnit offset)
{
    if (!offset)
        return;

    for (CheckedPtr child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        auto repaintRect = child->checkForRepaintDuringLayout() ? std::make_optional(child->borderBoxRectInContainer()) : std::nullopt;
        child->move(offset, { });
        if (repaintRect) {
            repaintRect->uniteEvenIfEmpty(child->borderBoxRectInContainer());
            repaintRectangle(*repaintRect);
        }
    }
}

void RenderMathMLMath::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (style().display() != Style::DisplayType::BlockFlow) {
        RenderMathMLRow::layoutBlock(relayoutChildren, pageLogicalHeight);
        return;
    }

    insertPositionedChildrenIntoContainingBlock();

    if (relayoutChildren == RelayoutChildren::No && simplifiedLayout())
        return;

    layoutFloatingChildren();

    recomputeLogicalWidth();
    computeAndSetBlockDirectionMarginsOfChildren();

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    LayoutUnit width, ascent, descent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(width, ascent, descent);
    layoutRowItems(width, ascent);

    // Display formulas must be centered horizontally within the content box if there is extra space left.
    // Otherwise, logical width must be updated to fit the content to avoid truncation.
    auto nonContentWidth = borderAndPaddingLogicalWidth() + scrollbarLogicalWidth();
    auto availableContentWidth = logicalWidth() - nonContentWidth;
    LayoutUnit centerOffset;
    if (width < availableContentWidth)
        centerOffset = (availableContentWidth - width) / 2;
    else
        setLogicalWidth(width + nonContentWidth);

    auto contentLeft = borderAndPaddingLogicalLeft();
    if (shouldPlaceVerticalScrollbarOnLeft())
        contentLeft += scrollbarLogicalWidth();
    moveInFlowChildrenHorizontally(contentLeft + centerOffset);
    shiftInFlowChildren(0_lu, borderAndPaddingBefore());

    setLogicalHeight(ascent + descent + borderAndPaddingLogicalHeight() + horizontalScrollbarHeight());
    updateLogicalHeight();

    layoutOutOfFlowBoxes(relayoutChildren);
}

}

#endif // ENABLE(MATHML)
