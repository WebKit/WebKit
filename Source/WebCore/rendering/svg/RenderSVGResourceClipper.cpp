/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021, 2022, 2023, 2024 Igalia S.L.
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

#include "ContainerNodeInlines.h"
#include "ElementIterator.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "ReferencedSVGResources.h"
#include "RenderLayerInlines.h"
#include "RenderSVGResourceClipperInlines.h"
#include "RenderSVGShape.h"
#include "RenderSVGText.h"
#include "RenderView.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGUseElement.h"
#include "SVGVisitedRendererTracking.h"
#include "StyleComputedStyle.h"
#include "StyleTransformResolver.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGResourceClipper);

RenderSVGResourceClipper::RenderSVGResourceClipper(SVGClipPathElement& element, Style::ComputedStyle&& style)
    : RenderSVGResourceContainer(Type::SVGResourceClipper, element, WTF::move(style))
{
    ASSERT(isRenderSVGResourceClipper());
}

RenderSVGResourceClipper::~RenderSVGResourceClipper() = default;

enum class ClippingMode {
    NoClipping,
    PathClipping,
    MaskClipping
};

static ClippingMode& NODELETE currentClippingMode()
{
    static ClippingMode s_clippingMode { ClippingMode::NoClipping };
    return s_clippingMode;
}

static Path& sharedClipAllPath()
{
    static LazyNeverDestroyed<Path> clipAllPath;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        clipAllPath.construct();
        clipAllPath.get().addRect(FloatRect());
    });
    return clipAllPath.get();
}

RefPtr<SVGGraphicsElement> RenderSVGResourceClipper::shouldApplyPathClipping() const
{
    if (currentClippingMode() == ClippingMode::MaskClipping)
        return nullptr;
    // This walks the SVGClipPathElement's children to decide whether path-based clipping is viable.
    // The result only changes on structural or style changes in the clip-path subtree, but without
    // caching it would run per shape per frame in animations (in scenes like MotionMark and Suits
    // about half the shapes are clip-pathed, thousands per frame).
    if (!m_cachedShouldApplyPathClippingResult)
        m_cachedShouldApplyPathClippingResult = WeakPtr { protect(clipPathElement())->shouldApplyPathClipping() };
    return m_cachedShouldApplyPathClippingResult->get();
}

void RenderSVGResourceClipper::repaintAllClients() const
{
    m_cachedShouldApplyPathClippingResult = std::nullopt;
    RenderSVGResourceContainer::repaintAllClients();
}

