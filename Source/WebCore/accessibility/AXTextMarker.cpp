/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "AXLogger.h"
#include "AXObjectCache.h"
#include "AXTreeStore.h"
#include "HTMLInputElement.h"
#include "RenderObject.h"
#include "TextIterator.h"

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXTextMarker);

TextMarkerData::TextMarkerData(AXObjectCache& cache, Node* nodeParam, const VisiblePosition& visiblePosition, int charStart, int charOffset, bool ignoredParam)
{
    initializeAXIDs(cache, nodeParam);

    node = nodeParam;
    auto position = visiblePosition.deepEquivalent();
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = position.anchorType();
    affinity = visiblePosition.affinity();

    characterStart = std::max(charStart, 0);
    characterOffset = std::max(charOffset, 0);
    ignored = ignoredParam;
}

TextMarkerData::TextMarkerData(AXObjectCache& cache, const CharacterOffset& characterOffset, bool ignoredParam)
{
    initializeAXIDs(cache, characterOffset.node);

    node = characterOffset.node;
    auto visiblePosition = cache.visiblePositionFromCharacterOffset(characterOffset);
    auto position = visiblePosition.deepEquivalent();
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    // When creating from a CharacterOffset, always set the anchorType to PositionIsOffsetInAnchor.
    anchorType = Position::PositionIsOffsetInAnchor;
    affinity = visiblePosition.affinity();

    characterStart = std::max(characterOffset.startIndex, 0);
    this->characterOffset = std::max(characterOffset.offset, 0);
    ignored = ignoredParam;
}

void TextMarkerData::initializeAXIDs(AXObjectCache& cache, Node* node)
{
    memset(static_cast<void*>(this), 0, sizeof(*this));

    treeID = cache.treeID().toUInt64();
    if (RefPtr object = cache.getOrCreate(node))
        objectID = object->objectID().toUInt64();
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
        m_data = *data;
}

AXTextMarker::AXTextMarker(const CharacterOffset& characterOffset)
{
    ASSERT(isMainThread());

    if (characterOffset.isNull())
        return;

    if (auto* cache = characterOffset.node->document().axObjectCache())
        m_data = cache->textMarkerDataForCharacterOffset(characterOffset);
}

void AXTextMarker::setNodeIfNeeded() const
{
    ASSERT(isMainThread());
    if (m_data.node)
        return;

    WeakPtr cache = std::get<WeakPtr<AXObjectCache>>(axTreeForID(treeID()));
    if (!cache)
        return;

    auto* object = cache->objectForID(objectID());
    if (!object)
        return;

    WeakPtr node = object->node();
    if (!node)
        return;

    const_cast<AXTextMarker*>(this)->m_data.node = node.get();
    cache->setNodeInUse(node.get());
}

