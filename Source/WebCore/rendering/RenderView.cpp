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
#include "FlowThreadController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLIFrameElement.h"
#include "HitTestResult.h"
#include "ImageQualityController.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderNamedFlowThread.h"
#include "RenderSelectionInfo.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "StyleInheritedData.h"
#include "TransformState.h"
#include <wtf/StackStats.h>

namespace WebCore {

struct SelectionIterator {
    RenderObject* m_current;
    Vector<RenderMultiColumnSpannerPlaceholder*> m_spannerStack;
    
    SelectionIterator(RenderObject* o)
    {
        m_current = o;
        checkForSpanner();
    }
    
    void checkForSpanner()
    {
        if (!m_current || !m_current->isRenderMultiColumnSpannerPlaceholder())
            return;
        RenderMultiColumnSpannerPlaceholder* placeholder = toRenderMultiColumnSpannerPlaceholder(m_current);
        m_spannerStack.append(placeholder);
        m_current = placeholder->spanner();
    }
    
    RenderObject* current()
    {
        return m_current;
    }
    
    RenderObject* next()
    {
        RenderObject* currentSpan = m_spannerStack.isEmpty() ? 0 : m_spannerStack.last()->spanner();
        m_current = m_current->nextInPreOrder(currentSpan);
        checkForSpanner();
        if (!m_current && currentSpan) {
            RenderObject* placeholder = m_spannerStack.last();
            m_spannerStack.removeLast();
            m_current = placeholder->nextInPreOrder();
            checkForSpanner();
        }
        return m_current;
    }
};

RenderView::RenderView(Document& document, PassRef<RenderStyle> style)
    : RenderBlockFlow(document, WTF::move(style))
    , m_frameView(*document.view())
    , m_selectionUnsplitStart(0)
    , m_selectionUnsplitEnd(0)
    , m_selectionUnsplitStartPos(-1)
    , m_selectionUnsplitEndPos(-1)
    , m_rendererCount(0)
    , m_maximalOutlineSize(0)
    , m_lazyRepaintTimer(this, &RenderView::lazyRepaintTimerFired)
    , m_pageLogicalHeight(0)
    , m_pageLogicalHeightChanged(false)
    , m_layoutState(nullptr)
    , m_layoutStateDisableCount(0)
    , m_renderQuoteHead(0)
    , m_renderCounterCount(0)
    , m_selectionWasCaret(false)
    , m_hasSoftwareFilters(false)
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionRectGatherer(*this)
#endif
{
    setIsRenderView();

    // FIXME: We should find a way to enforce this at compile time.
    ASSERT(document.view());

    // init RenderObject attributes
    setInline(false);
    
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
    
    setPositionState(AbsolutePosition); // to 0,0 :)
}

RenderView::~RenderView()
{
}

void RenderView::scheduleLazyRepaint(RenderBox& renderer)
{
    if (renderer.renderBoxNeedsLazyRepaint())
        return;
    renderer.setRenderBoxNeedsLazyRepaint(true);
    m_renderersNeedingLazyRepaint.add(&renderer);
    if (!m_lazyRepaintTimer.isActive())
        m_lazyRepaintTimer.startOneShot(0);
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

void RenderView::lazyRepaintTimerFired(Timer<RenderView>&)
{
    bool shouldRepaint = !document().inPageCache();

    for (auto& renderer : m_renderersNeedingLazyRepaint) {
        if (shouldRepaint)
            renderer->repaint();
        renderer->setRenderBoxNeedsLazyRepaint(false);
    }
    m_renderersNeedingLazyRepaint.clear();
}

bool RenderView::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool RenderView::hitTest(const HitTestRequest& request, const HitTestLocation& location, HitTestResult& result)
{
    document().updateLayout();

    if (layer()->hitTest(request, location, result))
        return true;

    // FIXME: Consider if this test should be done unconditionally.
    if (request.allowsFrameScrollbars()) {
        // ScrollView scrollbars are not the same as RenderLayer scrollbars tested by RenderLayer::hitTestOverflowControls,
        // so we need to test ScrollView scrollbars separately here.
        Scrollbar* frameScrollbar = frameView().scrollbarAtPoint(location.roundedPoint());
        if (frameScrollbar) {
            result.setScrollbar(frameScrollbar);
            return true;
        }
    }

    return false;
}

void RenderView::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = !shouldUsePrintingLayout() ? LayoutUnit(viewLogicalHeight()) : logicalHeight;
}

void RenderView::updateLogicalWidth()
{
    if (!shouldUsePrintingLayout())
        setLogicalWidth(viewLogicalWidth());
}

LayoutUnit RenderView::availableLogicalHeight(AvailableLogicalHeightType) const
{
    // Make sure block progression pagination for percentages uses the column extent and
    // not the view's extent. See https://bugs.webkit.org/show_bug.cgi?id=135204.
    if (multiColumnFlowThread() && multiColumnFlowThread()->firstMultiColumnSet())
        return multiColumnFlowThread()->firstMultiColumnSet()->computedColumnHeight();

#if PLATFORM(IOS)
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

void RenderView::layoutContent(const LayoutState& state)
{
    UNUSED_PARAM(state);
    ASSERT(needsLayout());

    RenderBlockFlow::layout();
    if (hasRenderNamedFlowThreads())
        flowThreadController().layoutRenderNamedFlowThreads();
#ifndef NDEBUG
    checkLayoutState(state);
#endif
}

#ifndef NDEBUG
void RenderView::checkLayoutState(const LayoutState& state)
{
    ASSERT(layoutDeltaMatches(LayoutSize()));
    ASSERT(!m_layoutStateDisableCount);
    ASSERT(m_layoutState.get() == &state);
}
#endif

void RenderView::initializeLayoutState(LayoutState& state)
{
    // FIXME: May be better to push a clip and avoid issuing offscreen repaints.
    state.m_clipped = false;

    state.m_pageLogicalHeight = m_pageLogicalHeight;
    state.m_pageLogicalHeightChanged = m_pageLogicalHeightChanged;
    state.m_isPaginated = state.m_pageLogicalHeight;
}

// The algorithm below assumes this is a full layout. In case there are previously computed values for regions, supplemental steps are taken
// to ensure the results are the same as those obtained from a full layout (i.e. the auto-height regions from all the flows are marked as needing
// layout).
// 1. The flows are laid out from the outer flow to the inner flow. This successfully computes the outer non-auto-height regions size so the 
// inner flows have the necessary information to correctly fragment the content.
// 2. The flows are laid out from the inner flow to the outer flow. After an inner flow is laid out it goes into the constrained layout phase
// and marks the auto-height regions they need layout. This means the outer flows will relayout if they depend on regions with auto-height regions
// belonging to inner flows. This step will correctly set the computedAutoHeight for the auto-height regions. It's possible for non-auto-height
// regions to relayout if they depend on auto-height regions. This will invalidate the inner flow threads and mark them as needing layout.
// 3. The last step is to do one last layout if there are pathological dependencies between non-auto-height regions and auto-height regions
// as detected in the previous step.
void RenderView::layoutContentInAutoLogicalHeightRegions(const LayoutState& state)
{
    // We need to invalidate all the flows with auto-height regions if one such flow needs layout.
    // If none is found we do a layout a check back again afterwards.
    if (!flowThreadController().updateFlowThreadsNeedingLayout()) {
        // Do a first layout of the content. In some cases more layouts are not needed (e.g. only flows with non-auto-height regions have changed).
        layoutContent(state);

        // If we find no named flow needing a two step layout after the first layout, exit early.
        // Otherwise, initiate the two step layout algorithm and recompute all the flows.
        if (!flowThreadController().updateFlowThreadsNeedingTwoStepLayout())
            return;
    }

    // Layout to recompute all the named flows with auto-height regions.
    layoutContent(state);

    // Propagate the computed auto-height values upwards.
    // Non-auto-height regions may invalidate the flow thread because they depended on auto-height regions, but that's ok.
    flowThreadController().updateFlowThreadsIntoConstrainedPhase();

    // Do one last layout that should update the auto-height regions found in the main flow
    // and solve pathological dependencies between regions (e.g. a non-auto-height region depending
    // on an auto-height one).
    if (needsLayout())
        layoutContent(state);
}

void RenderView::layoutContentToComputeOverflowInRegions(const LayoutState& state)
{
    if (!hasRenderNamedFlowThreads())
        return;

    // First pass through the flow threads and mark the regions as needing a simple layout.
    // The regions extract the overflow from the flow thread and pass it to their containg
    // block chain.
    flowThreadController().updateFlowThreadsIntoOverflowPhase();
    if (needsLayout())
        layoutContent(state);

    // In case scrollbars resized the regions a new pass is necessary to update the flow threads
    // and recompute the overflow on regions. This is the final state of the flow threads.
    flowThreadController().updateFlowThreadsIntoFinalPhase();
    if (needsLayout())
        layoutContent(state);

    // Finally reset the layout state of the flow threads.
    flowThreadController().updateFlowThreadsIntoMeasureContentPhase();
}

void RenderView::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    if (!document().paginated())
        setPageLogicalHeight(0);

    if (shouldUsePrintingLayout())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = logicalWidth();

    // Use calcWidth/Height to get the new width/height, since this will take the full page zoom factor into account.
    bool relayoutChildren = !shouldUsePrintingLayout() && (width() != viewWidth() || height() != viewHeight());
    if (relayoutChildren) {
        setChildNeedsLayout(MarkOnlyThis);

        for (auto& box : childrenOfType<RenderBox>(*this)) {
            if (box.hasRelativeLogicalHeight()
                || box.style().logicalHeight().isPercent()
                || box.style().logicalMinHeight().isPercent()
                || box.style().logicalMaxHeight().isPercent()
                || box.isSVGRoot()
                )
                box.setChildNeedsLayout(MarkOnlyThis);
        }
    }

    ASSERT(!m_layoutState);
    if (!needsLayout())
        return;

    m_layoutState = std::make_unique<LayoutState>();
    initializeLayoutState(*m_layoutState);

    m_pageLogicalHeightChanged = false;

    if (checkTwoPassLayoutForAutoHeightRegions())
        layoutContentInAutoLogicalHeightRegions(*m_layoutState);
    else
        layoutContent(*m_layoutState);

    layoutContentToComputeOverflowInRegions(*m_layoutState);

#ifndef NDEBUG
    checkLayoutState(*m_layoutState);
#endif
    m_layoutState = nullptr;
    clearNeedsLayout();
}

LayoutUnit RenderView::pageOrViewLogicalHeight() const
{
    if (document().printing())
        return pageLogicalHeight();
    
    if (multiColumnFlowThread() && !style().hasInlineColumnAxis()) {
        if (int pageLength = frameView().pagination().pageLength)
            return pageLength;
    }

    return viewLogicalHeight();
}

LayoutUnit RenderView::clientLogicalWidthForFixedPosition() const
{
    // FIXME: If the FrameView's fixedVisibleContentRect() is not empty, perhaps it should be consulted here too?
    if (frameView().fixedElementsLayoutRelativeToFrame())
        return (isHorizontalWritingMode() ? frameView().visibleWidth() : frameView().visibleHeight()) / frameView().frame().frameScaleFactor();

#if PLATFORM(IOS)
    if (frameView().useCustomFixedPositionLayoutRect())
        return isHorizontalWritingMode() ? frameView().customFixedPositionLayoutRect().width() : frameView().customFixedPositionLayoutRect().height();
#endif

    return clientLogicalWidth();
}

LayoutUnit RenderView::clientLogicalHeightForFixedPosition() const
{
    // FIXME: If the FrameView's fixedVisibleContentRect() is not empty, perhaps it should be consulted here too?
    if (frameView().fixedElementsLayoutRelativeToFrame())
        return (isHorizontalWritingMode() ? frameView().visibleHeight() : frameView().visibleWidth()) / frameView().frame().frameScaleFactor();

#if PLATFORM(IOS)
    if (frameView().useCustomFixedPositionLayoutRect())
        return isHorizontalWritingMode() ? frameView().customFixedPositionLayoutRect().height() : frameView().customFixedPositionLayoutRect().width();
#endif

    return clientLogicalHeight();
}

#if PLATFORM(IOS)
static inline LayoutSize fixedPositionOffset(const FrameView& frameView)
{
    return frameView.useCustomFixedPositionLayoutRect() ? (frameView.customFixedPositionLayoutRect().location() - LayoutPoint()) : frameView.scrollOffset();
}
#endif

void RenderView::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(repaintContainer, !repaintContainer || repaintContainer == this);
    ASSERT_UNUSED(wasFixed, !wasFixed || *wasFixed == (mode & IsFixed));

