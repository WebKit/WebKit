/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXTextMarker.h"

#include "AXIsolatedObject.h"
#include "AXLogger.h"
#include "AXObjectCache.h"
#include "AXTreeStore.h"
#include "HTMLInputElement.h"
#include "RenderObject.h"
#include "TextIterator.h"
#include <wtf/text/MakeString.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXTextMarker);

static AXID nodeID(AXObjectCache& cache, Node* node)
{
    if (RefPtr object = cache.getOrCreate(node))
        return object->objectID();
    return { };
}

TextMarkerData::TextMarkerData(AXObjectCache& cache, const VisiblePosition& visiblePosition, int charStart, int charOffset, bool ignoredParam)
{
    ASSERT(isMainThread());

    memset(static_cast<void*>(this), 0, sizeof(*this));
    treeID = cache.treeID().toUInt64();
    auto position = visiblePosition.deepEquivalent();
    objectID = nodeID(cache, position.anchorNode()).toUInt64();
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = position.anchorType();
    affinity = visiblePosition.affinity();
    characterStart = std::max(charStart, 0);
    characterOffset = std::max(charOffset, 0);
    ignored = ignoredParam;
}

TextMarkerData::TextMarkerData(AXObjectCache& cache, const CharacterOffset& characterOffsetParam, bool ignoredParam)
{
    ASSERT(isMainThread());

    memset(static_cast<void*>(this), 0, sizeof(*this));
    treeID = cache.treeID().toUInt64();
    objectID = nodeID(cache, characterOffsetParam.node.get()).toUInt64();
    auto visiblePosition = cache.visiblePositionFromCharacterOffset(characterOffsetParam);
    auto position = visiblePosition.deepEquivalent();
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = Position::PositionIsOffsetInAnchor;
    affinity = visiblePosition.affinity();
    characterStart = std::max(characterOffsetParam.startIndex, 0);
    characterOffset = std::max(characterOffsetParam.offset, 0);
    ignored = ignoredParam;
}

AXTextMarker::AXTextMarker(const VisiblePosition& visiblePosition)
{
    ASSERT(isMainThread());

    if (visiblePosition.isNull())
        return;

    auto* node = visiblePosition.deepEquivalent().anchorNode();
    ASSERT(node);
    if (!node)
        return;

    auto* cache = node->document().axObjectCache();
    if (!cache)
        return;

    if (auto data = cache->textMarkerDataForVisiblePosition(visiblePosition))
        m_data = WTFMove(*data);
}

AXTextMarker::AXTextMarker(const CharacterOffset& characterOffset)
{
    ASSERT(isMainThread());

    if (characterOffset.isNull())
        return;

    if (auto* cache = characterOffset.node->document().axObjectCache())
        m_data = cache->textMarkerDataForCharacterOffset(characterOffset);
}

AXTextMarker::operator VisiblePosition() const
{
    ASSERT(isMainThread());

    WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID());
    if (!cache)
        return { };

    return cache->visiblePositionForTextMarkerData(m_data);
}

AXTextMarker::operator CharacterOffset() const
{
    ASSERT(isMainThread());

    if (isIgnored() || isNull())
        return { };

    WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(m_data.axTreeID());
    if (!cache)
        return { };

    RefPtr object = cache->objectForID(m_data.axObjectID());
    if (!object)
        return { };

    CharacterOffset result(object->node(), m_data.characterStart, m_data.characterOffset);
    // When we are at a line wrap and the VisiblePosition is upstream, it means the text marker is at the end of the previous line.
    // We use the previous CharacterOffset so that it will match the Range.
    if (m_data.affinity == Affinity::Upstream)
        return cache->previousCharacterOffset(result, false);
    return result;
}

bool AXTextMarker::hasSameObjectAndOffset(const AXTextMarker& other) const
{
    return offset() == other.offset() && objectID() == other.objectID() && treeID() == other.treeID();
}

