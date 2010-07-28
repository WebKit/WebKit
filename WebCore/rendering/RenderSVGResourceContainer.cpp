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

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGResourceContainer.h"

#include "RenderSVGShadowTreeRootContainer.h"
#include "SVGStyledTransformableElement.h"

namespace WebCore {

RenderSVGResourceContainer::RenderSVGResourceContainer(SVGStyledElement* node)
    : RenderSVGHiddenContainer(node)
    , RenderSVGResource()
    , m_id(node->hasID() ? node->getIdAttribute() : nullAtom)
{
    ASSERT(node->document());
    node->document()->accessSVGExtensions()->addResource(m_id, this);
}

RenderSVGResourceContainer::~RenderSVGResourceContainer()
{
    ASSERT(node());
    ASSERT(node()->document());
    node()->document()->accessSVGExtensions()->removeResource(m_id);
}

void RenderSVGResourceContainer::idChanged()
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

AffineTransform RenderSVGResourceContainer::transformOnNonScalingStroke(RenderObject* object, const AffineTransform& resourceTransform)
{
    if (!object->isRenderPath())
        return resourceTransform;

    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(object->node());
    AffineTransform transform = resourceTransform;
    transform.multiply(element->getScreenCTM());
    return transform;
}

bool RenderSVGResourceContainer::containsCyclicReference(const Node* startNode) const
{
    ASSERT(startNode->document());

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

        // Dive into shadow tree to check for cycles there.
        if (node->hasTagName(SVGNames::useTag)) {
            ASSERT(renderer->isSVGShadowTreeRootContainer());
            if (Node* shadowRoot = static_cast<RenderSVGShadowTreeRootContainer*>(renderer)->rootElement()) {
                if (containsCyclicReference(shadowRoot))
                    return true;
            }

        }

        if (node->hasChildNodes()) {
            if (containsCyclicReference(node))
                return true;
        }
    }

    return false;
}

}

#endif