AXTextMarker::operator VisiblePosition() const
{
    ASSERT(isMainThread());

    WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID());
    if (!cache)
        return { };

    setNodeIfNeeded();
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

    if (m_data.node) {
        // Make sure that this node is still in cache->m_textMarkerNodes. Since this method can be called as a result of a dispatch from the AX thread, the Node may have gone away in a previous main loop cycle.
        if (!cache->isNodeInUse(m_data.node))
            return { };
    } else
        setNodeIfNeeded();

    CharacterOffset result(m_data.node, m_data.characterStart, m_data.characterOffset);
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
    WeakPtr node = characterOffset.node;
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
    auto separator = ", ";
    RefPtr object = this->object();
    return makeString(
        "treeID ", treeID().loggingString()
        , separator, "objectID ", objectID().loggingString()
        , separator, "role ", object ? accessibilityRoleToString(object->roleValue()) : String("no object"_s)
        , isIgnored() ? makeString(separator, "ignored") : ""_s
        , isMainThread() && node() ? makeString(separator, node()->debugDescription()) : ""_s
        , separator, "anchor ", m_data.anchorType
        , separator, "affinity ", m_data.affinity
        , separator, "offset ", m_data.offset
        , separator, "characterStart ", m_data.characterStart
        , separator, "characterOffset ", m_data.characterOffset
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

AXTextMarkerRange::AXTextMarkerRange(AXID treeID, AXID objectID, unsigned start, unsigned end)
{
    if (start > end)
        std::swap(start, end);
    m_start = AXTextMarker({ treeID, objectID, nullptr, start, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, start });
    m_end = AXTextMarker({ treeID, objectID, nullptr, end, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, end });
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
        || m_start.m_data.treeID != m_end.m_data.treeID)
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
        || m_start.m_data.treeID != other.m_start.m_data.treeID)) {
        return std::nullopt;
    }

    // Fast path: both ranges span one object
    if (m_start.m_data.objectID == m_end.m_data.objectID
        && other.m_start.m_data.objectID == other.m_end.m_data.objectID) {
        if (m_start.m_data.objectID != other.m_start.m_data.objectID)
            return std::nullopt;

        unsigned startOffset = std::max(m_start.m_data.characterOffset, other.m_start.m_data.characterOffset);
        unsigned endOffset = std::min(m_end.m_data.characterOffset, other.m_end.m_data.characterOffset);

        if (startOffset > endOffset)
            return std::nullopt;

        auto startMarker = AXTextMarker({ m_start.treeID(), m_start.objectID(), nullptr, startOffset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, startOffset });
        auto endMarker = AXTextMarker({ m_start.treeID(), m_start.objectID(), nullptr, endOffset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, endOffset });
        return { { startMarker, endMarker } };
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
    return makeString("start: {", m_start.debugDescription(), "}\nend:   {", m_end.debugDescription(), "}");
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
// Finds the next object with text runs in the given direction, optionally stopping at the given ID and returning std::nullopt.
static RefPtr<AXIsolatedObject> findObjectWithRuns(AXIsolatedObject& start, AXDirection direction, std::optional<AXID> stopAtID = std::nullopt)
{
    // FIXME: aria-owns breaks this function, as aria-owns causes the AX tree to be changed, affecting
    // our search below, but it doesn't actually change text position on the page. So we need to ignore
    // aria-owns tree changes here in order to behave correctly. We also probably need to do something
    // about text within aria-hidden containers, which affects the AX tree.

    AccessibilitySearchCriteria criteria { &start, direction == AXDirection::Next ? AccessibilitySearchDirection::Next : AccessibilitySearchDirection::Previous, emptyString(), 1, false, false };
    RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(start.treeID()));
    RefPtr root = tree ? tree->rootNode() : nullptr;
    if (!root)
        return nullptr;
    criteria.anchorObject = root.get();
    criteria.searchKeys = { AccessibilitySearchKey::HasTextRuns };
    if (stopAtID)
        criteria.stopAtID = *stopAtID;

    AXCoreObject::AccessibilityChildrenVector results;
    Accessibility::findMatchingObjects(criteria, results);
    return results.isEmpty() ? nullptr : dynamicDowncast<AXIsolatedObject>(results[0]);
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
        auto current = rootMarker.toTextLeafMarker();
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
    if (!isInTextLeaf())
        return toTextLeafMarker().nextMarkerFromOffset(offset);

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
    if (!isInTextLeaf()) {
        auto textLeafMarker = toTextLeafMarker();
        // We couldn't turn this non-text-leaf marker into a marker pointing to actual text, e.g. because
        // this marker points at an empty container / group at the end of the document. In this case, we
        // call ourselves the last marker.
        if (!textLeafMarker.isValid())
            return *this;
        return textLeafMarker.findLastBefore(stopAtID);
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

    auto start = m_start.toTextLeafMarker();
    if (!start.isValid())
        return emptyString();
    auto end = m_end.toTextLeafMarker();
    if (!end.isValid())
        return emptyString();

    if (start.isolatedObject() == end.isolatedObject()) {
        size_t minOffset = std::min(start.offset(), end.offset());
        size_t maxOffset = std::max(start.offset(), end.offset());
        return start.runs()->substring(minOffset, maxOffset - minOffset);
    }

    StringBuilder result;
    result.append(start.runs()->substring(start.offset()));

    // FIXME: If we've been given reversed markers, i.e. the end marker actually comes before the start marker,
    // we may want to detect this and try searching AXDirection::Previous?
    RefPtr current = findObjectWithRuns(*start.isolatedObject(), AXDirection::Next);
    while (current && current->objectID() != end.objectID()) {
        const auto* runs = current->textRuns();
        for (unsigned i = 0; i < runs->size(); i++)
            result.append(runs->at(i).text);
        current = findObjectWithRuns(*current, AXDirection::Next);
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
    if (!isInTextLeaf())
        return toTextLeafMarker().findMarker(direction);

    size_t runIndex = runs()->indexForOffset(offset());
    RELEASE_ASSERT(runIndex != notFound);
    auto hasMoreTextInCurrentRun = [&] {
        if (direction == AXDirection::Next)
            return offset() < runs()->at(runIndex).text.length();
        return offset() > 0;
    };
    bool hasAnotherRun = runIndex + 1 < runs()->size();
    // The next run should not have empty text, because we're going to return a position containing offset() + 1, which would be wrong.
    RELEASE_ASSERT(direction == AXDirection::Previous || !hasAnotherRun || runs()->runLength(runIndex + 1) > 0);
    // Checking for the presence of another run is only relevant for moving AXDirection::Next, as just checking that offset() > 0 is sufficient for AXDirection::Previous.
    if (hasMoreTextInCurrentRun() || (direction == AXDirection::Next && hasAnotherRun)) {
        // Return an offset to the next character in the current run.
        return { treeID(), objectID(), direction == AXDirection::Next ? offset() + 1 : offset() - 1 };
    }
    // offset() pointed to the last character in the given object's runs, so let's traverse to find the next object with runs.
    if (RefPtr object = findObjectWithRuns(*this->isolatedObject(), direction, stopAtID)) {
        RELEASE_ASSERT(direction == AXDirection::Next ? object->textRuns()->runLength(0) : object->textRuns()->lastRunLength());
        return { object->treeID(), object->objectID(), direction == AXDirection::Next ? 0 : object->textRuns()->lastRunLength() };
    }

    return { };
}

AXTextMarker AXTextMarker::findMarker(AXDirection direction, AXTextUnit textUnit, AXTextUnitBoundary boundary) const
{
    if (!isValid())
        return { };
    if (!isInTextLeaf())
        return toTextLeafMarker().findMarker(direction, textUnit, boundary);

    if (textUnit == AXTextUnit::Line) {
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
            currentObject = findObjectWithRuns(*currentObject, direction);
            if (currentObject)
                currentRuns = currentObject->textRuns();
        }
        return linePosition;
    }
    // FIXME: Implement the other AXTextUnits.

    return { };
}

AXTextMarker AXTextMarker::toTextLeafMarker() const
{
    if (!isValid() || isInTextLeaf()) {
        // If something has constructed a leaf marker, it should've done so with an in-bounds offset.
        RELEASE_ASSERT(!isValid() || isolatedObject()->textRuns()->totalLength() >= offset());
        return *this;
    }

    // Find the leaf node our offset points to. For example:
    // AXTextMarker { ID 1: Group, Offset 6 }
    // ID 1: Group
    //  - ID 2: Foo
    //  - ID 3: Line1
    //          Line2
    // Calling toTextLeafMarker() on the original marker should yield new marker:
    // AXTextMarker { ID 3: StaticText, Offset 3 }
    // Because we had to walk over ID 2 which had length 3 text.
    size_t precedingOffset = 0;
    RefPtr start = isolatedObject();
    RefPtr current = start->hasTextRuns() ? WTFMove(start) : findObjectWithRuns(*start, AXDirection::Next);
    while (current) {
        unsigned totalLength = current->textRuns()->totalLength();
        if (precedingOffset + totalLength >= offset())
            break;
        precedingOffset += totalLength;
        current = findObjectWithRuns(*current, AXDirection::Next);
    }

    if (!current)
        return { };

    RELEASE_ASSERT(offset() >= precedingOffset);
    return { current->treeID(), current->objectID(), static_cast<unsigned>(offset() - precedingOffset) };
}

bool AXTextMarker::isInTextLeaf() const
{
    RefPtr object = this->isolatedObject();
    // FIXME: Is it possible for non-leaf nodes to have text runs? If so, we don't handle them correctly.
    return !object || (!object->children().size() && object->textRuns());
}

AXTextMarkerRange AXTextMarker::lineRange(LineRangeType type) const
{
    if (!isValid())
        return { { }, { } };

    if (type == LineRangeType::Current)
        return { findMarker(AXDirection::Previous, AXTextUnit::Line, AXTextUnitBoundary::Start), findMarker(AXDirection::Next, AXTextUnit::Line, AXTextUnitBoundary::End) };
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
