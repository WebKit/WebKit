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

#ifndef DocumentMarkerController_h
#define DocumentMarkerController_h

#include "DocumentMarker.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class IntPoint;
class IntRect;
class Node;
class Range;

class DocumentMarkerController : public Noncopyable {
public:
    ~DocumentMarkerController() { detach(); }

    void detach();
    void addMarker(Range*, DocumentMarker::MarkerType, String description = String());
    void addMarker(Node*, DocumentMarker);
    void copyMarkers(Node* srcNode, unsigned startOffset, int length, Node* dstNode, int delta, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    bool hasMarkers(Range*, DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers);
    void removeMarkers(Range*, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(Node*, unsigned startOffset, int length, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(Node*, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void repaintMarkers(DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void setRenderedRectForMarker(Node*, const DocumentMarker&, const IntRect&);
    void invalidateRenderedRectsForMarkersInRect(const IntRect&);
    void shiftMarkers(Node*, unsigned startOffset, int delta, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void setMarkersActive(Range*, bool);
    void setMarkersActive(Node*, unsigned startOffset, unsigned endOffset, bool);

    DocumentMarker* markerContainingPoint(const IntPoint&, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    Vector<DocumentMarker> markersForNode(Node*);
    Vector<IntRect> renderedRectsForMarkers(DocumentMarker::MarkerType = DocumentMarker::AllMarkers);

private:
    typedef std::pair<Vector<DocumentMarker>, Vector<IntRect> > MarkerMapVectorPair;
    typedef HashMap<RefPtr<Node>, MarkerMapVectorPair*> MarkerMap;
    MarkerMap m_markers;
    void removeMarkersFromMarkerMapVectorPair(Node*, MarkerMapVectorPair*, DocumentMarker::MarkerType);
};

} // namespace WebCore

#endif // DocumentMarkerController_h
