/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2013, 2016 Igalia S.L.
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
#include "RenderMathMLOperator.h"

#if ENABLE(MATHML)

#include "FontSelector.h"
#include "MathMLNames.h"
#include "MathMLOperatorElement.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "ScaleTransformOperation.h"
#include "TransformOperations.h"
#include <cmath>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLOperator);

RenderMathMLOperator::RenderMathMLOperator(MathMLOperatorElement& element, RenderStyle&& style)
    : RenderMathMLToken(element, WTFMove(style))
{
    updateTokenContent();
}

RenderMathMLOperator::RenderMathMLOperator(Document& document, RenderStyle&& style)
    : RenderMathMLToken(document, WTFMove(style))
{
}

MathMLOperatorElement& RenderMathMLOperator::element() const
{
    return static_cast<MathMLOperatorElement&>(nodeForNonAnonymous());
}

UChar32 RenderMathMLOperator::textContent() const
{
    return element().operatorChar().character;
}

bool RenderMathMLOperator::isInvisibleOperator() const
{
    // The following operators are invisible: U+2061 FUNCTION APPLICATION, U+2062 INVISIBLE TIMES, U+2063 INVISIBLE SEPARATOR, U+2064 INVISIBLE PLUS.
    UChar32 character = textContent();
    return 0x2061 <= character && character <= 0x2064;
}

bool RenderMathMLOperator::hasOperatorFlag(MathMLOperatorDictionary::Flag flag) const
{
    return element().hasProperty(flag);
}

LayoutUnit RenderMathMLOperator::leadingSpace() const
{
    // FIXME: Negative leading spaces must be implemented (https://webkit.org/b/124830).
    LayoutUnit leadingSpace = toUserUnits(element().defaultLeadingSpace(), style(), 0);
    leadingSpace = toUserUnits(element().leadingSpace(), style(), leadingSpace);
    return std::max<LayoutUnit>(0, leadingSpace);
}

LayoutUnit RenderMathMLOperator::trailingSpace() const
{
    // FIXME: Negative trailing spaces must be implemented (https://webkit.org/b/124830).
    LayoutUnit trailingSpace = toUserUnits(element().defaultTrailingSpace(), style(), 0);
    trailingSpace = toUserUnits(element().trailingSpace(), style(), trailingSpace);
    return std::max<LayoutUnit>(0, trailingSpace);
}

LayoutUnit RenderMathMLOperator::minSize() const
{
    LayoutUnit minSize { style().fontCascade().size() }; // Default minsize is "1em".
    minSize = toUserUnits(element().minSize(), style(), minSize);
    return std::max<LayoutUnit>(0, minSize);
}

LayoutUnit RenderMathMLOperator::maxSize() const
{
    LayoutUnit maxSize = intMaxForLayoutUnit; // Default maxsize is "infinity".
    maxSize = toUserUnits(element().maxSize(), style(), maxSize);
    return std::max<LayoutUnit>(0, maxSize);
}

bool RenderMathMLOperator::isVertical() const
{
    return element().operatorChar().isVertical;
}


void RenderMathMLOperator::stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline)
{
    ASSERT(isStretchy());
    ASSERT(isVertical());
    ASSERT(!isStretchWidthLocked());

    if (!isVertical() || (heightAboveBaseline == m_stretchHeightAboveBaseline && depthBelowBaseline == m_stretchDepthBelowBaseline))
        return;

    m_stretchHeightAboveBaseline = heightAboveBaseline;
    m_stretchDepthBelowBaseline = depthBelowBaseline;

    if (hasOperatorFlag(MathMLOperatorDictionary::Symmetric)) {
        // We make the operator stretch symmetrically above and below the axis.
        LayoutUnit axis = mathAxisHeight();
        LayoutUnit halfStretchSize = std::max(m_stretchHeightAboveBaseline - axis, m_stretchDepthBelowBaseline + axis);
        m_stretchHeightAboveBaseline = halfStretchSize + axis;
        m_stretchDepthBelowBaseline = halfStretchSize - axis;
    }
    // We try to honor the minsize/maxsize condition by increasing or decreasing both height and depth proportionately.
    // The MathML specification does not indicate what to do when maxsize < minsize, so we follow Gecko and make minsize take precedence.
    LayoutUnit size = stretchSize();
    float aspect = 1.0;
    if (size > 0) {
        LayoutUnit minSizeValue = minSize();
        if (size < minSizeValue)
            aspect = minSizeValue.toFloat() / size;
        else {
            LayoutUnit maxSizeValue = maxSize();
            if (maxSizeValue < size)
                aspect = maxSizeValue.toFloat() / size;
        }
    }
    m_stretchHeightAboveBaseline *= aspect;
    m_stretchDepthBelowBaseline *= aspect;

    m_mathOperator.stretchTo(style(), m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline);

    setLogicalHeight(m_mathOperator.ascent() + m_mathOperator.descent());
}

void RenderMathMLOperator::stretchTo(LayoutUnit width)
{
    ASSERT(isStretchy());
    ASSERT(!isVertical());
    ASSERT(!isStretchWidthLocked());

    if (isVertical() || m_stretchWidth == width)
        return;

    m_stretchWidth = width;
    m_mathOperator.stretchTo(style(), width);

    setLogicalWidth(leadingSpace() + width + trailingSpace());
    setLogicalHeight(m_mathOperator.ascent() + m_mathOperator.descent());
}

