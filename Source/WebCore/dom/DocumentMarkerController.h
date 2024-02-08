/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "DocumentMarker.h"
#include "Timer.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class Document;
class LayoutPoint;
class Node;
class RenderedDocumentMarker;
class Text;

struct SimpleRange;

enum class RemovePartiallyOverlappingMarker : bool { No, Yes };
enum class FilterMarkerResult : bool { Keep, Remove };

class DocumentMarkerController : public CanMakeCheckedPtr {
    WTF_MAKE_NONCOPYABLE(DocumentMarkerController); WTF_MAKE_FAST_ALLOCATED;
public:
    DocumentMarkerController(Document&);
    ~DocumentMarkerController();

    void detach();

    WEBCORE_EXPORT void addMarker(const SimpleRange&, DocumentMarker::Type, const DocumentMarker::Data& = { });
    void addMarker(Text&, unsigned startOffset, unsigned length, DocumentMarker::Type, DocumentMarker::Data&& = { });
    WEBCORE_EXPORT void addMarker(Node&, DocumentMarker&&);
    void addDraggedContentMarker(const SimpleRange&);

    void copyMarkers(Node& source, OffsetRange, Node& destination);
    bool hasMarkers() const;
    WEBCORE_EXPORT bool hasMarkers(const SimpleRange&, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers());

    // When a marker partially overlaps with range, if removePartiallyOverlappingMarkers is true, we completely
    // remove the marker. If the argument is false, we will adjust the span of the marker so that it retains
    // the portion that is outside of the range.
    WEBCORE_EXPORT void removeMarkers(const SimpleRange&, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers(), RemovePartiallyOverlappingMarker = RemovePartiallyOverlappingMarker::No);
    WEBCORE_EXPORT void removeMarkers(Node&, OffsetRange, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers(), const Function<FilterMarkerResult(const DocumentMarker&)>& filterFunction = nullptr, RemovePartiallyOverlappingMarker = RemovePartiallyOverlappingMarker::No);

    WEBCORE_EXPORT void filterMarkers(const SimpleRange&, const Function<FilterMarkerResult(const DocumentMarker&)>& filterFunction, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers(), RemovePartiallyOverlappingMarker = RemovePartiallyOverlappingMarker::No);

    WEBCORE_EXPORT void removeMarkers(OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers());
    void removeMarkers(Node&, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers());
    void repaintMarkers(OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers());
    void shiftMarkers(Node&, unsigned startOffset, int delta);

    void dismissMarkers(OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers());

    WEBCORE_EXPORT Vector<WeakPtr<RenderedDocumentMarker>> markersFor(Node&, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers()) const;
    WEBCORE_EXPORT Vector<WeakPtr<RenderedDocumentMarker>> markersInRange(const SimpleRange&, OptionSet<DocumentMarker::Type>);
    WEBCORE_EXPORT Vector<SimpleRange> rangesForMarkersInRange(const SimpleRange&, OptionSet<DocumentMarker::Type>);
    void clearDescriptionOnMarkersIntersectingRange(const SimpleRange&, OptionSet<DocumentMarker::Type>);

    WEBCORE_EXPORT void updateRectsForInvalidatedMarkersOfType(DocumentMarker::Type);

    void invalidateRectsForAllMarkers();
    void invalidateRectsForMarkersInNode(Node&);

    WeakPtr<DocumentMarker> markerContainingPoint(const LayoutPoint&, DocumentMarker::Type);
    WEBCORE_EXPORT Vector<FloatRect> renderedRectsForMarkers(DocumentMarker::Type);

    WEBCORE_EXPORT void forEachOfTypes(OptionSet<DocumentMarker::Type>, const Function<bool(Node&, RenderedDocumentMarker&)>);

#if ENABLE(TREE_DEBUGGING)
    void showMarkers() const;
#endif

private:
    struct TextRange {
        Ref<Node> node;
        OffsetRange range;
    };
    Vector<TextRange> collectTextRanges(const SimpleRange&);

    void forEach(const SimpleRange&, OptionSet<DocumentMarker::Type>, const Function<bool(Node&, RenderedDocumentMarker&)>);

    using MarkerMap = HashMap<Ref<Node>, std::unique_ptr<Vector<RenderedDocumentMarker>>>;

    bool possiblyHasMarkers(OptionSet<DocumentMarker::Type>) const;
    void removeMarkers(OptionSet<DocumentMarker::Type>, const Function<FilterMarkerResult(const RenderedDocumentMarker&)>& filterFunction);
    OptionSet<DocumentMarker::Type> removeMarkersFromList(MarkerMap::iterator, OptionSet<DocumentMarker::Type>, const Function<FilterMarkerResult(const RenderedDocumentMarker&)>& filterFunction = nullptr);

    void fadeAnimationTimerFired();

    Ref<Document> protectedDocument() const;

    MarkerMap m_markers;
    // Provide a quick way to determine whether a particular marker type is absent without going through the map.
    OptionSet<DocumentMarker::Type> m_possiblyExistingMarkerTypes;
    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;

    Timer m_fadeAnimationTimer;
};

WEBCORE_EXPORT void addMarker(const SimpleRange&, DocumentMarker::Type, const DocumentMarker::Data& = { });
void addMarker(Text&, unsigned startOffset, unsigned length, DocumentMarker::Type, DocumentMarker::Data&& = { });
void removeMarkers(const SimpleRange&, OptionSet<DocumentMarker::Type> = DocumentMarker::allMarkers(), RemovePartiallyOverlappingMarker = RemovePartiallyOverlappingMarker::No);

WEBCORE_EXPORT SimpleRange makeSimpleRange(Node&, const DocumentMarker&);

inline bool DocumentMarkerController::hasMarkers() const
{
    ASSERT(m_markers.isEmpty() == !m_possiblyExistingMarkerTypes.containsAny(DocumentMarker::allMarkers()));
    return !m_markers.isEmpty();
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
void showDocumentMarkers(const WebCore::DocumentMarkerController*);
#endif
