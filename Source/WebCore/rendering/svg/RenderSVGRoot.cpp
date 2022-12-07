/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGRoot.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "Page.h"
#include "RenderChildIterator.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGText.h"
#include "RenderSVGViewportContainer.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGLayerTransformUpdater.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGSVGElement.h"
#include "SVGViewSpec.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGRoot);

RenderSVGRoot::RenderSVGRoot(SVGSVGElement& element, RenderStyle&& style)
    : RenderReplaced(element, WTFMove(style))
{
}

RenderSVGRoot::~RenderSVGRoot() = default;

SVGSVGElement& RenderSVGRoot::svgSVGElement() const
{
    return downcast<SVGSVGElement>(nodeForNonAnonymous());
}

RenderSVGViewportContainer* RenderSVGRoot::viewportContainer() const
{
    auto* child = firstChild();
    if (!child || !child->isAnonymous())
        return nullptr;
    return dynamicDowncast<RenderSVGViewportContainer>(child);
}

bool RenderSVGRoot::hasIntrinsicAspectRatio() const
{
    return computeIntrinsicAspectRatio();
}

void RenderSVGRoot::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, FloatSize& intrinsicRatio) const
{
    ASSERT(!shouldApplySizeContainment());

    // Spec: http://www.w3.org/TR/SVG/coords.html#IntrinsicSizing
    // SVG needs to specify how to calculate some intrinsic sizing properties to enable inclusion within other languages.

    // The intrinsic aspect ratio of the viewport of SVG content is necessary for example, when including SVG from an ‘object’
    // element in HTML styled with CSS. It is possible (indeed, common) for an SVG graphic to have an intrinsic aspect ratio but
    // not to have an intrinsic width or height. The intrinsic aspect ratio must be calculated based upon the following rules:
    // - The aspect ratio is calculated by dividing a width by a height.
    // - If the ‘width’ and ‘height’ of the rootmost ‘svg’ element are both specified with unit identifiers (in, mm, cm, pt, pc,
    //   px, em, ex) or in user units, then the aspect ratio is calculated from the ‘width’ and ‘height’ attributes after
    //   resolving both values to user units.
    intrinsicSize.setWidth(floatValueForLength(svgSVGElement().intrinsicWidth(), 0));
    intrinsicSize.setHeight(floatValueForLength(svgSVGElement().intrinsicHeight(), 0));

    if (style().aspectRatioType() == AspectRatioType::Ratio) {
        intrinsicRatio = FloatSize::narrowPrecision(style().aspectRatioLogicalWidth(), style().aspectRatioLogicalHeight());
        return;
    }

    std::optional<LayoutSize> intrinsicRatioValue;
    if (!intrinsicSize.isEmpty())
        intrinsicRatioValue = { intrinsicSize.width(), intrinsicSize.height() }; 
    else {
        // - If either/both of the ‘width’ and ‘height’ of the rootmost ‘svg’ element are in percentage units (or omitted), the
        //   aspect ratio is calculated from the width and height values of the ‘viewBox’ specified for the current SVG document
        //   fragment. If the ‘viewBox’ is not correctly specified, or set to 'none', the intrinsic aspect ratio cannot be
        //   calculated and is considered unspecified.
        FloatSize viewBoxSize = svgSVGElement().viewBox().size();
        if (!viewBoxSize.isEmpty()) {
            // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
            intrinsicRatioValue = { viewBoxSize.width(), viewBoxSize.height() };
        }
    }

    if (intrinsicRatioValue)
        intrinsicRatio = *intrinsicRatioValue;
    else if (style().aspectRatioType() == AspectRatioType::AutoAndRatio)
        intrinsicRatio = FloatSize::narrowPrecision(style().aspectRatioLogicalWidth(), style().aspectRatioLogicalHeight());
}

bool RenderSVGRoot::isEmbeddedThroughSVGImage() const
{
    return isInSVGImage(&svgSVGElement());
}

