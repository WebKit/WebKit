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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef SVGCharacterLayoutInfo_h
#define SVGCharacterLayoutInfo_h

#if ENABLE(SVG)
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

#include "TransformationMatrix.h"
#include <wtf/RefCounted.h>
#include "SVGRenderStyle.h"
#include "SVGTextContentElement.h"

namespace WebCore {

class InlineBox;
class InlineFlowBox;
class SVGInlineTextBox;
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

// Holds extra data, when the character is laid out on a path
struct SVGCharOnPath : RefCounted<SVGCharOnPath> {
    static PassRefPtr<SVGCharOnPath> create() { return adoptRef(new SVGCharOnPath); }

    float xScale;
    float yScale;

    float xShift;
    float yShift;

    float orientationAngle;

    bool hidden : 1;
    
private:
    SVGCharOnPath()
        : xScale(1.0f)
        , yScale(1.0f)
        , xShift(0.0f)
        , yShift(0.0f)
        , orientationAngle(0.0f)
        , hidden(false)
    {
    }
};

struct SVGChar {
    SVGChar()
        : x(0.0f)
        , y(0.0f)
        , angle(0.0f)
        , orientationShiftX(0.0f)
        , orientationShiftY(0.0f)
        , pathData()
        , drawnSeperated(false)
        , newTextChunk(false)
    {
    }

    ~SVGChar()
    {
    }

    float x;
    float y;
    float angle;

    float orientationShiftX;
    float orientationShiftY;

    RefPtr<SVGCharOnPath> pathData;

    // Determines wheter this char needs to be drawn seperated
    bool drawnSeperated : 1;

    // Determines wheter this char starts a new chunk
    bool newTextChunk : 1;

    // Helper methods
    bool isHidden() const;
    TransformationMatrix characterTransform() const;
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

// Convenience typedef
typedef SVGTextContentElement::SVGLengthAdjustType ELengthAdjust;

struct SVGTextChunk {
    SVGTextChunk()
        : anchor(TA_START)
        , textLength(0.0f)
        , lengthAdjust(SVGTextContentElement::LENGTHADJUST_SPACING)
        , ctm()
        , isVerticalText(false)
        , isTextPath(false)
        , start(0)
        , end(0)
    { }

    // text-anchor support
    ETextAnchor anchor;

    // textLength & lengthAdjust support
    float textLength;
    ELengthAdjust lengthAdjust;
    TransformationMatrix ctm;

    // status flags
    bool isVerticalText : 1;
    bool isTextPath : 1;

    // main chunk data
    Vector<SVGChar>::iterator start;
    Vector<SVGChar>::iterator end;

    Vector<SVGInlineBoxCharacterRange> boxes;
};

struct SVGTextChunkWalkerBase {
    virtual ~SVGTextChunkWalkerBase() { }

    virtual void operator()(SVGInlineTextBox* textBox, int startOffset, const TransformationMatrix& chunkCtm,
                            const Vector<SVGChar>::iterator& start, const Vector<SVGChar>::iterator& end) = 0;

    // Followings methods are only used for painting text chunks
    virtual void start(InlineBox*) = 0;
    virtual void end(InlineBox*) = 0;
    
    virtual bool setupFill(InlineBox*) = 0;
    virtual bool setupStroke(InlineBox*) = 0;
};

template<typename CallbackClass>
struct SVGTextChunkWalker : public SVGTextChunkWalkerBase {
public:
    typedef void (CallbackClass::*SVGTextChunkWalkerCallback)(SVGInlineTextBox* textBox,
                                                              int startOffset,
                                                              const TransformationMatrix& chunkCtm,
                                                              const Vector<SVGChar>::iterator& start,
                                                              const Vector<SVGChar>::iterator& end);

    // These callbacks are only used for painting!
    typedef void (CallbackClass::*SVGTextChunkStartCallback)(InlineBox* box);
    typedef void (CallbackClass::*SVGTextChunkEndCallback)(InlineBox* box);

    typedef bool (CallbackClass::*SVGTextChunkSetupFillCallback)(InlineBox* box);
    typedef bool (CallbackClass::*SVGTextChunkSetupStrokeCallback)(InlineBox* box);

    SVGTextChunkWalker(CallbackClass* object,
                       SVGTextChunkWalkerCallback walker,
                       SVGTextChunkStartCallback start = 0,
                       SVGTextChunkEndCallback end = 0,
                       SVGTextChunkSetupFillCallback fill = 0,
                       SVGTextChunkSetupStrokeCallback stroke = 0)
        : m_object(object)
        , m_walkerCallback(walker)
        , m_startCallback(start)
        , m_endCallback(end)
        , m_setupFillCallback(fill)
        , m_setupStrokeCallback(stroke)
    {
        ASSERT(object);
        ASSERT(walker);
    }

    virtual void operator()(SVGInlineTextBox* textBox, int startOffset, const TransformationMatrix& chunkCtm,
                            const Vector<SVGChar>::iterator& start, const Vector<SVGChar>::iterator& end)
    {
        (*m_object.*m_walkerCallback)(textBox, startOffset, chunkCtm, start, end);
    }

    // Followings methods are only used for painting text chunks
    virtual void start(InlineBox* box)
    {
        if (m_startCallback)
            (*m_object.*m_startCallback)(box);
        else
            ASSERT_NOT_REACHED();
    }

    virtual void end(InlineBox* box)
    {
        if (m_endCallback)
            (*m_object.*m_endCallback)(box);
        else
            ASSERT_NOT_REACHED();
    }

    virtual bool setupFill(InlineBox* box)
    {
        if (m_setupFillCallback)
            return (*m_object.*m_setupFillCallback)(box);

        ASSERT_NOT_REACHED();
        return false;
    }

    virtual bool setupStroke(InlineBox* box)
    {
        if (m_setupStrokeCallback)
            return (*m_object.*m_setupStrokeCallback)(box);

        ASSERT_NOT_REACHED();
        return false;
    }

private:
    CallbackClass* m_object;
    SVGTextChunkWalkerCallback m_walkerCallback;
    SVGTextChunkStartCallback m_startCallback;
    SVGTextChunkEndCallback m_endCallback;
    SVGTextChunkSetupFillCallback m_setupFillCallback;
    SVGTextChunkSetupStrokeCallback m_setupStrokeCallback;
};

struct SVGTextChunkLayoutInfo {
    SVGTextChunkLayoutInfo(Vector<SVGTextChunk>& textChunks)
        : assignChunkProperties(true)
        , handlingTextPath(false)
        , svgTextChunks(textChunks)
        , it(0)
    {
    }

    bool assignChunkProperties : 1;
    bool handlingTextPath : 1;

    Vector<SVGTextChunk>& svgTextChunks;
    Vector<SVGChar>::iterator it;

    SVGTextChunk chunk;
};

struct SVGTextDecorationInfo {
    // ETextDecoration is meant to be used here
    HashMap<int, RenderObject*> fillServerMap;
    HashMap<int, RenderObject*> strokeServerMap;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGCharacterLayoutInfo_h
