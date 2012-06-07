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

#include "ColumnInfo.h"
#include "Document.h"
#include "Element.h"
#include "FloatQuad.h"
#include "FlowThreadController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderNamedFlowThread.h"
#include "RenderSelectionInfo.h"
#include "RenderWidget.h"
#include "RenderWidgetProtector.h"
#include "TransformState.h"

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

namespace WebCore {

RenderView::RenderView(Node* node, FrameView* view)
    : RenderBlock(node)
    , m_frameView(view)
    , m_selectionStart(0)
    , m_selectionEnd(0)
    , m_selectionStartPos(-1)
    , m_selectionEndPos(-1)
    , m_maximalOutlineSize(0)
    , m_pageLogicalHeight(0)
    , m_pageLogicalHeightChanged(false)
    , m_layoutState(0)
    , m_layoutStateDisableCount(0)
{
    // Clear our anonymous bit, set because RenderObject assumes
    // any renderer with document as the node is anonymous.
    setIsAnonymous(false);

    // init RenderObject attributes
    setInline(false);
    
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
    
    setPositioned(true); // to 0,0 :)
}

RenderView::~RenderView()
{
}

bool RenderView::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return layer()->hitTest(request, result);
}

void RenderView::computeLogicalHeight()
{
    if (!shouldUsePrintingLayout() && m_frameView)
        setLogicalHeight(viewLogicalHeight());
}

void RenderView::computeLogicalWidth()
{
    if (!shouldUsePrintingLayout() && m_frameView)
        setLogicalWidth(viewLogicalWidth());
}

void RenderView::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    RenderBlock::computePreferredLogicalWidths();
}

bool RenderView::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isBox();
}

void RenderView::layout()
{
    if (!document()->paginated())
        setPageLogicalHeight(0);

    if (shouldUsePrintingLayout())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = logicalWidth();

    // Use calcWidth/Height to get the new width/height, since this will take the full page zoom factor into account.
    bool relayoutChildren = !shouldUsePrintingLayout() && (!m_frameView || width() != viewWidth() || height() != viewHeight());
    if (relayoutChildren) {
        setChildNeedsLayout(true, MarkOnlyThis);
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if ((child->isBox() && toRenderBox(child)->hasRelativeLogicalHeight())
                    || child->style()->logicalHeight().isPercent()
                    || child->style()->logicalMinHeight().isPercent()
                    || child->style()->logicalMaxHeight().isPercent())
                child->setChildNeedsLayout(true, MarkOnlyThis);
        }
    }

    ASSERT(!m_layoutState);
    LayoutState state;
    // FIXME: May be better to push a clip and avoid issuing offscreen repaints.
    state.m_clipped = false;
    state.m_pageLogicalHeight = m_pageLogicalHeight;
    state.m_pageLogicalHeightChanged = m_pageLogicalHeightChanged;
    state.m_isPaginated = state.m_pageLogicalHeight;
    m_pageLogicalHeightChanged = false;
    m_layoutState = &state;

    if (needsLayout()) {
        RenderBlock::layout();
        if (hasRenderNamedFlowThreads())
            flowThreadController()->layoutRenderNamedFlowThreads();
    }

    ASSERT(layoutDelta() == LayoutSize());
    ASSERT(m_layoutStateDisableCount == 0);
    ASSERT(m_layoutState == &state);
    m_layoutState = 0;
    setNeedsLayout(false);
}

void RenderView::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed, bool useTransforms, TransformState& transformState, ApplyContainerFlipOrNot, bool* wasFixed) const
{
    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(repaintContainer, !repaintContainer || repaintContainer == this);
    ASSERT_UNUSED(wasFixed, !wasFixed || *wasFixed == fixed);

    if (!repaintContainer && useTransforms && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        transformState.applyTransform(t);
    }
    
    if (fixed && m_frameView)
        transformState.move(m_frameView->scrollOffsetForFixedPosition());
}

