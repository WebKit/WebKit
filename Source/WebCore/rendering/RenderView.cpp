/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "RenderView.h"

#include "Document.h"
#include "Element.h"
#include "FloatQuad.h"
#include "FloatingObjects.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLBodyElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HitTestResult.h"
#include "ImageQualityController.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "RenderDescendantIterator.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayoutState.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderQuote.h"
#include "RenderTreeBuilder.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "StyleInheritedData.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderView);

RenderView::RenderView(Document& document, RenderStyle&& style)
    : RenderBlockFlow(document, WTFMove(style))
    , m_frameView(*document.view())
    , m_selection(*this)
    , m_lazyRepaintTimer(*this, &RenderView::lazyRepaintTimerFired)
{
    setIsRenderView();

    // FIXME: We should find a way to enforce this at compile time.
    ASSERT(document.view());

    // init RenderObject attributes
    setInline(false);
    
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
    
    setPositionState(PositionType::Absolute); // to 0,0 :)
}

RenderView::~RenderView()
{
    ASSERT_WITH_MESSAGE(m_rendererCount == 1, "All other renderers in this render tree should have been destroyed");
}

void RenderView::scheduleLazyRepaint(RenderBox& renderer)
{
    if (renderer.renderBoxNeedsLazyRepaint())
        return;
    renderer.setRenderBoxNeedsLazyRepaint(true);
    m_renderersNeedingLazyRepaint.add(&renderer);
    if (!m_lazyRepaintTimer.isActive())
        m_lazyRepaintTimer.startOneShot(0_s);
}

void RenderView::unscheduleLazyRepaint(RenderBox& renderer)
{
    if (!renderer.renderBoxNeedsLazyRepaint())
        return;
    renderer.setRenderBoxNeedsLazyRepaint(false);
    m_renderersNeedingLazyRepaint.remove(&renderer);
    if (m_renderersNeedingLazyRepaint.isEmpty())
        m_lazyRepaintTimer.stop();
}

void RenderView::lazyRepaintTimerFired()
{
    for (auto& renderer : m_renderersNeedingLazyRepaint) {
        renderer->repaint();
        renderer->setRenderBoxNeedsLazyRepaint(false);
    }
    m_renderersNeedingLazyRepaint.clear();
}

RenderBox::LogicalExtentComputedValues RenderView::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit) const
{
    return { !shouldUsePrintingLayout() ? LayoutUnit(viewLogicalHeight()) : logicalHeight, 0_lu, ComputedMarginValues() };
}

void RenderView::updateLogicalWidth()
{
    setLogicalWidth(shouldUsePrintingLayout() ? m_pageLogicalSize->width() : LayoutUnit(viewLogicalWidth()));
}

LayoutUnit RenderView::availableLogicalHeight(AvailableLogicalHeightType) const
{
    // Make sure block progression pagination for percentages uses the column extent and
    // not the view's extent. See https://bugs.webkit.org/show_bug.cgi?id=135204.
    if (multiColumnFlow() && multiColumnFlow()->firstMultiColumnSet())
        return multiColumnFlow()->firstMultiColumnSet()->computedColumnHeight();

#if PLATFORM(IOS_FAMILY)
    // Workaround for <rdar://problem/7166808>.
    if (document().isPluginDocument() && frameView().useFixedLayout())
        return frameView().fixedLayoutSize().height();
#endif
    return isHorizontalWritingMode() ? frameView().visibleHeight() : frameView().visibleWidth();
}

bool RenderView::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return child.isBox();
}

void RenderView::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    if (!document().paginated())
        m_pageLogicalSize = { };

    if (shouldUsePrintingLayout()) {
        if (!m_pageLogicalSize)
            m_pageLogicalSize = LayoutSize(logicalWidth(), 0_lu);
        m_minPreferredLogicalWidth = m_pageLogicalSize->width();
        m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth;
    }

    // Use calcWidth/Height to get the new width/height, since this will take the full page zoom factor into account.
    bool relayoutChildren = !shouldUsePrintingLayout() && (width() != viewWidth() || height() != viewHeight());
    if (relayoutChildren) {
        setChildNeedsLayout(MarkOnlyThis);

        for (auto& box : childrenOfType<RenderBox>(*this)) {
            if (box.hasRelativeLogicalHeight()
                || box.style().logicalHeight().isPercentOrCalculated()
                || box.style().logicalMinHeight().isPercentOrCalculated()
                || box.style().logicalMaxHeight().isPercentOrCalculated()
                || box.isSVGRoot()
                )
                box.setChildNeedsLayout(MarkOnlyThis);
        }
    }

    ASSERT(!frameView().layoutContext().layoutState());
    if (!needsLayout())
        return;

    LayoutStateMaintainer statePusher(*this, { }, false, m_pageLogicalSize.valueOr(LayoutSize()).height(), m_pageLogicalHeightChanged);

    m_pageLogicalHeightChanged = false;

    RenderBlockFlow::layout();

