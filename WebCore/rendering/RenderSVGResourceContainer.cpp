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

static inline SVGDocumentExtensions* svgExtensionsFromNode(Node* node)
{
    ASSERT(node);
    ASSERT(node->document());
    return node->document()->accessSVGExtensions();
}

RenderSVGResourceContainer::RenderSVGResourceContainer(SVGStyledElement* node)
    : RenderSVGHiddenContainer(node)
    , m_id(node->hasID() ? node->getIdAttribute() : nullAtom)
    , m_registered(false)
{
}

RenderSVGResourceContainer::~RenderSVGResourceContainer()
{
    if (m_registered)
        svgExtensionsFromNode(node())->removeResource(m_id);
}

void RenderSVGResourceContainer::layout()
{
    // Invalidate all resources if our layout changed.
    if (m_everHadLayout && selfNeedsLayout())
        invalidateClients();

    RenderSVGHiddenContainer::layout();
}

void RenderSVGResourceContainer::destroy()
{
    SVGResourcesCache::resourceDestroyed(this);
    RenderSVGHiddenContainer::destroy();
}

void RenderSVGResourceContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderSVGHiddenContainer::styleDidChange(diff, oldStyle);

    if (!m_registered) {
        m_registered = true;
        registerResource();
    }
}

void RenderSVGResourceContainer::idChanged()
{
    // Invalidate all our current clients.
    invalidateClients();

    // Remove old id, that is guaranteed to be present in cache.
    SVGDocumentExtensions* extensions = svgExtensionsFromNode(node());
    extensions->removeResource(m_id);
    m_id = static_cast<Element*>(node())->getIdAttribute();

    registerResource();
}

void RenderSVGResourceContainer::markAllClientsForInvalidation(InvalidationMode mode)
{
    if (m_clients.isEmpty())
        return;

    bool needsLayout = mode == LayoutAndBoundariesInvalidation;

    HashSet<RenderObject*>::iterator end = m_clients.end();
    for (HashSet<RenderObject*>::iterator it = m_clients.begin(); it != end; ++it) {
        markClientForInvalidation(*it, mode);
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*it, needsLayout);
    }
}

void RenderSVGResourceContainer::markClientForInvalidation(RenderObject* client, InvalidationMode mode)
{
    ASSERT(client);
    ASSERT(!m_clients.isEmpty());

    switch (mode) {
    case LayoutAndBoundariesInvalidation:
    case BoundariesInvalidation:
        client->setNeedsBoundariesUpdate();
        break;
    case RepaintInvalidation:
        if (client->view())
            client->repaint();
        break;
    }
}

void RenderSVGResourceContainer::addClient(RenderObject* client)
{
    ASSERT(client);
    m_clients.add(client);
}

void RenderSVGResourceContainer::removeClient(RenderObject* client)
{
    ASSERT(client);
    m_clients.remove(client);
}

void RenderSVGResourceContainer::registerResource()
{
    SVGDocumentExtensions* extensions = svgExtensionsFromNode(node());
    if (!extensions->isPendingResource(m_id)) {
        extensions->addResource(m_id, this);
        return;
    }

    OwnPtr<HashSet<SVGStyledElement*> > clients(extensions->removePendingResource(m_id));

    // Cache us with the new id.
    extensions->addResource(m_id, this);

    // Update cached resources of pending clients.
    const HashSet<SVGStyledElement*>::const_iterator end = clients->end();
    for (HashSet<SVGStyledElement*>::const_iterator it = clients->begin(); it != end; ++it) {
        RenderObject* renderer = (*it)->renderer();
        if (!renderer)
            continue;
        SVGResourcesCache::clientUpdatedFromElement(renderer, renderer->style());
        renderer->setNeedsLayout(true);
    }
}

// FIXME: This does not belong here.
AffineTransform RenderSVGResourceContainer::transformOnNonScalingStroke(RenderObject* object, const AffineTransform& resourceTransform)
{
    if (!object->isRenderPath())
        return resourceTransform;

    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(object->node());
    AffineTransform transform = resourceTransform;
    transform.multiply(element->getScreenCTM());
    return transform;
}

}

#endif
