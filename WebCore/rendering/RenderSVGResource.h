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
    MarkerResourceType,
    FilterResourceType,
    ClipperResourceType
};

class RenderSVGResource : public RenderSVGHiddenContainer {
public:
    RenderSVGResource(SVGStyledElement* node)
        : RenderSVGHiddenContainer(node)
        , m_id(node->getIDAttribute())
    {
        ASSERT(node->document());
        node->document()->accessSVGExtensions()->addResource(m_id, this);
    }

    virtual ~RenderSVGResource()
    {
        ASSERT(node());
        ASSERT(node()->document());
        node()->document()->accessSVGExtensions()->removeResource(m_id);
    }

    void idChanged()
    {
        ASSERT(node());
        ASSERT(node()->document());
        SVGDocumentExtensions* extensions = node()->document()->accessSVGExtensions();

        // Remove old id, that is guaranteed to be present in cache
        extensions->removeResource(m_id);

        m_id = static_cast<Element*>(node())->getIDAttribute();

        // It's possible that an element is referencing us with the new id, and has to be notified that we're existing now
        if (extensions->isPendingResource(m_id)) {
            OwnPtr<HashSet<SVGStyledElement*> > clients(extensions->removePendingResource(m_id));
            if (clients->isEmpty())
                return;

            HashSet<SVGStyledElement*>::const_iterator it = clients->begin();
            const HashSet<SVGStyledElement*>::const_iterator end = clients->end();

            for (; it != end; ++it) {
                if (RenderObject* renderer = (*it)->renderer())
                    renderer->setNeedsLayout(true);
            }
        }

        // Recache us with the new id
        extensions->addResource(m_id, this);
    }

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

    virtual bool applyResource(RenderObject*, GraphicsContext*&) = 0;
    virtual void postApplyResource(RenderObject*, GraphicsContext*&) { }
    virtual FloatRect resourceBoundingBox(const FloatRect&) const = 0;

    virtual RenderSVGResourceType resourceType() const = 0;

private:
    AtomicString m_id;
};

template<typename Renderer>
Renderer* getRenderSVGResourceById(Document* document, const AtomicString& id)
{
    if (id.isEmpty())
        return 0;

    if (RenderSVGResource* renderResource = document->accessSVGExtensions()->resourceById(id))
        return renderResource->cast<Renderer>();

    return 0;
}

}

#endif
#endif
