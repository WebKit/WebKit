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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLMath);

RenderMathMLMath::RenderMathMLMath(MathMLRowElement& element, RenderStyle&& style)
    : RenderMathMLRow(element, WTFMove(style))
{
}

void RenderMathMLMath::centerChildren(LayoutUnit contentWidth)
{
    LayoutUnit centerBlockOffset = (logicalWidth() - contentWidth) / 2;
    if (!style().isLeftToRightDirection())
        centerBlockOffset = -centerBlockOffset;
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!child->isOutOfFlowPositioned())
            child->setLocation(child->location() + LayoutPoint(centerBlockOffset, 0_lu));
    }
}

void RenderMathMLMath::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (style().display() != DisplayType::Block) {
        RenderMathMLRow::layoutBlock(relayoutChildren, pageLogicalHeight);
        return;
    }

    if (!relayoutChildren && simplifiedLayout())
        return;

    recomputeLogicalWidth();

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    LayoutUnit width, ascent, descent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(width, ascent, descent);
    layoutRowItems(logicalWidth(), ascent);

    // Display formulas must be centered horizontally if there is extra space left.
    // Otherwise, logical width must be updated to the content width to avoid truncation.
    if (width < logicalWidth())
        centerChildren(width);
    else
        setLogicalWidth(width);

    setLogicalHeight(borderTop() + paddingTop() + ascent + descent + borderBottom() + paddingBottom() + horizontalScrollbarHeight());
    updateLogicalHeight();

    layoutPositionedObjects(relayoutChildren);

    clearNeedsLayout();
}

}

#endif // ENABLE(MATHML)