#ifndef NDEBUG
    frameView().layoutContext().checkLayoutState();
#endif
    clearNeedsLayout();
}

LayoutUnit RenderView::pageOrViewLogicalHeight() const
{
    if (shouldUsePrintingLayout())
        return m_pageLogicalSize->height();
    
    if (multiColumnFlow() && !style().hasInlineColumnAxis()) {
        if (int pageLength = frameView().pagination().pageLength)
            return pageLength;
    }

    return viewLogicalHeight();
}

LayoutUnit RenderView::clientLogicalWidthForFixedPosition() const
{
    // FIXME: If the FrameView's fixedVisibleContentRect() is not empty, perhaps it should be consulted here too?
    if (frameView().fixedElementsLayoutRelativeToFrame())
        return LayoutUnit((isHorizontalWritingMode() ? frameView().visibleWidth() : frameView().visibleHeight()) / frameView().frame().frameScaleFactor());

#if PLATFORM(IOS_FAMILY)
    if (frameView().useCustomFixedPositionLayoutRect())
        return isHorizontalWritingMode() ? frameView().customFixedPositionLayoutRect().width() : frameView().customFixedPositionLayoutRect().height();
#endif

    if (settings().visualViewportEnabled())
        return isHorizontalWritingMode() ? frameView().layoutViewportRect().width() : frameView().layoutViewportRect().height();

    return clientLogicalWidth();
}

LayoutUnit RenderView::clientLogicalHeightForFixedPosition() const
{
    // FIXME: If the FrameView's fixedVisibleContentRect() is not empty, perhaps it should be consulted here too?
    if (frameView().fixedElementsLayoutRelativeToFrame())
        return LayoutUnit((isHorizontalWritingMode() ? frameView().visibleHeight() : frameView().visibleWidth()) / frameView().frame().frameScaleFactor());

#if PLATFORM(IOS_FAMILY)
    if (frameView().useCustomFixedPositionLayoutRect())
        return isHorizontalWritingMode() ? frameView().customFixedPositionLayoutRect().height() : frameView().customFixedPositionLayoutRect().width();
#endif

    if (settings().visualViewportEnabled())
        return isHorizontalWritingMode() ? frameView().layoutViewportRect().height() : frameView().layoutViewportRect().width();

    return clientLogicalHeight();
}

void RenderView::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    // If a container was specified, and was not nullptr or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(ancestorContainer, !ancestorContainer || ancestorContainer == this);
    ASSERT_UNUSED(wasFixed, !wasFixed || *wasFixed == (mode & IsFixed));

    if (mode & IsFixed)
        transformState.move(toLayoutSize(frameView().scrollPositionRespectingCustomFixedPosition()));

    if (!ancestorContainer && mode & UseTransforms && shouldUseTransformFromContainer(nullptr)) {
        TransformationMatrix t;
        getTransformFromContainer(nullptr, LayoutSize(), t);
        transformState.applyTransform(t);
    }
}

const RenderObject* RenderView::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    // If a container was specified, and was not nullptr or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(ancestorToStopAt, !ancestorToStopAt || ancestorToStopAt == this);

    LayoutPoint scrollPosition = frameView().scrollPositionRespectingCustomFixedPosition();

    if (!ancestorToStopAt && shouldUseTransformFromContainer(nullptr)) {
        TransformationMatrix t;
        getTransformFromContainer(nullptr, LayoutSize(), t);
        geometryMap.pushView(this, toLayoutSize(scrollPosition), &t);
    } else
        geometryMap.pushView(this, toLayoutSize(scrollPosition));

    return nullptr;
}

