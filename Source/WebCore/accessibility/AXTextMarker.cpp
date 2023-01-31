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

#include "AXObjectCache.h"
#include "AXTreeStore.h"
#include "HTMLInputElement.h"
#include "RenderObject.h"

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

AXTextMarker::operator VisiblePosition() const
{
    ASSERT(isMainThread());

    auto* cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID());
    return cache ? cache->visiblePositionForTextMarkerData(m_data) : VisiblePosition();
}

AXTextMarker::operator CharacterOffset() const
{
    ASSERT(isMainThread());

    if (isIgnored() || isNull())
        return { };

    CharacterOffset result(m_data.node, m_data.characterStart, m_data.characterOffset);
    // When we are at a line wrap and the VisiblePosition is upstream, it means the text marker is at the end of the previous line.
    // We use the previous CharacterOffset so that it will match the Range.
    if (m_data.affinity == Affinity::Upstream) {
        if (auto* cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(m_data.axTreeID()))
            return cache->previousCharacterOffset(result, false);
    }
    return result;
}

AXCoreObject* AXTextMarker::object() const
{
    if (isNull())
        return nullptr;

    auto* tree = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID());
    return tree ? tree->objectForID(objectID()) : nullptr;
}

#if ENABLE(TREE_DEBUGGING)
String AXTextMarker::debugDescription() const
{
    if (isNull())
        return "<null>"_s;

    auto separator = ", ";
    return makeString(
        "treeID ", treeID().loggingString()
        , separator, "objectID ", objectID().loggingString()
        , separator, node()->debugDescription()
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

AXTextMarkerRange::AXTextMarkerRange(const AXTextMarker& s, const AXTextMarker& e)
    : m_start(s)
    , m_end(e)
{
}

AXTextMarkerRange::operator VisiblePositionRange() const
{
    ASSERT(isMainThread());
    return { m_start, m_end };
}

std::optional<SimpleRange> AXTextMarkerRange::simpleRange() const
{
    ASSERT(isMainThread());

    auto startBoundaryPoint = makeBoundaryPoint(m_start);
    auto endBoundaryPoint = makeBoundaryPoint(m_end);
    if (!startBoundaryPoint || !endBoundaryPoint)
        return std::nullopt;
    return { { *startBoundaryPoint, *endBoundaryPoint } };
}

} // namespace WebCore
