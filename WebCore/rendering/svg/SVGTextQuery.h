/*
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

#ifndef SVGTextQuery_h
#define SVGTextQuery_h

#if ENABLE(SVG)
#include "FloatRect.h"
#include <wtf/Vector.h>

namespace WebCore {

class InlineFlowBox;
class RenderObject;
class RenderStyle;
class SVGInlineTextBox;
struct SVGTextChunkPart;

class SVGTextQuery {
public:
    SVGTextQuery(RenderObject*);

    unsigned numberOfCharacters() const;
    float textLength() const;
    float subStringLength(unsigned startPosition, unsigned length) const;
    FloatPoint startPositionOfCharacter(unsigned position) const;
    FloatPoint endPositionOfCharacter(unsigned position) const;
    float rotationOfCharacter(unsigned position) const;
    FloatRect extentOfCharacter(unsigned position) const;
    int characterNumberAtPosition(const FloatPoint&) const;

    // Public helper struct. Private classes in SVGTextQuery inherit from it.
    struct Data;

private:
    typedef bool (SVGTextQuery::*ProcessTextChunkPartCallback)(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool executeQuery(Data*, ProcessTextChunkPartCallback) const;

    void collectTextBoxesInFlowBox(InlineFlowBox*);
    float measureCharacterRange(const SVGInlineTextBox*, RenderStyle*, bool isVerticalText, int startPosition, int length) const;
    bool mapStartAndLengthIntoChunkPartCoordinates(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&, int& startPosition, int& endPosition) const;

private:
    bool numberOfCharactersCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool textLengthCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool subStringLengthCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool startPositionOfCharacterCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool endPositionOfCharacterCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool rotationOfCharacterCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool extentOfCharacterCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;
    bool characterNumberAtPositionCallback(Data*, const SVGInlineTextBox*, const SVGTextChunkPart&) const;

private:
    Vector<SVGInlineTextBox*> m_textBoxes;
};

}

#endif
#endif
