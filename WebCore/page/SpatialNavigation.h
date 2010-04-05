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
        , distance(maxDistance())
        , parentDistance(maxDistance())
        , alignment(None)
        , parentAlignment(None)
    {
    }

    FocusCandidate(Node* n)
        : node(n)
        , distance(maxDistance())
        , parentDistance(maxDistance())
        , alignment(None)
        , parentAlignment(None)
    {
    }

    bool isNull() const { return !node; }
    Document* document() const { return node ? node->document() : 0; }

    Node* node;
    long long distance;
    long long parentDistance;
    RectsAlignment alignment;
    RectsAlignment parentAlignment;
};

void distanceDataForNode(FocusDirection direction, Node* start, FocusCandidate& candidate);
bool scrollInDirection(Frame*, FocusDirection);
void scrollIntoView(Element*);
bool hasOffscreenRect(Node*);
bool isInRootDocument(Node*);

} // namspace WebCore

#endif // SpatialNavigation_h
