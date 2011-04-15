/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 *
 */

#include "config.h"
#include "DocumentMarkerController.h"

#include "Node.h"
#include "Range.h"
#include "TextIterator.h"

namespace WebCore {

static IntRect placeholderRectForMarker()
{
    return IntRect(-1, -1, -1, -1);
}

inline bool DocumentMarkerController::possiblyHasMarkers(DocumentMarker::MarkerTypes types)
{
    return m_possiblyExistingMarkerTypes.intersects(types);
}

DocumentMarkerController::DocumentMarkerController()
    : m_possiblyExistingMarkerTypes(0)
{
}

void DocumentMarkerController::detach()
{
    m_possiblyExistingMarkerTypes = 0;
    if (m_markers.isEmpty())
        return;
    deleteAllValues(m_markers);
    m_markers.clear();
}

void DocumentMarkerController::addMarker(Range* range, DocumentMarker::MarkerType type, String description)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        int exception = 0;
        DocumentMarker marker = {type, textPiece->startOffset(exception), textPiece->endOffset(exception), description, false};
        addMarker(textPiece->startContainer(exception), marker);
    }
}

void DocumentMarkerController::removeMarkers(Range* range, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        if (!possiblyHasMarkers(markerTypes))
            return;
        ASSERT(!m_markers.isEmpty());

        RefPtr<Range> textPiece = markedText.range();
        int startOffset = textPiece->startOffset();
        int endOffset = textPiece->endOffset();
        removeMarkers(textPiece->startContainer(), startOffset, endOffset - startOffset, markerTypes, shouldRemovePartiallyOverlappingMarker);
    }
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void DocumentMarkerController::addMarker(Node* node, DocumentMarker newMarker) 
{
    ASSERT(newMarker.endOffset >= newMarker.startOffset);
    if (newMarker.endOffset == newMarker.startOffset)
        return;

    m_possiblyExistingMarkerTypes.add(newMarker.type);

    MarkerMapVectorPair* vectorPair = m_markers.get(node);

    if (!vectorPair) {
        vectorPair = new MarkerMapVectorPair;
        vectorPair->first.append(newMarker);
        vectorPair->second.append(placeholderRectForMarker());
        m_markers.set(node, vectorPair);
    } else {
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        size_t numMarkers = markers.size();
        ASSERT(numMarkers == rects.size());
        size_t i;
        // Iterate over all markers whose start offset is less than or equal to the new marker's.
        // If one of them is of the same type as the new marker and touches it or intersects with it
        // (there is at most one), remove it and adjust the new marker's start offset to encompass it.
        for (i = 0; i < numMarkers; ++i) {
            DocumentMarker marker = markers[i];
            if (marker.startOffset > newMarker.startOffset)
                break;
            if (marker.type == newMarker.type && marker.endOffset >= newMarker.startOffset) {
                newMarker.startOffset = marker.startOffset;
                markers.remove(i);
                rects.remove(i);
                numMarkers--;
                break;
            }
        }
        size_t j = i;
        // Iterate over all markers whose end offset is less than or equal to the new marker's,
        // removing markers of the same type as the new marker which touch it or intersect with it,
        // adjusting the new marker's end offset to cover them if necessary.
        while (j < numMarkers) {
            DocumentMarker marker = markers[j];
            if (marker.startOffset > newMarker.endOffset)
                break;
            if (marker.type == newMarker.type) {
                markers.remove(j);
                rects.remove(j);
                if (newMarker.endOffset <= marker.endOffset) {
                    newMarker.endOffset = marker.endOffset;
                    break;
                }
                numMarkers--;
            } else
                j++;
        }
        // At this point i points to the node before which we want to insert.
        markers.insert(i, newMarker);
        rects.insert(i, placeholderRectForMarker());
    }

    // repaint the affected node
    if (node->renderer())
        node->renderer()->repaint();
}

// copies markers from srcNode to dstNode, applying the specified shift delta to the copies.  The shift is
// useful if, e.g., the caller has created the dstNode from a non-prefix substring of the srcNode.
void DocumentMarkerController::copyMarkers(Node* srcNode, unsigned startOffset, int length, Node* dstNode, int delta)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerMapVectorPair* vectorPair = m_markers.get(srcNode);
    if (!vectorPair)
        return;

    ASSERT(vectorPair->first.size() == vectorPair->second.size());

    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    Vector<DocumentMarker>& markers = vectorPair->first;
    for (size_t i = 0; i != markers.size(); ++i) {
        DocumentMarker marker = markers[i];

        // stop if we are now past the specified range
        if (marker.startOffset > endOffset)
            break;

        // skip marker that is before the specified range or is the wrong type
        if (marker.endOffset < startOffset)
            continue;

        // pin the marker to the specified range and apply the shift delta
        docDirty = true;
        if (marker.startOffset < startOffset)
            marker.startOffset = startOffset;
        if (marker.endOffset > endOffset)
            marker.endOffset = endOffset;
        marker.startOffset += delta;
        marker.endOffset += delta;

        addMarker(dstNode, marker);
    }

    // repaint the affected node
    if (docDirty && dstNode->renderer())
        dstNode->renderer()->repaint();
}

