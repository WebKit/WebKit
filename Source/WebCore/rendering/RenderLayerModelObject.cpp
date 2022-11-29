/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderLayerModelObject.h"

#include "InspectorInstrumentation.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderSVGBlock.h"
#include "RenderSVGModelObject.h"
#include "RenderView.h"
#include "SVGGraphicsElement.h"
#include "Settings.h"
#include "StyleScrollSnapPoints.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLayerModelObject);

bool RenderLayerModelObject::s_wasFloating = false;
bool RenderLayerModelObject::s_hadLayer = false;
bool RenderLayerModelObject::s_hadTransform = false;
bool RenderLayerModelObject::s_layerWasSelfPainting = false;

RenderLayerModelObject::RenderLayerModelObject(Element& element, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderElement(element, WTFMove(style), baseTypeFlags | RenderLayerModelObjectFlag)
{
}

RenderLayerModelObject::RenderLayerModelObject(Document& document, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderElement(document, WTFMove(style), baseTypeFlags | RenderLayerModelObjectFlag)
{
}

RenderLayerModelObject::~RenderLayerModelObject()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderLayerModelObject::willBeDestroyed()
{
    if (isPositioned()) {
        if (style().hasViewportConstrainedPosition())
            view().frameView().removeViewportConstrainedObject(*this);
    }

    if (hasLayer()) {
        setHasLayer(false);
        destroyLayer();
    }

    RenderElement::willBeDestroyed();
}

void RenderLayerModelObject::willBeRemovedFromTree(IsInternalMove isInternalMove)
{
    if (auto* layer = this->layer(); layer && layer->needsFullRepaint() && isInternalMove == IsInternalMove::No)
        issueRepaint(std::nullopt, ClipRepaintToLayer::No, ForceRepaint::Yes);

    RenderElement::willBeRemovedFromTree(isInternalMove);
}

void RenderLayerModelObject::destroyLayer()
{
    ASSERT(!hasLayer());
    ASSERT(m_layer);
#if PLATFORM(IOS_FAMILY)
    m_layer->willBeDestroyed();
#endif
    m_layer = nullptr;
}

void RenderLayerModelObject::createLayer()
{
    ASSERT(!m_layer);
    m_layer = makeUnique<RenderLayer>(*this);
    setHasLayer(true);
    m_layer->insertOnlyThisLayer(RenderLayer::LayerChangeTiming::StyleChange);
}

bool RenderLayerModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

void RenderLayerModelObject::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    s_wasFloating = isFloating();
    s_hadLayer = hasLayer();
    s_hadTransform = hasTransform();
    if (s_hadLayer)
        s_layerWasSelfPainting = layer()->isSelfPaintingLayer();

    auto* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (diff == StyleDifference::RepaintLayer && parent() && oldStyle && oldStyle->clip() != newStyle.clip())
        layer()->clearClipRectsIncludingDescendants();
    RenderElement::styleWillChange(diff, newStyle);
}

void RenderLayerModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderElement::styleDidChange(diff, oldStyle);
    updateFromStyle();

    bool gainedOrLostLayer = false;
    if (requiresLayer()) {
        if (!layer() && layerCreationAllowedForSubtree()) {
            gainedOrLostLayer = true;
            if (s_wasFloating && isFloating())
                setChildNeedsLayout();
            createLayer();
            if (parent() && !needsLayout() && containingBlock())
                layer()->setRepaintStatus(NeedsFullRepaint);
        }
    } else if (layer() && layer()->parent()) {
        gainedOrLostLayer = true;
#if ENABLE(CSS_COMPOSITING)
        if (oldStyle && oldStyle->hasBlendMode())
            layer()->willRemoveChildWithBlendMode();
#endif
        setHasTransformRelatedProperty(false); // All transform-related properties force layers, so we know we don't have one or the object doesn't support them.
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        setHasSVGTransform(false); // Same reason as for setHasTransformRelatedProperty().
#endif
        setHasReflection(false);

        // Repaint the about to be destroyed self-painting layer when style change also triggers repaint.
        if (layer()->isSelfPaintingLayer() && layer()->repaintStatus() == NeedsFullRepaint && layer()->repaintRects())
            repaintUsingContainer(containerForRepaint().renderer, layer()->repaintRects()->clippedOverflowRect);

        layer()->removeOnlyThisLayer(RenderLayer::LayerChangeTiming::StyleChange); // calls destroyLayer() which clears m_layer
        if (s_wasFloating && isFloating())
            setChildNeedsLayout();
        if (s_hadTransform)
            setNeedsLayoutAndPrefWidthsRecalc();
    }

    if (gainedOrLostLayer)
        InspectorInstrumentation::didAddOrRemoveScrollbars(*this);

    if (layer()) {
        layer()->styleChanged(diff, oldStyle);
        if (s_hadLayer && layer()->isSelfPaintingLayer() != s_layerWasSelfPainting)
            setChildNeedsLayout();
    }

    bool newStyleIsViewportConstrained = style().hasViewportConstrainedPosition();
    bool oldStyleIsViewportConstrained = oldStyle && oldStyle->hasViewportConstrainedPosition();
    if (newStyleIsViewportConstrained != oldStyleIsViewportConstrained) {
        if (newStyleIsViewportConstrained && layer())
            view().frameView().addViewportConstrainedObject(*this);
        else
            view().frameView().removeViewportConstrainedObject(*this);
    }

    const RenderStyle& newStyle = style();
    if (oldStyle && oldStyle->scrollPadding() != newStyle.scrollPadding()) {
        if (isDocumentElementRenderer()) {
            FrameView& frameView = view().frameView();
            frameView.updateScrollbarSteps();
        } else if (RenderLayer* renderLayer = layer())
            renderLayer->updateScrollbarSteps();
    }

    bool scrollMarginChanged = oldStyle && oldStyle->scrollMargin() != newStyle.scrollMargin();
    bool scrollAlignChanged = oldStyle && oldStyle->scrollSnapAlign() != newStyle.scrollSnapAlign();
    bool scrollSnapStopChanged = oldStyle && oldStyle->scrollSnapStop() != newStyle.scrollSnapStop();
    if (scrollMarginChanged || scrollAlignChanged || scrollSnapStopChanged) {
        if (auto* scrollSnapBox = enclosingScrollableContainerForSnapping())
            scrollSnapBox->setNeedsLayout();
    }
}

