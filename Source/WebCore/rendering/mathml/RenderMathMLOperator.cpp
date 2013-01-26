/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
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
#include "RenderText.h"
#include "ScaleTransformOperation.h"
#include "TransformOperations.h"

namespace WebCore {
    
using namespace MathMLNames;

RenderMathMLOperator::RenderMathMLOperator(Element* element)
    : RenderMathMLBlock(element)
    , m_stretchHeight(0)
    , m_operator(0)
    , m_operatorType(Default)
{
}

RenderMathMLOperator::RenderMathMLOperator(Element* element, UChar operatorChar)
    : RenderMathMLBlock(element)
    , m_stretchHeight(0)
    , m_operator(convertHyphenMinusToMinusSign(operatorChar))
    , m_operatorType(Default)
{
}

bool RenderMathMLOperator::isChildAllowed(RenderObject*, RenderStyle*) const
{
    return false;
}

static const float gOperatorExpansion = 1.2f;

void RenderMathMLOperator::stretchToHeight(int height)
{
    height *= gOperatorExpansion;
    if (m_stretchHeight == height)
        return;
    m_stretchHeight = height;
    
    updateFromElement();
}

void RenderMathMLOperator::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    
    if (firstChild())
        updateFromElement();
}

// This is a table of stretchy characters.
// FIXME: Should this be read from the unicode characteristics somehow?
// table:  stretchy operator, top char, extension char, bottom char, middle char
static struct StretchyCharacter {
    UChar character;
    UChar topGlyph;
    UChar extensionGlyph;
    UChar bottomGlyph;
    UChar middleGlyph;
} stretchyCharacters[13] = {
    { 0x28  , 0x239b, 0x239c, 0x239d, 0x0    }, // left parenthesis
    { 0x29  , 0x239e, 0x239f, 0x23a0, 0x0    }, // right parenthesis
    { 0x5b  , 0x23a1, 0x23a2, 0x23a3, 0x0    }, // left square bracket
    { 0x2308, 0x23a1, 0x23a2, 0x23a2, 0x0    }, // left ceiling
    { 0x230a, 0x23a2, 0x23a2, 0x23a3, 0x0    }, // left floor
    { 0x5d  , 0x23a4, 0x23a5, 0x23a6, 0x0    }, // right square bracket
    { 0x2309, 0x23a4, 0x23a5, 0x23a5, 0x0    }, // right ceiling
    { 0x230b, 0x23a5, 0x23a5, 0x23a6, 0x0    }, // right floor
    { 0x7b  , 0x23a7, 0x23aa, 0x23a9, 0x23a8 }, // left curly bracket
    { 0x7c  , 0x23aa, 0x23aa, 0x23aa, 0x0    }, // vertical bar
    { 0x2016, 0x2016, 0x2016, 0x2016, 0x0    }, // double vertical line
    { 0x7d  , 0x23ab, 0x23aa, 0x23ad, 0x23ac }, // right curly bracket
    { 0x222b, 0x2320, 0x23ae, 0x2321, 0x0    } // integral sign
};

float RenderMathMLOperator::glyphHeightForCharacter(UChar character) const
{
    GlyphData data = style()->font().glyphDataForCharacter(character, false);
    FloatRect glyphBounds = data.fontData->boundsForGlyph(data.glyph);
    return glyphBounds.height();
}

float RenderMathMLOperator::widthForCharacter(UChar character) const
{
    TextRun textRun = constructTextRun(const_cast<RenderMathMLOperator*>(this), style()->font(), &character, 1, style());
    textRun.disableRoundingHacks();
    return style()->font().width(textRun);
}

void RenderMathMLOperator::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    int stretchyIndex = stretchyCharacterIndex();
    if (stretchyIndex < 0) {
        RenderMathMLBlock::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
        return;
    }

    // Use the max of all the possible characters used in the operator.
    // This may sometimes give us widths that are a bit too large, but that's
    // better than too small.
    // FIXME: Don't have the width be affected by operator stretching. Then we can
    // stop overriding computeIntrinsicLogicalWidths entirely.
    struct StretchyCharacter* partsData = &stretchyCharacters[stretchyIndex];
    float maxGlyphWidth = widthForCharacter(partsData->character);
    maxGlyphWidth = std::max(maxGlyphWidth, widthForCharacter(partsData->topGlyph));
    maxGlyphWidth = std::max(maxGlyphWidth, widthForCharacter(partsData->extensionGlyph));
    maxGlyphWidth = std::max(maxGlyphWidth, widthForCharacter(partsData->bottomGlyph));
    if (partsData->middleGlyph)
        maxGlyphWidth = std::max(maxGlyphWidth, widthForCharacter(partsData->middleGlyph));

    minLogicalWidth = maxLogicalWidth = maxGlyphWidth + borderAndPaddingLogicalWidth();
}