void RenderView::mapAbsoluteToLocalPoint(bool fixed, bool useTransforms, TransformState& transformState) const
{
    if (fixed && m_frameView)
        transformState.move(m_frameView->scrollOffsetForFixedPosition());

    if (useTransforms && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        transformState.applyTransform(t);
    }
}

bool RenderView::requiresColumns(int desiredColumnCount) const
{
    if (m_frameView) {
        if (Frame* frame = m_frameView->frame()) {
            if (Page* page = frame->page())
                return frame == page->mainFrame() && page->pagination().mode != Page::Pagination::Unpaginated;
        }
    }
    return RenderBlock::requiresColumns(desiredColumnCount);
}

void RenderView::calcColumnWidth()
{
    int columnWidth = contentLogicalWidth();
    if (m_frameView && style()->hasInlineColumnAxis()) {
        if (Frame* frame = m_frameView->frame()) {
            if (Page* page = frame->page()) {
                if (int pageLength = page->pagination().pageLength)
                    columnWidth = pageLength;
            }
        }
    }
    setDesiredColumnCountAndWidth(1, columnWidth);
}

ColumnInfo::PaginationUnit RenderView::paginationUnit() const
{
    if (m_frameView) {
        if (Frame* frame = m_frameView->frame()) {
            if (Page* page = frame->page())
                return (frame == page->mainFrame() && page->pagination().behavesLikeColumns) ? ColumnInfo::Column : ColumnInfo::Page;
        }
    }
    return ColumnInfo::Page;
}

void RenderView::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!needsLayout());
    // RenderViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);
    paintObject(paintInfo, paintOffset);
}

static inline bool isComposited(RenderObject* object)
{
    return object->hasLayer() && toRenderBoxModelObject(object)->layer()->isComposited();
}

static inline bool rendererObscuresBackground(RenderObject* object)
{
    return object && object->style()->visibility() == VISIBLE
        && object->style()->opacity() == 1
        && !object->style()->hasTransform()
        && !isComposited(object);
}
    
void RenderView::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint&)
{
    // Check to see if we are enclosed by a layer that requires complex painting rules.  If so, we cannot blit
    // when scrolling, and we need to use slow repaints.  Examples of layers that require this are transparent layers,
    // layers with reflections, or transformed layers.
    // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being inside
    // a transform, transparency layer, etc.
    Element* elt;
    for (elt = document()->ownerElement(); view() && elt && elt->renderer(); elt = elt->document()->ownerElement()) {
        RenderLayer* layer = elt->renderer()->enclosingLayer();
        if (layer->cannotBlitToWindow()) {
            frameView()->setCannotBlitToWindow();
            break;
        }

#if USE(ACCELERATED_COMPOSITING)
        if (RenderLayer* compositingLayer = layer->enclosingCompositingLayer()) {
            if (!compositingLayer->backing()->paintsIntoWindow()) {
                frameView()->setCannotBlitToWindow();
                break;
            }
        }
#endif
    }

    if (document()->ownerElement() || !view())
        return;

    bool rootFillsViewport = false;
    Node* documentElement = document()->documentElement();
    if (RenderObject* rootRenderer = documentElement ? documentElement->renderer() : 0) {
        // The document element's renderer is currently forced to be a block, but may not always be.
        RenderBox* rootBox = rootRenderer->isBox() ? toRenderBox(rootRenderer) : 0;
        rootFillsViewport = rootBox && !rootBox->x() && !rootBox->y() && rootBox->width() >= width() && rootBox->height() >= height();
    }
    
    Page* page = document()->page();
    float pageScaleFactor = page ? page->pageScaleFactor() : 1;

    // If painting will entirely fill the view, no need to fill the background.
    if (rootFillsViewport && rendererObscuresBackground(firstChild()) && pageScaleFactor >= 1)
        return;

    // This code typically only executes if the root element's visibility has been set to hidden,
    // if there is a transform on the <html>, or if there is a page scale factor less than 1.
    // Only fill with the base background color (typically white) if we're the root document, 
    // since iframes/frames with no background in the child document should show the parent's background.
    if (frameView()->isTransparent()) // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being transparent.
        frameView()->setCannotBlitToWindow(); // The parent must show behind the child.
    else {
        Color baseColor = frameView()->baseBackgroundColor();
        if (baseColor.alpha() > 0) {
            CompositeOperator previousOperator = paintInfo.context->compositeOperation();
            paintInfo.context->setCompositeOperation(CompositeCopy);
            paintInfo.context->fillRect(paintInfo.rect, baseColor, style()->colorSpace());
            paintInfo.context->setCompositeOperation(previousOperator);
        } else
            paintInfo.context->clearRect(paintInfo.rect);
    }
}