bool RenderLayerModelObject::shouldPlaceVerticalScrollbarOnLeft() const
{
// RTL Scrollbars require some system support, and this system support does not exist on certain versions of OS X. iOS uses a separate mechanism.
#if PLATFORM(IOS_FAMILY)
    return false;
#else
    switch (settings().userInterfaceDirectionPolicy()) {
    case UserInterfaceDirectionPolicy::Content:
        return style().shouldPlaceVerticalScrollbarOnLeft();
    case UserInterfaceDirectionPolicy::System:
        return settings().systemLayoutDirection() == TextDirection::RTL;
    }
    ASSERT_NOT_REACHED();
    return style().shouldPlaceVerticalScrollbarOnLeft();
#endif
}

std::optional<LayerRepaintRects> RenderLayerModelObject::layerRepaintRects() const
{
    return hasLayer() ? layer()->repaintRects() : std::nullopt;
}

bool RenderLayerModelObject::startAnimation(double timeOffset, const Animation& animation, const KeyframeList& keyframes)
{
    if (!layer() || !layer()->backing())
        return false;
    return layer()->backing()->startAnimation(timeOffset, animation, keyframes);
}

void RenderLayerModelObject::animationPaused(double timeOffset, const String& name)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->animationPaused(timeOffset, name);
}

void RenderLayerModelObject::animationFinished(const String& name)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->animationFinished(name);
}

void RenderLayerModelObject::transformRelatedPropertyDidChange()
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->transformRelatedPropertyDidChange();
}

