/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *               2004, 2005 Rob Buis <buis@kde.org>
 *               2005 Eric Seidel <eric@webkit.org>
 *               2009 Dirk Schulze <krit@webkit.org>
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
 *
 */

#ifndef RenderSVGResourceFilter_h
#define RenderSVGResourceFilter_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FloatRect.h"
#include "ImageBuffer.h"
#include "RenderSVGResourceContainer.h"
#include "SVGFilter.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SVGUnitTypes.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

struct FilterData {
    FilterData()
        : builded(false)
    {
    }

    RefPtr<SVGFilter> filter;
    OwnPtr<SVGFilterBuilder> builder;
    FloatRect boundaries;
    FloatSize scale;
    bool builded;
};

class GraphicsContext;

class RenderSVGResourceFilter : public RenderSVGResourceContainer {
public:
    RenderSVGResourceFilter(SVGFilterElement*);
    virtual ~RenderSVGResourceFilter();

    virtual const char* renderName() const { return "RenderSVGResourceFilter"; }

    virtual void invalidateClients();
    virtual void invalidateClient(RenderObject*);

    virtual bool applyResource(RenderObject*, RenderStyle*, GraphicsContext*&, unsigned short resourceMode);
    virtual void postApplyResource(RenderObject*, GraphicsContext*&, unsigned short resourceMode);

    virtual FloatRect resourceBoundingBox(RenderObject*);

    PassOwnPtr<SVGFilterBuilder> buildPrimitives();

    SVGUnitTypes::SVGUnitType filterUnits() const { return toUnitType(static_cast<SVGFilterElement*>(node())->filterUnits()); }
    SVGUnitTypes::SVGUnitType primitiveUnits() const { return toUnitType(static_cast<SVGFilterElement*>(node())->primitiveUnits()); }

    virtual RenderSVGResourceType resourceType() const { return s_resourceType; }
    static RenderSVGResourceType s_resourceType;

private:
    bool fitsInMaximumImageSize(const FloatSize&, FloatSize&);

    // Intermediate storage during 
    GraphicsContext* m_savedContext;
    OwnPtr<ImageBuffer> m_sourceGraphicBuffer;

    HashMap<RenderObject*, FilterData*> m_filter;
};

}

#endif
#endif
