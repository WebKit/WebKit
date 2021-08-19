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

#include "ElementAncestorIterator.h"
#include "NodeRenderStyle.h"
#include "RenderChildIterator.h"
#include "RenderElement.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderSVGImage.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderSVGTransformableContainer.h"
#include "RenderSVGViewportContainer.h"
#include "SVGGeometryElement.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "TransformState.h"

namespace WebCore {

LayoutRect SVGRenderSupport::clippedOverflowRectForRepaint(const RenderElement& renderer, const RenderLayerModelObject* repaintContainer)
{
    // Return early for any cases where we don't actually paint
    if (renderer.style().visibility() != Visibility::Visible && !renderer.enclosingLayer()->hasVisibleContent())
        return LayoutRect();

    // Pass our local paint rect to computeFloatVisibleRectInContainer() which will
    // map to parent coords and recurse up the parent chain.
    FloatRect repaintRect = renderer.repaintRectInLocalCoordinates();
    return enclosingLayoutRect(renderer.computeFloatRectForRepaint(repaintRect, repaintContainer));
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

    // At the SVG/HTML boundary (aka RenderSVGRoot), we apply the localToBorderBoxTransform
    // to map an element from SVG viewport coordinates to CSS box coordinates.
    if (is<RenderSVGRoot>(parent))
        transform = downcast<RenderSVGRoot>(parent).localToBorderBoxTransform() * renderer.localToParentTransform();
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
    auto parent = renderer.parent();
    return !(is<RenderSVGContainer>(parent) && downcast<RenderSVGContainer>(*parent).didTransformToRootUpdate());
}

// Update a bounding box taking into account the validity of the other bounding box.
static inline void updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, const RenderObject* other, FloatRect otherBoundingBox)
{
    bool otherValid = is<RenderSVGContainer>(*other) ? downcast<RenderSVGContainer>(*other).isObjectBoundingBoxValid() : true;
    if (!otherValid)
        return;

    if (!objectBoundingBoxValid) {
        objectBoundingBox = otherBoundingBox;
        objectBoundingBoxValid = true;
        return;
    }

    objectBoundingBox.uniteEvenIfEmpty(otherBoundingBox);
}

void SVGRenderSupport::computeContainerBoundingBoxes(const RenderElement& container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& repaintBoundingBox)
{
    objectBoundingBox = FloatRect();
    objectBoundingBoxValid = false;
    strokeBoundingBox = FloatRect();

    // When computing the strokeBoundingBox, we use the repaintRects of the container's children so that the container's stroke includes
    // the resources applied to the children (such as clips and filters). This allows filters applied to containers to correctly bound
    // the children, and also improves inlining of SVG content, as the stroke bound is used in that situation also.
    for (auto& current : childrenOfType<RenderObject>(container)) {
        if (current.isSVGHiddenContainer())
            continue;

        // Don't include elements in the union that do not render.
        if (is<RenderSVGShape>(current) && downcast<RenderSVGShape>(current).isRenderingDisabled())
            continue;

        const AffineTransform& transform = current.localToParentTransform();
        if (transform.isIdentity()) {
            updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, &current, current.objectBoundingBox());
            strokeBoundingBox.unite(current.repaintRectInLocalCoordinates());
        } else {
            updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, &current, transform.mapRect(current.objectBoundingBox()));
            strokeBoundingBox.unite(transform.mapRect(current.repaintRectInLocalCoordinates()));
        }
    }

    repaintBoundingBox = strokeBoundingBox;
}

bool SVGRenderSupport::paintInfoIntersectsRepaintRect(const FloatRect& localRepaintRect, const AffineTransform& localTransform, const PaintInfo& paintInfo)
{
    if (localTransform.isIdentity())
        return localRepaintRect.intersects(paintInfo.rect);

    return localTransform.mapRect(localRepaintRect).intersects(paintInfo.rect);
}

RenderSVGRoot* SVGRenderSupport::findTreeRootObject(RenderElement& start)
{
    return lineageOfType<RenderSVGRoot>(start).first();
}

