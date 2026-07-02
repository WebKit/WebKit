/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2015 Google Inc. All rights reserved.
 * Copyright (C) 2023, 2024 Igalia S.L.
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

#include "BlendingKeyframes.h"
#include "ContainerNodeInlines.h"
#include "InspectorInstrumentation.h"
#include "LocalFrameView.h"
#include "MotionPath.h"
#include "ReferencedSVGResources.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderMultiColumnSet.h"
#include "RenderObjectInlines.h"
#include "RenderSVGBlock.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceLinearGradient.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGResourceRadialGradient.h"
#include "RenderSVGText.h"
#include "RenderView.h"
#include "SVGClipPathElement.h"
#include "SVGFilterElement.h"
#include "SVGGraphicsElement.h"
#include "SVGMarkerElement.h"
#include "SVGMaskElement.h"
#include "SVGTextElement.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleTransformResolver.h"
#include "TransformOperationData.h"
#include "TransformState.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderLayerModelObject);

bool RenderLayerModelObject::s_wasFloating = false;
bool RenderLayerModelObject::s_hadLayer = false;
bool RenderLayerModelObject::s_wasTransformed = false;
bool RenderLayerModelObject::s_layerWasSelfPainting = false;

RenderLayerModelObject::RenderLayerModelObject(Type type, Element& element, Style::ComputedStyle&& style, OptionSet<TypeFlag> baseTypeFlags, TypeSpecificFlags typeSpecificFlags)
    : RenderElement(type, element, WTF::move(style), baseTypeFlags | TypeFlag::IsLayerModelObject, typeSpecificFlags)
{
    ASSERT(isRenderLayerModelObject());
}

