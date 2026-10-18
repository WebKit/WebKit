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

#include "SVGTextFragment.h"
#include <span>
#include <wtf/CheckedPtr.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class AffineTransform;
class RenderSVGInlineText;
class SVGTextContentElement;

// A SVGTextChunk describes a range of SVGTextFragments, see the SVG spec definition of a "text chunk".
class SVGTextChunk {
public:
    explicit SVGTextChunk(const RenderSVGInlineText& firstText);

    void appendFragments(std::span<SVGTextFragment>);

    unsigned LEGACY_NODELETE totalCharacters() const;
    float LEGACY_NODELETE totalLength() const;
    float LEGACY_NODELETE totalAnchorShift() const;
    void layout() const;

private:
    friend class SVGTextChunkBuilder;

    enum class ChunkStyle : uint8_t {
        MiddleAnchor = 1 << 0,
        EndAnchor = 1 << 1,
        RightToLeftText = 1 << 2,
        VerticalText = 1 << 3,
        LengthAdjustSpacing = 1 << 4,
        LengthAdjustSpacingAndGlyphs = 1 << 5
    };

    void LEGACY_NODELETE processTextAnchorCorrection() const;
    void LEGACY_NODELETE applySpacingAndGlyphsTransform() const;
    void LEGACY_NODELETE setLengthAdjustTransform(const AffineTransform&) const;
    void LEGACY_NODELETE processTextLengthSpacingCorrection() const;

    bool isVerticalText() const { return m_chunkStyle.contains(ChunkStyle::VerticalText); }
    float desiredTextLength() const { return m_desiredTextLength; }

    bool hasDesiredTextLength() const { return m_desiredTextLength > 0 && m_chunkStyle.containsAny({ ChunkStyle::LengthAdjustSpacing, ChunkStyle::LengthAdjustSpacingAndGlyphs }); }
    bool hasTextAnchor() const { return m_chunkStyle.contains(ChunkStyle::RightToLeftText) ? !m_chunkStyle.contains(ChunkStyle::EndAnchor) : m_chunkStyle.containsAny({ ChunkStyle::MiddleAnchor, ChunkStyle::EndAnchor }); }
    bool hasLengthAdjustSpacing() const { return m_chunkStyle.contains(ChunkStyle::LengthAdjustSpacing); }
    bool hasLengthAdjustSpacingAndGlyphs() const { return m_chunkStyle.contains(ChunkStyle::LengthAdjustSpacingAndGlyphs); }

    const SVGTextFragment* LEGACY_NODELETE firstFragment() const;

private:
    // Fragment ranges of the text boxes this chunk spans. A chunk may start or end inside a box.
    Vector<std::span<SVGTextFragment>> m_fragmentRanges;

    // Owner used by SVGTextChunkBuilder to group chunks for element-level textLength. webkit.org/b/61855.
    CheckedPtr<const SVGTextContentElement> m_textContentElement;

    float m_desiredTextLength { 0 };
    OptionSet<ChunkStyle> m_chunkStyle;

    // ElementGroup: textLength applied by SVGTextChunkBuilder; layout() must skip per-chunk scaling.
    enum class TextLengthLayoutMode : uint8_t {
        SingleChunk,
        ElementGroup
    };
    TextLengthLayoutMode m_textLengthLayoutMode { TextLengthLayoutMode::SingleChunk };
};

} // namespace WebCore