bool RenderSVGRoot::isEmbeddedThroughFrameContainingSVGDocument() const
{
    // If our frame has an owner renderer, we're embedded through eg. object/embed/iframe,
    // but we only negotiate if we're in an SVG document inside object/embed, not iframe.
    if (!frame().ownerRenderer() || !frame().ownerRenderer()->isEmbeddedObject())
        return false;
    return frame().document()->isSVGDocument();
}

LayoutUnit RenderSVGRoot::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.width();

    if (isEmbeddedThroughFrameContainingSVGDocument())
        return containingBlock()->availableLogicalWidth();

    // Standalone SVG / SVG embedded via SVGImage (background-image/border-image/etc) / Inline SVG.
    auto result = RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
    if (svgSVGElement().hasIntrinsicWidth())
        return result;

    // Percentage units are not scaled, Length(100, %) resolves to 100% of the unzoomed RenderView content size.
    // However for SVGs purposes we need to always include zoom in the RenderSVGRoot boundaries.
    result *= style().effectiveZoom();
    return result;
}

LayoutUnit RenderSVGRoot::computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth) const
{
    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.height();

    if (isEmbeddedThroughFrameContainingSVGDocument())
        return containingBlock()->availableLogicalHeight(IncludeMarginBorderPadding);

    // Standalone SVG / SVG embedded via SVGImage (background-image/border-image/etc) / Inline SVG.
    auto result = RenderReplaced::computeReplacedLogicalHeight(estimatedUsedWidth);
    if (svgSVGElement().hasIntrinsicHeight())
        return result;

    // Percentage units are not scaled, Length(100, %) resolves to 100% of the unzoomed RenderView content size.
    // However for SVGs purposes we need to always include zoom in the RenderSVGRoot boundaries.
    result *= style().effectiveZoom();
    return result;
}

bool RenderSVGRoot::updateLayoutSizeIfNeeded()
{
    auto previousSize = size();
    updateLogicalWidth();
    updateLogicalHeight();
    return selfNeedsLayout() || previousSize != size();
}

void RenderSVGRoot::layout()
{
    SetForScope change(m_inLayout, true);
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    m_resourcesNeedingToInvalidateClients.clear();

    // Arbitrary affine transforms are incompatible with RenderLayoutState.
    LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    // Update layer transform before laying out children (SVG needs access to the transform matrices during layout for on-screen text font-size calculations).
    // Eventually re-update if the transform reference box, relevant for transform-origin, has changed during layout.
    //
    // FIXME: LBSE should not repeat the same mistake -- remove the on-screen text font-size hacks that predate the modern solutions to this.
    {
        ASSERT(!m_isLayoutSizeChanged);
        SetForScope trackLayoutSizeChanges(m_isLayoutSizeChanged, updateLayoutSizeIfNeeded());

        ASSERT(!m_didTransformToRootUpdate);
        SVGLayerTransformUpdater transformUpdater(*this);
        SetForScope trackTransformChanges(m_didTransformToRootUpdate, transformUpdater.layerTransformChanged());
        layoutChildren();
    }

    clearOverflow();
    if (!shouldApplyViewportClip()) {
        addVisualOverflow(visualOverflowRectEquivalent());
        addVisualEffectOverflow();
    }

    invalidateBackgroundObscurationStatus();

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGRoot::layoutChildren()
{
    SVGContainerLayout containerLayout(*this);
    containerLayout.layoutChildren(selfNeedsLayout() || SVGRenderSupport::filtersForceContainerLayout(*this));

    SVGBoundingBoxComputation boundingBoxComputation(*this);
    m_objectBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::objectBoundingBoxDecoration);

    constexpr auto objectBoundingBoxDecorationWithoutTransformations = SVGBoundingBoxComputation::objectBoundingBoxDecoration | SVGBoundingBoxComputation::DecorationOption::IgnoreTransformations;
    m_objectBoundingBoxWithoutTransformations = boundingBoxComputation.computeDecoratedBoundingBox(objectBoundingBoxDecorationWithoutTransformations);

    m_strokeBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::strokeBoundingBoxDecoration);
    containerLayout.positionChildrenRelativeToContainer();

    if (!m_resourcesNeedingToInvalidateClients.isEmpty()) {
        // Invalidate resource clients, which may mark some nodes for layout.
        for (auto& resource :  m_resourcesNeedingToInvalidateClients) {
            resource->removeAllClientsFromCache();
            SVGResourcesCache::clientStyleChanged(*resource, StyleDifference::Layout, nullptr, resource->style());
        }

        SetForScope clearLayoutSizeChanged(m_isLayoutSizeChanged, false);
        containerLayout.layoutChildren(false);
    }
}