RenderLayerModelObject::RenderLayerModelObject(Type type, Document& document, Style::ComputedStyle&& style, OptionSet<TypeFlag> baseTypeFlags, TypeSpecificFlags typeSpecificFlags)
    : RenderElement(type, document, WTF::move(style), baseTypeFlags | TypeFlag::IsLayerModelObject, typeSpecificFlags)
{
    ASSERT(isRenderLayerModelObject());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderLayerModelObject::~RenderLayerModelObject() = default;

void RenderLayerModelObject::willBeDestroyed()
{
    if (isPositioned()) {
        if (style().hasViewportConstrainedPosition())
            view().frameView().removeViewportConstrainedObject(*this);
    }

    destroyLayer();

    RenderElement::willBeDestroyed();
}

void RenderLayerModelObject::destroyLayer()
{
    setHasLayer(false);
    m_layer = nullptr;
}

void RenderLayerModelObject::createLayer()
{
    ASSERT(!m_layer);
    m_layer = RenderLayer::create(*this);
    setHasLayer(true);
    setMayHaveLayerInSubtreeIncludingAncestors();
    m_layer->insertOnlyThisLayer();
}

bool RenderLayerModelObject::createLayerIfAllowed()
{
    if (m_layer || !layerCreationAllowedForSubtree())
        return false;
    createLayer();
    if (parent() && !needsLayout())
        m_layer->setRepaintStatus(RepaintStatus::NeedsFullRepaint);
    return true;
}

void RenderLayerModelObject::removeOnlyThisLayerWithRepaint()
{
    ASSERT(m_layer && m_layer->parent());
    // Repaint the about-to-be-destroyed self-painting layer when a repaint is pending.
    if (m_layer->isSelfPaintingLayer() && m_layer->repaintStatus() == RepaintStatus::NeedsFullRepaint && m_layer->cachedClippedOverflowRect())
        repaintUsingContainer(containerForRepaint().renderer.get(), *m_layer->cachedClippedOverflowRect());
    m_layer->removeOnlyThisLayer();
}

bool RenderLayerModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

bool RenderLayerModelObject::requiresLayerForSVGIntrinsicReasons() const
{
    // Plain 2D transforms need no layer, paintRendererByApplyingTransformForSVG() handles them.
    // 3D transforms require compositing, hence a layer, as do grouping effects, z-index, etc.
    return createsGroup()
        || style().transform().has3DOperation()
        || style().translate().is3DOperation()
        || style().scale().is3DOperation()
        || style().rotate().is3DOperation()
        || style().transformStyle3D() == TransformStyle3D::Preserve3D
        || !style().perspective().isNone()
        || hasHiddenBackface()
        || hasReflection()
        || !style().specifiedZIndex().isAuto()
        || style().isolation() != Isolation::Auto;
}

void RenderLayerModelObject::styleWillChange(Style::Difference diff, const Style::ComputedStyle& newStyle)
{
    s_wasFloating = isFloating();
    s_hadLayer = hasLayer();
    s_wasTransformed = isTransformed();
    if (s_hadLayer)
        s_layerWasSelfPainting = layer()->isSelfPaintingLayer();

    auto* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (diff == Style::DifferenceResult::RepaintLayer && parent() && oldStyle && oldStyle->clip() != newStyle.clip())
        layer()->clearClipRectsIncludingDescendants();
    RenderElement::styleWillChange(diff, newStyle);
}

void RenderLayerModelObject::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    updateFromStyle();
    RenderElement::styleDidChange(diff, oldStyle);

    // When an out-of-flow-positioned element changes its display between block and inline-block,
    // then an incremental layout on the element's containing block lays out the element through
    // LayoutPositionedObjects, which skips laying out the element's parent.
    // The element's parent needs to relayout so that it calls
    // RenderBlockFlow::setStaticInlinePositionForChild with the out-of-flow-positioned child, so
    // that when it's laid out, its RenderBox::computeOutOfFlowPositionedLogicalWidth/Height takes into
    // account its new inline/block position rather than its old block/inline position.
    // Position changes and other types of display changes are handled elsewhere.
    if ((oldStyle && isOutOfFlowPositioned() && parent() && (parent() != containingBlock()))
        && (style().position() == oldStyle->position())
        && (style().originalDisplay().isInlineType() != oldStyle->originalDisplay().isInlineType())
        && (style().originalDisplay().isBlockType() || style().originalDisplay().isInlineType())
        && (oldStyle->originalDisplay().isBlockType() || oldStyle->originalDisplay().isInlineType()))
            parent()->setChildNeedsLayout();

    bool gainedOrLostLayer = false;
    if (requiresLayer()) {
        if (createLayerIfAllowed()) {
            gainedOrLostLayer = true;
            if (s_wasFloating && isFloating())
                setChildNeedsLayout();
        }
    } else if (layer() && layer()->parent()) {
        gainedOrLostLayer = true;
        if (oldStyle && oldStyle->blendMode() != BlendMode::Normal)
            layer()->willRemoveChildWithBlendMode();
        // For CSS renderers every transform-related property forces a layer, so reaching the
        // layer-removal branch means there is no transform and these flags can be cleared. Under
        // LBSE a plain 2D SVG transform needs no layer, so an SVG renderer can lose its layer while
        // still being transformed. updateFromStyle() above already set the correct flags for it, so
        // leave them untouched here.
        if (!isSVGLayerAwareRenderer()) {
            setHasTransformRelatedProperty(false);
            setHasSVGTransform(false);
        }
        setHasReflection(false);

        removeOnlyThisLayerWithRepaint();
        if (s_wasFloating && isFloating())
            setChildNeedsLayout();
        if (s_wasTransformed)
            setNeedsLayoutAndInvalidateContentLogicalWidths();
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

    if (oldStyle && !oldStyle->scrollPaddingEqual(style())) {
        if (isDocumentElementRenderer()) {
            CheckedRef frameView = view().frameView();
            frameView->updateScrollbarSteps();
        } else if (CheckedPtr renderLayer = layer())
            renderLayer->updateScrollbarSteps();
    }

    if (oldStyle && !oldStyle->scrollSnapDataEquivalent(style())) {
        if (auto* scrollSnapBox = enclosingScrollableContainer())
            scrollSnapBox->setNeedsLayout();
    }
}

bool RenderLayerModelObject::applyCachedClipAndScrollPosition(RepaintRects&, const RenderLayerModelObject*, VisibleRectContext) const
{
    return false;
}

bool RenderLayerModelObject::shouldPlaceVerticalScrollbarOnLeft() const
{
// RTL Scrollbars require some system support, and this system support does not exist on certain versions of macOS. iOS uses a separate mechanism.
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

std::optional<LayoutRect> RenderLayerModelObject::cachedLayerClippedOverflowRect() const
{
    return hasLayer() ? layer()->cachedClippedOverflowRect() : std::nullopt;
}

bool RenderLayerModelObject::startAnimation(double timeOffset, const GraphicsLayerAnimation& animation, const BlendingKeyframes& keyframes)
{
    if (!layer() || !layer()->backing())
        return false;
    return layer()->backing()->startAnimation(timeOffset, animation, keyframes);
}

void RenderLayerModelObject::animationPaused(double timeOffset, const BlendingKeyframes& keyframes)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->animationPaused(timeOffset, keyframes.acceleratedAnimationName());
}

void RenderLayerModelObject::animationFinished(const BlendingKeyframes& keyframes)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->animationFinished(keyframes.acceleratedAnimationName());
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
    if (auto* box = dynamicDowncast<RenderBox>(this); box && MotionPath::needsUpdateAfterContainingBlockLayout(style().offsetPath())) {
        if (auto* containingBlock = this->containingBlock()) {
            view().frameView().layoutContext().setBoxNeedsTransformUpdateAfterContainerLayout(*box, *containingBlock);
            return;
        }
    }
    // Transform-origin depends on box size, so we need to update the layer transform after layout.
    if (hasLayer())
        layer()->updateTransform();
}

