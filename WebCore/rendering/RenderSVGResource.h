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

#ifndef RenderSVGResource_h
#define RenderSVGResource_h

#if ENABLE(SVG)
#include "FloatRect.h"
#include "RenderSVGHiddenContainer.h"

namespace WebCore {

enum RenderSVGResourceType {
    MaskerResourceType,
    ClipperResourceType
};

class RenderSVGResource : public RenderSVGHiddenContainer {
public:
    RenderSVGResource(SVGStyledElement* node) : RenderSVGHiddenContainer(node) { }

    template<class Renderer>
    Renderer* cast()
    {
        if (Renderer::s_resourceType == resourceType())
            return static_cast<Renderer*>(this);

        return 0;
    }

    virtual RenderSVGResource* toRenderSVGResource() { return this; }
    virtual bool isSVGResource() const { return true; }
    virtual bool drawsContents() { return false; }

    virtual void invalidateClients() = 0;
    virtual void invalidateClient(RenderObject*) = 0;

    virtual bool applyResource(RenderObject*, GraphicsContext*) = 0;
    virtual FloatRect resourceBoundingBox(const FloatRect&) const = 0;

    virtual RenderSVGResourceType resourceType() const = 0;
};

template<typename Renderer>
Renderer* getRenderSVGResourceById(Document* document, const AtomicString& id)
{
    if (id.isEmpty())
        return 0;

    Element* element = document->getElementById(id);
    if (!element || !element->isSVGElement())
        return 0;

    RenderObject* renderer = element->renderer();
    if (!renderer)
        return 0;

    RenderSVGResource* renderResource = renderer->toRenderSVGResource();
    if (!renderResource)
        return 0;

    return renderResource->cast<Renderer>();
}

}

#endif
#endif
