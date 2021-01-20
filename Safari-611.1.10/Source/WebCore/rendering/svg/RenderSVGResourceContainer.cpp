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
 */

#include "config.h"
#include "RenderSVGResourceContainer.h"

#include "RenderLayer.h"
#include "RenderSVGRoot.h"
#include "RenderView.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceContainer);

static inline SVGDocumentExtensions& svgExtensionsFromElement(SVGElement& element)
{
    return element.document().accessSVGExtensions();
}

RenderSVGResourceContainer::RenderSVGResourceContainer(SVGElement& element, RenderStyle&& style)
    : RenderSVGHiddenContainer(element, WTFMove(style))
    , m_id(element.getIdAttribute())
{
}

RenderSVGResourceContainer::~RenderSVGResourceContainer() = default;

void RenderSVGResourceContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    // Invalidate all resources if our layout changed.
    if (selfNeedsClientInvalidation())
        RenderSVGRoot::addResourceForClientInvalidation(this);

    RenderSVGHiddenContainer::layout();
}

void RenderSVGResourceContainer::willBeDestroyed()
{
    SVGResourcesCache::resourceDestroyed(*this);

    if (m_registered) {
        svgExtensionsFromElement(element()).removeResource(m_id);
        m_registered = false;
    }

    RenderSVGHiddenContainer::willBeDestroyed();
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
    removeAllClientsFromCache();

    // Remove old id, that is guaranteed to be present in cache.
    svgExtensionsFromElement(element()).removeResource(m_id);
    m_id = element().getIdAttribute();

    registerResource();
}

void RenderSVGResourceContainer::markAllClientsForRepaint()
{
    markAllClientsForInvalidation(RepaintInvalidation);
}

void RenderSVGResourceContainer::markAllClientsForInvalidation(InvalidationMode mode)
{
    // FIXME: Style invalidation should either be a pre-layout task or this function
    // should never get called while in layout. See webkit.org/b/208903.
    if ((m_clients.isEmpty() && m_clientLayers.isEmpty()) || m_isInvalidating)
        return;

    SetForScope<bool> isInvalidating(m_isInvalidating, true);

    bool needsLayout = mode == LayoutAndBoundariesInvalidation;
    bool markForInvalidation = mode != ParentOnlyInvalidation;
    auto* root = SVGRenderSupport::findTreeRootObject(*this);

    for (auto* client : m_clients) {
        // We should not mark any client outside the current root for invalidation
        if (root != SVGRenderSupport::findTreeRootObject(*client))
            continue;

        if (is<RenderSVGResourceContainer>(*client)) {
            downcast<RenderSVGResourceContainer>(*client).removeAllClientsFromCache(markForInvalidation);
            continue;
        }

        if (markForInvalidation)
            markClientForInvalidation(*client, mode);

        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*client, needsLayout);
    }

    markAllClientLayersForInvalidation();
}

void RenderSVGResourceContainer::markAllClientLayersForInvalidation()
{
    if (m_clientLayers.isEmpty())
        return;

    auto& document = (*m_clientLayers.begin())->renderer().document();
    if (!document.view() || document.renderTreeBeingDestroyed())
        return;

    auto inLayout = document.view()->layoutContext().isInLayout();
    for (auto* clientLayer : m_clientLayers) {
        // FIXME: We should not get here while in layout. See webkit.org/b/208903.
        // Repaint should also be triggered through some other means.
        if (inLayout) {
            clientLayer->renderer().repaint();
            continue;
        }
        if (auto* enclosingElement = clientLayer->enclosingElement())
            enclosingElement->invalidateStyleAndLayerComposition();
        clientLayer->renderer().repaint();
    }
}

void RenderSVGResourceContainer::markClientForInvalidation(RenderObject& client, InvalidationMode mode)
{
    ASSERT(!m_clients.isEmpty());

    switch (mode) {
    case LayoutAndBoundariesInvalidation:
    case BoundariesInvalidation:
        client.setNeedsBoundariesUpdate();
        break;
    case RepaintInvalidation:
        if (!client.renderTreeBeingDestroyed())
            client.repaint();
        break;
    case ParentOnlyInvalidation:
        break;
    }
}

void RenderSVGResourceContainer::addClient(RenderElement& client)
{
    m_clients.add(&client);
}

void RenderSVGResourceContainer::removeClient(RenderElement& client)
{
    removeClientFromCache(client, false);
    m_clients.remove(&client);
}

void RenderSVGResourceContainer::addClientRenderLayer(RenderLayer* client)
{
    ASSERT(client);
    m_clientLayers.add(client);
}

void RenderSVGResourceContainer::removeClientRenderLayer(RenderLayer* client)
{
    ASSERT(client);
    m_clientLayers.remove(client);
}

void RenderSVGResourceContainer::registerResource()
{
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    if (!extensions.isIdOfPendingResource(m_id)) {
        extensions.addResource(m_id, *this);
        return;
    }

    std::unique_ptr<SVGDocumentExtensions::PendingElements> clients = extensions.removePendingResource(m_id);

    // Cache us with the new id.
    extensions.addResource(m_id, *this);

    // Update cached resources of pending clients.
    for (auto* client : *clients) {
        ASSERT(client->hasPendingResources());
        extensions.clearHasPendingResourcesIfPossible(*client);
        auto* renderer = client->renderer();
        if (!renderer)
            continue;
        SVGResourcesCache::clientStyleChanged(*renderer, StyleDifference::Layout, renderer->style());
        renderer->setNeedsLayout();
    }
}

bool RenderSVGResourceContainer::shouldTransformOnTextPainting(const RenderElement& renderer, AffineTransform& resourceTransform)
{
#if USE(CG)
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(resourceTransform);
    return false;
#else
    // This method should only be called for RenderObjects that deal with text rendering. Cmp. RenderObject.h's is*() methods.
    ASSERT(renderer.isSVGText() || renderer.isSVGTextPath() || renderer.isSVGInline());

    // In text drawing, the scaling part of the graphics context CTM is removed, compare SVGInlineTextBox::paintTextWithShadows.
    // So, we use that scaling factor here, too, and then push it down to pattern or gradient space
    // in order to keep the pattern or gradient correctly scaled.
    float scalingFactor = SVGRenderingContext::calculateScreenFontSizeScalingFactor(renderer);
    if (scalingFactor == 1)
        return false;
    resourceTransform.scale(scalingFactor);
    return true;
#endif
}

// FIXME: This does not belong here.
AffineTransform RenderSVGResourceContainer::transformOnNonScalingStroke(RenderObject* object, const AffineTransform& resourceTransform)
{
    if (!object->isSVGShape())
        return resourceTransform;

    SVGGraphicsElement* element = downcast<SVGGraphicsElement>(object->node());
    AffineTransform transform = element->getScreenCTM(SVGLocatable::DisallowStyleUpdate);
    transform *= resourceTransform;
    return transform;
}

}