bool RenderSVGRoot::shouldApplyViewportClip() const
{
    // the outermost svg is clipped if auto, and svg document roots are always clipped
    // When the svg is stand-alone (isDocumentElement() == true) the viewport clipping should always
    // be applied, noting that the window scrollbars should be hidden if overflow=hidden.
    return isNonVisibleOverflow(effectiveOverflowX()) || style().overflowX() == Overflow::Auto || this->isDocumentElementRenderer();
}

// FIXME: Basically a copy of RenderBlock::paint() - ideally one would share this code.
// However with LFC on the horizon that investment is useless, we should concentrate
// on LFC/SVG integration once the LBSE is upstreamed.
void RenderSVGRoot::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Don't paint, if the context explicitly disabled it.
    if (paintInfo.context().paintingDisabled() && !paintInfo.context().detectingContentfulPaint())
        return;

    // An empty viewport disables rendering.
    if (borderBoxRect().isEmpty())
        return;

    auto adjustedPaintOffset = paintOffset + location();

    // Check if we need to do anything at all.
    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (!isDocumentElementRenderer() && !paintInfo.paintBehavior.contains(PaintBehavior::CompositedOverflowScrollContent)) {
        auto overflowBox = visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, paintInfo.phase, adjustedPaintOffset);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if ((paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::ChildBlockBackground) && hasNonVisibleOverflow() && layer() && layer()->scrollableArea()
        && style().visibility() == Visibility::Visible && paintInfo.shouldPaintWithinRoot(*this) && !paintInfo.paintRootBackgroundOnly())
        layer()->scrollableArea()->paintOverflowControls(paintInfo.context(), roundedIntPoint(adjustedPaintOffset), snappedIntRect(paintInfo.rect));
}

void RenderSVGRoot::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if ((paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::ChildBlockBackground) && style().visibility() == Visibility::Visible) {
        if (hasVisibleBoxDecorations())
            paintBoxDecorations(paintInfo, paintOffset);
    }

    auto adjustedPaintOffset = paintOffset + location();
    if (paintInfo.phase == PaintPhase::Mask && style().visibility() == Visibility::Visible) {
        // FIXME: [LBSE] Upstream SVGRenderSupport changes
        // SVGRenderSupport::paintSVGMask(*this, paintInfo, adjustedPaintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhase::ClippingMask && style().visibility() == Visibility::Visible) {
        // FIXME: [LBSE] Upstream SVGRenderSupport changes
        // SVGRenderSupport::paintSVGClippingMask(*this, paintInfo);
        return;
    }

    if (paintInfo.paintRootBackgroundOnly())
        return;

    GraphicsContext& context = paintInfo.context();
    if (context.detectingContentfulPaint()) {
        for (auto& current : childrenOfType<RenderObject>(*this)) {
            if (!current.isSVGHiddenContainer()) {
                context.setContentfulPaintDetected();
                return;
            }
        }
        return;
    }

    // Don't paint if we don't have kids, except if we have filters we should paint those.
    if (!firstChild()) {
        auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
        if (!resources || !resources->filter()) {
            if (paintInfo.phase == PaintPhase::Foreground)
                page().addRelevantUnpaintedObject(this, visualOverflowRect());
            return;
        }
        return;
    }

    if (paintInfo.phase == PaintPhase::BlockBackground)
        return;

    auto scrolledOffset = paintOffset;
    scrolledOffset.moveBy(-scrollPosition());

    if (paintInfo.phase != PaintPhase::SelfOutline)
        paintContents(paintInfo, scrolledOffset);

    if ((paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) && hasOutline() && style().visibility() == Visibility::Visible)
        paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, size()));
}

