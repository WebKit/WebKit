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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "Range.h"
#include "RenderBlockFlow.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "RenderedDocumentMarker.h"
#include "TextIterator.h"
#include <stdio.h>

namespace WebCore {

inline bool DocumentMarkerController::possiblyHasMarkers(OptionSet<DocumentMarker::MarkerType> types)
{
    return m_possiblyExistingMarkerTypes.containsAny(types);
}

DocumentMarkerController::DocumentMarkerController(Document& document)
    : m_document(document)
{
}

DocumentMarkerController::~DocumentMarkerController() = default;

void DocumentMarkerController::detach()
{
    m_markers.clear();
    m_possiblyExistingMarkerTypes = { };
}

void DocumentMarkerController::addMarker(Range& range, DocumentMarker::MarkerType type, const String& description)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(type, textPiece->startOffset(), textPiece->endOffset(), description));
    }
}

void DocumentMarkerController::addMarker(Range& range, DocumentMarker::MarkerType type)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(type, textPiece->startOffset(), textPiece->endOffset()));
    }

}

void DocumentMarkerController::addMarkerToNode(Node& node, unsigned startOffset, unsigned length, DocumentMarker::MarkerType type)
{
    addMarker(node, DocumentMarker(type, startOffset, startOffset + length));
}

void DocumentMarkerController::addMarkerToNode(Node& node, unsigned startOffset, unsigned length, DocumentMarker::MarkerType type, DocumentMarker::Data&& data)
{
    addMarker(node, DocumentMarker(type, startOffset, startOffset + length, WTFMove(data)));
}

void DocumentMarkerController::addTextMatchMarker(const Range& range, bool activeMatch)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        unsigned startOffset = textPiece->startOffset();
        unsigned endOffset = textPiece->endOffset();
        addMarker(textPiece->startContainer(), DocumentMarker(startOffset, endOffset, activeMatch));
    }
}

#if PLATFORM(IOS_FAMILY)

void DocumentMarkerController::addMarker(Range& range, DocumentMarker::MarkerType type, const String& description, const Vector<String>& interpretations, const RetainPtr<id>& metadata)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(type, textPiece->startOffset(), textPiece->endOffset(), description, interpretations, metadata));
    }
}

void DocumentMarkerController::addDictationPhraseWithAlternativesMarker(Range& range, const Vector<String>& interpretations)
{
    ASSERT(interpretations.size() > 1);
    if (interpretations.size() <= 1)
        return;

    size_t numberOfAlternatives = interpretations.size() - 1;
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        DocumentMarker marker(DocumentMarker::DictationPhraseWithAlternatives, textPiece->startOffset(), textPiece->endOffset(), emptyString(), Vector<String>(numberOfAlternatives), RetainPtr<id>());
        for (size_t i = 0; i < numberOfAlternatives; ++i)
            marker.setAlternative(interpretations[i + 1], i);
        addMarker(textPiece->startContainer(), marker);
    }
}

void DocumentMarkerController::addDictationResultMarker(Range& range, const RetainPtr<id>& metadata)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(DocumentMarker::DictationResult, textPiece->startOffset(), textPiece->endOffset(), String(), Vector<String>(), metadata));
    }
}

#endif

void DocumentMarkerController::addDraggedContentMarker(Range& range)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        auto textPiece = markedText.range();
        DocumentMarker::DraggedContentData draggedContentData { markedText.node() };
        addMarker(textPiece->startContainer(), { DocumentMarker::DraggedContent, textPiece->startOffset(), textPiece->endOffset(), WTFMove(draggedContentData) });
    }
}

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
void DocumentMarkerController::addPlatformTextCheckingMarker(Range& range, const String& key, const String& value)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        auto textPiece = markedText.range();
        DocumentMarker::PlatformTextCheckingData textCheckingData { key, value };
        addMarker(textPiece->startContainer(), { DocumentMarker::PlatformTextChecking, textPiece->startOffset(), textPiece->endOffset(), WTFMove(textCheckingData) });
    }
}
#endif

void DocumentMarkerController::removeMarkers(Range& range, OptionSet<DocumentMarker::MarkerType> markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        if (!possiblyHasMarkers(markerTypes))
            return;
        ASSERT(!m_markers.isEmpty());

        auto textPiece = markedText.range();
        unsigned startOffset = textPiece->startOffset();
        unsigned endOffset = textPiece->endOffset();
        removeMarkers(textPiece->startContainer(), startOffset, endOffset - startOffset, markerTypes, nullptr, shouldRemovePartiallyOverlappingMarker);
    }
}