bool RenderView::shouldRepaint(const LayoutRect& r) const
{
    if (printing() || r.width() == 0 || r.height() == 0)
        return false;

    if (!m_frameView)
        return false;

    if (m_frameView->repaintsDisabled())
        return false;

    return true;
}

void RenderView::repaintViewRectangle(const LayoutRect& ur, bool immediate)
{
    if (!shouldRepaint(ur))
        return;

    // We always just invalidate the root view, since we could be an iframe that is clipped out
    // or even invisible.
    Element* elt = document()->ownerElement();
    if (!elt)
        m_frameView->repaintContentRectangle(pixelSnappedIntRect(ur), immediate);
    else if (RenderBox* obj = elt->renderBox()) {
        LayoutRect vr = viewRect();
        LayoutRect r = intersection(ur, vr);
        
        // Subtract out the contentsX and contentsY offsets to get our coords within the viewing
        // rectangle.
        r.moveBy(-vr.location());

        // FIXME: Hardcoded offsets here are not good.
        r.moveBy(obj->contentBoxRect().location());
        obj->repaintRectangle(r, immediate);
    }
}

void RenderView::repaintRectangleInViewAndCompositedLayers(const LayoutRect& ur, bool immediate)
{
    if (!shouldRepaint(ur))
        return;

    repaintViewRectangle(ur, immediate);
    
#if USE(ACCELERATED_COMPOSITING)
    if (compositor()->inCompositingMode())
        compositor()->repaintCompositedLayersAbsoluteRect(pixelSnappedIntRect(ur));
#endif
}

void RenderView::computeRectForRepaint(RenderBoxModelObject* repaintContainer, LayoutRect& rect, bool fixed) const
{
    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(repaintContainer, !repaintContainer || repaintContainer == this);

    if (printing())
        return;

    if (style()->isFlippedBlocksWritingMode()) {
        // We have to flip by hand since the view's logical height has not been determined.  We
        // can use the viewport width and height.
        if (style()->isHorizontalWritingMode())
            rect.setY(viewHeight() - rect.maxY());
        else
            rect.setX(viewWidth() - rect.maxX());
    }

    if (fixed && m_frameView)
        rect.move(m_frameView->scrollXForFixedPosition(), m_frameView->scrollYForFixedPosition());
        
    // Apply our transform if we have one (because of full page zooming).
    if (!repaintContainer && m_layer && m_layer->transform())
        rect = m_layer->transform()->mapRect(rect);
}

void RenderView::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(pixelSnappedIntRect(accumulatedOffset, m_layer->size()));
}