void RenderSVGRoot::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Style is non-final if the element has a pending stylesheet before it. We end up with renderers with such styles if a script
    // forces renderer construction by querying something layout dependent.
    // Avoid FOUC by not painting. Switching to final style triggers repaint.
    if (style().isNotFinal())
        return;

    // We don't paint our own background, but we do let the kids paint their backgrounds.
    auto paintInfoForChild(paintInfo);
    if (paintInfo.phase == PaintPhase::ChildOutlines)
        paintInfoForChild.phase = PaintPhase::Outline;
    else if (paintInfo.phase == PaintPhase::ChildBlockBackgrounds)
        paintInfoForChild.phase = PaintPhase::ChildBlockBackground;

    paintInfoForChild.updateSubtreePaintRootForChildren(this);
    for (auto& child : childrenOfType<RenderElement>(*this)) {
        if (!child.hasSelfPaintingLayer())
            child.paint(paintInfoForChild, paintOffset);
    }
}

void RenderSVGRoot::willBeDestroyed()
{
    RenderBlock::removePercentHeightDescendant(const_cast<RenderSVGRoot&>(*this));

    SVGResourcesCache::clientDestroyed(*this);
    RenderReplaced::willBeDestroyed();
}

void RenderSVGRoot::insertedIntoTree(IsInternalMove isInternalMove)
{
    RenderReplaced::insertedIntoTree(isInternalMove);
    SVGResourcesCache::clientWasAddedToTree(*this);
}

void RenderSVGRoot::willBeRemovedFromTree(IsInternalMove isInternalMove)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(*this);
    RenderReplaced::willBeRemovedFromTree(isInternalMove);
}

void RenderSVGRoot::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(*this, diff, oldStyle, style());
}

bool RenderSVGRoot::paintingAffectedByExternalOffset() const
{
    // Standalone SVGs have no parent above the outermost <svg> that could affect the positioning.
    if (isDocumentElementRenderer())
        return false;

    // SVGs embedded via 'SVGImage' paint at (0, 0) by construction.
    if (isEmbeddedThroughSVGImage())
        return false;

    // <object> / <embed> might receive a non-zero paintOffset.
    if (isEmbeddedThroughFrameContainingSVGDocument())
        return true;

    // Inline SVG. A non-SVG ancestor might induce a non-zero paintOffset.
    if (auto* parentNode = svgSVGElement().parentNode())
        return !is<SVGElement>(parentNode);

    ASSERT_NOT_REACHED();
    return false;
}

bool RenderSVGRoot::needsHasSVGTransformFlags() const
{
    // Only mark us as transformed if really needed. Whenver a non-zero paintOffset could reach
    // RenderSVGRoot from an ancestor, the pixel snapping logic needs to be applied. Since the rest
    // of the SVG subtree doesn't know anything about subpixel offsets, we'll have to stop use/set
    // 'adjustedSubpixelOffset' starting at the RenderSVGRoot boundary. This mostly affects inline
    // SVG documents and SVGs embedded via <object> / <embed>.
    return svgSVGElement().hasTransformRelatedAttributes() || paintingAffectedByExternalOffset();
}

