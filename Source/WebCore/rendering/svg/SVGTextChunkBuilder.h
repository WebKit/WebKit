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

#include "SVGTextChunk.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderSVGInlineText;
class SVGInlineTextBox;
struct SVGTextFragment;

// SVGTextChunkBuilder performs the third layout phase for SVG text.
//
// Phase one built the layout information from the SVG DOM stored in the RenderSVGInlineText objects (SVGTextLayoutAttributes).
// Phase two performed the actual per-character layout, computing the final positions for each character, stored in the SVGInlineTextBox objects (SVGTextFragment).
// Phase three performs all modifications that have to be applied to each individual text chunk (text-anchor & textLength).

class SVGTextChunkBuilder {
public:
    SVGTextChunkBuilder();
    SVGTextChunkBuilder(SVGTextChunkBuilder&&) = default;
    SVGTextChunkBuilder(const SVGTextChunkBuilder&) = delete;

    const Vector<SVGTextChunk>& textChunks() const { return m_textChunks; }
    unsigned totalCharacters() const;
    float totalLength() const;
    float totalAnchorShift() const;
    AffineTransform transformationForTextBox(InlineIterator::SVGTextBoxIterator) const;

    void buildTextChunks(const Vector<InlineIterator::SVGTextBoxIterator>& lineLayoutBoxes, const HashSet<InlineIterator::SVGTextBox::Key>& chunkStarts, SVGTextFragmentMap&);
    void layoutTextChunks(const Vector<InlineIterator::SVGTextBoxIterator>& lineLayoutBoxes, const HashSet<InlineIterator::SVGTextBox::Key>& chunkStarts, SVGTextFragmentMap&);

private:
    Vector<SVGTextChunk> m_textChunks;
    SVGChunkTransformMap m_textBoxTransformations;
};

} // namespace WebCore
