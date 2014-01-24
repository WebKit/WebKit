/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2013 Igalia S.L.
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

#if ENABLE(MATHML)

#include "RenderMathMLOperator.h"

#include "FontCache.h"
#include "FontSelector.h"
#include "MathMLNames.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "ScaleTransformOperation.h"
#include "TransformOperations.h"
#include <wtf/MathExtras.h>

namespace WebCore {
    
using namespace MathMLNames;

// FIXME: The OpenType MATH table contains information that should override this table (http://wkbug/122297).
static RenderMathMLOperator::StretchyCharacter stretchyCharacters[14] = {
    { 0x28  , 0x239b, 0x239c, 0x239d, 0x0    }, // left parenthesis
    { 0x29  , 0x239e, 0x239f, 0x23a0, 0x0    }, // right parenthesis
    { 0x5b  , 0x23a1, 0x23a2, 0x23a3, 0x0    }, // left square bracket
    { 0x2308, 0x23a1, 0x23a2, 0x23a2, 0x0    }, // left ceiling
    { 0x230a, 0x23a2, 0x23a2, 0x23a3, 0x0    }, // left floor
    { 0x5d  , 0x23a4, 0x23a5, 0x23a6, 0x0    }, // right square bracket
    { 0x2309, 0x23a4, 0x23a5, 0x23a5, 0x0    }, // right ceiling
    { 0x230b, 0x23a5, 0x23a5, 0x23a6, 0x0    }, // right floor
    { 0x7b  , 0x23a7, 0x23aa, 0x23a9, 0x23a8 }, // left curly bracket
    { 0x7c  , 0x7c,   0x7c,   0x7c,   0x0    }, // vertical bar
    { 0x2016, 0x2016, 0x2016, 0x2016, 0x0    }, // double vertical line
    { 0x2225, 0x2225, 0x2225, 0x2225, 0x0    }, // parallel to
    { 0x7d  , 0x23ab, 0x23aa, 0x23ad, 0x23ac }, // right curly bracket
    { 0x222b, 0x2320, 0x23ae, 0x2321, 0x0    } // integral sign
};

RenderMathMLOperator::RenderMathMLOperator(MathMLElement& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, std::move(style))
    , m_stretchHeight(0)
    , m_operator(0)
    , m_operatorType(Default)
    , m_stretchyCharacter(nullptr)
{
}

RenderMathMLOperator::RenderMathMLOperator(MathMLElement& element, PassRef<RenderStyle> style, UChar operatorChar)
    : RenderMathMLBlock(element, std::move(style))
    , m_stretchHeight(0)
    , m_operator(convertHyphenMinusToMinusSign(operatorChar))
    , m_operatorType(Default)
    , m_stretchyCharacter(nullptr)
{
}

bool RenderMathMLOperator::isChildAllowed(const RenderObject&, const RenderStyle&) const
{
    return false;
}

static const float gOperatorExpansion = 1.2f;

float RenderMathMLOperator::expandedStretchHeight() const
{
    return m_stretchHeight * gOperatorExpansion;
}

void RenderMathMLOperator::stretchToHeight(int height)
{
    if (m_stretchHeight == height)
        return;

    m_stretchHeight = height;
    updateStyle();
}

void RenderMathMLOperator::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    updateFromElement();
}

FloatRect RenderMathMLOperator::glyphBoundsForCharacter(UChar character)
{
    GlyphData data = style().font().glyphDataForCharacter(character, false);
    return data.fontData->boundsForGlyph(data.glyph);
}

float RenderMathMLOperator::glyphHeightForCharacter(UChar character)
{
    return glyphBoundsForCharacter(character).height();
}

float RenderMathMLOperator::advanceForCharacter(UChar character)
{
    // Hyphen minus is always replaced by the minus sign in rendered text.
    GlyphData data = style().font().glyphDataForCharacter(convertHyphenMinusToMinusSign(character), false);
    return data.fontData->widthForGlyph(data.glyph);
}