void DocumentMarkerController::removeMarkers(Node* node, unsigned startOffset, int length, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!(m_markers.isEmpty()));

    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    Vector<IntRect>& rects = vectorPair->second;
    ASSERT(markers.size() == rects.size());
    bool docDirty = false;
    unsigned endOffset = startOffset + length;
    for (size_t i = 0; i < markers.size();) {
        DocumentMarker marker = markers[i];

        // markers are returned in order, so stop if we are now past the specified range
        if (marker.startOffset >= endOffset)
            break;

        // skip marker that is wrong type or before target
        if (marker.endOffset <= startOffset || !markerTypes.contains(marker.type)) {
            i++;
            continue;
        }

        // at this point we know that marker and target intersect in some way
        docDirty = true;

        // pitch the old marker and any associated rect
        markers.remove(i);
        rects.remove(i);

        if (shouldRemovePartiallyOverlappingMarker)
            // Stop here. Don't add resulting slices back.
            continue;

        // add either of the resulting slices that are left after removing target
        if (startOffset > marker.startOffset) {
            DocumentMarker newLeft = marker;
            newLeft.endOffset = startOffset;
            markers.insert(i, newLeft);
            rects.insert(i, placeholderRectForMarker());
            // i now points to the newly-inserted node, but we want to skip that one
            i++;
        }
        if (marker.endOffset > endOffset) {
            DocumentMarker newRight = marker;
            newRight.startOffset = endOffset;
            markers.insert(i, newRight);
            rects.insert(i, placeholderRectForMarker());
            // i now points to the newly-inserted node, but we want to skip that one
            i++;
        }
    }

    if (markers.isEmpty()) {
        ASSERT(rects.isEmpty());
        m_markers.remove(node);
        delete vectorPair;
    }
    
    if (m_markers.isEmpty())
        m_possiblyExistingMarkerTypes = 0;

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

DocumentMarker* DocumentMarkerController::markerContainingPoint(const IntPoint& point, DocumentMarker::MarkerType markerType)
{
    if (!possiblyHasMarkers(markerType))
        return 0;
    ASSERT(!(m_markers.isEmpty()));

    // outer loop: process each node that contains any markers
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        MarkerMapVectorPair* vectorPair = nodeIterator->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        ASSERT(markers.size() == rects.size());
        unsigned markerCount = markers.size();
        for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            DocumentMarker& marker = markers[markerIndex];

            // skip marker that is wrong type
            if (marker.type != markerType)
                continue;

            IntRect& r = rects[markerIndex];

            // skip placeholder rects
            if (r == placeholderRectForMarker())
                continue;

            if (r.contains(point))
                return &marker;
        }
    }

    return 0;
}

Vector<DocumentMarker> DocumentMarkerController::markersForNode(Node* node)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (vectorPair)
        return vectorPair->first;
    return Vector<DocumentMarker>();
}

Vector<DocumentMarker> DocumentMarkerController::markersInRange(Range* range, DocumentMarker::MarkerType markerType)
{
    if (!possiblyHasMarkers(markerType))
        return Vector<DocumentMarker>();

    Vector<DocumentMarker> foundMarkers;

    Node* startContainer = range->startContainer();
    ASSERT(startContainer);
    Node* endContainer = range->endContainer();
    ASSERT(endContainer);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        Vector<DocumentMarker> markers = markersForNode(node);
        Vector<DocumentMarker>::const_iterator end = markers.end();
        for (Vector<DocumentMarker>::const_iterator it = markers.begin(); it != end; ++it) {
            if (markerType != it->type)
                continue;
            if (node == startContainer && it->endOffset <= static_cast<unsigned>(range->startOffset()))
                continue;
            if (node == endContainer && it->startOffset >= static_cast<unsigned>(range->endOffset()))
                continue;
            foundMarkers.append(*it);
        }
    }
    return foundMarkers;
}

