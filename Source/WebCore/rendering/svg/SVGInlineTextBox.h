/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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
 */

#pragma once

#include "LegacyInlineTextBox.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGResource.h"
#include "SVGTextFragment.h"

namespace WebCore {

class RenderSVGResource;
class SVGRootInlineBox;

class SVGInlineTextBox final : public LegacyInlineTextBox {
    WTF_MAKE_ISO_ALLOCATED(SVGInlineTextBox);
public:
    explicit SVGInlineTextBox(RenderSVGInlineText&);

    RenderSVGInlineText& renderer() const { return downcast<RenderSVGInlineText>(LegacyInlineTextBox::renderer()); }

    float virtualLogicalHeight() const override { return m_logicalHeight; }
    void setLogicalHeight(float height) { m_logicalHeight = height; }

    int selectionTop() { return top(); }
    int selectionHeight() { return static_cast<int>(ceilf(m_logicalHeight)); }
    int offsetForPosition(float x, bool includePartialGlyphs = true) const override;
    float positionForOffset(unsigned offset) const override;

    void paintSelectionBackground(PaintInfo&);
    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    LayoutRect localSelectionRect(unsigned startPosition, unsigned endPosition) const override;

    bool mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment&, unsigned& startPosition, unsigned& endPosition) const;

    FloatRect calculateBoundaries() const;

    void clearTextFragments() { m_textFragments.clear(); }
    Vector<SVGTextFragment>& textFragments() { return m_textFragments; }
    const Vector<SVGTextFragment>& textFragments() const { return m_textFragments; }

    void dirtyOwnLineBoxes() override;
    void dirtyLineBoxes() override;

    bool startsNewTextChunk() const { return m_startsNewTextChunk; }
    void setStartsNewTextChunk(bool newTextChunk) { m_startsNewTextChunk = newTextChunk; }

    int offsetForPositionInFragment(const SVGTextFragment&, float position, bool includePartialGlyphs) const;
    FloatRect selectionRectForTextFragment(const SVGTextFragment&, unsigned fragmentStartPosition, unsigned fragmentEndPosition, const RenderStyle&) const;
    
    OptionSet<RenderSVGResourceMode> paintingResourceMode() const { return OptionSet<RenderSVGResourceMode>::fromRaw(m_paintingResourceMode); }
    void setPaintingResourceMode(OptionSet<RenderSVGResourceMode> mode) { m_paintingResourceMode = mode.toRaw(); }

    SVGInlineTextBox* nextTextBox() const { return downcast<SVGInlineTextBox>(LegacyInlineTextBox::nextTextBox()); }
    
private:
    bool isSVGInlineTextBox() const override { return true; }

    TextRun constructTextRun(const RenderStyle&, const SVGTextFragment&) const;

    bool acquirePaintingResource(GraphicsContext*&, float scalingFactor, RenderBoxModelObject&, const RenderStyle&);
    void releasePaintingResource(GraphicsContext*&, const Path*);

    bool prepareGraphicsContextForTextPainting(GraphicsContext*&, float scalingFactor, const RenderStyle&);
    void restoreGraphicsContextAfterTextPainting(GraphicsContext*&);

    void paintDecoration(GraphicsContext&, OptionSet<TextDecoration>, const SVGTextFragment&);
    void paintDecorationWithStyle(GraphicsContext&, OptionSet<TextDecoration>, const SVGTextFragment&, RenderBoxModelObject& decorationRenderer);
    void paintTextWithShadows(GraphicsContext&, const RenderStyle&, TextRun&, const SVGTextFragment&, unsigned startPosition, unsigned endPosition);
    void paintText(GraphicsContext&, const RenderStyle&, const RenderStyle& selectionStyle, const SVGTextFragment&, bool hasSelection, bool paintSelectedTextOnly);

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

private:
    float m_logicalHeight { 0 };
    unsigned m_paintingResourceMode : 4; // RenderSVGResourceMode
    unsigned m_startsNewTextChunk : 1;
    RenderSVGResource* m_paintingResource { nullptr };
    Vector<SVGTextFragment> m_textFragments;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(SVGInlineTextBox, isSVGInlineTextBox())