void DocumentMarkerController::filterMarkers(Range& range, std::function<bool(DocumentMarker*)> filterFunction, OptionSet<DocumentMarker::MarkerType> markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    for (TextIterator markedText(&range); !markedText.atEnd(); markedText.advance()) {
        if (!possiblyHasMarkers(markerTypes))
            return;
        ASSERT(!m_markers.isEmpty());

        auto textPiece = markedText.range();
        unsigned startOffset = textPiece->startOffset();
        unsigned endOffset = textPiece->endOffset();
        removeMarkers(textPiece->startContainer(), startOffset, endOffset - startOffset, markerTypes, filterFunction, shouldRemovePartiallyOverlappingMarker);
    }
}

static void updateRenderedRectsForMarker(RenderedDocumentMarker& marker, Node& node)
{
    ASSERT(!node.document().view() || !node.document().view()->needsLayout());

    // FIXME: We should refactor this so that we don't use Range (because we only have one Node), but still share code with absoluteTextQuads().
    auto markerRange = Range::create(node.document(), &node, marker.startOffset(), &node, marker.endOffset());
    Vector<FloatQuad> absoluteMarkerQuads;
    markerRange->absoluteTextQuads(absoluteMarkerQuads, true);

    Vector<FloatRect> absoluteMarkerRects;
    absoluteMarkerRects.reserveInitialCapacity(absoluteMarkerQuads.size());
    for (const auto& quad : absoluteMarkerQuads)
        absoluteMarkerRects.uncheckedAppend(quad.boundingBox());

    marker.setUnclippedAbsoluteRects(absoluteMarkerRects);
}

void DocumentMarkerController::invalidateRectsForAllMarkers()
{
    if (!hasMarkers())
        return;

    for (auto& markers : m_markers.values()) {
        for (auto& marker : *markers)
            marker.invalidate();
    }

    if (Page* page = m_document.page())
        page->chrome().client().didInvalidateDocumentMarkerRects();
}

void DocumentMarkerController::invalidateRectsForMarkersInNode(Node& node)
{
    if (!hasMarkers())
        return;

    MarkerList* markers = m_markers.get(&node);
    ASSERT(markers);

    for (auto& marker : *markers)
        marker.invalidate();

    if (Page* page = m_document.page())
        page->chrome().client().didInvalidateDocumentMarkerRects();
}

static void updateMainFrameLayoutIfNeeded(Document& document)
{
    Frame* frame = document.frame();
    if (!frame)
        return;

    FrameView* mainFrameView = frame->mainFrame().view();
    if (!mainFrameView)
        return;

    mainFrameView->updateLayoutAndStyleIfNeededRecursive();
}

void DocumentMarkerController::updateRectsForInvalidatedMarkersOfType(DocumentMarker::MarkerType markerType)
{
    if (!possiblyHasMarkers(markerType))
        return;
    ASSERT(!(m_markers.isEmpty()));

    bool needsLayoutIfAnyRectsAreDirty = true;

    for (auto& nodeAndMarkers : m_markers) {
        Node& node = *nodeAndMarkers.key;
        for (auto& marker : *nodeAndMarkers.value) {
            if (marker.type() != markerType)
                continue;

            if (marker.isValid())
                continue;

            // We'll do up to one layout per call if we have any dirty markers.
            if (needsLayoutIfAnyRectsAreDirty) {
                updateMainFrameLayoutIfNeeded(m_document);
                needsLayoutIfAnyRectsAreDirty = false;
            }

            updateRenderedRectsForMarker(marker, node);
        }
    }
}