static Node* nodeAndOffsetForReplacedNode(Node& replacedNode, int& offset, int characterCount)
{
    // Use this function to include the replaced node itself in the range we are creating.
    auto nodeRange = AXObjectCache::rangeForNodeContents(replacedNode);
    bool isInNode = static_cast<unsigned>(characterCount) <= WebCore::characterCount(nodeRange);
    offset = replacedNode.computeNodeIndex() + (isInNode ? 0 : 1);
    return replacedNode.parentNode();
}

std::optional<BoundaryPoint> AXTextMarker::boundaryPoint() const
{
    ASSERT(isMainThread());

    CharacterOffset characterOffset = *this;
    if (characterOffset.isNull())
        return std::nullopt;

    int offset = characterOffset.startIndex + characterOffset.offset;
    RefPtr node = characterOffset.node;
    ASSERT(node);
    if (AccessibilityObject::replacedNodeNeedsCharacter(node.get()) || (node && node->hasTagName(HTMLNames::brTag)))
        node = nodeAndOffsetForReplacedNode(*node, offset, characterOffset.offset);
    if (!node)
        return std::nullopt;
    return { { *node, static_cast<unsigned>(offset) } };
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
RefPtr<AXIsolatedObject> AXTextMarker::isolatedObject() const
{
    return dynamicDowncast<AXIsolatedObject>(object());
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

RefPtr<AXCoreObject> AXTextMarker::object() const
{
    if (isNull())
        return nullptr;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainThread()) {
        auto tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID()));
        return tree ? tree->objectForID(objectID()) : nullptr;
    }
#endif
    auto tree = std::get<WeakPtr<AXObjectCache>>(axTreeForID(treeID()));
    return tree ? tree->objectForID(objectID()) : nullptr;
}

String AXTextMarker::debugDescription() const
{
    auto separator = ", "_s;
    RefPtr object = this->object();
    return makeString(
        "treeID "_s, treeID().loggingString()
        , separator, "objectID "_s, objectID().loggingString()
        , separator, "role "_s, object ? accessibilityRoleToString(object->roleValue()) : "no object"_str
        , isIgnored() ? makeString(separator, "ignored"_s) : ""_s
        , separator, "anchor "_s, m_data.anchorType
        , separator, "affinity "_s, m_data.affinity
        , separator, "offset "_s, m_data.offset
        , separator, "characterStart "_s, m_data.characterStart
        , separator, "characterOffset "_s, m_data.characterOffset
    );
}

AXTextMarkerRange::AXTextMarkerRange(const VisiblePositionRange& range)
    : m_start(range.start)
    , m_end(range.end)
{
    ASSERT(isMainThread());
}

AXTextMarkerRange::AXTextMarkerRange(const std::optional<SimpleRange>& range)
{
    ASSERT(isMainThread());

    if (!range)
        return;

    auto* cache = range->start.document().axObjectCache();
    if (!cache)
        return;

    m_start = AXTextMarker(cache->startOrEndCharacterOffsetForRange(*range, true));
    m_end = AXTextMarker(cache->startOrEndCharacterOffsetForRange(*range, false));
}

AXTextMarkerRange::AXTextMarkerRange(const AXTextMarker& start, const AXTextMarker& end)
{
    bool reverse = is_gt(partialOrder(start, end));
    m_start = reverse ? end : start;
    m_end = reverse ? start : end;
}

AXTextMarkerRange::AXTextMarkerRange(AXTextMarker&& start, AXTextMarker&& end)
{
    bool reverse = is_gt(partialOrder(start, end));
    m_start = reverse ? WTFMove(end) : WTFMove(start);
    m_end = reverse ? WTFMove(start) : WTFMove(end);
}

AXTextMarkerRange::AXTextMarkerRange(AXID treeID, AXID objectID, unsigned start, unsigned end)
{
    if (start > end)
        std::swap(start, end);
    m_start = AXTextMarker({ treeID, objectID, start, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, start });
    m_end = AXTextMarker({ treeID, objectID, end, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, end });
}

AXTextMarkerRange::operator VisiblePositionRange() const
{
    ASSERT(isMainThread());
    if (!m_start || !m_end)
        return { };
    return { m_start, m_end };
}