bool RenderLayerModelObject::shouldPaintSVGRenderer(const PaintInfo& paintInfo, const OptionSet<PaintPhase> relevantPaintPhases) const
{
    if (paintInfo.context().paintingDisabled())
        return false;

    if (!relevantPaintPhases.isEmpty() && !relevantPaintPhases.contains(paintInfo.phase))
        return false;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return false;

    if (style().usedVisibility() == Visibility::Hidden || style().display() == Style::DisplayType::None)
        return false;

    return true;
}

auto RenderLayerModelObject::computeVisibleRectsInSVGContainer(const RepaintRects& rects, const RenderLayerModelObject* container, VisibleRectContext context) const -> std::optional<RepaintRects>
{
    ASSERT(is<RenderSVGModelObject>(this) || is<RenderSVGBlock>(this));
    ASSERT(!style().hasInFlowPosition());
    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());

    if (container == this)
        return rects;

    bool containerIsSkipped;
    auto* localContainer = this->container(container, containerIsSkipped);
    if (!localContainer)
        return rects;

    ASSERT_UNUSED(containerIsSkipped, !containerIsSkipped);

    auto adjustedRects = rects;

    LayoutSize locationOffset;
    if (CheckedPtr modelObject = dynamicDowncast<RenderSVGModelObject>(this))
        locationOffset = modelObject->locationOffsetEquivalent();
    else if (auto* svgBlock = dynamicDowncast<RenderSVGBlock>(this))
        locationOffset = svgBlock->locationOffset();

    // We are now in our parent container's coordinate space. Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    if (!hasLayer()) {
        // Non-layered SVG elements: apply the SVG transform first, then locationOffset.
        // The localTransform() includes the transform-origin effect (which accounts for
        // the element's position in the reference box), matching the layer path order
        // (transform, then offset). Applying offset first would double-count the position.
        //
        // Don't use isTransformed() here -- the flags may already reflect the NEW transform
        // (e.g., identity after clearing the transform attribute), while localTransform()
        // still holds the OLD value needed for correct old-position repaint.
        if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(this)) {
            auto svgTransform = svgModel->localTransform();
            if (!svgTransform.isIdentity())
                adjustedRects.transform(TransformationMatrix(svgTransform));
        }
    } else if (layer()->transform())
        adjustedRects.transform(*layer()->transform());

    adjustedRects.move(locationOffset);

    if (localContainer->hasNonVisibleOverflow()) {
        bool isEmpty = !downcast<RenderLayerModelObject>(*localContainer).applyCachedClipAndScrollPosition(adjustedRects, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRects;
        }
    }

    return localContainer->computeVisibleRectsInContainer(adjustedRects, container, context);
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
    if (isTransformed())
        mode.remove(MapCoordinatesMode::IsFixed);

    if (wasFixed)
        *wasFixed = mode.contains(MapCoordinatesMode::IsFixed);

    auto containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    pushOntoTransformState(transformState, mode, nullptr, container, containerOffset, false);

    mode.remove(MapCoordinatesMode::ApplyContainerFlip);

    container->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

