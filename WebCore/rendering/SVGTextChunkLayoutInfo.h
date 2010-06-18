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
#include "SVGCharacterData.h"
#include "SVGRenderStyle.h"
#include "SVGTextContentElement.h"

#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

class InlineBox;
class InlineFlowBox;
class SVGInlineTextBox;

// A SVGTextChunk directly corresponds to the definition of a "text chunk" per SVG 1.1 specification
// For example, each absolute positioned character starts a text chunk (much more to respect, see spec).
// Each SVGTextChunk contains a Vector of SVGInlineBoxCharacterRange, describing how many boxes are spanned
// by this chunk. Following two examples should clarify the code a bit:
//
// 1. <text x="10 20 30">ABC</text> - one InlineTextBox is created, three SVGTextChunks each with one SVGInlineBoxCharaterRange
// [SVGTextChunk 1]
//    [SVGInlineBoxCharacterRange 1, startOffset=0, endOffset=1, box=0x1]
// [SVGTextChunk 2]
//    [SVGInlineBoxCharacterRange 1, startOffset=1, endOffset=2, box=0x1]
// [SVGTextChunk 3]
//    [SVGInlineBoxCharacterRange 1, startOffset=2, endOffset=3, box=0x1]
//
// 2. <text x="10">A<tspan>B</tspan>C</text> - three InlineTextBoxs are created, one SVGTextChunk, with three SVGInlineBoxCharacterRanges
// [SVGTextChunk 1]
//    [SVGInlineBoxCharacterRange 1, startOffset=0, endOffset=1, box=0x1]
//    [SVGInlineBoxCharacterRange 2, startOffset=0, endOffset=1, box=0x2]
//    [SVGInlineBoxCharacterRange 3, startOffset=0, endOffset=1, box=0x3]
//
// High level overview of the SVG text layout code:
// Step #1) - Build Vector of SVGChar objects starting from root <text> diving into children
// Step #2) - Build Vector of SVGTextChunk objects, containing offsets into the InlineTextBoxes and SVGChar vectors
// Step #3) - Apply chunk post processing (text-anchor / textLength support, which operate on text chunks!)
// Step #4) - Propagate information, how many chunk "parts" are associated with each SVGInlineTextBox (see below)
// Step #5) - Layout all InlineBoxes, only by measuring their context rect (x/y/width/height defined through SVGChars and transformations)
// Step #6) - Layout SVGRootInlineBox, it's parent RenderSVGText block and fixup child positions, to be relative to the root box
//
// When painting a range of characters, we have to determine how many can be drawn in a row. Each absolute postioned
// character is drawn individually. After step #2) we know all text chunks, and how they span across the SVGInlineTextBoxes.
// In step #4) we build a list of text chunk "parts" and store it in each SVGInlineTextBox. A chunk "part" is a part of a
// text chunk that lives in a SVGInlineTextBox (consists of a length, width, height and a monotonic offset from the chunk begin)
// The SVGTextChunkPart object describes this information.
// When painting we can follow the regular InlineBox flow, we start painting the SVGRootInlineBox, which just asks its children
// to paint. They can paint on their own because all position information are known. Previously we used to draw _all_ characters
// from the SVGRootInlineBox, which violates the whole concept of the multiple InlineBoxes, and made text selection very hard to
// implement.

struct SVGTextChunkPart {
    SVGTextChunkPart()
        : offset(-1)
        , length(-1)
        , width(0)
        , height(0)
    {
    }

    bool isValid() const
    {
        return offset != -1
            && length != -1
            && width
            && height;
    }

    // First character of this text chunk part, defining the origin to be drawn
    Vector<SVGChar>::const_iterator firstCharacter;

    // Start offset in textRenderer()->characters() buffer.
    int offset;

    // length/width/height of chunk part
    int length;
    float width;
    float height;
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

struct SVGChar;

// Convenience typedef
typedef SVGTextContentElement::SVGLengthAdjustType ELengthAdjust;

struct SVGTextChunk {
    SVGTextChunk(Vector<SVGChar>::iterator it)
        : anchor(TA_START)
        , textLength(0.0f)
        , lengthAdjust(SVGTextContentElement::LENGTHADJUST_SPACING)
        , isVerticalText(false)
        , isTextPath(false)
        , start(it)
        , end(it)
    {
    }

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

struct SVGTextChunkLayoutInfo {
    SVGTextChunkLayoutInfo()
        : m_assignChunkProperties(true)
        , m_handlingTextPath(false)
        , m_charsIt(0)
        , m_charsBegin(0)
        , m_charsEnd(0)
        , m_chunk(0)
    {
    }

    const Vector<SVGTextChunk>& textChunks() const { return m_svgTextChunks; }

    void buildTextChunks(Vector<SVGChar>::iterator charsBegin, Vector<SVGChar>::iterator charsEnd, InlineFlowBox* start);
    void layoutTextChunks();

private:
    void startTextChunk();
    void closeTextChunk();
    void recursiveBuildTextChunks(InlineFlowBox* start);

    bool m_assignChunkProperties : 1;
    bool m_handlingTextPath : 1;

    Vector<SVGChar>::iterator m_charsIt;
    Vector<SVGChar>::iterator m_charsBegin;
    Vector<SVGChar>::iterator m_charsEnd;

    Vector<SVGTextChunk> m_svgTextChunks;
    SVGTextChunk m_chunk;
};

// Helper functions
float calculateTextAnchorShiftForTextChunk(SVGTextChunk&, ETextAnchor);
float calculateTextLengthCorrectionForTextChunk(SVGTextChunk&, ELengthAdjust, float& computedLength);

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGTextChunkLayoutInfo_h
