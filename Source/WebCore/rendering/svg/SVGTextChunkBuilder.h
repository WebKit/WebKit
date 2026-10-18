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

#include "RenderSVGInlineText.h"
#include "SVGTextChunk.h"
#include "SVGTextFragment.h"
#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>

namespace WebCore {

// The laid out fragments of one text box, in line order.
struct SVGTextChunkBox {
    CheckedRef<const RenderSVGInlineText> text;
    std::span<SVGTextFragment> fragments;
    // Number of entries this box contributes to the chunk start list, each the index of a fragment that starts a new chunk.
    unsigned chunkStartCount { 0 };
};

// SVGTextChunkBuilder performs the third layout phase for SVG text.
//
// Phase one built the layout information from the SVG DOM stored in the RenderSVGInlineText objects (SVGTextLayoutAttributes).
// Phase two performed the actual per-character layout, computing the final positions for each character, stored as SVGTextFragments of the text boxes.
// Phase three performs all modifications that have to be applied to each individual text chunk (text-anchor & textLength).

class SVGTextChunkBuilder {
public:
    SVGTextChunkBuilder();
    SVGTextChunkBuilder(SVGTextChunkBuilder&&) = default;
    SVGTextChunkBuilder(const SVGTextChunkBuilder&) = delete;

    const Vector<SVGTextChunk>& textChunks() const LIFETIME_BOUND { return m_textChunks; }
    unsigned LEGACY_NODELETE totalCharacters() const;
    float LEGACY_NODELETE totalLength() const;
    float LEGACY_NODELETE totalAnchorShift() const;

    void buildTextChunks(std::span<const SVGTextChunkBox>, std::span<const unsigned> chunkStarts);
    void layoutTextChunks(std::span<const SVGTextChunkBox>, std::span<const unsigned> chunkStarts);

private:
    // SVG2 §11.10: applies 'textLength' across all chunks of an owning element when
    // list-valued x/y splits it into per-glyph chunks. spacingAndGlyphs only;
    // spacing, nested tspan textLength, inline-size, and forced line breaks are
    // still per-chunk. webkit.org/b/61855.
    void applyElementLevelTextLength();

    Vector<SVGTextChunk> m_textChunks;
};

} // namespace WebCore
