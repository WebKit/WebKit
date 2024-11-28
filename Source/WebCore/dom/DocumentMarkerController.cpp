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

#include "config.h"
#include "DocumentMarkerController.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "LocalFrame.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "RenderBlockFlow.h"
#include "RenderLayer.h"
#include "RenderReplaced.h"
#include "RenderText.h"
#include "RenderedDocumentMarker.h"
#include "TextIterator.h"
#include <stdio.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentMarkerController);

constexpr Seconds markerFadeAnimationDuration = 200_ms;

constexpr double markerAnimationFrameRate = 30;

inline bool DocumentMarkerController::possiblyHasMarkers(OptionSet<DocumentMarkerType> types) const
{
    return m_possiblyExistingMarkerTypes.containsAny(types);
}

DocumentMarkerController::DocumentMarkerController(Document& document)
    : m_document(document)
    , m_fadeAnimationTimer(*this, &DocumentMarkerController::fadeAnimationTimerFired)
    , m_writingToolsTextSuggestionAnimationTimer(*this, &DocumentMarkerController::writingToolsTextSuggestionAnimationTimerFired)
{
}

DocumentMarkerController::~DocumentMarkerController() = default;

void DocumentMarkerController::detach()
{
    m_markers.clear();
    m_possiblyExistingMarkerTypes = { };
    m_fadeAnimationTimer.stop();
    m_writingToolsTextSuggestionAnimationTimer.stop();
}

auto DocumentMarkerController::collectTextRanges(const SimpleRange& range) -> Vector<TextRange>
{
    Vector<TextRange> ranges;
    for (TextIterator iterator(range); !iterator.atEnd(); iterator.advance()) {
        auto currentRange = iterator.range();
        ranges.append({ WTFMove(currentRange.start.container), { currentRange.start.offset, currentRange.end.offset } });
    }
    return ranges;
}

void DocumentMarkerController::addMarker(const SimpleRange& range, DocumentMarkerType type, const DocumentMarker::Data& data)
{
    for (auto& textPiece : collectTextRanges(range))
        addMarker(textPiece.node, { type, textPiece.range, DocumentMarker::Data { data } });
}

void DocumentMarkerController::addMarker(Text& node, unsigned startOffset, unsigned length, DocumentMarkerType type, DocumentMarker::Data&& data)
{
    addMarker(node, { type, { startOffset, startOffset + length }, WTFMove(data) });
}

void DocumentMarkerController::addDraggedContentMarker(const SimpleRange& range)
{
    // FIXME: Since the marker is already stored in a map keyed by node, we can probably change things around so we don't have to also store the node in the marker.
    for (auto& textPiece : collectTextRanges(range))
        addMarker(textPiece.node, { DocumentMarkerType::DraggedContent, textPiece.range, RefPtr<Node> { textPiece.node.ptr() } });
}

void DocumentMarkerController::addTransparentContentMarker(const SimpleRange& range, WTF::UUID uuid)
{
    // FIXME: Since the marker is already stored in a map keyed by node, we can probably change things around so we don't have to also store the node in the marker.
    for (auto& textPiece : collectTextRanges(range))
        addMarker(textPiece.node, { DocumentMarkerType::TransparentContent, textPiece.range, DocumentMarker::TransparentContentData { { textPiece.node.ptr() }, uuid } });
}

void DocumentMarkerController::removeMarkers(const SimpleRange& range, OptionSet<DocumentMarkerType> types, RemovePartiallyOverlappingMarker overlapRule)
{
    filterMarkers(range, nullptr, types, overlapRule);
}

void DocumentMarkerController::filterMarkers(const SimpleRange& range, const Function<FilterMarkerResult(const DocumentMarker&)>& filter, OptionSet<DocumentMarkerType> types, RemovePartiallyOverlappingMarker overlapRule)
{
    for (auto& textPiece : collectTextRanges(range)) {
        if (!possiblyHasMarkers(types))
            return;
        ASSERT(!m_markers.isEmpty());
        removeMarkers(textPiece.node, textPiece.range, types, filter, overlapRule);
    }
}