const RenderSVGRoot* SVGRenderSupport::findTreeRootObject(const RenderElement& start)
{
    return lineageOfType<RenderSVGRoot>(start).first();
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
    const RenderElement* start = &renderer;
    while (start && !is<RenderSVGRoot>(*start) && !is<RenderSVGViewportContainer>(*start))
        start = start->parent();

    ASSERT(start);
    if (is<RenderSVGViewportContainer>(*start))
        return downcast<RenderSVGViewportContainer>(*start).isLayoutSizeChanged();

    return downcast<RenderSVGRoot>(*start).isLayoutSizeChanged();
}

bool SVGRenderSupport::transformToRootChanged(RenderElement* ancestor)
{
    while (ancestor && !is<RenderSVGRoot>(*ancestor)) {
        if (is<RenderSVGTransformableContainer>(*ancestor))
            return downcast<RenderSVGTransformableContainer>(*ancestor).didTransformToRootUpdate();
        if (is<RenderSVGViewportContainer>(*ancestor))
            return downcast<RenderSVGViewportContainer>(*ancestor).didTransformToRootUpdate();
        ancestor = ancestor->parent();
    }

    return false;
}

void SVGRenderSupport::layoutDifferentRootIfNeeded(const RenderElement& renderer)
{
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer)) {
        auto* svgRoot = SVGRenderSupport::findTreeRootObject(renderer);
        ASSERT(svgRoot);
        resources->layoutDifferentRootIfNeeded(svgRoot);
    }
}

void SVGRenderSupport::layoutChildren(RenderElement& start, bool selfNeedsLayout)
{
    bool layoutSizeChanged = layoutSizeOfNearestViewportChanged(start);
    bool transformChanged = transformToRootChanged(&start);
    HashSet<RenderElement*> elementsThatDidNotReceiveLayout;

    for (auto& child : childrenOfType<RenderObject>(start)) {
        bool needsLayout = selfNeedsLayout;
        bool childEverHadLayout = child.everHadLayout();

        if (transformChanged) {
            // If the transform changed we need to update the text metrics (note: this also happens for layoutSizeChanged=true).
            if (is<RenderSVGText>(child))
                downcast<RenderSVGText>(child).setNeedsTextMetricsUpdate();
            needsLayout = true;
        }

        if (layoutSizeChanged && is<SVGElement>(*child.node())) {
            // When selfNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            auto& element = downcast<SVGElement>(*child.node());
            if (element.hasRelativeLengths()) {
                // When the layout size changed and when using relative values tell the RenderSVGShape to update its shape object
                if (is<RenderSVGShape>(child))
                    downcast<RenderSVGShape>(child).setNeedsShapeUpdate();
                else if (is<RenderSVGText>(child)) {
                    auto& svgText = downcast<RenderSVGText>(child);
                    svgText.setNeedsTextMetricsUpdate();
                    svgText.setNeedsPositioningValuesUpdate();
                }

                needsLayout = true;
            }
        }

        if (needsLayout)
            child.setNeedsLayout(MarkOnlyThis);

        if (child.needsLayout()) {
            layoutDifferentRootIfNeeded(downcast<RenderElement>(child));
            downcast<RenderElement>(child).layout();
            // Renderers are responsible for repainting themselves when changing, except
            // for the initial paint to avoid potential double-painting caused by non-sensical "old" bounds.
            // We could handle this in the individual objects, but for now it's easier to have
            // parent containers call repaint().  (RenderBlock::layout* has similar logic.)
            if (!childEverHadLayout)
                child.repaint();
        } else if (layoutSizeChanged && is<RenderElement>(child))
            elementsThatDidNotReceiveLayout.add(&downcast<RenderElement>(child));

        ASSERT(!child.needsLayout());
    }

    if (!layoutSizeChanged) {
        ASSERT(elementsThatDidNotReceiveLayout.isEmpty());
        return;
    }

    // If the layout size changed, invalidate all resources of all children that didn't go through the layout() code path.
    for (auto* element : elementsThatDidNotReceiveLayout)
        invalidateResourcesOfChildren(*element);
}

bool SVGRenderSupport::isOverflowHidden(const RenderElement& renderer)
{
    // RenderSVGRoot should never query for overflow state - it should always clip itself to the initial viewport size.
    ASSERT(!renderer.isDocumentElementRenderer());

    return renderer.style().overflowX() == Overflow::Hidden || renderer.style().overflowX() == Overflow::Scroll;
}