void RenderLayerModelObject::suspendAnimations(MonotonicTime time)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->suspendAnimations(time);
}

TransformationMatrix* RenderLayerModelObject::layerTransform() const
{
    if (hasLayer())
        return layer()->transform();
    return nullptr;
}

void RenderLayerModelObject::updateLayerTransform()
{
    // Transform-origin depends on box size, so we need to update the layer transform after layout.
    if (hasLayer())
        layer()->updateTransform();
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
bool RenderLayerModelObject::shouldPaintSVGRenderer(const PaintInfo& paintInfo, const OptionSet<PaintPhase> relevantPaintPhases) const
{
    if (paintInfo.context().paintingDisabled())
        return false;

    if (!relevantPaintPhases.isEmpty() && !relevantPaintPhases.contains(paintInfo.phase))
        return false;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return false;

    if (style().visibility() == Visibility::Hidden || style().display() == DisplayType::None)
        return false;

    return true;
}

std::optional<LayoutRect> RenderLayerModelObject::computeVisibleRectInSVGContainer(const LayoutRect& rect, const RenderLayerModelObject* container, RenderObject::VisibleRectContext context) const
{
    ASSERT(is<RenderSVGModelObject>(this) || is<RenderSVGBlock>(this));
    ASSERT(!style().hasInFlowPosition());
    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());

    if (container == this)
        return rect;

    bool containerIsSkipped;
    auto* localContainer = this->container(container, containerIsSkipped);
    if (!localContainer)
        return rect;

    ASSERT_UNUSED(containerIsSkipped, !containerIsSkipped);

    LayoutRect adjustedRect = rect;

    LayoutSize locationOffset;
    if (is<RenderSVGModelObject>(this))
        locationOffset = downcast<RenderSVGModelObject>(*this).locationOffsetEquivalent();
    else if (is<RenderSVGBlock>(this))
        locationOffset = downcast<RenderSVGBlock>(*this).locationOffset();

    LayoutPoint topLeft = adjustedRect.location();
    topLeft.move(locationOffset);

    // We are now in our parent container's coordinate space. Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    if (hasLayer() && layer()->transform()) {
        adjustedRect = layer()->transform()->mapRect(adjustedRect);
        topLeft = adjustedRect.location();
        topLeft.move(locationOffset);
    }

    // FIXME: We ignore the lightweight clipping rect that controls use, since if |o| is in mid-layout,
    // its controlClipRect will be wrong. For overflow clip we use the values cached by the layer.
    adjustedRect.setLocation(topLeft);
    if (localContainer->hasNonVisibleOverflow()) {
        bool isEmpty = !downcast<RenderLayerModelObject>(*localContainer).applyCachedClipAndScrollPosition(adjustedRect, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRect;
        }
    }

    return localContainer->computeVisibleRectInContainer(adjustedRect, container, context);
}

void RenderLayerModelObject::mapLocalToSVGContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    ASSERT(is<RenderSVGModelObject>(this) || is<RenderSVGBlock>(this));
    ASSERT(style().position() == PositionType::Static);

    if (ancestorContainer == this)
        return;

    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());

    bool ancestorSkipped;
    auto* container = this->container(ancestorContainer, ancestorSkipped);
    if (!container)
        return;

    ASSERT_UNUSED(ancestorSkipped, !ancestorSkipped);

    // If this box has a transform, it acts as a fixed position container for fixed descendants,
    // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
    if (hasTransform())
        mode.remove(IsFixed);

    if (wasFixed)
        *wasFixed = mode.contains(IsFixed);

    auto containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    pushOntoTransformState(transformState, mode, nullptr, container, containerOffset, false);

    mode.remove(ApplyContainerFlip);

    container->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

