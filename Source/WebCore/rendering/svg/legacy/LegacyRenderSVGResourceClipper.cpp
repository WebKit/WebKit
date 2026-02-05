/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "LegacyRenderSVGResourceClipper.h"

#include "ContainerNodeInlines.h"
#include "ElementChildIteratorInlines.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "LegacyRenderSVGResourceClipperInlines.h"
#include "LegacyRenderSVGShape.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "RenderObjectDocument.h"
#include "RenderSVGText.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderView.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGUseElement.h"
#include "SVGVisitedRendererTracking.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyRenderSVGResourceClipper);

LegacyRenderSVGResourceClipper::LegacyRenderSVGResourceClipper(SVGClipPathElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(Type::LegacySVGResourceClipper, element, WTF::move(style))
{
}

LegacyRenderSVGResourceClipper::~LegacyRenderSVGResourceClipper() = default;

void LegacyRenderSVGResourceClipper::removeAllClientsFromCache()
{
    m_clipBoundaries.fill(FloatRect { });
    m_clipperMap.clear();
}

void LegacyRenderSVGResourceClipper::removeClientFromCache(RenderElement& client)
{
    m_clipperMap.remove(client);
}

auto LegacyRenderSVGResourceClipper::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode) -> OptionSet<ApplyResult>
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    auto repaintRect = renderer.repaintRectInLocalCoordinates();
    if (repaintRect.isEmpty())
        return { ApplyResult::ResourceApplied };

    auto boundingBox = renderer.objectBoundingBox();
    return applyClippingToContext(*context, renderer, boundingBox, boundingBox);
}

auto LegacyRenderSVGResourceClipper::pathOnlyClipping(GraphicsContext& context, const RenderElement& renderer, const AffineTransform& animatedLocalTransform, const FloatRect& objectBoundingBox, float usedZoom) -> OptionSet<ApplyResult>
{
    // If the current clip-path gets clipped itself, we have to fall back to masking.
    if (style().hasClipPath())
        return { };

    WindRule clipRule = WindRule::NonZero;
    Path clipPath;

    auto rendererRequiresMaskClipping = [&clipPath](auto& renderer) {
        // Only shapes or paths are supported for direct clipping. We need to fall back to masking for texts.
        if (is<RenderSVGText>(renderer))
            return true;
        auto& style = renderer.style();
        if (style.display() == DisplayType::None || style.usedVisibility() != Visibility::Visible)
            return false;
        // Current shape in clip-path gets clipped too. Fall back to masking.
        if (style.hasClipPath())
            return true;
        // Fall back to masking if there is more than one clipping path.
        if (!clipPath.isEmpty())
            return true;
        return false;
    };

    // If clip-path only contains one visible shape or path, we can use path-based clipping. Invisible
    // shapes don't affect the clipping and can be ignored. If clip-path contains more than one
    // visible shape, the additive clipping may not work, caused by the clipRule. EvenOdd
    // as well as NonZero can cause self-clipping of the elements.
    // See also http://www.w3.org/TR/SVG/painting.html#FillRuleProperty
    for (RefPtr childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        RefPtr graphicsElement = dynamicDowncast<SVGGraphicsElement>(*childNode);
        if (!graphicsElement)
            continue;
        CheckedPtr renderer = graphicsElement->renderer();
        if (!renderer)
            continue;

        // For <use> elements, check visibility of the target element and skip if no visible target.
        if (RefPtr useElement = dynamicDowncast<SVGUseElement>(graphicsElement.get())) {
            auto* clipChildRenderer = useElement->rendererClipChild();
            if (!clipChildRenderer)
                continue;
            if (rendererRequiresMaskClipping(*clipChildRenderer))
                return { };
        } else {
            if (rendererRequiresMaskClipping(*renderer))
                return { };
        }

        clipPath = graphicsElement->toClipPath();
        clipRule = renderer->style().clipRule();
    }

    // Only one visible shape/path was found. Directly continue clipping and transform the content to userspace if necessary.
    std::optional<AffineTransform> transform;
    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        transform.emplace();
        transform->translate(objectBoundingBox.location());
        transform->scale(objectBoundingBox.size());
    } else if (usedZoom != 1) {
        transform.emplace();
        transform->scale(usedZoom);
    }

    // Transform path by animatedLocalTransform.
    AffineTransform oldCTM = context.getCTM();
    context.concatCTM(animatedLocalTransform);
    if (transform)
        context.concatCTM(*transform);

    // The SVG specification wants us to clip everything, if clip-path doesn't have a child.
    if (clipPath.isEmpty())
        clipPath.addRect(FloatRect());

    OptionSet<ApplyResult> result = { ApplyResult::ResourceApplied };
    if (auto* shapeRenderer = dynamicDowncast<LegacyRenderSVGShape>(renderer); shapeRenderer && shapeRenderer->shapeType() == LegacyRenderSVGShape::ShapeType::Rectangle) {
        // When clipping a rect with a path, if we know the path is entirely inside the rect, we can skip a clip when filling the rect.
        auto clipBounds = clipPath.fastBoundingRect();
        if (transform)
            clipBounds = transform->mapRect(clipBounds);
        if (objectBoundingBox.contains(clipBounds))
            result.add(ApplyResult::ClipContainsRendererContent);
    }

    context.clipPath(clipPath, clipRule);
    context.setCTM(oldCTM);
    return result;
}

