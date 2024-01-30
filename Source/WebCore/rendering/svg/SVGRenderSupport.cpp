/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SVGRenderSupport.h"

#include "ElementAncestorIteratorInlines.h"
#include "LegacyRenderSVGResourceClipper.h"
#include "LegacyRenderSVGResourceFilter.h"
#include "LegacyRenderSVGResourceMarker.h"
#include "LegacyRenderSVGResourceMasker.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShapeInlines.h"
#include "LegacyRenderSVGTransformableContainer.h"
#include "LegacyRenderSVGViewportContainer.h"
#include "NodeRenderStyle.h"
#include "ReferencedSVGResources.h"
#include "RenderChildIterator.h"
#include "RenderElement.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShapeInlines.h"
#include "RenderSVGText.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGeometryElement.h"
#include "SVGRenderStyle.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "TransformOperationData.h"
#include "TransformState.h"

namespace WebCore {

LayoutRect SVGRenderSupport::clippedOverflowRectForRepaint(const RenderElement& renderer, const RenderLayerModelObject* repaintContainer, RenderObject::VisibleRectContext context)
{
    // Return early for any cases where we don't actually paint
    if (renderer.isInsideEntirelyHiddenLayer())
        return { };

    // Pass our local paint rect to computeFloatVisibleRectInContainer() which will
    // map to parent coords and recurse up the parent chain.
    return enclosingLayoutRect(renderer.computeFloatRectForRepaint(renderer.repaintRectInLocalCoordinates(context.repaintRectCalculation()), repaintContainer));
}

std::optional<FloatRect> SVGRenderSupport::computeFloatVisibleRectInContainer(const RenderElement& renderer, const FloatRect& rect, const RenderLayerModelObject* container, RenderObject::VisibleRectContext context)
{
    // Ensure our parent is an SVG object.
    ASSERT(renderer.parent());
    auto& parent = *renderer.parent();
    if (!is<SVGElement>(parent.element()))
        return FloatRect();

    FloatRect adjustedRect = rect;
    adjustedRect.inflate(renderer.style().outlineWidth());

    // Translate to coords in our parent renderer, and then call computeFloatVisibleRectInContainer() on our parent.
    adjustedRect = renderer.localToParentTransform().mapRect(adjustedRect);

    return parent.computeFloatVisibleRectInContainer(adjustedRect, container, context);
}

const RenderElement& SVGRenderSupport::localToParentTransform(const RenderElement& renderer, AffineTransform &transform)
{
    ASSERT(renderer.parent());
    auto& parent = *renderer.parent();

    // At the SVG/HTML boundary (aka LegacyRenderSVGRoot), we apply the localToBorderBoxTransform
    // to map an element from SVG viewport coordinates to CSS box coordinates.
    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(parent))
        transform = svgRoot->localToBorderBoxTransform() * renderer.localToParentTransform();
    else
        transform = renderer.localToParentTransform();

    return parent;
}

void SVGRenderSupport::mapLocalToContainer(const RenderElement& renderer, const RenderLayerModelObject* ancestorContainer, TransformState& transformState, bool* wasFixed)
{
    AffineTransform transform;
    auto& parent = localToParentTransform(renderer, transform);

    transformState.applyTransform(transform);

    OptionSet<MapCoordinatesMode> mode = UseTransforms;
    parent.mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

const RenderElement* SVGRenderSupport::pushMappingToContainer(const RenderElement& renderer, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap)
{
    ASSERT_UNUSED(ancestorToStopAt, ancestorToStopAt != &renderer);

    AffineTransform transform;
    auto& parent = localToParentTransform(renderer, transform);

    geometryMap.push(&renderer, transform);
    return &parent;
}

bool SVGRenderSupport::checkForSVGRepaintDuringLayout(const RenderElement& renderer)
{
    if (!renderer.checkForRepaintDuringLayout())
        return false;
    // When a parent container is transformed in SVG, all children will be painted automatically
    // so we are able to skip redundant repaint checks.
    auto* parent = dynamicDowncast<LegacyRenderSVGContainer>(renderer.parent());
    return !parent || !parent->didTransformToRootUpdate();
}

// Update a bounding box taking into account the validity of the other bounding box.
static inline void updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, const RenderObject* other, FloatRect otherBoundingBox)
{
    auto* otherContainer = dynamicDowncast<LegacyRenderSVGContainer>(*other);
    bool otherValid = !otherContainer || otherContainer->isObjectBoundingBoxValid();
    if (!otherValid)
        return;

    if (!objectBoundingBoxValid) {
        objectBoundingBox = otherBoundingBox;
        objectBoundingBoxValid = true;
        return;
    }

    objectBoundingBox.uniteEvenIfEmpty(otherBoundingBox);
}