void RenderMathMLOperator::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    UChar stretchedCharacter;
    bool allowStretching = shouldAllowStretching(stretchedCharacter);
    if (!allowStretching) {
        RenderMathMLBlock::computePreferredLogicalWidths();
        return;
    }

    float maximumGlyphWidth = advanceForCharacter(stretchedCharacter);
    for (unsigned index = 0; index < WTF_ARRAY_LENGTH(stretchyCharacters); ++index) {
        if (stretchyCharacters[index].character != stretchedCharacter)
            continue;

        StretchyCharacter& character = stretchyCharacters[index];
        if (character.topGlyph)
            maximumGlyphWidth = std::max(maximumGlyphWidth, advanceForCharacter(character.topGlyph));
        if (character.extensionGlyph)
            maximumGlyphWidth = std::max(maximumGlyphWidth, advanceForCharacter(character.extensionGlyph));
        if (character.bottomGlyph)
            maximumGlyphWidth = std::max(maximumGlyphWidth, advanceForCharacter(character.bottomGlyph));
        if (character.middleGlyph)
            maximumGlyphWidth = std::max(maximumGlyphWidth, advanceForCharacter(character.middleGlyph));
        m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = maximumGlyphWidth;
        return;
    }

    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = maximumGlyphWidth;
}

// FIXME: It's cleaner to only call updateFromElement when an attribute has changed. The body of
// this method should probably be moved to a private stretchHeightChanged or checkStretchHeight
// method. Probably at the same time, addChild/removeChild methods should be made to work for
// dynamic DOM changes.
void RenderMathMLOperator::updateFromElement()
{
    RenderElement* savedRenderer = element().renderer();

    // Destroy our current children
    destroyLeftoverChildren();

    // Since we share a node with our children, destroying our children may set our node's
    // renderer to 0, so we need to restore it.
    element().setRenderer(savedRenderer);

    RenderPtr<RenderMathMLBlock> container = createRenderer<RenderMathMLBlock>(element(), RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX));
    // This container doesn't offer any useful information to accessibility.
    container->setIgnoreInAccessibilityTree(true);
    container->initializeStyle();

    RenderPtr<RenderText> text;
    if (m_operator)
        text = createRenderer<RenderText>(document(), String(&m_operator, 1));
    else
        text = createRenderer<RenderText>(document(), element().textContent().replace(hyphenMinus, minusSign).impl());

    container->addChild(text.leakPtr());
    addChild(container.leakPtr());

    updateStyle();
    setNeedsLayoutAndPrefWidthsRecalc();
}

bool RenderMathMLOperator::shouldAllowStretching(UChar& stretchedCharacter)
{
    if (equalIgnoringCase(element().getAttribute(MathMLNames::stretchyAttr), "false"))
        return false;

    if (m_operator) {
        stretchedCharacter = m_operator;
        return true;
    }

    // FIXME: This does not handle surrogate pairs (http://wkbug.com/122296/).
    String opText = element().textContent();
    stretchedCharacter = 0;
    for (unsigned i = 0; i < opText.length(); ++i) {
        // If there's more than one non-whitespace character in this node, then don't even try to stretch it.
        if (stretchedCharacter && !isSpaceOrNewline(opText[i]))
            return false;

        if (!isSpaceOrNewline(opText[i]))
            stretchedCharacter = opText[i];
    }

    return stretchedCharacter;
}

// FIXME: We should also look at alternate characters defined in the OpenType MATH table (http://wkbug/122297).
RenderMathMLOperator::StretchyCharacter* RenderMathMLOperator::findAcceptableStretchyCharacter(UChar character)
{
    StretchyCharacter* stretchyCharacter = 0;
    const int maxIndex = WTF_ARRAY_LENGTH(stretchyCharacters);
    for (int index = 0; index < maxIndex; ++index) {
        if (stretchyCharacters[index].character == character) {
            stretchyCharacter = &stretchyCharacters[index];
            break;
        }
    }

    // If we didn't find a stretchy character set for this character, we don't know how to stretch it.
    if (!stretchyCharacter)
        return 0;

    float height = glyphHeightForCharacter(stretchyCharacter->topGlyph) + glyphHeightForCharacter(stretchyCharacter->bottomGlyph);
    if (stretchyCharacter->middleGlyph)
        height += glyphHeightForCharacter(stretchyCharacter->middleGlyph);

    if (height > expandedStretchHeight())
        return 0;

    return stretchyCharacter;
}

