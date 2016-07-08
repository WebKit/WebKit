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

#include "MathMLNames.h"
#include <cmath>

namespace WebCore {

using namespace MathMLNames;

RenderMathMLPadded::RenderMathMLPadded(Element& element, RenderStyle&& style)
    : RenderMathMLRow(element, WTFMove(style))
{
}

void RenderMathMLPadded::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    // Determine the intrinsic width of the content.
    RenderMathMLRow::computePreferredLogicalWidths();

    // Only the width attribute should modify the width.
    // We parse it using the preferred width of the content as its default value.
    LayoutUnit width = m_maxPreferredLogicalWidth;
    parseMathMLLength(element()->fastGetAttribute(MathMLNames::widthAttr), width, &style(), false);

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = width;

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLPadded::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // We first layout our children as a normal <mrow> element.
    LayoutUnit contentAscent, contentDescent, contentWidth;
    contentAscent = contentDescent = 0;
    RenderMathMLRow::computeLineVerticalStretch(contentAscent, contentDescent);
    RenderMathMLRow::layoutRowItems(contentAscent, contentDescent);
    contentWidth = logicalWidth();

    // We parse the mpadded attributes using the content metrics as the default value.
    // FIXME: We should also accept pseudo-units and (some) negative values.
    // See https://bugs.webkit.org/show_bug.cgi?id=85730
    LayoutUnit width = contentWidth;
    LayoutUnit ascent = contentAscent;
    LayoutUnit descent = contentDescent;
    LayoutUnit lspace = 0;
    LayoutUnit voffset = 0;
    parseMathMLLength(element()->fastGetAttribute(MathMLNames::widthAttr), width, &style());
    parseMathMLLength(element()->fastGetAttribute(MathMLNames::heightAttr), ascent, &style());
    parseMathMLLength(element()->fastGetAttribute(MathMLNames::depthAttr), descent, &style());
    parseMathMLLength(element()->fastGetAttribute(MathMLNames::lspaceAttr), lspace, &style());
    parseMathMLLength(element()->fastGetAttribute(MathMLNames::voffsetAttr), voffset, &style());
    if (width < 0)
        width = 0;
    if (ascent < 0)
        ascent = 0;
    if (descent < 0)
        descent = 0;
    if (lspace < 0)
        lspace = 0;

    // Align children on the new baseline and shift them by (lspace, -voffset)
    LayoutPoint contentLocation(lspace, ascent - contentAscent - voffset);
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
        child->setLocation(child->location() + contentLocation);

    // Set the final metrics.
    setLogicalWidth(width);
    m_ascent = ascent;
    setLogicalHeight(ascent + descent);

    clearNeedsLayout();
}

void RenderMathMLPadded::updateFromElement()
{
    RenderMathMLRow::updateFromElement();
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLPadded::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLRow::styleDidChange(diff, oldStyle);
    setNeedsLayoutAndPrefWidthsRecalc();
}

Optional<int> RenderMathMLPadded::firstLineBaseline() const
{
    return Optional<int>(std::lround(static_cast<float>(m_ascent)));
}

}

#endif