void RenderView::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (wasFixed)
        *wasFixed = false;
    quads.append(FloatRect(FloatPoint(), m_layer->size()));
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
    document()->updateStyleIfNeeded();

    typedef HashMap<RenderObject*, RenderSelectionInfo*> SelectionMap;
    SelectionMap selectedObjects;

    RenderObject* os = m_selectionStart;
    RenderObject* stop = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            selectedObjects.set(os, new RenderSelectionInfo(os, clipToVisibleContent));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isRenderView()) {
                RenderSelectionInfo* blockInfo = selectedObjects.get(cb);
                if (blockInfo)
                    break;
                selectedObjects.set(cb, new RenderSelectionInfo(cb, clipToVisibleContent));
                cb = cb->containingBlock();
            }
        }

        os = os->nextInPreOrder();
    }

    // Now create a single bounding box rect that encloses the whole selection.
    LayoutRect selRect;
    SelectionMap::iterator end = selectedObjects.end();
    for (SelectionMap::iterator i = selectedObjects.begin(); i != end; ++i) {
        RenderSelectionInfo* info = i->second;
        // RenderSelectionInfo::rect() is in the coordinates of the repaintContainer, so map to page coordinates.
        LayoutRect currRect = info->rect();
        if (RenderBoxModelObject* repaintContainer = info->repaintContainer()) {
            FloatQuad absQuad = repaintContainer->localToAbsoluteQuad(FloatRect(currRect));
            currRect = absQuad.enclosingBoundingBox(); 
        }
        selRect.unite(currRect);
        delete info;
    }
    return pixelSnappedIntRect(selRect);
}

#if USE(ACCELERATED_COMPOSITING)
// Compositing layer dimensions take outline size into account, so we have to recompute layer
// bounds when it changes.
// FIXME: This is ugly; it would be nice to have a better way to do this.
void RenderView::setMaximalOutlineSize(int o)
{
    if (o != m_maximalOutlineSize) {
        m_maximalOutlineSize = o;

        // maximalOutlineSize affects compositing layer dimensions.
        compositor()->setCompositingLayersNeedRebuild();    // FIXME: this really just needs to be a geometry update.
    }
}
#endif