void RenderLayerModelObject::applySVGTransform(TransformationMatrix& transform, const SVGGraphicsElement& graphicsElement, const Style::ComputedStyle& style, const FloatRect& boundingBox, const std::optional<AffineTransform>& preApplySVGTransformMatrix, const std::optional<AffineTransform>& postApplySVGTransformMatrix, OptionSet<Style::TransformResolverOption> options) const
{
    auto svgTransform = graphicsElement.transform().concatenate().value_or(identity);
    auto* supplementalTransform = graphicsElement.supplementalTransform(); // SMIL <animateMotion>

    // This check does not use style.hasTransformRelatedProperty() on purpose -- we only want to know if either the 'transform' property, an
    // offset path, or the individual transform operations are set (perspective / transform-style: preserve-3d are not relevant here).
    bool hasCSSTransform = !style.transform().isNone()
        || !style.offsetPath().isNone()
        || !style.rotate().isNone()
        || !style.translate().isNone()
        || !style.scale().isNone();
    bool hasSVGTransform = !svgTransform.isIdentity() || preApplySVGTransformMatrix || postApplySVGTransformMatrix || supplementalTransform;

    // Common case: 'viewBox' set on outermost <svg> element -> 'preApplySVGTransformMatrix'
    // passed by RenderSVGViewportContainer::applyTransform(), the anonymous single child
    // of RenderSVGRoot, that wraps all direct children from <svg> as present in DOM. All
    // other transformations are unset (no need to compute transform-origin, etc. in that case).
    if (!hasCSSTransform && !hasSVGTransform)
        return;

    Style::TransformResolver transformResolver { transform, style };

    auto affectedByTransformOrigin = [&]() {
        if (preApplySVGTransformMatrix && !preApplySVGTransformMatrix->isIdentityOrTranslation())
            return true;
        if (postApplySVGTransformMatrix && !postApplySVGTransformMatrix->isIdentityOrTranslation())
            return true;
        if (supplementalTransform && !supplementalTransform->isIdentityOrTranslation())
            return true;
        if (hasCSSTransform)
            return transformResolver.affectedByTransformOrigin();
        return !svgTransform.isIdentityOrTranslation();
    };

    FloatPoint3D originTranslate;
    if (options.contains(Style::TransformResolverOption::TransformOrigin) && affectedByTransformOrigin())
        originTranslate = transformResolver.computeTransformOrigin(boundingBox);

    transformResolver.applyTransformOrigin(originTranslate);

    if (supplementalTransform)
        transform.multiplyAffineTransform(*supplementalTransform);

    if (preApplySVGTransformMatrix)
        transform.multiplyAffineTransform(preApplySVGTransformMatrix.value());

    // CSS transforms take precedence over SVG transforms.
    if (hasCSSTransform)
        transformResolver.applyCSSTransform(TransformOperationData(boundingBox, this), options);
    else if (!svgTransform.isIdentity())
        transform.multiplyAffineTransform(svgTransform);

    if (postApplySVGTransformMatrix)
        transform.multiplyAffineTransform(postApplySVGTransformMatrix.value());

    transformResolver.unapplyTransformOrigin(originTranslate);
}

void RenderLayerModelObject::updateHasSVGTransformFlags()
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());

    bool wasTransformed = isTransformed();
    bool hasSVGTransform = needsHasSVGTransformFlags();
    setHasTransformRelatedProperty(hasSVGTransform || style().hasTransformRelatedProperty());
    setHasSVGTransform(hasSVGTransform);

    // When the isTransformed() state changes, the enclosing layer's SVG children order cache
    // must be rebuilt. A transformed non-layer renderer is collected as a single atomic entry
    // without recursing into its subtree, so any layered descendants are not registered in
    // the DOM-order list. A stale classification either drops those descendants (when toggled
    // to transformed) or leaves them un-wrapped by the new transform (when toggled away).
    if (isTransformed() != wasTransformed) {
        // dirtyChildrenInDOMOrderForSVG() asserts m_svgData, so only an SVG layer may receive it. A
        // non-SVG enclosing layer (reachable for an SVG renderer reparented under non-SVG content) has
        // no DOM-order cache to rebuild.
        if (CheckedPtr layer = enclosingLayer(); layer && layer->isSVGLayer())
            layer->dirtyChildrenInDOMOrderForSVG();
    }
}

