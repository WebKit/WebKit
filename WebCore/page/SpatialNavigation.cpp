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

namespace WebCore {

static long long spatialDistance(FocusDirection, const IntRect&, const IntRect&);
static IntRect renderRectRelativeToRootDocument(RenderObject*);
static RectsAlignment alignmentForRects(FocusDirection, const IntRect&, const IntRect&);
static bool areRectsFullyAligned(FocusDirection, const IntRect&, const IntRect&);
static bool areRectsPartiallyAligned(FocusDirection, const IntRect&, const IntRect&);
static bool isRectInDirection(FocusDirection, const IntRect&, const IntRect&);
static void deflateIfOverlapped(IntRect&, IntRect&);
static bool checkNegativeCoordsForNode(Node*, const IntRect&);

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
    candidate.alignment = alignmentForRects(direction, curRect, targetRect);
    candidate.distance = spatialDistance(direction, curRect, targetRect);
}

// FIXME: This function does not behave correctly with transformed frames.
static IntRect renderRectRelativeToRootDocument(RenderObject* render)
{
    ASSERT(render);

    IntRect rect(render->absoluteClippedOverflowRect());

    if (rect.isEmpty()) {
        Element* e = static_cast<Element*>(render->node());
        rect = e->getRect();
    }

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
        if (HTMLFrameOwnerElement* ownerElement = frame->ownerElement())
            rect.move(ownerElement->offsetLeft(), ownerElement->offsetTop());
    }

    return rect;
}

static RectsAlignment alignmentForRects(FocusDirection direction, const IntRect& curRect, const IntRect& targetRect)
{
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
    IntPoint center(targetRect.center());
    int targetMiddle = isHorizontalMove(direction) ? center.x() : center.y();

    switch (direction) {
    case FocusDirectionLeft:
        return targetMiddle < curRect.x();
    case FocusDirectionRight:
        return targetMiddle > curRect.right();
    case FocusDirectionUp:
        return targetMiddle < curRect.y();
    case FocusDirectionDown:
        return targetMiddle > curRect.bottom();
    default:
        ASSERT_NOT_REACHED();
    }

    return false;
}

// Checks if |node| is offscreen the visible area (viewport) of its container
// document. In case it is, one can scroll in direction or take any different
// desired action later on.
bool hasOffscreenRect(Node* node)
{
    // Get the FrameView in which |node| is (which means the current viewport if |node|
    // is not in an inner document), so we can check if its content rect is visible
    // before we actually move the focus to it.
    FrameView* frameView = node->document()->view();
    if (!frameView)
        return true;

    IntRect containerViewportRect = frameView->visibleContentRect();

    RenderObject* render = node->renderer();
    if (!render)
        return true;

    IntRect rect(render->absoluteClippedOverflowRect());
    if (rect.isEmpty())
        return true;

    return !containerViewportRect.intersects(rect);
}

// In a bottom-up way, this method tries to scroll |frame| in a given direction
// |direction|, going up in the frame tree hierarchy in case it does not succeed.
bool scrollInDirection(Frame* frame, FocusDirection direction)
{
    if (!frame)
        return false;

    ScrollDirection scrollDirection;

    switch (direction) {
    case FocusDirectionLeft:
        scrollDirection = ScrollLeft;
        break;
    case FocusDirectionRight:
        scrollDirection = ScrollRight;
        break;
    case FocusDirectionUp:
        scrollDirection = ScrollUp;
        break;
    case FocusDirectionDown:
        scrollDirection = ScrollDown;
        break;
    default:
        return false;
    }

    return frame->eventHandler()->scrollRecursively(scrollDirection, ScrollByLine);
}

void scrollIntoView(Element* element)
{
    // NOTE: Element's scrollIntoView method could had been used here, but
    // it is preferable to inflate |element|'s bounding rect a bit before
    // scrolling it for accurate reason.
    // Element's scrollIntoView method does not provide this flexibility.
    static const int fudgeFactor = 2;
    IntRect bounds = element->getRect();
    bounds.inflate(fudgeFactor);
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

    static const int fudgeFactor = -2;

    // Avoid negative width or height values.
    if ((a.width() + 2 * fudgeFactor > 0) && (a.height() + 2 * fudgeFactor > 0))
        a.inflate(fudgeFactor);

    if ((b.width() + 2 * fudgeFactor > 0) && (b.height() + 2 * fudgeFactor > 0))
        b.inflate(fudgeFactor);
}

static bool checkNegativeCoordsForNode(Node* node, const IntRect& curRect)
{
    ASSERT(node || node->renderer());

    if (curRect.x() > 0 && curRect.y() > 0)
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

} // namespace WebCore