std::optional<SimpleRange> AXTextMarkerRange::simpleRange() const
{
    ASSERT(isMainThread());

    auto startBoundaryPoint = m_start.boundaryPoint();
    if (!startBoundaryPoint)
        return std::nullopt;
    auto endBoundaryPoint = m_end.boundaryPoint();
    if (!endBoundaryPoint)
        return std::nullopt;
    return { { *startBoundaryPoint, *endBoundaryPoint } };
}

std::optional<CharacterRange> AXTextMarkerRange::characterRange() const
{
    if (m_start.m_data.objectID != m_end.m_data.objectID
        || UNLIKELY(m_start.m_data.treeID != m_end.m_data.treeID))
        return std::nullopt;

    if (m_start.m_data.characterOffset > m_end.m_data.characterOffset) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return { { m_start.m_data.characterOffset, m_end.m_data.characterOffset - m_start.m_data.characterOffset } };
}

std::optional<AXTextMarkerRange> AXTextMarkerRange::intersectionWith(const AXTextMarkerRange& other) const
{
    if (UNLIKELY(m_start.m_data.treeID != m_end.m_data.treeID
        || other.m_start.m_data.treeID != other.m_end.m_data.treeID
        || m_start.m_data.treeID != other.m_start.m_data.treeID))
        return std::nullopt;

    // Fast path: both ranges span one object
    if (m_start.m_data.objectID == m_end.m_data.objectID
        && other.m_start.m_data.objectID == other.m_end.m_data.objectID) {
        if (m_start.m_data.objectID != other.m_start.m_data.objectID)
            return std::nullopt;

        unsigned startOffset = std::max(m_start.m_data.characterOffset, other.m_start.m_data.characterOffset);
        unsigned endOffset = std::min(m_end.m_data.characterOffset, other.m_end.m_data.characterOffset);

        if (startOffset > endOffset)
            return std::nullopt;

        return { {
            AXTextMarker({ m_start.treeID(), m_start.objectID(), startOffset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, startOffset }),
            AXTextMarker({ m_start.treeID(), m_start.objectID(), endOffset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, endOffset })
        } };
    }

    return Accessibility::retrieveValueFromMainThread<std::optional<AXTextMarkerRange>>([this, &other] () -> std::optional<AXTextMarkerRange> {
        auto intersection = WebCore::intersection(*this, other);
        if (intersection.isNull())
            return std::nullopt;

        return { AXTextMarkerRange(intersection) };
    });
}

String AXTextMarkerRange::debugDescription() const
{
    return makeString("start: {"_s, m_start.debugDescription(), "}\nend:   {"_s, m_end.debugDescription(), '}');
}

