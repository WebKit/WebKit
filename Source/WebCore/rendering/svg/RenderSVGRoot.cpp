/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2020, 2021 Igalia S.L.
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
#include "RenderSVGForeignObject.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
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

void RenderSVGRoot::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const
{
    ASSERT(!shouldApplySizeContainment(*this));

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
        intrinsicRatio = style().logicalAspectRatio();
        return;
    }

    std::optional<double> intrinsicRatioValue;
    if (!intrinsicSize.isEmpty())
        intrinsicRatioValue = intrinsicSize.width() / static_cast<double>(intrinsicSize.height());
    else {
        // - If either/both of the ‘width’ and ‘height’ of the rootmost ‘svg’ element are in percentage units (or omitted), the
        //   aspect ratio is calculated from the width and height values of the ‘viewBox’ specified for the current SVG document
        //   fragment. If the ‘viewBox’ is not correctly specified, or set to 'none', the intrinsic aspect ratio cannot be
        //   calculated and is considered unspecified.
        FloatSize viewBoxSize = svgSVGElement().viewBox().size();
        if (!viewBoxSize.isEmpty()) {
            // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
            intrinsicRatioValue = viewBoxSize.width() / static_cast<double>(viewBoxSize.height());
        }
    }

    if (intrinsicRatioValue)
        intrinsicRatio = *intrinsicRatioValue;
    else if (style().aspectRatioType() == AspectRatioType::AutoAndRatio)
        intrinsicRatio = style().logicalAspectRatio();
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

    // SVG embedded via SVGImage (background-image/border-image/etc) / Inline SVG.
    return RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit RenderSVGRoot::computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth) const
{
    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.height();

    if (isEmbeddedThroughFrameContainingSVGDocument())
        return containingBlock()->availableLogicalHeight(IncludeMarginBorderPadding);

    // Standalone SVG / SVG embedded via SVGImage (background-image/border-image/etc) / Inline SVG.
    return RenderReplaced::computeReplacedLogicalHeight(estimatedUsedWidth);
}

void RenderSVGRoot::layout()
{
    SetForScope<bool> change(m_inLayout, true);
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    m_resourcesNeedingToInvalidateClients.clear();

    // Arbitrary affine transforms are incompatible with RenderLayoutState.
    LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext());

    bool needsLayout = selfNeedsLayout();
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    auto previousLogicalSize = size();
    updateLogicalWidth();
    updateLogicalHeight();
    m_isLayoutSizeChanged = needsLayout || (svgSVGElement().hasRelativeLengths() && previousLogicalSize != size());

    auto oldTransform = m_supplementalLocalToParentTransform;
    computeTransformationMatrices();

    if (oldTransform != m_supplementalLocalToParentTransform)
        m_didTransformToRootUpdate = true;
    else if (previousLogicalSize != size())
        m_didTransformToRootUpdate = true;

    // FIXME: [LBSE] Upstream SVGLengthContext changes
    // svgSVGElement().updateLengthContext();

    // FIXME: [LBSE] Upstream SVGLayerTransformUpdater
    // SVGLayerTransformUpdater transformUpdater(*this);
    updateLayerInformation();

    {
        SVGContainerLayout containerLayout(*this);
        containerLayout.layoutChildren(needsLayout || SVGRenderSupport::filtersForceContainerLayout(*this));

        SVGBoundingBoxComputation boundingBoxComputation(*this);
        m_objectBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::objectBoundingBoxDecoration);
        m_strokeBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::strokeBoundingBoxDecoration);
    }

    if (!m_resourcesNeedingToInvalidateClients.isEmpty()) {
        // Invalidate resource clients, which may mark some nodes for layout.
        for (auto& resource :  m_resourcesNeedingToInvalidateClients) {
            resource->removeAllClientsFromCache();
            SVGResourcesCache::clientStyleChanged(*resource, StyleDifference::Layout, resource->style());
        }

        m_isLayoutSizeChanged = false;

        SVGContainerLayout containerLayout(*this);
        containerLayout.layoutChildren(false);
    }

    m_isLayoutSizeChanged = false;
    m_didTransformToRootUpdate = false;

    clearOverflow();
    if (!shouldApplyViewportClip()) {
        auto visualOverflowRect = enclosingLayoutRect(m_viewBoxTransform.mapRect(visualOverflowRectEquivalent()));
        addVisualOverflow(visualOverflowRect);
        addVisualEffectOverflow();
    }
    invalidateBackgroundObscurationStatus();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

