/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class AffineTransform;
class SVGInlineTextBox;

// A SVGTextChunk describes a range of SVGTextFragments, see the SVG spec definition of a "text chunk".
class SVGTextChunk {
public:
    enum ChunkStyle {
        DefaultStyle = 1 << 0,
        MiddleAnchor = 1 << 1,
        EndAnchor = 1 << 2,
        RightToLeftText = 1 << 3,
        VerticalText = 1 << 4,
        LengthAdjustSpacing = 1 << 5,
        LengthAdjustSpacingAndGlyphs = 1 << 6
    };

    SVGTextChunk(const Vector<SVGInlineTextBox*>&, unsigned first, unsigned limit);

    unsigned totalCharacters() const;
    float totalLength() const;
    float totalAnchorShift() const;
    void layout(HashMap<SVGInlineTextBox*, AffineTransform>&) const;

private:
    void processTextAnchorCorrection() const;
    void buildBoxTransformations(HashMap<SVGInlineTextBox*, AffineTransform>&) const;
    void processTextLengthSpacingCorrection() const;

    bool isVerticalText() const { return m_chunkStyle & VerticalText; }
    float desiredTextLength() const { return m_desiredTextLength; }

    bool hasDesiredTextLength() const { return m_desiredTextLength > 0 && ((m_chunkStyle & LengthAdjustSpacing) || (m_chunkStyle & LengthAdjustSpacingAndGlyphs)); }
    bool hasTextAnchor() const {  return m_chunkStyle & RightToLeftText ? !(m_chunkStyle & EndAnchor) : (m_chunkStyle & (MiddleAnchor | EndAnchor)); }
    bool hasLengthAdjustSpacing() const { return m_chunkStyle & LengthAdjustSpacing; }
    bool hasLengthAdjustSpacingAndGlyphs() const { return m_chunkStyle & LengthAdjustSpacingAndGlyphs; }

    bool boxSpacingAndGlyphsTransform(const SVGInlineTextBox*, AffineTransform&) const;

private:
    // Contains all SVGInlineTextBoxes this chunk spans.
    Vector<SVGInlineTextBox*> m_boxes;

    unsigned m_chunkStyle { DefaultStyle };
    float m_desiredTextLength { 0 };
};

} // namespace WebCore
