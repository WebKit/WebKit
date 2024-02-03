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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "ElementIterator.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "ReferencedSVGResources.h"
#include "RenderLayerInlines.h"
#include "RenderSVGResourceClipperInlines.h"
#include "RenderSVGText.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGRenderStyle.h"
#include "SVGUseElement.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceClipper);

RenderSVGResourceClipper::RenderSVGResourceClipper(SVGClipPathElement& element, RenderStyle&& style)
    : RenderSVGResourceContainer(Type::SVGResourceClipper, element, WTFMove(style))
{
    ASSERT(isRenderSVGResourceClipper());
}

RenderSVGResourceClipper::~RenderSVGResourceClipper() = default;

enum class ClippingMode {
    NoClipping,
    PathClipping,
    MaskClipping
};

static ClippingMode& currentClippingMode()
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

SVGGraphicsElement* RenderSVGResourceClipper::shouldApplyPathClipping() const
{
    if (currentClippingMode() == ClippingMode::MaskClipping)
        return nullptr;
    return clipPathElement().shouldApplyPathClipping();
}

void RenderSVGResourceClipper::applyPathClipping(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const FloatRect& objectBoundingBox, SVGGraphicsElement& graphicsElement)
{
    ASSERT(hasLayer());
    ASSERT(layer()->isSelfPaintingLayer());

    ASSERT(currentClippingMode() == ClippingMode::NoClipping || currentClippingMode() == ClippingMode::MaskClipping);
    SetForScope<ClippingMode> switchClippingMode(currentClippingMode(), ClippingMode::PathClipping);

    auto* clipRendererPtr = graphicsElement.renderer();
    ASSERT(clipRendererPtr);
    ASSERT(clipRendererPtr->hasLayer());
    auto& clipRenderer = downcast<RenderSVGModelObject>(*clipRendererPtr);

    AffineTransform clipPathTransform;
    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        clipPathTransform.translate(objectBoundingBox.location());
        clipPathTransform.scale(objectBoundingBox.size());
    } else if (!targetRenderer.isSVGLayerAwareRenderer()) {
        clipPathTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        clipPathTransform.scale(targetRenderer.style().effectiveZoom());
    }
    if (layer()->isTransformed())
        clipPathTransform.multiply(layer()->transform()->toAffineTransform());

    const auto& clipPath = clipRenderer.computeClipPath(clipPathTransform);
    auto windRule = clipRenderer.style().svgStyle().clipRule();

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
    ASSERT(targetRenderer.hasLayer());

    if (SVGHitTestCycleDetectionScope::isVisiting(*this))
        return;

    ASSERT(currentClippingMode() == ClippingMode::NoClipping || currentClippingMode() == ClippingMode::MaskClipping);
    SetForScope<ClippingMode> switchClippingMode(currentClippingMode(), ClippingMode::MaskClipping);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    if (auto* referencedClipperRenderer = svgClipperResourceFromStyle()) {
        // FIXME: Rename SVGHitTestCycleDetectionScope -> SVGResourceCycleDetectionScope
        SVGHitTestCycleDetectionScope clippingScope(*this);
        referencedClipperRenderer->applyMaskClipping(paintInfo, targetRenderer, objectBoundingBox);
    }

    AffineTransform contentTransform;

    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        contentTransform.scale(objectBoundingBox.width(), objectBoundingBox.height());
    } else if (!targetRenderer.isSVGLayerAwareRenderer()) {
        contentTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        contentTransform.scale(targetRenderer.style().effectiveZoom());
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
    auto& frameView = view().frameView();
    auto oldBehavior = frameView.paintBehavior();
    frameView.setPaintBehavior(oldBehavior | PaintBehavior::RenderingSVGClipOrMask);

    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;

        context.setCompositeOperation(CompositeOperator::DestinationIn);
        context.beginTransparencyLayer(1);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }

    layer()->paintSVGResourceLayer(context, contentTransform);

    if (pushTransparencyLayer)
        context.endTransparencyLayer();
    frameView.setPaintBehavior(oldBehavior);
}

bool RenderSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const LayoutPoint& nodeAtPoint)
{
    auto point = nodeAtPoint;
    if (!SVGRenderSupport::pointInClippingArea(*this, point))
        return false;

    SVGHitTestCycleDetectionScope hitTestScope(*this);

    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform applyTransform;
        applyTransform.translate(objectBoundingBox.location());
        applyTransform.scale(objectBoundingBox.size());
        point = LayoutPoint(applyTransform.inverse().value_or(AffineTransform()).mapPoint(point));
    }

    HitTestResult result(toLayoutPoint(point - flooredLayoutPoint(this->objectBoundingBox().minXMinYCorner())));
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::SVGClipContent, HitTestRequest::Type::DisallowUserAgentShadowContent };
    return layer()->hitTest(hitType, result);
}

FloatRect RenderSVGResourceClipper::resourceBoundingBox(const RenderObject& object, RepaintRectCalculation repaintRectCalculation)
{
    auto targetBoundingBox = object.objectBoundingBox();

    if (SVGHitTestCycleDetectionScope::isVisiting(*this))
        return targetBoundingBox;

    SVGHitTestCycleDetectionScope queryScope(*this);

    auto clipContentRepaintRect = clipPathElement().calculateClipContentRepaintRect(repaintRectCalculation);
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

void RenderSVGResourceClipper::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());
    applySVGTransform(transform, clipPathElement(), style, boundingBox, std::nullopt, std::nullopt, options);
}

bool RenderSVGResourceClipper::needsHasSVGTransformFlags() const
{
    return clipPathElement().hasTransformRelatedAttributes();
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