std::partial_ordering partialOrder(const AXTextMarker& marker1, const AXTextMarker& marker2)
{
    if (marker1.objectID() == marker2.objectID() && LIKELY(marker1.treeID() == marker2.treeID())) {
        if (LIKELY(marker1.m_data.characterOffset < marker2.m_data.characterOffset))
            return std::partial_ordering::less;
        if (marker1.m_data.characterOffset > marker2.m_data.characterOffset)
            return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return marker1.partialOrderByTraversal(marker2);
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    auto result = std::partial_ordering::unordered;
    Accessibility::performFunctionOnMainThreadAndWait([&] () {
        auto startBoundaryPoint = marker1.boundaryPoint();
        if (!startBoundaryPoint)
            return;
        auto endBoundaryPoint = marker2.boundaryPoint();
        if (!endBoundaryPoint)
            return;
        result = treeOrder<ComposedTree>(*startBoundaryPoint, *endBoundaryPoint);
    });
    return result;
}

bool AXTextMarkerRange::isConfinedTo(AXID objectID) const
{
    return m_start.objectID() == objectID
        && m_end.objectID() == objectID
        && LIKELY(m_start.treeID() == m_end.treeID());
}

#if ENABLE(AX_THREAD_TEXT_APIS)
static void appendChildren(RefPtr<AXCoreObject> object, bool isForward, RefPtr<AXCoreObject> startObject, AccessibilityObject::AccessibilityChildrenVector& vector)
{
    AccessibilityObject::AccessibilityChildrenVector captionAndRows;
    bool isExposedTable = object->isTable() && object->isExposable();
    if (isExposedTable) {
        // Only consider the caption and rows as potential text-run yielding children. This is necessary because the
        // current table AX hierarchy scheme involves adding multiple different types of objects (rows, columns) that
        // each have the same cells (and thus the same text) as their children.
        for (const auto& child : object->children()) {
            if (child->roleValue() == AccessibilityRole::Caption) {
                captionAndRows.append(child);
                break;
            }
        }
        captionAndRows.appendVector(object->rows());
    }

    const auto& children = isExposedTable ? captionAndRows : object->children();
    size_t childrenSize = children.size();

    size_t startIndex = isForward ? childrenSize : 0;
    size_t endIndex = isForward ? 0 : childrenSize;
    size_t searchPosition = startObject ? children.find(startObject) : notFound;

    if (searchPosition != notFound) {
        if (isForward)
            endIndex = searchPosition + 1;
        else
            endIndex = searchPosition;
    }

    auto append = [&vector] (RefPtr<AXCoreObject> object) {
        if (object)
            vector.append(WTFMove(object));
    };

    if (isForward) {
        for (size_t i = startIndex; i > endIndex; i--)
            append(children.at(i - 1));
    } else {
        for (size_t i = startIndex; i < endIndex; i++)
            append(children.at(i));
    }
}

// Finds the next object with text runs in the given direction, optionally stopping at the given ID and returning std::nullopt.
// You may optionally pass a lambda that runs each time an object is "exited" in the traversal, i.e. we processed its children
// (if present) and are moving beyond it. This can help mirror TextIterator::exitNode in the contexts where that's necessary.
static AXIsolatedObject* findObjectWithRuns(AXIsolatedObject& start, AXDirection direction, std::optional<AXID> stopAtID = std::nullopt, const std::function<void(AXIsolatedObject&)>& exitObject = [] (AXIsolatedObject&) { })
{
    RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(start.treeID()));
    // `root` is a stand-in for `anchorObject` in findMatchingObjects, which this function partially copies from.
    RefPtr root = tree ? tree->rootNode() : nullptr;
    if (!root)
        return nullptr;

    // FIXME: aria-owns breaks this function, as aria-owns causes the AX tree to be changed, affecting
    // our search below, but it doesn't actually change text position on the page. So we need to ignore
    // aria-owns tree changes here in order to behave correctly. We also probably need to do something
    // about text within aria-hidden containers, which affects the AX tree.

    // This search algorithm only searches the elements before/after the starting object.
    // It does this by stepping up the parent chain and at each level doing a DFS.
    RefPtr startObject = &start;

    bool isForward = direction == AXDirection::Next;
    // The first iteration of the outer loop will examine the children of the start object for matches. However, when
    // iterating backwards, the start object children should not be considered, so the loop is skipped ahead. We make an
    // exception when no start object was specified because we want to search everything regardless of search direction.
    RefPtr<AXCoreObject> previousObject;
    if (!isForward && startObject != root.get()) {
        previousObject = startObject;
        startObject = startObject->parentObjectUnignored();
    }

    for (auto* stopObject = root->parentObjectUnignored(); startObject && startObject != stopObject; startObject = startObject->parentObjectUnignored()) {
        if (stopAtID && stopAtID->isValid() && startObject->objectID() == *stopAtID)
            return nullptr;
        // Only append the children after/before the previous element, so that the search does not check elements that are
        // already behind/ahead of start element.
        AXCoreObject::AccessibilityChildrenVector searchStack;
        appendChildren(startObject, isForward, previousObject, searchStack);

        // This now does a DFS at the current level of the parent.
        while (!searchStack.isEmpty()) {
            RefPtr searchObject = searchStack.takeLast();

            if (stopAtID && stopAtID->isValid() && searchObject->objectID() == *stopAtID)
                return nullptr;

            if (searchObject->hasTextRuns())
                return dynamicDowncast<AXIsolatedObject>(searchObject.get());

            appendChildren(searchObject, isForward, nullptr, searchStack);
        }

        // When moving backwards, the parent object needs to be checked, because technically it's "before" the starting element.
        if (!isForward && startObject != root.get() && startObject->hasTextRuns())
            return startObject.get();

        exitObject(*startObject);
        previousObject = startObject;
    }
    return nullptr;
}