Vector<FloatRect> DocumentMarkerController::renderedRectsForMarkers(DocumentMarker::MarkerType markerType)
{
    Vector<FloatRect> result;

    if (!possiblyHasMarkers(markerType))
        return result;
    ASSERT(!(m_markers.isEmpty()));

    RefPtr<Frame> frame = m_document.frame();
    if (!frame)
        return result;
    FrameView* frameView = frame->view();
    if (!frameView)
        return result;

    updateRectsForInvalidatedMarkersOfType(markerType);

    bool isSubframe = !frame->isMainFrame();
    IntRect subframeClipRect;
    if (isSubframe)
        subframeClipRect = frameView->windowToContents(frameView->windowClipRect());

    for (auto& nodeAndMarkers : m_markers) {
        Node& node = *nodeAndMarkers.key;
        FloatRect overflowClipRect;
        if (RenderObject* renderer = node.renderer())
            overflowClipRect = renderer->absoluteClippedOverflowRect();
        for (auto& marker : *nodeAndMarkers.value) {
            if (marker.type() != markerType)
                continue;

            auto renderedRects = marker.unclippedAbsoluteRects();

            // Clip document markers by their overflow clip.
            if (node.renderer()) {
                for (auto& rect : renderedRects)
                    rect.intersect(overflowClipRect);
            }

            // Clip subframe document markers by their frame.
            if (isSubframe) {
                for (auto& rect : renderedRects)
                    rect.intersect(subframeClipRect);
            }

            for (const auto& rect : renderedRects) {
                if (!rect.isEmpty())
                    result.append(rect);
            }
        }
    }
    
    return result;
}