RenderSVGResourceClipper* RenderLayerModelObject::svgClipperResourceFromStyle() const
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return nullptr;

    return WTF::switchOn(style().clipPath(),
        [&](const Style::ReferencePath& clipPath) -> RenderSVGResourceClipper* {
            if (RefPtr referencedClipPathElement = ReferencedSVGResources::referencedClipPathElement(treeScopeForSVGReferences(), clipPath)) {
                if (auto* referencedClipperRenderer = dynamicDowncast<RenderSVGResourceClipper>(referencedClipPathElement->renderer()))
                    return referencedClipperRenderer;
            }

            if (RefPtr svgElement = dynamicDowncast<SVGElement>(this->element()))
                document().addPendingSVGResource(clipPath.fragment(), *svgElement);

            return nullptr;

        },
        [&](const auto&) -> RenderSVGResourceClipper* {
            return nullptr;
        }
    );
}

RenderSVGResourceFilter* RenderLayerModelObject::svgFilterResourceFromStyle() const
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return nullptr;

    if (style().filter().size() != 1)
        return nullptr;

    return WTF::switchOn(style().filter().first(),
        [&](const Style::FilterReference& filterReference) -> RenderSVGResourceFilter* {
            if (RefPtr referencedFilterElement = ReferencedSVGResources::referencedFilterElement(treeScopeForSVGReferences(), filterReference)) {
                if (auto* referencedFilterRenderer = dynamicDowncast<RenderSVGResourceFilter>(referencedFilterElement->renderer()))
                    return referencedFilterRenderer;
            }

            if (RefPtr svgElement = dynamicDowncast<SVGElement>(this->element()))
                document().addPendingSVGResource(filterReference.cachedFragment, *svgElement);

            return nullptr;
        },
        []<CSSValueID C, typename T>(const FunctionNotation<C, T>&) -> RenderSVGResourceFilter* {
            return nullptr;
        }
    );
}

RenderSVGResourceMasker* RenderLayerModelObject::svgMaskerResourceFromStyle() const
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return nullptr;

    RefPtr maskImage = style().maskLayers().usedFirst().image().tryStyleImage();
    if (!maskImage)
        return nullptr;

    auto resourceID = SVGURIReference::fragmentIdentifierFromIRIString(maskImage->url(), protect(document()));

    if (RefPtr referencedMaskElement = ReferencedSVGResources::referencedMaskElement(treeScopeForSVGReferences(), *maskImage)) {
        if (auto* referencedMaskerRenderer = dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer()))
            return referencedMaskerRenderer;
    }

    if (RefPtr element = this->element())
        document().addPendingSVGResource(resourceID, downcast<SVGElement>(*element));

    return nullptr;
}

RenderSVGResourceMarker* RenderLayerModelObject::svgMarkerStartResourceFromStyle() const
{
    return svgMarkerResourceFromStyle(style().markerStart());
}

RenderSVGResourceMarker* RenderLayerModelObject::svgMarkerMidResourceFromStyle() const
{
    return svgMarkerResourceFromStyle(style().markerMid());
}

RenderSVGResourceMarker* RenderLayerModelObject::svgMarkerEndResourceFromStyle() const
{
    return svgMarkerResourceFromStyle(style().markerEnd());
}

RenderSVGResourceMarker* RenderLayerModelObject::svgMarkerResourceFromStyle(const Style::SVGMarkerResource& markerResource) const
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return nullptr;

    auto markerResourceURL = markerResource.tryURL();
    if (!markerResourceURL)
        return nullptr;

    if (RefPtr referencedMarkerElement = ReferencedSVGResources::referencedMarkerElement(treeScopeForSVGReferences(), *markerResourceURL)) {
        if (auto* referencedMarkerRenderer = dynamicDowncast<RenderSVGResourceMarker>(referencedMarkerElement->renderer()))
            return referencedMarkerRenderer;
    }

    if (RefPtr element = dynamicDowncast<SVGElement>(this->element())) {
        auto resourceID = SVGURIReference::fragmentIdentifierFromIRIString(markerResourceURL->resolved.string(), document());
        document().addPendingSVGResource(resourceID, *element);
    }

    return nullptr;
}

