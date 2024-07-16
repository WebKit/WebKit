/*
 * Copyright (C) 2021, 2022, 2023 Igalia S.L.
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
#include "SVGBoundingBoxComputation.h"

#include "NestingLevelIncrementer.h"
#include "ReferencedSVGResources.h"
#include "RenderChildIterator.h"
#include "RenderObjectInlines.h"
#include "RenderSVGContainer.h"
#include "RenderSVGForeignObject.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGImage.h"
#include "RenderSVGInline.h"
#include "RenderSVGPath.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShape.h"
#include "RenderSVGText.h"
#include "SVGClipPathElement.h"
#include "SVGLayerTransformComputation.h"

namespace WebCore {

SVGBoundingBoxComputation::SVGBoundingBoxComputation(const RenderLayerModelObject& renderer)
    : m_renderer(renderer)
{
}

FloatRect SVGBoundingBoxComputation::computeDecoratedBoundingBox(const SVGBoundingBoxComputation::DecorationOptions& options, bool* boundingBoxValid) const
{
    // SVG2: Bounding boxes algorithm (https://svgwg.org/svg2-draft/coords.html#BoundingBoxes)

    // The following algorithm defines how to compute a bounding box for a given element. The inputs to the algorithm are:
    // - element, the element we are computing a bounding box for;
    // - space, a coordinate space in which the bounding box will be computed;
    // - fill, a boolean indicating whether the bounding box includes the geometry of the element and its descendants;
    // - stroke, a boolean indicating whether the bounding box includes the stroke of the element and its descendants;
    // - markers, a boolean indicating whether the bounding box includes the markers of the element and its descendants; and
    // - clipped, a boolean indicating whether the bounding box is affected by any clipping paths applied to the element and its descendants.

    // The algorithm to compute the bounding box is as follows, depending on the type of element:
    // - a shape (RenderSVGShape)
    // - a text content element (RenderSVGText or RenderSVGInline)
    // - an "a" element within a text content element (-> creates RenderSVGInline)
    if (is<RenderSVGShape>(m_renderer) || is<RenderSVGText>(m_renderer) || is<RenderSVGInline>(m_renderer))
        return handleShapeOrTextOrInline(options, boundingBoxValid);

    // - a container element (RenderSVGRoot / RenderSVGContainer)
    // - "use" (RenderSVGTransformableContainer)
    if (is<RenderSVGRoot>(m_renderer) || is<RenderSVGContainer>(m_renderer))
        return handleRootOrContainer(options, boundingBoxValid);

    // - "foreignObject"
    // - "image"
    if (is<RenderSVGForeignObject>(m_renderer) || is<RenderSVGImage>(m_renderer))
        return handleForeignObjectOrImage(options, boundingBoxValid);

    ASSERT_NOT_REACHED();
    return FloatRect();
}

FloatRect SVGBoundingBoxComputation::handleShapeOrTextOrInline(const SVGBoundingBoxComputation::DecorationOptions& options, bool* boundingBoxValid) const
{
    // 1. Let box be a rectangle initialized to (0, 0, 0, 0).
    FloatRect box;

    // 2. Let fill-shape be the equivalent path of element if it is a shape, or a shape that includes each of the
    //    glyph cells corresponding to the text within the elements otherwise.
    // 3. If fill is true, then set box to the tightest rectangle in the coordinate system space that contains fill-shape.
    //
    // Note: The values of the fill, fill-opacity and fill-rule properties do not affect fill-shape.
    if (options.contains(DecorationOption::IncludeFillShape))
        box = m_renderer->objectBoundingBox();

    // 4. If stroke is true and the element's stroke is anything other than none, then set box to be the union of box
    //    and the tightest rectangle in coordinate system space that contains the stroke shape of the element, with the
    //    assumption that the element has no dash pattern.
    //
    // Note: The values of the stroke-opacity, stroke-dasharray and stroke-dashoffset do not affect the calculation of the stroke shape.
    if (options.contains(DecorationOption::IncludeStrokeShape)) {
        if (options.contains(DecorationOption::CalculateFastRepaintRect) && is<RenderSVGShape>(m_renderer))
            box.unite(downcast<RenderSVGShape>(m_renderer.get()).approximateStrokeBoundingBox());
        else
            box.unite(m_renderer->strokeBoundingBox());
    }

    // 5. If markers is true, then for each marker marker rendered on the element:
    // - For each descendant graphics element child of the "marker" element that defines marker's content:
    //   - If child has an ancestor element within the "marker" that is 'display: none', has a failing conditional processing attribute,
    //     or is not an "a", "g", "svg" or "switch" element, then continue to the next descendant graphics element.
    //   - Otherwise, set box to be the union of box and the result of invoking the algorithm to compute a bounding box with child as
    //     the element, space as the target coordinate space, true for fill, stroke and markers, and clipped for clipped.
    if (options.contains(DecorationOption::IncludeMarkers)) {
        if (CheckedPtr svgPath = dynamicDowncast<RenderSVGPath>(m_renderer.get())) {
            DecorationOptions optionsForMarker = { DecorationOption::IncludeFillShape, DecorationOption::IncludeStrokeShape, DecorationOption::IncludeMarkers };
            if (options.contains(DecorationOption::IncludeClippers))
                optionsForMarker.add(DecorationOption::IncludeClippers);
            if (options.contains(DecorationOption::CalculateFastRepaintRect))
                optionsForMarker.add(DecorationOption::CalculateFastRepaintRect);
            box.unite(svgPath->computeMarkerBoundingBox(optionsForMarker));
        }
    }

    // 6. If clipped is true and the value of clip-path on element is not none, then set box to be the tightest rectangle
    //    in coordinate system space that contains the intersection of box and the clipping path.
    adjustBoxForClippingAndEffects(options, box);

    // 7. Return box.
    if (boundingBoxValid)
        *boundingBoxValid = true;
    return box;
}

FloatRect SVGBoundingBoxComputation::handleRootOrContainer(const SVGBoundingBoxComputation::DecorationOptions& options, bool* boundingBoxValid) const
{
    auto transformationMatrixFromChild = [&](const RenderLayerModelObject& child) -> std::optional<AffineTransform> {
        if (!child.isTransformed() || !child.hasLayer())
            return std::nullopt;

        ASSERT(child.isSVGLayerAwareRenderer());
        ASSERT(!child.isRenderSVGRoot());

        auto transform = SVGLayerTransformComputation(child).computeAccumulatedTransform(m_renderer.ptr(), TransformState::TrackSVGCTMMatrix);
        return transform.isIdentity() ? std::nullopt : std::make_optional(WTFMove(transform));
    };

    auto uniteBoundingBoxRespectingValidity = [] (bool& boxValid, FloatRect& box, const RenderLayerModelObject& child, const FloatRect& childBoundingBox) {
        auto* containerChild = dynamicDowncast<RenderSVGContainer>(child);
        bool isBoundingBoxValid = !containerChild || containerChild->isObjectBoundingBoxValid();
        if (!isBoundingBoxValid)
            return;

        if (boxValid) {
            box.uniteEvenIfEmpty(childBoundingBox);
            return;
        }

        box = childBoundingBox;
        boxValid = true;
    };

    // 1. Let box be a rectangle initialized to (0, 0, 0, 0).
    FloatRect box;
    bool boxValid = false;

    // 2. Let parent be the container element if it is one, or the root of the "use" element's shadow tree otherwise.

    // 3. For each descendant graphics element child of parent:
    //    - If child is not rendered then continue to the next descendant graphics element.
    //    - Otherwise, set box to be the union of box and the result of invoking the algorithm to compute a bounding box with child
    //      as the element and the same values for space, fill, stroke, markers and clipped as the corresponding algorithm input values.
    for (CheckedRef child : childrenOfType<RenderLayerModelObject>(m_renderer)) {
        if (is<RenderSVGHiddenContainer>(child))
            continue;
        if (auto* shape = dynamicDowncast<RenderSVGShape>(child.get()); shape && shape->isRenderingDisabled())
            continue;

        SVGBoundingBoxComputation childBoundingBoxComputation(child);
        auto childBox = childBoundingBoxComputation.computeDecoratedBoundingBox(options);
        if (options.contains(DecorationOption::OverrideBoxWithFilterBoxForChildren) && is<RenderSVGContainer>(child)) {
            DecorationOptions optionsForChild = { DecorationOption::OverrideBoxWithFilterBox };
            if (options.contains(DecorationOption::CalculateFastRepaintRect))
                optionsForChild.add(DecorationOption::CalculateFastRepaintRect);
            childBoundingBoxComputation.adjustBoxForClippingAndEffects(optionsForChild, childBox);
        }

        if (!options.contains(DecorationOption::IgnoreTransformations)) {
            if (auto transform = transformationMatrixFromChild(child))
                childBox = transform->mapRect(childBox);
        }

        if (options == objectBoundingBoxDecoration)
            uniteBoundingBoxRespectingValidity(boxValid, box, child, childBox);
        else
            box.unite(childBox);
    }

    // 4. If clipped is true:
    //    - If the value of clip-path on element is not none, then set box to be the tightest rectangle in coordinate system space that
    //      contains the intersection of box and the clipping path.
    //    - If the overflow property applies to the element and does not have a value of visible, then set box to be the tightest rectangle
    //      in coordinate system space that contains the intersection of box and the element's overflow bounds.
    //    - If the clip property applies to the element and does not have a value of auto, then set box to be the tightest rectangle in coordinate
    //      system space that contains the intersection of box and the rectangle specified by clip. (TODO!)
    adjustBoxForClippingAndEffects(options, box, { DecorationOption::OverrideBoxWithFilterBox });

    if (options.contains(DecorationOption::IncludeClippers) && m_renderer->hasNonVisibleOverflow()) {
        ASSERT(m_renderer->hasLayer());

        ASSERT(is<RenderSVGViewportContainer>(m_renderer) || is<RenderSVGResourceMarker>(m_renderer) || is<RenderSVGRoot>(m_renderer));

        LayoutRect overflowClipRect;
        if (CheckedPtr svgModelObject = dynamicDowncast<RenderSVGModelObject>(m_renderer.get()))
            overflowClipRect = svgModelObject->overflowClipRect(svgModelObject->currentSVGLayoutLocation());
        else if (CheckedPtr box = dynamicDowncast<RenderBox>(m_renderer.get()))
            overflowClipRect = box->overflowClipRect(box->location());
        else {
            ASSERT_NOT_REACHED();
            return FloatRect();
        }

        box.intersect(overflowClipRect);
    }

    // 5. Return box.
    if (boundingBoxValid)
        *boundingBoxValid = boxValid;
    return box;
}

FloatRect SVGBoundingBoxComputation::handleForeignObjectOrImage(const SVGBoundingBoxComputation::DecorationOptions& options, bool* boundingBoxValid) const
{
    // 1. Let box be the tightest rectangle in coordinate space space that contains the positioning rectangle
    //    defined by the "x", "y", "width" and "height" geometric properties of the element.
    //
    // Note: The fill, stroke and markers input arguments to this algorithm do not affect the bounding box returned for these elements.
    auto box = m_renderer->objectBoundingBox();

    // 2. If clipped is true and the value of clip-path on element is not none, then set box to be the tightest rectangle
    //    in coordinate system space that contains the intersection of box and the clipping path.
    adjustBoxForClippingAndEffects(options, box);

    // 3. Return box.
    if (boundingBoxValid)
        *boundingBoxValid = true;
    return box;
}

void SVGBoundingBoxComputation::adjustBoxForClippingAndEffects(const SVGBoundingBoxComputation::DecorationOptions& options, FloatRect& box, const SVGBoundingBoxComputation::DecorationOptions& optionsToCheckForFilters) const
{
    bool includeFilter = false;
    for (auto filterOption : optionsToCheckForFilters) {
        if (options.contains(filterOption)) {
            includeFilter = true;
            break;
        }
    }

    if (includeFilter) {
        if (auto* referencedFilterRenderer = m_renderer->svgFilterResourceFromStyle()) {
            auto repaintRectCalculation = options.contains(DecorationOption::CalculateFastRepaintRect) ? RepaintRectCalculation::Fast : RepaintRectCalculation::Accurate;

            auto resourceRect = referencedFilterRenderer->resourceBoundingBox(m_renderer, repaintRectCalculation);
            if (box.isEmpty() && options.contains(DecorationOption::UseFilterBoxOnEmptyRect))
                box = resourceRect;
            else
                box.intersect(resourceRect);
        }
    }

    if (options.contains(DecorationOption::IncludeClippers)) {
        if (CheckedPtr referencedClipperRenderer = m_renderer->svgClipperResourceFromStyle()) {
            auto repaintRectCalculation = options.contains(DecorationOption::CalculateFastRepaintRect) ? RepaintRectCalculation::Fast : RepaintRectCalculation::Accurate;
            box.intersect(referencedClipperRenderer->resourceBoundingBox(m_renderer, repaintRectCalculation));
        }
    }

    if (options.contains(DecorationOption::IncludeMaskers)) {
        if (CheckedPtr referencedMaskerRenderer = m_renderer->svgMaskerResourceFromStyle()) {
            // When masks are nested, the inner masks do not affect the outer mask dimension, so skip the computation for inner masks.
            static unsigned s_maskBoundingBoxNestingLevel = 0;
            NestingLevelIncrementer incrementer { s_maskBoundingBoxNestingLevel };
            if (s_maskBoundingBoxNestingLevel < 2) {
                auto repaintRectCalculation = options.contains(DecorationOption::CalculateFastRepaintRect) ? RepaintRectCalculation::Fast : RepaintRectCalculation::Accurate;
                box.intersect(referencedMaskerRenderer->resourceBoundingBox(m_renderer, repaintRectCalculation));
            }
        }
    }

    if (options.contains(DecorationOption::IncludeOutline))
        box.inflate(m_renderer->outlineStyleForRepaint().outlineSize());
}

LayoutRect SVGBoundingBoxComputation::computeVisualOverflowRect(const RenderLayerModelObject& renderer)
{
    DecorationOptions options = repaintBoundingBoxDecoration | DecorationOption::IncludeOutline | DecorationOption::IgnoreTransformations;
    if (is<RenderSVGContainer>(renderer))
        options = options | DecorationOption::UseFilterBoxOnEmptyRect;
    auto repaintBoundingBoxWithoutTransformations = computeDecoratedBoundingBox(renderer, options);
    if (repaintBoundingBoxWithoutTransformations.isEmpty())
        return { };

    auto visualOverflowRect = enclosingLayoutRect(repaintBoundingBoxWithoutTransformations);
    visualOverflowRect.moveBy(-renderer.nominalSVGLayoutLocation());
    return visualOverflowRect;
}

}