Vector<IntRect> DocumentMarkerController::renderedRectsForMarkers(DocumentMarker::MarkerType markerType)
{
    Vector<IntRect> result;

    if (!possiblyHasMarkers(markerType))
        return result;
    ASSERT(!(m_markers.isEmpty()));

    // outer loop: process each node
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        MarkerMapVectorPair* vectorPair = nodeIterator->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        ASSERT(markers.size() == rects.size());
        unsigned markerCount = markers.size();
        for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            DocumentMarker marker = markers[markerIndex];

            // skip marker that is wrong type
            if (marker.type != markerType)
                continue;

            IntRect r = rects[markerIndex];
            // skip placeholder rects
            if (r == placeholderRectForMarker())
                continue;

            result.append(r);
        }
    }

    return result;
}

void DocumentMarkerController::removeMarkers(Node* node, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());
    
    MarkerMap::iterator iterator = m_markers.find(node);
    if (iterator != m_markers.end())
        removeMarkersFromMarkerMapVectorPair(node, iterator->second, markerTypes);
}

void DocumentMarkerController::removeMarkers(DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    // outer loop: process each markered node in the document
    MarkerMap markerMapCopy = m_markers;
    MarkerMap::iterator end = markerMapCopy.end();
    for (MarkerMap::iterator i = markerMapCopy.begin(); i != end; ++i) {
        Node* node = i->first.get();
        MarkerMapVectorPair* vectorPair = i->second;
        removeMarkersFromMarkerMapVectorPair(node, vectorPair, markerTypes);
    }

    m_possiblyExistingMarkerTypes.remove(markerTypes);
}

// This function may release node and vectorPair.
void DocumentMarkerController::removeMarkersFromMarkerMapVectorPair(Node* node, MarkerMapVectorPair* vectorPair, DocumentMarker::MarkerTypes markerTypes)
{
    if (markerTypes == DocumentMarker::AllMarkers()) {
        delete vectorPair;
        m_markers.remove(node);
        if (RenderObject* renderer = node->renderer())
            renderer->repaint();
    } else {
        bool needsRepaint = false;
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        ASSERT(markers.size() == rects.size());
        for (size_t i = 0; i != markers.size();) {
            DocumentMarker marker = markers[i];

            // skip nodes that are not of the specified type
            if (!markerTypes.contains(marker.type)) {
                ++i;
                continue;
            }

            // pitch the old marker
            markers.remove(i);
            rects.remove(i);
            needsRepaint = true;
            // i now is the index of the next marker
        }

        // Redraw the node if it changed. Do this before the node is removed from m_markers, since
        // m_markers might contain the last reference to the node.
        if (needsRepaint) {
            RenderObject* renderer = node->renderer();
            if (renderer)
                renderer->repaint();
        }

        // delete the node's list if it is now empty
        if (markers.isEmpty()) {
            ASSERT(rects.isEmpty());
            m_markers.remove(node);
            delete vectorPair;
        }
    }
    if (m_markers.isEmpty())
        m_possiblyExistingMarkerTypes = 0;
}

void DocumentMarkerController::repaintMarkers(DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {
        Node* node = i->first.get();

        // inner loop: process each marker in the current node
        MarkerMapVectorPair* vectorPair = i->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        bool nodeNeedsRepaint = false;
        for (size_t i = 0; i != markers.size(); ++i) {
            DocumentMarker marker = markers[i];

            // skip nodes that are not of the specified type
            if (markerTypes.contains(marker.type)) {
                nodeNeedsRepaint = true;
                break;
            }
        }

        if (!nodeNeedsRepaint)
            continue;

        // cause the node to be redrawn
        if (RenderObject* renderer = node->renderer())
            renderer->repaint();
    }
}

void DocumentMarkerController::setRenderedRectForMarker(Node* node, const DocumentMarker& marker, const IntRect& r)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair) {
        ASSERT_NOT_REACHED(); // shouldn't be trying to set the rect for a marker we don't already know about
        return;
    }

    Vector<DocumentMarker>& markers = vectorPair->first;
    ASSERT(markers.size() == vectorPair->second.size());
    unsigned markerCount = markers.size();
    for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
        DocumentMarker m = markers[markerIndex];
        if (m == marker) {
            vectorPair->second[markerIndex] = r;
            return;
        }
    }

    ASSERT_NOT_REACHED(); // shouldn't be trying to set the rect for a marker we don't already know about
}

void DocumentMarkerController::invalidateRenderedRectsForMarkersInRect(const IntRect& r)
{
    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {

        // inner loop: process each rect in the current node
        MarkerMapVectorPair* vectorPair = i->second;
        Vector<IntRect>& rects = vectorPair->second;

        unsigned rectCount = rects.size();
        for (unsigned rectIndex = 0; rectIndex < rectCount; ++rectIndex)
            if (rects[rectIndex].intersects(r))
                rects[rectIndex] = placeholderRectForMarker();
    }
}