void RenderView::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    if (mode & UseTransforms && shouldUseTransformFromContainer(nullptr)) {
        TransformationMatrix t;
        getTransformFromContainer(nullptr, LayoutSize(), t);
        transformState.applyTransform(t);
    }

    if (mode & IsFixed)
        transformState.move(toLayoutSize(frameView().scrollPositionRespectingCustomFixedPosition()));
}

bool RenderView::requiresColumns(int) const
{
    return frameView().pagination().mode != Pagination::Unpaginated;
}

void RenderView::computeColumnCountAndWidth()
{
    int columnWidth = contentLogicalWidth();
    if (style().hasInlineColumnAxis()) {
        if (int pageLength = frameView().pagination().pageLength)
            columnWidth = pageLength;
    }
    setComputedColumnCountAndWidth(1, columnWidth);
}

void RenderView::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!needsLayout());
    // RenderViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);

    // This avoids painting garbage between columns if there is a column gap.
    if (frameView().pagination().mode != Pagination::Unpaginated && paintInfo.shouldPaintWithinRoot(*this))
        paintInfo.context().fillRect(paintInfo.rect, frameView().baseBackgroundColor());

    paintObject(paintInfo, paintOffset);
}

RenderElement* RenderView::rendererForRootBackground() const
{
    auto* firstChild = this->firstChild();
    if (!firstChild)
        return nullptr;
    ASSERT(is<RenderElement>(*firstChild));
    auto& documentRenderer = downcast<RenderElement>(*firstChild);

    if (documentRenderer.hasBackground())
        return &documentRenderer;

    // We propagate the background only for HTML content.
    if (!is<HTMLHtmlElement>(documentRenderer.element()))
        return &documentRenderer;

    if (auto* body = document().body()) {
        if (auto* renderer = body->renderer())
            return renderer;
    }
    return &documentRenderer;
}

static inline bool rendererObscuresBackground(const RenderElement& rootElement)
{
    auto& style = rootElement.style();
    if (style.visibility() != Visibility::Visible || style.opacity() != 1 || style.hasTransform())
        return false;

    if (style.hasBorderRadius())
        return false;

    if (rootElement.isComposited())
        return false;

    auto* rendererForBackground = rootElement.view().rendererForRootBackground();
    if (!rendererForBackground)
        return false;

    if (rendererForBackground->style().backgroundClip() == FillBox::Text)
        return false;

    return true;
}

void RenderView::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    // Check to see if we are enclosed by a layer that requires complex painting rules.  If so, we cannot blit
    // when scrolling, and we need to use slow repaints.  Examples of layers that require this are transparent layers,
    // layers with reflections, or transformed layers.
    // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being inside
    // a transform, transparency layer, etc.
    for (HTMLFrameOwnerElement* element = document().ownerElement(); element && element->renderer(); element = element->document().ownerElement()) {
        RenderLayer* layer = element->renderer()->enclosingLayer();
        if (layer->cannotBlitToWindow()) {
            frameView().setCannotBlitToWindow();
            break;
        }

        if (RenderLayer* compositingLayer = layer->enclosingCompositingLayerForRepaint()) {
            if (!compositingLayer->backing()->paintsIntoWindow()) {
                frameView().setCannotBlitToWindow();
                break;
            }
        }
    }

    if (document().ownerElement())
        return;

    if (paintInfo.skipRootBackground())
        return;

    bool rootFillsViewport = false;
    bool rootObscuresBackground = false;
    Element* documentElement = document().documentElement();
    if (RenderElement* rootRenderer = documentElement ? documentElement->renderer() : nullptr) {
        // The document element's renderer is currently forced to be a block, but may not always be.
        RenderBox* rootBox = is<RenderBox>(*rootRenderer) ? downcast<RenderBox>(rootRenderer) : nullptr;
        rootFillsViewport = rootBox && !rootBox->x() && !rootBox->y() && rootBox->width() >= width() && rootBox->height() >= height();
        rootObscuresBackground = rendererObscuresBackground(*rootRenderer);
    }

    compositor().rootBackgroundColorOrTransparencyChanged();

    Page* page = document().page();
    float pageScaleFactor = page ? page->pageScaleFactor() : 1;

    // If painting will entirely fill the view, no need to fill the background.
    if (rootFillsViewport && rootObscuresBackground && pageScaleFactor >= 1)
        return;

    // This code typically only executes if the root element's visibility has been set to hidden,
    // if there is a transform on the <html>, or if there is a page scale factor less than 1.
    // Only fill with a background color (typically white) if we're the root document, 
    // since iframes/frames with no background in the child document should show the parent's background.
    // We use the base background color unless the backgroundShouldExtendBeyondPage setting is set,
    // in which case we use the document's background color.
    if (frameView().isTransparent()) // FIXME: This needs to be dynamic. We should be able to go back to blitting if we ever stop being transparent.
        frameView().setCannotBlitToWindow(); // The parent must show behind the child.
    else {
        const Color& documentBackgroundColor = frameView().documentBackgroundColor();
        const Color& backgroundColor = (settings().backgroundShouldExtendBeyondPage() && documentBackgroundColor.isValid()) ? documentBackgroundColor : frameView().baseBackgroundColor();
        if (backgroundColor.isVisible()) {
            CompositeOperator previousOperator = paintInfo.context().compositeOperation();
            paintInfo.context().setCompositeOperation(CompositeOperator::Copy);
            paintInfo.context().fillRect(paintInfo.rect, backgroundColor);
            paintInfo.context().setCompositeOperation(previousOperator);
        } else
            paintInfo.context().clearRect(paintInfo.rect);
    }
}