void SVGRenderSupport::computeContainerBoundingBoxes(const RenderElement& container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& repaintBoundingBox, RepaintRectCalculation repaintRectCalculation)
{
    objectBoundingBox = FloatRect();
    objectBoundingBoxValid = false;
    repaintBoundingBox = FloatRect();
    for (auto& current : childrenOfType<RenderObject>(container)) {
        if (current.isLegacyRenderSVGHiddenContainer())
            continue;

        // Don't include elements in the union that do not render.
        if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(current); shape && shape->isRenderingDisabled())
            continue;

        const AffineTransform& transform = current.localToParentTransform();
        if (transform.isIdentity()) {
            updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, &current, current.objectBoundingBox());
            repaintBoundingBox.unite(current.repaintRectInLocalCoordinates(repaintRectCalculation));
        } else {
            updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, &current, transform.mapRect(current.objectBoundingBox()));
            repaintBoundingBox.unite(transform.mapRect(current.repaintRectInLocalCoordinates(repaintRectCalculation)));
        }
    }
}

FloatRect SVGRenderSupport::computeContainerStrokeBoundingBox(const RenderElement& container)
{
    ASSERT(container.isLegacyRenderSVGRoot() || container.isLegacyRenderSVGContainer());
    FloatRect strokeBoundingBox = FloatRect();
    for (auto& current : childrenOfType<RenderObject>(container)) {
        if (current.isLegacyRenderSVGHiddenContainer())
            continue;

        // Don't include elements in the union that do not render.
        if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(current); shape && shape->isRenderingDisabled())
            continue;

        FloatRect childStrokeBoundingBox = current.strokeBoundingBox();
        if (auto* currentElement = dynamicDowncast<RenderElement>(current))
            SVGRenderSupport::intersectRepaintRectWithResources(*currentElement, childStrokeBoundingBox, RepaintRectCalculation::Accurate);
        const AffineTransform& transform = current.localToParentTransform();
        if (transform.isIdentity())
            strokeBoundingBox.unite(childStrokeBoundingBox);
        else
            strokeBoundingBox.unite(transform.mapRect(childStrokeBoundingBox));
    }
    return strokeBoundingBox;
}

bool SVGRenderSupport::paintInfoIntersectsRepaintRect(const FloatRect& localRepaintRect, const AffineTransform& localTransform, const PaintInfo& paintInfo)
{
    if (localTransform.isIdentity())
        return localRepaintRect.intersects(paintInfo.rect);

    return localTransform.mapRect(localRepaintRect).intersects(paintInfo.rect);
}

LegacyRenderSVGRoot* SVGRenderSupport::findTreeRootObject(RenderElement& start)
{
    return lineageOfType<LegacyRenderSVGRoot>(start).first();
}

const LegacyRenderSVGRoot* SVGRenderSupport::findTreeRootObject(const RenderElement& start)
{
    return lineageOfType<LegacyRenderSVGRoot>(start).first();
}

static inline void invalidateResourcesOfChildren(RenderElement& renderer)
{
    ASSERT(!renderer.needsLayout());
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer))
        resources->removeClientFromCache(renderer, false);

    for (auto& child : childrenOfType<RenderElement>(renderer))
        invalidateResourcesOfChildren(child);
}

static inline bool layoutSizeOfNearestViewportChanged(const RenderElement& renderer)
{
    for (auto* start = &renderer; start; start = start->parent()) {
        if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(*start))
            return svgRoot->isLayoutSizeChanged();
        if (auto* container = dynamicDowncast<LegacyRenderSVGViewportContainer>(*start))
            return container->isLayoutSizeChanged();
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool SVGRenderSupport::transformToRootChanged(RenderElement* ancestor)
{
    while (ancestor && !ancestor->isRenderOrLegacyRenderSVGRoot()) {
        if (CheckedPtr container = dynamicDowncast<LegacyRenderSVGTransformableContainer>(*ancestor))
            return container->didTransformToRootUpdate();
        if (CheckedPtr container = dynamicDowncast<LegacyRenderSVGViewportContainer>(*ancestor))
            return container->didTransformToRootUpdate();
        ancestor = ancestor->parent();
    }

    return false;
}

void SVGRenderSupport::layoutDifferentRootIfNeeded(const RenderElement& renderer)
{
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer))
        resources->layoutDifferentRootIfNeeded(renderer);
}