ClipperData::Inputs LegacyRenderSVGResourceClipper::computeInputs(const GraphicsContext& context, const RenderElement& renderer, const FloatRect& objectBoundingBox, const FloatRect& clippedContentBounds, float usedZoom)
{
    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);

    // Ignore 2D rotation, as it doesn't affect the size of the mask.
    FloatSize scale(absoluteTransform.xScale(), absoluteTransform.yScale());

    // Determine scale factor for the clipper. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(objectBoundingBox.size(), scale);

    return { objectBoundingBox, clippedContentBounds, scale, usedZoom, context.paintingDisabled() };
}

auto LegacyRenderSVGResourceClipper::applyClippingToContext(GraphicsContext& context, RenderElement& renderer, const FloatRect& objectBoundingBox, const FloatRect& clippedContentBounds, float usedZoom) -> OptionSet<ApplyResult>
{
    LOG_WITH_STREAM(SVG, stream << "LegacyRenderSVGResourceClipper " << this << " applyClippingToContext: renderer " << &renderer << " objectBoundingBox " << objectBoundingBox << " clippedContentBounds " << clippedContentBounds);

    AffineTransform animatedLocalTransform = clipPathElement().animatedLocalTransform();

    auto clipResult = pathOnlyClipping(context, renderer, animatedLocalTransform, objectBoundingBox, usedZoom);
    if (resourceWasApplied(clipResult)) {
        auto it = m_clipperMap.find(renderer);
        if (it != m_clipperMap.end())
            it->value->imageBuffer = nullptr;

        return clipResult;
    }

    auto& clipperData = *m_clipperMap.ensure(renderer, [&]() {
        return makeUnique<ClipperData>();
    }).iterator->value;

    if (clipperData.invalidate(computeInputs(context, renderer, objectBoundingBox, clippedContentBounds, usedZoom))) {
        // FIXME (149469): This image buffer should not be unconditionally unaccelerated. Making it match the context breaks nested clipping, though.
        clipperData.imageBuffer = context.createScaledImageBuffer(clippedContentBounds, clipperData.inputs.scale, DestinationColorSpace::SRGB(), RenderingMode::Unaccelerated); // FIXME
        if (!clipperData.imageBuffer)
            return { };

        GraphicsContext& maskContext = clipperData.imageBuffer->context();
        maskContext.concatCTM(animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
        LegacyRenderSVGResourceClipper* clipper;
        bool succeeded;
        if (resources && (clipper = resources->clipper())) {
            GraphicsContextStateSaver stateSaver(maskContext);

            if (!clipper->applyClippingToContext(maskContext, *this, objectBoundingBox, clippedContentBounds))
                return { };

            succeeded = drawContentIntoMaskImage(Ref { *clipperData.imageBuffer }, objectBoundingBox, usedZoom);
            // The context restore applies the clipping on non-CG platforms.
        } else
            succeeded = drawContentIntoMaskImage(Ref { *clipperData.imageBuffer }, objectBoundingBox, usedZoom);

        if (!succeeded)
            clipperData = { };
    }

    if (!clipperData.imageBuffer)
        return { };

    SVGRenderingContext::clipToImageBuffer(context, clippedContentBounds, clipperData.inputs.scale, clipperData.imageBuffer, true);
    return { ApplyResult::ResourceApplied };
}

bool LegacyRenderSVGResourceClipper::drawContentIntoMaskImage(ImageBuffer& maskImageBuffer, const FloatRect& objectBoundingBox, float usedZoom)
{
    GraphicsContext& maskContext = maskImageBuffer.context();

    AffineTransform maskContentTransformation;
    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskContentTransformation.translate(objectBoundingBox.location());
        maskContentTransformation.scale(objectBoundingBox.size());
        maskContext.concatCTM(maskContentTransformation);
    } else if (usedZoom != 1) {
        maskContentTransformation.scale(usedZoom);
        maskContext.concatCTM(maskContentTransformation);
    }

    // Switch to a paint behavior where all children of this <clipPath> will be rendered using special constraints:
    // - fill-opacity/stroke-opacity/opacity set to 1
    // - masker/filter not applied when rendering the children
    // - fill is set to the initial fill paint server (solid, black)
    // - stroke is set to the initial stroke paint server (none)
    auto oldBehavior = view().frameView().paintBehavior();
    view().frameView().setPaintBehavior(oldBehavior | PaintBehavior::RenderingSVGClipOrMask);

    // Draw all clipPath children into a global mask.
    for (Ref child : childrenOfType<SVGElement>(protectedClipPathElement())) {
        auto renderer = child->renderer();
        if (!renderer)
            continue;
        if (renderer->needsLayout()) {
            view().frameView().setPaintBehavior(oldBehavior);
            return false;
        }
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || (style.usedVisibility() != Visibility::Visible && !is<SVGUseElement>(child)))
            continue;

        WindRule newClipRule = style.clipRule();
        RefPtr useElement = dynamicDowncast<SVGUseElement>(child);
        if (useElement) {
            renderer = useElement->rendererClipChild();
            if (!renderer)
                continue;
            if (!useElement->hasAttributeWithoutSynchronization(SVGNames::clip_ruleAttr))
                newClipRule = renderer->style().clipRule();
        }

        // Only shapes, paths and texts are allowed for clipping.
        if (!renderer->isRenderOrLegacyRenderSVGShape() && !renderer->isRenderSVGText())
            continue;

        maskContext.setFillRule(newClipRule);

        // In the case of a <use> element, we obtained its renderere above, to retrieve its clipRule.
        // We have to pass the <use> renderer itself to renderSubtreeToContext() to apply it's x/y/transform/etc. values when rendering.
        // So if useElement is non-null, refetch the childNode->renderer(), as renderer got overridden above.
        SVGRenderingContext::renderSubtreeToContext(maskContext, useElement ? *child->renderer() : *renderer, maskContentTransformation);
    }

    view().frameView().setPaintBehavior(oldBehavior);
    return true;
}

