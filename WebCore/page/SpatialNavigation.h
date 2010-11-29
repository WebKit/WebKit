/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
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

#ifndef SpatialNavigation_h
#define SpatialNavigation_h

#include "FocusDirection.h"
#include "IntRect.h"
#include "Node.h"

#include <limits>

namespace WebCore {

class Element;
class Frame;
class IntRect;
class RenderObject;

using namespace std;

inline long long maxDistance()
{
    return numeric_limits<long long>::max();
}

inline int fudgeFactor()
{
    return 2;
}

bool isSpatialNavigationEnabled(const Frame*);

// Spatially speaking, two given elements in a web page can be:
// 1) Fully aligned: There is a full intersection between the rects, either
//    vertically or horizontally.
//
// * Horizontally       * Vertically
//    _
//   |_|                   _ _ _ _ _ _
//   |_|...... _          |_|_|_|_|_|_|
//   |_|      |_|         .       .
//   |_|......|_|   OR    .       .
//   |_|      |_|         .       .
//   |_|......|_|          _ _ _ _
//   |_|                  |_|_|_|_|
//
//
// 2) Partially aligned: There is a partial intersection between the rects, either
//    vertically or horizontally.
//
// * Horizontally       * Vertically
//    _                   _ _ _ _ _
//   |_|                 |_|_|_|_|_|
//   |_|.... _      OR           . .
//   |_|    |_|                  . .
//   |_|....|_|                  ._._ _
//          |_|                  |_|_|_|
//          |_|
//
// 3) Or, otherwise, not aligned at all.
//
// * Horizontally       * Vertically
//         _              _ _ _ _
//        |_|            |_|_|_|_|
//        |_|                    .
//        |_|                     .
//       .          OR             .
//    _ .                           ._ _ _ _ _
//   |_|                            |_|_|_|_|_|
//   |_|
//   |_|
//
// "Totally Aligned" elements are preferable candidates to move
// focus to over "Partially Aligned" ones, that on its turns are
// more preferable than "Not Aligned".
enum RectsAlignment {
    None = 0,
    Partial,
    Full
};

struct FocusCandidate {
    FocusCandidate()
        : node(0)
        , enclosingScrollableBox(0)
        , distance(maxDistance())
        , parentDistance(maxDistance())
        , alignment(None)
        , parentAlignment(None)
        , isOffscreen(true)
        , isOffscreenAfterScrolling(true)
    {
    }

    FocusCandidate(Node* n, FocusDirection);
    bool isNull() const { return !node; }
    bool inScrollableContainer() const { return node && enclosingScrollableBox; }
    Document* document() const { return node ? node->document() : 0; }

    Node* node;
    Node* enclosingScrollableBox;
    long long distance;
    long long parentDistance;
    RectsAlignment alignment;
    RectsAlignment parentAlignment;
    IntRect rect;
    bool isOffscreen;
    bool isOffscreenAfterScrolling;
};

bool scrollInDirection(Frame*, FocusDirection);
bool scrollInDirection(Node* container, FocusDirection);
bool hasOffscreenRect(Node*, FocusDirection direction = FocusDirectionNone);
bool isScrollableContainerNode(const Node*);
bool isNodeDeepDescendantOfDocument(Node*, Document*);
Node* scrollableEnclosingBoxOrParentFrameForNodeInDirection(FocusDirection, Node* node);
bool canScrollInDirection(FocusDirection, const Node* container);
bool canScrollInDirection(FocusDirection, const Frame*);
IntRect nodeRectInAbsoluteCoordinates(Node*, bool ignoreBorder = false);
IntRect frameRectInAbsoluteCoordinates(Frame*);
void distanceDataForNode(FocusDirection, FocusCandidate& current, FocusCandidate& candidate);
bool canBeScrolledIntoView(FocusDirection, const FocusCandidate&);
IntRect virtualRectForDirection(FocusDirection, const IntRect& startingRect);
} // namspace WebCore

#endif // SpatialNavigation_h