bool RenderView::shouldRepaint(const LayoutRect& rect) const
{
    return !printing() && !rect.isEmpty();
}

void RenderView::repaintRootContents()
{
    if (layer()->isComposited()) {
        layer()->setBackingNeedsRepaint(GraphicsLayer::DoNotClipToLayer);
        return;
    }

    // Always use layoutOverflowRect() to fix rdar://problem/27182267.
    // This should be cleaned up via webkit.org/b/159913 and webkit.org/b/159914.
    RenderLayerModelObject* repaintContainer = containerForRepaint();
    repaintUsingContainer(repaintContainer, computeRectForRepaint(layoutOverflowRect(), repaintContainer));
}

void RenderView::repaintViewRectangle(const LayoutRect& repaintRect) const
{
    if (!shouldRepaint(repaintRect))
        return;

    // FIXME: enclosingRect is needed as long as we integral snap ScrollView/FrameView/RenderWidget size/position.
    IntRect enclosingRect = enclosingIntRect(repaintRect);
    if (auto ownerElement = document().ownerElement()) {
        RenderBox* ownerBox = ownerElement->renderBox();
        if (!ownerBox)
            return;
        LayoutRect viewRect = this->viewRect();
#if PLATFORM(IOS_FAMILY)
        // Don't clip using the visible rect since clipping is handled at a higher level on iPhone.
        LayoutRect adjustedRect = enclosingRect;
#else
        LayoutRect adjustedRect = intersection(enclosingRect, viewRect);
#endif
        adjustedRect.moveBy(-viewRect.location());
        adjustedRect.moveBy(ownerBox->contentBoxRect().location());

        // A dirty rect in an iframe is relative to the contents of that iframe.
        // When we traverse between parent frames and child frames, we need to make sure
        // that the coordinate system is mapped appropriately between the iframe's contents
        // and the Renderer that contains the iframe. This transformation must account for a
        // left scrollbar (if one exists).
        FrameView& frameView = this->frameView();
        if (frameView.shouldPlaceBlockDirectionScrollbarOnLeft() && frameView.verticalScrollbar())
            adjustedRect.move(LayoutSize(frameView.verticalScrollbar()->occupiedWidth(), 0));

        ownerBox->repaintRectangle(adjustedRect);
        return;
    }

    frameView().addTrackedRepaintRect(snapRectToDevicePixels(repaintRect, document().deviceScaleFactor()));
    if (!m_accumulatedRepaintRegion) {
        frameView().repaintContentRectangle(enclosingRect);
        return;
    }
    m_accumulatedRepaintRegion->unite(enclosingRect);

    // Region will get slow if it gets too complex. Merge all rects so far to bounds if this happens.
    // FIXME: Maybe there should be a region type that does this automatically.
    static const unsigned maximumRepaintRegionGridSize = 16 * 16;
    if (m_accumulatedRepaintRegion->gridSize() > maximumRepaintRegionGridSize)
        m_accumulatedRepaintRegion = makeUnique<Region>(m_accumulatedRepaintRegion->bounds());
}

void RenderView::flushAccumulatedRepaintRegion() const
{
    ASSERT(!document().ownerElement());
    ASSERT(m_accumulatedRepaintRegion);
    auto repaintRects = m_accumulatedRepaintRegion->rects();
    for (auto& rect : repaintRects)
        frameView().repaintContentRectangle(rect);
    m_accumulatedRepaintRegion = nullptr;
}