static bool shouldInsertAsSeparateMarker(const DocumentMarker& newMarker)
{
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    if (newMarker.type() == DocumentMarker::PlatformTextChecking)
        return true;
#endif

#if PLATFORM(IOS_FAMILY)
    if (newMarker.type() == DocumentMarker::DictationPhraseWithAlternatives || newMarker.type() == DocumentMarker::DictationResult)
        return true;
#endif
    if (newMarker.type() == DocumentMarker::DraggedContent) {
        if (auto targetNode = WTF::get<DocumentMarker::DraggedContentData>(newMarker.data()).targetNode)
            return targetNode->renderer() && targetNode->renderer()->isRenderReplaced();
    }

    return false;
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void DocumentMarkerController::addMarker(Node& node, const DocumentMarker& newMarker)
{
    ASSERT(newMarker.endOffset() >= newMarker.startOffset());
    if (newMarker.endOffset() == newMarker.startOffset())
        return;

    if (auto* renderer = node.renderer()) {
        // FIXME: Factor the marker painting code out of InlineTextBox and teach simple line layout to use it.
        if (is<RenderText>(*renderer))
            downcast<RenderText>(*renderer).ensureLineBoxes();
        else if (is<RenderBlockFlow>(*renderer))
            downcast<RenderBlockFlow>(*renderer).ensureLineBoxes();
    }

    m_possiblyExistingMarkerTypes.add(newMarker.type());

    std::unique_ptr<MarkerList>& list = m_markers.add(&node, nullptr).iterator->value;

    if (!list) {
        list = makeUnique<MarkerList>();
        list->append(RenderedDocumentMarker(newMarker));
    } else if (shouldInsertAsSeparateMarker(newMarker)) {
        // We don't merge dictation markers.
        size_t i;
        size_t numberOfMarkers = list->size();
        for (i = 0; i < numberOfMarkers; ++i) {
            DocumentMarker marker = list->at(i);
            if (marker.startOffset() > newMarker.startOffset())
                break;
        }
        list->insert(i, RenderedDocumentMarker(newMarker));
    } else {
        RenderedDocumentMarker toInsert(newMarker);
        size_t numMarkers = list->size();
        size_t i;
        // Iterate over all markers whose start offset is less than or equal to the new marker's.
        // If one of them is of the same type as the new marker and touches it or intersects with it
        // (there is at most one), remove it and adjust the new marker's start offset to encompass it.
        for (i = 0; i < numMarkers; ++i) {
            DocumentMarker marker = list->at(i);
            if (marker.startOffset() > toInsert.startOffset())
                break;
            if (marker.type() == toInsert.type() && marker.endOffset() >= toInsert.startOffset()) {
                toInsert.setStartOffset(marker.startOffset());
                list->remove(i);
                numMarkers--;
                break;
            }
        }
        size_t j = i;
        // Iterate over all markers whose end offset is less than or equal to the new marker's,
        // removing markers of the same type as the new marker which touch it or intersect with it,
        // adjusting the new marker's end offset to cover them if necessary.
        while (j < numMarkers) {
            DocumentMarker marker = list->at(j);
            if (marker.startOffset() > toInsert.endOffset())
                break;
            if (marker.type() == toInsert.type()) {
                list->remove(j);
                if (toInsert.endOffset() <= marker.endOffset()) {
                    toInsert.setEndOffset(marker.endOffset());
                    break;
                }
                numMarkers--;
            } else
                j++;
        }
        // At this point i points to the node before which we want to insert.
        list->insert(i, RenderedDocumentMarker(toInsert));
    }

    if (node.renderer())
        node.renderer()->repaint();

    invalidateRectsForMarkersInNode(node);
}

// copies markers from srcNode to dstNode, applying the specified shift delta to the copies.  The shift is
// useful if, e.g., the caller has created the dstNode from a non-prefix substring of the srcNode.
void DocumentMarkerController::copyMarkers(Node& srcNode, unsigned startOffset, int length, Node& dstNode, int delta)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(DocumentMarker::allMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerList* list = m_markers.get(&srcNode);
    if (!list)
        return;

    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    for (auto& marker : *list) {
        // stop if we are now past the specified range
        if (marker.startOffset() > endOffset)
            break;

        // skip marker that is before the specified range or is the wrong type
        if (marker.endOffset() < startOffset)
            continue;

        // pin the marker to the specified range and apply the shift delta
        docDirty = true;
        if (marker.startOffset() < startOffset)
            marker.setStartOffset(startOffset);
        if (marker.endOffset() > endOffset)
            marker.setEndOffset(endOffset);
        marker.shiftOffsets(delta);

        addMarker(dstNode, marker);
    }

    if (docDirty && dstNode.renderer())
        dstNode.renderer()->repaint();
}

void DocumentMarkerController::removeMarkers(Node& node, unsigned startOffset, int length, OptionSet<DocumentMarker::MarkerType> markerTypes, std::function<bool(DocumentMarker*)> filterFunction, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!(m_markers.isEmpty()));

    MarkerList* list = m_markers.get(&node);
    if (!list)
        return;

    bool docDirty = false;
    unsigned endOffset = startOffset + length;
    for (size_t i = 0; i < list->size();) {
        DocumentMarker marker = list->at(i);

        // markers are returned in order, so stop if we are now past the specified range
        if (marker.startOffset() >= endOffset)
            break;

        // skip marker that is wrong type or before target
        if (marker.endOffset() <= startOffset || !markerTypes.contains(marker.type())) {
            i++;
            continue;
        }

        if (filterFunction && !filterFunction(&marker)) {
            i++;
            continue;
        }

        // at this point we know that marker and target intersect in some way
        docDirty = true;

        // pitch the old marker
        list->remove(i);

        if (shouldRemovePartiallyOverlappingMarker)
            // Stop here. Don't add resulting slices back.
            continue;

        // add either of the resulting slices that are left after removing target
        if (startOffset > marker.startOffset()) {
            DocumentMarker newLeft = marker;
            newLeft.setEndOffset(startOffset);
            list->insert(i, RenderedDocumentMarker(newLeft));
            // i now points to the newly-inserted node, but we want to skip that one
            i++;
        }
        if (marker.endOffset() > endOffset) {
            DocumentMarker newRight = marker;
            newRight.setStartOffset(endOffset);
            list->insert(i, RenderedDocumentMarker(newRight));
            // i now points to the newly-inserted node, but we want to skip that one
            i++;
        }
    }

    if (list->isEmpty()) {
        m_markers.remove(&node);
        if (m_markers.isEmpty())
            m_possiblyExistingMarkerTypes = { };
    }

    if (docDirty && node.renderer())
        node.renderer()->repaint();
}

DocumentMarker* DocumentMarkerController::markerContainingPoint(const LayoutPoint& point, DocumentMarker::MarkerType markerType)
{
    if (!possiblyHasMarkers(markerType))
        return nullptr;
    ASSERT(!(m_markers.isEmpty()));

    updateRectsForInvalidatedMarkersOfType(markerType);

    for (auto& nodeAndMarkers : m_markers) {
        for (auto& marker : *nodeAndMarkers.value) {
            if (marker.type() != markerType)
                continue;

            if (marker.contains(point))
                return &marker;
        }
    }

    return nullptr;
}

Vector<RenderedDocumentMarker*> DocumentMarkerController::markersFor(Node& node, OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return { };

    Vector<RenderedDocumentMarker*> result;
    MarkerList* list = m_markers.get(&node);
    if (!list)
        return result;

    for (auto& marker : *list) {
        if (markerTypes.contains(marker.type()))
            result.append(&marker);
    }

    return result;
}