void RenderView::setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode blockRepaintMode)
{
    // Make sure both our start and end objects are defined.
    // Check www.msnbc.com and try clicking around to find the case where this happened.
    if ((start && !end) || (end && !start))
        return;

    // Just return if the selection hasn't changed.
    if (m_selectionStart == start && m_selectionStartPos == startPos &&
        m_selectionEnd == end && m_selectionEndPos == endPos)
        return;

    // Record the old selected objects.  These will be used later
    // when we compare against the new selected objects.
    int oldStartPos = m_selectionStartPos;
    int oldEndPos = m_selectionEndPos;

    // Objects each have a single selection rect to examine.
    typedef HashMap<RenderObject*, RenderSelectionInfo*> SelectedObjectMap;
    SelectedObjectMap oldSelectedObjects;
    SelectedObjectMap newSelectedObjects;

    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.
    typedef HashMap<RenderBlock*, RenderBlockSelectionInfo*> SelectedBlockMap;
    SelectedBlockMap oldSelectedBlocks;
    SelectedBlockMap newSelectedBlocks;

    RenderObject* os = m_selectionStart;
    RenderObject* stop = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps.  They must be examined as well.
            oldSelectedObjects.set(os, new RenderSelectionInfo(os, true));
            if (blockRepaintMode == RepaintNewXOROld) {
                RenderBlock* cb = os->containingBlock();
                while (cb && !cb->isRenderView()) {
                    RenderBlockSelectionInfo* blockInfo = oldSelectedBlocks.get(cb);
                    if (blockInfo)
                        break;
                    oldSelectedBlocks.set(cb, new RenderBlockSelectionInfo(cb));
                    cb = cb->containingBlock();
                }
            }
        }

        os = os->nextInPreOrder();
    }

    // Now clear the selection.
    SelectedObjectMap::iterator oldObjectsEnd = oldSelectedObjects.end();
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i)
        i->first->setSelectionStateIfNeeded(SelectionNone);

    // set selection start and end
    m_selectionStart = start;
    m_selectionStartPos = startPos;
    m_selectionEnd = end;
    m_selectionEndPos = endPos;

    // Update the selection status of all objects between m_selectionStart and m_selectionEnd
    if (start && start == end)
        start->setSelectionStateIfNeeded(SelectionBoth);
    else {
        if (start)
            start->setSelectionStateIfNeeded(SelectionStart);
        if (end)
            end->setSelectionStateIfNeeded(SelectionEnd);
    }

    RenderObject* o = start;
    stop = rendererAfterPosition(end, endPos);

    while (o && o != stop) {
        if (o != start && o != end && o->canBeSelectionLeaf())
            o->setSelectionStateIfNeeded(SelectionInside);
        o = o->nextInPreOrder();
    }

    if (blockRepaintMode != RepaintNothing)
        m_layer->clearBlockSelectionGapsBounds();

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    o = start;
    while (o && o != stop) {
        if ((o->canBeSelectionLeaf() || o == start || o == end) && o->selectionState() != SelectionNone) {
            newSelectedObjects.set(o, new RenderSelectionInfo(o, true));
            RenderBlock* cb = o->containingBlock();
            while (cb && !cb->isRenderView()) {
                RenderBlockSelectionInfo* blockInfo = newSelectedBlocks.get(cb);
                if (blockInfo)
                    break;
                newSelectedBlocks.set(cb, new RenderBlockSelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }

        o = o->nextInPreOrder();
    }

    if (!m_frameView || blockRepaintMode == RepaintNothing) {
        // We built the maps, but we aren't going to use them.
        // We need to delete the values, otherwise they'll all leak!
        deleteAllValues(oldSelectedObjects);
        deleteAllValues(newSelectedObjects);
        deleteAllValues(oldSelectedBlocks);
        deleteAllValues(newSelectedBlocks);
        return;
    }

    m_frameView->beginDeferredRepaints();

    // Have any of the old selected objects changed compared to the new selection?
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i) {
        RenderObject* obj = i->first;
        RenderSelectionInfo* newInfo = newSelectedObjects.get(obj);
        RenderSelectionInfo* oldInfo = i->second;
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state() ||
            (m_selectionStart == obj && oldStartPos != m_selectionStartPos) ||
            (m_selectionEnd == obj && oldEndPos != m_selectionEndPos)) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedObjects.remove(obj);
                delete newInfo;
            }
        }
        delete oldInfo;
    }

    // Any new objects that remain were not found in the old objects dict, and so they need to be updated.
    SelectedObjectMap::iterator newObjectsEnd = newSelectedObjects.end();
    for (SelectedObjectMap::iterator i = newSelectedObjects.begin(); i != newObjectsEnd; ++i) {
        RenderSelectionInfo* newInfo = i->second;
        newInfo->repaint();
        delete newInfo;
    }

    // Have any of the old blocks changed?
    SelectedBlockMap::iterator oldBlocksEnd = oldSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = oldSelectedBlocks.begin(); i != oldBlocksEnd; ++i) {
        RenderBlock* block = i->first;
        RenderBlockSelectionInfo* newInfo = newSelectedBlocks.get(block);
        RenderBlockSelectionInfo* oldInfo = i->second;
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedBlocks.remove(block);
                delete newInfo;
            }
        }
        delete oldInfo;
    }

    // Any new blocks that remain were not found in the old blocks dict, and so they need to be updated.
    SelectedBlockMap::iterator newBlocksEnd = newSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = newSelectedBlocks.begin(); i != newBlocksEnd; ++i) {
        RenderBlockSelectionInfo* newInfo = i->second;
        newInfo->repaint();
        delete newInfo;
    }

    m_frameView->endDeferredRepaints();
}

void RenderView::getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const
{
    startRenderer = m_selectionStart;
    startOffset = m_selectionStartPos;
    endRenderer = m_selectionEnd;
    endOffset = m_selectionEndPos;
}

void RenderView::clearSelection()
{
    m_layer->repaintBlockSelectionGaps();
    setSelection(0, -1, 0, -1, RepaintNewMinusOld);
}