RenderSVGResourcePaintServer* RenderLayerModelObject::svgFillPaintServerResourceFromStyle(const Style::ComputedStyle& style) const
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return nullptr;

    auto fillURL = style.fill().tryAnyURL();
    if (!fillURL)
        return nullptr;

    if (RefPtr referencedElement = ReferencedSVGResources::referencedPaintServerElement(treeScopeForSVGReferences(), *fillURL)) {
        if (auto* referencedPaintServerRenderer = dynamicDowncast<RenderSVGResourcePaintServer>(referencedElement->renderer()))
            return referencedPaintServerRenderer;
    }

    if (RefPtr element = this->element())
        document().addPendingSVGResource(AtomString(fillURL->resolved.string()), downcast<SVGElement>(*element));

    return nullptr;
}

RenderSVGResourcePaintServer* RenderLayerModelObject::svgStrokePaintServerResourceFromStyle(const Style::ComputedStyle& style) const
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return nullptr;

    auto strokeURL = style.stroke().tryAnyURL();
    if (!strokeURL)
        return nullptr;

    if (RefPtr referencedElement = ReferencedSVGResources::referencedPaintServerElement(treeScopeForSVGReferences(), *strokeURL)) {
        if (auto* referencedPaintServerRenderer = dynamicDowncast<RenderSVGResourcePaintServer>(referencedElement->renderer()))
            return referencedPaintServerRenderer;
    }

    if (RefPtr element = this->element())
        document().addPendingSVGResource(AtomString(strokeURL->resolved.string()), downcast<SVGElement>(*element));

    return nullptr;
}

LegacyRenderSVGResourceClipper* RenderLayerModelObject::legacySVGClipperResourceFromStyle() const
{
    return WTF::switchOn(style().clipPath(),
        [&](const Style::ReferencePath& clipPath) -> LegacyRenderSVGResourceClipper* {
            return ReferencedSVGResources::referencedClipperRenderer(treeScopeForSVGReferences(), clipPath);
        },
        [&](const auto&) -> LegacyRenderSVGResourceClipper* {
            return nullptr;
        }
    );
}

bool RenderLayerModelObject::pointInSVGClippingArea(const FloatPoint& point) const
{
    auto clipPathReferenceBox = [&](CSSBoxType boxType) -> FloatRect {
        FloatRect referenceBox;
        switch (boxType) {
        case CSSBoxType::BorderBox:
        case CSSBoxType::MarginBox:
        case CSSBoxType::StrokeBox:
        case CSSBoxType::BoxMissing:
            // FIXME: strokeBoundingBox() takes dasharray into account but shouldn't.
            referenceBox = strokeBoundingBox();
            break;
        case CSSBoxType::ViewBox:
            if (element()) {
                // FIXME: [LBSE] This should not need to use SVGLengthContext, RenderSVGRoot holds that information.
                auto viewportSize = SVGLengthContext(downcast<SVGElement>(element())).viewportSize();
                if (viewportSize)
                    referenceBox.setSize(*viewportSize);
                break;
            }
            [[fallthrough]];
        case CSSBoxType::ContentBox:
        case CSSBoxType::FillBox:
        case CSSBoxType::PaddingBox:
            referenceBox = objectBoundingBox();
            break;
        }
        return referenceBox;
    };

    return WTF::switchOn(style().clipPath(),
        [&](const Style::BasicShapePath& clipPath) {
            auto referenceBox = clipPathReferenceBox(clipPath.referenceBox());
            if (!referenceBox.contains(point))
                return false;
            return Style::path(clipPath.shape(), referenceBox, style().usedZoomForLength()).contains(point, Style::windRule(clipPath.shape()));
        },
        [&](const Style::BoxPath& clipPath) {
            auto referenceBox = clipPathReferenceBox(clipPath.referenceBox());
            if (!referenceBox.contains(point))
                return false;
            return FloatRoundedRect { referenceBox }.path().contains(point);
        },
        [&](const auto&) {
            if (auto* referencedClipperRenderer = svgClipperResourceFromStyle())
                return referencedClipperRenderer->hitTestClipContent(objectBoundingBox(), LayoutPoint(point));
            return true;
        }
    );
}