void RenderMathMLOperator::resetStretchSize()
{
    ASSERT(!isStretchWidthLocked());

    if (isVertical()) {
        m_stretchHeightAboveBaseline = 0;
        m_stretchDepthBelowBaseline = 0;
    } else
        m_stretchWidth = 0;
}

void RenderMathMLOperator::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    LayoutUnit preferredWidth;

    if (!useMathOperator()) {
        RenderMathMLToken::computePreferredLogicalWidths();
        preferredWidth = m_maxPreferredLogicalWidth;
        if (isInvisibleOperator()) {
            // In some fonts, glyphs for invisible operators have nonzero width. Consequently, we subtract that width here to avoid wide gaps.
            GlyphData data = style().fontCascade().glyphDataForCharacter(textContent(), false);
            float glyphWidth = data.font ? data.font->widthForGlyph(data.glyph) : 0;
            ASSERT(glyphWidth <= preferredWidth);
            preferredWidth -= glyphWidth;
        }
    } else
        preferredWidth = m_mathOperator.maxPreferredWidth();

    // FIXME: The spacing should be added to the whole embellished operator (https://webkit.org/b/124831).
    // FIXME: The spacing should only be added inside (perhaps inferred) mrow (http://www.w3.org/TR/MathML/chapter3.html#presm.opspacing).
    preferredWidth = leadingSpace() + preferredWidth + trailingSpace();

    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = preferredWidth;

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLOperator::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutUnit leadingSpaceValue = leadingSpace();
    LayoutUnit trailingSpaceValue = trailingSpace();

    if (useMathOperator()) {
        for (auto child = firstChildBox(); child; child = child->nextSiblingBox())
            child->layoutIfNeeded();
        setLogicalWidth(leadingSpaceValue + m_mathOperator.width() + trailingSpaceValue);
        setLogicalHeight(m_mathOperator.ascent() + m_mathOperator.descent());
    } else {
        // We first do the normal layout without spacing.
        recomputeLogicalWidth();
        LayoutUnit width = logicalWidth();
        setLogicalWidth(width - leadingSpaceValue - trailingSpaceValue);
        RenderMathMLToken::layoutBlock(relayoutChildren, pageLogicalHeight);
        setLogicalWidth(width);

        // We then move the children to take spacing into account.
        LayoutPoint horizontalShift(style().direction() == TextDirection::LTR ? leadingSpaceValue : -leadingSpaceValue, 0_lu);
        for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
            child->setLocation(child->location() + horizontalShift);
    }

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

void RenderMathMLOperator::updateMathOperator()
{
    ASSERT(useMathOperator());
    MathOperator::Type type;
    if (isStretchy())
        type = isVertical() ? MathOperator::Type::VerticalOperator : MathOperator::Type::HorizontalOperator;
    else if (textContent() && isLargeOperatorInDisplayStyle())
        type = MathOperator::Type::DisplayOperator;
    else
        type = MathOperator::Type::NormalOperator;

    m_mathOperator.setOperator(style(), textContent(), type);
}

void RenderMathMLOperator::updateTokenContent()
{
    ASSERT(!isAnonymous());
    RenderMathMLToken::updateTokenContent();
    if (useMathOperator())
        updateMathOperator();
}

void RenderMathMLOperator::updateFromElement()
{
    updateTokenContent();
}

bool RenderMathMLOperator::useMathOperator() const
{
    // We use the MathOperator class to handle the following cases:
    // 1) Stretchy and large operators, since they require special painting.
    // 2) The minus sign, since it can be obtained from a hyphen in the DOM.
    return isStretchy() || (textContent() && isLargeOperatorInDisplayStyle()) || textContent() == minusSign;
}

void RenderMathMLOperator::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    m_mathOperator.reset(style());
}

LayoutUnit RenderMathMLOperator::verticalStretchedOperatorShift() const
{
    if (!isVertical() || !stretchSize())
        return 0;

    return (m_stretchDepthBelowBaseline - m_stretchHeightAboveBaseline - m_mathOperator.descent() + m_mathOperator.ascent()) / 2;
}

Optional<int> RenderMathMLOperator::firstLineBaseline() const
{
    if (useMathOperator())
        return Optional<int>(std::lround(static_cast<float>(m_mathOperator.ascent() - verticalStretchedOperatorShift())));
    return RenderMathMLToken::firstLineBaseline();
}

void RenderMathMLOperator::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLToken::paint(info, paintOffset);
    if (!useMathOperator())
        return;

    LayoutPoint operatorTopLeft = paintOffset + location();
    operatorTopLeft.move(style().isLeftToRightDirection() ? leadingSpace() : trailingSpace(), 0_lu);

    m_mathOperator.paint(style(), info, operatorTopLeft);
}

void RenderMathMLOperator::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    // We skip painting for invisible operators too to avoid some "missing character" glyph to appear if appropriate math fonts are not available.
    if (useMathOperator() || isInvisibleOperator())
        return;
    RenderMathMLToken::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}

}

#endif