    if (!repaintContainer && mode & UseTransforms && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        transformState.applyTransform(t);
    }
    
    if (mode & IsFixed)
#if PLATFORM(IOS)
        transformState.move(fixedPositionOffset(m_frameView));
#else
        transformState.move(frameView().scrollOffsetForFixedPosition());
#endif
}

const RenderObject* RenderView::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(ancestorToStopAt, !ancestorToStopAt || ancestorToStopAt == this);

#if PLATFORM(IOS)
    LayoutSize scrollOffset = fixedPositionOffset(frameView());
#else
    LayoutSize scrollOffset = frameView().scrollOffsetForFixedPosition();
#endif

    if (!ancestorToStopAt && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        geometryMap.pushView(this, scrollOffset, &t);
    } else
        geometryMap.pushView(this, scrollOffset);

    return 0;
}

void RenderView::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    if (mode & IsFixed)
#if PLATFORM(IOS)
        transformState.move(fixedPositionOffset(frameView()));
#else
        transformState.move(frameView().scrollOffsetForFixedPosition());
#endif

    if (mode & UseTransforms && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        transformState.applyTransform(t);
    }
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
        paintInfo.context->fillRect(paintInfo.rect, frameView().baseBackgroundColor(), ColorSpaceDeviceRGB);

    paintObject(paintInfo, paintOffset);
}