void SVGRenderSupport::layoutChildren(RenderElement& start, bool selfNeedsLayout)
{
    bool layoutSizeChanged = layoutSizeOfNearestViewportChanged(start);
    bool transformChanged = transformToRootChanged(&start);
    SingleThreadWeakHashSet<RenderElement> elementsThatDidNotReceiveLayout;

    for (auto& child : childrenOfType<RenderObject>(start)) {
        bool needsLayout = selfNeedsLayout;
        bool childEverHadLayout = child.everHadLayout();

        if (transformChanged) {
            // If the transform changed we need to update the text metrics (note: this also happens for layoutSizeChanged=true).
            if (CheckedPtr text = dynamicDowncast<RenderSVGText>(child))
                text->setNeedsTextMetricsUpdate();
            needsLayout = true;
        }

        if (layoutSizeChanged) {
            // When selfNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            if (auto* element = dynamicDowncast<SVGElement>(*child.node()); element && element->hasRelativeLengths()) {
                // When the layout size changed and when using relative values tell the LegacyRenderSVGShape to update its shape object
                if (CheckedPtr shape = dynamicDowncast<LegacyRenderSVGShape>(child))
                    shape->setNeedsShapeUpdate();
                else if (CheckedPtr svgText = dynamicDowncast<RenderSVGText>(child)) {
                    svgText->setNeedsTextMetricsUpdate();
                    svgText->setNeedsPositioningValuesUpdate();
                }
                child.setNeedsTransformUpdate();
                needsLayout = true;
            }
        }

        if (needsLayout)
            child.setNeedsLayout(MarkOnlyThis);

        if (child.needsLayout()) {
            CheckedRef childElement = downcast<RenderElement>(child);
            layoutDifferentRootIfNeeded(childElement);
            childElement->layout();
            // Renderers are responsible for repainting themselves when changing, except
            // for the initial paint to avoid potential double-painting caused by non-sensical "old" bounds.
            // We could handle this in the individual objects, but for now it's easier to have
            // parent containers call repaint().  (RenderBlock::layout* has similar logic.)
            if (!childEverHadLayout)
                child.repaint();
        } else if (layoutSizeChanged) {
            if (auto* childElement = dynamicDowncast<RenderElement>(child))
                elementsThatDidNotReceiveLayout.add(*childElement);
        }

        ASSERT(!child.needsLayout());
    }

    if (!layoutSizeChanged) {
        ASSERT(elementsThatDidNotReceiveLayout.isEmptyIgnoringNullReferences());
        return;
    }

    // If the layout size changed, invalidate all resources of all children that didn't go through the layout() code path.
    for (auto& element : elementsThatDidNotReceiveLayout)
        invalidateResourcesOfChildren(element);
}

bool SVGRenderSupport::isOverflowHidden(const RenderElement& renderer)
{
    // LegacyRenderSVGRoot should never query for overflow state - it should always clip itself to the initial viewport size.
    ASSERT(!renderer.isDocumentElementRenderer());

    return isNonVisibleOverflow(renderer.style().overflowX());
}

void SVGRenderSupport::intersectRepaintRectWithResources(const RenderElement& renderer, FloatRect& repaintRect, RepaintRectCalculation repaintRectCalculation)
{
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources)
        return;

    if (LegacyRenderSVGResourceFilter* filter = resources->filter())
        repaintRect = filter->resourceBoundingBox(renderer, repaintRectCalculation);

    if (LegacyRenderSVGResourceClipper* clipper = resources->clipper())
        repaintRect.intersect(clipper->resourceBoundingBox(renderer, repaintRectCalculation));

    if (auto* masker = resources->masker())
        repaintRect.intersect(masker->resourceBoundingBox(renderer, repaintRectCalculation));
}

