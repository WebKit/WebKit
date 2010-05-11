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

#ifndef SVGTextLayoutUtilities_h
#define SVGTextLayoutUtilities_h

#if ENABLE(SVG)
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatPoint;
class Font;
class InlineTextBox;
class RenderObject;
class RenderStyle;
class RenderSVGResource;
class SVGRenderStyle;
class TextRun;

struct SVGChar;
struct SVGCharacterLayoutInfo;
struct SVGInlineBoxCharacterRange;

enum SVGTextPaintSubphase {
    SVGTextPaintSubphaseBackground,
    SVGTextPaintSubphaseGlyphFill,
    SVGTextPaintSubphaseGlyphFillSelection,
    SVGTextPaintSubphaseGlyphStroke,
    SVGTextPaintSubphaseGlyphStrokeSelection,
    SVGTextPaintSubphaseForeground
};

struct SVGTextPaintInfo {
    SVGTextPaintInfo()
        : activePaintingResource(0)
        , subphase(SVGTextPaintSubphaseBackground)
    {
    }

    RenderSVGResource* activePaintingResource;
    SVGTextPaintSubphase subphase;
};

struct SVGLastGlyphInfo {
    SVGLastGlyphInfo()
        : isValid(false)
    {
    }

    bool isValid;
    String unicode;
    String glyphName;
};

bool isVerticalWritingMode(const SVGRenderStyle*);
float alignmentBaselineToShift(bool isVerticalText, const RenderObject* text, const Font&);
float glyphOrientationToAngle(const SVGRenderStyle*, bool isVerticalText, const UChar&);
float applyGlyphAdvanceAndShiftRespectingOrientation(bool isVerticalText, float orientationAngle, float glyphWidth, float glyphHeight, const Font&,
                                                     SVGChar&, float& xOrientationShift, float& yOrientationShift);
FloatPoint topLeftPositionOfCharacterRange(Vector<SVGChar>::iterator start, Vector<SVGChar>::iterator end);
float cummulatedWidthOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange&);
float cummulatedHeightOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange&);
TextRun svgTextRunForInlineTextBox(const UChar*, int length, const RenderStyle*, const InlineTextBox*, float xPos);

float calculateCSSKerning(const RenderStyle*);
bool applySVGKerning(SVGCharacterLayoutInfo&, const RenderStyle*, SVGLastGlyphInfo&, const String& unicodeString, const String& glyphName, bool isVerticalText);

}

#endif
#endif