static inline bool isComposited(RenderElement* object)
{
    return object->hasLayer() && toRenderLayerModelObject(object)->layer()->isComposited();
}

static inline bool rendererObscuresBackground(RenderElement* rootObject)
{
    if (!rootObject)
        return false;
    
    const RenderStyle& style = rootObject->style();
    if (style.visibility() != VISIBLE
        || style.opacity() != 1
        || style.hasTransform())
        return false;
    
    if (isComposited(rootObject))
        return false;

    if (rootObject->rendererForRootBackground().style().backgroundClip() == TextFillBox)
        return false;

    if (style.hasBorderRadius())
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
    if (RenderElement* rootRenderer = documentElement ? documentElement->renderer() : 0) {
        // The document element's renderer is currently forced to be a block, but may not always be.
        RenderBox* rootBox = rootRenderer->isBox() ? toRenderBox(rootRenderer) : 0;
        rootFillsViewport = rootBox && !rootBox->x() && !rootBox->y() && rootBox->width() >= width() && rootBox->height() >= height();
        rootObscuresBackground = rendererObscuresBackground(rootRenderer);
    }

    bool backgroundShouldExtendBeyondPage = frameView().frame().settings().backgroundShouldExtendBeyondPage();
    compositor().setRootExtendedBackgroundColor(backgroundShouldExtendBeyondPage ? frameView().documentBackgroundColor() : Color());

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
        Color backgroundColor = backgroundShouldExtendBeyondPage ? frameView().documentBackgroundColor() : frameView().baseBackgroundColor();
        if (backgroundColor.alpha()) {
            CompositeOperator previousOperator = paintInfo.context->compositeOperation();
            paintInfo.context->setCompositeOperation(CompositeCopy);
            paintInfo.context->fillRect(paintInfo.rect, backgroundColor, style().colorSpace());
            paintInfo.context->setCompositeOperation(previousOperator);
        } else
            paintInfo.context->clearRect(paintInfo.rect);
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
    repaint();
}

