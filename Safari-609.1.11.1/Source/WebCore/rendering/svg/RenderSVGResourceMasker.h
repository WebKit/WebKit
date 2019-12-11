/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include "RenderSVGResourceContainer.h"
#include "SVGMaskElement.h"
#include "SVGUnitTypes.h"

#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;

struct MaskerData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    std::unique_ptr<ImageBuffer> maskImage;
};

class RenderSVGResourceMasker final : public RenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceMasker);
public:
    RenderSVGResourceMasker(SVGMaskElement&, RenderStyle&&);
    virtual ~RenderSVGResourceMasker();

    SVGMaskElement& maskElement() const { return downcast<SVGMaskElement>(RenderSVGResourceContainer::element()); }

    void removeAllClientsFromCache(bool markForInvalidation = true) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;
    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override;
    FloatRect resourceBoundingBox(const RenderObject&) override;

    SVGUnitTypes::SVGUnitType maskUnits() const { return maskElement().maskUnits(); }
    SVGUnitTypes::SVGUnitType maskContentUnits() const { return maskElement().maskContentUnits(); }

    RenderSVGResourceType resourceType() const override { return MaskerResourceType; }

private:
    void element() const = delete;

    const char* renderName() const override { return "RenderSVGResourceMasker"; }

    bool drawContentIntoMaskImage(MaskerData*, ColorSpace, RenderObject*);
    void calculateMaskContentRepaintRect();

    FloatRect m_maskContentBoundaries;
    HashMap<RenderObject*, std::unique_ptr<MaskerData>> m_masker;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_SVG_RESOURCE(RenderSVGResourceMasker, MaskerResourceType)
