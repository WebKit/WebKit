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
 *
 */

#ifndef RenderSVGResourceMasker_h
#define RenderSVGResourceMasker_h

#if ENABLE(SVG)
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "RenderSVGResource.h"
#include "SVGMaskElement.h"
#include "SVGUnitTypes.h"

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

struct MaskerData {
    MaskerData(FloatRect rect = FloatRect(), bool emptyObject = false)
        : maskRect(rect)
        , emptyMask(emptyObject)
    {
    }

    OwnPtr<ImageBuffer> maskImage;
    FloatRect maskRect;
    bool emptyMask;
};

class RenderSVGResourceMasker : public RenderSVGResource {

public:
    RenderSVGResourceMasker(SVGStyledElement*);
    virtual ~RenderSVGResourceMasker();

    virtual const char* renderName() const { return "RenderSVGResourceMasker"; }

    virtual void invalidateClients();
    virtual void invalidateClient(RenderObject*);

    virtual bool applyResource(RenderObject*, GraphicsContext*);
    virtual FloatRect resourceBoundingBox(const FloatRect&) const;

    SVGUnitTypes::SVGUnitType maskUnits() const { return toUnitType(static_cast<SVGMaskElement*>(node())->maskUnits()); }
    SVGUnitTypes::SVGUnitType maskContentUnits() const { return toUnitType(static_cast<SVGMaskElement*>(node())->maskContentUnits()); }

    virtual RenderSVGResourceType resourceType() const { return s_resourceType; }
    static RenderSVGResourceType s_resourceType;

private:
    void createMaskImage(MaskerData*, const SVGMaskElement*, RenderObject*);

    HashMap<RenderObject*, MaskerData*> m_masker;
};

}

#endif
#endif
