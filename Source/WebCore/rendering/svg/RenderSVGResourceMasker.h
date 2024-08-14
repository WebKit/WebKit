/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2021, 2022, 2023 Igalia S.L.
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

#include "RenderSVGResourceContainer.h"
#include "SVGUnitTypes.h"

#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;
class SVGMaskElement;

class RenderSVGResourceMasker final : public RenderSVGResourceContainer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderSVGResourceMasker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGResourceMasker);
public:
    RenderSVGResourceMasker(SVGMaskElement&, RenderStyle&&);
    virtual ~RenderSVGResourceMasker();

    inline SVGMaskElement& maskElement() const;
    inline Ref<SVGMaskElement> protectedMaskElement() const;

    void applyMask(PaintInfo&, const RenderLayerModelObject& targetRenderer, const LayoutPoint& adjustedPaintOffset);

    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation);

    inline SVGUnitTypes::SVGUnitType maskUnits() const;
    inline SVGUnitTypes::SVGUnitType maskContentUnits() const;

    void invalidateMask()
    {
        m_masker.clear();
    }

    void removeReferencingCSSClient(const RenderElement&) final;

    bool drawContentIntoContext(GraphicsContext&, const FloatRect& objectBoundingBox);
    bool drawContentIntoContext(GraphicsContext&, const FloatRect& destinationRect, const FloatRect& sourceRect, ImagePaintingOptions);

private:
    void element() const = delete;

    ASCIILiteral renderName() const final { return "RenderSVGResourceMasker"_s; }
    HashMap<SingleThreadWeakRef<const RenderLayerModelObject>, RefPtr<ImageBuffer>> m_masker;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceMasker, isRenderSVGResourceMasker())