void RenderLayerModelObject::applySVGTransform(TransformationMatrix& transform, const SVGGraphicsElement& graphicsElement, const RenderStyle& style, const FloatRect& boundingBox, const std::optional<AffineTransform>& preApplySVGTransformMatrix, const std::optional<AffineTransform>& postApplySVGTransformMatrix, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    auto svgTransform = graphicsElement.transform().concatenate();
    auto* supplementalTransform = graphicsElement.supplementalTransform(); // SMIL <animateMotion>

    // This check does not use style.hasTransformRelatedProperty() on purpose -- we only want to know if either the 'transform' property, an
    // offset path, or the individual transform operations are set (perspective / transform-style: preserve-3d are not relevant here).
    bool hasCSSTransform = style.hasTransform() || style.rotate() || style.translate() || style.scale();
    bool hasSVGTransform = !svgTransform.isIdentity() || preApplySVGTransformMatrix || postApplySVGTransformMatrix || supplementalTransform;

    // Common case: 'viewBox' set on outermost <svg> element -> 'preApplySVGTransformMatrix'
    // passed by RenderSVGViewportContainer::applyTransform(), the anonymous single child
    // of RenderSVGRoot, that wraps all direct children from <svg> as present in DOM. All
    // other transformations are unset (no need to compute transform-origin, etc. in that case).
    if (!hasCSSTransform && !hasSVGTransform)
        return;

    auto affectedByTransformOrigin = [&]() {
        if (preApplySVGTransformMatrix && !preApplySVGTransformMatrix->isIdentityOrTranslation())
            return true;
        if (postApplySVGTransformMatrix && !postApplySVGTransformMatrix->isIdentityOrTranslation())
            return true;
        if (supplementalTransform && !supplementalTransform->isIdentityOrTranslation())
            return true;
        if (hasCSSTransform)
            return style.affectedByTransformOrigin();
        return !svgTransform.isIdentityOrTranslation();
    };

    FloatPoint3D originTranslate;
    if (options.contains(RenderStyle::TransformOperationOption::TransformOrigin) && affectedByTransformOrigin())
        originTranslate = style.computeTransformOrigin(boundingBox);

    style.applyTransformOrigin(transform, originTranslate);

    if (supplementalTransform)
        transform.multiplyAffineTransform(*supplementalTransform);

    if (preApplySVGTransformMatrix)
        transform.multiplyAffineTransform(preApplySVGTransformMatrix.value());

    // CSS transforms take precedence over SVG transforms.
    if (hasCSSTransform)
        style.applyCSSTransform(transform, boundingBox, options);
    else if (!svgTransform.isIdentity())
        transform.multiplyAffineTransform(svgTransform);

    if (postApplySVGTransformMatrix)
        transform.multiplyAffineTransform(postApplySVGTransformMatrix.value());

    style.unapplyTransformOrigin(transform, originTranslate);
}

void RenderLayerModelObject::updateHasSVGTransformFlags()
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());

    bool hasSVGTransform = needsHasSVGTransformFlags();
    setHasTransformRelatedProperty(hasSVGTransform || style().hasTransformRelatedProperty());
    setHasSVGTransform(hasSVGTransform);
}
#endif

bool rendererNeedsPixelSnapping(const RenderLayerModelObject& renderer)
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (renderer.document().settings().layerBasedSVGEngineEnabled() && renderer.isSVGLayerAwareRenderer() && !renderer.isSVGRoot())
        return false;
#else
    UNUSED_PARAM(renderer);
#endif

    return true;
}

FloatRect snapRectToDevicePixelsIfNeeded(const LayoutRect& rect, const RenderLayerModelObject& renderer)
{
    if (!rendererNeedsPixelSnapping(renderer))
        return rect;
    return snapRectToDevicePixels(rect, renderer.document().deviceScaleFactor());
}

FloatRect snapRectToDevicePixelsIfNeeded(const FloatRect& rect, const RenderLayerModelObject& renderer)
{
    if (!rendererNeedsPixelSnapping(renderer))
        return rect;
    return snapRectToDevicePixels(LayoutRect { rect }, renderer.document().deviceScaleFactor());
}

} // namespace WebCore