bool RenderSVGRoot::shouldApplyViewportClip() const
{
    // the outermost svg is clipped if auto, and svg document roots are always clipped
    // When the svg is stand-alone (isDocumentElement() == true) the viewport clipping should always
    // be applied, noting that the window scrollbars should be hidden if overflow=hidden.
    return effectiveOverflowX() == Overflow::Hidden
        || style().overflowX() == Overflow::Auto
        || style().overflowX() == Overflow::Scroll
        || this->isDocumentElementRenderer();
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
    SVGResourcesCache::clientStyleChanged(*this, diff, style());
}

void RenderSVGRoot::updateLayerInformation()
{
    /* FIXME: [LBSE] Upstream SVGRenderSupport changes
    if (SVGRenderSupport::isRenderingDisabledDueToEmptySVGViewBox(*this))
        layer()->dirtyAncestorChainVisibleDescendantStatus();
     */
}

void RenderSVGRoot::updateFromStyle()
{
    RenderReplaced::updateFromStyle();

    // FIXME: [LBSE] Upstream RenderObject changes
    // setHasSVGTransform();

    if (shouldApplyViewportClip())
        setHasNonVisibleOverflow();
}

LayoutRect RenderSVGRoot::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    auto repaintRect = LayoutRect(valueOrDefault(m_viewBoxTransform.inverse()).mapRect(borderBoxRect()));
    return computeRect(repaintRect, repaintContainer, context);
}

