/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (c) 2023, 2024 Igalia S.L.
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
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGRoot.h"
#include "SVGElementTypeHelpers.h"
#include "SVGResourceElementClient.h"
#include "SVGVisitedElementTracking.h"
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGResourceContainer);

RenderSVGResourceContainer::RenderSVGResourceContainer(Type type, SVGElement& element, RenderStyle&& style)
    : RenderSVGHiddenContainer(type, element, WTFMove(style), SVGModelObjectFlag::IsResourceContainer)
    , m_id(element.getIdAttribute())
{
    ASSERT(isRenderSVGResourceContainer());
}

RenderSVGResourceContainer::~RenderSVGResourceContainer() = default;

void RenderSVGResourceContainer::willBeDestroyed()
{
    m_registered = false;
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
    // Remove old id, that is guaranteed to be present in cache.
    m_id = element().getIdAttribute();

    registerResource();
}

static inline void notifyResourceChanged(SVGElement& element)
{
    static NeverDestroyed<SVGVisitedElementTracking::VisitedSet> s_visitedSet;

    SVGVisitedElementTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(element))
        return;

    SVGVisitedElementTracking::Scope recursionScope(recursionTracking, element);

    for (auto& cssClient : element.referencingCSSClients()) {
        if (!cssClient)
            continue;
        cssClient->resourceChanged(element);
    }
}

void RenderSVGResourceContainer::registerResource()
{
    Ref treeScope = this->treeScopeForSVGReferences();
    if (!treeScope->isIdOfPendingSVGResource(m_id))
        return;

    auto elements = copyToVectorOf<Ref<SVGElement>>(treeScope->removePendingSVGResource(m_id));
    for (auto& element : elements) {
        ASSERT(element->hasPendingResources());
        treeScope->clearHasPendingSVGResourcesIfPossible(element);
        notifyResourceChanged(element.get());
    }
}

void RenderSVGResourceContainer::repaintAllClients() const
{
    Ref svgElement = element();
    notifyResourceChanged(svgElement.get());
}

}