bool RenderMathMLOperator::stretchDisabledByMarkup() const
{
    if (!node()->isElementNode())
        return false;
    Element* mo = static_cast<Element*>(node());
    AtomicString stretchyAttr = mo->getAttribute(MathMLNames::stretchyAttr);
    return equalIgnoringCase(stretchyAttr, "false");
}

UChar RenderMathMLOperator::firstTextCharacter() const
{
    if (!m_operator) {
        String opText = node()->textContent();
        for (unsigned i = 0; i < opText.length(); i++) {
            if (!isSpaceOrNewline(opText[i]))
                return opText[i];
        }
    }
    return m_operator;
}

int RenderMathMLOperator::stretchyCharacterIndex() const
{
    bool stretchDisabled = stretchDisabledByMarkup();
    if (stretchDisabled)
        return -1;

    // Check for a stretchable character.
    UChar firstCharacater = firstTextCharacter();
    if (firstCharacater) {
        const int maxIndex = WTF_ARRAY_LENGTH(stretchyCharacters);
        for (int index = 0; index < maxIndex; index++) {
            if (stretchyCharacters[index].character == firstCharacater)
                return index;
        }
    }

    return -1;
}

// FIXME: It's cleaner to only call updateFromElement when an attribute has changed. The body of
// this method should probably be moved to a private stretchHeightChanged or checkStretchHeight
// method. Probably at the same time, addChild/removeChild methods should be made to work for
// dynamic DOM changes.
void RenderMathMLOperator::updateFromElement()
{
    RenderObject* savedRenderer = node()->renderer();

    // Destroy our current children
    children()->destroyLeftoverChildren();

    // Since we share a node with our children, destroying our children may set our node's
    // renderer to 0, so we need to restore it.
    node()->setRenderer(savedRenderer);
    
    int index = stretchyCharacterIndex();
    bool isStretchy = index >= 0;

    // We only stack glyphs if the stretch height is larger than a minimum size.
    bool shouldStack = isStretchy && m_stretchHeight > style()->fontSize();

    struct StretchyCharacter* partsData = 0;
    float topGlyphHeight = 0;
    float extensionGlyphHeight = 0;
    float bottomGlyphHeight = 0;
    float middleGlyphHeight = 0;
    if (isStretchy) {
        partsData = &stretchyCharacters[index];
        
        FontCachePurgePreventer fontCachePurgePreventer;
        
        topGlyphHeight = glyphHeightForCharacter(partsData->topGlyph);
        extensionGlyphHeight = glyphHeightForCharacter(partsData->extensionGlyph) - 1;
        bottomGlyphHeight = glyphHeightForCharacter(partsData->bottomGlyph);
        if (partsData->middleGlyph)
            middleGlyphHeight = glyphHeightForCharacter(partsData->middleGlyph) - 1;
        shouldStack = m_stretchHeight >= topGlyphHeight + middleGlyphHeight + bottomGlyphHeight && extensionGlyphHeight > 0;
    }

    bool stretchDisabled = stretchDisabledByMarkup();
    // Either stretch is disabled or we don't have a stretchable character over the minimum height
    if (stretchDisabled || !shouldStack) {
        RenderBlock* container = new (renderArena()) RenderMathMLBlock(toElement(node()));
        toRenderMathMLBlock(container)->setIgnoreInAccessibilityTree(true);
        
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(FLEX);
        newStyle->setJustifyContent(JustifyCenter);

        UChar firstCharacter = firstTextCharacter();
        m_isStretched = firstCharacter && isStretchy && m_stretchHeight > style()->fontMetrics().floatHeight();
        if (m_isStretched)
            newStyle->setHeight(Length(m_stretchHeight, Fixed));
        container->setStyle(newStyle.release());
        addChild(container);

        if (m_isStretched) {
            float scaleY = m_stretchHeight / glyphHeightForCharacter(firstCharacter);
            TransformOperations transform;
            transform.operations().append(ScaleTransformOperation::create(1.0, scaleY, ScaleTransformOperation::SCALE_X));

            RefPtr<RenderStyle> innerStyle = RenderStyle::create();
            innerStyle->inheritFrom(style());
            innerStyle->setTransform(transform);
            innerStyle->setTransformOriginY(Length(0, Fixed));

            RenderBlock* innerContainer = new (renderArena()) RenderMathMLBlock(toElement(node()));
            toRenderMathMLBlock(innerContainer)->setIgnoreInAccessibilityTree(true);
            innerContainer->setStyle(innerStyle);
            container->addChild(innerContainer);

            container = innerContainer;
        }

        // Build the text of the operator.
        RenderText* text = 0;
        if (m_operator) 
            text = new (renderArena()) RenderText(node(), StringImpl::create(&m_operator, 1));
        else if (node()->isElementNode())
            if (Element* mo = static_cast<Element*>(node()))
                text = new (renderArena()) RenderText(node(), mo->textContent().replace(hyphenMinus, minusSign).impl());
        // If we can't figure out the text, leave it blank.
        if (text) {
            RefPtr<RenderStyle> textStyle = RenderStyle::create();
            textStyle->inheritFrom(container->style());
            text->setStyle(textStyle.release());
            container->addChild(text);
        }
    } else {
        // Build stretchable characters as a stack of glyphs.
        m_isStretched = true;
        
        // To avoid gaps, we position glyphs after the top glyph upward by 1px. We also truncate
        // glyph heights to ints, and then reduce all but the top & bottom such heights by 1px.
        
        int remaining = m_stretchHeight - topGlyphHeight - bottomGlyphHeight;
        createGlyph(partsData->topGlyph, topGlyphHeight, 0);
        if (partsData->middleGlyph) {
            // We have a middle glyph (e.g. a curly bracket) that requires special processing.
            remaining -= middleGlyphHeight;
            int half = (remaining + 1) / 2;
            remaining -= half;
            while (remaining > 0) {
                int height = std::min<int>(remaining, extensionGlyphHeight);
                createGlyph(partsData->extensionGlyph, height, -1);
                remaining -= height;
            }
            
            // The middle glyph in the stack.
            createGlyph(partsData->middleGlyph, middleGlyphHeight, -1);
            
            remaining = half;
            while (remaining > 0) {
                int height = std::min<int>(remaining, extensionGlyphHeight);
                createGlyph(partsData->extensionGlyph, height, -1);
                remaining -= height;
            }
        } else {
            // We do not have a middle glyph and so we just extend from the top to the bottom glyph.
            while (remaining > 0) {
                int height = std::min<int>(remaining, extensionGlyphHeight);
                createGlyph(partsData->extensionGlyph, height, -1);
                remaining -= height;
            }
        }
        createGlyph(partsData->bottomGlyph, bottomGlyphHeight, -1);
    }
    
    setNeedsLayoutAndPrefWidthsRecalc();
}