void RenderSVGRoot::computeTransformationMatrices()
{
    // Compute SVG viewBox transformation against unscaled viewport.
    auto viewportSize = currentViewportSize();
    auto zoom = style().effectiveZoom();
    if (zoom != 1)
        viewportSize.scale(1.0 / zoom);
    m_viewBoxTransform = svgSVGElement().viewBoxToViewTransform(viewportSize.width(), viewportSize.height());

    // Compute total transformation matrix, taking border / padding (for child renderers that don't follow the CSS box model object!) + panning into account.
    auto panning = svgSVGElement().currentTranslateValue();
    auto contentLocation = contentBoxLocation();

    m_supplementalLocalToParentTransform.makeIdentity();
    m_supplementalLocalToParentTransform.translate(panning.x() + contentLocation.x(), panning.y() + contentLocation.y());
    m_supplementalLocalToParentTransform.scale(zoom);

    if (!m_viewBoxTransform.isIdentity())
        m_supplementalLocalToParentTransform.multiply(m_viewBoxTransform);
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

FloatSize RenderSVGRoot::currentViewportSize() const
{
    FloatSize result = contentBoxRect().size();
    result.setWidth(result.width() + verticalScrollbarWidth());
    result.setHeight(result.height() + horizontalScrollbarHeight());

    if (!isEmbeddedThroughFrameContainingSVGDocument()) {
        auto zoom = style().effectiveZoom();
        if (zoom != 1)
            result.scale(svgSVGElement().hasIntrinsicWidth() ? 1 : zoom, svgSVGElement().hasIntrinsicHeight() ? 1 : zoom);
    }

    return result;
}

void RenderSVGRoot::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (repaintContainer == this)
        return;

    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());

    bool containerSkipped;
    auto* container = this->container(repaintContainer, containerSkipped);
    if (!container)
        return;

    bool isFixedPos = isFixedPositioned();
    // If this box has a transform, it acts as a fixed position container for fixed descendants,
    // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
    if (hasTransform() && !isFixedPos)
        mode.remove(IsFixed);
    else if (isFixedPos)
        mode.add(IsFixed);

    if (wasFixed)
        *wasFixed = mode.contains(IsFixed);

    bool computingCTMOrScreenCTM = false;
    // FIXME: [LBSE] Upstream TransformState changes
    // bool computingCTMOrScreenCTM = transformState.transformMatrixTracking() != TransformState::DoNotTrackTransformMatrix;
    auto containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    bool preserve3D = mode & UseTransforms && (container->style().preserves3D() || style().preserves3D());

    if (mode & UseTransforms && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);

        /* FIXME: [LBSE] Upstream TransformState changes
        if (transformState.transformMatrixTracking() == TransformState::TrackSVGCTMMatrix) {
            auto offset = toLayoutSize(contentBoxLocation() + containerOffset);
            t.translateRight(-offset.width(), -offset.height());
        }
        */

        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);

    if (containerSkipped) {
        // There can't be a transform between repaintContainer and container, because transforms create containers, so it should be safe
        // to just subtract the delta between the repaintContainer and container.
        auto containerOffset = repaintContainer->offsetFromAncestorContainer(*container);
        transformState.move(-containerOffset.width(), -containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
        return;
    }

    mode.remove(ApplyContainerFlip);

    if (computingCTMOrScreenCTM) {
        // The CSS lengths/numbers above the SVG fragment (e.g. in HTML) do not adhere to SVG zoom rules.
        // All length information (e.g. top/width/...) include the scaling factor. In SVG no lengths are
        // scaled but a global scaling operation is included in the transform state.
        // For getCTM/getScreenCTM computations the result must be independent of the page zoom factor.
        // To compute these matrices within a non-SVG context (e.g. SVG embedded in HTML -- inline SVG)
        // the scaling needs to be removed from the CSS transform state.
        TransformState transformStateAboveSVGFragment(transformState.direction(), transformState.mappedPoint());
        // FIXME: [LBSE] Upstream TransformState changes
        // transformStateAboveSVGFragment.setTransformMatrixTracking(transformState.transformMatrixTracking());
        container->mapLocalToContainer(repaintContainer, transformStateAboveSVGFragment, mode, wasFixed);

        /* FIXME: [LBSE] Upstream TransformState changes
        if (transformState.transformMatrixTracking() == TransformState::TrackSVGScreenCTMMatrix) {
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
        }*/
    } else
        container->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
}

LayoutRect RenderSVGRoot::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy, PaintPhase) const
{
    auto clipRect = borderBoxRectInFragment(fragment);
    clipRect.setLocation(location + clipRect.location() + LayoutSize(borderLeft(), borderTop()));
    clipRect.setSize(clipRect.size() - LayoutSize(borderLeft() + borderRight(), borderTop() + borderBottom()));

    if (!isEmbeddedThroughFrameContainingSVGDocument()) {
        auto zoom = style().effectiveZoom();
        if (zoom != 1) {
            const auto& svgSVGElement = downcast<RenderSVGRoot>(*this).svgSVGElement();
            clipRect.scale(svgSVGElement.hasIntrinsicWidth() ? 1 : zoom, svgSVGElement.hasIntrinsicHeight() ? 1 : zoom);
        }
    }

    return clipRect;
}

void RenderSVGRoot::applyTransform(TransformationMatrix& transform, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    RenderReplaced::applyTransform(transform, boundingBox, options);

    // FIXME: [LBSE] Upstream SVGRenderSupport changes
    // SVGRenderSupport::applyTransform(*this, transform, style, boundingBox, std::nullopt, std::nullopt, options);
}

void RenderSVGRoot::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    auto localRect = LayoutRect(valueOrDefault(m_supplementalLocalToParentTransform.inverse()).mapRect(borderBoxRect()));
    rects.append(snappedIntRect(accumulatedOffset, localRect.size()));
}

void RenderSVGRoot::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, this))
        return;

    auto localRect = FloatRect(valueOrDefault(m_supplementalLocalToParentTransform.inverse()).mapRect(borderBoxRect()));
    quads.append(localToAbsoluteQuad(localRect, UseTransforms, wasFixed));
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