bool SVGRenderSupport::filtersForceContainerLayout(const RenderElement& renderer)
{
    // If any of this container's children need to be laid out, and a filter is applied
    // to the container, we need to repaint the entire container.
    if (!renderer.normalChildNeedsLayout())
        return false;

    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources || !resources->filter())
        return false;

    return true;
}

inline FloatRect clipPathReferenceBox(const RenderElement& renderer, CSSBoxType boxType)
{
    FloatRect referenceBox;
    switch (boxType) {
    case CSSBoxType::BorderBox:
    case CSSBoxType::MarginBox:
    case CSSBoxType::StrokeBox:
    case CSSBoxType::BoxMissing:
        // FIXME: strokeBoundingBox() takes dasharray into account but shouldn't.
        referenceBox = renderer.strokeBoundingBox();
        break;
    case CSSBoxType::ViewBox:
        if (renderer.element()) {
            auto viewportSize = SVGLengthContext(downcast<SVGElement>(renderer.element())).viewportSize();
            if (viewportSize)
                referenceBox.setSize(*viewportSize);
            break;
        }
        FALLTHROUGH;
    case CSSBoxType::ContentBox:
    case CSSBoxType::FillBox:
    case CSSBoxType::PaddingBox:
        referenceBox = renderer.objectBoundingBox();
        break;
    }
    return referenceBox;
}

inline bool isPointInCSSClippingArea(const RenderElement& renderer, const FloatPoint& point)
{
    PathOperation* clipPathOperation = renderer.style().clipPath();
    if (auto* clipPath = dynamicDowncast<ShapePathOperation>(clipPathOperation)) {
        FloatRect referenceBox = clipPathReferenceBox(renderer, clipPath->referenceBox());
        if (!referenceBox.contains(point))
            return false;
        return clipPath->pathForReferenceRect(referenceBox).contains(point, clipPath->windRule());
    }
    if (auto* clipPath = dynamicDowncast<BoxPathOperation>(clipPathOperation)) {
        FloatRect referenceBox = clipPathReferenceBox(renderer, clipPath->referenceBox());
        if (!referenceBox.contains(point))
            return false;
        return clipPath->pathForReferenceRect(FloatRoundedRect { referenceBox }).contains(point);
    }

    return true;
}

void SVGRenderSupport::clipContextToCSSClippingArea(GraphicsContext& context, const RenderElement& renderer)
{
    PathOperation* clipPathOperation = renderer.style().clipPath();
    if (auto* clipPath = dynamicDowncast<ShapePathOperation>(clipPathOperation)) {
        auto localToParentTransform = renderer.localToParentTransform();

        auto referenceBox = clipPathReferenceBox(renderer, clipPath->referenceBox());
        referenceBox = localToParentTransform.mapRect(referenceBox);

        auto path = clipPath->pathForReferenceRect(referenceBox);
        path.transform(valueOrDefault(localToParentTransform.inverse()));

        context.clipPath(path, clipPath->windRule());
    }
    if (auto* clipPath = dynamicDowncast<BoxPathOperation>(clipPathOperation)) {
        FloatRect referenceBox = clipPathReferenceBox(renderer, clipPath->referenceBox());
        context.clipPath(clipPath->pathForReferenceRect(FloatRoundedRect { referenceBox }));
    }
}

bool SVGRenderSupport::pointInClippingArea(const RenderElement& renderer, const FloatPoint& point)
{
    if (SVGHitTestCycleDetectionScope::isVisiting(renderer))
        return false;

    PathOperation* clipPathOperation = renderer.style().clipPath();
    if (is<ShapePathOperation>(clipPathOperation) || is<BoxPathOperation>(clipPathOperation))
        return isPointInCSSClippingArea(renderer, point);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (renderer.document().settings().layerBasedSVGEngineEnabled()) {
        if (auto* layerRenderer = dynamicDowncast<RenderLayerModelObject>(renderer)) {
            if (auto* referencedClipperRenderer = layerRenderer->svgClipperResourceFromStyle())
                return referencedClipperRenderer->hitTestClipContent(renderer.objectBoundingBox(), LayoutPoint(point));
        }

        return true;
    }
#endif

    // We just take clippers into account to determine if a point is on the node. The Specification may
    // change later and we also need to check maskers.
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources)
        return true;

    if (LegacyRenderSVGResourceClipper* clipper = resources->clipper())
        return clipper->hitTestClipContent(renderer.objectBoundingBox(), point);

    return true;
}