static void updateRenderedRectsForMarker(RenderedDocumentMarker& marker, Node& node)
{
    ASSERT(!node.document().view() || !node.document().view()->needsLayout());
    marker.setUnclippedAbsoluteRects(boundingBoxes(RenderObject::absoluteTextQuads(makeSimpleRange(node, marker), RenderObject::BoundingRectBehavior::UseSelectionHeight)));
}

void DocumentMarkerController::invalidateRectsForAllMarkers()
{
    if (!hasMarkers())
        return;

    for (auto& nodeMarkers : m_markers.values()) {
        for (auto& marker : *nodeMarkers)
            marker.invalidate();
    }

    if (RefPtr page = m_document->page())
        page->chrome().client().didInvalidateDocumentMarkerRects();
}

void DocumentMarkerController::invalidateRectsForMarkersInNode(Node& node)
{
    if (!hasMarkers())
        return;

    auto markers = m_markers.get(node);
    ASSERT(markers);

    for (auto& marker : *markers)
        marker.invalidate();

    if (RefPtr page = m_document->page())
        page->chrome().client().didInvalidateDocumentMarkerRects();
}

static void updateMainFrameLayoutIfNeeded(Document& document)
{
    RefPtr frame = document.frame();
    if (!frame)
        return;

    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return;

    if (RefPtr mainFrameView = localFrame->view())
        mainFrameView->updateLayoutAndStyleIfNeededRecursive();
}

Ref<Document> DocumentMarkerController::protectedDocument() const
{
    return m_document.get();
}

void DocumentMarkerController::updateRectsForInvalidatedMarkersOfType(DocumentMarkerType type)
{
    if (!possiblyHasMarkers(type))
        return;
    ASSERT(!m_markers.isEmpty());

    bool updatedLayout = false;
    for (auto& nodeMarkers : m_markers) {
        for (auto& marker : *nodeMarkers.value) {
            if (marker.type() != type || marker.isValid())
                continue;
            if (!updatedLayout) {
                updateMainFrameLayoutIfNeeded(protectedDocument());
                updatedLayout = true;
            }
            updateRenderedRectsForMarker(marker, nodeMarkers.key);
        }
    }
}

