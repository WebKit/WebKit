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

#include "Path.h"
#include "SVGTextChunkBuilder.h"
#include "SVGTextFragment.h"
#include "SVGTextLayoutAttributes.h"

namespace WebCore {

class RenderObject;
class RenderStyle;
class RenderSVGInlineText;
class RenderSVGTextPath;
class SVGElement;
class SVGInlineTextBox;
class SVGRenderStyle;

// SVGTextLayoutEngine performs the second layout phase for SVG text.
//
// The InlineBox tree was created, containing the text chunk information, necessary to apply
// certain SVG specific text layout properties (text-length adjustments and text-anchor).
// The second layout phase uses the SVGTextLayoutAttributes stored in the individual
// RenderSVGInlineText renderers to compute the final positions for each character
// which are stored in the SVGInlineTextBox objects.

class SVGTextLayoutEngine {
    WTF_MAKE_NONCOPYABLE(SVGTextLayoutEngine);
public:
    SVGTextLayoutEngine(Vector<SVGTextLayoutAttributes*>&);

    Vector<SVGTextLayoutAttributes*>& layoutAttributes() { return m_layoutAttributes; }

    void beginTextPathLayout(RenderSVGTextPath&, SVGTextLayoutEngine& lineLayout);
    void endTextPathLayout();

    void layoutInlineTextBox(SVGInlineTextBox&);
    void finishLayout();

private:
    void updateCharacterPositionIfNeeded(float& x, float& y);
    void updateCurrentTextPosition(float x, float y, float glyphAdvance);
    void updateRelativePositionAdjustmentsIfNeeded(float dx, float dy);

    void recordTextFragment(SVGInlineTextBox&, Vector<SVGTextMetrics>&);
    bool parentDefinesTextLength(RenderObject*) const;

    void layoutTextOnLineOrPath(SVGInlineTextBox&, RenderSVGInlineText&, const RenderStyle&);
    void finalizeTransformMatrices(Vector<SVGInlineTextBox*>&);

    bool currentLogicalCharacterAttributes(SVGTextLayoutAttributes*&);
    bool currentLogicalCharacterMetrics(SVGTextLayoutAttributes*&, SVGTextMetrics&);
    bool currentVisualCharacterMetrics(const SVGInlineTextBox&, Vector<SVGTextMetrics>&, SVGTextMetrics&);

    void advanceToNextLogicalCharacter(const SVGTextMetrics&);
    void advanceToNextVisualCharacter(const SVGTextMetrics&);

private:
    Vector<SVGTextLayoutAttributes*>& m_layoutAttributes;

    Vector<SVGInlineTextBox*> m_lineLayoutBoxes;
    Vector<SVGInlineTextBox*> m_pathLayoutBoxes;
    SVGTextChunkBuilder m_chunkLayoutBuilder;

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
    float m_textPathLength { 0.0f };
    float m_textPathStartOffset { 0.0f };
    float m_textPathCurrentOffset { 0.0f };
    float m_textPathSpacing { 0.0f };
    float m_textPathScaling { 1.0f };
};

} // namespace WebCore
