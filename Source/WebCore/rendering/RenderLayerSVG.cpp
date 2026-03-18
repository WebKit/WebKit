/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RenderLayerSVG.h"

#include "RenderLayerInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_PREFERABLY_COMPACT_TZONE_ALLOCATED_IMPL(RenderLayerSVG);

RenderLayerSVG::RenderLayerSVG(RenderLayerModelObject& renderer)
    : RenderLayer(renderer, RenderLayerType::SVG)
{
}

RenderLayerSVG::~RenderLayerSVG() = default;

void RenderLayerSVG::updateSVGSpecificAncestorState()
{
    for (auto* ancestor = renderer().parent(); ancestor; ancestor = ancestor->parent()) {
        if (auto* container = dynamicDowncast<RenderSVGHiddenContainer>(ancestor)) {
            m_enclosingSVGHiddenOrResourceContainer = container;
            return;
        }
    }

    m_enclosingSVGHiddenOrResourceContainer = nullptr;
}

bool RenderLayerSVG::hasVisibleContentForPainting() const
{
    if (!hasVisibleContent())
        return false;

    if (!m_enclosingSVGHiddenOrResourceContainer)
        return true;

    // Hidden SVG containers (<defs> / <symbol> ...) and their children are never painted directly.
    CheckedPtr container = m_enclosingSVGHiddenOrResourceContainer.get();
    if (!is<RenderSVGResourceContainer>(container.get()))
        return false;

    // SVG resource layers and their children are only painted indirectly, via paintSVGResourceLayer().
    ASSERT(container->hasLayer());
    CheckedPtr containerLayer = container->layer();
    return containerLayer->isPaintingSVGResourceLayer();
}

void RenderLayerSVG::paintSVGResourceLayer(GraphicsContext& context, const AffineTransform& layerContentTransform)
{
    bool wasPaintingSVGResourceLayer = m_isPaintingSVGResourceLayer;
    m_isPaintingSVGResourceLayer = true;
    context.concatCTM(layerContentTransform);

    auto localPaintDirtyRect = LayoutRect::infiniteRect();

    CheckedPtr rootPaintingLayer = [&] () -> RenderLayer* {
        auto* curr = parent();
        while (curr && !(curr->renderer().isAnonymous() && is<RenderSVGViewportContainer>(curr->renderer())))
            curr = curr->parent();
        return curr;
    }();
    ASSERT(rootPaintingLayer);

    LayerPaintingInfo paintingInfo(rootPaintingLayer, localPaintDirtyRect, PaintBehavior::Normal, LayoutSize());

    OptionSet<PaintLayerFlag> flags { PaintLayerFlag::TemporaryClipRects };
    if (!renderer().hasNonVisibleOverflow())
        flags.add({ PaintLayerFlag::PaintingOverflowContents, PaintLayerFlag::PaintingOverflowContentsRoot });

    paintLayer(context, paintingInfo, flags);

    m_isPaintingSVGResourceLayer = wasPaintingSVGResourceLayer;
}

} // namespace WebCore