void SVGRenderSupport::applyStrokeStyleToContext(GraphicsContext& context, const RenderStyle& style, const RenderElement& renderer)
{
    RefPtr element = dynamicDowncast<SVGElement>(renderer.element());
    if (!element) {
        ASSERT_NOT_REACHED();
        return;
    }

    const SVGRenderStyle& svgStyle = style.svgStyle();

    SVGLengthContext lengthContext(element.get());
    context.setStrokeThickness(lengthContext.valueForLength(style.strokeWidth()));
    context.setLineCap(style.capStyle());
    context.setLineJoin(style.joinStyle());
    if (style.joinStyle() == LineJoin::Miter)
        context.setMiterLimit(style.strokeMiterLimit());

    const Vector<SVGLengthValue>& dashes = svgStyle.strokeDashArray();
    if (dashes.isEmpty())
        context.setStrokeStyle(StrokeStyle::SolidStroke);
    else {
        float scaleFactor = 1;

        if (auto geometryElement = dynamicDowncast<SVGGeometryElement>(*element)) {
            ASSERT(renderer.isRenderOrLegacyRenderSVGShape());
            // FIXME: A value of zero is valid. Need to differentiate this case from being unspecified.
            if (float pathLength = geometryElement->pathLength()) {
                if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
                    scaleFactor = shape->getTotalLength() / pathLength;
#if ENABLE(LAYER_BASED_SVG_ENGINE)
                else if (auto* shape = dynamicDowncast<RenderSVGShape>(renderer))
                    scaleFactor = shape->getTotalLength() / pathLength;
#endif
            }
        }
        
        bool canSetLineDash = false;
        auto dashArray = WTF::map(dashes, [&](auto& dash) -> DashArrayElement {
            auto value = dash.value(lengthContext) * scaleFactor;
            if (value > 0)
                canSetLineDash = true;
            return value;
        });

        if (canSetLineDash)
            context.setLineDash(dashArray, lengthContext.valueForLength(svgStyle.strokeDashOffset()) * scaleFactor);
        else
            context.setStrokeStyle(StrokeStyle::SolidStroke);
    }
}

void SVGRenderSupport::styleChanged(RenderElement& renderer, const RenderStyle* oldStyle)
{
    if (renderer.element() && renderer.element()->isSVGElement() && (!oldStyle || renderer.style().hasBlendMode() != oldStyle->hasBlendMode()))
        SVGRenderSupport::updateMaskedAncestorShouldIsolateBlending(renderer);
}

bool SVGRenderSupport::isolatesBlending(const RenderStyle& style)
{
    return style.hasPositionedMask() || style.hasFilter() || style.hasBlendMode() || style.opacity() < 1.0f;
}

void SVGRenderSupport::updateMaskedAncestorShouldIsolateBlending(const RenderElement& renderer)
{
    ASSERT(renderer.element());
    ASSERT(renderer.element()->isSVGElement());

    for (auto& ancestor : ancestorsOfType<SVGGraphicsElement>(*renderer.element())) {
        auto* style = ancestor.computedStyle();
        if (!style || !isolatesBlending(*style))
            continue;
        if (style->hasPositionedMask())
            ancestor.setShouldIsolateBlending(renderer.style().hasBlendMode());
        return;
    }
}

