/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SpatialNavigation.h"

#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLMapElement.h"
#include "HTMLSelectElement.h"
#include "IntRect.h"
#include "Node.h"
#include "Page.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "Settings.h"

namespace WebCore {

static bool areRectsFullyAligned(FocusDirection, const LayoutRect&, const LayoutRect&);
static bool areRectsPartiallyAligned(FocusDirection, const LayoutRect&, const LayoutRect&);
static bool areRectsMoreThanFullScreenApart(FocusDirection, const LayoutRect& curRect, const LayoutRect& targetRect, const LayoutSize& viewSize);
static bool isRectInDirection(FocusDirection, const LayoutRect&, const LayoutRect&);
static void deflateIfOverlapped(LayoutRect&, LayoutRect&);
static LayoutRect rectToAbsoluteCoordinates(Frame* initialFrame, const LayoutRect&);
static void entryAndExitPointsForDirection(FocusDirection, const LayoutRect& startingRect, const LayoutRect& potentialRect, LayoutPoint& exitPoint, LayoutPoint& entryPoint);
static bool isScrollableNode(const Node*);

FocusCandidate::FocusCandidate(Node* node, FocusDirection direction)
    : visibleNode(nullptr)
    , focusableNode(nullptr)
    , enclosingScrollableBox(nullptr)
    , distance(maxDistance())
    , alignment(None)
    , isOffscreen(true)
    , isOffscreenAfterScrolling(true)
{
    ASSERT(is<Element>(node));

    if (is<HTMLAreaElement>(*node)) {
        HTMLAreaElement& area = downcast<HTMLAreaElement>(*node);
        HTMLImageElement* image = area.imageElement();
        if (!image || !image->renderer())
            return;

        visibleNode = image;
        rect = virtualRectForAreaElementAndDirection(&area, direction);
    } else {
        if (!node->renderer())
            return;

        visibleNode = node;
        rect = nodeRectInAbsoluteCoordinates(node, true /* ignore border */);
    }

    focusableNode = node;
    isOffscreen = hasOffscreenRect(visibleNode);
    isOffscreenAfterScrolling = hasOffscreenRect(visibleNode, direction);
}

bool isSpatialNavigationEnabled(const Frame* frame)
{
    return (frame && frame->settings().spatialNavigationEnabled());
}

static RectsAlignment alignmentForRects(FocusDirection direction, const LayoutRect& curRect, const LayoutRect& targetRect, const LayoutSize& viewSize)
{
    // If we found a node in full alignment, but it is too far away, ignore it.
    if (areRectsMoreThanFullScreenApart(direction, curRect, targetRect, viewSize))
        return None;

    if (areRectsFullyAligned(direction, curRect, targetRect))
        return Full;

    if (areRectsPartiallyAligned(direction, curRect, targetRect))
        return Partial;

    return None;
}

static inline bool isHorizontalMove(FocusDirection direction)
{
    return direction == FocusDirection::Left || direction == FocusDirection::Right;
}

static inline LayoutUnit start(FocusDirection direction, const LayoutRect& rect)
{
    return isHorizontalMove(direction) ? rect.y() : rect.x();
}

static inline LayoutUnit middle(FocusDirection direction, const LayoutRect& rect)
{
    LayoutPoint center(rect.center());
    return isHorizontalMove(direction) ? center.y(): center.x();
}

static inline LayoutUnit end(FocusDirection direction, const LayoutRect& rect)
{
    return isHorizontalMove(direction) ? rect.maxY() : rect.maxX();
}

// This method checks if rects |a| and |b| are fully aligned either vertically or
// horizontally. In general, rects whose central point falls between the top or
// bottom of each other are considered fully aligned.
// Rects that match this criteria are preferable target nodes in move focus changing
// operations.
// * a = Current focused node's rect.
// * b = Focus candidate node's rect.
static bool areRectsFullyAligned(FocusDirection direction, const LayoutRect& a, const LayoutRect& b)
{
    LayoutUnit aStart, bStart, aEnd, bEnd;

    switch (direction) {
    case FocusDirection::Left:
        aStart = a.x();
        bEnd = b.maxX();
        break;
    case FocusDirection::Right:
        aStart = b.x();
        bEnd = a.maxX();
        break;
    case FocusDirection::Up:
        aStart = a.y();
        bEnd = b.y();
        break;
    case FocusDirection::Down:
        aStart = b.y();
        bEnd = a.y();
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    if (aStart < bEnd)
        return false;

    aStart = start(direction, a);
    bStart = start(direction, b);

    LayoutUnit aMiddle = middle(direction, a);
    LayoutUnit bMiddle = middle(direction, b);

    aEnd = end(direction, a);
    bEnd = end(direction, b);

    // Picture of the totally aligned logic:
    //
    //     Horizontal    Vertical        Horizontal     Vertical
    //  ****************************  *****************************
    //  *  _          *   _ _ _ _  *  *         _   *      _ _    *
    //  * |_|     _   *  |_|_|_|_| *  *  _     |_|  *     |_|_|   *
    //  * |_|....|_|  *      .     *  * |_|....|_|  *       .     *
    //  * |_|    |_| (1)     .     *  * |_|    |_| (2)      .     *
    //  * |_|         *     _._    *  *        |_|  *    _ _._ _  *
    //  *             *    |_|_|   *  *             *   |_|_|_|_| *
    //  *             *            *  *             *             *
    //  ****************************  *****************************

    //     Horizontal    Vertical        Horizontal     Vertical
    //  ****************************  *****************************
    //  *  _......_   *   _ _ _ _  *  *  _          *    _ _ _ _  *
    //  * |_|    |_|  *  |_|_|_|_| *  * |_|     _   *   |_|_|_|_| *
    //  * |_|    |_|  *  .         *  * |_|    |_|  *           . *
    //  * |_|        (3) .         *  * |_|....|_| (4)          . *
    //  *             *  ._ _      *  *             *        _ _. *
    //  *             *  |_|_|     *  *             *       |_|_| *
    //  *             *            *  *             *             *
    //  ****************************  *****************************

    return ((bMiddle >= aStart && bMiddle <= aEnd) // (1)
            || (aMiddle >= bStart && aMiddle <= bEnd) // (2)
            || (bStart == aStart) // (3)
            || (bEnd == aEnd)); // (4)
}

// This method checks if |start| and |dest| have a partial intersection, either
// horizontally or vertically.
// * a = Current focused node's rect.
// * b = Focus candidate node's rect.
static bool areRectsPartiallyAligned(FocusDirection direction, const LayoutRect& a, const LayoutRect& b)
{
    LayoutUnit aStart  = start(direction, a);
    LayoutUnit bStart  = start(direction, b);
    LayoutUnit bMiddle = middle(direction, b);
    LayoutUnit aEnd = end(direction, a);
    LayoutUnit bEnd = end(direction, b);

    // Picture of the partially aligned logic:
    //
    //    Horizontal       Vertical
    // ********************************
    // *  _            *   _ _ _      *
    // * |_|           *  |_|_|_|     *
    // * |_|.... _     *      . .     *
    // * |_|    |_|    *      . .     *
    // * |_|....|_|    *      ._._ _  *
    // *        |_|    *      |_|_|_| *
    // *        |_|    *              *
    // *               *              *
    // ********************************
    //
    // ... and variants of the above cases.
    return ((bStart >= aStart && bStart <= aEnd)
            || (bMiddle >= aStart && bMiddle <= aEnd)
            || (bEnd >= aStart && bEnd <= aEnd));
}

static bool areRectsMoreThanFullScreenApart(FocusDirection direction, const LayoutRect& curRect, const LayoutRect& targetRect, const LayoutSize& viewSize)
{
    ASSERT(isRectInDirection(direction, curRect, targetRect));

    switch (direction) {
    case FocusDirection::Left:
        return curRect.x() - targetRect.maxX() > viewSize.width();
    case FocusDirection::Right:
        return targetRect.x() - curRect.maxX() > viewSize.width();
    case FocusDirection::Up:
        return curRect.y() - targetRect.maxY() > viewSize.height();
    case FocusDirection::Down:
        return targetRect.y() - curRect.maxY() > viewSize.height();
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

// Return true if rect |a| is below |b|. False otherwise.
static inline bool below(const LayoutRect& a, const LayoutRect& b)
{
    return a.y() > b.maxY();
}

// Return true if rect |a| is on the right of |b|. False otherwise.
static inline bool rightOf(const LayoutRect& a, const LayoutRect& b)
{
    return a.x() > b.maxX();
}

static bool isRectInDirection(FocusDirection direction, const LayoutRect& curRect, const LayoutRect& targetRect)
{
    switch (direction) {
    case FocusDirection::Left:
        return targetRect.maxX() <= curRect.x();
    case FocusDirection::Right:
        return targetRect.x() >= curRect.maxX();
    case FocusDirection::Up:
        return targetRect.maxY() <= curRect.y();
    case FocusDirection::Down:
        return targetRect.y() >= curRect.maxY();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

// Checks if |node| is offscreen the visible area (viewport) of its container
// document. In case it is, one can scroll in direction or take any different
// desired action later on.
bool hasOffscreenRect(Node* node, FocusDirection direction)
{
    // Get the FrameView in which |node| is (which means the current viewport if |node|
    // is not in an inner document), so we can check if its content rect is visible
    // before we actually move the focus to it.
    FrameView* frameView = node->document().view();
    if (!frameView)
        return true;

    ASSERT(!frameView->needsLayout());

    LayoutRect containerViewportRect = frameView->visibleContentRect();
    // We want to select a node if it is currently off screen, but will be
    // exposed after we scroll. Adjust the viewport to post-scrolling position.
    // If the container has overflow:hidden, we cannot scroll, so we do not pass direction
    // and we do not adjust for scrolling.
    switch (direction) {
    case FocusDirection::Left:
        containerViewportRect.setX(containerViewportRect.x() - Scrollbar::pixelsPerLineStep());
        containerViewportRect.setWidth(containerViewportRect.width() + Scrollbar::pixelsPerLineStep());
        break;
    case FocusDirection::Right:
        containerViewportRect.setWidth(containerViewportRect.width() + Scrollbar::pixelsPerLineStep());
        break;
    case FocusDirection::Up:
        containerViewportRect.setY(containerViewportRect.y() - Scrollbar::pixelsPerLineStep());
        containerViewportRect.setHeight(containerViewportRect.height() + Scrollbar::pixelsPerLineStep());
        break;
    case FocusDirection::Down:
        containerViewportRect.setHeight(containerViewportRect.height() + Scrollbar::pixelsPerLineStep());
        break;
    default:
        break;
    }

    RenderObject* render = node->renderer();
    if (!render)
        return true;

    LayoutRect rect(render->absoluteClippedOverflowRectForSpatialNavigation());
    if (rect.isEmpty())
        return true;

    return !containerViewportRect.intersects(rect);
}

bool scrollInDirection(Frame* frame, FocusDirection direction)
{
    ASSERT(frame);

    if (frame && canScrollInDirection(frame->document(), direction)) {
        LayoutUnit dx;
        LayoutUnit dy;
        switch (direction) {
        case FocusDirection::Left:
            dx = - Scrollbar::pixelsPerLineStep();
            break;
        case FocusDirection::Right:
            dx = Scrollbar::pixelsPerLineStep();
            break;
        case FocusDirection::Up:
            dy = - Scrollbar::pixelsPerLineStep();
            break;
        case FocusDirection::Down:
            dy = Scrollbar::pixelsPerLineStep();
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }

        frame->view()->scrollBy(IntSize(dx, dy));
        return true;
    }
    return false;
}

bool scrollInDirection(Node* container, FocusDirection direction)
{
    ASSERT(container);
    if (is<Document>(*container))
        return scrollInDirection(downcast<Document>(*container).frame(), direction);

    if (!container->renderBox())
        return false;

    if (canScrollInDirection(container, direction)) {
        LayoutUnit dx;
        LayoutUnit dy;
        switch (direction) {
        case FocusDirection::Left:
            dx = - std::min<LayoutUnit>(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollLeft());
            break;
        case FocusDirection::Right:
            ASSERT(container->renderBox()->scrollWidth() > (container->renderBox()->scrollLeft() + container->renderBox()->clientWidth()));
            dx = std::min<LayoutUnit>(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollWidth() - (container->renderBox()->scrollLeft() + container->renderBox()->clientWidth()));
            break;
        case FocusDirection::Up:
            dy = - std::min<LayoutUnit>(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollTop());
            break;
        case FocusDirection::Down:
            ASSERT(container->renderBox()->scrollHeight() - (container->renderBox()->scrollTop() + container->renderBox()->clientHeight()));
            dy = std::min<LayoutUnit>(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollHeight() - (container->renderBox()->scrollTop() + container->renderBox()->clientHeight()));
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }

        if (auto* scrollableArea = container->renderBox()->enclosingLayer()->scrollableArea())
            scrollableArea->scrollByRecursively(IntSize(dx, dy));
        return true;
    }

    return false;
}

static void deflateIfOverlapped(LayoutRect& a, LayoutRect& b)
{
    if (!a.intersects(b) || a.contains(b) || b.contains(a))
        return;

    LayoutUnit deflateFactor = -fudgeFactor();

    // Avoid negative width or height values.
    if ((a.width() + 2 * deflateFactor > 0) && (a.height() + 2 * deflateFactor > 0))
        a.inflate(deflateFactor);

    if ((b.width() + 2 * deflateFactor > 0) && (b.height() + 2 * deflateFactor > 0))
        b.inflate(deflateFactor);
}

bool isScrollableNode(const Node* node)
{
    if (!node)
        return false;
    ASSERT(!node->isDocumentNode());
    auto* renderer = node->renderer();
    return is<RenderBox>(renderer) && downcast<RenderBox>(*renderer).canBeScrolledAndHasScrollableArea() && node->hasChildNodes();
}

Node* scrollableEnclosingBoxOrParentFrameForNodeInDirection(FocusDirection direction, Node* node)
{
    ASSERT(node);
    Node* parent = node;
    do {
        if (is<Document>(*parent))
            parent = downcast<Document>(*parent).document().frame()->ownerElement();
        else
            parent = parent->parentNode();
    } while (parent && !canScrollInDirection(parent, direction) && !is<Document>(*parent));

    return parent;
}

bool canScrollInDirection(const Node* container, FocusDirection direction)
{
    ASSERT(container);

    if (is<HTMLSelectElement>(*container))
        return false;

    if (is<Document>(*container))
        return canScrollInDirection(downcast<Document>(*container).frame(), direction);

    if (!isScrollableNode(container))
        return false;

    switch (direction) {
    case FocusDirection::Left:
        return (container->renderer()->style().overflowX() != Overflow::Hidden && container->renderBox()->scrollLeft() > 0);
    case FocusDirection::Up:
        return (container->renderer()->style().overflowY() != Overflow::Hidden && container->renderBox()->scrollTop() > 0);
    case FocusDirection::Right:
        return (container->renderer()->style().overflowX() != Overflow::Hidden && container->renderBox()->scrollLeft() + container->renderBox()->clientWidth() < container->renderBox()->scrollWidth());
    case FocusDirection::Down:
        return (container->renderer()->style().overflowY() != Overflow::Hidden && container->renderBox()->scrollTop() + container->renderBox()->clientHeight() < container->renderBox()->scrollHeight());
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool canScrollInDirection(const Frame* frame, FocusDirection direction)
{
    if (!frame->view())
        return false;
    ScrollbarMode verticalMode;
    ScrollbarMode horizontalMode;
    frame->view()->calculateScrollbarModesForLayout(horizontalMode, verticalMode);
    if ((direction == FocusDirection::Left || direction == FocusDirection::Right) && ScrollbarAlwaysOff == horizontalMode)
        return false;
    if ((direction == FocusDirection::Up || direction == FocusDirection::Down) &&  ScrollbarAlwaysOff == verticalMode)
        return false;
    LayoutSize size = frame->view()->totalContentsSize();
    LayoutPoint scrollPosition = frame->view()->scrollPosition();
    LayoutRect rect = frame->view()->unobscuredContentRectIncludingScrollbars();

    // FIXME: wrong in RTL documents.
    switch (direction) {
    case FocusDirection::Left:
        return scrollPosition.x() > 0;
    case FocusDirection::Up:
        return scrollPosition.y() > 0;
    case FocusDirection::Right:
        return rect.width() + scrollPosition.x() < size.width();
    case FocusDirection::Down:
        return rect.height() + scrollPosition.y() < size.height();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

// FIXME: This is completely broken. This should be deleted and callers should be calling ScrollView::contentsToWindow() instead.
static LayoutRect rectToAbsoluteCoordinates(Frame* initialFrame, const LayoutRect& initialRect)
{
    LayoutRect rect = initialRect;
    for (Frame* frame = initialFrame; frame; frame = frame->tree().parent()) {
        if (Element* element = frame->ownerElement()) {
            do {
                rect.move(LayoutUnit(element->offsetLeft()), LayoutUnit(element->offsetTop()));
            } while ((element = element->offsetParent()));
            rect.moveBy((-frame->view()->scrollPosition()));
        }
    }
    return rect;
}

LayoutRect nodeRectInAbsoluteCoordinates(Node* node, bool ignoreBorder)
{
    ASSERT(node && node->renderer() && !node->document().view()->needsLayout());

    if (is<Document>(*node))
        return frameRectInAbsoluteCoordinates(downcast<Document>(*node).frame());

    LayoutRect rect;
    if (RenderObject* renderer = node->renderer())
        rect = rectToAbsoluteCoordinates(node->document().frame(), renderer->absoluteBoundingBoxRect());

    // For authors that use border instead of outline in their CSS, we compensate by ignoring the border when calculating
    // the rect of the focused element.
    if (ignoreBorder) {
        rect.move(node->renderer()->style().borderLeftWidth(), node->renderer()->style().borderTopWidth());
        rect.setWidth(rect.width() - node->renderer()->style().borderLeftWidth() - node->renderer()->style().borderRightWidth());
        rect.setHeight(rect.height() - node->renderer()->style().borderTopWidth() - node->renderer()->style().borderBottomWidth());
    }
    return rect;
}

LayoutRect frameRectInAbsoluteCoordinates(Frame* frame)
{
    return rectToAbsoluteCoordinates(frame, frame->view()->visibleContentRect());
}

// This method calculates the exitPoint from the startingRect and the entryPoint into the candidate rect.
// The line between those 2 points is the closest distance between the 2 rects.
void entryAndExitPointsForDirection(FocusDirection direction, const LayoutRect& startingRect, const LayoutRect& potentialRect, LayoutPoint& exitPoint, LayoutPoint& entryPoint)
{
    switch (direction) {
    case FocusDirection::Left:
        exitPoint.setX(startingRect.x());
        entryPoint.setX(potentialRect.maxX());
        break;
    case FocusDirection::Up:
        exitPoint.setY(startingRect.y());
        entryPoint.setY(potentialRect.maxY());
        break;
    case FocusDirection::Right:
        exitPoint.setX(startingRect.maxX());
        entryPoint.setX(potentialRect.x());
        break;
    case FocusDirection::Down:
        exitPoint.setY(startingRect.maxY());
        entryPoint.setY(potentialRect.y());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (direction) {
    case FocusDirection::Left:
    case FocusDirection::Right:
        if (below(startingRect, potentialRect)) {
            exitPoint.setY(startingRect.y());
            entryPoint.setY(potentialRect.maxY());
        } else if (below(potentialRect, startingRect)) {
            exitPoint.setY(startingRect.maxY());
            entryPoint.setY(potentialRect.y());
        } else {
            exitPoint.setY(std::max(startingRect.y(), potentialRect.y()));
            entryPoint.setY(exitPoint.y());
        }
        break;
    case FocusDirection::Up:
    case FocusDirection::Down:
        if (rightOf(startingRect, potentialRect)) {
            exitPoint.setX(startingRect.x());
            entryPoint.setX(potentialRect.maxX());
        } else if (rightOf(potentialRect, startingRect)) {
            exitPoint.setX(startingRect.maxX());
            entryPoint.setX(potentialRect.x());
        } else {
            exitPoint.setX(std::max(startingRect.x(), potentialRect.x()));
            entryPoint.setX(exitPoint.x());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool areElementsOnSameLine(const FocusCandidate& firstCandidate, const FocusCandidate& secondCandidate)
{
    if (firstCandidate.isNull() || secondCandidate.isNull())
        return false;

    if (!firstCandidate.visibleNode->renderer() || !secondCandidate.visibleNode->renderer())
        return false;

    if (!firstCandidate.rect.intersects(secondCandidate.rect))
        return false;

    if (is<HTMLAreaElement>(*firstCandidate.focusableNode) || is<HTMLAreaElement>(*secondCandidate.focusableNode))
        return false;

    if (!firstCandidate.visibleNode->renderer()->isRenderInline() || !secondCandidate.visibleNode->renderer()->isRenderInline())
        return false;

    if (firstCandidate.visibleNode->renderer()->containingBlock() != secondCandidate.visibleNode->renderer()->containingBlock())
        return false;

    return true;
}

// Consider only those nodes as candidate which are exactly in the focus-direction.
// e.g. If we are moving down then the nodes that are above current focused node should be considered as invalid.
bool isValidCandidate(FocusDirection direction, const FocusCandidate& current, FocusCandidate& candidate)
{
    LayoutRect currentRect = current.rect;
    LayoutRect candidateRect = candidate.rect;

    switch (direction) {
    case FocusDirection::Left:
        return candidateRect.x() < currentRect.maxX();
    case FocusDirection::Up:
        return candidateRect.y() < currentRect.maxY();
    case FocusDirection::Right:
        return candidateRect.maxX() > currentRect.x();
    case FocusDirection::Down:
        return candidateRect.maxY() > currentRect.y();
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void distanceDataForNode(FocusDirection direction, const FocusCandidate& current, FocusCandidate& candidate)
{
    if (areElementsOnSameLine(current, candidate)) {
        if ((direction == FocusDirection::Up && current.rect.y() > candidate.rect.y()) || (direction == FocusDirection::Down && candidate.rect.y() > current.rect.y())) {
            candidate.distance = 0;
            candidate.alignment = Full;
            return;
        }
    }

    LayoutRect nodeRect = candidate.rect;
    LayoutRect currentRect = current.rect;
    deflateIfOverlapped(currentRect, nodeRect);

    if (!isRectInDirection(direction, currentRect, nodeRect))
        return;

    LayoutPoint exitPoint;
    LayoutPoint entryPoint;
    LayoutUnit sameAxisDistance;
    LayoutUnit otherAxisDistance;
    entryAndExitPointsForDirection(direction, currentRect, nodeRect, exitPoint, entryPoint);

    switch (direction) {
    case FocusDirection::Left:
        sameAxisDistance = exitPoint.x() - entryPoint.x();
        otherAxisDistance = absoluteValue(exitPoint.y() - entryPoint.y());
        break;
    case FocusDirection::Up:
        sameAxisDistance = exitPoint.y() - entryPoint.y();
        otherAxisDistance = absoluteValue(exitPoint.x() - entryPoint.x());
        break;
    case FocusDirection::Right:
        sameAxisDistance = entryPoint.x() - exitPoint.x();
        otherAxisDistance = absoluteValue(entryPoint.y() - exitPoint.y());
        break;
    case FocusDirection::Down:
        sameAxisDistance = entryPoint.y() - exitPoint.y();
        otherAxisDistance = absoluteValue(entryPoint.x() - exitPoint.x());
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    float x = (entryPoint.x() - exitPoint.x()) * (entryPoint.x() - exitPoint.x());
    float y = (entryPoint.y() - exitPoint.y()) * (entryPoint.y() - exitPoint.y());

    float euclidianDistance = sqrt(x + y);

    // Loosely based on http://www.w3.org/TR/WICD/#focus-handling
    // df = dotDist + dx + dy + 2 * (xdisplacement + ydisplacement) - sqrt(Overlap)

    float distance = euclidianDistance + sameAxisDistance + 2 * otherAxisDistance;
    candidate.distance = roundf(distance);
    LayoutSize viewSize = candidate.visibleNode->document().page()->mainFrame().view()->visibleContentRect().size();
    candidate.alignment = alignmentForRects(direction, currentRect, nodeRect, viewSize);
}

bool canBeScrolledIntoView(FocusDirection direction, const FocusCandidate& candidate)
{
    ASSERT(candidate.visibleNode && candidate.isOffscreen);
    LayoutRect candidateRect = candidate.rect;
    for (Node* parentNode = candidate.visibleNode->parentNode(); parentNode; parentNode = parentNode->parentNode()) {
        if (!parentNode->renderer())
            continue;
        LayoutRect parentRect = nodeRectInAbsoluteCoordinates(parentNode);
        if (!candidateRect.intersects(parentRect)) {
            if (((direction == FocusDirection::Left || direction == FocusDirection::Right) && parentNode->renderer()->style().overflowX() == Overflow::Hidden)
                || ((direction == FocusDirection::Up || direction == FocusDirection::Down) && parentNode->renderer()->style().overflowY() == Overflow::Hidden))
                return false;
        }
        if (parentNode == candidate.enclosingScrollableBox)
            return canScrollInDirection(parentNode, direction);
    }
    return true;
}

// The starting rect is the rect of the focused node, in document coordinates.
// Compose a virtual starting rect if there is no focused node or if it is off screen.
// The virtual rect is the edge of the container or frame. We select which
// edge depending on the direction of the navigation.
LayoutRect virtualRectForDirection(FocusDirection direction, const LayoutRect& startingRect, LayoutUnit width)
{
    LayoutRect virtualStartingRect = startingRect;
    switch (direction) {
    case FocusDirection::Left:
        virtualStartingRect.setX(virtualStartingRect.maxX() - width);
        virtualStartingRect.setWidth(width);
        break;
    case FocusDirection::Up:
        virtualStartingRect.setY(virtualStartingRect.maxY() - width);
        virtualStartingRect.setHeight(width);
        break;
    case FocusDirection::Right:
        virtualStartingRect.setWidth(width);
        break;
    case FocusDirection::Down:
        virtualStartingRect.setHeight(width);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return virtualStartingRect;
}

LayoutRect virtualRectForAreaElementAndDirection(HTMLAreaElement* area, FocusDirection direction)
{
    ASSERT(area);
    ASSERT(area->imageElement());
    // Area elements tend to overlap more than other focusable elements. We flatten the rect of the area elements
    // to minimize the effect of overlapping areas.
    LayoutRect rect = virtualRectForDirection(direction, rectToAbsoluteCoordinates(area->document().frame(), area->computeRect(area->imageElement()->renderer())), 1);
    return rect;
}

HTMLFrameOwnerElement* frameOwnerElement(FocusCandidate& candidate)
{
    return candidate.isFrameOwnerElement() ? downcast<HTMLFrameOwnerElement>(candidate.visibleNode) : nullptr;
}

} // namespace WebCore