void SVGRenderSupport::intersectRepaintRectWithResources(const RenderElement& renderer, FloatRect& repaintRect)
{
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources)
        return;

    if (RenderSVGResourceFilter* filter = resources->filter())
        repaintRect = filter->resourceBoundingBox(renderer);

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        repaintRect.intersect(clipper->resourceBoundingBox(renderer));

    if (RenderSVGResourceMasker* masker = resources->masker())
        repaintRect.intersect(masker->resourceBoundingBox(renderer));
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
        // FIXME: strokeBoundingBox() takes dasharray into account but shouldn't.
        referenceBox = renderer.strokeBoundingBox();
        break;
    case CSSBoxType::ViewBox:
        if (renderer.element()) {
            FloatSize viewportSize;
            SVGLengthContext(downcast<SVGElement>(renderer.element())).determineViewport(viewportSize);
            referenceBox.setSize(viewportSize);
            break;
        }
        FALLTHROUGH;
    case CSSBoxType::ContentBox:
    case CSSBoxType::FillBox:
    case CSSBoxType::PaddingBox:
    case CSSBoxType::BoxMissing:
        referenceBox = renderer.objectBoundingBox();
        break;
    }
    return referenceBox;
}

inline bool isPointInCSSClippingArea(const RenderElement& renderer, const FloatPoint& point)
{
    ClipPathOperation* clipPathOperation = renderer.style().clipPath();
    if (is<ShapeClipPathOperation>(clipPathOperation)) {
        auto& clipPath = downcast<ShapeClipPathOperation>(*clipPathOperation);
        FloatRect referenceBox = clipPathReferenceBox(renderer, clipPath.referenceBox());
        if (!referenceBox.contains(point))
            return false;
        return clipPath.pathForReferenceRect(referenceBox).contains(point, clipPath.windRule());
    }
    if (is<BoxClipPathOperation>(clipPathOperation)) {
        auto& clipPath = downcast<BoxClipPathOperation>(*clipPathOperation);
        FloatRect referenceBox = clipPathReferenceBox(renderer, clipPath.referenceBox());
        if (!referenceBox.contains(point))
            return false;
        return clipPath.pathForReferenceRect(FloatRoundedRect {referenceBox}).contains(point);
    }

    return true;
}

void SVGRenderSupport::clipContextToCSSClippingArea(GraphicsContext& context, const RenderElement& renderer)
{
    ClipPathOperation* clipPathOperation = renderer.style().clipPath();
    if (is<ShapeClipPathOperation>(clipPathOperation)) {
        auto& clipPath = downcast<ShapeClipPathOperation>(*clipPathOperation);
        auto localToParentTransform = renderer.localToParentTransform();

        auto referenceBox = clipPathReferenceBox(renderer, clipPath.referenceBox());
        referenceBox = localToParentTransform.mapRect(referenceBox);

        auto path = clipPath.pathForReferenceRect(referenceBox);
        path.transform(localToParentTransform.inverse().value_or(AffineTransform()));

        context.clipPath(path, clipPath.windRule());
    }
    if (is<BoxClipPathOperation>(clipPathOperation)) {
        auto& clipPath = downcast<BoxClipPathOperation>(*clipPathOperation);
        FloatRect referenceBox = clipPathReferenceBox(renderer, clipPath.referenceBox());
        context.clipPath(clipPath.pathForReferenceRect(FloatRoundedRect {referenceBox}));
    }
}

bool SVGRenderSupport::pointInClippingArea(const RenderElement& renderer, const FloatPoint& point)
{
    if (SVGHitTestCycleDetectionScope::isVisiting(renderer))
        return false;

    ClipPathOperation* clipPathOperation = renderer.style().clipPath();
    if (is<ShapeClipPathOperation>(clipPathOperation) || is<BoxClipPathOperation>(clipPathOperation))
        return isPointInCSSClippingArea(renderer, point);

    // We just take clippers into account to determine if a point is on the node. The Specification may
    // change later and we also need to check maskers.
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer);
    if (!resources)
        return true;

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        return clipper->hitTestClipContent(renderer.objectBoundingBox(), point);

    return true;
}

