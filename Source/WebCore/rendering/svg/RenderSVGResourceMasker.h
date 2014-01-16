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

#ifndef RenderSVGResourceMasker_h
#define RenderSVGResourceMasker_h

#if ENABLE(SVG)
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "RenderSVGResourceContainer.h"
#include "SVGMaskElement.h"
#include "SVGUnitTypes.h"

#include <wtf/HashMap.h>

namespace WebCore {

struct MaskerData {
    std::unique_ptr<ImageBuffer> maskImage;
};

class RenderSVGResourceMasker FINAL : public RenderSVGResourceContainer {
public:
    RenderSVGResourceMasker(SVGMaskElement&, PassRef<RenderStyle>);
    virtual ~RenderSVGResourceMasker();

    SVGMaskElement& maskElement() const { return toSVGMaskElement(RenderSVGResourceContainer::element()); }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true);
    virtual void removeClientFromCache(RenderObject&, bool markForInvalidation = true);
    virtual bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, unsigned short resourceMode) override;
    virtual FloatRect resourceBoundingBox(const RenderObject&) override;

    SVGUnitTypes::SVGUnitType maskUnits() const { return maskElement().maskUnits(); }
    SVGUnitTypes::SVGUnitType maskContentUnits() const { return maskElement().maskContentUnits(); }

    virtual RenderSVGResourceType resourceType() const { return s_resourceType; }
    static RenderSVGResourceType s_resourceType;

private:
    void element() const WTF_DELETED_FUNCTION;

    virtual const char* renderName() const override { return "RenderSVGResourceMasker"; }

    bool drawContentIntoMaskImage(MaskerData*, ColorSpace, RenderObject*);
    void calculateMaskContentRepaintRect();

    FloatRect m_maskContentBoundaries;
    HashMap<RenderObject*, std::unique_ptr<MaskerData>> m_masker;
};

}

#endif
#endif
