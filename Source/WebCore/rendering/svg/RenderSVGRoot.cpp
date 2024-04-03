/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009-2023 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2020, 2021, 2022, 2023, 2024 Igalia S.L.
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

#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "LocalFrame.h"
#include "Page.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderSVGText.h"
#include "RenderSVGViewportContainer.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGLayerTransformUpdater.h"
#include "SVGSVGElement.h"
#include "SVGViewSpec.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGRoot);

const int defaultWidth = 300;
const int defaultHeight = 150;

RenderSVGRoot::RenderSVGRoot(SVGSVGElement& element, RenderStyle&& style)
    : RenderReplaced(Type::SVGRoot, element, WTFMove(style))
{
    ASSERT(isRenderSVGRoot());
    LayoutSize intrinsicSize(calculateIntrinsicSize());
    if (!intrinsicSize.width())
        intrinsicSize.setWidth(defaultWidth);
    if (!intrinsicSize.height())
        intrinsicSize.setHeight(defaultHeight);
    setIntrinsicSize(intrinsicSize);
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

CheckedPtr<RenderSVGViewportContainer> RenderSVGRoot::checkedViewportContainer() const
{
    return viewportContainer();
}

bool RenderSVGRoot::hasIntrinsicAspectRatio() const
{
    return computeIntrinsicAspectRatio();
}

FloatSize RenderSVGRoot::calculateIntrinsicSize() const
{
    return FloatSize(floatValueForLength(svgSVGElement().intrinsicWidth(), 0), floatValueForLength(svgSVGElement().intrinsicHeight(), 0));
}

void RenderSVGRoot::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, FloatSize& intrinsicRatio) const
{
    ASSERT(!shouldApplySizeContainment());

    // https://www.w3.org/TR/SVG/coords.html#IntrinsicSizing
    intrinsicSize = calculateIntrinsicSize();

    if (style().aspectRatioType() == AspectRatioType::Ratio) {
        intrinsicRatio = FloatSize::narrowPrecision(style().aspectRatioLogicalWidth(), style().aspectRatioLogicalHeight());
        return;
    }

    std::optional<LayoutSize> intrinsicRatioValue;
    if (!intrinsicSize.isEmpty())
        intrinsicRatioValue = { intrinsicSize.width(), intrinsicSize.height() }; 
    else {
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
    if (!frame().ownerRenderer() || !frame().ownerRenderer()->isRenderEmbeddedObject() || !isDocumentElementRenderer())
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
    result *= style().usedZoom();
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
    result *= style().usedZoom();
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
    if (!shouldApplyViewportClip())
        addVisualOverflow(visualOverflowRectEquivalent());
    addVisualEffectOverflow();

    invalidateBackgroundObscurationStatus();

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGRoot::layoutChildren()
{
    SVGContainerLayout containerLayout(*this);
    containerLayout.layoutChildren(selfNeedsLayout());

    SVGBoundingBoxComputation boundingBoxComputation(*this);
    m_objectBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::objectBoundingBoxDecoration);
    m_strokeBoundingBox = std::nullopt;

    constexpr auto objectBoundingBoxDecorationWithoutTransformations = SVGBoundingBoxComputation::objectBoundingBoxDecoration | SVGBoundingBoxComputation::DecorationOption::IgnoreTransformations;
    m_objectBoundingBoxWithoutTransformations = boundingBoxComputation.computeDecoratedBoundingBox(objectBoundingBoxDecorationWithoutTransformations);

    containerLayout.positionChildrenRelativeToContainer();
}

FloatRect RenderSVGRoot::strokeBoundingBox() const
{
    if (!m_strokeBoundingBox) {
        // Initialize m_strokeBoundingBox before calling computeDecoratedBoundingBox, since recursively referenced markers can cause us to re-enter here.
        m_strokeBoundingBox = FloatRect { };
        SVGBoundingBoxComputation boundingBoxComputation(*this);
        m_strokeBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::strokeBoundingBoxDecoration);
    }
    return *m_strokeBoundingBox;
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
        && style().usedVisibility() == Visibility::Visible && paintInfo.shouldPaintWithinRoot(*this) && !paintInfo.paintRootBackgroundOnly())
        layer()->scrollableArea()->paintOverflowControls(paintInfo.context(), roundedIntPoint(adjustedPaintOffset), snappedIntRect(paintInfo.rect));
}

