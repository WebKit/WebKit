/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L.
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

#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderSVGResourceClipper.h"

namespace WebCore {

inline bool RenderLayer::canPaintTransparencyWithSetOpacity() const { return isBitmapOnly() && !hasNonOpacityTransparency(); }
inline bool RenderLayer::hasBackdropFilter() const { return renderer().hasBackdropFilter(); }
inline bool RenderLayer::hasFilter() const { return renderer().hasFilter(); }
inline bool RenderLayer::hasPerspective() const { return renderer().style().hasPerspective(); }
inline bool RenderLayer::isTransformed() const { return renderer().isTransformed(); }
inline bool RenderLayer::isTransparent() const { return renderer().isTransparent() || renderer().hasMask(); }
inline bool RenderLayer::overlapBoundsIncludeChildren() const { return hasFilter() && renderer().style().filter().hasFilterThatMovesPixels(); }
inline bool RenderLayer::preserves3D() const { return renderer().style().preserves3D(); }
inline int RenderLayer::zIndex() const { return renderer().style().usedZIndex(); }

inline bool RenderLayer::hasBlendMode() const { return renderer().hasBlendMode(); } // FIXME: Why ask the renderer this given we have m_blendMode?

inline bool RenderLayer::canUseOffsetFromAncestor() const
{
    // FIXME: This really needs to know if there are transforms on this layer and any of the layers between it and the ancestor in question.
    return !isTransformed() && !renderer().isRenderOrLegacyRenderSVGRoot();
}

inline bool RenderLayer::paintsWithTransparency(OptionSet<PaintBehavior> paintBehavior) const
{
    if (!renderer().isTransparent() && !hasNonOpacityTransparency())
        return false;
    return (paintBehavior & PaintBehavior::FlattenCompositingLayers) || !isComposited();
}

inline bool RenderLayer::hasNonOpacityTransparency() const
{
    if (renderer().hasMask())
        return true;

    if (hasBlendMode() || isolatesBlending())
        return true;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (!renderer().document().settings().layerBasedSVGEngineEnabled())
        return false;

    // SVG clip-paths may use clipping masks, if so, flag this layer as transparent.
    if (auto* svgClipper = renderer().svgClipperResourceFromStyle(); svgClipper && !svgClipper->shouldApplyPathClipping())
        return true;
#endif

    return false;
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
inline RenderSVGHiddenContainer* RenderLayer::enclosingSVGHiddenOrResourceContainer() const
{
    return m_enclosingSVGHiddenOrResourceContainer.get();
}
#endif

} // namespace WebCore