void RenderMathMLOperator::updateStyle()
{
    ASSERT(firstChild());
    if (!firstChild())
        return;

    UChar stretchedCharacter;
    bool allowStretching = shouldAllowStretching(stretchedCharacter);

    float stretchedCharacterHeight = style().fontMetrics().floatHeight();
    m_isStretched = allowStretching && expandedStretchHeight() > stretchedCharacterHeight;

    // Sometimes we cannot stretch an operator properly, so in that case, we should just use the original size.
    m_stretchyCharacter = m_isStretched ? findAcceptableStretchyCharacter(stretchedCharacter) : 0;
    if (!m_stretchyCharacter)
        m_isStretched = false;
}

int RenderMathMLOperator::firstLineBaseline() const
{
    if (m_isStretched)
        return expandedStretchHeight() * 2 / 3 - (expandedStretchHeight() - m_stretchHeight) / 2;
    return RenderMathMLBlock::firstLineBaseline();
}

void RenderMathMLOperator::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    if (m_isStretched)
        logicalHeight = expandedStretchHeight();
    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

LayoutRect RenderMathMLOperator::paintCharacter(PaintInfo& info, UChar character, const LayoutPoint& origin, CharacterPaintTrimming trim)
{
    GlyphData data = style().font().glyphDataForCharacter(character, false);
    FloatRect glyphBounds = data.fontData->boundsForGlyph(data.glyph);

    LayoutRect glyphPaintRect(origin, LayoutSize(glyphBounds.x() + glyphBounds.width(), glyphBounds.height()));
    glyphPaintRect.setY(origin.y() + glyphBounds.y());

    // In order to have glyphs fit snugly with one another we snap the connecting edges to pixel boundaries
    // and trim off one pixel. The pixel trim is to account for fonts that have edge pixels that have less
    // than full coverage. These edge pixels can introduce small seams between connected glyphs
    FloatRect clipBounds = info.rect;
    switch (trim) {
    case TrimTop:
        glyphPaintRect.shiftYEdgeTo(glyphPaintRect.y().ceil() + 1);
        clipBounds.shiftYEdgeTo(glyphPaintRect.y());
        break;
    case TrimBottom:
        glyphPaintRect.shiftMaxYEdgeTo(glyphPaintRect.maxY().floor() - 1);
        clipBounds.shiftMaxYEdgeTo(glyphPaintRect.maxY());
        break;
    case TrimTopAndBottom:
        LayoutUnit temp = glyphPaintRect.y() + 1;
        glyphPaintRect.shiftYEdgeTo(temp.ceil());
        glyphPaintRect.shiftMaxYEdgeTo(glyphPaintRect.maxY().floor() - 1);
        clipBounds.shiftYEdgeTo(glyphPaintRect.y());
        clipBounds.shiftMaxYEdgeTo(glyphPaintRect.maxY());
        break;
    }

    // Clipping the enclosing IntRect avoids any potential issues at joined edges.
    GraphicsContextStateSaver stateSaver(*info.context);
    info.context->clip(clipBounds);

    info.context->drawText(style().font(), TextRun(&character, 1), origin);

    return glyphPaintRect;
}

