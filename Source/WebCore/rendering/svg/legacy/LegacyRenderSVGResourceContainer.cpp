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
#include "LegacyRenderSVGResourceContainer.h"

#include "LegacyRenderSVGRoot.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGraphicsElement.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceContainer);

LegacyRenderSVGResourceContainer::LegacyRenderSVGResourceContainer(Type type, SVGElement& element, RenderStyle&& style)
    : LegacyRenderSVGHiddenContainer(type, element, WTFMove(style), SVGModelObjectFlag::IsResourceContainer)
    , m_id(element.getIdAttribute())
{
}

LegacyRenderSVGResourceContainer::~LegacyRenderSVGResourceContainer() = default;

void LegacyRenderSVGResourceContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    // Invalidate all resources if our layout changed.
    if (selfNeedsClientInvalidation())
        LegacyRenderSVGRoot::addResourceForClientInvalidation(this);

    LegacyRenderSVGHiddenContainer::layout();
}

void LegacyRenderSVGResourceContainer::willBeDestroyed()
{
    SVGResourcesCache::resourceDestroyed(*this);

    if (m_registered) {
        treeScopeForSVGReferences().removeSVGResource(m_id);
        m_registered = false;
    }

    LegacyRenderSVGHiddenContainer::willBeDestroyed();
}

void LegacyRenderSVGResourceContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    LegacyRenderSVGHiddenContainer::styleDidChange(diff, oldStyle);

    if (!m_registered) {
        m_registered = true;
        registerResource();
    }
}

void LegacyRenderSVGResourceContainer::idChanged()
{
    // Invalidate all our current clients.
    removeAllClientsFromCache();

    // Remove old id, that is guaranteed to be present in cache.
    treeScopeForSVGReferences().removeSVGResource(m_id);
    m_id = element().getIdAttribute();

    registerResource();
}

void LegacyRenderSVGResourceContainer::markAllClientsForRepaint()
{
    markAllClientsForInvalidation(RepaintInvalidation);
}

void LegacyRenderSVGResourceContainer::markAllClientsForInvalidation(InvalidationMode mode)
{
    SingleThreadWeakHashSet<RenderObject> visitedRenderers;
    markAllClientsForInvalidationIfNeeded(mode, &visitedRenderers);
}

void LegacyRenderSVGResourceContainer::markAllClientsForInvalidationIfNeeded(InvalidationMode mode, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    // FIXME: Style invalidation should either be a pre-layout task or this function
    // should never get called while in layout. See webkit.org/b/208903.
    if ((m_clients.isEmptyIgnoringNullReferences() && m_clientLayers.isEmptyIgnoringNullReferences()) || m_isInvalidating)
        return;

    SetForScope isInvalidating(m_isInvalidating, true);

    bool needsLayout = mode == LayoutAndBoundariesInvalidation;
    bool markForInvalidation = mode != ParentOnlyInvalidation;
    auto* root = SVGRenderSupport::findTreeRootObject(*this);

    for (auto& client : m_clients) {
        // We should not mark any client outside the current root for invalidation
        if (root != SVGRenderSupport::findTreeRootObject(client))
            continue;

        if (CheckedPtr container = dynamicDowncast<LegacyRenderSVGResourceContainer>(client)) {
            container->removeAllClientsFromCacheIfNeeded(markForInvalidation, visitedRenderers);
            continue;
        }

        if (markForInvalidation)
            markClientForInvalidation(client, mode);

        LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidationIfNeeded(client, needsLayout, visitedRenderers);
    }

    markAllClientLayersForInvalidation();
}

void LegacyRenderSVGResourceContainer::markAllClientLayersForInvalidation()
{
    if (m_clientLayers.isEmptyIgnoringNullReferences())
        return;

    auto& document = (*m_clientLayers.begin()).renderer().document();
    if (!document.view() || document.renderTreeBeingDestroyed())
        return;

    auto inLayout = document.view()->layoutContext().isInLayout();
    for (auto& clientLayer : m_clientLayers) {
        // FIXME: We should not get here while in layout. See webkit.org/b/208903.
        // Repaint should also be triggered through some other means.
        if (inLayout) {
            clientLayer.renderer().repaint();
            continue;
        }
        if (auto* enclosingElement = clientLayer.enclosingElement())
            enclosingElement->invalidateStyleAndLayerComposition();
        clientLayer.renderer().repaint();
    }
}

void LegacyRenderSVGResourceContainer::markClientForInvalidation(RenderObject& client, InvalidationMode mode)
{
    ASSERT(!m_clients.isEmptyIgnoringNullReferences());

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

void LegacyRenderSVGResourceContainer::addClient(RenderElement& client)
{
    m_clients.add(client);
}

void LegacyRenderSVGResourceContainer::removeClient(RenderElement& client)
{
    removeClientFromCache(client, false);
    m_clients.remove(client);
}

void LegacyRenderSVGResourceContainer::addClientRenderLayer(RenderLayer& client)
{
    m_clientLayers.add(client);
}

void LegacyRenderSVGResourceContainer::removeClientRenderLayer(RenderLayer& client)
{
    m_clientLayers.remove(client);
}

void LegacyRenderSVGResourceContainer::registerResource()
{
    auto& treeScope = this->treeScopeForSVGReferences();
    if (!treeScope.isIdOfPendingSVGResource(m_id)) {
        treeScope.addSVGResource(m_id, *this);
        return;
    }

    auto elements = copyToVectorOf<Ref<SVGElement>>(treeScope.removePendingSVGResource(m_id));

    treeScope.addSVGResource(m_id, *this);

    // Update cached resources of pending clients.
    for (auto& client : elements) {
        ASSERT(client->hasPendingResources());
        treeScope.clearHasPendingSVGResourcesIfPossible(client);
        auto* renderer = client->renderer();
        if (!renderer)
            continue;
        SVGResourcesCache::clientStyleChanged(*renderer, StyleDifference::Layout, nullptr, renderer->style());
        renderer->setNeedsLayout();
    }
}

float LegacyRenderSVGResourceContainer::computeTextPaintingScale(const RenderElement& renderer)
{
#if USE(CG)
    UNUSED_PARAM(renderer);
    return 1;
#else
    // This method should only be called for RenderObjects that deal with text rendering. Cmp. RenderObject.h's is*() methods.
    ASSERT(renderer.isRenderSVGText() || renderer.isRenderSVGTextPath() || renderer.isRenderSVGInline());

    // In text drawing, the scaling part of the graphics context CTM is removed, compare SVGInlineTextBox::paintTextWithShadows.
    // So, we use that scaling factor here, too, and then push it down to pattern or gradient space
    // in order to keep the pattern or gradient correctly scaled.
    return SVGRenderingContext::calculateScreenFontSizeScalingFactor(renderer);
#endif
}

// FIXME: This does not belong here.
AffineTransform LegacyRenderSVGResourceContainer::transformOnNonScalingStroke(RenderObject* object, const AffineTransform& resourceTransform)
{
    if (!object->isRenderOrLegacyRenderSVGShape())
        return resourceTransform;

    SVGGraphicsElement* element = downcast<SVGGraphicsElement>(object->node());
    AffineTransform transform = element->getScreenCTM(SVGLocatable::DisallowStyleUpdate);
    transform *= resourceTransform;
    return transform;
}

}
