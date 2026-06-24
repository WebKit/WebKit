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

#include <WebCore/PaintPhase.h>
#include <WebCore/RenderElement.h>
#include <wtf/OptionSet.h>
#include <wtf/UniquelyOwned.h>

namespace WebCore {

class BlendingKeyframes;
class GraphicsLayerAnimation;
class LegacyRenderSVGResourceClipper;
class RenderLayer;
class RenderSVGResourceClipper;
class RenderSVGResourceFilter;
class RenderSVGResourceMarker;
class RenderSVGResourceMasker;
class RenderSVGResourcePaintServer;
class SVGGraphicsElement;

namespace Style {
struct SVGMarkerResource;
enum class TransformResolverOption : uint8_t;
}

class RenderLayerModelObject : public RenderElement {
    WTF_MAKE_TZONE_ALLOCATED(RenderLayerModelObject);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderLayerModelObject);
public:
    virtual ~RenderLayerModelObject();

    void destroyLayer();

    bool NODELETE hasSelfPaintingLayer() const;
    RenderLayer* layer() const LIFETIME_BOUND { return m_layer.get(); }

    void styleWillChange(Style::Difference, const Style::ComputedStyle& newStyle) override;
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) override;

    virtual bool requiresLayer() const = 0;
    bool requiresLayerForSVGIntrinsicReasons() const;

    // Returns true if the background is painted opaque in the given rect.
    // The query rect is given in local coordinate system.
    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const { return false; }

    // Returns false if the rect has no intersection with the applied clip rect. When the context specifies edge-inclusive
    // intersection, this return value allows distinguishing between no intersection and zero-area intersection.
    virtual bool applyCachedClipAndScrollPosition(RepaintRects&, const RenderLayerModelObject*, VisibleRectContext) const;

    virtual bool isScrollableOrRubberbandableBox() const { return false; }

    bool NODELETE shouldPlaceVerticalScrollbarOnLeft() const;

    std::optional<LayoutRect> NODELETE cachedLayerClippedOverflowRect() const;

    bool startAnimation(double timeOffset, const GraphicsLayerAnimation&, const BlendingKeyframes&) override;
    void animationPaused(double timeOffset, const BlendingKeyframes&) override;
    void animationFinished(const BlendingKeyframes&) override;
    void transformRelatedPropertyDidChange() override;

    void suspendAnimations(MonotonicTime = MonotonicTime()) override;

    // Single source of truth deciding if a SVG renderer should be painted. All SVG renderers
    // use this method to test if they should continue processing in the paint() function or stop.
    bool shouldPaintSVGRenderer(const PaintInfo&, const OptionSet<PaintPhase> relevantPaintPhases = OptionSet<PaintPhase>()) const;

    // Provides the SVG implementation for computeVisibleRectsInContainer().
    // This lives in RenderLayerModelObject, which is the common base-class for all SVG renderers.
    std::optional<RepaintRects> computeVisibleRectsInSVGContainer(const RepaintRects&, const RenderLayerModelObject* container, VisibleRectContext) const;

    // Provides the SVG implementation for mapLocalToContainer().
    // This lives in RenderLayerModelObject, which is the common base-class for all SVG renderers.
    void mapLocalToSVGContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const;

    void applySVGTransform(TransformationMatrix&, const SVGGraphicsElement&, const Style::ComputedStyle&, const FloatRect& boundingBox, const std::optional<AffineTransform>& preApplySVGTransformMatrix, const std::optional<AffineTransform>& postApplySVGTransformMatrix, OptionSet<Style::TransformResolverOption>) const;
    void updateHasSVGTransformFlags();
    virtual bool needsHasSVGTransformFlags() const { ASSERT_NOT_REACHED(); return false; }

    enum class SVGAttributeChangeRepaintMode : bool {
        // Issue a full repaint at the new position from inside the function.
        Issue,
        // Skip the post-mutation repaint - the caller will emit a delta repaint
        // (via repaintAfterLayoutIfNeeded()) using a pre-mutation rect snapshot.
        Defer
    };
    void updateTransformAndRepaintForSVGAfterAttributeChange(SVGAttributeChangeRepaintMode = SVGAttributeChangeRepaintMode::Issue);
    bool svgTransformAttributeChangeInducesLayerComposition();

    LayoutPoint nominalSVGLayoutLocation() const { return flooredLayoutPoint(objectBoundingBoxWithoutTransformations().minXMinYCorner()); }
    LayoutPoint objectBoundingBoxLocation() const { return flooredLayoutPoint(objectBoundingBox().minXMinYCorner()); }
    virtual LayoutPoint currentSVGLayoutLocation() const { ASSERT_NOT_REACHED(); return { }; }
    virtual void setCurrentSVGLayoutLocation(const LayoutPoint&) { ASSERT_NOT_REACHED(); }

    RenderSVGResourcePaintServer* svgFillPaintServerResourceFromStyle(const Style::ComputedStyle&) const;
    RenderSVGResourcePaintServer* svgStrokePaintServerResourceFromStyle(const Style::ComputedStyle&) const;

    RenderSVGResourceClipper* svgClipperResourceFromStyle() const;
    RenderSVGResourceFilter* svgFilterResourceFromStyle() const;
    RenderSVGResourceMasker* svgMaskerResourceFromStyle() const;
    RenderSVGResourceMarker* svgMarkerStartResourceFromStyle() const;
    RenderSVGResourceMarker* svgMarkerMidResourceFromStyle() const;
    RenderSVGResourceMarker* svgMarkerEndResourceFromStyle() const;

    LegacyRenderSVGResourceClipper* legacySVGClipperResourceFromStyle() const;

    bool pointInSVGClippingArea(const FloatPoint&) const;

    void paintSVGClippingMask(PaintInfo&, const FloatRect& objectBoundingBox) const;
    void paintSVGMask(PaintInfo&, const LayoutPoint& adjustedPaintOffset) const;
    void paintSVGEventRegion(PaintInfo&, const LayoutPoint& paintOffset);

    TransformationMatrix* NODELETE layerTransform() const;

    virtual void updateLayerTransform();
    virtual void applyTransform(TransformationMatrix&, const Style::ComputedStyle&, const FloatRect& boundingBox, OptionSet<Style::TransformResolverOption>) const = 0;
    void applyTransform(TransformationMatrix&, const Style::ComputedStyle&, const FloatRect& boundingBox) const;

    virtual void invalidateCachedVisualOverflowRect() { }

    // LBSE: flag the transform-dependent bounding boxes (objectBoundingBox / strokeBoundingBox)
    // for lazy recomputation. Overridden by RenderSVGContainer / RenderSVGRoot, which cache them.
    // The deferred SVG transform-attribute flush uses it to dirty the container ancestor chain of
    // a renderer whose transform changed without a layout. No-op otherwise.
    virtual void invalidateCachedSVGTransformDependentBoundingBoxes() { }

    inline bool shouldUsePositionedClipping() const;

