/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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
#include "Path.h"
#include "PathArcLengthMapper.h"
#include "SVGTextChunkBuilder.h"
#include "SVGTextFragment.h"
#include "SVGTextLayoutAttributes.h"

namespace WebCore {

class RenderObject;
class RenderSVGInlineText;
class RenderSVGTextPath;
class SVGElement;

namespace Style {
class ComputedStyle;
}

// A text box as seen by SVGTextLayoutEngine.
struct SVGTextLayoutBox {
    CheckedRef<const RenderSVGInlineText> text;
    unsigned start { 0 };
    unsigned length { 0 };
    // Display box index, where the fragments go when the engine stores them by box index.
    size_t index { 0 };
    // Renderer of the previous leaf box on the line.
    CheckedPtr<const RenderObject> previousLeafRenderer;
};

// SVGTextLayoutEngine performs the second layout phase for SVG text.
//
// The display boxes were created, containing the text chunk information, necessary to apply
// certain SVG specific text layout properties (text-length adjustments and text-anchor).
// The second layout phase uses the SVGTextLayoutAttributes stored in the individual
// RenderSVGInlineText renderers to compute the final positions for each character
// which are stored as SVGTextFragments of the text boxes.

class SVGTextLayoutEngine {
public:
    // Keeps the fragments of each text box in engine-owned storage.
    SVGTextLayoutEngine(Vector<SVGTextLayoutAttributes*>&);
    // Appends the fragments of each text box to fragmentsForBoxes[box.index]. The vector must not be resized while the engine exists.
    SVGTextLayoutEngine(Vector<SVGTextLayoutAttributes*>&, Vector<Vector<SVGTextFragment>>& fragmentsForBoxes);
    SVGTextLayoutEngine(SVGTextLayoutEngine&&) = default;
    SVGTextLayoutEngine(const SVGTextLayoutEngine&) = delete;

    Vector<SVGTextLayoutAttributes*>& layoutAttributes() LIFETIME_BOUND { return m_layoutAttributes; }

    void beginTextPathLayout(const RenderSVGTextPath&, SVGTextLayoutEngine& lineLayout);
    void endTextPathLayout();

    void layoutInlineTextBox(const SVGTextLayoutBox&);

    void finishLayout();

private:
    void LEGACY_NODELETE updateCharacterPositionIfNeeded(float& x, float& y);
    void LEGACY_NODELETE updateCurrentTextPosition(float x, float y, float glyphAdvance);
    void LEGACY_NODELETE updateRelativePositionAdjustmentsIfNeeded(float dx, float dy);

    void recordTextFragment(const SVGTextLayoutBox&, const Vector<SVGTextMetrics>&);
    Vector<SVGTextFragment>& fragmentsForBox(const SVGTextLayoutBox&);
    bool parentDefinesTextLength(RenderObject*) const;

    float computeTextPathStartOffset(const RenderSVGTextPath&) const;

    void layoutTextOnLineOrPath(const SVGTextLayoutBox&, const Style::ComputedStyle&);

    bool LEGACY_NODELETE currentLogicalCharacterAttributes(SVGTextLayoutAttributes*&);
    bool LEGACY_NODELETE currentLogicalCharacterMetrics(SVGTextLayoutAttributes*&, SVGTextMetrics&);
    bool currentVisualCharacterMetrics(const SVGTextLayoutBox&, const Vector<SVGTextMetrics>&, SVGTextMetrics&);

    void LEGACY_NODELETE advanceToNextLogicalCharacter(const SVGTextMetrics&);
    void LEGACY_NODELETE advanceToNextVisualCharacter(const SVGTextMetrics&);

private:
    Vector<SVGTextLayoutAttributes*>& m_layoutAttributes;

    Vector<SVGTextChunkBox> m_lineLayoutBoxes;

    // Output. Fragments go to m_fragmentsForBoxes if set. Otherwise each laid out box appends its own entry to m_ownedFragments.
    Vector<Vector<SVGTextFragment>>* m_fragmentsForBoxes { nullptr };
    Vector<Vector<SVGTextFragment>> m_ownedFragments;

    SVGTextChunkBuilder m_chunkLayoutBuilder;
    Vector<unsigned> m_lineLayoutChunkStarts;

    SVGTextFragment m_currentTextFragment;
    unsigned m_layoutAttributesPosition { 0 };
    unsigned m_logicalCharacterOffset { 0 };
    unsigned m_logicalMetricsListOffset { 0 };
    unsigned m_visualCharacterOffset { 0 };
    unsigned m_visualMetricsListOffset { 0 };
    float m_x { 0.0f };
    float m_y { 0.0f };
    float m_dx { 0.0f };
    float m_dy { 0.0f };
    float m_lastChunkStartPosition { 0.0f };
    bool m_lastChunkHasTextLength { false };
    bool m_lastChunkIsVerticalText { false };
    bool m_isVerticalText { false };
    bool m_inPathLayout { false };

    // Text on path layout
    Path m_textPath;
    PathArcLengthMapper m_textPathMapper;
    float m_textPathLength { 0.0f };
    float m_textPathStartOffset { 0.0f };
    float m_textPathCurrentOffset { 0.0f };
    float m_textPathSpacing { 0.0f };
    float m_textPathScaling { 1.0f };
    bool m_textPathReversed { false };
};

} // namespace WebCore