void RenderSVGRoot::updateFromStyle()
{
    RenderReplaced::updateFromStyle();
    updateHasSVGTransformFlags();

    // Additionally update style of the anonymous RenderSVGViewportContainer,
    // which handles zoom / pan / viewBox transformations.
    if (auto* viewportContainer = this->viewportContainer())
        viewportContainer->updateFromStyle();

    if (shouldApplyViewportClip())
        setHasNonVisibleOverflow();
}

void RenderSVGRoot::updateLayerTransform()
{
    RenderReplaced::updateLayerTransform();

    if (!hasLayer())
        return;

    // An empty viewBox disables the rendering -- dirty the visible descendant status!
    if (svgSVGElement().hasAttribute(SVGNames::viewBoxAttr) && svgSVGElement().hasEmptyViewBox())
        layer()->dirtyVisibleContentStatus();
}

LayoutRect RenderSVGRoot::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    return computeRect(borderBoxRect(), repaintContainer, context);
}

bool RenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    auto adjustedLocation = accumulatedOffset + location();

    ASSERT(SVGHitTestCycleDetectionScope::isEmpty());

    auto visualOverflowRect = this->visualOverflowRect();
    visualOverflowRect.moveBy(adjustedLocation);

    // Test SVG content if the point is in our content box or it is inside the visualOverflowRect and the overflow is visible.
    if (contentBoxRect().contains(adjustedLocation) || (!shouldApplyViewportClip() && locationInContainer.intersects(visualOverflowRect))) {
        // Check kids first.
        for (auto* child = lastChild(); child; child = child->previousSibling()) {
            if (!child->hasLayer() && child->nodeAtPoint(request, result, locationInContainer, adjustedLocation, hitTestAction)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
                ASSERT(SVGHitTestCycleDetectionScope::isEmpty());
                return true;
            }
        }
    }

    // If we didn't early exit above, we've just hit the container <svg> element. Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) && visibleToHitTesting(request)) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(adjustedLocation)));
            if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
                return true;
        }
    }

    ASSERT(SVGHitTestCycleDetectionScope::isEmpty());

    return false;
}

bool RenderSVGRoot::hasRelativeDimensions() const
{
    return svgSVGElement().intrinsicHeight().isPercentOrCalculated() || svgSVGElement().intrinsicWidth().isPercentOrCalculated();
}

void RenderSVGRoot::addResourceForClientInvalidation(RenderSVGResourceContainer* resource)
{
    UNUSED_PARAM(resource);
    /* FIXME: [LBSE] Rename findTreeRootObject -> findLegacyTreeRootObject, and re-add findTreeRootObject for RenderSVGRoot
    if (auto* svgRoot = SVGRenderSupport::findTreeRootObject(*resource))
        svgRoot->m_resourcesNeedingToInvalidateClients.add(resource);
    */
}

FloatSize RenderSVGRoot::computeViewportSize() const
{
    FloatSize result = contentBoxRect().size();
    result.setWidth(result.width() + verticalScrollbarWidth());
    result.setHeight(result.height() + horizontalScrollbarHeight());
    return result;
}

