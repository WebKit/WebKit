/*
 * Copyright (C) 2021, 2022, 2023, 2024 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "AffineTransform.h"
#include "PatternAttributes.h"
#include "RenderSVGResourcePaintServer.h"
#include "SVGPatternElement.h"

class Pattern;

namespace WebCore {

class RenderSVGResourcePattern : public RenderSVGResourcePaintServer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourcePattern);
public:
    RenderSVGResourcePattern(SVGElement&, RenderStyle&&);
    virtual ~RenderSVGResourcePattern();

    inline SVGPatternElement& patternElement() const;

    bool prepareFillOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) final;
    bool prepareStrokeOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) final;

    void invalidatePattern()
    {
        m_attributes = std::nullopt;
        repaintAllClients();
    }

protected:
    RefPtr<Pattern> buildPattern(GraphicsContext&, const RenderLayerModelObject&);

    void collectPatternAttributesIfNeeded();

    bool buildTileImageTransform(const RenderElement&, const PatternAttributes&, const SVGPatternElement&, FloatRect& patternBoundaries, AffineTransform& tileImageTransform) const;

    RefPtr<ImageBuffer> createTileImage(const PatternAttributes&, const FloatRect&, const FloatRect& scale, const AffineTransform& tileImageTransform) const;

    std::optional<PatternAttributes> m_attributes;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourcePattern, isRenderSVGResourcePattern())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