SVGHitTestCycleDetectionScope::SVGHitTestCycleDetectionScope(const RenderElement& element, bool condition)
{
    if (condition) {
        m_element = element;
        auto result = visitedElements().add(*m_element);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}

SVGHitTestCycleDetectionScope::~SVGHitTestCycleDetectionScope()
{
    if (m_element) {
        bool result = visitedElements().remove(*m_element);
        ASSERT_UNUSED(result, result);
    }
}

SingleThreadWeakHashSet<RenderElement>& SVGHitTestCycleDetectionScope::visitedElements()
{
    static NeverDestroyed<SingleThreadWeakHashSet<RenderElement>> s_visitedElements;
    return s_visitedElements;
}

bool SVGHitTestCycleDetectionScope::isEmpty()
{
    return visitedElements().isEmptyIgnoringNullReferences();
}

bool SVGHitTestCycleDetectionScope::isVisiting(const RenderElement& element)
{
    return visitedElements().contains(element);
}

FloatRect SVGRenderSupport::calculateApproximateStrokeBoundingBox(const RenderElement& renderer)
{
    auto calculateApproximateScalingStrokeBoundingBox = [&](const auto& renderer, FloatRect fillBoundingBox) -> FloatRect {
        // Implementation of
        // https://drafts.fxtf.org/css-masking/#compute-stroke-bounding-box
        // except that we ignore whether the stroke is none.

        using Renderer = std::decay_t<decltype(renderer)>;

        ASSERT(renderer.style().svgStyle().hasStroke());

        auto strokeBoundingBox = fillBoundingBox;
        const float strokeWidth = renderer.strokeWidth();
        if (strokeWidth <= 0)
            return strokeBoundingBox;

        float delta = strokeWidth / 2;
        switch (renderer.shapeType()) {
        case Renderer::ShapeType::Empty: {
            // Spec: "A negative value is illegal. A value of zero disables rendering of the element."
            return strokeBoundingBox;
        }
        case Renderer::ShapeType::Ellipse:
        case Renderer::ShapeType::Circle:
            break;
        case Renderer::ShapeType::Rectangle:
        case Renderer::ShapeType::RoundedRectangle: {
#if USE(CG)
            // CoreGraphics can inflate the stroke by 1px when drawing a rectangle with antialiasing disabled at non-integer coordinates, we need to compensate.
            if (renderer.style().svgStyle().shapeRendering() == ShapeRendering::CrispEdges)
                delta += 1;
#endif
            break;
        }
        case Renderer::ShapeType::Path:
        case Renderer::ShapeType::Line: {
            auto& style = renderer.style();
            if (renderer.shapeType() == Renderer::ShapeType::Path && style.joinStyle() == LineJoin::Miter) {
                const float miter = style.strokeMiterLimit();
                if (miter < sqrtOfTwoDouble && style.capStyle() == LineCap::Square)
                    delta *= sqrtOfTwoDouble;
                else
                    delta *= std::max(miter, 1.0f);
            } else if (style.capStyle() == LineCap::Square)
                delta *= sqrtOfTwoDouble;
            break;
        }
        }

        strokeBoundingBox.inflate(delta);
        return strokeBoundingBox;
    };

    auto calculateApproximateNonScalingStrokeBoundingBox = [&](const auto& renderer, FloatRect fillBoundingBox) -> FloatRect {
        ASSERT(renderer.hasPath());
        ASSERT(renderer.style().svgStyle().hasStroke());
        ASSERT(renderer.hasNonScalingStroke());

        auto strokeBoundingBox = fillBoundingBox;
        auto nonScalingTransform = renderer.nonScalingStrokeTransform();
        if (auto inverse = nonScalingTransform.inverse()) {
            auto* usePath = renderer.nonScalingStrokePath(&renderer.path(), nonScalingTransform);
            auto strokeBoundingRect = calculateApproximateScalingStrokeBoundingBox(renderer, usePath->fastBoundingRect());
            strokeBoundingRect = inverse.value().mapRect(strokeBoundingRect);
            strokeBoundingBox.unite(strokeBoundingRect);
        }
        return strokeBoundingBox;
    };

    auto calculate = [&](const auto& renderer) {
        if (!renderer.style().svgStyle().hasStroke())
            return renderer.objectBoundingBox();
        if (renderer.hasNonScalingStroke())
            return calculateApproximateNonScalingStrokeBoundingBox(renderer, renderer.objectBoundingBox());
        return calculateApproximateScalingStrokeBoundingBox(renderer, renderer.objectBoundingBox());
    };

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(renderer))
        return shape->adjustStrokeBoundingBoxForMarkersAndZeroLengthLinecaps(RepaintRectCalculation::Fast, calculate(*shape));

    const auto& shape = downcast<RenderSVGShape>(renderer);
    return shape.adjustStrokeBoundingBoxForZeroLengthLinecaps(RepaintRectCalculation::Fast, calculate(shape));
#else
    const auto& shape = downcast<LegacyRenderSVGShape>(renderer);
    return shape.adjustStrokeBoundingBoxForMarkersAndZeroLengthLinecaps(RepaintRectCalculation::Fast, calculate(shape));
#endif
}

}