Vector<FloatRect> DocumentMarkerController::renderedRectsForMarkers(DocumentMarkerType type)
{
    Vector<FloatRect> result;

    if (!possiblyHasMarkers(type))
        return result;
    ASSERT(!m_markers.isEmpty());

    RefPtr frame = m_document->frame();
    if (!frame)
        return result;
    RefPtr frameView = frame->view();
    if (!frameView)
        return result;

    updateRectsForInvalidatedMarkersOfType(type);

    bool isSubframe = !frame->isMainFrame();
    IntRect subframeClipRect;
    if (isSubframe)
        subframeClipRect = frameView->windowToContents(frameView->windowClipRect());

    for (auto& nodeMarkers : m_markers) {
        CheckedPtr renderer = nodeMarkers.key->renderer();
        FloatRect overflowClipRect;
        if (renderer)
            overflowClipRect = renderer->absoluteClippedOverflowRectForRepaint();
        for (auto& marker : *nodeMarkers.value) {
            if (marker.type() != type)
                continue;

            auto renderedRects = marker.unclippedAbsoluteRects();

            // Clip document markers by their overflow clip.
            if (renderer) {
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

static bool shouldInsertAsSeparateMarker(const DocumentMarker& marker)
{
    switch (marker.type()) {
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    case DocumentMarkerType::PlatformTextChecking:
        return true;
#endif

#if PLATFORM(IOS_FAMILY)
    case DocumentMarkerType::DictationPhraseWithAlternatives:
    case DocumentMarkerType::DictationResult:
        return true;
#endif

    case DocumentMarkerType::TransparentContent:
        return is<RenderReplaced>(std::get<DocumentMarker::TransparentContentData>(marker.data()).node->renderer());

    case DocumentMarkerType::DraggedContent:
        return is<RenderReplaced>(std::get<RefPtr<Node>>(marker.data())->renderer());

    default:
        return false;
    }
}

static bool canMergeMarkers(const DocumentMarker& marker, const DocumentMarker& other)
{
    ASSERT(!shouldInsertAsSeparateMarker(marker));
    ASSERT(!shouldInsertAsSeparateMarker(other));

    if (marker.type() != other.type())
        return false;

#if ENABLE(WRITING_TOOLS)
    if (marker.type() == DocumentMarkerType::WritingToolsTextSuggestion)
        return std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data()).suggestionID == std::get<DocumentMarker::WritingToolsTextSuggestionData>(other.data()).suggestionID;
#endif

    return true;
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void DocumentMarkerController::addMarker(Node& node, DocumentMarker&& newMarker)
{
    ASSERT(newMarker.endOffset() >= newMarker.startOffset());
    if (newMarker.endOffset() == newMarker.startOffset())
        return;

    m_possiblyExistingMarkerTypes.add(newMarker.type());

    auto& list = m_markers.add(node, nullptr).iterator->value;

    if (!list) {
        list = makeUnique<Vector<RenderedDocumentMarker>>();
        list->append(RenderedDocumentMarker(WTFMove(newMarker)));
    } else if (shouldInsertAsSeparateMarker(newMarker)) {
        // We don't merge dictation markers.
        size_t i;
        size_t numberOfMarkers = list->size();
        for (i = 0; i < numberOfMarkers; ++i) {
            DocumentMarker marker = list->at(i);
            if (marker.startOffset() > newMarker.startOffset())
                break;
        }
        list->insert(i, RenderedDocumentMarker(WTFMove(newMarker)));
    } else {
        RenderedDocumentMarker toInsert(WTFMove(newMarker));
        size_t numMarkers = list->size();
        size_t i;
        // Iterate over all markers whose start offset is less than or equal to the new marker's.
        // If one of them is of the same type as the new marker and touches it or intersects with it
        // (there is at most one), remove it and adjust the new marker's start offset to encompass it.
        for (i = 0; i < numMarkers; ++i) {
            DocumentMarker marker = list->at(i);
            if (marker.startOffset() > toInsert.startOffset())
                break;
            if (canMergeMarkers(marker, toInsert) && marker.endOffset() >= toInsert.startOffset()) {
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
            if (canMergeMarkers(marker, toInsert)) {
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

    if (CheckedPtr renderer = node.renderer())
        renderer->repaint();

#if ENABLE(WRITING_TOOLS)
    if (newMarker.type() == DocumentMarkerType::WritingToolsTextSuggestion) {
        if (!m_writingToolsTextSuggestionAnimationTimer.isActive())
            m_writingToolsTextSuggestionAnimationTimer.startRepeating(1_s / markerAnimationFrameRate);
    }
#endif

    invalidateRectsForMarkersInNode(node);
}

// Copies markers from source to destination, applying the specified shift delta to the copies. The shift is
// useful if, e.g., the caller has created the destination from a non-prefix substring of the source.
void DocumentMarkerController::copyMarkers(Node& source, OffsetRange range, Node& destination)
{
    if (range.start >= range.end)
        return;

    if (!possiblyHasMarkers(DocumentMarker::allMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    auto list = m_markers.get(source);
    if (!list)
        return;

    bool needRepaint = false;
    for (auto& marker : *list) {
        // Stop if we are now past the specified range.
        if (marker.startOffset() >= range.end)
            break;

        // Skip marker that is before the specified range.
        if (marker.endOffset() < range.start)
            continue;

        // Pin the marker to the specified range and apply the shift delta.
        auto copiedMarker = marker;
        if (copiedMarker.startOffset() < range.start)
            copiedMarker.setStartOffset(range.start);
        if (copiedMarker.endOffset() >= range.end)
            copiedMarker.setEndOffset(range.end);

        addMarker(destination, WTFMove(copiedMarker));
        needRepaint = true;
    }

    if (needRepaint) {
        if (CheckedPtr renderer = destination.renderer())
            renderer->repaint();
    }
}

void DocumentMarkerController::removeMarkers(Node& node, OffsetRange range, OptionSet<DocumentMarkerType> types, const Function<FilterMarkerResult(const DocumentMarker&)>& filter, RemovePartiallyOverlappingMarker overlapRule)
{
    if (range.start >= range.end)
        return;

    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    auto list = m_markers.get(node);
    if (!list)
        return;

    bool needRepaint = false;
    for (size_t i = 0; i < list->size(); ) {
        auto& marker = list->at(i);

        // markers are returned in order, so stop if we are now past the specified range
        if (marker.startOffset() >= range.end)
            break;

        // skip marker that is wrong type or before target
        if (marker.endOffset() <= range.start || !types.contains(marker.type())) {
            i++;
            continue;
        }

        if (filter && filter(marker) == FilterMarkerResult::Keep) {
            i++;
            continue;
        }

        // At this point we know that marker and target intersect in some way.
        needRepaint = true;

        DocumentMarker copiedMarker = marker;
        list->remove(i);
        if (overlapRule == RemovePartiallyOverlappingMarker::Yes)
            continue;

        // Add either of the resulting slices that remain after removing target.
        if (range.start > copiedMarker.startOffset()) {
            auto newLeft = copiedMarker;
            newLeft.setEndOffset(range.start);
            list->insert(i, RenderedDocumentMarker(WTFMove(newLeft)));
            i++;
        }
        if (copiedMarker.endOffset() > range.end) {
            copiedMarker.setStartOffset(range.end);
            list->insert(i, RenderedDocumentMarker(WTFMove(copiedMarker)));
            i++;
        }
    }

    if (list->isEmpty()) {
        m_markers.remove(node);
        if (m_markers.isEmpty())
            m_possiblyExistingMarkerTypes = { };
    }

    if (needRepaint) {
        if (CheckedPtr renderer = node.renderer())
            renderer->repaint();
    }
}

WeakPtr<DocumentMarker> DocumentMarkerController::markerContainingPoint(const LayoutPoint& point, DocumentMarkerType type)
{
    if (!possiblyHasMarkers(type))
        return nullptr;
    ASSERT(!m_markers.isEmpty());

    updateRectsForInvalidatedMarkersOfType(type);
    for (auto& nodeMarkers : m_markers.values()) {
        for (auto& marker : *nodeMarkers) {
            if (marker.type() == type && marker.contains(point))
                return marker;
        }
    }
    return nullptr;
}

Vector<WeakPtr<RenderedDocumentMarker>> DocumentMarkerController::markersFor(Node& node, OptionSet<DocumentMarkerType> types) const
{
    if (!possiblyHasMarkers(types))
        return { };

    Vector<WeakPtr<RenderedDocumentMarker>> result;
    auto list = m_markers.get(&node);
    if (!list)
        return result;

    for (auto& marker : *list) {
        if (types.contains(marker.type()))
            result.append(marker);
    }

    return result;
}

template<>
void DocumentMarkerController::forEach<DocumentMarkerController::IterationDirection::Forwards>(const SimpleRange& range, OptionSet<DocumentMarkerType> types, Function<bool(Node&, RenderedDocumentMarker&)>&& function)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    for (Ref node : intersectingNodes(range)) {
        if (auto list = m_markers.get(node.ptr())) {
            auto offsetRange = characterDataOffsetRange(range, node.get());
            for (auto& marker : *list) {
                // Markers are stored in order, so stop if we are now past the specified range.
                if (marker.startOffset() >= offsetRange.end)
                    break;
                if (marker.endOffset() > offsetRange.start && types.contains(marker.type())) {
                    if (function(node, marker))
                        return;
                }
            }
        }
    }
}

template<>
void DocumentMarkerController::forEach<DocumentMarkerController::IterationDirection::Backwards>(const SimpleRange& range, OptionSet<DocumentMarkerType> types, Function<bool(Node&, RenderedDocumentMarker&)>&& function)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    Vector<Ref<WebCore::Node>> nodes;
    for (Ref node : intersectingNodes(range))
        nodes.append(node);

    for (auto nodeIterator = nodes.rbegin(); nodeIterator != nodes.rend(); ++nodeIterator) {
        auto node = *nodeIterator;
        auto markers = markersFor(node, types);

        for (auto markerIterator = markers.rbegin(); markerIterator != markers.rend(); ++markerIterator) {
            auto marker = *markerIterator;
            if (!marker)
                continue;

            if (function(node.get(), *marker))
                return;
        }
    }
}

void DocumentMarkerController::forEachOfTypes(OptionSet<DocumentMarkerType> types, Function<bool(Node&, RenderedDocumentMarker&)>&& function)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    for (auto& nodeMarkers : m_markers) {
        for (auto& marker : *nodeMarkers.value) {
            if (!types.contains(marker.type()))
                continue;

            Ref node = nodeMarkers.key;
            function(node, marker);
        }
    }
}

Vector<WeakPtr<RenderedDocumentMarker>> DocumentMarkerController::markersInRange(const SimpleRange& range, OptionSet<DocumentMarkerType> types)
{
    // FIXME: Consider making forEach public and changing callers to use that function instead of this one.
    Vector<WeakPtr<RenderedDocumentMarker>> markers;
    forEach(range, types, [&] (Node&, RenderedDocumentMarker& marker) {
        markers.append(marker);
        return false;
    });
    return markers;
}

Vector<SimpleRange> DocumentMarkerController::rangesForMarkersInRange(const SimpleRange& range, OptionSet<DocumentMarkerType> types)
{
    Vector<SimpleRange> ranges;
    forEach(range, types, [&] (Node& node, RenderedDocumentMarker& marker) {
        ranges.append(makeSimpleRange(node, marker));
        return false;
    });
    return ranges;
}

void DocumentMarkerController::removeMarkers(Node& node, OptionSet<DocumentMarkerType> types)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());
    
    auto iterator = m_markers.find(node);
    if (iterator != m_markers.end())
        removeMarkersFromList(iterator, types);
}

void DocumentMarkerController::removeMarkers(OptionSet<DocumentMarkerType> types)
{
    removeMarkers(types, nullptr);
}

void DocumentMarkerController::removeMarkers(OptionSet<DocumentMarkerType> types, const Function<FilterMarkerResult(const RenderedDocumentMarker&)>& filter)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    auto removedMarkerTypes = types;
    for (auto& node : copyToVector(m_markers.keys()))
        removedMarkerTypes = removedMarkerTypes & removeMarkersFromList(m_markers.find(node), types, filter);

#if ENABLE(WRITING_TOOLS)
    if (removedMarkerTypes.contains(DocumentMarkerType::WritingToolsTextSuggestion))
        m_writingToolsTextSuggestionAnimationTimer.stop();
#endif

    m_possiblyExistingMarkerTypes.remove(removedMarkerTypes);
}

OptionSet<DocumentMarkerType> DocumentMarkerController::removeMarkersFromList(MarkerMap::iterator iterator, OptionSet<DocumentMarkerType> types, const Function<FilterMarkerResult(const RenderedDocumentMarker&)>& filter)
{
    bool needsRepainting = false;
    bool listCanBeRemoved;
    auto removedMarkerTypes = types;

    if (types == DocumentMarker::allMarkers() && !filter) {
        needsRepainting = true;
        listCanBeRemoved = true;
    } else {
        auto list = iterator->value.get();

        for (size_t i = 0; i != list->size(); ) {
            auto& marker = list->at(i);

            // skip nodes that are not of the specified type
            auto markerType = marker.type();
            if (!types.contains(markerType)) {
                ++i;
                continue;
            }

            if (filter && filter(marker) == FilterMarkerResult::Keep) {
                ++i;
                removedMarkerTypes.remove(markerType);
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
        if (CheckedPtr renderer = iterator->key->renderer())
            renderer->repaint();
    }

    if (listCanBeRemoved) {
        m_markers.remove(iterator);
        if (m_markers.isEmpty())
            m_possiblyExistingMarkerTypes = { };
    }

    return removedMarkerTypes;
}

void DocumentMarkerController::repaintMarkers(OptionSet<DocumentMarkerType> types)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    for (auto& nodeMarkers : m_markers) {
        for (auto& marker : *nodeMarkers.value) {
            if (types.contains(marker.type())) {
                if (CheckedPtr renderer = nodeMarkers.key->renderer())
                    renderer->repaint();
                break;
            }
        }
    }
}

void DocumentMarkerController::shiftMarkers(Node& node, unsigned startOffset, int delta)
{
    if (!possiblyHasMarkers(DocumentMarker::allMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    auto list = m_markers.get(node);
    if (!list)
        return;

    bool didShiftMarker = false;
    for (size_t i = 0; i != list->size(); ) {
        auto& marker = list->at(i);

#if PLATFORM(IOS_FAMILY)
        // FIXME: No obvious reason this should be iOS-specific. Remove the #if at some point.
        auto targetStartOffset = clampTo<unsigned>(static_cast<int>(marker.startOffset()) + delta);
        auto targetEndOffset = clampTo<unsigned>(static_cast<int>(marker.endOffset()) + delta);
#endif

        if (marker.startOffset() >= startOffset) {
            // There is no need to adjust or remove the marker if it's before the start of the
            // text being removed.

#if PLATFORM(IOS_FAMILY)
            if (targetStartOffset >= node.length() || targetEndOffset <= 0) {
                list->remove(i);
                continue;
            }
#endif

            ASSERT((int)marker.startOffset() + delta >= 0);
            marker.shiftOffsets(delta);
            didShiftMarker = true;
        }
#if PLATFORM(IOS_FAMILY)
        // FIXME: No obvious reason this should be iOS-specific. Remove the #if at some point.
        else if (marker.endOffset() > startOffset) {
            if (targetEndOffset <= marker.startOffset()) {
                list->remove(i);
                continue;
            }
            marker.setEndOffset(std::min(targetEndOffset, node.length()));
            didShiftMarker = true;
        }
#endif

        ++i;
    }

    if (didShiftMarker) {
        invalidateRectsForMarkersInNode(node);
        if (CheckedPtr renderer = node.renderer())
            renderer->repaint();
    }
}

void DocumentMarkerController::dismissMarkers(OptionSet<DocumentMarkerType> types)
{
    if (!possiblyHasMarkers(types))
        return;
    ASSERT(!m_markers.isEmpty());

    bool requiresAnimation = false;
    forEachOfTypes(types, [&] (Node&, RenderedDocumentMarker& marker) {
        if (!marker.isBeingDismissed()) {
            requiresAnimation = true;
            marker.setBeingDismissed(true);
        }
        return false;
    });

    if (requiresAnimation && !m_fadeAnimationTimer.isActive())
        m_fadeAnimationTimer.startRepeating(1_s / markerAnimationFrameRate);
}

void DocumentMarkerController::writingToolsTextSuggestionAnimationTimerFired()
{
#if ENABLE(WRITING_TOOLS)
    forEachOfTypes({ DocumentMarkerType::WritingToolsTextSuggestion }, [](Node& node, RenderedDocumentMarker&) {
        if (CheckedPtr renderer = node.renderer())
            renderer->repaint();
        return false;
    });
#endif
}

void DocumentMarkerController::fadeAnimationTimerFired()
{
    bool shouldRemoveMarkers = false;
    bool cancelTimer = true;

    forEachOfTypes(DocumentMarker::allMarkers(), [&] (Node& node, RenderedDocumentMarker& marker) {
        if (!marker.isBeingDismissed())
            return false;

        float animationProgress = 1;
        if (auto animationStartTime = marker.animationStartTime())
            animationProgress = (WallTime::now() - animationStartTime.value()) / markerFadeAnimationDuration;

        animationProgress = std::min<float>(animationProgress, 1.0);

        float opacity = 1 - animationProgress;
        marker.setOpacity(opacity);

        if (!opacity)
            shouldRemoveMarkers = true;
        else {
            cancelTimer = false;
            if (CheckedPtr renderer = node.renderer())
                renderer->repaint();
        }

        return false;
    });

    if (shouldRemoveMarkers) {
        removeMarkers(DocumentMarker::allMarkers(), [&] (const RenderedDocumentMarker& marker) {
            if (marker.isBeingDismissed() && !marker.opacity())
                return FilterMarkerResult::Remove;
            return FilterMarkerResult::Keep;
        });
    }

    if (cancelTimer)
        m_fadeAnimationTimer.stop();
}

bool DocumentMarkerController::hasMarkers(const SimpleRange& range, OptionSet<DocumentMarkerType> types)
{
    bool foundMarker = false;
    forEach(range, types, [&] (Node&, RenderedDocumentMarker&) {
        foundMarker = true;
        return true;
    });
    return foundMarker;
}

void DocumentMarkerController::clearDescriptionOnMarkersIntersectingRange(const SimpleRange& range, OptionSet<DocumentMarkerType> types)
{
    forEach(range, types, [&] (Node&, RenderedDocumentMarker& marker) {
        marker.clearData();
        return false;
    });
}

std::tuple<float, float> DocumentMarkerController::markerYPositionAndHeightForFont(const FontCascade& font)
{
    auto ascent = font.metricsOfPrimaryFont().intAscent();
    auto fontSize = std::clamp(font.size(), 10.0f, 40.0f);

    auto y = ascent + 0.11035 * fontSize;
    auto height = 0.13247 * fontSize;

    return { y, height };
}

void addMarker(const SimpleRange& range, DocumentMarkerType type, const DocumentMarker::Data& data)
{
    range.start.protectedDocument()->checkedMarkers()->addMarker(range, type, data);
}

void addMarker(Text& node, unsigned startOffset, unsigned length, DocumentMarkerType type, DocumentMarker::Data&& data)
{
    node.protectedDocument()->checkedMarkers()->addMarker(node, startOffset, length, type, WTFMove(data));
}

void removeMarkers(const SimpleRange& range, OptionSet<DocumentMarkerType> types, RemovePartiallyOverlappingMarker policy)
{
    range.start.protectedDocument()->checkedMarkers()->removeMarkers(range, types, policy);
}

SimpleRange makeSimpleRange(Node& node, const DocumentMarker& marker)
{
    unsigned startOffset = marker.startOffset();
    unsigned endOffset = marker.endOffset();
    return { { node, startOffset }, { node, endOffset } };
}

#if ENABLE(TREE_DEBUGGING)

void DocumentMarkerController::showMarkers() const
{
    fprintf(stderr, "%d nodes have markers:\n", m_markers.size());
    for (auto& nodeMarkers : m_markers) {
        fprintf(stderr, "%p", nodeMarkers.key.ptr());
        for (auto& marker : *nodeMarkers.value)
            fprintf(stderr, " %u:[%d:%d]", enumToUnderlyingType(marker.type()), marker.startOffset(), marker.endOffset());
        fputc('\n', stderr);
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
