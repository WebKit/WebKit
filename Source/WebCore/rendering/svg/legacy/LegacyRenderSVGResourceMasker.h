/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "ImageBuffer.h"
#include "LegacyRenderSVGResourceContainer.h"
#include "SVGUnitTypes.h"

#include <wtf/EnumeratedArray.h>
#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;
class SVGMaskElement;

struct MaskerData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    RefPtr<ImageBuffer> maskImage;
};

class LegacyRenderSVGResourceMasker final : public LegacyRenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGResourceMasker);
public:
    LegacyRenderSVGResourceMasker(SVGMaskElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGResourceMasker();

    inline SVGMaskElement& maskElement() const;

    void removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;
    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override;
    bool drawContentIntoContext(GraphicsContext&, const FloatRect& objectBoundingBox);
    bool drawContentIntoContext(GraphicsContext&, const FloatRect& destinationRect, const FloatRect& sourceRect, ImagePaintingOptions);
    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) override;

    inline SVGUnitTypes::SVGUnitType maskUnits() const;
    inline SVGUnitTypes::SVGUnitType maskContentUnits() const;

    RenderSVGResourceType resourceType() const override { return MaskerResourceType; }

private:
    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGResourceMasker"_s; }

    bool drawContentIntoMaskImage(MaskerData*, const DestinationColorSpace&, RenderObject*);
    void calculateMaskContentRepaintRect(RepaintRectCalculation);

    EnumeratedArray<RepaintRectCalculation, FloatRect, RepaintRectCalculation::Accurate> m_maskContentBoundaries;
    HashMap<SingleThreadWeakRef<RenderObject>, std::unique_ptr<MaskerData>> m_masker;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_LEGACY_RENDER_SVG_RESOURCE(LegacyRenderSVGResourceMasker, MaskerResourceType)