void RenderView::repaintViewAndCompositedLayers()
{
    repaintRootContents();

    RenderLayerCompositor& compositor = this->compositor();
    if (compositor.usesCompositing())
        compositor.repaintCompositedLayers();
}

Optional<LayoutRect> RenderView::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    // If a container was specified, and was not nullptr or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(container, !container || container == this);

    if (printing())
        return rect;
    
    LayoutRect adjustedRect = rect;
    if (style().isFlippedBlocksWritingMode()) {
        // We have to flip by hand since the view's logical height has not been determined.  We
        // can use the viewport width and height.
        if (style().isHorizontalWritingMode())
            adjustedRect.setY(viewHeight() - adjustedRect.maxY());
        else
            adjustedRect.setX(viewWidth() - adjustedRect.maxX());
    }

    if (context.hasPositionFixedDescendant)
        adjustedRect.moveBy(frameView().scrollPositionRespectingCustomFixedPosition());
    
    // Apply our transform if we have one (because of full page zooming).
    if (!container && layer() && layer()->transform())
        adjustedRect = LayoutRect(layer()->transform()->mapRect(snapRectToDevicePixels(adjustedRect, document().deviceScaleFactor())));
    return adjustedRect;
}

bool RenderView::isScrollableOrRubberbandableBox() const
{
    // The main frame might be allowed to rubber-band even if there is no content to scroll to. This is unique to
    // the main frame; subframes and overflow areas have to have content that can be scrolled to in order to rubber-band.
    FrameView::Scrollability defineScrollable = frame().ownerElement() ? FrameView::Scrollability::Scrollable : FrameView::Scrollability::ScrollableOrRubberbandable;
    return frameView().isScrollable(defineScrollable);
}

void RenderView::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(snappedIntRect(accumulatedOffset, layer()->size()));
}

void RenderView::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (wasFixed)
        *wasFixed = false;
    quads.append(FloatRect(FloatPoint(), layer()->size()));
}

bool RenderView::printing() const
{
    return document().printing();
}

bool RenderView::shouldUsePrintingLayout() const
{
    if (!printing())
        return false;
    return frameView().frame().shouldUsePrintingLayout();
}

LayoutRect RenderView::viewRect() const
{
    if (shouldUsePrintingLayout())
        return LayoutRect(LayoutPoint(), size());
    return frameView().visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
}

IntRect RenderView::unscaledDocumentRect() const
{
    LayoutRect overflowRect(layoutOverflowRect());
    flipForWritingMode(overflowRect);
    return snappedIntRect(overflowRect);
}

bool RenderView::rootBackgroundIsEntirelyFixed() const
{
    if (auto* rootBackgroundRenderer = rendererForRootBackground())
        return rootBackgroundRenderer->style().hasEntirelyFixedBackground();
    return false;
}
    
LayoutRect RenderView::unextendedBackgroundRect() const
{
    // FIXME: What is this? Need to patch for new columns?
    return unscaledDocumentRect();
}
    
LayoutRect RenderView::backgroundRect() const
{
    // FIXME: New columns care about this?
    if (frameView().hasExtendedBackgroundRectForPainting())
        return frameView().extendedBackgroundRectForPainting();

    return unextendedBackgroundRect();
}

IntRect RenderView::documentRect() const
{
    FloatRect overflowRect(unscaledDocumentRect());
    if (hasTransform())
        overflowRect = layer()->currentTransform().mapRect(overflowRect);
    return IntRect(overflowRect);
}

int RenderView::viewHeight() const
{
    int height = 0;
    if (!shouldUsePrintingLayout()) {
        height = frameView().layoutHeight();
        height = frameView().useFixedLayout() ? ceilf(style().effectiveZoom() * float(height)) : height;
    }
    return height;
}

int RenderView::viewWidth() const
{
    int width = 0;
    if (!shouldUsePrintingLayout()) {
        width = frameView().layoutWidth();
        width = frameView().useFixedLayout() ? ceilf(style().effectiveZoom() * float(width)) : width;
    }
    return width;
}

int RenderView::viewLogicalHeight() const
{
    int height = style().isHorizontalWritingMode() ? viewHeight() : viewWidth();
    return height;
}

