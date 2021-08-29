/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
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
#include "RenderSVGResourceClipper.h"

#include "ElementIterator.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "Logging.h"
#include "RenderSVGText.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SVGNames.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGUseElement.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceClipper);

RenderSVGResourceClipper::RenderSVGResourceClipper(SVGClipPathElement& element, RenderStyle&& style)
    : RenderSVGResourceContainer(element, WTFMove(style))
{
}

RenderSVGResourceClipper::~RenderSVGResourceClipper() = default;

void RenderSVGResourceClipper::removeAllClientsFromCache(bool markForInvalidation)
{
    m_clipBoundaries = { };
    m_clipper.clear();

    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceClipper::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    m_clipper.remove(&client);

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceClipper::applyResource(RenderElement& renderer, const RenderStyle&, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode)
{
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, !resourceMode);

    auto repaintRect = renderer.repaintRectInLocalCoordinates();
    if (repaintRect.isEmpty())
        return true;

    return applyClippingToContext(*context, renderer, renderer.objectBoundingBox());
}

bool RenderSVGResourceClipper::pathOnlyClipping(GraphicsContext& context, const AffineTransform& animatedLocalTransform, const FloatRect& objectBoundingBox, float effectiveZoom)
{
    // If the current clip-path gets clipped itself, we have to fall back to masking.
    if (style().clipPath())
        return false;
    WindRule clipRule = WindRule::NonZero;
    Path clipPath;

    // If clip-path only contains one visible shape or path, we can use path-based clipping. Invisible
    // shapes don't affect the clipping and can be ignored. If clip-path contains more than one
    // visible shape, the additive clipping may not work, caused by the clipRule. EvenOdd
    // as well as NonZero can cause self-clipping of the elements.
    // See also http://www.w3.org/TR/SVG/painting.html#FillRuleProperty
    for (Node* childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        auto* renderer = childNode->renderer();
        if (!renderer)
            continue;
        // Only shapes or paths are supported for direct clipping. We need to fall back to masking for texts.
        if (is<RenderSVGText>(renderer))
            return false;
        if (!is<SVGGraphicsElement>(*childNode))
            continue;
        auto& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
            continue;
        // Current shape in clip-path gets clipped too. Fall back to masking.
        if (style.clipPath())
            return false;
        // Fall back to masking if there is more than one clipping path.
        if (!clipPath.isEmpty())
            return false;
        clipPath = downcast<SVGGraphicsElement>(*childNode).toClipPath();
        clipRule = style.svgStyle().clipRule();
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

bool RenderSVGResourceClipper::applyClippingToContext(GraphicsContext& context, RenderElement& renderer, const FloatRect& objectBoundingBox, float effectiveZoom)
{
    ClipperData& clipperData = addRendererToClipper(renderer);
    
    LOG_WITH_STREAM(SVG, stream << "RenderSVGResourceClipper " << this << " applyClippingToContext: renderer " << &renderer << " objectBoundingBox " << objectBoundingBox << " (existing image buffer " << clipperData.imageBuffer.get() << ")");

    AffineTransform animatedLocalTransform = clipPathElement().animatedLocalTransform();

    if (!clipperData.imageBuffer && pathOnlyClipping(context, animatedLocalTransform, objectBoundingBox, effectiveZoom))
        return true;

    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(renderer);
    if (!clipperData.isValidForGeometry(objectBoundingBox, absoluteTransform)) {
        // FIXME (149469): This image buffer should not be unconditionally unaccelerated. Making it match the context breaks nested clipping, though.
        auto maskImage = SVGRenderingContext::createImageBuffer(objectBoundingBox, absoluteTransform, DestinationColorSpace::SRGB(), RenderingMode::Unaccelerated, &context);
        if (!maskImage)
            return false;

        clipperData = { WTFMove(maskImage), objectBoundingBox, absoluteTransform };

        GraphicsContext& maskContext = clipperData.imageBuffer->context();
        maskContext.concatCTM(animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
        RenderSVGResourceClipper* clipper;
        bool succeeded;
        if (resources && (clipper = resources->clipper())) {
            GraphicsContextStateSaver stateSaver(maskContext);

            if (!clipper->applyClippingToContext(maskContext, *this, objectBoundingBox))
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

    SVGRenderingContext::clipToImageBuffer(context, absoluteTransform, objectBoundingBox, clipperData.imageBuffer, true);
    return true;
}

bool RenderSVGResourceClipper::drawContentIntoMaskImage(ImageBuffer& maskImageBuffer, const FloatRect& objectBoundingBox, float effectiveZoom)
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
    view().frameView().setPaintBehavior(oldBehavior | PaintBehavior::RenderingSVGMask);

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
        bool isUseElement = child.hasTagName(SVGNames::useTag);
        if (isUseElement) {
            SVGUseElement& useElement = downcast<SVGUseElement>(child);
            renderer = useElement.rendererClipChild();
            if (!renderer)
                continue;
            if (!useElement.hasAttributeWithoutSynchronization(SVGNames::clip_ruleAttr))
                newClipRule = renderer->style().svgStyle().clipRule();
        }

        // Only shapes, paths and texts are allowed for clipping.
        if (!renderer->isSVGShape() && !renderer->isSVGText())
            continue;

        maskContext.setFillRule(newClipRule);

        // In the case of a <use> element, we obtained its renderere above, to retrieve its clipRule.
        // We have to pass the <use> renderer itself to renderSubtreeToContext() to apply it's x/y/transform/etc. values when rendering.
        // So if isUseElement is true, refetch the childNode->renderer(), as renderer got overridden above.
        SVGRenderingContext::renderSubtreeToContext(maskContext, isUseElement ? *child.renderer() : *renderer, maskContentTransformation);
    }

    view().frameView().setPaintBehavior(oldBehavior);
    return true;
}

void RenderSVGResourceClipper::calculateClipContentRepaintRect()
{
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    for (Node* childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isSVGShape() && !renderer->isSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;
        const RenderStyle& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
             continue;
        m_clipBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }
    m_clipBoundaries = clipPathElement().animatedLocalTransform().mapRect(m_clipBoundaries);
}

RenderSVGResourceClipper::ClipperData& RenderSVGResourceClipper::addRendererToClipper(const RenderObject& object)
{
    return m_clipper.add(&object, ClipperData()).iterator->value;
}

bool RenderSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const FloatPoint& nodeAtPoint)
{
    FloatPoint point = nodeAtPoint;
    if (!SVGRenderSupport::pointInClippingArea(*this, point))
        return false;

    SVGHitTestCycleDetectionScope hitTestScope(*this);

    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        point = transform.inverse().value_or(AffineTransform()).mapPoint(point);
    }

    point = clipPathElement().animatedLocalTransform().inverse().value_or(AffineTransform()).mapPoint(point);

    for (Node* childNode = clipPathElement().firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isSVGShape() && !renderer->isSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;

        IntPoint hitPoint;
        HitTestResult result(hitPoint);
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::SVGClipContent, HitTestRequest::Type::DisallowUserAgentShadowContent };
        if (renderer->nodeAtFloatPoint(hitType, result, point, HitTestForeground))
            return true;
    }

    return false;
}

FloatRect RenderSVGResourceClipper::resourceBoundingBox(const RenderObject& object)
{
    // Resource was not layouted yet. Give back the boundingBox of the object.
    if (selfNeedsLayout()) {
        addRendererToClipper(object); // For selfNeedsClientInvalidation().
        return object.objectBoundingBox();
    }
    
    if (m_clipBoundaries.isEmpty())
        calculateClipContentRepaintRect();

    if (clipPathElement().clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        FloatRect objectBoundingBox = object.objectBoundingBox();
        AffineTransform transform;
        transform.translate(objectBoundingBox.location());
        transform.scale(objectBoundingBox.size());
        return transform.mapRect(m_clipBoundaries);
    }

    return m_clipBoundaries;
}

}