void RenderMathMLOperator::fillWithExtensionGlyph(PaintInfo& info, const LayoutPoint& from, const LayoutPoint& to)
{
    ASSERT(m_stretchyCharacter);
    ASSERT(m_stretchyCharacter->extensionGlyph);
    ASSERT(from.y() <= to.y());

    // If there is no space for the extension glyph, we don't need to do anything.
    if (from.y() == to.y())
        return;

    GraphicsContextStateSaver stateSaver(*info.context);

    FloatRect glyphBounds = glyphBoundsForCharacter(m_stretchyCharacter->extensionGlyph);

    // Clipping the extender region here allows us to draw the bottom extender glyph into the
    // regions of the bottom glyph without worrying about overdraw (hairy pixels) and simplifies later clipping.
    LayoutRect clipBounds = info.rect;
    clipBounds.shiftYEdgeTo(from.y());
    clipBounds.shiftMaxYEdgeTo(to.y());
    info.context->clip(clipBounds);

    // Trimming may remove up to two pixels from the top of the extender glyph, so we move it up by two pixels.
    float offsetToGlyphTop = glyphBounds.y() + 2;
    LayoutPoint glyphOrigin = LayoutPoint(from.x(), from.y() - offsetToGlyphTop);
    FloatRect lastPaintedGlyphRect(from, FloatSize());

    while (lastPaintedGlyphRect.maxY() < to.y()) {
        lastPaintedGlyphRect = paintCharacter(info, m_stretchyCharacter->extensionGlyph, glyphOrigin, TrimTopAndBottom);
        glyphOrigin.setY(glyphOrigin.y() + lastPaintedGlyphRect.height());

        // There's a chance that if the font size is small enough the glue glyph has been reduced to an empty rectangle
        // with trimming. In that case we just draw nothing.
        if (lastPaintedGlyphRect.isEmpty())
            break;
    }
}

void RenderMathMLOperator::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);

    if (info.context->paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE)
        return;

    if (!m_isStretched && !m_stretchyCharacter) {
        RenderMathMLBlock::paint(info, paintOffset);
        return;
    }

    GraphicsContextStateSaver stateSaver(*info.context);
    info.context->setFillColor(style().visitedDependentColor(CSSPropertyColor), style().colorSpace());

    ASSERT(m_stretchyCharacter->topGlyph);
    ASSERT(m_stretchyCharacter->bottomGlyph);

    // We are positioning the glyphs so that the edge of the tight glyph bounds line up exactly with the edges of our paint box.
    LayoutPoint operatorTopLeft = ceiledIntPoint(paintOffset + location());
    FloatRect topGlyphBounds = glyphBoundsForCharacter(m_stretchyCharacter->topGlyph);
    LayoutPoint topGlyphOrigin(operatorTopLeft.x(), operatorTopLeft.y() - topGlyphBounds.y());
    LayoutRect topGlyphPaintRect = paintCharacter(info, m_stretchyCharacter->topGlyph, topGlyphOrigin, TrimBottom);

    FloatRect bottomGlyphBounds = glyphBoundsForCharacter(m_stretchyCharacter->bottomGlyph);
    LayoutPoint bottomGlyphOrigin(operatorTopLeft.x(), operatorTopLeft.y() + offsetHeight() - (bottomGlyphBounds.height() + bottomGlyphBounds.y()));
    LayoutRect bottomGlyphPaintRect = paintCharacter(info, m_stretchyCharacter->bottomGlyph, bottomGlyphOrigin, TrimTop);

    if (m_stretchyCharacter->middleGlyph) {
        // Center the glyph origin between the start and end glyph paint extents. Then shift it half the paint height toward the bottom glyph.
        FloatRect middleGlyphBounds = glyphBoundsForCharacter(m_stretchyCharacter->middleGlyph);
        LayoutPoint middleGlyphOrigin(operatorTopLeft.x(), topGlyphOrigin.y());
        middleGlyphOrigin.moveBy(LayoutPoint(0, (bottomGlyphPaintRect.y() - topGlyphPaintRect.maxY()) / 2.0));
        middleGlyphOrigin.moveBy(LayoutPoint(0, middleGlyphBounds.height() / 2.0));

        LayoutRect middleGlyphPaintRect = paintCharacter(info, m_stretchyCharacter->middleGlyph, middleGlyphOrigin, TrimTopAndBottom);
        fillWithExtensionGlyph(info, topGlyphPaintRect.minXMaxYCorner(), middleGlyphPaintRect.minXMinYCorner());
        fillWithExtensionGlyph(info, middleGlyphPaintRect.minXMaxYCorner(), bottomGlyphPaintRect.minXMinYCorner());
    } else
        fillWithExtensionGlyph(info, topGlyphPaintRect.minXMaxYCorner(), bottomGlyphPaintRect.minXMinYCorner());
}

void RenderMathMLOperator::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    if (m_isStretched)
        return;
    RenderMathMLBlock::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}
    
}

#endif