Vector<RenderedDocumentMarker*> DocumentMarkerController::markersInRange(Range& range, OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return Vector<RenderedDocumentMarker*>();

    Vector<RenderedDocumentMarker*> foundMarkers;

    Node& startContainer = range.startContainer();
    Node& endContainer = range.endContainer();

    Node* pastLastNode = range.pastLastNode();
    for (Node* node = range.firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        ASSERT(node);
        for (auto* marker : markersFor(*node)) {
            if (!markerTypes.contains(marker->type()))
                continue;
            if (node == &startContainer && marker->endOffset() <= range.startOffset())
                continue;
            if (node == &endContainer && marker->startOffset() >= range.endOffset())
                continue;
            foundMarkers.append(marker);
        }
    }
    return foundMarkers;
}

void DocumentMarkerController::removeMarkers(Node& node, OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());
    
    auto iterator = m_markers.find(&node);
    if (iterator != m_markers.end())
        removeMarkersFromList(iterator, markerTypes);
}

void DocumentMarkerController::removeMarkers(OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    for (auto& node : copyToVector(m_markers.keys())) {
        auto iterator = m_markers.find(node);
        if (iterator != m_markers.end())
            removeMarkersFromList(iterator, markerTypes);
    }

    m_possiblyExistingMarkerTypes.remove(markerTypes);
}

void DocumentMarkerController::removeMarkersFromList(MarkerMap::iterator iterator, OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    bool needsRepainting = false;
    bool listCanBeRemoved;

    if (markerTypes == DocumentMarker::allMarkers()) {
        needsRepainting = true;
        listCanBeRemoved = true;
    } else {
        MarkerList* list = iterator->value.get();

        for (size_t i = 0; i != list->size(); ) {
            DocumentMarker marker = list->at(i);

            // skip nodes that are not of the specified type
            if (!markerTypes.contains(marker.type())) {
                ++i;
                continue;
            }

            // pitch the old marker
            list->remove(i);
            needsRepainting = true;
            // i now is the index of the next marker
        }

        listCanBeRemoved = list->isEmpty();
    }

    if (needsRepainting) {
        if (auto renderer = iterator->key->renderer())
            renderer->repaint();
    }

    if (listCanBeRemoved) {
        m_markers.remove(iterator);
        if (m_markers.isEmpty())
            m_possiblyExistingMarkerTypes = { };
    }
}

void DocumentMarkerController::repaintMarkers(OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    // outer loop: process each markered node in the document
    for (auto& marker : m_markers) {
        Node* node = marker.key.get();

        // inner loop: process each marker in the current node
        bool nodeNeedsRepaint = false;
        for (auto& documentMarker : *marker.value) {
            if (markerTypes.contains(documentMarker.type())) {
                nodeNeedsRepaint = true;
                break;
            }
        }

        if (!nodeNeedsRepaint)
            continue;

        // cause the node to be redrawn
        if (auto renderer = node->renderer())
            renderer->repaint();
    }
}

