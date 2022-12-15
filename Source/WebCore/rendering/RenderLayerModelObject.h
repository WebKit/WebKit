/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

#include "PaintPhase.h"
#include "RenderElement.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class KeyframeList;
class RenderLayer;
class SVGGraphicsElement;

struct LayerRepaintRects {
    LayoutRect clippedOverflowRect;
    LayoutRect outlineBoundsRect;
};

class RenderLayerModelObject : public RenderElement {
    WTF_MAKE_ISO_ALLOCATED(RenderLayerModelObject);
public:
    virtual ~RenderLayerModelObject();

    void destroyLayer();

    bool hasSelfPaintingLayer() const;
    RenderLayer* layer() const { return m_layer.get(); }

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual bool requiresLayer() const = 0;

    // Returns true if the background is painted opaque in the given rect.
    // The query rect is given in local coordinate system.
    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const { return false; }

    // Returns false if the rect has no intersection with the applied clip rect. When the context specifies edge-inclusive
    // intersection, this return value allows distinguishing between no intersection and zero-area intersection.
    virtual bool applyCachedClipAndScrollPosition(LayoutRect&, const RenderLayerModelObject*, VisibleRectContext) const { return false; }

    virtual bool isScrollableOrRubberbandableBox() const { return false; }

    bool shouldPlaceVerticalScrollbarOnLeft() const;

    std::optional<LayerRepaintRects> layerRepaintRects() const;

    bool startAnimation(double timeOffset, const Animation&, const KeyframeList&) override;
    void animationPaused(double timeOffset, const String& name) override;
    void animationFinished(const String& name) override;
    void transformRelatedPropertyDidChange() override;

    void suspendAnimations(MonotonicTime = MonotonicTime()) override;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // Single source of truth deciding if a SVG renderer should be painted. All SVG renderers
    // use this method to test if they should continue processing in the paint() function or stop.
    bool shouldPaintSVGRenderer(const PaintInfo&, const OptionSet<PaintPhase> relevantPaintPhases = OptionSet<PaintPhase>()) const;

    // Provides the SVG implementation for computeVisibleRectInContainer().
    // This lives in RenderLayerModelObject, which is the common base-class for all SVG renderers.
    std::optional<LayoutRect> computeVisibleRectInSVGContainer(const LayoutRect&, const RenderLayerModelObject* container, VisibleRectContext) const;

    // Provides the SVG implementation for mapLocalToContainer().
    // This lives in RenderLayerModelObject, which is the common base-class for all SVG renderers.
    void mapLocalToSVGContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const;

    void applySVGTransform(TransformationMatrix&, const SVGGraphicsElement&, const RenderStyle&, const FloatRect& boundingBox, const std::optional<AffineTransform>& preApplySVGTransformMatrix, const std::optional<AffineTransform>& postApplySVGTransformMatrix, OptionSet<RenderStyle::TransformOperationOption>) const;
    void updateHasSVGTransformFlags();
    virtual bool needsHasSVGTransformFlags() const { ASSERT_NOT_REACHED(); return false; }

    LayoutPoint nominalSVGLayoutLocation() const { return flooredLayoutPoint(objectBoundingBoxWithoutTransformations().minXMinYCorner()); }
    virtual LayoutPoint currentSVGLayoutLocation() const { ASSERT_NOT_REACHED(); return { }; }
    virtual void setCurrentSVGLayoutLocation(const LayoutPoint&) { ASSERT_NOT_REACHED(); }
#endif

    TransformationMatrix* layerTransform() const;

    virtual void updateLayerTransform();
    virtual void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const = 0;

protected:
    RenderLayerModelObject(Element&, RenderStyle&&, BaseTypeFlags);
    RenderLayerModelObject(Document&, RenderStyle&&, BaseTypeFlags);

    void createLayer();
    void willBeDestroyed() override;
    void willBeRemovedFromTree(IsInternalMove) override;

    virtual void updateFromStyle() { }

private:
    std::unique_ptr<RenderLayer> m_layer;

    // Used to store state between styleWillChange and styleDidChange
    static bool s_wasFloating;
    static bool s_hadLayer;
    static bool s_hadTransform;
    static bool s_layerWasSelfPainting;
};

// Pixel-snapping (== 'device pixel alignment') helpers.
bool rendererNeedsPixelSnapping(const RenderLayerModelObject&);
FloatRect snapRectToDevicePixelsIfNeeded(const LayoutRect&, const RenderLayerModelObject&);
FloatRect snapRectToDevicePixelsIfNeeded(const FloatRect&, const RenderLayerModelObject&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderLayerModelObject, isRenderLayerModelObject())
