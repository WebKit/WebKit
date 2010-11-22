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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "HTMLFrameOwnerElement.h"
#include "IntRect.h"
#include "Node.h"
#include "Page.h"
#include "RenderLayer.h"
#include "Settings.h"

namespace WebCore {

static long long spatialDistance(FocusDirection, const IntRect&, const IntRect&);
static IntRect renderRectRelativeToRootDocument(RenderObject*);
static RectsAlignment alignmentForRects(FocusDirection, const IntRect&, const IntRect&, const IntSize& viewSize);
static bool areRectsFullyAligned(FocusDirection, const IntRect&, const IntRect&);
static bool areRectsPartiallyAligned(FocusDirection, const IntRect&, const IntRect&);
static bool areRectsMoreThanFullScreenApart(FocusDirection direction, const IntRect& curRect, const IntRect& targetRect, const IntSize& viewSize);
static bool isRectInDirection(FocusDirection, const IntRect&, const IntRect&);
static void deflateIfOverlapped(IntRect&, IntRect&);
static bool checkNegativeCoordsForNode(Node*, const IntRect&);
static IntRect rectToAbsoluteCoordinates(Frame* initialFrame, const IntRect& rect);
static void entryAndExitPointsForDirection(FocusDirection direction, const IntRect& startingRect, const IntRect& potentialRect, IntPoint& exitPoint, IntPoint& entryPoint);


FocusCandidate::FocusCandidate(Node* n)
    : node(n)
    , enclosingScrollableBox(0)
    , distance(maxDistance())
    , parentDistance(maxDistance())
    , alignment(None)
    , parentAlignment(None)
    , rect(nodeRectInAbsoluteCoordinates(n, true /* ignore border */))
{
}

bool isSpatialNavigationEnabled(const Frame* frame)
{
    return (frame && frame->settings() && frame->settings()->isSpatialNavigationEnabled());
}

void distanceDataForNode(FocusDirection direction, Node* start, FocusCandidate& candidate)
{
    RenderObject* startRender = start->renderer();
    if (!startRender) {
        candidate.distance = maxDistance();
        return;
    }

    RenderObject* destRender = candidate.node->renderer();
    if (!destRender) {
        candidate.distance = maxDistance();
        return;
    }

    IntRect curRect = renderRectRelativeToRootDocument(startRender);
    IntRect targetRect  = renderRectRelativeToRootDocument(destRender);

    // The bounding rectangle of two consecutive nodes can overlap. In such cases,
    // deflate both.
    deflateIfOverlapped(curRect, targetRect);

    // If empty rects or negative width or height, bail out.
    if (curRect.isEmpty() || targetRect.isEmpty()
     || targetRect.width() <= 0 || targetRect.height() <= 0) {
        candidate.distance = maxDistance();
        return;
    }

    // Negative coordinates can be used if node is scrolled up offscreen.
    if (!checkNegativeCoordsForNode(start, curRect)) {
        candidate.distance = maxDistance();
        return;
    }

    if (!checkNegativeCoordsForNode(candidate.node, targetRect)) {
        candidate.distance = maxDistance();
        return;
    }

    if (!isRectInDirection(direction, curRect, targetRect)) {
        candidate.distance = maxDistance();
        return;
    }

    // The distance between two nodes is not to be considered alone when evaluating/looking
    // for the best focus candidate node. Alignment of rects can be also a good point to be
    // considered in order to make the algorithm to behavior in a more intuitive way.
    IntSize viewSize = candidate.node->document()->page()->mainFrame()->view()->visibleContentRect().size();
    candidate.alignment = alignmentForRects(direction, curRect, targetRect, viewSize);
    candidate.distance = spatialDistance(direction, curRect, targetRect);
}

// FIXME: This function does not behave correctly with transformed frames.
static IntRect renderRectRelativeToRootDocument(RenderObject* render)
{
    ASSERT(render && render->node());

    IntRect rect = render->node()->getRect();

    // In cases when the |render|'s associated node is in a scrollable inner
    // document, we only consider its scrollOffset if it is not offscreen.
    Node* node = render->node();
    Document* mainDocument = node->document()->page()->mainFrame()->document();
    bool considerScrollOffset = !(hasOffscreenRect(node) && node->document() != mainDocument);

    if (considerScrollOffset) {
        if (FrameView* frameView = render->node()->document()->view())
            rect.move(-frameView->scrollOffset());
    }

    // Handle nested frames.
    for (Frame* frame = render->document()->frame(); frame; frame = frame->tree()->parent()) {
        if (Element* element = static_cast<Element*>(frame->ownerElement())) {
            do {
                rect.move(element->offsetLeft(), element->offsetTop());
            } while ((element = element->offsetParent()));
        }
    }

    return rect;
}

static RectsAlignment alignmentForRects(FocusDirection direction, const IntRect& curRect, const IntRect& targetRect, const IntSize& viewSize)
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
    return direction == FocusDirectionLeft || direction == FocusDirectionRight;
}