void RenderView::repaintViewRectangle(const LayoutRect& repaintRect) const
{
    if (!shouldRepaint(repaintRect))
        return;

    if (auto ownerElement = document().ownerElement()) {
        RenderBox* ownerBox = ownerElement->renderBox();
        if (!ownerBox)
            return;
        LayoutRect viewRect = this->viewRect();
#if PLATFORM(IOS)
        // Don't clip using the visible rect since clipping is handled at a higher level on iPhone.
        LayoutRect adjustedRect = repaintRect;
#else
        LayoutRect adjustedRect = intersection(repaintRect, viewRect);
#endif
        adjustedRect.moveBy(-viewRect.location());
        adjustedRect.moveBy(ownerBox->contentBoxRect().location());
        ownerBox->repaintRectangle(adjustedRect);
        return;
    }

    frameView().addTrackedRepaintRect(snapRectToDevicePixels(repaintRect, document().deviceScaleFactor()));

    // FIXME: convert all repaint rect dependencies to FloatRect.
    IntRect enclosingRect = enclosingIntRect(repaintRect);
    if (!m_accumulatedRepaintRegion) {
        frameView().repaintContentRectangle(enclosingRect);
        return;
    }
    m_accumulatedRepaintRegion->unite(enclosingRect);

    // Region will get slow if it gets too complex. Merge all rects so far to bounds if this happens.
    // FIXME: Maybe there should be a region type that does this automatically.
    static const unsigned maximumRepaintRegionGridSize = 16 * 16;
    if (m_accumulatedRepaintRegion->gridSize() > maximumRepaintRegionGridSize)
        m_accumulatedRepaintRegion = std::make_unique<Region>(m_accumulatedRepaintRegion->bounds());
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
    if (compositor.inCompositingMode())
        compositor.repaintCompositedLayers();
}

LayoutRect RenderView::visualOverflowRect() const
{
    if (frameView().paintsEntireContents())
        return layoutOverflowRect();

    return RenderBlockFlow::visualOverflowRect();
}

void RenderView::computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect& rect, bool fixed) const
{
    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(repaintContainer, !repaintContainer || repaintContainer == this);

    if (printing())
        return;

    if (style().isFlippedBlocksWritingMode()) {
        // We have to flip by hand since the view's logical height has not been determined.  We
        // can use the viewport width and height.
        if (style().isHorizontalWritingMode())
            rect.setY(viewHeight() - rect.maxY());
        else
            rect.setX(viewWidth() - rect.maxX());
    }

    if (fixed) {
#if PLATFORM(IOS)
        rect.move(fixedPositionOffset(frameView()));
#else
        rect.move(frameView().scrollOffsetForFixedPosition());
#endif
    }
        
    // Apply our transform if we have one (because of full page zooming).
    if (!repaintContainer && layer() && layer()->transform())
        rect = LayoutRect(layer()->transform()->mapRect(snapRectToDevicePixels(rect, document().deviceScaleFactor())));
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

static RenderObject* rendererAfterPosition(RenderObject* object, unsigned offset)
{
    if (!object)
        return 0;

    RenderObject* child = object->childAt(offset);
    return child ? child : object->nextInPreOrderAfterChildren();
}

IntRect RenderView::selectionBounds(bool clipToVisibleContent) const
{
    LayoutRect selRect = subtreeSelectionBounds(*this, clipToVisibleContent);

    if (hasRenderNamedFlowThreads()) {
        for (auto* namedFlowThread : *m_flowThreadController->renderNamedFlowThreadList()) {
            LayoutRect currRect = subtreeSelectionBounds(*namedFlowThread, clipToVisibleContent);
            selRect.unite(currRect);
        }
    }

    return snappedIntRect(selRect);
}

LayoutRect RenderView::subtreeSelectionBounds(const SelectionSubtreeRoot& root, bool clipToVisibleContent) const
{
    typedef HashMap<RenderObject*, std::unique_ptr<RenderSelectionInfo>> SelectionMap;
    SelectionMap selectedObjects;

    RenderObject* os = root.selectionStart();
    RenderObject* stop = rendererAfterPosition(root.selectionEnd(), root.selectionEndPos());
    SelectionIterator selectionIterator(os);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == root.selectionStart() || os == root.selectionEnd()) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            selectedObjects.set(os, std::make_unique<RenderSelectionInfo>(*os, clipToVisibleContent));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isRenderView()) {
                std::unique_ptr<RenderSelectionInfo>& blockInfo = selectedObjects.add(cb, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = std::make_unique<RenderSelectionInfo>(*cb, clipToVisibleContent);
                cb = cb->containingBlock();
            }
        }

        os = selectionIterator.next();
    }

    // Now create a single bounding box rect that encloses the whole selection.
    LayoutRect selRect;
    SelectionMap::iterator end = selectedObjects.end();
    for (SelectionMap::iterator i = selectedObjects.begin(); i != end; ++i) {
        RenderSelectionInfo* info = i->value.get();
        // RenderSelectionInfo::rect() is in the coordinates of the repaintContainer, so map to page coordinates.
        LayoutRect currRect = info->rect();
        if (RenderLayerModelObject* repaintContainer = info->repaintContainer()) {
            FloatQuad absQuad = repaintContainer->localToAbsoluteQuad(FloatRect(currRect));
            currRect = absQuad.enclosingBoundingBox(); 
        }
        selRect.unite(currRect);
    }
    return selRect;
}

