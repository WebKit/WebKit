/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 *
 */

#ifndef SVGTextChunkLayoutInfo_h
#define SVGTextChunkLayoutInfo_h

#if ENABLE(SVG)
#include "AffineTransform.h"
#include "SVGTextContentElement.h"

#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

class InlineBox;
class SVGInlineTextBox;

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

struct SVGChar;

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
    AffineTransform ctm;

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

    virtual void operator()(SVGInlineTextBox* textBox, int startOffset, const AffineTransform& chunkCtm,
                            const Vector<SVGChar>::iterator& start, const Vector<SVGChar>::iterator& end) = 0;

    // Followings methods are only used for painting text chunks
    virtual void start(InlineBox*) = 0;
    virtual void end(InlineBox*) = 0;
};

template<typename CallbackClass>
struct SVGTextChunkWalker : public SVGTextChunkWalkerBase {
public:
    typedef void (CallbackClass::*SVGTextChunkWalkerCallback)(SVGInlineTextBox* textBox,
                                                              int startOffset,
                                                              const AffineTransform& chunkCtm,
                                                              const Vector<SVGChar>::iterator& start,
                                                              const Vector<SVGChar>::iterator& end);

    // These callbacks are only used for painting!
    typedef void (CallbackClass::*SVGTextChunkStartCallback)(InlineBox* box);
    typedef void (CallbackClass::*SVGTextChunkEndCallback)(InlineBox* box);

    SVGTextChunkWalker(CallbackClass* object, 
                       SVGTextChunkWalkerCallback walker,
                       SVGTextChunkStartCallback start = 0,
                       SVGTextChunkEndCallback end = 0)
        : m_object(object)
        , m_walkerCallback(walker)
        , m_startCallback(start)
        , m_endCallback(end)
    {
        ASSERT(object);
        ASSERT(walker);
    }

    virtual void operator()(SVGInlineTextBox* textBox, int startOffset, const AffineTransform& chunkCtm,
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

private:
    CallbackClass* m_object;
    SVGTextChunkWalkerCallback m_walkerCallback;
    SVGTextChunkStartCallback m_startCallback;
    SVGTextChunkEndCallback m_endCallback;
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

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGTextChunkLayoutInfo_h
