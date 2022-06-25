/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "FloatRect.h"
#include "SVGTextFragment.h"
#include <wtf/Vector.h>

namespace WebCore {

class LegacyInlineFlowBox;
class RenderObject;
class SVGInlineTextBox;

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
    typedef bool (SVGTextQuery::*ProcessTextFragmentCallback)(Data*, const SVGTextFragment&) const;
    bool executeQuery(Data*, ProcessTextFragmentCallback) const;

    void collectTextBoxesInFlowBox(LegacyInlineFlowBox*);
    bool mapStartEndPositionsIntoFragmentCoordinates(Data*, const SVGTextFragment&, unsigned& startPosition, unsigned& endPosition) const;
    void modifyStartEndPositionsRespectingLigatures(Data*, unsigned& startPosition, unsigned& endPosition) const;

private:
    bool numberOfCharactersCallback(Data*, const SVGTextFragment&) const;
    bool textLengthCallback(Data*, const SVGTextFragment&) const;
    bool subStringLengthCallback(Data*, const SVGTextFragment&) const;
    bool startPositionOfCharacterCallback(Data*, const SVGTextFragment&) const;
    bool endPositionOfCharacterCallback(Data*, const SVGTextFragment&) const;
    bool rotationOfCharacterCallback(Data*, const SVGTextFragment&) const;
    bool extentOfCharacterCallback(Data*, const SVGTextFragment&) const;
    bool characterNumberAtPositionCallback(Data*, const SVGTextFragment&) const;

private:
    Vector<SVGInlineTextBox*> m_textBoxes;
};

} // namespace WebCore
