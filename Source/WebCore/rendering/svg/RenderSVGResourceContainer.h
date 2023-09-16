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

#pragma once

#include "LegacyRenderSVGHiddenContainer.h"
#include "RenderSVGResource.h"
#include "SVGDocumentExtensions.h"
#include <wtf/WeakHashSet.h>

namespace WebCore {

class RenderLayer;

class RenderSVGResourceContainer : public LegacyRenderSVGHiddenContainer,
                                   public RenderSVGResource {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceContainer);
public:
    virtual ~RenderSVGResourceContainer();

    void layout() override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;

    bool isSVGResourceContainer() const final { return true; }

    static float computeTextPaintingScale(const RenderElement&);
    static AffineTransform transformOnNonScalingStroke(RenderObject*, const AffineTransform& resourceTransform);

    void idChanged();
    void markAllClientsForRepaint();
    void addClientRenderLayer(RenderLayer&);
    void removeClientRenderLayer(RenderLayer&);
    void markAllClientLayersForInvalidation();

protected:
    RenderSVGResourceContainer(SVGElement&, RenderStyle&&);

    enum InvalidationMode {
        LayoutAndBoundariesInvalidation,
        BoundariesInvalidation,
        RepaintInvalidation,
        ParentOnlyInvalidation
    };

    // Used from the invalidateClient/invalidateClients methods from classes, inheriting from us.
    virtual bool selfNeedsClientInvalidation() const { return everHadLayout() && selfNeedsLayout(); }

    void markAllClientsForInvalidation(InvalidationMode);
    void markClientForInvalidation(RenderObject&, InvalidationMode);

private:
    friend class SVGResourcesCache;
    void addClient(RenderElement&);
    void removeClient(RenderElement&);

    void willBeDestroyed() final;
    void registerResource();

    AtomString m_id;
    WeakHashSet<RenderElement> m_clients;
    WeakHashSet<RenderLayer> m_clientLayers;
    bool m_registered { false };
    bool m_isInvalidating { false };
};

inline RenderSVGResourceContainer* getRenderSVGResourceContainerById(TreeScope& treeScope, const AtomString& id)
{
    if (id.isEmpty())
        return nullptr;

    if (RenderSVGResourceContainer* renderResource = treeScope.svgResourceById(id))
        return renderResource;

    return nullptr;
}

template<typename Renderer>
Renderer* getRenderSVGResourceById(TreeScope& treeScope, const AtomString& id)
{
    // Using the RenderSVGResource type here avoids ambiguous casts for types that
    // descend from both RenderObject and RenderSVGResourceContainer.
    RenderSVGResource* container = getRenderSVGResourceContainerById(treeScope, id);
    if (is<Renderer>(container))
        return downcast<Renderer>(container);

    return nullptr;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceContainer, isSVGResourceContainer())
