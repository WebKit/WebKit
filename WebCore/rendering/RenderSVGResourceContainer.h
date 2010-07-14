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

namespace WebCore {

class RenderSVGResourceContainer : public RenderSVGHiddenContainer,
                                   public RenderSVGResource {
public:
    RenderSVGResourceContainer(SVGStyledElement* node)
        : RenderSVGHiddenContainer(node)
        , RenderSVGResource()
        , m_id(node->hasID() ? node->getIdAttribute() : nullAtom)
    {
        ASSERT(node->document());
        node->document()->accessSVGExtensions()->addResource(m_id, this);
    }

    virtual ~RenderSVGResourceContainer()
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
        m_id = static_cast<Element*>(node())->getIdAttribute();

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

    virtual bool isSVGResourceContainer() const { return true; }
    virtual bool drawsContents() { return false; }

    virtual RenderSVGResourceContainer* toRenderSVGResourceContainer() { return this; }
    virtual bool childElementReferencesResource(const SVGRenderStyle*, const String&) const { return false; }

    static AffineTransform transformOnNonScalingStroke(RenderObject* object, const AffineTransform resourceTransform)
    {
        if (!object->isRenderPath())
            return resourceTransform;

        SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(object->node());
        AffineTransform transform = resourceTransform;
        transform.multiply(element->getScreenCTM());
        return transform;
    }

    bool containsCyclicReference(const Node* startNode) const
    {
        Document* document = startNode->document();
        ASSERT(document);
    
        for (Node* node = startNode->firstChild(); node; node = node->nextSibling()) {
            if (!node->isSVGElement())
                continue;
    
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;
    
            RenderStyle* style = renderer->style();
            if (!style)
                continue;
    
            const SVGRenderStyle* svgStyle = style->svgStyle();
            ASSERT(svgStyle);
    
            // Let the class inheriting from us decide whether the child element references ourselves.
            if (childElementReferencesResource(svgStyle, m_id))
                return true;
    
            if (node->hasChildNodes()) {
                if (containsCyclicReference(node))
                    return true;
            }
        }
    
        return false;
    }

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
