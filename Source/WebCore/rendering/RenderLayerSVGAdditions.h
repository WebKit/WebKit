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

#pragma once

#include <WebCore/LayoutRect.h>
#include <WebCore/PaintPhase.h>
#include <wtf/InlineWeakPtr.h>
#include <wtf/OptionSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderElement;
class RenderLayer;

class SVGPaintOrderLayerItem {
public:
    static SVGPaintOrderLayerItem makeLayered(RenderElement& renderer, RenderLayer& layer, int zIndex)
    {
        return SVGPaintOrderLayerItem { &renderer, &layer, zIndex, { }, { } };
    }

    static SVGPaintOrderLayerItem makeAtomic(RenderElement& renderer, LayoutSize ancestorOffset)
    {
        return SVGPaintOrderLayerItem { &renderer, nullptr, 0, { PaintPhase::Foreground, PaintPhase::Outline }, ancestorOffset };
    }

    static SVGPaintOrderLayerItem makeOutlineOnly(RenderElement& renderer, LayoutSize ancestorOffset)
    {
        return SVGPaintOrderLayerItem { &renderer, nullptr, 0, { PaintPhase::SelfOutline }, ancestorOffset };
    }

    SingleThreadWeakPtr<RenderElement> renderer;
    InlineWeakPtr<RenderLayer> layer; // null for non-layer children
    int zIndex { 0 };
    OptionSet<PaintPhase> phasesToPaint; // Empty for layered children; drives the non-layer paint loop.
    LayoutSize accumulatedAncestorOffset; // Precomputed offset from non-layered ancestors between child and layer's renderer.

private:
    SVGPaintOrderLayerItem(RenderElement* renderer, RenderLayer* layer, int zIndex, OptionSet<PaintPhase> phasesToPaint, LayoutSize accumulatedAncestorOffset)
        : renderer(renderer)
        , layer(layer)
        , zIndex(zIndex)
        , phasesToPaint(phasesToPaint)
        , accumulatedAncestorOffset(accumulatedAncestorOffset)
    {
    }
};

} // namespace WebCore
