/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
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

#include "InlineIteratorSVGTextBox.h"
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class AffineTransform;
class SVGInlineTextBox;

using SVGChunkTransformMap = HashMap<InlineIterator::SVGTextBox::Key, AffineTransform>;
using SVGTextFragmentMap = HashMap<InlineIterator::SVGTextBox::Key, Vector<SVGTextFragment>>;

// A SVGTextChunk describes a range of SVGTextFragments, see the SVG spec definition of a "text chunk".
class SVGTextChunk {
public:
    SVGTextChunk(const Vector<InlineIterator::SVGTextBoxIterator>&, unsigned first, unsigned limit, SVGTextFragmentMap&);

    unsigned totalCharacters() const;
    float totalLength() const;
    float totalAnchorShift() const;
    void layout(SVGChunkTransformMap&) const;

private:
    enum class ChunkStyle : uint8_t {
        MiddleAnchor = 1 << 0,
        EndAnchor = 1 << 1,
        RightToLeftText = 1 << 2,
        VerticalText = 1 << 3,
        LengthAdjustSpacing = 1 << 4,
        LengthAdjustSpacingAndGlyphs = 1 << 5
    };

    void processTextAnchorCorrection() const;
    void buildBoxTransformations(SVGChunkTransformMap&) const;
    void processTextLengthSpacingCorrection() const;

    bool isVerticalText() const { return m_chunkStyle.contains(ChunkStyle::VerticalText); }
    float desiredTextLength() const { return m_desiredTextLength; }

    bool hasDesiredTextLength() const { return m_desiredTextLength > 0 && m_chunkStyle.containsAny({ ChunkStyle::LengthAdjustSpacing, ChunkStyle::LengthAdjustSpacingAndGlyphs }); }
    bool hasTextAnchor() const { return m_chunkStyle.contains(ChunkStyle::RightToLeftText) ? !m_chunkStyle.contains(ChunkStyle::EndAnchor) : m_chunkStyle.containsAny({ ChunkStyle::MiddleAnchor, ChunkStyle::EndAnchor }); }
    bool hasLengthAdjustSpacing() const { return m_chunkStyle.contains(ChunkStyle::LengthAdjustSpacing); }
    bool hasLengthAdjustSpacingAndGlyphs() const { return m_chunkStyle.contains(ChunkStyle::LengthAdjustSpacingAndGlyphs); }

    bool boxSpacingAndGlyphsTransform(const Vector<SVGTextFragment>&, AffineTransform&) const;

    Vector<SVGTextFragment>& fragments(InlineIterator::SVGTextBoxIterator);
    const Vector<SVGTextFragment>& fragments(InlineIterator::SVGTextBoxIterator) const;

private:
    // Contains all SVGInlineTextBoxes this chunk spans.
    struct BoxAndFragments {
        InlineIterator::SVGTextBoxIterator box;
        Vector<SVGTextFragment>& fragments;
    };
    Vector<BoxAndFragments> m_boxes;

    float m_desiredTextLength { 0 };
    OptionSet<ChunkStyle> m_chunkStyle;
};

} // namespace WebCore