bool RenderLayerModelObject::svgTransformAttributeChangeInducesLayerComposition()
{
    // True when a just-parsed SVG transform attribute flips whether this container needs a layer,
    // so the change must create or destroy one. A 2D transform gates layer creation for any SVG
    // container (requiresLayer() returns true for isTransformed() && isRenderSVGContainer()), which
    // covers both <g> (RenderSVGTransformableContainer) and nested <svg> (RenderSVGViewportContainer).
    if (!isRenderSVGContainer())
        return false;
    // requiresLayer() reads the cached hasSVGTransform() flag - so make sure it's not stale.
    updateHasSVGTransformFlags();
    return requiresLayer() != hasLayer();
}

void RenderLayerModelObject::updateTransformAndRepaintForSVGAfterAttributeChange(SVGAttributeChangeRepaintMode repaintMode)
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());

    updateHasSVGTransformFlags();

    // A layer is neither created nor destroyed below, so probe it once up front.
    const bool isLayered = hasLayer();
    ASSERT(!isRenderSVGContainer() || requiresLayer() == isLayered);

    // Refresh the transform, returning the (previous, current) pair. For layered renderers this
    // forces a stacking context on an identity-to-non-identity transition (the batched flush skips
    // RenderLayer::styleChanged) and marks the layer subtree for a position update. A non-layered
    // RenderSVGModelObject updates its cached m_localTransform in place, while a non-layered
    // RenderSVGText is only probed (see the branch below).
    auto refreshTransform = [&]() -> std::pair<AffineTransform, AffineTransform> {
        if (isLayered) {
            auto previousTransform = layerTransform() ? layerTransform()->toAffineTransform() : identity;
            updateLayerTransform();
            auto currentTransform = layerTransform() ? layerTransform()->toAffineTransform() : identity;
            if (previousTransform.isIdentity() && !currentTransform.isIdentity())
                layer()->forceStackingContextIfNeeded();
            layer()->setSelfAndDescendantsNeedPositionUpdate();
            return { previousTransform, currentTransform };
        }
        if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(this)) {
            auto previousTransform = svgModel->localTransform();
            svgModel->updateLocalTransform();
            return { previousTransform, svgModel->localTransform() };
        }
        // RenderSVGText is probed, not cached: writing m_localTransform before layout would feed
        // updateScaledFont() a reference box that is only final after layout. The caller writes it
        // once the scale is known unchanged, or leaves it for layout on a scale change.
        if (auto* text = dynamicDowncast<RenderSVGText>(this))
            return { text->localTransform(), text->computeLocalTransform() };
        return { identity, identity };
    };

    // A re-layout is only necessary when the effective x/y scale changes: the next RenderSVGText
    // layout then sees a different screen font scaling factor, text metrics, etc.
    auto scaleChangedBetween = [](const AffineTransform& previousTransform, const AffineTransform& currentTransform) {
        return previousTransform != currentTransform
            && (!WTF::areEssentiallyEqual(previousTransform.xScale(), currentTransform.xScale())
                || !WTF::areEssentiallyEqual(previousTransform.yScale(), currentTransform.yScale()));
    };

    auto [previousTransform, currentTransform] = refreshTransform();

    // A scale change re-measures the text at the new on-screen font size (SVG sizes glyphs by the
    // transform scale). Mark the affected text - this renderer when the transform is on a <text>,
    // otherwise the transformed container's text descendants - and let layout recompute its metrics,
    // refresh its cached transform and repaint. setNeedsLayout() also defers the flush's position
    // update, which would otherwise run with the new transform but pre-relayout metrics.
    if (scaleChangedBetween(previousTransform, currentTransform)) {
        auto markTextForRelayout = [](RenderSVGText& text) {
            text.setNeedsTextMetricsUpdate();
            text.setNeedsLayout();
        };
        if (auto* text = dynamicDowncast<RenderSVGText>(this)) {
            markTextForRelayout(*text);
            repaintClientsOfReferencedSVGResources();
            return;
        }
        bool markedAny = false;
        for (auto& text : descendantsOfType<RenderSVGText>(*this)) {
            markTextForRelayout(text);
            markedAny = true;
        }
        if (markedAny) {
            repaintClientsOfReferencedSVGResources();
            return;
        }
    }

    // Scale unchanged, so no relayout is needed - just repaint the move. For a non-layered renderer
    // the batched transform flush repaints the moved region by comparing the renderer's repaint rect
    // from before and after the change, but it skips RenderSVGText because a text rect depends on
    // metrics the flush does not recompute. So non-layered text caches its transform here
    // (refreshTransform() only probed it) and compares its own before and after rects. Other
    // non-layered renderers were already cached by refreshTransform() and repainted by the flush.
    // Layered renderers repaint through their layer position update.
    if (auto* text = dynamicDowncast<RenderSVGText>(this); text && !isLayered) {
        auto repaintContainer = containerForRepaint().renderer;
        auto oldRects = rectsForRepaintingAfterLayout(repaintContainer.get(), RepaintOutlineBounds::Yes);
        text->updateLocalTransform();
        auto newRects = rectsForRepaintingAfterLayout(repaintContainer.get(), RepaintOutlineBounds::Yes);
        repaintAfterLayoutIfNeeded(SingleThreadWeakPtr<const RenderLayerModelObject> { repaintContainer.get() }, RequiresFullRepaint::No, oldRects, newRects);
    } else if (!isLayered && repaintMode == SVGAttributeChangeRepaintMode::Issue)
        repaint();

    // Renderers inside <clipPath>/<mask>/<pattern>/etc. don't paint directly - a transform
    // change is only visible by repainting the resource's clients.
    repaintClientsOfReferencedSVGResources();
}