void RenderView::setPageLogicalSize(LayoutSize size)
{
    if (!m_pageLogicalSize || m_pageLogicalSize->height() != size.height())
        m_pageLogicalHeightChanged = true;

    m_pageLogicalSize = size;
}

float RenderView::zoomFactor() const
{
    return frameView().frame().pageZoomFactor();
}

IntSize RenderView::viewportSizeForCSSViewportUnits() const
{
    return frameView().viewportSizeForCSSViewportUnits();
}

Node* RenderView::nodeForHitTest() const
{
    return document().documentElement();
}

void RenderView::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    if (multiColumnFlow() && multiColumnFlow()->firstMultiColumnSet())
        return multiColumnFlow()->firstMultiColumnSet()->updateHitTestResult(result, point);

    if (auto* node = nodeForHitTest()) {
        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);

        LayoutPoint adjustedPoint = point;
        offsetForContents(adjustedPoint);

        result.setLocalPoint(adjustedPoint);
    }
}

// FIXME: This function is obsolete and only used by embedded WebViews inside AppKit NSViews.
// Do not add callers of this function!
// The idea here is to take into account what object is moving the pagination point, and
// thus choose the best place to chop it.
void RenderView::setBestTruncatedAt(int y, RenderBoxModelObject* forRenderer, bool forcedBreak)
{
    // Nobody else can set a page break once we have a forced break.
    if (m_legacyPrinting.m_forcedPageBreak)
        return;

    // Forced breaks always win over unforced breaks.
    if (forcedBreak) {
        m_legacyPrinting.m_forcedPageBreak = true;
        m_legacyPrinting.m_bestTruncatedAt = y;
        return;
    }

    // Prefer the widest object that tries to move the pagination point
    LayoutRect boundingBox = forRenderer->borderBoundingBox();
    if (boundingBox.width() > m_legacyPrinting.m_truncatorWidth) {
        m_legacyPrinting.m_truncatorWidth = boundingBox.width();
        m_legacyPrinting.m_bestTruncatedAt = y;
    }
}

bool RenderView::usesCompositing() const
{
    return m_compositor && m_compositor->usesCompositing();
}

RenderLayerCompositor& RenderView::compositor()
{
    if (!m_compositor)
        m_compositor = makeUnique<RenderLayerCompositor>(*this);

    return *m_compositor;
}

void RenderView::setIsInWindow(bool isInWindow)
{
    if (m_compositor)
        m_compositor->setIsInWindow(isInWindow);
}

ImageQualityController& RenderView::imageQualityController()
{
    if (!m_imageQualityController)
        m_imageQualityController = makeUnique<ImageQualityController>(*this);
    return *m_imageQualityController;
}

void RenderView::registerForVisibleInViewportCallback(RenderElement& renderer)
{
    ASSERT(!m_visibleInViewportRenderers.contains(&renderer));
    m_visibleInViewportRenderers.add(&renderer);
}

void RenderView::unregisterForVisibleInViewportCallback(RenderElement& renderer)
{
    ASSERT(m_visibleInViewportRenderers.contains(&renderer));
    m_visibleInViewportRenderers.remove(&renderer);
}

void RenderView::updateVisibleViewportRect(const IntRect& visibleRect)
{
    resumePausedImageAnimationsIfNeeded(visibleRect);

    for (auto* renderer : m_visibleInViewportRenderers) {
        auto state = visibleRect.intersects(enclosingIntRect(renderer->absoluteClippedOverflowRect())) ? VisibleInViewportState::Yes : VisibleInViewportState::No;
        renderer->setVisibleInViewportState(state);
    }
}

void RenderView::addRendererWithPausedImageAnimations(RenderElement& renderer, CachedImage& image)
{
    ASSERT(!renderer.hasPausedImageAnimations() || m_renderersWithPausedImageAnimation.contains(&renderer));

    renderer.setHasPausedImageAnimations(true);
    auto& images = m_renderersWithPausedImageAnimation.ensure(&renderer, [] {
        return Vector<CachedImage*>();
    }).iterator->value;
    if (!images.contains(&image))
        images.append(&image);
}

void RenderView::removeRendererWithPausedImageAnimations(RenderElement& renderer)
{
    ASSERT(renderer.hasPausedImageAnimations());
    ASSERT(m_renderersWithPausedImageAnimation.contains(&renderer));

    renderer.setHasPausedImageAnimations(false);
    m_renderersWithPausedImageAnimation.remove(&renderer);
}

