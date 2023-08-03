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

    setNodeIfNeeded();
    CharacterOffset result(m_data.node, m_data.characterStart, m_data.characterOffset);
    // When we are at a line wrap and the VisiblePosition is upstream, it means the text marker is at the end of the previous line.
    // We use the previous CharacterOffset so that it will match the Range.
    if (m_data.affinity == Affinity::Upstream) {
        if (WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(m_data.axTreeID()))
            return cache->previousCharacterOffset(result, false);
    }
    return result;
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

#if ENABLE(TREE_DEBUGGING)
String AXTextMarker::debugDescription() const
{
    auto separator = ", ";
    RefPtr object = this->object();
    return makeString(
        "treeID ", treeID().loggingString()
        , separator, "objectID ", objectID().loggingString()
        , separator, "role ", object ? accessibilityRoleToString(object->roleValue()) : String("no object"_s)
        , separator, isMainThread() ? node()->debugDescription()
            : makeString("node 0x", hex(reinterpret_cast<uintptr_t>(m_data.node)))
        , separator, "offset ", m_data.offset
        , separator, "AnchorType ", m_data.anchorType
        , separator, "Affinity ", m_data.affinity
        , separator, "characterStart ", m_data.characterStart
        , separator, "characterOffset ", m_data.characterOffset
        , separator, "isIgnored ", isIgnored()
    );
}
#endif

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

std::partial_ordering partialOrder(const AXTextMarker& marker1, const AXTextMarker& marker2)
{
    if (marker1.objectID() == marker2.objectID() && LIKELY(marker1.treeID() == marker2.treeID())) {
        if (LIKELY(marker1.m_data.characterOffset < marker2.m_data.characterOffset))
            return std::partial_ordering::less;
        if (marker1.m_data.characterOffset > marker2.m_data.characterOffset)
            return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
    }

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

} // namespace WebCore