void RenderView::repaintSelection() const
{
    repaintSubtreeSelection(*this);

    if (hasRenderNamedFlowThreads()) {
        for (auto* namedFlowThread : *m_flowThreadController->renderNamedFlowThreadList())
            repaintSubtreeSelection(*namedFlowThread);
    }
}

void RenderView::repaintSubtreeSelection(const SelectionSubtreeRoot& root) const
{
    HashSet<RenderBlock*> processedBlocks;

    RenderObject* end = rendererAfterPosition(root.selectionEnd(), root.selectionEndPos());
    SelectionIterator selectionIterator(root.selectionStart());
    for (RenderObject* o = selectionIterator.current(); o && o != end; o = selectionIterator.next()) {
        if (!o->canBeSelectionLeaf() && o != root.selectionStart() && o != root.selectionEnd())
            continue;
        if (o->selectionState() == SelectionNone)
            continue;

        RenderSelectionInfo(*o, true).repaint();

        // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
        for (RenderBlock* block = o->containingBlock(); block && !block->isRenderView(); block = block->containingBlock()) {
            if (!processedBlocks.add(block).isNewEntry)
                break;
            RenderSelectionInfo(*block, true).repaint();
        }
    }
}

// Compositing layer dimensions take outline size into account, so we have to recompute layer
// bounds when it changes.
// FIXME: This is ugly; it would be nice to have a better way to do this.
void RenderView::setMaximalOutlineSize(int o)
{
    if (o != m_maximalOutlineSize) {
        m_maximalOutlineSize = o;

        // maximalOutlineSize affects compositing layer dimensions.
        compositor().setCompositingLayersNeedRebuild();    // FIXME: this really just needs to be a geometry update.
    }
}

void RenderView::setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode blockRepaintMode)
{
    // Make sure both our start and end objects are defined.
    // Check www.msnbc.com and try clicking around to find the case where this happened.
    if ((start && !end) || (end && !start))
        return;

    bool caretChanged = m_selectionWasCaret != frame().selection().isCaret();
    m_selectionWasCaret = frame().selection().isCaret();
    // Just return if the selection hasn't changed.
    if (m_selectionUnsplitStart == start && m_selectionUnsplitStartPos == startPos
        && m_selectionUnsplitEnd == end && m_selectionUnsplitEndPos == endPos && !caretChanged) {
        return;
    }

#if ENABLE(SERVICE_CONTROLS)
    // Clear the current rects and create a notifier for the new rects we are about to gather.
    // The Notifier updates the Editor when it goes out of scope and is destroyed.
    std::unique_ptr<SelectionRectGatherer::Notifier> rectNotifier = m_selectionRectGatherer.clearAndCreateNotifier();
#endif // ENABLE(SERVICE_CONTROLS)
    // Set global positions for new selection.
    m_selectionUnsplitStart = start;
    m_selectionUnsplitStartPos = startPos;
    m_selectionUnsplitEnd = end;
    m_selectionUnsplitEndPos = endPos;

    // If there is no RenderNamedFlowThreads we follow the regular selection.
    if (!hasRenderNamedFlowThreads()) {
        RenderSubtreesMap singleSubtreeMap;
        singleSubtreeMap.set(this, SelectionSubtreeRoot(start, startPos, end, endPos));
        updateSelectionForSubtrees(singleSubtreeMap, blockRepaintMode);
        return;
    }

    splitSelectionBetweenSubtrees(start, startPos, end, endPos, blockRepaintMode);
}

void RenderView::splitSelectionBetweenSubtrees(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode blockRepaintMode)
{
    // Compute the visible selection end points for each of the subtrees.
    RenderSubtreesMap renderSubtreesMap;

    SelectionSubtreeRoot initialSelection;
    renderSubtreesMap.set(this, initialSelection);
    for (auto* namedFlowThread : *flowThreadController().renderNamedFlowThreadList())
        renderSubtreesMap.set(namedFlowThread, initialSelection);

    if (start && end) {
        Node* startNode = start->node();
        Node* endNode = end->node();
        Node* stopNode = NodeTraversal::nextSkippingChildren(endNode);

        for (Node* node = startNode; node != stopNode; node = NodeTraversal::next(node)) {
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;

            SelectionSubtreeRoot& root = renderer->selectionRoot();
            SelectionSubtreeRoot selectionData = renderSubtreesMap.get(&root);
            if (selectionData.selectionClear()) {
                selectionData.setSelectionStart(node->renderer());
                selectionData.setSelectionStartPos(node == startNode ? startPos : 0);
            }

            selectionData.setSelectionEnd(node->renderer());
            if (node == endNode)
                selectionData.setSelectionEndPos(endPos);
            else
                selectionData.setSelectionEndPos(node->offsetInCharacters() ? node->maxCharacterOffset() : node->countChildNodes());

            renderSubtreesMap.set(&root, selectionData);
        }
    }
    
    updateSelectionForSubtrees(renderSubtreesMap, blockRepaintMode);
}

