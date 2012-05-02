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

#include "FontSelector.h"
#include "MathMLNames.h"
#include "RenderText.h"

namespace WebCore {
    
using namespace MathMLNames;

RenderMathMLOperator::RenderMathMLOperator(Element* element)
    : RenderMathMLBlock(element)
    , m_stretchHeight(0)
    , m_operator(0)
{
}

RenderMathMLOperator::RenderMathMLOperator(Node* node, UChar operatorChar)
    : RenderMathMLBlock(node)
    , m_stretchHeight(0)
    , m_operator(convertHyphenMinusToMinusSign(operatorChar))
{
}

bool RenderMathMLOperator::isChildAllowed(RenderObject*, RenderStyle*) const
{
    return false;
}

static const float gOperatorSpacer = 0.1f;
static const float gOperatorExpansion = 1.2f;

void RenderMathMLOperator::stretchToHeight(int height)
{
    height *= gOperatorExpansion;
    if (m_stretchHeight == height)
        return;
    m_stretchHeight = height;
    
    updateFromElement();
}

void RenderMathMLOperator::computePreferredLogicalWidths() 
{
    ASSERT(preferredLogicalWidthsDirty());
    
    // Check for an uninitialized operator.
    if (!firstChild())
        updateFromElement();
    
    RenderMathMLBlock::computePreferredLogicalWidths();
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
    { 0x7c  , 0x23d0, 0x23d0, 0x23d0, 0x0    }, // vertical bar
    { 0x2016, 0x2016, 0x2016, 0x2016, 0x0    }, // double vertical line
    { 0x7d  , 0x23ab, 0x23aa, 0x23ad, 0x23ac }, // right curly bracket
    { 0x222b, 0x2320, 0x23ae, 0x2321, 0x0    } // integral sign
};

// We stack glyphs using a 14px height with a displayed glyph height
// of 10px.  The line height is set to less than the 14px so that there
// are no blank spaces between the stacked glyphs.
//
// Certain glyphs (e.g. middle and bottom) need to be adjusted upwards
// in the stack so that there isn't a gap.
//
// All of these settings are represented in the constants below.

// FIXME: use fractions of style()->fontSize() for proper zooming/resizing.
// FIXME: None of this should be hard-coded. Not only does this cause problems
// with zooming, but it also sets up assumptions that break when a font contains
// a glyph that is smaller than expected. For example, the STIX vertical bar
// glyph is smaller than expected, and glyphHeightForCharacter() and
// lineHeightForCharacter() were added to make smaller glyphs work in this system.
// Really, the system should just be reconsidered.
static const int gGlyphFontSize = 14;
static const int gGlyphLineHeight = 11;
static const int gMinimumStretchHeight = 24;
static const int gGlyphHeight = 10;
static const int gTopGlyphTopAdjust = 1;
static const int gMiddleGlyphTopAdjust = -1;
static const int gBottomGlyphTopAdjust = -3;
static const float gMinimumRatioForStretch = 0.10f;

// This should always be called instead of accessing gGlyphHeight directly.
int RenderMathMLOperator::glyphHeightForCharacter(UChar character)
{
    GlyphData data = style()->font().glyphDataForCharacter(character, false);
    FloatRect glyphBounds = data.fontData->boundsForGlyph(data.glyph);
    if (glyphBounds.height() && glyphBounds.height() < gGlyphHeight)
        return glyphBounds.height();
    return gGlyphHeight;
}

// This should always be called instead of accessing gGlyphLineHeight directly.
int RenderMathMLOperator::lineHeightForCharacter(UChar character)
{
    int glyphHeight = glyphHeightForCharacter(character);
    if (glyphHeight < gGlyphHeight)
        return (glyphHeight - 1) > 0 ? glyphHeight - 1 : 1;
    return gGlyphLineHeight;
}

void RenderMathMLOperator::updateFromElement()
{
    RenderObject* savedRenderer = node()->renderer();

    // Destroy our current children
    children()->destroyLeftoverChildren();

    // Since we share a node with our children, destroying our children may set our node's
    // renderer to 0, so we need to restore it.
    node()->setRenderer(savedRenderer);
    
    // If the operator is fixed, it will be contained in m_operator
    UChar firstChar = m_operator;
    
    // This boolean indicates whether stretching is disabled via the markup.
    bool stretchDisabled = false;
    
    // We may need the element later if we can't stretch.
    if (node()->nodeType() == Node::ELEMENT_NODE) {
        if (Element* mo = static_cast<Element*>(node())) {
            AtomicString stretchyAttr = mo->getAttribute(MathMLNames::stretchyAttr);
            stretchDisabled = equalIgnoringCase(stretchyAttr, "false");
            
            // If stretching isn't disabled, get the character from the text content.
            if (!stretchDisabled && !firstChar) {
                String opText = mo->textContent();
                for (unsigned int i = 0; !firstChar && i < opText.length(); i++) {
                    if (!isSpaceOrNewline(opText[i]))
                        firstChar = opText[i];
                }
            }
        }
    }
    
    // The 'index' holds the stretchable character's glyph information
    int index = -1;
    
    // isStretchy indicates whether the character is streatchable via a number of factors.
    bool isStretchy = false;
    
    // Check for a stretchable character.
    if (!stretchDisabled && firstChar) {
        const int maxIndex = WTF_ARRAY_LENGTH(stretchyCharacters);
        for (index++; index < maxIndex; index++) {
            if (stretchyCharacters[index].character == firstChar) {
                isStretchy = true;
                break;
            }
        }
    }
    
    // We only stretch character if the stretch height is larger than a minimum size (e.g. 24px).
    bool shouldStretch = isStretchy && m_stretchHeight>gMinimumStretchHeight;
    
    // Either stretch is disabled or we don't have a stretchable character over the minimum height
    if (stretchDisabled || !shouldStretch) {
        m_isStacked = false;
        RenderBlock* container = new (renderArena()) RenderMathMLBlock(node());
        
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(INLINE_BLOCK);
        newStyle->setVerticalAlign(BASELINE);
        
        // Check for a stretchable character that is under the minimum height and use the
        // font size to adjust the glyph size.
        int currentFontSize = style()->fontSize();
        if (!stretchDisabled && isStretchy && m_stretchHeight > 0 && m_stretchHeight <= gMinimumStretchHeight  && m_stretchHeight > currentFontSize) {
            FontDescription desc = style()->fontDescription();
            desc.setIsAbsoluteSize(true);
            desc.setSpecifiedSize(m_stretchHeight);
            desc.setComputedSize(m_stretchHeight);
            newStyle->setFontDescription(desc);
            newStyle->font().update(style()->font().fontSelector());
        }

        container->setStyle(newStyle.release());
        addChild(container);
        
        // Build the text of the operator.
        RenderText* text = 0;
        if (m_operator) 
            text = new (renderArena()) RenderText(node(), StringImpl::create(&m_operator, 1));
        else if (node()->nodeType() == Node::ELEMENT_NODE)
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
        m_isStacked = true;
    
        int extensionGlyphLineHeight = lineHeightForCharacter(stretchyCharacters[index].extensionGlyph);
        int topGlyphLineHeight = lineHeightForCharacter(stretchyCharacters[index].topGlyph);
        int bottomGlyphLineHeight = lineHeightForCharacter(stretchyCharacters[index].bottomGlyph);
        
        if (stretchyCharacters[index].middleGlyph) {
            // We have a middle glyph (e.g. a curly bracket) that requires special processing.
            int glyphHeight = glyphHeightForCharacter(stretchyCharacters[index].middleGlyph);
            int middleGlyphLineHeight = lineHeightForCharacter(stretchyCharacters[index].middleGlyph);
            int half = (m_stretchHeight - glyphHeight) / 2;
            if (half <= glyphHeight) {
                // We only have enough space for a single middle glyph.
                createGlyph(stretchyCharacters[index].topGlyph, topGlyphLineHeight, half, gTopGlyphTopAdjust);
                createGlyph(stretchyCharacters[index].middleGlyph, middleGlyphLineHeight, glyphHeight, gMiddleGlyphTopAdjust);
                createGlyph(stretchyCharacters[index].bottomGlyph, bottomGlyphLineHeight, 0, gBottomGlyphTopAdjust);
            } else {
                // We have to extend both the top and bottom to the middle.
                createGlyph(stretchyCharacters[index].topGlyph, topGlyphLineHeight, glyphHeight, gTopGlyphTopAdjust);
                int remaining = half - glyphHeight;
                while (remaining > 0) {
                    if (remaining < glyphHeight) {
                        createGlyph(stretchyCharacters[index].extensionGlyph, extensionGlyphLineHeight, remaining);
                        remaining = 0;
                    } else {
                        createGlyph(stretchyCharacters[index].extensionGlyph, extensionGlyphLineHeight, glyphHeight);
                        remaining -= glyphHeight;
                    }
                }
                
                // The middle glyph in the stack.
                createGlyph(stretchyCharacters[index].middleGlyph, middleGlyphLineHeight, glyphHeight, gMiddleGlyphTopAdjust);
                
                // The remaining is the top half minus the middle glyph height.
                remaining = half - glyphHeight;
                // We need to make sure we have the full height in case the height is odd.
                if (m_stretchHeight % 2 == 1)
                    remaining++;
                
                // Extend to the bottom glyph.
                while (remaining > 0) {
                    if (remaining < glyphHeight) {
                        createGlyph(stretchyCharacters[index].extensionGlyph, extensionGlyphLineHeight, remaining);
                        remaining = 0;
                    } else {
                        createGlyph(stretchyCharacters[index].extensionGlyph, extensionGlyphLineHeight, glyphHeight);
                        remaining -= glyphHeight;
                    }
                }
                
                // The bottom glyph in the stack.
                createGlyph(stretchyCharacters[index].bottomGlyph, bottomGlyphLineHeight, 0, gBottomGlyphTopAdjust);
            }
        } else {
            // We do not have a middle glyph and so we just extend from the top to the bottom glyph.
            int glyphHeight = glyphHeightForCharacter(stretchyCharacters[index].extensionGlyph);
            int remaining = m_stretchHeight - 2 * glyphHeight;
            createGlyph(stretchyCharacters[index].topGlyph, topGlyphLineHeight, glyphHeight, gTopGlyphTopAdjust);
            while (remaining > 0) {
                if (remaining < glyphHeight) {
                    createGlyph(stretchyCharacters[index].extensionGlyph, extensionGlyphLineHeight, remaining);
                    remaining = 0;
                } else {
                    createGlyph(stretchyCharacters[index].extensionGlyph, extensionGlyphLineHeight, glyphHeight);
                    remaining -= glyphHeight;
                }
            }
            createGlyph(stretchyCharacters[index].bottomGlyph, bottomGlyphLineHeight, 0, gBottomGlyphTopAdjust);
        }
    }
    
    setNeedsLayoutAndPrefWidthsRecalc();
}

PassRefPtr<RenderStyle> RenderMathMLOperator::createStackableStyle(int lineHeight, int maxHeightForRenderer, int topRelative)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(style());
    newStyle->setDisplay(BLOCK);
    