AXTextRunLineID AXTextMarker::lineID() const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().lineID();

    const auto* runs = this->runs();
    size_t runIndex = runs->indexForOffset(offset());
    return runIndex != notFound ? runs->lineID(runIndex) : AXTextRunLineID();
}

int AXTextMarker::lineIndex() const
{
    if (!isValid())
        return -1;
    if (!isInTextRun())
        return toTextRunMarker().lineIndex();

    AXTextMarker startMarker;
    RefPtr object = isolatedObject();
    if (object->isTextControl())
        startMarker = { object->treeID(), object->objectID(), 0 };
    else if (auto* editableAncestor = object->editableAncestor())
        startMarker = { editableAncestor->treeID(), editableAncestor->objectID(), 0 };
    else if (RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID())))
        startMarker = tree->firstMarker();
    else
        return -1;

    auto currentLineID = startMarker.lineID();
    auto targetLineID = lineID();
    if (currentLineID == targetLineID)
        return 0;

    auto currentMarker = WTFMove(startMarker);
    if (!currentMarker.atLineEnd()) {
        // Start from a line end, so that subsequent calls to nextLineEnd() yield a new line.
        // Otherwise if we started from the middle of a line, we would count the the first line twice.
        auto nextLineEndMarker = currentMarker.nextLineEnd();
        RELEASE_ASSERT(nextLineEndMarker.lineID() == currentMarker.lineID());
        currentMarker = WTFMove(nextLineEndMarker);
    }

    unsigned index = 0;
    while (currentLineID && currentLineID != targetLineID) {
        currentMarker = currentMarker.nextLineEnd();
        currentLineID = currentMarker.lineID();
        ++index;
    }
    return index;
}

CharacterRange AXTextMarker::characterRangeForLine(unsigned lineIndex) const
{
    if (!isValid())
        return { };

    RefPtr object = isolatedObject();
    if (!object || !object->isTextControl())
        return { };
    // This implementation doesn't respect the offset as the only known callsite hardcodes zero. We'll need to make changes to support this if a usecase arrives for it.
    RELEASE_ASSERT(!offset());

    auto* stopObject = object->siblingOrParent(AXDirection::Next);
    auto stopAtID = stopObject ? std::make_optional(stopObject->objectID()) : std::nullopt;

    auto textRunMarker = toTextRunMarker(stopAtID);
    // If we couldn't convert this object to a text-run marker, it means we are a text control with no text descendant.
    if (!textRunMarker.isValid())
        return { };

    unsigned precedingLength = 0;
    auto currentLineRange = textRunMarker.lineRange(LineRangeType::Current);
    while (lineIndex && currentLineRange) {
        precedingLength += currentLineRange.toString().length();
        auto lineEndMarker = currentLineRange.end().nextLineEnd(stopAtID);
        currentLineRange = { lineEndMarker.previousLineStart(stopAtID), WTFMove(lineEndMarker) };
        --lineIndex;
    }
    return currentLineRange ? CharacterRange(precedingLength, currentLineRange.toString().length()) : CharacterRange();
}

AXTextMarkerRange AXTextMarker::markerRangeForLineIndex(unsigned lineIndex) const
{
    // This implementation doesn't respect the offset as the only known callsite hardcodes zero. We'll need to make changes to support this if a usecase arrives for it.
    RELEASE_ASSERT(!offset());

    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().markerRangeForLineIndex(lineIndex);

    auto currentLineRange = lineRange(LineRangeType::Current);
    while (lineIndex && currentLineRange) {
        auto lineEndMarker = currentLineRange.end().nextLineEnd();
        currentLineRange = { lineEndMarker.previousLineStart(), WTFMove(lineEndMarker) };
        --lineIndex;
    }
    return currentLineRange;
}