void RenderView::updateSelectionForSubtrees(RenderSubtreesMap& renderSubtreesMap, SelectionRepaintMode blockRepaintMode)
{
    SubtreeOldSelectionDataMap oldSelectionDataMap;
    for (auto it = renderSubtreesMap.begin(); it != renderSubtreesMap.end(); ++it) {
        SelectionSubtreeRoot& subtreeSelectionData = it->value;
        if (hasRenderNamedFlowThreads())
            subtreeSelectionData.adjustForVisibleSelection(document());

        std::unique_ptr<OldSelectionData> oldSelectionData = std::make_unique<OldSelectionData>();
        clearSubtreeSelection(*it->key, blockRepaintMode, *oldSelectionData);
        oldSelectionDataMap.set(it->key, WTF::move(oldSelectionData));

        SelectionSubtreeRoot& root = *it->key;
        root.setSelectionStart(subtreeSelectionData.selectionStart());
        root.setSelectionStartPos(subtreeSelectionData.selectionStartPos());
        root.setSelectionEnd(subtreeSelectionData.selectionEnd());
        root.setSelectionEndPos(subtreeSelectionData.selectionEndPos());
    }

    // Update selection status for the objects inside the selection subtrees.
    // This needs to be done after the previous loop updated the selectionStart/End
    // parameters of all subtrees because we're going to be climbing up the containing
    // block chain and we might end up in a different selection subtree.
    for (auto it = renderSubtreesMap.begin(); it != renderSubtreesMap.end(); ++it) {
        SelectionSubtreeRoot subtreeSelectionData = it->value;
        OldSelectionData& oldSelectionData = *oldSelectionDataMap.get(it->key);
        applySubtreeSelection(*it->key, subtreeSelectionData.selectionStart(), subtreeSelectionData.selectionEnd(), subtreeSelectionData.selectionEndPos(), blockRepaintMode, oldSelectionData);
    }
}

void RenderView::clearSubtreeSelection(const SelectionSubtreeRoot& root, SelectionRepaintMode blockRepaintMode, OldSelectionData& oldSelectionData)
{
    // Record the old selected objects.  These will be used later
    // when we compare against the new selected objects.
    oldSelectionData.selectionStartPos = root.selectionStartPos();
    oldSelectionData.selectionEndPos = root.selectionEndPos();
    
    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.

    RenderObject* os = root.selectionStart();
    RenderObject* stop = rendererAfterPosition(root.selectionEnd(), root.selectionEndPos());
    SelectionIterator selectionIterator(os);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == root.selectionStart() || os == root.selectionEnd())
            && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps.  They must be examined as well.
            oldSelectionData.selectedObjects.set(os, std::make_unique<RenderSelectionInfo>(*os, true));
            if (blockRepaintMode == RepaintNewXOROld) {
                RenderBlock* cb = os->containingBlock();
                while (cb && !cb->isRenderView()) {
                    std::unique_ptr<RenderBlockSelectionInfo>& blockInfo = oldSelectionData.selectedBlocks.add(cb, nullptr).iterator->value;
                    if (blockInfo)
                        break;
                    blockInfo = std::make_unique<RenderBlockSelectionInfo>(*cb);
                    cb = cb->containingBlock();
                }
            }
        }

        os = selectionIterator.next();
    }

    auto oldObjectsEnd = oldSelectionData.selectedObjects.end();
    for (auto i = oldSelectionData.selectedObjects.begin(); i != oldObjectsEnd; ++i)
        i->key->setSelectionStateIfNeeded(SelectionNone);
}