void RenderView::selectionStartEnd(int& startPos, int& endPos) const
{
    startPos = m_selectionStartPos;
    endPos = m_selectionEndPos;
}

bool RenderView::printing() const
{
    return document()->printing();
}

bool RenderView::shouldUsePrintingLayout() const
{
    if (!printing() || !m_frameView)
        return false;
    Frame* frame = m_frameView->frame();
    return frame && frame->shouldUsePrintingLayout();
}

size_t RenderView::getRetainedWidgets(Vector<RenderWidget*>& renderWidgets)
{
    size_t size = m_widgets.size();

    renderWidgets.reserveCapacity(size);

    RenderWidgetSet::const_iterator end = m_widgets.end();
    for (RenderWidgetSet::const_iterator it = m_widgets.begin(); it != end; ++it) {
        renderWidgets.uncheckedAppend(*it);
        (*it)->ref();
    }
    
    return size;
}

void RenderView::releaseWidgets(Vector<RenderWidget*>& renderWidgets)
{
    size_t size = renderWidgets.size();

    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->deref(renderArena());
}

void RenderView::updateWidgetPositions()
{
    // updateWidgetPosition() can possibly cause layout to be re-entered (via plug-ins running
    // scripts in response to NPP_SetWindow, for example), so we need to keep the Widgets
    // alive during enumeration.    

    Vector<RenderWidget*> renderWidgets;
    size_t size = getRetainedWidgets(renderWidgets);
    
    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->updateWidgetPosition();

    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->widgetPositionsUpdated();

    releaseWidgets(renderWidgets);
}

void RenderView::addWidget(RenderWidget* o)
{
    m_widgets.add(o);
}

void RenderView::removeWidget(RenderWidget* o)
{
    m_widgets.remove(o);
}

void RenderView::notifyWidgets(WidgetNotification notification)
{
    Vector<RenderWidget*> renderWidgets;
    size_t size = getRetainedWidgets(renderWidgets);

    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->notifyWidget(notification);

    releaseWidgets(renderWidgets);
}

LayoutRect RenderView::viewRect() const
{
    if (shouldUsePrintingLayout())
        return LayoutRect(LayoutPoint(), size());
    if (m_frameView)
        return m_frameView->visibleContentRect();
    return LayoutRect();
}


IntRect RenderView::unscaledDocumentRect() const
{
    LayoutRect overflowRect(layoutOverflowRect());
    flipForWritingMode(overflowRect);
    return pixelSnappedIntRect(overflowRect);
}

LayoutRect RenderView::backgroundRect(RenderBox* backgroundRenderer) const
{
    if (!hasColumns())
        return unscaledDocumentRect();

    ColumnInfo* columnInfo = this->columnInfo();
    LayoutRect backgroundRect(0, 0, columnInfo->desiredColumnWidth(), columnInfo->columnHeight() * columnInfo->columnCount());
    if (!isHorizontalWritingMode())
        backgroundRect = backgroundRect.transposedRect();
    backgroundRenderer->flipForWritingMode(backgroundRect);

    return backgroundRect;
}

IntRect RenderView::documentRect() const
{
    IntRect overflowRect(unscaledDocumentRect());
    if (hasTransform())
        overflowRect = layer()->currentTransform().mapRect(overflowRect);
    return overflowRect;
}

int RenderView::viewHeight() const
{
    int height = 0;
    if (!shouldUsePrintingLayout() && m_frameView) {
        height = m_frameView->layoutHeight();
        height = m_frameView->useFixedLayout() ? ceilf(style()->effectiveZoom() * float(height)) : height;
    }
    return height;
}

int RenderView::viewWidth() const
{
    int width = 0;
    if (!shouldUsePrintingLayout() && m_frameView) {
        width = m_frameView->layoutWidth();
        width = m_frameView->useFixedLayout() ? ceilf(style()->effectiveZoom() * float(width)) : width;
    }
    return width;
}