    FontDescription desc = style()->fontDescription();
    desc.setIsAbsoluteSize(true);
    desc.setSpecifiedSize(gGlyphFontSize);
    desc.setComputedSize(gGlyphFontSize);
    newStyle->setFontDescription(desc);
    newStyle->font().update(style()->font().fontSelector());
    newStyle->setLineHeight(Length(lineHeight, Fixed));
    newStyle->setVerticalAlign(TOP);

    if (maxHeightForRenderer > 0)
        newStyle->setMaxHeight(Length(maxHeightForRenderer, Fixed));
    
    newStyle->setOverflowY(OHIDDEN);
    newStyle->setOverflowX(OHIDDEN);
    if (topRelative) {
        newStyle->setTop(Length(topRelative, Fixed));
        newStyle->setPosition(RelativePosition);
    }

    return newStyle.release();
}

RenderBlock* RenderMathMLOperator::createGlyph(UChar glyph, int lineHeight, int maxHeightForRenderer, int charRelative, int topRelative)
{
    RenderBlock* container = new (renderArena()) RenderMathMLBlock(node());
    container->setStyle(createStackableStyle(lineHeight, maxHeightForRenderer, topRelative));
    addChild(container);
    RenderBlock* parent = container;
    if (charRelative) {
        RenderBlock* charBlock = new (renderArena()) RenderBlock(node());
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

LayoutUnit RenderMathMLOperator::baselinePosition(FontBaseline, bool firstLine, LineDirectionMode lineDirection, LinePositionMode linePositionMode) const
{
    if (m_isStacked)
        return m_stretchHeight * 2 / 3 - (m_stretchHeight - static_cast<int>(m_stretchHeight / gOperatorExpansion)) / 2;    
    return RenderBlock::baselinePosition(AlphabeticBaseline, firstLine, lineDirection, linePositionMode);
}
    
}

#endif