int AXTextMarker::lineNumberForIndex(unsigned index) const
{
    RefPtr object = isolatedObject();
    if (!object)
        return -1;
    auto* stopObject = object->siblingOrParent(AXDirection::Next);
    auto stopAtID = stopObject ? std::make_optional(stopObject->objectID()) : std::nullopt;

    // To match the behavior of the VisiblePosition implementation of this functionality, we need to
    // check an extra position ahead (as tested by ax-thread-text-apis/textarea-line-for-index.html),
    // so increment index.
    ++index;

    unsigned lineIndex = 0;
    auto currentMarker = *this;
    while (index) {
        auto oldMarker = WTFMove(currentMarker);
        currentMarker = oldMarker.findMarker(AXDirection::Next, stopAtID);
        if (!currentMarker.isValid())
            break;

        object = currentMarker.isolatedObject();
        // Skip line breaks to avoid double-counting a line change.
        if (object && object->roleValue() == AccessibilityRole::LineBreak)
            continue;

        if (oldMarker.lineID() != currentMarker.lineID())
            ++lineIndex;

        --index;
    }
    // Only return the line number if the index was a valid offset into our descendants.
    return !index ? lineIndex : -1;
}

bool AXTextMarker::atLineBoundaryForDirection(AXDirection direction) const
{
    auto adjacentMarker = findMarker(direction);
    return adjacentMarker.lineID() != lineID();
}

unsigned AXTextMarker::offsetFromRoot() const
{
    RELEASE_ASSERT(!isMainThread());

    if (!isValid())
        return 0;
    RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID()));
    if (RefPtr root = tree ? tree->rootNode() : nullptr) {
        AXTextMarker rootMarker { root->treeID(), root->objectID(), 0 };
        unsigned offset = 0;
        auto current = rootMarker.toTextRunMarker();
        while (current.isValid() && !hasSameObjectAndOffset(current)) {
            current = current.findMarker(AXDirection::Next);
            offset++;
        }
        // If this assert fails, it means we couldn't navigate from root to `this`, which should never happen.
        RELEASE_ASSERT(hasSameObjectAndOffset(current));
        return offset;
    }
    return 0;
}

AXTextMarker AXTextMarker::nextMarkerFromOffset(unsigned offset) const
{
    RELEASE_ASSERT(!isMainThread());

    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().nextMarkerFromOffset(offset);

    auto marker = *this;
    while (offset) {
        if (auto newMarker = marker.findMarker(AXDirection::Next))
            marker = WTFMove(newMarker);
        else
            break;

        --offset;
    }
    return marker;
}

AXTextMarker AXTextMarker::findLastBefore(std::optional<AXID> stopAtID) const
{
    RELEASE_ASSERT(!isMainThread());

    if (!isValid())
        return { };
    if (!isInTextRun()) {
        auto textRunMarker = toTextRunMarker();
        // We couldn't turn this non-text-run marker into a marker pointing to actual text, e.g. because
        // this marker points at an empty container / group at the end of the document. In this case, we
        // call ourselves the last marker.
        if (!textRunMarker.isValid())
            return *this;
        return textRunMarker.findLastBefore(stopAtID);
    }

    AXTextMarker marker;
    auto newMarker = *this;
    // FIXME: Do we need to compare both tree ID and object ID here?
    while (newMarker.isValid() && (!stopAtID || !stopAtID->isValid() || *stopAtID != newMarker.objectID())) {
        marker = WTFMove(newMarker);
        newMarker = marker.findMarker(AXDirection::Next, stopAtID);
    }
    return marker;
}

