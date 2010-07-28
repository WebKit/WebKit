/*
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

#ifndef RenderSVGResourceContainer_h
#define RenderSVGResourceContainer_h

#if ENABLE(SVG)
#include "RenderSVGHiddenContainer.h"

#include "SVGStyledTransformableElement.h"
#include "RenderSVGResource.h"
#include "RenderSVGShadowTreeRootContainer.h"

namespace WebCore {

class RenderSVGResourceContainer : public RenderSVGHiddenContainer,
                                   public RenderSVGResource {
public:
    RenderSVGResourceContainer(SVGStyledElement*);
    virtual ~RenderSVGResourceContainer();

    void idChanged();

    virtual bool isSVGResourceContainer() const { return true; }
    virtual bool drawsContents() { return false; }

    virtual RenderSVGResourceContainer* toRenderSVGResourceContainer() { return this; }
    virtual bool childElementReferencesResource(const SVGRenderStyle*, const String&) const { return false; }

    static AffineTransform transformOnNonScalingStroke(RenderObject*, const AffineTransform& resourceTransform);

    bool containsCyclicReference(const Node* startNode) const;

private:
    friend class SVGResourcesCache;

    // FIXME: No-ops for now, until follow-up patch on bug 43031 lands.
    void addClient(RenderObject*) { }
    void removeClient(RenderObject*) { }

private:
    AtomicString m_id;
};

inline RenderSVGResourceContainer* getRenderSVGResourceContainerById(Document* document, const AtomicString& id)
{
    if (id.isEmpty())
        return 0;

    if (RenderSVGResourceContainer* renderResource = document->accessSVGExtensions()->resourceById(id))
        return renderResource;

    return 0;
}

template<typename Renderer>
Renderer* getRenderSVGResourceById(Document* document, const AtomicString& id)
{
    if (RenderSVGResourceContainer* container = getRenderSVGResourceContainerById(document, id))
        return container->cast<Renderer>();

    return 0;
}

}

#endif
#endif