void RenderSVGRoot::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());

    if (repaintContainer == this)
        return;

    bool containerSkipped;
    auto* container = this->container(repaintContainer, containerSkipped);
    if (!container)
        return;

    bool isFixedPos = isFixedPositioned();
    // If this box has a transform, it acts as a fixed position container for fixed descendants,
    // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
    if (isFixedPos)
        mode.add(IsFixed);
    else if (mode.contains(IsFixed) && canContainFixedPositionObjects())
        mode.remove(IsFixed);

    if (wasFixed)
        *wasFixed = mode.contains(IsFixed);

    auto containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    bool preserve3D = mode & UseTransforms && participatesInPreserve3D(container);
    if (mode & UseTransforms && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);

        // For getCTM() computations we have to stay within the SVG subtree. However when the outermost <svg>
        // is transformed itself, we need to call mapLocalToContainer() at least up to the parent of the
        // outermost <svg>. That will also include the offset within the container, due to CSS positioning,
        // which shall not be included in getCTM() (unlike getScreenCTM()) -- fix that.
        if (transformState.transformMatrixTracking() == TransformState::TrackSVGCTMMatrix) {
            auto offset = toLayoutSize(contentBoxLocation() + containerOffset);
            t.translateRight(-offset.width(), -offset.height());
        }

        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else {
        if (transformState.transformMatrixTracking() == TransformState::TrackSVGCTMMatrix)
            containerOffset -= toLayoutSize(contentBoxLocation());

        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    }

    if (containerSkipped) {
        // There can't be a transform between repaintContainer and container, because transforms create containers, so it should be safe
        // to just subtract the delta between the repaintContainer and container.
        auto containerOffset = repaintContainer->offsetFromAncestorContainer(*container);
        transformState.move(-containerOffset.width(), -containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
        return;
    }

    mode.remove(ApplyContainerFlip);
    if (transformState.transformMatrixTracking() == TransformState::DoNotTrackTransformMatrix) {
        container->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
        return;
    }

    if (transformState.transformMatrixTracking() == TransformState::TrackSVGCTMMatrix)
        return;

    // The CSS lengths/numbers above the SVG fragment (e.g. in HTML) do not adhere to SVG zoom rules.
    // All length information (e.g. top/width/...) include the scaling factor. In SVG no lengths are
    // scaled but a global scaling operation is included in the transform state.
    // For getCTM/getScreenCTM computations the result must be independent of the page zoom factor.
    // To compute these matrices within a non-SVG context (e.g. SVG embedded in HTML -- inline SVG)
    // the scaling needs to be removed from the CSS transform state.
    TransformState transformStateAboveSVGFragment(settings().css3DTransformInteroperabilityEnabled(), transformState.direction(), transformState.mappedPoint());
    transformStateAboveSVGFragment.setTransformMatrixTracking(transformState.transformMatrixTracking());
    container->mapLocalToContainer(repaintContainer, transformStateAboveSVGFragment, mode, wasFixed);

    auto scale = 1.0 / style().effectiveZoom();
    if (auto transformAboveSVGFragment = transformStateAboveSVGFragment.releaseTrackedTransform()) {
        FloatPoint location(transformAboveSVGFragment->e(), transformAboveSVGFragment->f());
        location.scale(scale);

        auto unscaledTransform = TransformationMatrix().scale(scale) * *transformAboveSVGFragment;
        unscaledTransform.setE(location.x());
        unscaledTransform.setF(location.y());
        transformState.applyTransform(unscaledTransform, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    }

    // Respect scroll offset, after mapping to container coordinates.
    if (RefPtr<FrameView> view = document().view()) {
        LayoutPoint scrollPosition = view->scrollPosition();
        scrollPosition.scale(scale);
        transformState.move(-toLayoutSize(scrollPosition));
    }
}

LayoutRect RenderSVGRoot::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy, PaintPhase) const
{
    // SVG2: For those elements to which the overflow property can apply. If the overflow property has the value hidden or scroll, a clip, the exact size of the SVG viewport is applied.
    // Unlike RenderBox, RenderSVGRoot explicitely includes the padding in the overflow clip rect. In SVG the padding applied to the outermost <svg>
    // shrinks the area available for child renderers to paint into -- reflect this by shrinking the overflow clip rect itself.
    auto clipRect = borderBoxRectInFragment(fragment);
    clipRect.setLocation(location + clipRect.location() + toLayoutSize(contentBoxLocation()));
    clipRect.setSize(clipRect.size() - LayoutSize(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent()));
    return clipRect;
}

void RenderSVGRoot::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(snappedIntRect(accumulatedOffset, borderBoxRect().size()));
}

void RenderSVGRoot::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    auto* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, this))
        return;

    quads.append(localToAbsoluteQuad(FloatRect { borderBoxRect() }, UseTransforms, wasFixed));
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
