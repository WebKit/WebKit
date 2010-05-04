/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGRootInlineBox_h
#define SVGRootInlineBox_h

#if ENABLE(SVG)
#include "RootInlineBox.h"
#include "SVGCharacterData.h"
#include "SVGCharacterLayoutInfo.h"
#include "SVGTextChunkLayoutInfo.h"
#include "SVGRenderSupport.h"

namespace WebCore {

class InlineTextBox;
class RenderSVGRoot;
class SVGInlineTextBox;

struct LastGlyphInfo {
    LastGlyphInfo() : isValid(false) { }

    String unicode;
    String glyphName;
    bool isValid;
};

class SVGRootInlineBox : public RootInlineBox, protected SVGRenderBase {
public:
    SVGRootInlineBox(RenderObject* obj)
        : RootInlineBox(obj)
        , m_height(0)
    {
    }

    virtual bool isSVGRootInlineBox() { return true; }

    virtual int virtualHeight() const { return m_height; }
    void setHeight(int h) { m_height = h; }
    
    virtual void paint(RenderObject::PaintInfo&, int tx, int ty);

    virtual int placeBoxesHorizontally(int x, int& leftPosition, int& rightPosition, bool& needsWordSpacing);
    virtual int verticallyAlignBoxes(int heightOfBlock);

    virtual void computePerCharacterLayoutInformation();

    virtual FloatRect objectBoundingBox() const { return FloatRect(); }
    virtual FloatRect repaintRectInLocalCoordinates() const { return FloatRect(); }

    // Used by SVGInlineTextBox
    const Vector<SVGTextChunk>& svgTextChunks() const;

    void walkTextChunks(SVGTextChunkWalkerBase*, const SVGInlineTextBox* textBox = 0);

private:
    friend struct SVGRootInlineBoxPaintWalker;

    void layoutInlineBoxes();
    void layoutInlineBoxes(InlineFlowBox* start, Vector<SVGChar>::iterator& it, int& minX, int& maxX, int& minY, int& maxY);

    void buildLayoutInformation(InlineFlowBox* start, SVGCharacterLayoutInfo&);
    void buildLayoutInformationForTextBox(SVGCharacterLayoutInfo&, InlineTextBox*, LastGlyphInfo&);

    void buildTextChunks(Vector<SVGChar>&, Vector<SVGTextChunk>&, InlineFlowBox* start);
    void buildTextChunks(Vector<SVGChar>&, InlineFlowBox* start, SVGTextChunkLayoutInfo&);
    void layoutTextChunks();

    SVGTextDecorationInfo retrievePaintServersForTextDecoration(RenderObject* start);

private:
    int m_height;
    Vector<SVGChar> m_svgChars;
    Vector<SVGTextChunk> m_svgTextChunks;
};

// Shared with SVGRenderTreeAsText / SVGInlineTextBox
TextRun svgTextRunForInlineTextBox(const UChar*, int len, RenderStyle* style, const InlineTextBox* textBox, float xPos);
FloatPoint topLeftPositionOfCharacterRange(Vector<SVGChar>::iterator start, Vector<SVGChar>::iterator end);
float cummulatedWidthOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange& range);
float cummulatedHeightOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange& range);

RenderSVGRoot* findSVGRootObject(RenderObject* start);

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGRootInlineBox_h
