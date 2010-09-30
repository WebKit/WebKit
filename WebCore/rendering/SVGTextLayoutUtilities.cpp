/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "SVGTextLayoutUtilities.h"

#if ENABLE(SVG)
#include "FloatPoint.h"
#include "InlineTextBox.h"
#include "RenderObject.h"
#include "SVGCharacterData.h"
#include "SVGCharacterLayoutInfo.h"
#include "SVGFontElement.h"
#include "SVGRenderStyle.h"
#include "SVGTextChunkLayoutInfo.h"
#include "TextRun.h"
#include "UnicodeRange.h"

#include <float.h>

namespace WebCore {

bool isVerticalWritingMode(const SVGRenderStyle* style)
{
    return style->writingMode() == WM_TBRL || style->writingMode() == WM_TB; 
}

static inline EAlignmentBaseline dominantBaselineToShift(bool isVerticalText, const RenderObject* text, const Font& font)
{
    ASSERT(text);

    const SVGRenderStyle* style = text->style() ? text->style()->svgStyle() : 0;
    ASSERT(style);

    const SVGRenderStyle* parentStyle = text->parent() && text->parent()->style() ? text->parent()->style()->svgStyle() : 0;

    EDominantBaseline baseline = style->dominantBaseline();
    if (baseline == DB_AUTO) {
        if (isVerticalText)
            baseline = DB_CENTRAL;
        else
            baseline = DB_ALPHABETIC;
    }

    switch (baseline) {
    case DB_USE_SCRIPT:
        // TODO: The dominant-baseline and the baseline-table components are set by
        //       determining the predominant script of the character data content.
        return AB_ALPHABETIC;
    case DB_NO_CHANGE:
        {
            if (parentStyle)
                return dominantBaselineToShift(isVerticalText, text->parent(), font);

            ASSERT_NOT_REACHED();
            return AB_AUTO;
        }
    case DB_RESET_SIZE:
        {
            if (parentStyle)
                return dominantBaselineToShift(isVerticalText, text->parent(), font);

            ASSERT_NOT_REACHED();
            return AB_AUTO;    
        }
    case DB_IDEOGRAPHIC:
        return AB_IDEOGRAPHIC;
    case DB_ALPHABETIC:
        return AB_ALPHABETIC;
    case DB_HANGING:
        return AB_HANGING;
    case DB_MATHEMATICAL:
        return AB_MATHEMATICAL;
    case DB_CENTRAL:
        return AB_CENTRAL;
    case DB_MIDDLE:
        return AB_MIDDLE;
    case DB_TEXT_AFTER_EDGE:
        return AB_TEXT_AFTER_EDGE;
    case DB_TEXT_BEFORE_EDGE:
        return AB_TEXT_BEFORE_EDGE;
    default:
        ASSERT_NOT_REACHED();
        return AB_AUTO;
    }
}

float alignmentBaselineToShift(bool isVerticalText, const RenderObject* text, const Font& font)
{
    ASSERT(text);

    const SVGRenderStyle* style = text->style() ? text->style()->svgStyle() : 0;
    ASSERT(style);

    const SVGRenderStyle* parentStyle = text->parent() && text->parent()->style() ? text->parent()->style()->svgStyle() : 0;

    EAlignmentBaseline baseline = style->alignmentBaseline();
    if (baseline == AB_AUTO) {
        if (parentStyle && style->dominantBaseline() == DB_AUTO)
            baseline = dominantBaselineToShift(isVerticalText, text->parent(), font);
        else
            baseline = dominantBaselineToShift(isVerticalText, text, font);

        ASSERT(baseline != AB_AUTO);    
    }

    // Note: http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
    switch (baseline) {
    case AB_BASELINE:
        {
            if (parentStyle)
                return dominantBaselineToShift(isVerticalText, text->parent(), font);

            return 0.0f;
        }
    case AB_BEFORE_EDGE:
    case AB_TEXT_BEFORE_EDGE:
        return font.ascent();
    case AB_MIDDLE:
        return font.xHeight() / 2.0f;
    case AB_CENTRAL:
        // Not needed, we're taking this into account already for vertical text!
        // return (font.ascent() - font.descent()) / 2.0f;
        return 0.0f;
    case AB_AFTER_EDGE:
    case AB_TEXT_AFTER_EDGE:
    case AB_IDEOGRAPHIC:
        return font.descent();
    case AB_ALPHABETIC:
        return 0.0f;
    case AB_HANGING:
        return font.ascent() * 8.0f / 10.0f;
    case AB_MATHEMATICAL:
        return font.ascent() / 2.0f;
    default:
        ASSERT_NOT_REACHED();
        return 0.0f;
    }
}

float glyphOrientationToAngle(const SVGRenderStyle* svgStyle, bool isVerticalText, const UChar& character)
{
    switch (isVerticalText ? svgStyle->glyphOrientationVertical() : svgStyle->glyphOrientationHorizontal()) {
    case GO_AUTO:
        {
            // Spec: Fullwidth ideographic and fullwidth Latin text will be set with a glyph-orientation of 0-degrees.
            // Text which is not fullwidth will be set with a glyph-orientation of 90-degrees.
            unsigned int unicodeRange = findCharUnicodeRange(character);
            if (unicodeRange == cRangeSetLatin || unicodeRange == cRangeArabic)
                return 90.0f;

            return 0.0f;
        }
    case GO_90DEG:
        return 90.0f;
    case GO_180DEG:
        return 180.0f;
    case GO_270DEG:
        return 270.0f;
    case GO_0DEG:
    default:
        return 0.0f;
    }
}

static inline bool glyphOrientationIsMultiplyOf180Degrees(float orientationAngle)
{
    return fabsf(fmodf(orientationAngle, 180.0f)) == 0.0f;
}

float applyGlyphAdvanceAndShiftRespectingOrientation(bool isVerticalText, float orientationAngle, float glyphWidth, float glyphHeight, const Font& font, SVGChar& svgChar, float& xOrientationShift, float& yOrientationShift)
{
    bool orientationIsMultiplyOf180Degrees = glyphOrientationIsMultiplyOf180Degrees(orientationAngle);

    // The function is based on spec requirements:
    //
    // Spec: If the 'glyph-orientation-horizontal' results in an orientation angle that is not a multiple of
    // of 180 degrees, then the current text position is incremented according to the vertical metrics of the glyph.
    //
    // Spec: If if the 'glyph-orientation-vertical' results in an orientation angle that is not a multiple of
    // 180 degrees,then the current text position is incremented according to the horizontal metrics of the glyph.

    // vertical orientation handling
    if (isVerticalText) {
        if (orientationAngle == 0.0f) {
            xOrientationShift = -glyphWidth / 2.0f;
            yOrientationShift = font.ascent();
        } else if (orientationAngle == 90.0f) {
            xOrientationShift = -glyphHeight;
            yOrientationShift = font.descent();
            svgChar.orientationShiftY = -font.ascent();
        } else if (orientationAngle == 270.0f) {
            xOrientationShift = glyphHeight;
            yOrientationShift = font.descent();
            svgChar.orientationShiftX = -glyphWidth;
            svgChar.orientationShiftY = -font.ascent();
        } else if (orientationAngle == 180.0f) {
            yOrientationShift = font.ascent();
            svgChar.orientationShiftX = -glyphWidth / 2.0f;
            svgChar.orientationShiftY = font.ascent() - font.descent();
        }

        // vertical advance calculation
        if (orientationAngle != 0.0f && !orientationIsMultiplyOf180Degrees)
            return glyphWidth;

        return glyphHeight; 
    }

    // horizontal orientation handling
    if (orientationAngle == 90.0f) {
        xOrientationShift = glyphWidth / 2.0f;
        yOrientationShift = -font.descent();
        svgChar.orientationShiftX = -glyphWidth / 2.0f - font.descent(); 
        svgChar.orientationShiftY = font.descent();
    } else if (orientationAngle == 270.0f) {
        xOrientationShift = -glyphWidth / 2.0f;
        yOrientationShift = -font.descent();
        svgChar.orientationShiftX = -glyphWidth / 2.0f + font.descent();
        svgChar.orientationShiftY = glyphHeight;
    } else if (orientationAngle == 180.0f) {
        xOrientationShift = glyphWidth / 2.0f;
        svgChar.orientationShiftX = -glyphWidth / 2.0f;
        svgChar.orientationShiftY = font.ascent() - font.descent();
    }

    // horizontal advance calculation
    if (orientationAngle != 0.0f && !orientationIsMultiplyOf180Degrees)
        return glyphHeight;

    return glyphWidth;
}

FloatPoint topLeftPositionOfCharacterRange(Vector<SVGChar>::iterator it, Vector<SVGChar>::iterator end)
{
    float lowX = FLT_MAX, lowY = FLT_MAX;
    for (; it != end; ++it) {
        if (it->isHidden())
            continue;

        float x = (*it).x;
        float y = (*it).y;

        if (x < lowX)
            lowX = x;

        if (y < lowY)
            lowY = y;
    }

    return FloatPoint(lowX, lowY);
}

float cummulatedWidthOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange& range)
{
    ASSERT(!range.isOpen());
    ASSERT(range.isClosed());
    ASSERT(range.box->isSVGInlineTextBox());

    InlineTextBox* textBox = static_cast<InlineTextBox*>(range.box);
    RenderText* text = textBox->textRenderer();
    RenderStyle* style = text->style();
    return style->font().floatWidth(svgTextRunForInlineTextBox(text->characters() + textBox->start() + range.startOffset, range.endOffset - range.startOffset, style, textBox));
}

float cummulatedHeightOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange& range)
{
    ASSERT(!range.isOpen());
    ASSERT(range.isClosed());
    ASSERT(range.box->isSVGInlineTextBox());

    InlineTextBox* textBox = static_cast<InlineTextBox*>(range.box);
    return (range.endOffset - range.startOffset) * textBox->textRenderer()->style()->font().height();
}

TextRun svgTextRunForInlineTextBox(const UChar* characters, int length, const RenderStyle* style, const InlineTextBox* textBox)
{
    ASSERT(textBox);
    ASSERT(style);

    TextRun run(characters
                , length
                , false /* allowTabs */
                , 0 /* xPos, only relevant with allowTabs=true */
                , 0 /* padding, only relevant for justified text, not relevant for SVG */
                , !textBox->isLeftToRightDirection()
                , textBox->m_dirOverride || style->visuallyOrdered() /* directionalOverride */);

#if ENABLE(SVG_FONTS)
    run.setReferencingRenderObject(textBox->textRenderer()->parent());
#endif

    // Disable any word/character rounding.
    run.disableRoundingHacks();

    // We handle letter & word spacing ourselves.
    run.disableSpacing();
    return run;
}

float calculateCSSKerning(SVGElement* context, const RenderStyle* style)
{
    const Font& font = style->font();
    const SVGRenderStyle* svgStyle = style->svgStyle();

    SVGLength kerningLength = svgStyle->kerning();
    if (kerningLength.unitType() == LengthTypePercentage)
        return kerningLength.valueAsPercentage() * font.pixelSize();

    return kerningLength.value(context);
}