void RenderSVGRoot::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if ((paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::ChildBlockBackground) && style().usedVisibility() == Visibility::Visible) {
        if (hasVisibleBoxDecorations())
            paintBoxDecorations(paintInfo, paintOffset);
    }

    auto adjustedPaintOffset = paintOffset + location();
    if (paintInfo.phase == PaintPhase::Mask && style().usedVisibility() == Visibility::Visible) {
        paintSVGMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhase::ClippingMask && style().usedVisibility() == Visibility::Visible) {
        paintSVGClippingMask(paintInfo, objectBoundingBox());
        return;
    }

    if (paintInfo.paintRootBackgroundOnly())
        return;

    GraphicsContext& context = paintInfo.context();
    if (context.detectingContentfulPaint()) {
        for (auto& current : childrenOfType<RenderObject>(*this)) {
            if (!current.isRenderSVGHiddenContainer()) {
                context.setContentfulPaintDetected();
                return;
            }
        }
        return;
    }

    // Don't paint if we don't have kids, except if we have filters we should paint those.
    if (!firstChild()) {
        // FIXME: We should only call addRelevantUnpaintedObject() if there is no filter. Revisit this if we add filter support to LBSE.
        if (paintInfo.phase == PaintPhase::Foreground)
            page().addRelevantUnpaintedObject(*this, visualOverflowRect());
        return;
    }

    if (paintInfo.phase == PaintPhase::BlockBackground)
        return;

    auto scrolledOffset = paintOffset;
    scrolledOffset.moveBy(-scrollPosition());

    if (paintInfo.phase != PaintPhase::SelfOutline)
        paintContents(paintInfo, scrolledOffset);

    if ((paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) && hasOutline() && style().usedVisibility() == Visibility::Visible)
        paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, size()));
}

void RenderSVGRoot::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
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
    RenderReplaced::willBeDestroyed();
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

bool RenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    auto adjustedLocation = accumulatedOffset + location();

    auto visualOverflowRect = this->visualOverflowRect();
    visualOverflowRect.moveBy(adjustedLocation);

    // Test SVG content if the point is in our content box or it is inside the visualOverflowRect and the overflow is visible.
    if (contentBoxRect().contains(adjustedLocation) || (!shouldApplyViewportClip() && locationInContainer.intersects(visualOverflowRect))) {
        // Check kids first.
        for (auto* child = lastChild(); child; child = child->previousSibling()) {
            if (!child->hasLayer() && child->nodeAtPoint(request, result, locationInContainer, adjustedLocation, hitTestAction)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
                return true;
            }
        }
    }

    // If we didn't early exit above, we've just hit the container <svg> element. Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) && visibleToHitTesting(request)) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(adjustedLocation)));
            if (result.addNodeToListBasedTestResult(protectedNodeForHitTest().get(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
                return true;
        }
    }

    return false;
}

bool RenderSVGRoot::hasRelativeDimensions() const
{
    return svgSVGElement().intrinsicHeight().isPercentOrCalculated() || svgSVGElement().intrinsicWidth().isPercentOrCalculated();
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

    bool preserve3D = mode & UseTransforms && participatesInPreserve3D();
    if (mode & UseTransforms && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(containerOffset, t);

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
    TransformState transformStateAboveSVGFragment(transformState.direction(), transformState.mappedPoint());
    transformStateAboveSVGFragment.setTransformMatrixTracking(transformState.transformMatrixTracking());
    container->mapLocalToContainer(repaintContainer, transformStateAboveSVGFragment, mode, wasFixed);

    auto scale = 1.0 / style().usedZoom();
    if (auto transformAboveSVGFragment = transformStateAboveSVGFragment.releaseTrackedTransform()) {
        FloatPoint location(transformAboveSVGFragment->e(), transformAboveSVGFragment->f());
        location.scale(scale);

        auto unscaledTransform = TransformationMatrix().scale(scale) * *transformAboveSVGFragment;
        unscaledTransform.setE(location.x());
        unscaledTransform.setF(location.y());
        transformState.applyTransform(unscaledTransform, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    }

    // Respect scroll offset, after mapping to container coordinates.
    if (RefPtr view = document().view()) {
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

void RenderSVGRoot::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append({ accumulatedOffset, borderBoxRect().size() });
}

void RenderSVGRoot::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    auto* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, *this))
        return;

    quads.append(localToAbsoluteQuad(FloatRect { borderBoxRect() }, UseTransforms, wasFixed));
}

}