String AXTextMarkerRange::toString() const
{
    RELEASE_ASSERT(!isMainThread());

    auto start = m_start.toTextRunMarker();
    if (!start.isValid())
        return emptyString();
    auto end = m_end.toTextRunMarker();
    if (!end.isValid())
        return emptyString();

    if (start.isolatedObject() == end.isolatedObject()) {
        size_t minOffset = std::min(start.offset(), end.offset());
        size_t maxOffset = std::max(start.offset(), end.offset());
        return start.runs()->substring(minOffset, maxOffset - minOffset);
    }

    StringBuilder result;
    auto emitNewlineOnExit = [&] (AXIsolatedObject& object) {
        if (!object.shouldEmitNewlinesBeforeAndAfterNode())
            return;

        // Like TextIterator, don't emit a newline if the most recently emitted character was already a newline.
        if (result.length() && result[result.length() - 1] != '\n')
            result.append('\n');
    };

    result.append(start.runs()->substring(start.offset()));

    // FIXME: If we've been given reversed markers, i.e. the end marker actually comes before the start marker,
    // we may want to detect this and try searching AXDirection::Previous?
    RefPtr current = findObjectWithRuns(*start.isolatedObject(), AXDirection::Next, std::nullopt, emitNewlineOnExit);
    while (current && current->objectID() != end.objectID()) {
        const auto* runs = current->textRuns();
        for (unsigned i = 0; i < runs->size(); i++)
            result.append(runs->at(i).text);
        current = findObjectWithRuns(*current, AXDirection::Next, std::nullopt, emitNewlineOnExit);
    }
    result.append(end.runs()->substring(0, end.offset()));
    return result.toString();
}

const AXTextRuns* AXTextMarker::runs() const
{
    RefPtr object = isolatedObject();
    return object ? object->textRuns() : nullptr;
}

AXTextMarker AXTextMarker::findMarker(AXDirection direction, std::optional<AXID> stopAtID) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().findMarker(direction, stopAtID);

    if ((direction == AXDirection::Next && offset() < runs()->totalLength()) || (direction == AXDirection::Previous && offset() > 0))
        return { treeID(), objectID(), direction == AXDirection::Next ? offset() + 1 : offset() - 1 };

    // offset() pointed to the last character in the given object's runs, so let's traverse to find the next object with runs.
    if (RefPtr object = findObjectWithRuns(*isolatedObject(), direction, stopAtID)) {
        RELEASE_ASSERT(direction == AXDirection::Next ? object->textRuns()->runLength(0) : object->textRuns()->lastRunLength());
        return { object->treeID(), object->objectID(), direction == AXDirection::Next ? 0 : object->textRuns()->lastRunLength() };
    }

    return { };
}

AXTextMarker AXTextMarker::findMarker(AXDirection direction, AXTextUnit textUnit, AXTextUnitBoundary boundary, std::optional<AXID> stopAtID) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker(stopAtID).findMarker(direction, textUnit, boundary, stopAtID);

    if (textUnit == AXTextUnit::Line) {
        // If, for example, we are asked to find the next line end, and are at the very end of a line already,
        // we need the end position of the next line instead. Determine this by checking the next or previous marker.
        auto adjacentMarker = findMarker(direction, stopAtID);
        if (adjacentMarker.lineID() != lineID()) {
            bool findOnNextLine = (direction == AXDirection::Previous && boundary == AXTextUnitBoundary::Start)
                || (direction == AXDirection::Next && boundary == AXTextUnitBoundary::End);

            if (findOnNextLine)
                return adjacentMarker.findMarker(direction, textUnit, boundary, stopAtID);
        }

        size_t runIndex = runs()->indexForOffset(offset());
        RELEASE_ASSERT(runIndex != notFound);
        RefPtr currentObject = isolatedObject();
        const auto* currentRuns = currentObject->textRuns();

        auto computeOffset = [&] (size_t runEndOffset, size_t runLength) {
            // This works because `runEndOffset` is the offset pointing to the end of the given run, which includes the length of all runs preceding it. So subtracting that from the length of the current run gives us an offset to the start of the current run.
            return boundary == AXTextUnitBoundary::End ? runEndOffset : runEndOffset - runLength;
        };
        auto linePosition = AXTextMarker(treeID(), objectID(), computeOffset(currentRuns->runLengthSumTo(runIndex), currentRuns->runLength(runIndex)));
        auto startLineID = currentRuns->lineID(runIndex);
        // We found the start run and associated line, now iterate until we find a line boundary.
        while (currentObject) {
            RELEASE_ASSERT(currentRuns->size());
            unsigned cumulativeOffset = 0;
            for (size_t i = 0; i < currentRuns->size(); i++) {
                cumulativeOffset += currentRuns->runLength(i);
                if (currentRuns->lineID(i) != startLineID)
                    return linePosition;
                linePosition = AXTextMarker(currentObject->treeID(), currentObject->objectID(), computeOffset(cumulativeOffset, currentRuns->runLength(i)));
            }
            currentObject = findObjectWithRuns(*currentObject, direction, stopAtID);
            if (currentObject)
                currentRuns = currentObject->textRuns();
        }
        return linePosition;
    }
    // FIXME: Implement the other AXTextUnits.

    return { };
}