void RenderView::applySubtreeSelection(SelectionSubtreeRoot& root, RenderObject* start, RenderObject* end, int endPos, SelectionRepaintMode blockRepaintMode, const OldSelectionData& oldSelectionData)
{
    // Update the selection status of all objects between selectionStart and selectionEnd
    if (start && start == end)
        start->setSelectionStateIfNeeded(SelectionBoth);
    else {
        if (start)
            start->setSelectionStateIfNeeded(SelectionStart);
        if (end)
            end->setSelectionStateIfNeeded(SelectionEnd);
    }

    RenderObject* o = start;
    RenderObject* stop = rendererAfterPosition(end, endPos);
    SelectionIterator selectionIterator(o);
    
    while (o && o != stop) {
        if (o != start && o != end && o->canBeSelectionLeaf())
            o->setSelectionStateIfNeeded(SelectionInside);
        o = selectionIterator.next();
    }

    if (blockRepaintMode != RepaintNothing)
        layer()->clearBlockSelectionGapsBounds();

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    SelectedObjectMap newSelectedObjects;
    SelectedBlockMap newSelectedBlocks;
    o = start;
    selectionIterator = SelectionIterator(o);
    while (o && o != stop) {
        if ((o->canBeSelectionLeaf() || o == start || o == end) && o->selectionState() != SelectionNone) {
            std::unique_ptr<RenderSelectionInfo> selectionInfo = std::make_unique<RenderSelectionInfo>(*o, true);

#if ENABLE(SERVICE_CONTROLS)
            for (auto& rect : selectionInfo->collectedSelectionRects())
                m_selectionRectGatherer.addRect(selectionInfo->repaintContainer(), rect);
            if (!o->isTextOrLineBreak())
                m_selectionRectGatherer.setTextOnly(false);
#endif

            newSelectedObjects.set(o, WTF::move(selectionInfo));

            RenderBlock* cb = o->containingBlock();
            while (cb && !cb->isRenderView()) {
                std::unique_ptr<RenderBlockSelectionInfo>& blockInfo = newSelectedBlocks.add(cb, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = std::make_unique<RenderBlockSelectionInfo>(*cb);
                cb = cb->containingBlock();

#if ENABLE(SERVICE_CONTROLS)
                m_selectionRectGatherer.addGapRects(blockInfo->repaintContainer(), blockInfo->rects());
#endif
            }
        }

        o = selectionIterator.next();
    }

    if (blockRepaintMode == RepaintNothing)
        return;

    // Have any of the old selected objects changed compared to the new selection?
    auto oldObjectsEnd = oldSelectionData.selectedObjects.end();
    for (auto it = oldSelectionData.selectedObjects.begin(); it != oldObjectsEnd; ++it) {
        RenderObject* obj = it->key;
        RenderSelectionInfo* newInfo = newSelectedObjects.get(obj);
        RenderSelectionInfo* oldInfo = it->value.get();
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state()
            || (root.selectionStart() == obj && oldSelectionData.selectionStartPos != root.selectionStartPos())
            || (root.selectionEnd() == obj && oldSelectionData.selectionEndPos != root.selectionEndPos())) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedObjects.remove(obj);
            }
        }
    }

    // Any new objects that remain were not found in the old objects dict, and so they need to be updated.
    auto newObjectsEnd = newSelectedObjects.end();
    for (auto it = newSelectedObjects.begin(); it != newObjectsEnd; ++it)
        it->value->repaint();

    // Have any of the old blocks changed?
    auto oldBlocksEnd = oldSelectionData.selectedBlocks.end();
    for (auto it = oldSelectionData.selectedBlocks.begin(); it != oldBlocksEnd; ++it) {
        RenderBlock* block = it->key;
        RenderBlockSelectionInfo* newInfo = newSelectedBlocks.get(block);
        RenderBlockSelectionInfo* oldInfo = it->value.get();
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedBlocks.remove(block);
            }
        }
    }

    // Any new blocks that remain were not found in the old blocks dict, and so they need to be updated.
    auto newBlocksEnd = newSelectedBlocks.end();
    for (auto it = newSelectedBlocks.begin(); it != newBlocksEnd; ++it)
        it->value->repaint();
}

void RenderView::getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const
{
    startRenderer = m_selectionUnsplitStart;
    startOffset = m_selectionUnsplitStartPos;
    endRenderer = m_selectionUnsplitEnd;
    endOffset = m_selectionUnsplitEndPos;
}

void RenderView::clearSelection()
{
    layer()->repaintBlockSelectionGaps();
    setSelection(0, -1, 0, -1, RepaintNewMinusOld);
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
    RenderElement* rootObject = document().documentElement() ? document().documentElement()->renderer() : 0;
    if (!rootObject)
        return false;

    return rootObject->rendererForRootBackground().hasEntirelyFixedBackground();
}
    
LayoutRect RenderView::unextendedBackgroundRect(RenderBox*) const
{
    // FIXME: What is this? Need to patch for new columns?
    return unscaledDocumentRect();
}
    
LayoutRect RenderView::backgroundRect(RenderBox* backgroundRenderer) const
{
    // FIXME: New columns care about this?
    if (frameView().hasExtendedBackgroundRectForPainting())
        return frameView().extendedBackgroundRectForPainting();

    return unextendedBackgroundRect(backgroundRenderer);
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

float RenderView::zoomFactor() const
{
    return frameView().frame().pageZoomFactor();
}

void RenderView::pushLayoutState(RenderObject& root)
{
    ASSERT(m_layoutStateDisableCount == 0);
    ASSERT(m_layoutState == 0);

    m_layoutState = std::make_unique<LayoutState>(root);
    pushLayoutStateForCurrentFlowThread(root);
}

bool RenderView::shouldDisableLayoutStateForSubtree(RenderObject* renderer) const
{
    RenderObject* o = renderer;
    while (o) {
        if (o->hasTransform() || o->hasReflection())
            return true;
        o = o->container();
    }
    return false;
}

IntSize RenderView::viewportSizeForCSSViewportUnits() const
{
    return frameView().viewportSizeForCSSViewportUnits();
}

void RenderView::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = document().documentElement();
    if (node) {
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
    IntRect boundingBox = forRenderer->borderBoundingBox();
    if (boundingBox.width() > m_legacyPrinting.m_truncatorWidth) {
        m_legacyPrinting.m_truncatorWidth = boundingBox.width();
        m_legacyPrinting.m_bestTruncatedAt = y;
    }
}

bool RenderView::usesCompositing() const
{
    return m_compositor && m_compositor->inCompositingMode();
}

RenderLayerCompositor& RenderView::compositor()
{
    if (!m_compositor)
        m_compositor = std::make_unique<RenderLayerCompositor>(*this);

    return *m_compositor;
}

void RenderView::setIsInWindow(bool isInWindow)
{
    if (m_compositor)
        m_compositor->setIsInWindow(isInWindow);
}

void RenderView::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    if (hasRenderNamedFlowThreads())
        flowThreadController().styleDidChange();

    frameView().styleDidChange();
}