PassRefPtr<RenderStyle> RenderMathMLOperator::createStackableStyle(int maxHeightForRenderer)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(style());
    newStyle->setDisplay(FLEX);
    
    newStyle->setMaxHeight(Length(maxHeightForRenderer, Fixed));
    
    newStyle->setOverflowY(OHIDDEN);
    newStyle->setOverflowX(OHIDDEN);

    return newStyle.release();
}

RenderBlock* RenderMathMLOperator::createGlyph(UChar glyph, int maxHeightForRenderer, int charRelative)
{
    RenderBlock* container = new (renderArena()) RenderMathMLBlock(toElement(node()));
    toRenderMathMLBlock(container)->setIgnoreInAccessibilityTree(true);
    container->setStyle(createStackableStyle(maxHeightForRenderer));
    addChild(container);
    RenderBlock* parent = container;
    if (charRelative) {
        RenderBlock* charBlock = createAnonymous(document());
        RefPtr<RenderStyle> charStyle = RenderStyle::create();
        charStyle->inheritFrom(container->style());
        charStyle->setDisplay(INLINE_BLOCK);
        charStyle->setTop(Length(charRelative, Fixed));
        charStyle->setPosition(RelativePosition);
        charBlock->setStyle(charStyle);
        parent->addChild(charBlock);
        parent = charBlock;
    }
    
    RenderText* text = new (renderArena()) RenderText(node(), StringImpl::create(&glyph, 1));
    text->setStyle(container->style());
    parent->addChild(text);
    return container;
}

int RenderMathMLOperator::firstLineBoxBaseline() const
{
    if (m_isStretched)
        return m_stretchHeight * 2 / 3 - (m_stretchHeight - static_cast<int>(m_stretchHeight / gOperatorExpansion)) / 2;    
    return RenderMathMLBlock::firstLineBoxBaseline();
}
    
}

#endif
