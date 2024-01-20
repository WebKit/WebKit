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

#include "ElementChildIteratorInlines.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "LegacyRenderSVGResourceClipperInlines.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "RenderSVGText.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGUseElement.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceClipper);

LegacyRenderSVGResourceClipper::LegacyRenderSVGResourceClipper(SVGClipPathElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(Type::LegacySVGResourceClipper, element, WTFMove(style))
{
}

LegacyRenderSVGResourceClipper::~LegacyRenderSVGResourceClipper() = default;

void LegacyRenderSVGResourceClipper::removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    m_clipBoundaries.fill(FloatRect { });
    m_clipperMap.clear();

    markAllClientsForInvalidationIfNeeded(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation, visitedRenderers);
}

void LegacyRenderSVGResourceClipper::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    m_clipperMap.remove(client);

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool LegacyRenderSVGResourceClipper::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    auto repaintRect = renderer.repaintRectInLocalCoordinates();
    if (repaintRect.isEmpty())
        return true;

    auto boundingBox = renderer.objectBoundingBox();
    return applyClippingToContext(*context, renderer, boundingBox, boundingBox);
}

bool LegacyRenderSVGResourceClipper::pathOnlyClipping(GraphicsContext& context, const AffineTransform& animatedLocalTransform, const FloatRect& objectBoundingBox, float effectiveZoom)
{
    // If the current clip-path gets clipped itself, we have to fall back to masking.
    if (style().clipPath())
        return false;
    WindRule clipRule = WindRule::NonZero;
    Path clipPath;

    auto rendererRequiresMaskClipping = [&clipPath](RenderObject& renderer) {
        // Only shapes or paths are supported for direct clipping. We need to fall back to masking for texts.
        if (is<RenderSVGText>(renderer))
            return true;
        auto& style = renderer.style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
            return false;
        // Current shape in clip-path gets clipped too. Fall back to masking.
        if (style.clipPath())
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
    for (Node* childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        auto* graphicsElement = dynamicDowncast<SVGGraphicsElement>(*childNode);
        if (!graphicsElement)
            continue;
        auto* renderer = graphicsElement->renderer();
        if (!renderer)
            continue;
        if (rendererRequiresMaskClipping(*renderer))
            return false;

        // For <use> elements, delegate the decision whether to use mask clipping or not to the referenced element.
        if (auto* useElement = dynamicDowncast<SVGUseElement>(graphicsElement)) {
            auto* clipChildRenderer = useElement->rendererClipChild();
            if (clipChildRenderer && rendererRequiresMaskClipping(*clipChildRenderer))
                return false;
        }

        clipPath = graphicsElement->toClipPath();
        clipRule = renderer->style().svgStyle().clipRule();
    }

    // Only one visible shape/path was found. Directly continue clipping and transform the content to userspace if necessary.
    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        clipPath.transform(transform);
    } else if (effectiveZoom != 1) {
        AffineTransform transform;
        transform.scale(effectiveZoom);
        clipPath.transform(transform);
    }

    // Transform path by animatedLocalTransform.
    clipPath.transform(animatedLocalTransform);

    // The SVG specification wants us to clip everything, if clip-path doesn't have a child.
    if (clipPath.isEmpty())
        clipPath.addRect(FloatRect());
    context.clipPath(clipPath, clipRule);
    return true;
}

ClipperData::Inputs LegacyRenderSVGResourceClipper::computeInputs(const GraphicsContext& context, const RenderElement& renderer, const FloatRect& objectBoundingBox, const FloatRect& clippedContentBounds, float effectiveZoom)
{
    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);

    // Ignore 2D rotation, as it doesn't affect the size of the mask.
    FloatSize scale(absoluteTransform.xScale(), absoluteTransform.yScale());

    // Determine scale factor for the clipper. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(objectBoundingBox.size(), scale);

    return { objectBoundingBox, clippedContentBounds, scale, effectiveZoom, context.paintingDisabled() };
}

