/*
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef SVGCharacterLayoutInfo_h
#define SVGCharacterLayoutInfo_h

#if ENABLE(SVG)
#include "Path.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

class InlineFlowBox;
class SVGElement;
class SVGLengthList;
class SVGNumberList;
class SVGTextPositioningElement;

template<class Type>
class PositionedVector : public Vector<Type> {
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
struct SVGChar;

struct SVGCharacterLayoutInfo {
    SVGCharacterLayoutInfo(Vector<SVGChar>&);

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

    void processedChunk(float savedShiftX, float savedShiftY);
    void processedSingleCharacter();

    bool nextPathLayoutPointAndAngle(float glyphAdvance, float extraAdvance, float newOffset);

    // Used for text-on-path.
    void addLayoutInformation(InlineFlowBox*, float textAnchorOffset = 0.0f);

    bool inPathLayout() const;
    void setInPathLayout(bool value);

    // Used for anything else.
    void addLayoutInformation(SVGTextPositioningElement*);

    // Global position
    float curx;
    float cury;

    // Global rotation
    float angle;

    // Accumulated dx/dy values
    float dx;
    float dy;

    // Accumulated baseline-shift values
    float shiftx;
    float shifty;

    // Path specific advance values to handle lengthAdjust
    float pathExtraAdvance;
    float pathTextLength;
    float pathChunkLength;

    // Result vector
    Vector<SVGChar>& svgChars;
    bool nextDrawnSeperated : 1;

private:
    // Used for baseline-shift.
    void addStackContent(StackType, float);

    // Used for angle.
    void addStackContent(StackType, SVGNumberList*);

    // Used for x/y/dx/dy.    
    void addStackContent(StackType, SVGLengthList*, const SVGElement*);

    void addStackContent(StackType, const PositionedFloatVector&);

    void xStackWalk();
    void yStackWalk();
    void dxStackWalk();
    void dyStackWalk();
    void angleStackWalk();
    void baselineShiftStackWalk();

    bool isInitialLayout() const;

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

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGCharacterLayoutInfo_h
