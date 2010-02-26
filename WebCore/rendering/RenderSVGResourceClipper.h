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

#ifndef RenderSVGResourceClipper_h
#define RenderSVGResourceClipper_h

#if ENABLE(SVG)
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "RenderSVGResource.h"
#include "SVGClipPathElement.h"
#include "SVGUnitTypes.h"

#include <wtf/HashSet.h>

namespace WebCore {

class RenderSVGResourceClipper : public RenderSVGResource {

public:
    RenderSVGResourceClipper(SVGStyledElement*);
    virtual ~RenderSVGResourceClipper();

    virtual const char* renderName() const { return "RenderSVGResourceClipper"; }

    virtual void invalidateClients();
    virtual void invalidateClient(RenderObject*);

    virtual bool applyResource(RenderObject*, GraphicsContext*);
    virtual FloatRect resourceBoundingBox(const FloatRect&) const;

    virtual RenderSVGResourceType resourceType() const { return ClipperResourceType; }

    SVGUnitTypes::SVGUnitType clipPathUnits() const { return toUnitType(static_cast<SVGClipPathElement*>(node())->clipPathUnits()); }

    static RenderSVGResourceType s_resourceType;
private:
    HashSet<RenderObject*> m_clipper;
};

}

#endif
#endif