void DocumentMarkerController::shiftMarkers(Node& node, unsigned startOffset, int delta)
{
    if (!possiblyHasMarkers(DocumentMarker::allMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerList* list = m_markers.get(&node);
    if (!list)
        return;

    bool didShiftMarker = false;
    for (size_t i = 0; i != list->size(); ) {
        RenderedDocumentMarker& marker = list->at(i);
        // FIXME: How can this possibly be iOS-specific code?
#if PLATFORM(IOS_FAMILY)
        int targetStartOffset = marker.startOffset() + delta;
        int targetEndOffset = marker.endOffset() + delta;
        if (targetStartOffset >= node.maxCharacterOffset() || targetEndOffset <= 0) {
            list->remove(i);
            continue;
        }
#endif
        if (marker.startOffset() >= startOffset) {
            ASSERT((int)marker.startOffset() + delta >= 0);
            marker.shiftOffsets(delta);
            didShiftMarker = true;
#if !PLATFORM(IOS_FAMILY)
        }
#else
        // FIXME: Inserting text inside a DocumentMarker does not grow the marker.
        // See <https://bugs.webkit.org/show_bug.cgi?id=62504>.
        } else if (marker.endOffset() > startOffset) {
            if (marker.endOffset() + delta <= marker.startOffset()) {
                list->remove(i);
                continue;
            }
            marker.setEndOffset(targetEndOffset < node.maxCharacterOffset() ? targetEndOffset : node.maxCharacterOffset());
            didShiftMarker = true;
        }
#endif
        ++i;
    }

    if (didShiftMarker) {
        invalidateRectsForMarkersInNode(node);

        if (node.renderer())
            node.renderer()->repaint();
    }
}

void DocumentMarkerController::setMarkersActive(Range& range, bool active)
{
    if (!possiblyHasMarkers(DocumentMarker::allMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    Node& startContainer = range.startContainer();
    Node& endContainer = range.endContainer();

    Node* pastLastNode = range.pastLastNode();

    for (Node* node = range.firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        unsigned startOffset = node == &startContainer ? range.startOffset() : 0;
        unsigned endOffset = node == &endContainer ? range.endOffset() : std::numeric_limits<unsigned>::max();
        setMarkersActive(*node, startOffset, endOffset, active);
    }
}

void DocumentMarkerController::setMarkersActive(Node& node, unsigned startOffset, unsigned endOffset, bool active)
{
    MarkerList* list = m_markers.get(&node);
    if (!list)
        return;

    bool didActivateMarker = false;
    for (auto& marker : *list) {
        // Markers are returned in order, so stop if we are now past the specified range.
        if (marker.startOffset() >= endOffset)
            break;

        // Skip marker that is wrong type or before target.
        if (marker.endOffset() < startOffset || marker.type() != DocumentMarker::TextMatch)
            continue;

        marker.setActiveMatch(active);
        didActivateMarker = true;
    }

    if (didActivateMarker && node.renderer())
        node.renderer()->repaint();
}

bool DocumentMarkerController::hasMarkers(Range& range, OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return false;
    ASSERT(!m_markers.isEmpty());

    Node& startContainer = range.startContainer();
    Node& endContainer = range.endContainer();

    Node* pastLastNode = range.pastLastNode();
    for (Node* node = range.firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        ASSERT(node);
        for (auto* marker : markersFor(*node)) {
            if (!markerTypes.contains(marker->type()))
                continue;
            if (node == &startContainer && marker->endOffset() <= static_cast<unsigned>(range.startOffset()))
                continue;
            if (node == &endContainer && marker->startOffset() >= static_cast<unsigned>(range.endOffset()))
                continue;
            return true;
        }
    }
    return false;
}

void DocumentMarkerController::clearDescriptionOnMarkersIntersectingRange(Range& range, OptionSet<DocumentMarker::MarkerType> markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    Node& startContainer = range.startContainer();
    Node& endContainer = range.endContainer();

    Node* pastLastNode = range.pastLastNode();
    for (Node* node = range.firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        unsigned startOffset = node == &startContainer ? range.startOffset() : 0;
        unsigned endOffset = node == &endContainer ? static_cast<unsigned>(range.endOffset()) : std::numeric_limits<unsigned>::max();
        MarkerList* list = m_markers.get(node);
        if (!list)
            continue;

        for (size_t i = 0; i < list->size(); ++i) {
            DocumentMarker& marker = list->at(i);

            // markers are returned in order, so stop if we are now past the specified range
            if (marker.startOffset() >= endOffset)
                break;

            // skip marker that is wrong type or before target
            if (marker.endOffset() <= startOffset || !markerTypes.contains(marker.type())) {
                i++;
                continue;
            }

            marker.clearData();
        }
    }
}

#if ENABLE(TREE_DEBUGGING)
void DocumentMarkerController::showMarkers() const
{
    fprintf(stderr, "%d nodes have markers:\n", m_markers.size());
    for (auto& marker : m_markers) {
        Node* node = marker.key.get();
        fprintf(stderr, "%p", node);
        for (auto& documentMarker : *marker.value)
            fprintf(stderr, " %d:[%d:%d](%d)", documentMarker.type(), documentMarker.startOffset(), documentMarker.endOffset(), documentMarker.isActiveMatch());

        fprintf(stderr, "\n");
    }
}
#endif

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
void showDocumentMarkers(const WebCore::DocumentMarkerController* controller)
{
    if (controller)
        controller->showMarkers();
}
#endif
