/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009-2023 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "LegacyRenderSVGRoot.h"

#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGResourceContainer.h"
#include "LocalFrame.h"
#include "Page.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGSVGElement.h"
#include "SVGViewSpec.h"
#include "TransformState.h"
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LegacyRenderSVGRoot);

const int defaultWidth = 300;
const int defaultHeight = 150;

LegacyRenderSVGRoot::LegacyRenderSVGRoot(SVGSVGElement& element, RenderStyle&& style)
    : RenderReplaced(Type::LegacySVGRoot, element, WTFMove(style), ReplacedFlag::UsesBoundaryCaching)
{
    ASSERT(isLegacyRenderSVGRoot());
    LayoutSize intrinsicSize(computeIntrinsicSize());
    if (!intrinsicSize.width())
        intrinsicSize.setWidth(defaultWidth);
    if (!intrinsicSize.height())
        intrinsicSize.setHeight(defaultHeight);
    setIntrinsicSize(intrinsicSize);
}

LegacyRenderSVGRoot::~LegacyRenderSVGRoot() = default;

SVGSVGElement& LegacyRenderSVGRoot::svgSVGElement() const
{
    return downcast<SVGSVGElement>(nodeForNonAnonymous());
}

Ref<SVGSVGElement> LegacyRenderSVGRoot::protectedSVGSVGElement() const
{
    return svgSVGElement();
}

bool LegacyRenderSVGRoot::hasIntrinsicAspectRatio() const
{
    return computeIntrinsicAspectRatio();
}

FloatSize LegacyRenderSVGRoot::computeIntrinsicSize() const
{
    ASSERT_IMPLIES(view().frameView().layoutContext().isInRenderTreeLayout(), !shouldApplySizeContainment());
    FloatSize intrinsicSize = { svgSVGElement().intrinsicWidth(), svgSVGElement().intrinsicHeight() };
    // Transpose for vertical writing mode
    if (!isHorizontalWritingMode())
        return intrinsicSize.transposedSize();
    return intrinsicSize;
}

FloatSize LegacyRenderSVGRoot::preferredAspectRatio() const
{
    ASSERT(!shouldApplySizeContainment());

    if (style().aspectRatio().isRatio())
        return FloatSize::narrowPrecision(style().aspectRatioLogicalWidth().value, style().aspectRatioLogicalHeight().value);

    auto intrinsicSize = computeIntrinsicSize();
    std::optional<FloatSize> intrinsicRatioValue;
    FloatSize preferredAspectRatio;
    if (!intrinsicSize.isEmpty())
        preferredAspectRatio = { intrinsicSize.width(), intrinsicSize.height() };
    else {
        FloatSize viewBoxSize = svgSVGElement().currentViewBoxRect().size();
        if (!viewBoxSize.isEmpty()) {
            // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
            if (isHorizontalWritingMode())
                intrinsicRatioValue = { viewBoxSize.width(), viewBoxSize.height() };
            else
                intrinsicRatioValue = { viewBoxSize.height(), viewBoxSize.width() };
        }
    }

    if (intrinsicRatioValue)
        return *intrinsicRatioValue;
    if (style().aspectRatio().isAutoAndRatio())
        return FloatSize::narrowPrecision(style().aspectRatioLogicalWidth().value, style().aspectRatioLogicalHeight().value);

    return preferredAspectRatio;
}

bool LegacyRenderSVGRoot::isEmbeddedThroughSVGImage() const
{
    return isInSVGImage(protectedSVGSVGElement().ptr());
}

bool LegacyRenderSVGRoot::isEmbeddedThroughFrameContainingSVGDocument() const
{
    // If our frame has an owner renderer, we're embedded through eg. object/embed/iframe,
    // but we only negotiate if we're in an SVG document inside object/embed, not iframe.
    if (!frame().ownerRenderer() || !frame().ownerRenderer()->isRenderEmbeddedObject() || !isDocumentElementRenderer())
        return false;
    return frame().document()->isSVGDocument();
}

