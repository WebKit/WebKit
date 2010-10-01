/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGInlineTextBox_h
#define SVGInlineTextBox_h

#if ENABLE(SVG)
#include "InlineTextBox.h"
#include "SVGTextChunkLayoutInfo.h"
#include "SVGTextLayoutUtilities.h"

namespace WebCore {

class RenderSVGResource;
class SVGRootInlineBox;

struct SVGCharacterLayoutInfo;
struct SVGLastGlyphInfo;

class SVGInlineTextBox : public InlineTextBox {
public:
    SVGInlineTextBox(RenderObject*);

    virtual bool isSVGInlineTextBox() const { return true; }

    virtual int virtualLogicalHeight() const { return m_logicalHeight; }
    void setLogicalHeight(int height) { m_logicalHeight = height; }

    virtual int selectionTop() { return m_y; }
    virtual int selectionHeight() { return m_logicalHeight; }
    virtual int offsetForPosition(int x, bool includePartialGlyphs = true) const;
    virtual int positionForOffset(int offset) const;

    virtual void paint(PaintInfo&, int tx, int ty);
    virtual IntRect selectionRect(int absx, int absy, int startPos, int endPos);

    virtual void selectionStartEnd(int& startPos, int& endPos);
    void mapStartEndPositionsIntoChunkPartCoordinates(int& startPos, int& endPos, const SVGTextChunkPart&) const;

    SVGRootInlineBox* svgRootInlineBox() const;

    // Helper functions shared with SVGRootInlineBox     
    void measureCharacter(RenderStyle*, int position, int& charsConsumed, String& glyphName, String& unicodeString, float& glyphWidth, float& glyphHeight) const;
    FloatRect calculateGlyphBoundaries(RenderStyle*, int position, const SVGChar&) const;

    void buildLayoutInformation(SVGCharacterLayoutInfo&, SVGLastGlyphInfo&);

    const AffineTransform& chunkTransformation() const { return m_chunkTransformation; }
    void setChunkTransformation(const AffineTransform& transform) { m_chunkTransformation = transform; }
    void addChunkPartInformation(const SVGTextChunkPart& part) { m_svgTextChunkParts.append(part); }
    const Vector<SVGTextChunkPart>& svgTextChunkParts() const { return m_svgTextChunkParts; }

    virtual IntRect calculateBoundaries() const;

private:
    TextRun constructTextRun(RenderStyle*) const;
    AffineTransform buildChunkTransformation(SVGChar& firstCharacter) const;

    bool acquirePaintingResource(GraphicsContext*&, RenderObject*, RenderStyle*);
    void releasePaintingResource(GraphicsContext*&);

    bool prepareGraphicsContextForTextPainting(GraphicsContext*&, TextRun&, RenderStyle*);
    void restoreGraphicsContextAfterTextPainting(GraphicsContext*&, TextRun&);

    void computeTextMatchMarkerRect(RenderStyle*);
    void paintDecoration(GraphicsContext*, const FloatPoint& textOrigin, ETextDecoration, bool hasSelection);
    void paintDecorationWithStyle(GraphicsContext*, const FloatPoint& textOrigin, RenderObject*, ETextDecoration);
    void paintSelection(GraphicsContext*, const FloatPoint& textOrigin, RenderStyle*);
    void paintText(GraphicsContext*, const FloatPoint& textOrigin, RenderStyle*, RenderStyle* selectionStyle, bool hasSelection, bool paintSelectedTextOnly);
    void paintTextWithShadows(GraphicsContext*, const FloatPoint& textOrigin, RenderStyle*, TextRun&, int startPos, int endPos);

    FloatRect selectionRectForTextChunkPart(const SVGTextChunkPart&, int partStartPos, int partEndPos, RenderStyle*);

private:
    int m_logicalHeight;
    AffineTransform m_chunkTransformation;
    Vector<SVGTextChunkPart> m_svgTextChunkParts;
    mutable SVGTextChunkPart m_currentChunkPart;
    RenderSVGResource* m_paintingResource;
    int m_paintingResourceMode;
};

} // namespace WebCore

#endif
#endif // SVGInlineTextBox_h