void RenderLayerModelObject::paintSVGClippingMask(PaintInfo& paintInfo, const FloatRect& objectBoundingBox) const
{
    ASSERT(paintInfo.phase == PaintPhase::ClippingMask);
    auto& context = paintInfo.context();
    if (!paintInfo.shouldPaintWithinRoot(*this) || style().usedVisibility() != Visibility::Visible || context.paintingDisabled())
        return;

    ASSERT(document().settings().layerBasedSVGEngineEnabled());
    if (auto* referencedClipperRenderer = svgClipperResourceFromStyle())
        referencedClipperRenderer->applyMaskClipping(paintInfo, *this, objectBoundingBox);
}

void RenderLayerModelObject::paintSVGMask(PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset) const
{
    ASSERT(paintInfo.phase == PaintPhase::Mask);
    auto& context = paintInfo.context();
    if (!paintInfo.shouldPaintWithinRoot(*this) || context.paintingDisabled())
        return;

    ASSERT(isSVGLayerAwareRenderer());
    if (auto* referencedMaskerRenderer = svgMaskerResourceFromStyle())
        referencedMaskerRenderer->applyMask(paintInfo, *this, adjustedPaintOffset);
}

void RenderLayerModelObject::paintSVGEventRegion(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhase::EventRegion);
    if (style().usedVisibility() == Visibility::Hidden || objectBoundingBox().isEmpty())
        return;

    auto adjustedPaintOffset = paintOffset + currentSVGLayoutLocation();
    auto coordinateSystemOriginTranslation = adjustedPaintOffset - nominalSVGLayoutLocation();
    auto eventRegionBounds = strokeBoundingBox();
    eventRegionBounds.move(coordinateSystemOriginTranslation);
    paintInfo.eventRegionContext()->unite(FloatRoundedRect(eventRegionBounds), *this, style(), false);
}

AffineTransform RenderLayerModelObject::computeRendererTransform() const
{
    if (!isTransformed())
        return { };
    if (CheckedPtr renderLayer = layer())
        return renderLayer->currentTransform(Style::TransformResolver::individualTransformOperations).toAffineTransform();
    TransformationMatrix matrix;
    auto referenceBoxRect = transformReferenceBoxRect(style());
    applyTransform(matrix, style(), referenceBoxRect, Style::TransformResolver::individualTransformOperations);
    return matrix.toAffineTransform();
}

void RenderLayerModelObject::contentChanged(ContentChangeType changeType, const std::optional<FloatRect>& dirtyRect)
{
    if (CheckedPtr layer = this->layer())
        layer->contentChanged(changeType, dirtyRect);
}

bool RenderLayerModelObject::hasAcceleratedCompositing() const
{
    return view().compositor().hasAcceleratedCompositing();
}

#if ASSERT_ENABLED
bool RenderLayerModelObject::layerAccessPreventedSlow() const
{
    return view().frameView().layerAccessPrevented();
}
#endif

bool rendererNeedsPixelSnapping(const RenderLayerModelObject& renderer)
{
    if (renderer.document().settings().layerBasedSVGEngineEnabled() && renderer.isSVGLayerAwareRenderer() && !renderer.isRenderSVGRoot())
        return false;
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