void DocumentMarkerController::shiftMarkers(Node* node, unsigned startOffset, int delta)
{
    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    Vector<IntRect>& rects = vectorPair->second;
    ASSERT(markers.size() == rects.size());

    bool docDirty = false;
    for (size_t i = 0; i != markers.size(); ++i) {
        DocumentMarker& marker = markers[i];
        if (marker.startOffset >= startOffset) {
            ASSERT((int)marker.startOffset + delta >= 0);
            marker.startOffset += delta;
            marker.endOffset += delta;
            docDirty = true;

            // Marker moved, so previously-computed rendered rectangle is now invalid
            rects[i] = placeholderRectForMarker();
        }
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

void DocumentMarkerController::setMarkersActive(Range* range, bool active)
{
    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    Node* endContainer = range->endContainer(ec);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        int startOffset = node == startContainer ? range->startOffset(ec) : 0;
        int endOffset = node == endContainer ? range->endOffset(ec) : INT_MAX;
        setMarkersActive(node, startOffset, endOffset, active);
    }
}

void DocumentMarkerController::setMarkersActive(Node* node, unsigned startOffset, unsigned endOffset, bool active)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    ASSERT(markers.size() == vectorPair->second.size());

    bool docDirty = false;
    for (size_t i = 0; i != markers.size(); ++i) {
        DocumentMarker& marker = markers[i];

        // Markers are returned in order, so stop if we are now past the specified range.
        if (marker.startOffset >= endOffset)
            break;

        // Skip marker that is wrong type or before target.
        if (marker.endOffset < startOffset || marker.type != DocumentMarker::TextMatch)
            continue;

        marker.activeMatch = active;
        docDirty = true;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

bool DocumentMarkerController::hasMarkers(Range* range, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return false;
    ASSERT(!m_markers.isEmpty());

    Node* startContainer = range->startContainer();
    ASSERT(startContainer);
    Node* endContainer = range->endContainer();
    ASSERT(endContainer);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        Vector<DocumentMarker> markers = markersForNode(node);
        Vector<DocumentMarker>::const_iterator end = markers.end();
        for (Vector<DocumentMarker>::const_iterator it = markers.begin(); it != end; ++it) {
            if (!markerTypes.contains(it->type))
                continue;
            if (node == startContainer && it->endOffset <= static_cast<unsigned>(range->startOffset()))
                continue;
            if (node == endContainer && it->startOffset >= static_cast<unsigned>(range->endOffset()))
                continue;
            return true;
        }
    }
    return false;
}

void DocumentMarkerController::clearDescriptionOnMarkersIntersectingRange(Range* range, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    Node* startContainer = range->startContainer();
    Node* endContainer = range->endContainer();

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        unsigned startOffset = node == startContainer ? range->startOffset() : 0;
        unsigned endOffset = node == endContainer ? static_cast<unsigned>(range->endOffset()) : std::numeric_limits<unsigned>::max();
        MarkerMapVectorPair* vectorPair = m_markers.get(node);
        if (!vectorPair)
            continue;

        Vector<DocumentMarker>& markers = vectorPair->first;
        for (size_t i = 0; i < markers.size(); ++i) {
            DocumentMarker& marker = markers[i];

            // markers are returned in order, so stop if we are now past the specified range
            if (marker.startOffset >= endOffset)
                break;

            // skip marker that is wrong type or before target
            if (marker.endOffset <= startOffset || !markerTypes.contains(marker.type)) {
                i++;
                continue;
            }

            marker.description = String();
        }
    }
}

#ifndef NDEBUG
void DocumentMarkerController::showMarkers() const
{
    fprintf(stderr, "%d nodes have markers:\n", m_markers.size());
    MarkerMap::const_iterator end = m_markers.end();
    for (MarkerMap::const_iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        Node* node = nodeIterator->first.get();
        fprintf(stderr, "%p", node);
        MarkerMapVectorPair* vectorPair = nodeIterator->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        unsigned markerCount = markers.size();
        for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex)
            fprintf(stderr, " %d:[%d:%d](%d)", markers[markerIndex].type, markers[markerIndex].startOffset, markers[markerIndex].endOffset, markers[markerIndex].activeMatch);
        fprintf(stderr, "\n");
    }
}
#endif

} // namespace WebCore


#ifndef NDEBUG
void showDocumentMarkers(const WebCore::DocumentMarkerController* controller)
{
    if (controller)
        controller->showMarkers();
}
#endif