int RenderView::viewLogicalHeight() const
{
    int height = style()->isHorizontalWritingMode() ? viewHeight() : viewWidth();

    if (hasColumns() && !style()->hasInlineColumnAxis()) {
        if (Frame* frame = m_frameView->frame()) {
            if (Page* page = frame->page()) {
                if (frame == page->mainFrame()) {
                    if (int pageLength = page->pagination().pageLength)
                        height = pageLength;
                }
            }
        }
    }

    return height;
}

float RenderView::zoomFactor() const
{
    Frame* frame = m_frameView->frame();
    return frame ? frame->pageZoomFactor() : 1;
}

void RenderView::pushLayoutState(RenderObject* root)
{
    ASSERT(m_layoutStateDisableCount == 0);
    ASSERT(m_layoutState == 0);

    m_layoutState = new (renderArena()) LayoutState(root);
}

void RenderView::pushLayoutState(RenderFlowThread* flowThread, bool regionsChanged)
{
    m_layoutState = new (renderArena()) LayoutState(m_layoutState, flowThread, regionsChanged);
}

bool RenderView::shouldDisableLayoutStateForSubtree(RenderObject* renderer) const
{
    RenderObject* o = renderer;
    while (o) {
        if (o->hasColumns() || o->hasTransform() || o->hasReflection())
            return true;
        o = o->container();
    }
    return false;
}

void RenderView::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = document()->documentElement();
    if (node) {
        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        result.setLocalPoint(point);
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

#if USE(ACCELERATED_COMPOSITING)
bool RenderView::usesCompositing() const
{
    return m_compositor && m_compositor->inCompositingMode();
}

RenderLayerCompositor* RenderView::compositor()
{
    if (!m_compositor)
        m_compositor = adoptPtr(new RenderLayerCompositor(this));

    return m_compositor.get();
}
#endif

void RenderView::didMoveOnscreen()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_compositor)
        m_compositor->didMoveOnscreen();
#endif
}

void RenderView::willMoveOffscreen()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_compositor)
        m_compositor->willMoveOffscreen();
#endif
}

void RenderView::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    
    for (RenderObject* renderer = firstChild(); renderer; renderer = renderer->nextSibling()) {
        if (renderer->isRenderNamedFlowThread()) {
            RenderNamedFlowThread* flowRenderer = toRenderNamedFlowThread(renderer);
            flowRenderer->setStyle(RenderFlowThread::createFlowThreadStyle(style()));
        }
    }
}

bool RenderView::hasRenderNamedFlowThreads() const
{
    return m_flowThreadController && m_flowThreadController->hasRenderNamedFlowThreads();
}

FlowThreadController* RenderView::flowThreadController()
{
    if (!m_flowThreadController)
        m_flowThreadController = FlowThreadController::create(this);

    return m_flowThreadController.get();
}

RenderBlock::IntervalArena* RenderView::intervalArena()
{
    if (!m_intervalArena)
        m_intervalArena = IntervalArena::create();
    return m_intervalArena.get();
}

void RenderView::setFixedPositionedObjectsNeedLayout()
{
    ASSERT(m_frameView);

    PositionedObjectsListHashSet* positionedObjects = this->positionedObjects();
    if (!positionedObjects)
        return;

    PositionedObjectsListHashSet::const_iterator end = positionedObjects->end();
    for (PositionedObjectsListHashSet::const_iterator it = positionedObjects->begin(); it != end; ++it) {
        RenderBox* currBox = *it;
        currBox->setNeedsLayout(true);
    }
}

void RenderView::insertFixedPositionedObject(RenderBox* object)
{
    if (!m_positionedObjects)
        m_positionedObjects = adoptPtr(new PositionedObjectsListHashSet);

    m_positionedObjects->add(object);
}

void RenderView::removeFixedPositionedObject(RenderBox* object)
{
    if (!m_positionedObjects)
        return;

    m_positionedObjects->remove(object);
}

} // namespace WebCore