void RenderSVGResourceClipper::applyPathClipping(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const FloatRect& objectBoundingBox, SVGGraphicsElement& graphicsElement)
{
    ASSERT(hasLayer());
    ASSERT(layer()->isSelfPaintingLayer());

    ASSERT(currentClippingMode() == ClippingMode::NoClipping || currentClippingMode() == ClippingMode::MaskClipping);
    SetForScope<ClippingMode> switchClippingMode(currentClippingMode(), ClippingMode::PathClipping);

    auto* clipRendererPtr = graphicsElement.renderer();
    ASSERT(clipRendererPtr);
    auto& clipRenderer = downcast<RenderSVGModelObject>(*clipRendererPtr);

    AffineTransform clipPathTransform;
    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        // objectBoundingBox.location() is already in the target's absolute user space, so it positions
        // the clip on its own. The caller does not translate the context for objectBoundingBox units
        // (only for userSpaceOnUse), so there is nothing to undo here.
        clipPathTransform.translate(objectBoundingBox.location());
        // <foreignObject> paints its content at layout location() while its objectBoundingBox top-left
        // sits at its x/y, so re-anchor the clip to the content origin by current minus nominal. Shapes,
        // images and text paint at the nominal origin, where objectBoundingBox.location() already
        // matches, so they must not get this term.
        // FIXME: Make <foreignObject> and text agree on content vs bounding-box origin so this
        // <foreignObject>-only adjustment can be removed.
        if (targetRenderer.isRenderSVGForeignObject()) {
            auto contentOriginAdjustment = targetRenderer.currentSVGLayoutLocation() - targetRenderer.nominalSVGLayoutLocation();
            clipPathTransform.translate(contentOriginAdjustment.width().toFloat(), contentOriginAdjustment.height().toFloat());
        }
        clipPathTransform.scale(objectBoundingBox.size());
    } else if (!targetRenderer.isSVGLayerAwareRenderer()) {
        clipPathTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        clipPathTransform.scale(targetRenderer.style().usedZoom());
    }
    if (layer()->isTransformed())
        clipPathTransform.multiply(layer()->transform()->toAffineTransform());

    clipRenderer.computeClipContentTransform(clipPathTransform);
    if (!m_cachedPathClip || m_cachedPathClipRenderer.get() != &clipRenderer) {
        m_cachedPathClip = clipRenderer.computeClipPathGeometry();
        m_cachedPathClipRenderer = clipRenderer;
    }
    const auto& clipPath = *m_cachedPathClip;
    auto windRule = clipRenderer.style().clipRule();

    if (auto* shape = dynamicDowncast<RenderSVGShape>(targetRenderer); shape && shape->shapeType() == RenderSVGShape::ShapeType::Rectangle) {
        // When clipping a rect with a path, if we know the path is entirely inside the rect, we can skip a clip when filling the rect.
        auto clipBounds = clipPathTransform.mapRect(clipPath.fastBoundingRect());
        shape->setFillRequiresClip(!objectBoundingBox.contains(clipBounds));
    }

    // The SVG specification wants us to clip everything, if clip-path doesn't have a child.
    if (clipPath.isEmpty())
        context.clipPath(sharedClipAllPath(), windRule);
    else {
        auto ctm = context.getCTM();
        context.concatCTM(clipPathTransform);
        context.clipPath(clipPath, windRule);
        context.setCTM(ctm);
    }
}

void RenderSVGResourceClipper::applyMaskClipping(PaintInfo& paintInfo, const RenderLayerModelObject& targetRenderer, const FloatRect& objectBoundingBox)
{
    ASSERT(hasLayer());
    ASSERT(layer()->isSelfPaintingLayer());

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    ASSERT(currentClippingMode() == ClippingMode::NoClipping || currentClippingMode() == ClippingMode::MaskClipping);
    SetForScope<ClippingMode> switchClippingMode(currentClippingMode(), ClippingMode::MaskClipping);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    if (CheckedPtr referencedClipperRenderer = svgClipperResourceFromStyle())
        referencedClipperRenderer->applyMaskClipping(paintInfo, targetRenderer, objectBoundingBox);

    AffineTransform contentTransform;

    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        contentTransform.scale(objectBoundingBox.width(), objectBoundingBox.height());
    } else if (!targetRenderer.isSVGLayerAwareRenderer()) {
        contentTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        contentTransform.scale(targetRenderer.style().usedZoom());
    }

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = targetRenderer.hasLayer() && targetRenderer.layer()->hasCompositedMask();
    bool flattenCompositingLayers = paintInfo.paintBehavior.contains(PaintBehavior::FlattenCompositingLayers);

    // Switch to a paint behavior where all children of the <clipPath> are rendered using special constraints:
    // - fill-opacity/stroke-opacity/opacity set to 1
    // - masker/filter not applied when rendering the children
    // - fill is set to the initial fill paint server (solid, black)
    // - stroke is set to the initial stroke paint server (none)
    Ref frameView = view().frameView();
    auto oldBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldBehavior | PaintBehavior::RenderingSVGClipOrMask);

    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;

        context.setCompositeOperation(CompositeOperator::DestinationIn);
        context.beginTransparencyLayer(1);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }

    protect(layer())->paintResourceLayerForSVG(context, contentTransform);

    if (pushTransparencyLayer)
        context.endTransparencyLayer();
    frameView->setPaintBehavior(oldBehavior);
}

