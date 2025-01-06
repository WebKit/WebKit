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

#include "AffineTransform.h"
#include "PatternAttributes.h"
#include "RenderSVGResourcePaintServer.h"
#include "SVGPatternElement.h"

class Pattern;

namespace WebCore {

class RenderSVGResourcePattern final : public RenderSVGResourcePaintServer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderSVGResourcePattern);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGResourcePattern);
public:
    RenderSVGResourcePattern(SVGElement&, RenderStyle&&);
    virtual ~RenderSVGResourcePattern();

    inline SVGPatternElement& patternElement() const;
    inline Ref<SVGPatternElement> protectedPatternElement() const;

    bool prepareFillOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) final;
    bool prepareStrokeOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) final;

    enum class SuppressRepaint { Yes, No };
    void invalidatePattern(SuppressRepaint suppressRepaint = SuppressRepaint::No)
    {
        m_attributes = std::nullopt;
        m_imageMap.clear();
        m_transformMap.clear();
        if (suppressRepaint == SuppressRepaint::No)
            repaintAllClients();
    }

protected:
    RefPtr<Pattern> buildPattern(GraphicsContext&, const RenderLayerModelObject&);

    void collectPatternAttributesIfNeeded();

    bool buildTileImageTransform(const RenderElement&, const PatternAttributes&, const SVGPatternElement&, FloatRect& patternBoundaries, AffineTransform& tileImageTransform) const;

    RefPtr<ImageBuffer> createTileImage(GraphicsContext&, const PatternAttributes&, const FloatSize&, const FloatSize& scale, const AffineTransform& tileImageTransform) const;

    void removeReferencingCSSClient(const RenderElement&) override;

    std::optional<PatternAttributes> m_attributes;

    UncheckedKeyHashMap<SingleThreadWeakRef<const RenderLayerModelObject>, RefPtr<ImageBuffer>> m_imageMap;
    UncheckedKeyHashMap<SingleThreadWeakRef<const RenderLayerModelObject>, AffineTransform> m_transformMap;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourcePattern, isRenderSVGResourcePattern())