LayoutUnit LegacyRenderSVGRoot::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.width();

    if (isEmbeddedThroughFrameContainingSVGDocument())
        return containingBlock()->contentBoxLogicalWidth();

    // SVG embedded via SVGImage (background-image/border-image/etc) / Inline SVG.
    return RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit LegacyRenderSVGRoot::computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth) const
{
    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.height();

    if (isEmbeddedThroughFrameContainingSVGDocument())
        return containingBlock()->availableLogicalHeight(AvailableLogicalHeightType::IncludeMarginBorderPadding);

    // SVG embedded via SVGImage (background-image/border-image/etc) / Inline SVG.
    return RenderReplaced::computeReplacedLogicalHeight(estimatedUsedWidth);
}

void LegacyRenderSVGRoot::layout()
{
    SetForScope change(m_inLayout, true);
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    m_resourcesNeedingToInvalidateClients.clear();

    // Arbitrary affine transforms are incompatible with RenderLayoutState.
    LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext());

    bool needsLayout = selfNeedsLayout();
    auto checkForRepaintOverride = !needsLayout ? std::make_optional(LayoutRepainter::CheckForRepaint::No) : std::nullopt;
    LayoutRepainter repainter(*this, checkForRepaintOverride);

    LayoutSize oldSize = size();
    updateLogicalWidth();
    updateLogicalHeight();
    buildLocalToBorderBoxTransform();

    m_isLayoutSizeChanged = needsLayout || (svgSVGElement().hasRelativeLengths() && oldSize != size());
    SVGRenderSupport::layoutChildren(*this, needsLayout || SVGRenderSupport::filtersForceContainerLayout(*this));

    if (!m_resourcesNeedingToInvalidateClients.isEmptyIgnoringNullReferences()) {
        // Invalidate resource clients, which may mark some nodes for layout.
        for (auto& resource :  m_resourcesNeedingToInvalidateClients) {
            resource.removeAllClientsFromCacheAndMarkForInvalidation();
            SVGResourcesCache::clientStyleChanged(resource, StyleDifference::Layout, nullptr, resource.style());
        }

        m_isLayoutSizeChanged = false;
        SVGRenderSupport::layoutChildren(*this, false);
    }

    // At this point LayoutRepainter already grabbed the old bounds,
    // recalculate them now so repaintAfterLayout() uses the new bounds.
    if (m_needsBoundariesOrTransformUpdate) {
        updateCachedBoundaries();
        m_needsBoundariesOrTransformUpdate = false;
    }

    clearOverflow();
    if (!shouldApplyViewportClip())
        addVisualOverflow(computeContentsInkOverflow());

    updateLayerTransform();
    m_hasBoxDecorations = isDocumentElementRenderer() ? hasVisibleBoxDecorationStyle() : hasVisibleBoxDecorations();
    invalidateBackgroundObscurationStatus();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

LayoutRect LegacyRenderSVGRoot::computeContentsInkOverflow() const
{
    FloatRect contentRepaintRect = repaintRectInLocalCoordinates();
    contentRepaintRect = m_localToBorderBoxTransform.mapRect(contentRepaintRect);
    // Condition the visual overflow rect to avoid being clipped/culled
    // out if it is huge. This may sacrifice overflow, but usually only
    // overflow that would never be seen anyway.
    // To condition, we intersect with something that we oftentimes
    // consider to be "infinity".
    return intersection(enclosingLayoutRect(contentRepaintRect), LayoutRect::infiniteRect());
}

bool LegacyRenderSVGRoot::shouldApplyViewportClip() const
{
    // the outermost svg is clipped if auto, and svg document roots are always clipped
    // When the svg is stand-alone (isDocumentElement() == true) the viewport clipping should always
    // be applied, noting that the window scrollbars should be hidden if overflow=hidden.
    return isNonVisibleOverflow(effectiveOverflowX()) || style().overflowX() == Overflow::Auto || this->isDocumentElementRenderer();
}