AXTextMarker AXTextMarker::toTextRunMarker(std::optional<AXID> stopAtID) const
{
    if (!isValid() || isInTextRun()) {
        // If something has constructed a text-run marker, it should've done so with an in-bounds offset.
        RELEASE_ASSERT(!isValid() || isolatedObject()->textRuns()->totalLength() >= offset());
        return *this;
    }

    // Find the node our offset points to. For example:
    // AXTextMarker { ID 1: Group, Offset 6 }
    // ID 1: Group
    //  - ID 2: Foo
    //  - ID 3: Line1
    //          Line2
    // Calling toTextRunMarker() on the original marker should yield new marker:
    // AXTextMarker { ID 3: StaticText, Offset 3 }
    // Because we had to walk over ID 2 which had length 3 text.
    size_t precedingOffset = 0;
    RefPtr start = isolatedObject();
    RefPtr current = start->hasTextRuns() ? WTFMove(start) : findObjectWithRuns(*start, AXDirection::Next, stopAtID);
    while (current) {
        unsigned totalLength = current->textRuns()->totalLength();
        if (precedingOffset + totalLength >= offset())
            break;
        precedingOffset += totalLength;
        current = findObjectWithRuns(*current, AXDirection::Next, stopAtID);
    }

    if (!current)
        return { };

    RELEASE_ASSERT(offset() >= precedingOffset);
    return { current->treeID(), current->objectID(), static_cast<unsigned>(offset() - precedingOffset) };
}

bool AXTextMarker::isInTextRun() const
{
    const auto* runs = this->runs();
    return runs && runs->size();
}

AXTextMarkerRange AXTextMarker::lineRange(LineRangeType type) const
{
    if (!isValid())
        return { { }, { } };

    if (type == LineRangeType::Current) {
        auto startMarker = atLineStart() ? *this : previousLineStart();
        auto endMarker = atLineEnd() ? *this : nextLineEnd();

        return { WTFMove(startMarker), WTFMove(endMarker) };
    }
    // FIXME: The other types aren't implemented yet.
    RELEASE_ASSERT_NOT_REACHED();
}

std::partial_ordering AXTextMarker::partialOrderByTraversal(const AXTextMarker& other) const
{
    RELEASE_ASSERT(!isMainThread());

    if (hasSameObjectAndOffset(other))
        return std::partial_ordering::equivalent;
    if (!isValid() || !other.isValid())
        return std::partial_ordering::unordered;

    auto foundOtherInDirection = [&] (AXDirection direction) {
        auto current = *this;
        while (current.isValid()) {
            current = current.findMarker(direction);
            if (current.hasSameObjectAndOffset(other))
                return true;
        }
        return false;
    };

    // `other` comes after us in tree order since we found it by traversing AXDirection::Next.
    if (foundOtherInDirection(AXDirection::Next))
        return std::partial_ordering::less;
    if (foundOtherInDirection(AXDirection::Previous))
        return std::partial_ordering::greater;

    RELEASE_ASSERT_NOT_REACHED();
    return std::partial_ordering::unordered;
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

} // namespace WebCore
