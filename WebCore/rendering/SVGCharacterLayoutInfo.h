/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef SVGCharacterLayoutInfo_h
#define SVGCharacterLayoutInfo_h

#if ENABLE(SVG)

#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/Assertions.h>

#include "AffineTransform.h"
#include "SVGRenderStyle.h"

namespace WebCore {

class InlineBox;
class InlineFlowBox;
class SVGLengthList;
class SVGNumberList;
class SVGTextPositioningElement;

template<class Type>
class PositionedVector : public Vector<Type>
{
public:
    PositionedVector<Type>()
        : m_position(0)
    {
    }

    unsigned position() const
    {
        return m_position;
    }

    void advance(unsigned position)
    {
        m_position += position;
        ASSERT(m_position < Vector<Type>::size());
    }

    Type valueAtCurrentPosition() const
    {
        ASSERT(m_position < Vector<Type>::size());
        return Vector<Type>::at(m_position);
    }

private:
    unsigned m_position;
};

class PositionedFloatVector : public PositionedVector<float> { };

struct SVGCharacterLayoutInfo {
    SVGCharacterLayoutInfo();

    enum StackType { XStack, YStack, DxStack, DyStack, AngleStack, BaselineShiftStack };

    bool xValueAvailable() const;
    bool yValueAvailable() const;
    bool dxValueAvailable() const;
    bool dyValueAvailable() const;
    bool angleValueAvailable() const;
    bool baselineShiftValueAvailable() const;

    float xValueNext() const;
    float yValueNext() const;
    float dxValueNext() const;
    float dyValueNext() const;
    float angleValueNext() const;
    float baselineShiftValueNext() const;

    void processedChunk(float lastShiftX, float lastShiftY);
    void processedSingleCharacter();

    bool nextPathLayoutPointAndAngle(float& x, float& y, float& angle, float glyphAdvance, float newOffset);

    // Used for text-on-path.
    void addLayoutInformation(InlineFlowBox*, float textAnchorOffset = 0.0);

    bool inPathLayout() const;
    void setInPathLayout(bool value);

    // Used for anything else.
    void addLayoutInformation(SVGTextPositioningElement*);

    // Global position
    float curx;
    float cury;

    // Accumulated dx/dy values
    float dx;
    float dy;

    // Accumulated applied shift values
    float shiftx;
    float shifty;

private:
    // Used for baseline-shift.
    void addStackContent(StackType, float);

    // Used for angle.
    void addStackContent(StackType, SVGNumberList*);

    // Used for x/y/dx/dy.    
    void addStackContent(StackType, SVGLengthList*);

    void addStackContent(StackType, const PositionedFloatVector&);

    void xStackWalk();
    void yStackWalk();
    void dxStackWalk();
    void dyStackWalk();
    void angleStackWalk();
    void baselineShiftStackWalk();

private:
    bool xStackChanged : 1;
    bool yStackChanged : 1;
    bool dxStackChanged : 1;
    bool dyStackChanged : 1;
    bool angleStackChanged : 1;
    bool baselineShiftStackChanged : 1;

    // text on path layout
    bool pathLayout : 1;
    float currentOffset;
    float startOffset;
    float layoutPathLength;
    Path layoutPath;

    Vector<PositionedFloatVector> xStack;
    Vector<PositionedFloatVector> yStack;
    Vector<PositionedFloatVector> dxStack;
    Vector<PositionedFloatVector> dyStack;
    Vector<PositionedFloatVector> angleStack;
    Vector<float> baselineShiftStack;
};

struct SVGChar {
    float x;
    float y;

    AffineTransform transform;

    // Determines wheter this char is visible (ie. false for chars "off" the text layout path)
    bool visible : 1;

    // Determines wheter this char needs to be drawn seperated
    bool drawnSeperated : 1;

    // Determines wheter this char starts a new chunk
    bool newTextChunk : 1;
};

struct SVGInlineBoxCharacterRange {
    SVGInlineBoxCharacterRange()
        : startOffset(INT_MIN)
        , endOffset(INT_MIN)
        , box(0)
    {
    }

    bool isOpen() const { return (startOffset == endOffset) && (endOffset == INT_MIN); }
    bool isClosed() const { return startOffset != INT_MIN && endOffset != INT_MIN; }

    int startOffset;
    int endOffset;

    InlineBox* box;
};

struct SVGTextChunk {
    SVGTextChunk()
        : anchor(TA_START)
        , isVerticalText(false)
        , start(0)
        , end(0)
    { }

    ETextAnchor anchor;
    bool isVerticalText : 1;

    Vector<SVGChar>::iterator start;
    Vector<SVGChar>::iterator end;

    Vector<SVGInlineBoxCharacterRange> boxes;
};

struct SVGTextChunkLayoutInfo {
    SVGTextChunkLayoutInfo()
        : assignChunkProperties(true)
        , advanceOnly(false)
    {
    }

    bool assignChunkProperties : 1;
    bool advanceOnly : 1;

    SVGTextChunk chunk;
    Vector<SVGChar>::iterator it;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGCharacterLayoutInfo_h