void LegacyRenderSVGRoot::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!isSkippedContentRoot(*this));

    // An empty viewport disables rendering.
    bool clipViewport = shouldApplyViewportClip();
    if (clipViewport && contentBoxSize().isEmpty())
        return;

    // Don't paint, if the context explicitly disabled it.
    if (paintInfo.phase != PaintPhase::EventRegion && paintInfo.context().paintingDisabled() && !paintInfo.context().detectingContentfulPaint())
        return;

    // SVG outlines are painted during PaintPhase::Foreground.
    if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline)
        return;

    // An empty viewBox also disables rendering.
    // (http://www.w3.org/TR/SVG/coords.html#ViewBoxAttribute)
    if (svgSVGElement().hasEmptyViewBox())
        return;

    GraphicsContext& context = paintInfo.context();
    if (context.detectingContentfulPaint()) {
        for (auto& current : childrenOfType<RenderObject>(*this)) {
            if (!current.isLegacyRenderSVGHiddenContainer()) {
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
                page().addRelevantUnpaintedObject(*this, visualOverflowRect());
            return;
        }
    }

    if (paintInfo.phase == PaintPhase::Foreground)
        page().addRelevantRepaintedObject(*this, visualOverflowRect());

    // Make a copy of the PaintInfo because applyTransform will modify the damage rect.
    PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context().save();

    // Apply initial viewport clip
    if (clipViewport) {
        auto clipRect = snappedIntRect(overflowClipRect(paintOffset));
        childPaintInfo.context().clip(clipRect);
        if (paintInfo.phase == PaintPhase::EventRegion && childPaintInfo.eventRegionContext())
            childPaintInfo.eventRegionContext()->pushClip(clipRect);
    }

    // Convert from container offsets (html renderers) to a relative transform (svg renderers).
    // Transform from our paint container's coordinate system to our local coords.
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset);
    auto transform = AffineTransform::makeTranslation(toFloatSize(adjustedPaintOffset)) * localToBorderBoxTransform();
    childPaintInfo.applyTransform(transform);
    if (paintInfo.phase == PaintPhase::EventRegion && childPaintInfo.eventRegionContext())
        childPaintInfo.eventRegionContext()->pushTransform(transform);

    // SVGRenderingContext must be destroyed before we restore the childPaintInfo.context(), because a filter may have
    // changed the context and it is only reverted when the SVGRenderingContext destructor finishes applying the filter.
    {
        SVGRenderingContext renderingContext;
        bool continueRendering = true;
        if (childPaintInfo.phase == PaintPhase::Foreground) {
            renderingContext.prepareToRenderSVGContent(*this, childPaintInfo);
            continueRendering = renderingContext.isRenderingPrepared();
        }

        if (continueRendering) {
            childPaintInfo.updateSubtreePaintRootForChildren(this);
            for (auto& child : childrenOfType<RenderElement>(*this))
                child.paint(childPaintInfo, location());
        }
    }

    if (paintInfo.phase == PaintPhase::EventRegion && childPaintInfo.eventRegionContext()) {
        childPaintInfo.eventRegionContext()->popTransform();
        if (clipViewport)
            childPaintInfo.eventRegionContext()->popClip();
    }
    childPaintInfo.context().restore();
}

void LegacyRenderSVGRoot::willBeDestroyed()
{
    RenderBlock::removePercentHeightDescendant(const_cast<LegacyRenderSVGRoot&>(*this));

    SVGResourcesCache::clientDestroyed(*this);
    RenderReplaced::willBeDestroyed();
}

void LegacyRenderSVGRoot::insertedIntoTree()
{
    RenderReplaced::insertedIntoTree();
    SVGResourcesCache::clientWasAddedToTree(*this);
}

void LegacyRenderSVGRoot::willBeRemovedFromTree()
{
    SVGResourcesCache::clientWillBeRemovedFromTree(*this);
    RenderReplaced::willBeRemovedFromTree();
}

void LegacyRenderSVGRoot::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (diff == StyleDifference::Layout)
        invalidateCachedBoundaries();

    // Box decorations may have appeared/disappeared - recompute status.
    if (diff == StyleDifference::Repaint)
        m_hasBoxDecorations = hasVisibleBoxDecorationStyle();

    RenderReplaced::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(*this, diff, oldStyle, style());
}

// RenderBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin. This method gives us exactly that.
void LegacyRenderSVGRoot::buildLocalToBorderBoxTransform()
{
    float scale = style().usedZoom();
    FloatPoint translate = svgSVGElement().currentTranslateValue();
    LayoutSize borderAndPadding(borderLeft() + paddingLeft(), borderTop() + paddingTop());
    m_localToBorderBoxTransform = svgSVGElement().viewBoxToViewTransform(contentBoxWidth() / scale, contentBoxHeight() / scale);
    if (borderAndPadding.isZero() && scale == 1 && translate == FloatPoint::zero())
        return;
    m_localToBorderBoxTransform = AffineTransform(scale, 0, 0, scale, borderAndPadding.width() + translate.x(), borderAndPadding.height() + translate.y()) * m_localToBorderBoxTransform;
}