static inline int start(FocusDirection direction, const IntRect& rect)
{
    return isHorizontalMove(direction) ? rect.y() : rect.x();
}

static inline int middle(FocusDirection direction, const IntRect& rect)
{
    IntPoint center(rect.center());
    return isHorizontalMove(direction) ? center.y(): center.x();
}

static inline int end(FocusDirection direction, const IntRect& rect)
{
    return isHorizontalMove(direction) ? rect.bottom() : rect.right();
}

// This method checks if rects |a| and |b| are fully aligned either vertically or
// horizontally. In general, rects whose central point falls between the top or
// bottom of each other are considered fully aligned.
// Rects that match this criteria are preferable target nodes in move focus changing
// operations.
// * a = Current focused node's rect.
// * b = Focus candidate node's rect.
static bool areRectsFullyAligned(FocusDirection direction, const IntRect& a, const IntRect& b)
{
    int aStart, bStart, aEnd, bEnd;

    switch (direction) {
    case FocusDirectionLeft:
        aStart = a.x();
        bEnd = b.right();
        break;
    case FocusDirectionRight:
        aStart = b.x();
        bEnd = a.right();
        break;
    case FocusDirectionUp:
        aStart = a.y();
        bEnd = b.y();
        break;
    case FocusDirectionDown:
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

    int aMiddle = middle(direction, a);
    int bMiddle = middle(direction, b);

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
static bool areRectsPartiallyAligned(FocusDirection direction, const IntRect& a, const IntRect& b)
{
    int aStart  = start(direction, a);
    int bStart  = start(direction, b);
    int bMiddle = middle(direction, b);
    int aEnd = end(direction, a);
    int bEnd = end(direction, b);

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
            || (bStart >= aStart && bStart <= aEnd)
            || (bEnd >= aStart && bEnd <= aEnd)
            || (bMiddle >= aStart && bMiddle <= aEnd)
            || (bEnd >= aStart && bEnd <= aEnd));
}

static bool areRectsMoreThanFullScreenApart(FocusDirection direction, const IntRect& curRect, const IntRect& targetRect, const IntSize& viewSize)
{
    ASSERT(isRectInDirection(direction, curRect, targetRect));

    switch (direction) {
    case FocusDirectionLeft:
        return curRect.x() - targetRect.right() > viewSize.width();
    case FocusDirectionRight:
        return targetRect.x() - curRect.right() > viewSize.width();
    case FocusDirectionUp:
        return curRect.y() - targetRect.bottom() > viewSize.height();
    case FocusDirectionDown:
        return targetRect.y() - curRect.bottom() > viewSize.height();
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

// Return true if rect |a| is below |b|. False otherwise.
static inline bool below(const IntRect& a, const IntRect& b)
{
    return a.y() > b.bottom();
}

// Return true if rect |a| is on the right of |b|. False otherwise.
static inline bool rightOf(const IntRect& a, const IntRect& b)
{
    return a.x() > b.right();
}

// * a = Current focused node's rect.
// * b = Focus candidate node's rect.
static long long spatialDistance(FocusDirection direction, const IntRect& a, const IntRect& b)
{
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    if (direction == FocusDirectionLeft) {
        // #1  |--|
        //
        // #2  |--|  |--|
        //
        // #3  |--|

        x1 = a.x();
        x2 = b.right();

        if (below(a, b)) {
            // #1 The a rect is below b.
            y1 = a.y();
            y2 = b.bottom();
        } else if (below(b, a)) {
            // #3 The b rect is below a.
            y1 = a.bottom();
            y2 = b.y();
        } else {
            // #2 Both b and a share some common y's.
            y1 = 0;
            y2 = 0;
        }
    } else if (direction == FocusDirectionRight) {
        //        |--|  #1
        //
        //  |--|  |--|  #2
        //
        //        |--|  #3

        x1 = a.right();
        x2 = b.x();

        if (below(a, b)) {
            // #1 The b rect is above a.
            y1 = a.y();
            y2 = b.bottom();
        } else if (below(b, a)) {
            // #3 The b rect is below a.
            y1 = a.bottom();
            y2 = b.y();
        } else {
            // #2 Both b and a share some common y's.
            y1 = 0;
            y2 = 0;
        }
    } else if (direction == FocusDirectionUp) {
        //
        //   #1    #2    #3
        //
        //  |--|  |--|  |--|
        //
        //        |--|

        y1 = a.y();
        y2 = b.bottom();

        if (rightOf(a, b)) {
            // #1 The b rect is to the left of a.
            x1 = a.x();
            x2 = b.right();
        } else if (rightOf(b, a)) {
            // #3 The b rect is to the right of a.
            x1 = a.right();
            x2 = b.x();
        } else {
            // #2 Both b and a share some common x's.
            x1 = 0;
            x2 = 0;
        }
    } else if (direction == FocusDirectionDown) {
        //        |--|
        //
        //  |--|  |--|  |--|
        //
        //   #1    #2    #3

        y1 = a.bottom();
        y2 = b.y();

        if (rightOf(a, b)) {
            // #1 The b rect is to the left of a.
            x1 = a.x();
            x2 = b.right();
        } else if (rightOf(b, a)) {
            // #3 The b rect is to the right of a
            x1 = a.right();
            x2 = b.x();
        } else {
            // #2 Both b and a share some common x's.
            x1 = 0;
            x2 = 0;
        }
    }

    long long dx = x1 - x2;
    long long dy = y1 - y2;

    long long distance = (dx * dx) + (dy * dy);

    if (distance < 0)
        distance *= -1;

    return distance;
}

static bool isRectInDirection(FocusDirection direction, const IntRect& curRect, const IntRect& targetRect)
{
    switch (direction) {
    case FocusDirectionLeft:
        return targetRect.right() <= curRect.x();
    case FocusDirectionRight:
        return targetRect.x() >= curRect.right();
    case FocusDirectionUp:
        return targetRect.bottom() <= curRect.y();
    case FocusDirectionDown:
        return targetRect.y() >= curRect.bottom();
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
    FrameView* frameView = node->document()->view();
    if (!frameView)
        return true;

    IntRect containerViewportRect = frameView->visibleContentRect();
    // We want to select a node if it is currently off screen, but will be
    // exposed after we scroll. Adjust the viewport to post-scrolling position.
    // If the container has overflow:hidden, we cannot scroll, so we do not pass direction
    // and we do not adjust for scrolling.
    switch (direction) {
    case FocusDirectionLeft:
        containerViewportRect.setX(containerViewportRect.x() - Scrollbar::pixelsPerLineStep());
        containerViewportRect.setWidth(containerViewportRect.width() + Scrollbar::pixelsPerLineStep());
        break;
    case FocusDirectionRight:
        containerViewportRect.setWidth(containerViewportRect.width() + Scrollbar::pixelsPerLineStep());
        break;
    case FocusDirectionUp:
        containerViewportRect.setY(containerViewportRect.y() - Scrollbar::pixelsPerLineStep());
        containerViewportRect.setHeight(containerViewportRect.height() + Scrollbar::pixelsPerLineStep());
        break;
    case FocusDirectionDown:
        containerViewportRect.setHeight(containerViewportRect.height() + Scrollbar::pixelsPerLineStep());
        break;
    default:
        break;
    }

    RenderObject* render = node->renderer();
    if (!render)
        return true;

    IntRect rect(render->absoluteClippedOverflowRect());
    if (rect.isEmpty())
        return true;

    return !containerViewportRect.intersects(rect);
}

bool scrollInDirection(Frame* frame, FocusDirection direction)
{
    ASSERT(frame && canScrollInDirection(direction, frame->document()));

    if (frame && canScrollInDirection(direction, frame->document())) {
        int dx = 0;
        int dy = 0;
        switch (direction) {
        case FocusDirectionLeft:
            dx = - Scrollbar::pixelsPerLineStep();
            break;
        case FocusDirectionRight:
            dx = Scrollbar::pixelsPerLineStep();
            break;
        case FocusDirectionUp:
            dy = - Scrollbar::pixelsPerLineStep();
            break;
        case FocusDirectionDown:
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
    if (container->isDocumentNode())
        return scrollInDirection(static_cast<Document*>(container)->frame(), direction);

    if (!container->renderBox())
        return false;

    if (container && canScrollInDirection(direction, container)) {
        int dx = 0;
        int dy = 0;
        switch (direction) {
        case FocusDirectionLeft:
            dx = - min(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollLeft());
            break;
        case FocusDirectionRight:
            ASSERT(container->renderBox()->scrollWidth() > (container->renderBox()->scrollLeft() + container->renderBox()->clientWidth()));
            dx = min(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollWidth() - (container->renderBox()->scrollLeft() + container->renderBox()->clientWidth()));
            break;
        case FocusDirectionUp:
            dy = - min(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollTop());
            break;
        case FocusDirectionDown:
            ASSERT(container->renderBox()->scrollHeight() - (container->renderBox()->scrollTop() + container->renderBox()->clientHeight()));
            dy = min(Scrollbar::pixelsPerLineStep(), container->renderBox()->scrollHeight() - (container->renderBox()->scrollTop() + container->renderBox()->clientHeight()));
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }

        container->renderBox()->enclosingLayer()->scrollByRecursively(dx, dy);
        return true;
    }
    return false;
}

void scrollIntoView(Element* element)
{
    // NOTE: Element's scrollIntoView method could had been used here, but
    // it is preferable to inflate |element|'s bounding rect a bit before
    // scrolling it for accurate reason.
    // Element's scrollIntoView method does not provide this flexibility.
    IntRect bounds = element->getRect();
    bounds.inflate(fudgeFactor());
    element->renderer()->enclosingLayer()->scrollRectToVisible(bounds);
}

bool isInRootDocument(Node* node)
{
    if (!node)
        return false;

    Document* rootDocument = node->document()->page()->mainFrame()->document();
    return node->document() == rootDocument;
}

static void deflateIfOverlapped(IntRect& a, IntRect& b)
{
    if (!a.intersects(b) || a.contains(b) || b.contains(a))
        return;

    int deflateFactor = -fudgeFactor();

    // Avoid negative width or height values.
    if ((a.width() + 2 * deflateFactor > 0) && (a.height() + 2 * deflateFactor > 0))
        a.inflate(deflateFactor);

    if ((b.width() + 2 * deflateFactor > 0) && (b.height() + 2 * deflateFactor > 0))
        b.inflate(deflateFactor);
}

static bool checkNegativeCoordsForNode(Node* node, const IntRect& curRect)
{
    ASSERT(node || node->renderer());

    if (curRect.x() >= 0 && curRect.y() >= 0)
        return true;

    bool canBeScrolled = false;

    RenderObject* renderer = node->renderer();
    for (; renderer; renderer = renderer->parent()) {
        if (renderer->isBox() && toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()) {
            canBeScrolled = true;
            break;
        }
    }

    return canBeScrolled;
}

bool isScrollableContainerNode(const Node* node)
{
    if (!node)
        return false;

    if (RenderObject* renderer = node->renderer()) {
        return (renderer->isBox() && toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()
             && node->hasChildNodes() && !node->isDocumentNode());
    }

    return false;
}

bool isNodeDeepDescendantOfDocument(Node* node, Document* baseDocument)
{
    if (!node || !baseDocument)
        return false;

    bool descendant = baseDocument == node->document();

    Element* currentElement = static_cast<Element*>(node);
    while (!descendant) {
        Element* documentOwner = currentElement->document()->ownerElement();
        if (!documentOwner)
            break;

        descendant = documentOwner->document() == baseDocument;
        currentElement = documentOwner;
    }

    return descendant;
}

Node* scrollableEnclosingBoxOrParentFrameForNodeInDirection(FocusDirection direction, Node* node)
{
    ASSERT(node);
    Node* parent = node;
    do {
        if (parent->isDocumentNode())
            parent = static_cast<Document*>(parent)->document()->frame()->ownerElement();
        else
            parent = parent->parentNode();
    } while (parent && !canScrollInDirection(direction, parent) && !parent->isDocumentNode());

    return parent;
}

bool canScrollInDirection(FocusDirection direction, const Node* container)
{
    ASSERT(container);
    if (container->isDocumentNode())
        return canScrollInDirection(direction, static_cast<const Document*>(container)->frame());

    if (!isScrollableContainerNode(container))
        return false;

    switch (direction) {
    case FocusDirectionLeft:
        return (container->renderer()->style()->overflowX() != OHIDDEN && container->renderBox()->scrollLeft() > 0);
    case FocusDirectionUp:
        return (container->renderer()->style()->overflowY() != OHIDDEN && container->renderBox()->scrollTop() > 0);
    case FocusDirectionRight:
        return (container->renderer()->style()->overflowX() != OHIDDEN && container->renderBox()->scrollLeft() + container->renderBox()->clientWidth() < container->renderBox()->scrollWidth());
    case FocusDirectionDown:
        return (container->renderer()->style()->overflowY() != OHIDDEN && container->renderBox()->scrollTop() + container->renderBox()->clientHeight() < container->renderBox()->scrollHeight());
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool canScrollInDirection(FocusDirection direction, const Frame* frame)
{
    if (!frame->view())
        return false;
    ScrollbarMode verticalMode;
    ScrollbarMode horizontalMode;
    frame->view()->calculateScrollbarModesForLayout(horizontalMode, verticalMode);
    if ((direction == FocusDirectionLeft || direction == FocusDirectionRight) && ScrollbarAlwaysOff == horizontalMode)
        return false;
    if ((direction == FocusDirectionUp || direction == FocusDirectionDown) &&  ScrollbarAlwaysOff == verticalMode)
        return false;
    IntSize size = frame->view()->contentsSize();
    IntSize offset = frame->view()->scrollOffset();
    IntRect rect = frame->view()->visibleContentRect(true);

    switch (direction) {
    case FocusDirectionLeft:
        return offset.width() > 0;
    case FocusDirectionUp:
        return offset.height() > 0;
    case FocusDirectionRight:
        return rect.width() + offset.width() < size.width();
    case FocusDirectionDown:
        return rect.height() + offset.height() < size.height();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static IntRect rectToAbsoluteCoordinates(Frame* initialFrame, const IntRect& initialRect)
{
    IntRect rect = initialRect;
    for (Frame* frame = initialFrame; frame; frame = frame->tree()->parent()) {
        if (Element* element = static_cast<Element*>(frame->ownerElement())) {
            do {
                rect.move(element->offsetLeft(), element->offsetTop());
            } while ((element = element->offsetParent()));
            rect.move((-frame->view()->scrollOffset()));
        }
    }
    return rect;
}

IntRect nodeRectInAbsoluteCoordinates(Node* node, bool ignoreBorder)
{
    ASSERT(node && node->renderer());

    if (node->isDocumentNode())
        return frameRectInAbsoluteCoordinates(static_cast<Document*>(node)->frame());
    IntRect rect = rectToAbsoluteCoordinates(node->document()->frame(), node->getRect());

    // For authors that use border instead of outline in their CSS, we compensate by ignoring the border when calculating
    // the rect of the focused element.
    if (ignoreBorder) {
        rect.move(node->renderer()->style()->borderLeftWidth(), node->renderer()->style()->borderTopWidth());
        rect.setWidth(rect.width() - node->renderer()->style()->borderLeftWidth() - node->renderer()->style()->borderRightWidth());
        rect.setHeight(rect.height() - node->renderer()->style()->borderTopWidth() - node->renderer()->style()->borderBottomWidth());
    }
    return rect;
}

IntRect frameRectInAbsoluteCoordinates(Frame* frame)
{
    return rectToAbsoluteCoordinates(frame, frame->view()->visibleContentRect());
}

// This method calculates the exitPoint from the startingRect and the entryPoint into the candidate rect.
// The line between those 2 points is the closest distance between the 2 rects.
void entryAndExitPointsForDirection(FocusDirection direction, const IntRect& startingRect, const IntRect& potentialRect, IntPoint& exitPoint, IntPoint& entryPoint)
{
    switch (direction) {
    case FocusDirectionLeft:
        exitPoint.setX(startingRect.x());
        entryPoint.setX(potentialRect.right());
        break;
    case FocusDirectionUp:
        exitPoint.setY(startingRect.y());
        entryPoint.setY(potentialRect.bottom());
        break;
    case FocusDirectionRight:
        exitPoint.setX(startingRect.right());
        entryPoint.setX(potentialRect.x());
        break;
    case FocusDirectionDown:
        exitPoint.setY(startingRect.bottom());
        entryPoint.setY(potentialRect.y());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (direction) {
    case FocusDirectionLeft:
    case FocusDirectionRight:
        if (below(startingRect, potentialRect)) {
            exitPoint.setY(startingRect.y());
            entryPoint.setY(potentialRect.bottom());
        } else if (below(potentialRect, startingRect)) {
            exitPoint.setY(startingRect.bottom());
            entryPoint.setY(potentialRect.y());
        } else {
            exitPoint.setY(max(startingRect.y(), potentialRect.y()));
            entryPoint.setY(exitPoint.y());
        }
        break;
    case FocusDirectionUp:
    case FocusDirectionDown:
        if (rightOf(startingRect, potentialRect)) {
            exitPoint.setX(startingRect.x());
            entryPoint.setX(potentialRect.right());
        } else if (rightOf(potentialRect, startingRect)) {
            exitPoint.setX(startingRect.right());
            entryPoint.setX(potentialRect.x());
        } else {
            exitPoint.setX(max(startingRect.x(), potentialRect.x()));
            entryPoint.setX(exitPoint.x());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void distanceDataForNode(FocusDirection direction, FocusCandidate& current, FocusCandidate& candidate)
{
    if (candidate.isNull())
        return;
    if (!candidate.node->renderer())
        return;
    IntRect nodeRect = candidate.rect;
    IntRect currentRect = current.rect;
    deflateIfOverlapped(currentRect, nodeRect);

    if (!isRectInDirection(direction, currentRect, nodeRect))
        return;

    IntPoint exitPoint;
    IntPoint entryPoint;
    int sameAxisDistance = 0;
    int otherAxisDistance = 0;
    entryAndExitPointsForDirection(direction, currentRect, nodeRect, exitPoint, entryPoint);

    switch (direction) {
    case FocusDirectionLeft:
        sameAxisDistance = exitPoint.x() - entryPoint.x();
        otherAxisDistance = abs(exitPoint.y() - entryPoint.y());
        break;
    case FocusDirectionUp:
        sameAxisDistance = exitPoint.y() - entryPoint.y();
        otherAxisDistance = abs(exitPoint.x() - entryPoint.x());
        break;
    case FocusDirectionRight:
        sameAxisDistance = entryPoint.x() - exitPoint.x();
        otherAxisDistance = abs(entryPoint.y() - exitPoint.y());
        break;
    case FocusDirectionDown:
        sameAxisDistance = entryPoint.y() - exitPoint.y();
        otherAxisDistance = abs(entryPoint.x() - exitPoint.x());
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    int x = (entryPoint.x() - exitPoint.x()) * (entryPoint.x() - exitPoint.x());
    int y = (entryPoint.y() - exitPoint.y()) * (entryPoint.y() - exitPoint.y());

    float euclidianDistance = sqrt((x + y) * 1.0f);

    // Loosely based on http://www.w3.org/TR/WICD/#focus-handling
    // df = dotDist + dx + dy + 2 * (xdisplacement + ydisplacement) - sqrt(Overlap)

    float distance = euclidianDistance + sameAxisDistance + 2 * otherAxisDistance;
    candidate.distance = roundf(distance);
    IntSize viewSize = candidate.node->document()->page()->mainFrame()->view()->visibleContentRect().size();
    candidate.alignment = alignmentForRects(direction, currentRect, nodeRect, viewSize);
}

bool canBeScrolledIntoView(FocusDirection direction, FocusCandidate& candidate)
{
    ASSERT(candidate.node && hasOffscreenRect(candidate.node));
    IntRect candidateRect = candidate.rect;
    for (Node* parentNode = candidate.node->parent(); parentNode; parentNode = parentNode->parent()) {
        IntRect parentRect = nodeRectInAbsoluteCoordinates(parentNode);
        if (!candidateRect.intersects(parentRect)) {
            if (((direction == FocusDirectionLeft || direction == FocusDirectionRight) && parentNode->renderer()->style()->overflowX() == OHIDDEN)
                || ((direction == FocusDirectionUp || direction == FocusDirectionDown) && parentNode->renderer()->style()->overflowY() == OHIDDEN))
                return false;
        }
        if (parentNode == candidate.enclosingScrollableBox)
            return canScrollInDirection(direction, parentNode);
    }
    return true;
}

// The starting rect is the rect of the focused node, in document coordinates.
// Compose a virtual starting rect if there is no focused node or if it is off screen.
// The virtual rect is the edge of the container or frame. We select which
// edge depending on the direction of the navigation.
IntRect virtualRectForDirection(FocusDirection direction, const IntRect& startingRect)
{
    IntRect virtualStartingRect = startingRect;
    switch (direction) {
    case FocusDirectionLeft:
        virtualStartingRect.setX(virtualStartingRect.right());
        virtualStartingRect.setWidth(0);
        break;
    case FocusDirectionUp:
        virtualStartingRect.setY(virtualStartingRect.bottom());
        virtualStartingRect.setHeight(0);
        break;
    case FocusDirectionRight:
        virtualStartingRect.setWidth(0);
        break;
    case FocusDirectionDown:
        virtualStartingRect.setHeight(0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return virtualStartingRect;
}


} // namespace WebCore
