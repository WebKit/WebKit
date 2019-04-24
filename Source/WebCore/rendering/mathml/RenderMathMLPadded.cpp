/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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
#include "RenderMathMLPadded.h"

#if ENABLE(MATHML)

#include <cmath>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLPadded);

RenderMathMLPadded::RenderMathMLPadded(MathMLPaddedElement& element, RenderStyle&& style)
    : RenderMathMLRow(element, WTFMove(style))
{
}

LayoutUnit RenderMathMLPadded::voffset() const
{
    return toUserUnits(element().voffset(), style(), 0);
}

LayoutUnit RenderMathMLPadded::lspace() const
{
    LayoutUnit lspace = toUserUnits(element().lspace(), style(), 0);
    // FIXME: Negative lspace values are not supported yet (https://bugs.webkit.org/show_bug.cgi?id=85730).
    return std::max<LayoutUnit>(0, lspace);
}

LayoutUnit RenderMathMLPadded::mpaddedWidth(LayoutUnit contentWidth) const
{
    return std::max<LayoutUnit>(0, toUserUnits(element().width(), style(), contentWidth));
}

LayoutUnit RenderMathMLPadded::mpaddedHeight(LayoutUnit contentHeight) const
{
    return std::max<LayoutUnit>(0, toUserUnits(element().height(), style(), contentHeight));
}

LayoutUnit RenderMathMLPadded::mpaddedDepth(LayoutUnit contentDepth) const
{
    return std::max<LayoutUnit>(0, toUserUnits(element().depth(), style(), contentDepth));
}

void RenderMathMLPadded::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    // Determine the intrinsic width of the content.
    RenderMathMLRow::computePreferredLogicalWidths();

    // Only the width attribute should modify the width.
    // We parse it using the preferred width of the content as its default value.
    m_maxPreferredLogicalWidth = mpaddedWidth(m_maxPreferredLogicalWidth);
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLPadded::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // We first layout our children as a normal <mrow> element.
    LayoutUnit contentWidth, contentAscent, contentDescent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(contentWidth, contentAscent, contentDescent);
    layoutRowItems(contentWidth, contentAscent);

    // We parse the mpadded attributes using the content metrics as the default value.
    LayoutUnit width = mpaddedWidth(contentWidth);
    LayoutUnit ascent = mpaddedHeight(contentAscent);
    LayoutUnit descent = mpaddedDepth(contentDescent);

    // Align children on the new baseline and shift them by (lspace, -voffset)
    LayoutPoint contentLocation(lspace(), ascent - contentAscent - voffset());
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
        child->setLocation(child->location() + contentLocation);

    // Set the final metrics.
    setLogicalWidth(width);
    setLogicalHeight(ascent + descent);

    layoutPositionedObjects(relayoutChildren);

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

Optional<int> RenderMathMLPadded::firstLineBaseline() const
{
    // We try and calculate the baseline from the position of the first child.
    LayoutUnit ascent;
    if (auto* baselineChild = firstChildBox())
        ascent = ascentForChild(*baselineChild) + baselineChild->logicalTop() + voffset();
    else
        ascent = mpaddedHeight(0);
    return Optional<int>(std::lround(static_cast<float>(ascent)));
}

}

#endif