const AffineTransform& LegacyRenderSVGRoot::localToParentTransform() const
{
    // Slightly optimized version of m_localToParentTransform = AffineTransform::makeTranslation(x(), y()) * m_localToBorderBoxTransform;
    m_localToParentTransform = m_localToBorderBoxTransform;
    if (x())
        m_localToParentTransform.setE(m_localToParentTransform.e() + roundToInt(x()));
    if (y())
        m_localToParentTransform.setF(m_localToParentTransform.f() + roundToInt(y()));
    return m_localToParentTransform;
}

LayoutRect LegacyRenderSVGRoot::localClippedOverflowRect(RepaintRectCalculation repaintRectCalculation) const
{
    auto contentRepaintRect = m_localToBorderBoxTransform.mapRect(repaintRectInLocalCoordinates(repaintRectCalculation));
    contentRepaintRect.intersect(snappedIntRect(borderBoxRect()));

    LayoutRect repaintRect = enclosingLayoutRect(contentRepaintRect);
    if (m_hasBoxDecorations || hasRenderOverflow())
        repaintRect.unite(unionRect(localSelectionRect(false), visualOverflowRect()));

    return enclosingIntRect(repaintRect);
}

LayoutRect LegacyRenderSVGRoot::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    auto rects = RepaintRects { localClippedOverflowRect(context.repaintRectCalculation()) };
    return RenderReplaced::computeRects(rects, repaintContainer, context).clippedOverflowRect;
}

auto LegacyRenderSVGRoot::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    auto rects = RepaintRects { localClippedOverflowRect(RepaintRectCalculation::Fast) };
    if (repaintOutlineBounds == RepaintOutlineBounds::Yes)
        rects.outlineBoundsRect = localOutlineBoundsRepaintRect();

    return RenderReplaced::computeRects(rects, repaintContainer, visibleRectContextForRepaint());
}

std::optional<FloatRect> LegacyRenderSVGRoot::computeFloatVisibleRectInContainer(const FloatRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    // Apply our local transforms (except for x/y translation) and then call
    // RenderBox's method to handle all the normal CSS Box model bits
    FloatRect adjustedRect = m_localToBorderBoxTransform.mapRect(rect);

    // Apply initial viewport clip
    if (shouldApplyViewportClip()) {
        if (context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection)) {
            if (!adjustedRect.edgeInclusiveIntersect(snappedIntRect(borderBoxRect())))
                return std::nullopt;
        } else
            adjustedRect.intersect(snappedIntRect(borderBoxRect()));
    }

    if (m_hasBoxDecorations || hasRenderOverflow()) {
        // The selectionRect can project outside of the overflowRect, so take their union
        // for repainting to avoid selection painting glitches.
        LayoutRect decoratedRepaintRect = unionRect(localSelectionRect(false), visualOverflowRect());
        adjustedRect.unite(decoratedRepaintRect);
    }

    auto rects = RepaintRects { LayoutRect(enclosingIntRect(adjustedRect)) };
    auto rectsInContainer = RenderReplaced::computeVisibleRectsInContainer(rects, container, context);
    if (!rectsInContainer)
        return std::nullopt;

    return FloatRect(rectsInContainer->clippedOverflowRect);
}

// This method expects local CSS box coordinates.
// Callers with local SVG viewport coordinates should first apply the localToBorderBoxTransform
// to convert from SVG viewport coordinates to local CSS box coordinates.
void LegacyRenderSVGRoot::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    RenderReplaced::mapLocalToContainer(ancestorContainer, transformState, mode | ApplyContainerFlip, wasFixed);
}

const RenderElement* LegacyRenderSVGRoot::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return RenderReplaced::pushMappingToContainer(ancestorToStopAt, geometryMap);
}

void LegacyRenderSVGRoot::updateCachedBoundaries()
{
    m_strokeBoundingBox = std::nullopt;
    m_repaintBoundingBox = { };
    m_accurateRepaintBoundingBox = std::nullopt;

    auto boundingBoxes = SVGRenderSupport::computeContainerBoundingBoxes(*this);
    m_objectBoundingBox = boundingBoxes.objectBoundingBox;

    SVGRenderSupport::intersectRepaintRectWithResources(*this, boundingBoxes.repaintBoundingBox);
    boundingBoxes.repaintBoundingBox.inflate(horizontalBorderAndPaddingExtent());

    m_repaintBoundingBox = boundingBoxes.repaintBoundingBox;
}