bool LegacyRenderSVGResourceClipper::applyClippingToContext(GraphicsContext& context, RenderElement& renderer, const FloatRect& objectBoundingBox, const FloatRect& clippedContentBounds, float effectiveZoom)
{
    LOG_WITH_STREAM(SVG, stream << "LegacyRenderSVGResourceClipper " << this << " applyClippingToContext: renderer " << &renderer << " objectBoundingBox " << objectBoundingBox << " clippedContentBounds " << clippedContentBounds);

    AffineTransform animatedLocalTransform = clipPathElement().animatedLocalTransform();

    if (pathOnlyClipping(context, animatedLocalTransform, objectBoundingBox, effectiveZoom)) {
        auto it = m_clipperMap.find(renderer);
        if (it != m_clipperMap.end())
            it->value->imageBuffer = nullptr;

        return true;
    }

    auto& clipperData = *m_clipperMap.ensure(renderer, [&]() {
        return makeUnique<ClipperData>();
    }).iterator->value;

    if (clipperData.invalidate(computeInputs(context, renderer, objectBoundingBox, clippedContentBounds, effectiveZoom))) {
        // FIXME (149469): This image buffer should not be unconditionally unaccelerated. Making it match the context breaks nested clipping, though.
        clipperData.imageBuffer = context.createScaledImageBuffer(clippedContentBounds, clipperData.inputs.scale, DestinationColorSpace::SRGB(), RenderingMode::Unaccelerated); // FIXME
        if (!clipperData.imageBuffer)
            return false;

        GraphicsContext& maskContext = clipperData.imageBuffer->context();
        maskContext.concatCTM(animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
        LegacyRenderSVGResourceClipper* clipper;
        bool succeeded;
        if (resources && (clipper = resources->clipper())) {
            GraphicsContextStateSaver stateSaver(maskContext);

            if (!clipper->applyClippingToContext(maskContext, *this, objectBoundingBox, clippedContentBounds))
                return false;

            succeeded = drawContentIntoMaskImage(*clipperData.imageBuffer, objectBoundingBox, effectiveZoom);
            // The context restore applies the clipping on non-CG platforms.
        } else
            succeeded = drawContentIntoMaskImage(*clipperData.imageBuffer, objectBoundingBox, effectiveZoom);

        if (!succeeded)
            clipperData = { };
    }

    if (!clipperData.imageBuffer)
        return false;

    SVGRenderingContext::clipToImageBuffer(context, clippedContentBounds, clipperData.inputs.scale, clipperData.imageBuffer, true);
    return true;
}

bool LegacyRenderSVGResourceClipper::drawContentIntoMaskImage(ImageBuffer& maskImageBuffer, const FloatRect& objectBoundingBox, float effectiveZoom)
{
    GraphicsContext& maskContext = maskImageBuffer.context();

    AffineTransform maskContentTransformation;
    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskContentTransformation.translate(objectBoundingBox.location());
        maskContentTransformation.scale(objectBoundingBox.size());
        maskContext.concatCTM(maskContentTransformation);
    } else if (effectiveZoom != 1) {
        maskContentTransformation.scale(effectiveZoom);
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
    for (auto& child : childrenOfType<SVGElement>(clipPathElement())) {
        auto renderer = child.renderer();
        if (!renderer)
            continue;
        if (renderer->needsLayout()) {
            view().frameView().setPaintBehavior(oldBehavior);
            return false;
        }
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
            continue;

        WindRule newClipRule = style.svgStyle().clipRule();
        RefPtr useElement = dynamicDowncast<SVGUseElement>(child);
        if (useElement) {
            renderer = useElement->rendererClipChild();
            if (!renderer)
                continue;
            if (!useElement->hasAttributeWithoutSynchronization(SVGNames::clip_ruleAttr))
                newClipRule = renderer->style().svgStyle().clipRule();
        }

        // Only shapes, paths and texts are allowed for clipping.
        if (!renderer->isRenderOrLegacyRenderSVGShape() && !renderer->isRenderSVGText())
            continue;

        maskContext.setFillRule(newClipRule);

        // In the case of a <use> element, we obtained its renderere above, to retrieve its clipRule.
        // We have to pass the <use> renderer itself to renderSubtreeToContext() to apply it's x/y/transform/etc. values when rendering.
        // So if useElement is non-null, refetch the childNode->renderer(), as renderer got overridden above.
        SVGRenderingContext::renderSubtreeToContext(maskContext, useElement ? *child.renderer() : *renderer, maskContentTransformation);
    }

    view().frameView().setPaintBehavior(oldBehavior);
    return true;
}

void LegacyRenderSVGResourceClipper::calculateClipContentRepaintRect(RepaintRectCalculation repaintRectCalculation)
{
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    for (Node* childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isRenderOrLegacyRenderSVGShape() && !renderer->isRenderSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
             continue;
        m_clipBoundaries[repaintRectCalculation].unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates(repaintRectCalculation)));
    }
    m_clipBoundaries[repaintRectCalculation] = clipPathElement().animatedLocalTransform().mapRect(m_clipBoundaries[repaintRectCalculation]);
}

bool LegacyRenderSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const FloatPoint& nodeAtPoint)
{
    FloatPoint point = nodeAtPoint;
    if (!SVGRenderSupport::pointInClippingArea(*this, point))
        return false;

    SVGHitTestCycleDetectionScope hitTestScope(*this);

    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        point = valueOrDefault(transform.inverse()).mapPoint(point);
    }

    point = valueOrDefault(clipPathElement().animatedLocalTransform().inverse()).mapPoint(point);

    for (Node* childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
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

    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        FloatRect objectBoundingBox = object.objectBoundingBox();
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        return transform.mapRect(m_clipBoundaries[repaintRectCalculation]);
    }

    return m_clipBoundaries[repaintRectCalculation];
}

}