#if ASSERT_ENABLED
    bool layerAccessPreventedSlow() const;
#endif

    AffineTransform computeRendererTransform() const;

protected:
    RenderLayerModelObject(Type, Element&, Style::ComputedStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags);
    RenderLayerModelObject(Type, Document&, Style::ComputedStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags);

    void createLayer();
    void willBeDestroyed() override;

    virtual void updateFromStyle() { }

private:
    bool createLayerIfAllowed();
    void removeOnlyThisLayerWithRepaint();

    RenderSVGResourceMarker* svgMarkerResourceFromStyle(const Style::SVGMarkerResource&) const;

    UniquelyOwnedPtr<RenderLayer> m_layer;

    // Used to store state between styleWillChange and styleDidChange
    static bool s_wasFloating;
    static bool s_hadLayer;
    static bool s_wasTransformed;
    static bool s_layerWasSelfPainting;
};

// Pixel-snapping (== 'device pixel alignment') helpers.
bool NODELETE rendererNeedsPixelSnapping(const RenderLayerModelObject&);
FloatRect snapRectToDevicePixelsIfNeeded(const LayoutRect&, const RenderLayerModelObject&);
FloatRect snapRectToDevicePixelsIfNeeded(const FloatRect&, const RenderLayerModelObject&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderLayerModelObject, isRenderLayerModelObject())