bool applySVGKerning(SVGCharacterLayoutInfo& info, const RenderStyle* style, SVGLastGlyphInfo& lastGlyph, const String& unicodeString, const String& glyphName, bool isVerticalText)
{
#if ENABLE(SVG_FONTS)
    float kerning = 0.0f;

    const Font& font = style->font();
    if (!font.isSVGFont()) {
        lastGlyph.isValid = false;
        return false;
    }

    SVGFontElement* svgFont = font.svgFont();
    ASSERT(svgFont);

    if (lastGlyph.isValid) {
        if (isVerticalText)
            kerning = svgFont->verticalKerningForPairOfStringsAndGlyphs(lastGlyph.unicode, lastGlyph.glyphName, unicodeString, glyphName);
        else
            kerning = svgFont->horizontalKerningForPairOfStringsAndGlyphs(lastGlyph.unicode, lastGlyph.glyphName, unicodeString, glyphName);
    }

    lastGlyph.unicode = unicodeString;
    lastGlyph.glyphName = glyphName;
    lastGlyph.isValid = true;
    kerning *= style->font().size() / style->font().primaryFont()->unitsPerEm();

    if (kerning != 0.0f) {
        if (isVerticalText)
            info.cury -= kerning;
        else
            info.curx -= kerning;
        return true;
    }
#else
    UNUSED_PARAM(info);
    UNUSED_PARAM(item);
    UNUSED_PARAM(lastGlyph);
    UNUSED_PARAM(unicodeString);
    UNUSED_PARAM(glyphName);
#endif
    return false;
}

}

#endif