bool RenderView::hasRenderNamedFlowThreads() const
{
    return m_flowThreadController && m_flowThreadController->hasRenderNamedFlowThreads();
}

bool RenderView::checkTwoPassLayoutForAutoHeightRegions() const
{
    return hasRenderNamedFlowThreads() && m_flowThreadController->hasFlowThreadsWithAutoLogicalHeightRegions();
}

FlowThreadController& RenderView::flowThreadController()
{
    if (!m_flowThreadController)
        m_flowThreadController = std::make_unique<FlowThreadController>(this);

    return *m_flowThreadController;
}

void RenderView::pushLayoutStateForCurrentFlowThread(const RenderObject& object)
{
    if (!m_flowThreadController)
        return;

    RenderFlowThread* currentFlowThread = object.flowThreadContainingBlock();
    if (!currentFlowThread)
        return;

    m_layoutState->setCurrentRenderFlowThread(currentFlowThread);

    currentFlowThread->pushFlowThreadLayoutState(object);
}

void RenderView::popLayoutStateForCurrentFlowThread()
{
    if (!m_flowThreadController)
        return;

    RenderFlowThread* currentFlowThread = m_layoutState->currentRenderFlowThread();
    if (!currentFlowThread)
        return;

    currentFlowThread->popFlowThreadLayoutState();
}

IntervalArena* RenderView::intervalArena()
{
    if (!m_intervalArena)
        m_intervalArena = IntervalArena::create();
    return m_intervalArena.get();
}

ImageQualityController& RenderView::imageQualityController()
{
    if (!m_imageQualityController)
        m_imageQualityController = std::make_unique<ImageQualityController>(*this);
    return *m_imageQualityController;
}

void RenderView::addRendererWithPausedImageAnimations(RenderElement& renderer)
{
    if (renderer.hasPausedImageAnimations()) {
        ASSERT(m_renderersWithPausedImageAnimation.contains(&renderer));
        return;
    }
    renderer.setHasPausedImageAnimations(true);
    m_renderersWithPausedImageAnimation.add(&renderer);
}

void RenderView::removeRendererWithPausedImageAnimations(RenderElement& renderer)
{
    ASSERT(renderer.hasPausedImageAnimations());
    ASSERT(m_renderersWithPausedImageAnimation.contains(&renderer));

    renderer.setHasPausedImageAnimations(false);
    m_renderersWithPausedImageAnimation.remove(&renderer);
}

void RenderView::resumePausedImageAnimationsIfNeeded()
{
    auto visibleRect = frameView().windowToContents(frameView().windowClipRect());
    Vector<RenderElement*, 10> toRemove;
    for (auto* renderer : m_renderersWithPausedImageAnimation) {
        if (renderer->repaintForPausedImageAnimationsIfNeeded(visibleRect))
            toRemove.append(renderer);
    }
    for (auto& renderer : toRemove)
        removeRendererWithPausedImageAnimations(*renderer);
}

RenderView::RepaintRegionAccumulator::RepaintRegionAccumulator(RenderView* view)
    : m_rootView(view ? view->document().topDocument().renderView() : nullptr)
{
    if (!m_rootView)
        return;
    m_wasAccumulatingRepaintRegion = !!m_rootView->m_accumulatedRepaintRegion;
    if (!m_wasAccumulatingRepaintRegion)
        m_rootView->m_accumulatedRepaintRegion = std::make_unique<Region>();
}

RenderView::RepaintRegionAccumulator::~RepaintRegionAccumulator()
{
    if (!m_rootView)
        return;
    if (m_wasAccumulatingRepaintRegion)
        return;
    m_rootView->flushAccumulatedRepaintRegion();
}

unsigned RenderView::pageNumberForBlockProgressionOffset(int offset) const
{
    int columnNumber = 0;
    const Pagination& pagination = frameView().frame().page()->pagination();
    if (pagination.mode == Pagination::Unpaginated)
        return columnNumber;
    
    bool progressionIsInline = false;
    bool progressionIsReversed = false;
    
    if (multiColumnFlowThread()) {
        progressionIsInline = multiColumnFlowThread()->progressionIsInline();
        progressionIsReversed = multiColumnFlowThread()->progressionIsReversed();
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
    const Pagination& pagination = frameView().frame().page()->pagination();
    if (pagination.mode == Pagination::Unpaginated)
        return 0;
    
    if (multiColumnFlowThread() && multiColumnFlowThread()->firstMultiColumnSet())
        return multiColumnFlowThread()->firstMultiColumnSet()->columnCount();

    return 0;
}

} // namespace WebCore