void LegacyRenderSVGResourceClipper::calculateClipContentRepaintRect(RepaintRectCalculation repaintRectCalculation)
{
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    for (RefPtr childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        CheckedPtr renderer = dynamicDowncast<RenderElement>(childNode->renderer());
        if (!renderer || !childNode->isSVGElement())
            continue;
        if (!renderer->isRenderOrLegacyRenderSVGShape() && !renderer->isRenderSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || (style.usedVisibility() != Visibility::Visible && !childNode->hasTagName(SVGNames::useTag)))
            continue;

        // For <use> elements, check if the clipping target is visible.
        if (RefPtr useElement = dynamicDowncast<SVGUseElement>(childNode)) {
            if (!useElement->visibleTargetGraphicsElement())
                continue;
        }
        m_clipBoundaries[repaintRectCalculation].unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates(repaintRectCalculation)));
    }
}

bool LegacyRenderSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const FloatPoint& nodeAtPoint)
{
    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    FloatPoint point = nodeAtPoint;
    if (!SVGRenderSupport::pointInClippingArea(*this, point))
        return false;

    // The forward transform order is: OBB first, then local transform.
    // So the inverse order is: inverse local first, then inverse OBB.
    point = valueOrDefault(clipPathElement().animatedLocalTransform().inverse()).mapPoint(point);

    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        point = valueOrDefault(transform.inverse()).mapPoint(point);
    }

    for (RefPtr childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isRenderOrLegacyRenderSVGShape() && !renderer->isRenderSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;

        IntPoint hitPoint;
        HitTestResult result(hitPoint);
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::SVGClipContent, HitTestRequest::Type::DisallowUserAgentShadowContent };
        if (renderer->nodeAtFloatPoint(hitType, result, point, HitTestForeground))
            return true;
    }

    return false;
}

FloatRect LegacyRenderSVGResourceClipper::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation repaintRectCalculation)
{
    // Resource was not layouted yet. Give back the boundingBox of the object.
    if (selfNeedsLayout()) {
        m_clipperMap.ensure(object, [&]() { // For selfNeedsClientInvalidation().
            return makeUnique<ClipperData>();
        });
        return object.objectBoundingBox();
    }

    if (m_clipBoundaries[repaintRectCalculation].isEmpty())
        calculateClipContentRepaintRect(repaintRectCalculation);

    auto clipBoundaries = m_clipBoundaries[repaintRectCalculation];

    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        FloatRect objectBoundingBox = object.objectBoundingBox();
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        clipBoundaries = transform.mapRect(clipBoundaries);
    }

    return clipPathElement().animatedLocalTransform().mapRect(clipBoundaries);
}

}