FloatRect LegacyRenderSVGRoot::strokeBoundingBox() const
{
    // FIXME: Once we enable approximate repainting bounding box computation, m_strokeBoundingBox becomes std::nullopt in updateCachedBoundaries and gets lazily computed.
    // https://bugs.webkit.org/show_bug.cgi?id=262409
    if (!m_strokeBoundingBox) {
        // Initialize m_strokeBoundingBox before calling computeContainerStrokeBoundingBox, since recursively referenced markers can cause us to re-enter here.
        m_strokeBoundingBox = FloatRect { };
        m_strokeBoundingBox = SVGRenderSupport::computeContainerStrokeBoundingBox(*this);
    }
    return *m_strokeBoundingBox;
}

FloatRect LegacyRenderSVGRoot::repaintRectInLocalCoordinates(RepaintRectCalculation repaintRectCalculation) const
{
    if (repaintRectCalculation == RepaintRectCalculation::Fast)
        return m_repaintBoundingBox;

    if (!m_accurateRepaintBoundingBox) {
        // Initialize m_accurateRepaintBoundingBox before calling computeContainerBoundingBoxes, since recursively referenced markers can cause us to re-enter here.
        m_accurateRepaintBoundingBox = FloatRect { };

        auto boundingBoxes = SVGRenderSupport::computeContainerBoundingBoxes(*this, RepaintRectCalculation::Accurate);
        SVGRenderSupport::intersectRepaintRectWithResources(*this, boundingBoxes.repaintBoundingBox, RepaintRectCalculation::Accurate);
        boundingBoxes.repaintBoundingBox.inflate(horizontalBorderAndPaddingExtent());

        m_accurateRepaintBoundingBox = boundingBoxes.repaintBoundingBox;
    }
    return *m_accurateRepaintBoundingBox;
}

bool LegacyRenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    LayoutPoint pointInParent = locationInContainer.point() - toLayoutSize(accumulatedOffset);
    LayoutPoint pointInBorderBox = pointInParent - toLayoutSize(location());

    // Test SVG content if the point is in our content box or it is inside the visualOverflowRect and the overflow is visible.
    // FIXME: This should be an intersection when rect-based hit tests are supported by nodeAtFloatPoint.
    if (contentBoxRect().contains(pointInBorderBox) || (!shouldApplyViewportClip() && visualOverflowRect().contains(pointInParent))) {
        FloatPoint localPoint = valueOrDefault(localToParentTransform().inverse()).mapPoint(FloatPoint(pointInParent));

        for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
            // FIXME: nodeAtFloatPoint() doesn't handle rect-based hit tests yet.
            if (child->nodeAtFloatPoint(request, result, localPoint, hitTestAction)) {
                updateHitTestResult(result, pointInBorderBox);
                if (result.addNodeToListBasedTestResult(child->protectedNode().get(), request, locationInContainer) == HitTestProgress::Stop)
                    return true;
            }
        }
    }

    // If we didn't early exit above, we've just hit the container <svg> element. Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) && visibleToHitTesting(request)) {
        // Only return true here, if the last hit testing phase 'BlockBackground' is executed. If we'd return true in the 'Foreground' phase,
        // hit testing would stop immediately. For SVG only trees this doesn't matter. Though when we have a <foreignObject> subtree we need
        // to be able to detect hits on the background of a <div> element. If we'd return true here in the 'Foreground' phase, we are not able 
        // to detect these hits anymore.
        LayoutRect boundsRect(accumulatedOffset + location(), size());
        if (locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, pointInBorderBox);
            if (result.addNodeToListBasedTestResult(protectedNodeForHitTest().get(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
                return true;
        }
    }

    return false;
}

bool LegacyRenderSVGRoot::hasRelativeDimensions() const
{
    return false;
}

void LegacyRenderSVGRoot::addResourceForClientInvalidation(LegacyRenderSVGResourceContainer* resource)
{
    LegacyRenderSVGRoot* svgRoot = SVGRenderSupport::findTreeRootObject(*resource);
    if (!svgRoot)
        return;
    svgRoot->m_resourcesNeedingToInvalidateClients.add(*resource);
}

}