bool RenderSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const LayoutPoint& nodeAtPoint)
{
    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    auto point = nodeAtPoint;

    // If this <clipPath> has its own clip-path, the original target must also fall inside the
    // nested clip region. objectBoundingBox units inside the nested clipPath resolve against
    // the original referencing element's bounding box (passed in here), not this clipper's OBB.
    if (CheckedPtr nestedClipper = svgClipperResourceFromStyle()) {
        if (!nestedClipper->hitTestClipContent(objectBoundingBox, nodeAtPoint))
            return false;
    }

    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform applyTransform;
        applyTransform.translate(objectBoundingBox.location());
        applyTransform.scale(objectBoundingBox.size());
        point = LayoutPoint(applyTransform.inverse().value_or(AffineTransform()).mapPoint(point));
    }

    // Iterate children directly and call nodeAtPoint() on each, rather than using
    // layer()->hitTest(). The layer hit test infrastructure does not work for children
    // inside <clipPath> because they are inside a RenderSVGHiddenContainer, where
    // fragment collection and ancestor offset computation produce incorrect results.
    // For transformed children (with or without layers), inverse-map the point through
    // the child's SVG transform before testing.
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::SVGClipContent, HitTestRequest::Type::DisallowUserAgentShadowContent };
    for (CheckedPtr child = lastChild(); child; child = child->previousSibling()) {
        auto* svgChild = dynamicDowncast<RenderSVGModelObject>(*child);
        if (!svgChild)
            continue;

        auto testPoint = point;
        if (svgChild->isTransformed()) {
            // Compute the child's SVG transform and inverse-map the point.
            // For non-layer children, localTransform() is already populated.
            // For layer children, localTransform() is not maintained, so compute it.
            AffineTransform childTransform;
            if (svgChild->hasLayer()) {
                TransformationMatrix tm;
                auto referenceBoxRect = svgChild->transformReferenceBoxRect(svgChild->style());
                svgChild->applyTransform(tm, svgChild->style(), referenceBoxRect, Style::TransformResolver::allTransformOperations);
                childTransform = tm.toAffineTransform();
            } else
                childTransform = svgChild->localTransform();

            auto inverseTransform = childTransform.inverse();
            if (!inverseTransform)
                continue;
            testPoint = LayoutPoint(inverseTransform->mapPoint(FloatPoint(point)));
        }

        HitTestLocation testHitTestLocation(testPoint);
        HitTestResult result(testPoint);
        auto accumulatedOffset = toLayoutPoint(toLayoutSize(svgChild->nominalSVGLayoutLocation()) - toLayoutSize(svgChild->currentSVGLayoutLocation()));
        if (child->nodeAtPoint(hitType, result, testHitTestLocation, accumulatedOffset, HitTestAction::Foreground))
            return true;
    }
    return false;
}

FloatRect RenderSVGResourceClipper::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation repaintRectCalculation)
{
    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    auto targetBoundingBox = object.objectBoundingBox();
    if (recursionTracking.isVisiting(*this))
        return targetBoundingBox;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    auto clipContentRepaintRect = protect(clipPathElement())->calculateClipContentRepaintRect(repaintRectCalculation);
    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform contentTransform;
        contentTransform.translate(targetBoundingBox.location());
        contentTransform.scale(targetBoundingBox.size());
        return contentTransform.mapRect(clipContentRepaintRect);
    }

    return clipContentRepaintRect;
}

void RenderSVGResourceClipper::updateFromStyle()
{
    updateHasSVGTransformFlags();
}

void RenderSVGResourceClipper::applyTransform(TransformationMatrix& transform, const Style::ComputedStyle& style, const FloatRect& boundingBox, OptionSet<Style::TransformResolverOption> options) const
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());
    applySVGTransform(transform, protect(clipPathElement()), style, boundingBox, std::nullopt, std::nullopt, options);
}

bool RenderSVGResourceClipper::needsHasSVGTransformFlags() const
{
    return protect(clipPathElement())->hasTransformRelatedAttributes();
}

void RenderSVGResourceClipper::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderSVGHiddenContainer::styleDidChange(diff, oldStyle);

    // Ensure that descendants with layers are rooted within our layer.
    if (hasLayer())
        layer()->setIsOpportunisticStackingContext(true);
}

void RenderSVGResourceClipper::clearCacheBeforeLayout()
{
    m_cachedPathClip = std::nullopt;
    m_cachedPathClipRenderer = nullptr;
}

}