void RenderView::removeRendererWithPausedImageAnimations(RenderElement& renderer, CachedImage& image)
{
    ASSERT(renderer.hasPausedImageAnimations());

    auto it = m_renderersWithPausedImageAnimation.find(&renderer);
    ASSERT(it != m_renderersWithPausedImageAnimation.end());

    auto& images = it->value;
    if (!images.contains(&image))
        return;

    if (images.size() == 1)
        removeRendererWithPausedImageAnimations(renderer);
    else
        images.removeFirst(&image);
}

void RenderView::resumePausedImageAnimationsIfNeeded(const IntRect& visibleRect)
{
    Vector<std::pair<RenderElement*, CachedImage*>, 10> toRemove;
    for (auto& it : m_renderersWithPausedImageAnimation) {
        auto* renderer = it.key;
        for (auto* image : it.value) {
            if (renderer->repaintForPausedImageAnimationsIfNeeded(visibleRect, *image))
                toRemove.append(std::make_pair(renderer, image));
        }
    }
    for (auto& pair : toRemove)
        removeRendererWithPausedImageAnimations(*pair.first, *pair.second);
}

RenderView::RepaintRegionAccumulator::RepaintRegionAccumulator(RenderView* view)
{
    if (!view)
        return;

    auto* rootRenderView = view->document().topDocument().renderView();
    if (!rootRenderView)
        return;

    m_wasAccumulatingRepaintRegion = !!rootRenderView->m_accumulatedRepaintRegion;
    if (!m_wasAccumulatingRepaintRegion)
        rootRenderView->m_accumulatedRepaintRegion = makeUnique<Region>();
    m_rootView = makeWeakPtr(*rootRenderView);
}

RenderView::RepaintRegionAccumulator::~RepaintRegionAccumulator()
{
    if (m_wasAccumulatingRepaintRegion)
        return;
    if (!m_rootView)
        return;
    m_rootView.get()->flushAccumulatedRepaintRegion();
}

unsigned RenderView::pageNumberForBlockProgressionOffset(int offset) const
{
    int columnNumber = 0;
    const Pagination& pagination = page().pagination();
    if (pagination.mode == Pagination::Unpaginated)
        return columnNumber;
    
    bool progressionIsInline = false;
    bool progressionIsReversed = false;
    
    if (multiColumnFlow()) {
        progressionIsInline = multiColumnFlow()->progressionIsInline();
        progressionIsReversed = multiColumnFlow()->progressionIsReversed();
    } else
        return columnNumber;
    
    if (!progressionIsInline) {
        if (!progressionIsReversed)
            columnNumber = (pagination.pageLength + pagination.gap - offset) / (pagination.pageLength + pagination.gap);
        else
            columnNumber = offset / (pagination.pageLength + pagination.gap);
    }

    return columnNumber;
}

unsigned RenderView::pageCount() const
{
    const Pagination& pagination = page().pagination();
    if (pagination.mode == Pagination::Unpaginated)
        return 0;
    
    if (multiColumnFlow() && multiColumnFlow()->firstMultiColumnSet())
        return multiColumnFlow()->firstMultiColumnSet()->columnCount();

    return 0;
}

void RenderView::layerChildrenChangedDuringStyleChange(RenderLayer& layer)
{
    if (!m_styleChangeLayerMutationRoot) {
        m_styleChangeLayerMutationRoot = makeWeakPtr(layer);
        return;
    }

    RenderLayer* commonAncestor = m_styleChangeLayerMutationRoot->commonAncestorWithLayer(layer);
    m_styleChangeLayerMutationRoot = makeWeakPtr(commonAncestor);
}

RenderLayer* RenderView::takeStyleChangeLayerTreeMutationRoot()
{
    auto* result = m_styleChangeLayerMutationRoot.get();
    m_styleChangeLayerMutationRoot.clear();
    return result;
}

#if ENABLE(CSS_SCROLL_SNAP)
void RenderView::registerBoxWithScrollSnapPositions(const RenderBox& box)
{
    m_boxesWithScrollSnapPositions.add(&box);
}

void RenderView::unregisterBoxWithScrollSnapPositions(const RenderBox& box)
{
    m_boxesWithScrollSnapPositions.remove(&box);
}
#endif

} // namespace WebCore
