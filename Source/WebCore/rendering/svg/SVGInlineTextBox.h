/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2023 Igalia S.L.
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
#include "LegacyRenderSVGResource.h"
#include "RenderSVGResourcePaintServer.h"
#include "SVGTextFragment.h"

namespace WebCore {

class LegacyRenderSVGResource;
class RenderSVGInlineText;
class SVGPaintServerHandling;
class SVGRootInlineBox;

class SVGInlineTextBox final : public LegacyInlineTextBox {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGInlineTextBox);
public:
    explicit SVGInlineTextBox(RenderSVGInlineText&);

    inline RenderSVGInlineText& renderer() const;

    float virtualLogicalHeight() const override { return m_logicalHeight; }
    void setLogicalHeight(float height) { m_logicalHeight = height; }

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

    int offsetForPositionInFragment(const SVGTextFragment&, float position) const;
    FloatRect selectionRectForTextFragment(const SVGTextFragment&, unsigned fragmentStartPosition, unsigned fragmentEndPosition, const RenderStyle&) const;
    
    OptionSet<RenderSVGResourceMode> paintingResourceMode() const { return OptionSet<RenderSVGResourceMode>::fromRaw(m_legacyPaintingResourceMode); }
    void setPaintingResourceMode(OptionSet<RenderSVGResourceMode> mode) { m_legacyPaintingResourceMode = mode.toRaw(); }

    inline SVGInlineTextBox* nextTextBox() const;
    
private:
    bool isSVGInlineTextBox() const override { return true; }

    TextRun constructTextRun(const RenderStyle&, const SVGTextFragment&) const;

    bool acquirePaintingResource(SVGPaintServerHandling&, float scalingFactor, RenderBoxModelObject&, const RenderStyle&);
    void releasePaintingResource(SVGPaintServerHandling&);

    bool acquireLegacyPaintingResource(GraphicsContext*&, float scalingFactor, RenderBoxModelObject&, const RenderStyle&);
    void releaseLegacyPaintingResource(GraphicsContext*&, const Path*);

    void paintDecoration(GraphicsContext&, OptionSet<TextDecorationLine>, const SVGTextFragment&);
    void paintDecorationWithStyle(GraphicsContext&, OptionSet<TextDecorationLine>, const SVGTextFragment&, RenderBoxModelObject& decorationRenderer);
    void paintTextWithShadows(GraphicsContext&, const RenderStyle&, TextRun&, const SVGTextFragment&, unsigned startPosition, unsigned endPosition);
    void paintText(GraphicsContext&, const RenderStyle&, const RenderStyle& selectionStyle, const SVGTextFragment&, bool hasSelection, bool paintSelectedTextOnly);

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

private:
    float m_logicalHeight { 0 };
    unsigned m_legacyPaintingResourceMode : 4; // RenderSVGResourceMode
    unsigned m_startsNewTextChunk : 1;
    LegacyRenderSVGResource* m_legacyPaintingResource { nullptr };
    SVGPaintServerOrColor m_paintServerOrColor { };

    Vector<SVGTextFragment> m_textFragments;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(SVGInlineTextBox, isSVGInlineTextBox())