void SVGRenderSupport::applyStrokeStyleToContext(GraphicsContext& context, const RenderStyle& style, const RenderElement& renderer)
{
    Element* element = renderer.element();
    if (!is<SVGElement>(element)) {
        ASSERT_NOT_REACHED();
        return;
    }

    const SVGRenderStyle& svgStyle = style.svgStyle();

    SVGLengthContext lengthContext(downcast<SVGElement>(renderer.element()));
    context.setStrokeThickness(lengthContext.valueForLength(style.strokeWidth()));
    context.setLineCap(style.capStyle());
    context.setLineJoin(style.joinStyle());
    if (style.joinStyle() == MiterJoin)
        context.setMiterLimit(style.strokeMiterLimit());

    const Vector<SVGLengthValue>& dashes = svgStyle.strokeDashArray();
    if (dashes.isEmpty())
        context.setStrokeStyle(SolidStroke);
    else {
        DashArray dashArray;
        dashArray.reserveInitialCapacity(dashes.size());
        bool canSetLineDash = false;
        float scaleFactor = 1;

        if (is<SVGGeometryElement>(element)) {
            ASSERT(renderer.isSVGShape());
            // FIXME: A value of zero is valid. Need to differentiate this case from being unspecified.
            if (float pathLength = downcast<SVGGeometryElement>(element)->pathLength())
                scaleFactor = downcast<RenderSVGShape>(renderer).getTotalLength() / pathLength;
        }
        
        for (auto& dash : dashes) {
            dashArray.uncheckedAppend(dash.value(lengthContext) * scaleFactor);
            if (dashArray.last() > 0)
                canSetLineDash = true;
        }

        if (canSetLineDash)
            context.setLineDash(dashArray, lengthContext.valueForLength(svgStyle.strokeDashOffset()) * scaleFactor);
        else
            context.setStrokeStyle(SolidStroke);
    }
}

void SVGRenderSupport::styleChanged(RenderElement& renderer, const RenderStyle* oldStyle)
{
#if ENABLE(CSS_COMPOSITING)
    if (renderer.element() && renderer.element()->isSVGElement() && (!oldStyle || renderer.style().hasBlendMode() != oldStyle->hasBlendMode()))
        SVGRenderSupport::updateMaskedAncestorShouldIsolateBlending(renderer);
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(oldStyle);
#endif
}

#if ENABLE(CSS_COMPOSITING)

bool SVGRenderSupport::isolatesBlending(const RenderStyle& style)
{
    return style.svgStyle().isolatesBlending() || style.hasFilter() || style.hasBlendMode() || style.opacity() < 1.0f;
}

void SVGRenderSupport::updateMaskedAncestorShouldIsolateBlending(const RenderElement& renderer)
{
    ASSERT(renderer.element());
    ASSERT(renderer.element()->isSVGElement());

    for (auto& ancestor : ancestorsOfType<SVGGraphicsElement>(*renderer.element())) {
        auto* style = ancestor.computedStyle();
        if (!style || !isolatesBlending(*style))
            continue;
        if (style->svgStyle().hasMasker())
            ancestor.setShouldIsolateBlending(renderer.style().hasBlendMode());
        return;
    }
}

#endif

SVGHitTestCycleDetectionScope::SVGHitTestCycleDetectionScope(const RenderElement& element)
{
    m_element = makeWeakPtr(&element);
    auto result = visitedElements().add(*m_element);
    ASSERT_UNUSED(result, result.isNewEntry);
}

SVGHitTestCycleDetectionScope::~SVGHitTestCycleDetectionScope()
{
    bool result = visitedElements().remove(*m_element);
    ASSERT_UNUSED(result, result);
}

WeakHashSet<RenderElement>& SVGHitTestCycleDetectionScope::visitedElements()
{
    static NeverDestroyed<WeakHashSet<RenderElement>> s_visitedElements;
    return s_visitedElements;
}

bool SVGHitTestCycleDetectionScope::isEmpty()
{
    return visitedElements().computesEmpty();
}

bool SVGHitTestCycleDetectionScope::isVisiting(const RenderElement& element)
{
    return visitedElements().contains(element);
}

}
