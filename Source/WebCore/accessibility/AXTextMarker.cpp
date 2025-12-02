/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "AXTreeStoreInlines.h"
#include "AXUtilities.h"
#include "BoundaryPointInlines.h"
#include "HTMLInputElement.h"
#include "Logging.h"
#include "RenderObject.h"
#include "TextBoundaries.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/MakeString.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXTextMarker);
WTF_MAKE_TZONE_ALLOCATED_IMPL(AXTextMarkerRange);

using namespace Accessibility;

static std::optional<AXID> nodeID(AXObjectCache& cache, Node* node)
{
    if (RefPtr object = cache.getOrCreate(node))
        return object->objectID();
    return std::nullopt;
}

TextMarkerData::TextMarkerData(AXObjectCache& cache, const VisiblePosition& visiblePosition, int charStart, int charOffset, bool ignoredParam, TextMarkerOrigin originParam)
{
    ASSERT(isMainThread());
#if ENABLE(AX_THREAD_TEXT_APIS)
    ASSERT(!AXObjectCache::shouldCreateAXThreadCompatibleMarkers());
#endif

    zeroBytes(*this);
    treeID = cache.treeID().toUInt64();
    auto position = visiblePosition.deepEquivalent();
    auto optionalObjectID = nodeID(cache, position.protectedAnchorNode().get());
    objectID = optionalObjectID ? optionalObjectID->toUInt64() : 0;
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = position.anchorType();
    affinity = visiblePosition.affinity();
    characterStart = std::max(charStart, 0);
    characterOffset = std::max(charOffset, 0);
    ignored = ignoredParam;
    origin = originParam;
}

TextMarkerData::TextMarkerData(AXObjectCache& cache, const CharacterOffset& characterOffsetParam, bool ignoredParam, TextMarkerOrigin originParam)
{
    ASSERT(isMainThread());

    zeroBytes(*this);

    auto visiblePosition = cache.visiblePositionFromCharacterOffset(characterOffsetParam);
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::shouldCreateAXThreadCompatibleMarkers()) {
        if (std::optional data = cache.textMarkerDataForVisiblePosition(WTFMove(visiblePosition), origin))
            *this = *data;
        return;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    treeID = cache.treeID().toUInt64();
    auto optionalObjectID = nodeID(cache, characterOffsetParam.node.get());
    objectID = optionalObjectID ? optionalObjectID->toUInt64() : 0;
    auto position = visiblePosition.deepEquivalent();
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = Position::PositionIsOffsetInAnchor;
    affinity = visiblePosition.affinity();
    characterStart = std::max(characterOffsetParam.startIndex, 0);
    characterOffset = std::max(characterOffsetParam.offset, 0);
    ignored = ignoredParam;
    origin = originParam;
}

AXTextMarker::AXTextMarker(const VisiblePosition& visiblePosition, TextMarkerOrigin origin)
{
    ASSERT(isMainThread());

    if (visiblePosition.isNull())
        return;

    RefPtr node = visiblePosition.deepEquivalent().anchorNode();
    ASSERT(node);
    if (!node)
        return;

    CheckedPtr cache = node->protectedDocument()->axObjectCache();
    if (!cache)
        return;

    if (auto data = cache->textMarkerDataForVisiblePosition(visiblePosition, origin))
        m_data = WTFMove(*data);
}

AXTextMarker::AXTextMarker(const CharacterOffset& characterOffset, TextMarkerOrigin origin)
{
    ASSERT(isMainThread());

    if (characterOffset.isNull())
        return;

    if (CheckedPtr cache = characterOffset.node->protectedDocument()->axObjectCache())
        m_data = cache->textMarkerDataForCharacterOffset(characterOffset, origin);
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

    RefPtr object = m_data.axObjectID() ? cache->objectForID(*m_data.axObjectID()) : nullptr;
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
    // Guaranteed not to be null by checking Character::isNull().
    RefPtr node = characterOffset.node;

    int offset = characterOffset.startIndex + characterOffset.offset;
    if (AccessibilityObject::replacedNodeNeedsCharacter(*node) || WebCore::elementName(*node) == ElementName::HTML_br)
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

    // FIXME: The isNull() check should allow us to deref *treeID() and *objectID() safely
    // in this function for a bit more performance.
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainThread()) {
        auto tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID()));
        return tree ? tree->objectForID(objectID()) : nullptr;
    }
#endif
    auto tree = std::get<WeakPtr<AXObjectCache>>(axTreeForID(treeID()));
    return tree ? tree->objectForID(*objectID()) : nullptr;
}

String AXTextMarker::debugDescription() const
{
    auto separator = ", "_s;
    RefPtr object = this->object();

    // Most text markers have the default affinity of downstream — avoid noisy output by only logging anything if
    // the value is the non-default one: upstream.
    String affinity = m_data.affinity == Affinity::Downstream ? ""_s : makeString(separator, "upstream"_s);
    String origin = m_data.origin == TextMarkerOrigin::Unknown ? ""_s : makeString(separator, originToString(m_data.origin));
    // If there is no object, we'll log it once here, then emptyString() for role which also requires an object.
    String id = object ? makeString("ID "_s, object->objectID().loggingString()) : String("no object"_s);

    return makeString("{"_s
        , id
        , object ? makeString(separator, "role "_s, roleToString(object->role())) : ""_s
        , isIgnored() ? makeString(separator, "ignored"_s) : ""_s
        // Anchor type and other fields below are not used for text markers processed off the main-thread.
        , isMainThread() ? makeString(separator, "anchor "_s, m_data.anchorType) : ""_s
        , affinity
        , separator, "offset "_s, m_data.offset
        , isMainThread() ? makeString(separator, "charStart "_s, m_data.characterStart) : ""_s
        , isMainThread() ? makeString(separator, "charOffset "_s, m_data.characterOffset) : ""_s
        , origin
        , "}"_s
    );
}

AXTextMarkerRange::AXTextMarkerRange(const VisibleSelection& selection)
    : m_start(selection.visibleStart())
    , m_end(selection.visibleEnd())
{
    ASSERT(isMainThread());
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

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::shouldCreateAXThreadCompatibleMarkers()) {
        auto visiblePositionRange = makeVisiblePositionRange(range);
        m_start = AXTextMarker { visiblePositionRange.start };
        m_end = AXTextMarker { visiblePositionRange.end };
        return;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    if (CheckedPtr cache = range->start.protectedDocument()->axObjectCache()) {
        m_start = AXTextMarker(cache->startOrEndCharacterOffsetForRange(*range, true));
        m_end = AXTextMarker(cache->startOrEndCharacterOffsetForRange(*range, false));
    }
}

AXTextMarkerRange::AXTextMarkerRange(const AXTextMarker& start, const AXTextMarker& end)
{
    auto order = start <=> end;
    if (order == std::partial_ordering::unordered) {
        m_start = { };
        m_end = { };
        return;
    }

    bool reverse = is_gt(order);
    m_start = reverse ? end : start;
    m_end = reverse ? start : end;
}

AXTextMarkerRange::AXTextMarkerRange(AXTextMarker&& start, AXTextMarker&& end)
{
    auto order = start <=> end;
    if (order == std::partial_ordering::unordered) {
        m_start = { };
        m_end = { };
        return;
    }

    bool reverse = is_gt(order);
    m_start = reverse ? WTFMove(end) : WTFMove(start);
    m_end = reverse ? WTFMove(start) : WTFMove(end);
}

AXTextMarkerRange::AXTextMarkerRange(std::optional<AXID> treeID, std::optional<AXID> objectID, unsigned start, unsigned end)
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
    if (m_start.m_data.objectID != m_end.m_data.objectID)
        return std::nullopt;

    if (m_start.m_data.treeID != m_end.m_data.treeID) [[unlikely]]
        return std::nullopt;

    if (m_start.m_data.characterOffset > m_end.m_data.characterOffset) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return { { m_start.m_data.characterOffset, m_end.m_data.characterOffset - m_start.m_data.characterOffset } };
}

std::optional<AXTextMarkerRange> AXTextMarkerRange::intersectionWith(const AXTextMarkerRange& other) const
{
    if (m_start.m_data.treeID != m_end.m_data.treeID
        || other.m_start.m_data.treeID != other.m_end.m_data.treeID
        || m_start.m_data.treeID != other.m_start.m_data.treeID) [[unlikely]]
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

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        if (!*this || !other)
            return { };

        bool thisRangeComesBeforeOther = true;
        auto canFindIntersectionPoint = [&] (const auto& firstRange, const auto& secondRange) -> bool {
            RefPtr current = firstRange.m_end.object();
            while (current) {
                if (current->objectID() == secondRange.m_end.objectID())
                    return true;

                if (current->objectID() == secondRange.m_start.objectID()) {
                    if (firstRange.m_end.objectID() == secondRange.m_start.objectID()) {
                        // If these are the same, we still have an intersection.
                        return true;
                    }
                    // Otherwise, we found the start of the other range after exiting out of the origin object,
                    // meaning the ranges don't intersect, e.g.:
                    // fo|o b|ar ^baz^
                    return false;
                }
                current = current->nextInPreOrder();
            }
            return false;
        };

        // Start by assuming |other.end| follows |this.end|, and try to find it.
        // Take this example, where "|" denotes the range of |this|, and "^" denotes |other|.
        // fo|o ba^r b|az^
        // Starting from the second |, we would find the ^ after "z". This tells us the intersection is between
        // the second | and the first ^.
        thisRangeComesBeforeOther = canFindIntersectionPoint(*this, other);

        if (!thisRangeComesBeforeOther) {
            // We couldn't find the other range when starting from |this.end|. The ranges may intersect the
            // opposite way so try to find |this.end| starting from |other.end|.
            if (!canFindIntersectionPoint(other, *this))
                return { };
        }

        AXTextMarker intersectionStart;
        auto intersectionEnd = thisRangeComesBeforeOther ? m_end : other.m_end;
        RefPtr current = intersectionEnd.object();
        // The ranges intersect. Now search backwards to find the intersection point.
        while (current) {
            auto axID = current->objectID();
            if (axID == m_start.objectID()) {
                intersectionStart = m_start;
                break;
            }
            if (axID == other.m_start.objectID()) {
                intersectionStart = other.m_start;
                break;
            }
            current = current->previousInPreOrder();
        }

        if (!current)
            return { };

        if (!downcast<AXIsolatedObject>(current)->textRuns())
            intersectionStart = { *current, /* offset */ 0 };
        return { { WTFMove(intersectionStart), WTFMove(intersectionEnd) } };
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<std::optional<AXTextMarkerRange>>([this, &other] () -> std::optional<AXTextMarkerRange> {
        auto intersection = WebCore::intersection(*this, other);
        if (intersection.isNull())
            return std::nullopt;

        return { AXTextMarkerRange(intersection) };
    });
}

String AXTextMarkerRange::debugDescription() const
{
    return makeString("text: '"_s, toString(), "'"_s,
        ", start: {"_s, m_start.debugDescription(), '}',
        ", end: {"_s, m_end.debugDescription(), '}');
}

std::partial_ordering operator<=>(const AXTextMarker& marker1, const AXTextMarker& marker2)
{
    if (!marker1.isValid() || !marker2.isValid())
        return std::partial_ordering::unordered;

    if (marker1.objectID() == marker2.objectID()) {
        if (marker1.treeID() == marker2.treeID()) [[likely]] {
            if (marker1.m_data.characterOffset < marker2.m_data.characterOffset) [[likely]]
                return std::partial_ordering::less;
            if (marker1.m_data.characterOffset > marker2.m_data.characterOffset)
                return std::partial_ordering::greater;
            return std::partial_ordering::equivalent;
        }
    }

    // If one of the objects is the root web area with an offset of 0, we know
    // that it is the first possible text marker, so we can fast-path the ordering.
    RefPtr object = marker1.object();
    if (object && !marker1.offset() && object->isRootWebArea())
        return std::partial_ordering::less;

    RefPtr otherObject = marker2.object();
    if (otherObject && !marker2.offset() && otherObject->isRootWebArea())
        return std::partial_ordering::greater;

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return object && otherObject ? object->partialOrder(*otherObject) : std::partial_ordering::unordered;
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

bool AXTextMarkerRange::isConfinedTo(std::optional<AXID> objectID) const
{
    return m_start.objectID() == objectID
        && m_end.objectID() == objectID
        && m_start.treeID() == m_end.treeID();
}

#if ENABLE(AX_THREAD_TEXT_APIS)
String listMarkerTextOnSameLine(const AXTextMarker& marker)
{
    RefPtr textMarkerObject = marker.object();
    if (!textMarkerObject)
        return { };

    if (marker.offset()) {
        // Don't return list marker text if this AXTextMarker isn't directly adjacent to the list marker.
        // We determine this by the offset — any non-zero offset text marker is not adjacent to the list marker.
        return { };
    }

    RefPtr listItemAncestor = Accessibility::findAncestor(*textMarkerObject, /* includeSelf */ true, [] (const auto& selfOrAncestor) {
        return selfOrAncestor.isListItem();
    });

    if (listItemAncestor) {
        if (RefPtr listMarker = findUnignoredDescendant(*listItemAncestor, /* includeSelf */ false, [] (const auto& descendant) {
            return descendant.role() == AccessibilityRole::ListMarker;
        })) {
            auto lineID = listMarker->listMarkerLineID();
            if (lineID && lineID == marker.lineID())
                return listMarker->listMarkerText();
        }
    }
    return { };
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

String AXTextMarkerRange::toString(IncludeListMarkerText includeListMarkerText, IncludeImageAltText includeImageAltText) const
{
#if !ENABLE(AX_THREAD_TEXT_APIS)
    UNUSED_PARAM(includeListMarkerText);
#endif // !ENABLE(AX_THREAD_TEXT_APIS)

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (!isMainThread() && AXObjectCache::useAXThreadTextApis()) {
        // Traverses from m_start to m_end, collecting all text along the way.
        auto start = m_start.toTextRunMarker();
        if (!start.isValid())
            return emptyString();
        auto end = m_end.toTextRunMarker();
        if (!end.isValid())
            return emptyString();

        StringBuilder result;
        if (includeListMarkerText == IncludeListMarkerText::Yes)
            result.append(listMarkerTextOnSameLine(start));

        if (start.isolatedObject() == end.isolatedObject()) {
            size_t minOffset = std::min(start.offset(), end.offset());
            size_t maxOffset = std::max(start.offset(), end.offset());
            result.append(start.runs()->substring(minOffset, maxOffset - minOffset));
            return result.toString();
        }

        auto emitAuxiliaryText = [&] (AXIsolatedObject& object) {
            if (includeImageAltText == IncludeImageAltText::Yes && object.isImage()) {
                // This is an image, so add alt text (if it has any).
                result.append(object.description());
            }

            // FIXME: This function should not just be emitting newlines, but instead handling every character type in TextEmissionBehavior.
            auto behavior = object.textEmissionBehavior();
            if (behavior != TextEmissionBehavior::Newline && behavior != TextEmissionBehavior::DoubleNewline)
                return;

            // Like TextIterator, don't emit a newline if the most recently emitted character was already a newline.
            if (!result.length() || result[result.length() - 1] != '\n') {
                result.append('\n');
                if (behavior == TextEmissionBehavior::DoubleNewline)
                    result.append('\n');
            }
        };

        result.append(start.runs()->substring(start.offset()));

        // FIXME: If we've been given reversed markers, i.e. the end marker actually comes before the start marker,
        // we may want to detect this and try searching AXDirection::Previous?
        RefPtr current = findObjectWithRuns(*start.isolatedObject(), AXDirection::Next, std::nullopt, emitAuxiliaryText);
        while (current && current->objectID() != end.objectID()) {
            result.append(current->textRuns()->toStringView());
            current = findObjectWithRuns(*current, AXDirection::Next, std::nullopt, emitAuxiliaryText);
        }
        result.append(end.runs()->substring(0, end.offset()));
        return result.toString();
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<String>([this, includeImageAltText] () -> String {

        auto range = simpleRange();
        if (!range)
            return { };

        OptionSet<TextIteratorBehavior> behaviors = { TextIteratorBehavior::IgnoresFullSizeKana };
        if (includeImageAltText == IncludeImageAltText::Yes)
            behaviors.add(TextIteratorBehavior::EmitsImageAltText);

        TextIterator it = TextIterator(*range, behaviors);
        if (it.atEnd())
            return { };

        StringBuilder builder;
        for (; !it.atEnd(); it.advance()) {
            RefPtr node = it.node();
            // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
            if (it.text().length()) {
                // If this is in a list item, we need to add the text for the list marker
                // because a RenderListMarker does not have a Node equivalent and thus does not appear
                // when iterating text.
                // Don't add list marker text for new line character.
                if (it.text().length() != 1 || !isASCIIWhitespace(it.text()[0]))
                    builder.append(AccessibilityObject::listMarkerTextForNodeAndPosition(node.get(), makeDeprecatedLegacyPosition(it.range().start)));
                it.appendTextToStringBuilder(builder);
            } else {
                if (AccessibilityObject::replacedNodeNeedsCharacter(*node))
                    builder.append(objectReplacementCharacter);
            }
        }

        return builder.toString().isolatedCopy();
    });
}

#if ENABLE(AX_THREAD_TEXT_APIS)
AXTextMarker AXTextMarker::convertToDomOffset() const
{
    ASSERT(!isMainThread());

    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().convertToDomOffset();

    auto newData = m_data;
    newData.offset = runs()->domOffset(offset());
    newData.characterOffset = m_data.offset;
    newData.characterStart = 0;
    newData.affinity = Affinity::Downstream;

    return { newData };
}

void AXTextMarker::clampOffsetToLengthIfNeeded(unsigned length) const
{
    if (offset() <= length)
        return;

    const_cast<AXTextMarker*>(this)->m_data.offset = length;
}

AXTextRunLineID AXTextMarker::lineID() const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().lineID();

    const auto* runs = this->runs();
    size_t runIndex = runs->indexForOffset(offset(), affinity());
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
        startMarker = { *object, 0 };
    else if (RefPtr editableAncestor = object->editableAncestor())
        startMarker = { editableAncestor->treeID(), editableAncestor->objectID(), 0 };
    else if (RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID())))
        startMarker = tree->firstMarker();
    else
        return -1;
    // Do this conversion early so we only do it once, rather than in every function that requires
    // |startMarker| to be a text-run marker.
    startMarker = startMarker.toTextRunMarker();

    auto currentLineID = startMarker.lineID();
    auto targetLineID = lineID();
    if (currentLineID == targetLineID)
        return 0;

    auto currentMarker = WTFMove(startMarker);
    if (!currentMarker.atLineEnd()) {
        // Start from a line end, so that subsequent calls to nextLineEnd() yield a new line.
        // Otherwise if we started from the middle of a line, we would count the the first line twice.
        auto nextLineEndMarker = currentMarker.nextLineEnd();
        TEXT_MARKER_ASSERT_DOUBLE(nextLineEndMarker.lineID() == currentMarker.lineID(), nextLineEndMarker, currentMarker);
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
    TEXT_MARKER_ASSERT(!offset());

    std::optional stopAtID = object->idOfNextSiblingIncludingIgnoredOrParent();
    auto textRunMarker = toTextRunMarker(stopAtID);
    // If we couldn't convert this object to a text-run marker, it means we are a text control with no text descendant.
    if (!textRunMarker.isValid())
        return { };

    unsigned precedingLength = 0;
    // Use IncludeTrailingLineBreak::Yes to match AccessibilityRenderObject::doAXRangeForLine, which behaves this way (specifically):
    //   if (isHardLineBreak(lineEnd))
    //     ++lineEndIndex;
    // This behavior is a little questionable, since our implementation of length-for-text-marker-range does not behave this way,
    // meaning we will compute a different length between these two APIs for the same logical range.
    auto currentLineRange = textRunMarker.lineRange(LineRangeType::Current, IncludeTrailingLineBreak::Yes);
    while (lineIndex && currentLineRange) {
        precedingLength += currentLineRange.toString().length();
        auto lineEndMarker = currentLineRange.end().nextLineEnd(IncludeTrailingLineBreak::Yes, stopAtID);
        currentLineRange = { lineEndMarker.previousLineStart(stopAtID), WTFMove(lineEndMarker) };
        --lineIndex;
    }
    return currentLineRange ? CharacterRange(precedingLength, currentLineRange.toString().length()) : CharacterRange();
}

AXTextMarkerRange AXTextMarker::markerRangeForLineIndex(unsigned lineIndex) const
{
    // This implementation doesn't respect the offset as the only known callsite hardcodes zero. We'll need to make changes to support this if a usecase arrives for it.
    TEXT_MARKER_ASSERT(!offset());

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

    if (object->isTextControl() && index >= object->textMarkerRange().toString().length() - 1) {
        // Mimic behavior of AccessibilityRenderObject::visiblePositionForIndex.
        return -1;
    }

    std::optional stopAtID = object->idOfNextSiblingIncludingIgnoredOrParent();
    unsigned lineIndex = 0;
    auto currentMarker = *this;
    while (index) {
        auto oldMarker = WTFMove(currentMarker);
        currentMarker = oldMarker.findMarker(AXDirection::Next, CoalesceObjectBreaks::Yes, IgnoreBRs::Yes, stopAtID);
        if (!currentMarker.isValid())
            break;

        if (oldMarker.lineID() != currentMarker.lineID())
            ++lineIndex;

        --index;
    }
    // Only return the line number if the index was a valid offset into our descendants.
    return !index ? lineIndex : -1;
}

bool AXTextMarker::atLineBoundaryForDirection(AXDirection direction) const
{
    if (!isValid())
        return false;
    if (!isInTextRun())
        return toTextRunMarker().atLineBoundaryForDirection(direction);

    size_t runIndex = runs()->indexForOffset(offset(), affinity());
    TEXT_MARKER_ASSERT(runIndex != notFound);
    if (runIndex == notFound)
        return false;

    RefPtr currentObject = isolatedObject();
    const auto* currentRuns = currentObject->textRuns();
    return atLineBoundaryForDirection(direction, currentRuns, runIndex);
}

bool AXTextMarker::atLineBoundaryForDirection(AXDirection direction, const AXTextRuns* runs, size_t runIndex) const
{
    RefPtr nextObjectWithRuns = findObjectWithRuns(*isolatedObject(), direction);
    // If the next object is a line break, it will often have the same line index as the previous static text
    // (even though it is a newline). In this case, advance one object to check the next line index.
    if (nextObjectWithRuns && nextObjectWithRuns->isLineBreak())
        nextObjectWithRuns = findObjectWithRuns(*nextObjectWithRuns, direction);

    auto* nextRuns = nextObjectWithRuns ? nextObjectWithRuns->textRuns() : nullptr;
    // If there are more runs in the same containing block with the same line, we are not at a start or end and can exit early.
    // No need to continue searching when the containing block changes.
    while (nextRuns && runs->containingBlock == nextRuns->containingBlock) {
        // If our lineID exists beyond our current object, we can safely say we aren't at a line boundary.
        if (runs->lineID(runIndex) == nextRuns->lineID(direction == AXDirection::Next ? 0 : nextRuns->size() - 1))
            return false;
        nextObjectWithRuns = findObjectWithRuns(*nextObjectWithRuns, direction);
        nextRuns = nextObjectWithRuns ? nextObjectWithRuns->textRuns() : nullptr;
    }

    // The current line/containing block ends with the current object and runs. Now, check if we are at
    // the start/end of the line using the marker's position within its line.
    unsigned sumToRunIndex = runIndex ? runs->runLengthSumTo(runIndex - 1) : 0;
    TEXT_MARKER_ASSERT(offset() >= sumToRunIndex);
    if (offset() < sumToRunIndex)
        return false;

    unsigned offsetInLine = offset() - sumToRunIndex;
    return direction == AXDirection::Previous ? !offsetInLine : runs->runLength(runIndex) == offsetInLine;
}

unsigned AXTextMarker::offsetFromRoot() const
{
    ASSERT(!isMainThread());

    if (!isValid())
        return 0;
    RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID()));
    if (RefPtr root = tree ? tree->rootNode() : nullptr) {
        AXTextMarker rootMarker { root->treeID(), root->objectID(), 0 };
        unsigned offset = 0;
        auto current = rootMarker;

        bool needsNewlineOffset = false;
        auto applyNewlineOffset = [&] () {
            if (needsNewlineOffset && offset) {
                // Only represent a newline if we have started counting
                // actual, non-whitespace text (i.e. offset is > 0).
                offset++;
            }
            needsNewlineOffset = false;
        };

        while (current.isValid() && !hasSameObjectAndOffset(current)) {
            applyNewlineOffset();
            auto previous = current;
            // If an object has text runs, and we are not at the very last position in those runs, use findMarker to navigate within them.
            // Otherwise, we want to explore all objects.
            RefPtr currentObject = current.isolatedObject();
            const auto* runs = currentObject->textRuns();
            if (runs && current.offset() < runs->totalLength()) {
                current = previous.findMarker(AXDirection::Next, CoalesceObjectBreaks::No, IgnoreBRs::No);
                // While searching, we want to explore all positions (hence, we don't coalesce newlines or skip line breaks above)
                // But, don't increment if the previous and current have the same visual position.
                if (!previous.equivalentTextPosition(current))
                    offset++;
            } else {
                if (currentObject->emitsNewline()) {
                    // If the next text we come across is on a new line, we need to increment the offset, since the
                    // previous + current text marker won't share an equivalent visual text position.
                    needsNewlineOffset = true;
                }
                RefPtr nextObject = currentObject->nextInPreOrder();
                current = nextObject ? AXTextMarker { *nextObject, 0 } : AXTextMarker();
            }
        }
        applyNewlineOffset();

        // If this assert fails, it means we couldn't navigate from root to `this`, which should never happen.
        TEXT_MARKER_ASSERT_DOUBLE(hasSameObjectAndOffset(current), (*this), current);
        return offset;
    }
    return 0;
}

AXTextMarker AXTextMarker::nextMarkerFromOffset(unsigned offset, ForceSingleOffsetMovement forceSingleOffsetMovement, std::optional<AXID> stopAtID) const
{
    ASSERT(!isMainThread());

    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker(stopAtID).nextMarkerFromOffset(offset, forceSingleOffsetMovement, stopAtID);

    auto marker = *this;
    while (offset) {
        if (auto newMarker = marker.findMarker(AXDirection::Next, CoalesceObjectBreaks::No, IgnoreBRs::No, stopAtID, forceSingleOffsetMovement))
            marker = WTFMove(newMarker);
        else
            break;

        --offset;
    }
    return marker;
}

AXTextMarker AXTextMarker::findLastBefore(std::optional<AXID> stopAtID) const
{
    ASSERT(!isMainThread());

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

    RefPtr lastObjectWithRuns = isolatedObject();
    // FIXME: Do we need to compare both tree ID and object ID here?
    while (!stopAtID || *stopAtID != lastObjectWithRuns->objectID()) {
        RefPtr newObject = findObjectWithRuns(*lastObjectWithRuns, AXDirection::Next, stopAtID);
        if (!newObject)
            break;
        lastObjectWithRuns = WTFMove(newObject);
    }

    return AXTextMarker { *lastObjectWithRuns, lastObjectWithRuns->textRuns()->totalLength() };
}

AXTextMarkerRange AXTextMarker::rangeWithSameStyle() const
{
    ASSERT(!isMainThread());

    if (!isValid())
        return { };

    auto originalStyle = object()->stylesForAttributedString();
    auto findMarkerWithDifferentStyle = [&] (AXDirection direction) -> AXTextMarker {
        RefPtr current = isolatedObject();
        while (current) {
            RefPtr next = findObjectWithRuns(*current, direction);
            if (next && originalStyle != next->stylesForAttributedString())
                break;
            current = WTFMove(next);
        }

        if (current)
            return AXTextMarker { *current, direction == AXDirection::Next ? current->textRuns()->totalLength() : 0 };
        if (RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(object()->treeID()))) {
            // The style is unchanged from `this` to the start or end of tree. Return the start-or-end-of-tree position.
            return direction == AXDirection::Next ? tree->lastMarker() : tree->firstMarker();
        }
        return { };
    };

    return { findMarkerWithDifferentStyle(AXDirection::Previous), findMarkerWithDifferentStyle(AXDirection::Next) };
}

static FloatRect viewportRelativeFrameFromRuns(Ref<AXIsolatedObject> object, unsigned start, unsigned end)
{
    const auto* runs = object->textRuns();
    auto relativeFrame = object->relativeFrame();
    if (!start && end == runs->totalLength()) {
        // If the caller wants the entirety of this object's text, we don't need to to do any estimating,
        // and can just return the relative frame.
        return relativeFrame;
    }

    auto runsLocalRect = runs->localRect(start, end, object->fontOrientation());
    // The rect we got above is a "local" rect, relative to nothing else. Move it to be
    // anchored at this object's relative frame.
    runsLocalRect.move(relativeFrame.x(), relativeFrame.y());
    return runsLocalRect;
}

static FloatRect viewportRelativeFrameFromRuns(Ref<AXIsolatedObject> object, unsigned offset)
{
    const auto* runs = object->textRuns();
    // Get the bounds starting from |offset| to the end of the runs.
    return viewportRelativeFrameFromRuns(object, offset, runs->totalLength());
}

FloatRect AXTextMarkerRange::viewportRelativeFrame() const
{
    ASSERT(!isMainThread());

    auto start = m_start.toTextRunMarker();
    if (!start.isValid())
        return { };
    auto end = m_end.toTextRunMarker();
    if (!end.isValid())
        return { };

    if (*start.objectID() == *end.objectID()) {
        // The range is self-contained.
        return viewportRelativeFrameFromRuns(*start.isolatedObject(), start.offset(), end.offset());
    }

    // The range spans multiple objects, so we'll need to traverse objects with text runs
    // from start to end and accumulate the final bounds.
    FloatRect result = viewportRelativeFrameFromRuns(*start.isolatedObject(), start.offset());

    RefPtr current = start.isolatedObject();
    while (current && current->objectID() != *end.objectID()) {
        result.unite(viewportRelativeFrameFromRuns(*current, /* offset */ 0));
        current = findObjectWithRuns(*current, AXDirection::Next, /* stopAtID */ *end.objectID());
    }
    result.unite(viewportRelativeFrameFromRuns(*end.isolatedObject(), /* start */ 0, /* end */ end.offset()));

    return result;
}

AXTextMarkerRange AXTextMarkerRange::convertToDomOffsetRange() const
{
    ASSERT(!isMainThread());

    return {
        m_start.convertToDomOffset(),
        m_end.convertToDomOffset()
    };
}

const AXTextRuns* AXTextMarker::runs() const
{
    ASSERT(!isMainThread());

    RefPtr object = isolatedObject();
    return object ? object->textRuns() : nullptr;
}

// Custom text unit iterator wrappers

static int previousSentenceStartFromOffset(StringView text, unsigned offset)
{
    return ubrk_preceding(sentenceBreakIterator(text), offset);
}

static int nextSentenceEndFromOffset(StringView text, unsigned offset)
{
    int endIndex = ubrk_following(sentenceBreakIterator(text), offset);

    if (!text.substring(offset, endIndex).containsOnly<isASCIIWhitespace>()) {
        // To match AXObjectCache::nextBoundary, don't include a newline character at the end of sentences.
        while (endIndex > 0 && text.length() && text.substring(0, endIndex).endsWith('\n'))
            --endIndex;
    } else {
        // If we are looking at a range that is *only* newline characters, the end should be the next sentence boundary.
        while (endIndex < Checked<int>(text.length()) - 1 && text.length() && text.substring(0, endIndex + 1).endsWith('\n'))
            ++endIndex;
    }
    return endIndex;
}

AXTextMarker AXTextMarker::findMarker(AXDirection direction, CoalesceObjectBreaks coalesceObjectBreaks, IgnoreBRs ignoreBRs, std::optional<AXID> stopAtID, ForceSingleOffsetMovement forceSingleOffsetMovement) const
{
    // This method has two boolean options:
    // - coalesceObjectBreaks: Mimics behavior from textMarkerDataForNextCharacterOffset, where we skip nodes
    //   that have the same visual position (i.e., there is 0 length between them). When false, we traverse all
    //   possible text markers (which is important for searching)
    // - ignoreBRs: In most cases, we want to skip <br> tags when not in an editable context. This is not true,
    //   for example, when computing text marker indexes.

    RefPtr object = isolatedObject();
    if (!object) {
        // Equivalent to checking AXTextMarker::isValid, but "inlined" because this function is super hot.
        return { };
    }
    const auto* runs = object->textRuns();
    if (!runs || !runs->size()) {
        // Equivalent to checking AXTextMarker::isInTextRun, but "inlined" because this function is super hot.
        return toTextRunMarker().findMarker(direction, coalesceObjectBreaks, ignoreBRs, stopAtID);
    }

    // If the BR isn't in an editable ancestor, we shouldn't be including it (in most cases of findMarker).
    bool shouldSkipBR = ignoreBRs == IgnoreBRs::Yes && object && object->role() == AccessibilityRole::LineBreak && !object->editableAncestor();
    bool isWithinRunBounds = ((direction == AXDirection::Next && offset() < runs->totalLength()) || (direction == AXDirection::Previous && offset()));
    if (!shouldSkipBR && isWithinRunBounds) {
        if (runs->containsOnlyASCII || forceSingleOffsetMovement == ForceSingleOffsetMovement::Yes) {
            // In the common case where the text-runs only contain ASCII, all we need to do is the move the offset by 1,
            // which is more efficient than turning the runs into a string and creating a CachedTextBreakIterator.
            return AXTextMarker { treeID(), objectID(), direction == AXDirection::Next ? offset() + 1 : offset() - 1 };
        }

        CachedTextBreakIterator iterator(runs->toStringView(), { }, TextBreakIterator::CaretMode { }, nullAtom());
        unsigned newOffset = direction == AXDirection::Next ? iterator.following(offset()).value_or(offset() + 1) : iterator.preceding(offset()).value_or(offset() - 1);
        return AXTextMarker { treeID(), objectID(), newOffset };
    }

    // offset() pointed to the last character in the given object's runs, so let's traverse to find the next object with runs.
    object = findObjectWithRuns(*object, direction, stopAtID);
    if (object) {
        bool nextRunHasLength = direction == AXDirection::Next ? object->textRuns()->runLength(0) : object->textRuns()->lastRunLength();
        TEXT_MARKER_ASSERT(nextRunHasLength);
        if (!nextRunHasLength)
            return { };

        // The startingOffset is used to advance one position farther when we are coalescing object breaks and skipping positions.
        unsigned startingOffset = 0;
        if (coalesceObjectBreaks == CoalesceObjectBreaks::Yes || shouldSkipBR)
            startingOffset = 1;

        return AXTextMarker { *object, direction == AXDirection::Next ? startingOffset : object->textRuns()->lastRunLength() - startingOffset };
    }
    return { };
}

AXTextMarker AXTextMarker::findLine(AXDirection direction, AXTextUnitBoundary boundary, IncludeTrailingLineBreak includeTrailingLineBreak, std::optional<AXID> stopAtID) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker(stopAtID).findLine(direction, boundary, includeTrailingLineBreak, stopAtID);

    size_t runIndex = runs()->indexForOffset(offset(), affinity());
    TEXT_MARKER_ASSERT(runIndex != notFound);
    if (runIndex == notFound)
        return { };

    RefPtr currentObject = isolatedObject();
    const auto* currentRuns = currentObject->textRuns();
    auto origin = boundary == AXTextUnitBoundary::Start && direction == AXDirection::Previous ? TextMarkerOrigin::PreviousLineStart : TextMarkerOrigin::NextLineEnd;

    // If, for example, we are asked to find the next line end, and are at the very end of a line already,
    // we need the end position of the next line instead. Determine this by checking the next or previous marker.
    if (atLineBoundaryForDirection(direction, currentRuns, runIndex)) {
        auto adjacentMarker = findMarker(direction, CoalesceObjectBreaks::No, IgnoreBRs::Yes, stopAtID);
        bool findingNextLineEnd = direction == AXDirection::Next && boundary == AXTextUnitBoundary::End;
        bool findOnNextLine = findingNextLineEnd || (direction == AXDirection::Previous && boundary == AXTextUnitBoundary::Start);

        if (findingNextLineEnd && currentRuns == adjacentMarker.runs()) {
            // Imagine wanting to find the next-line-end in this markup (taken from
            // editable-single-letter-soft-linebreak-lines.html), where | represents the current position:
            //   A| (offset 1, upstream)
            //   B
            //   C
            // `findMarker` currently doesn't set any affinity, leaving it as the default value of downstream.
            // Thus, adjacentMarker will be (offset 2, downstream), which actually skips the line "B" is on:
            //   A
            //   B
            //  |C
            // We need to detect this to avoid skipping a line.
            size_t adjacentRunIndex = currentRuns->indexForOffset(adjacentMarker.offset(), adjacentMarker.affinity());
            if (adjacentRunIndex != notFound && adjacentRunIndex > runIndex && adjacentRunIndex - runIndex > 1) {
                // The scenario we're trying to detect should only have resulted in one run / line being skipped.
                // Our affinity flip won't result in the correct behavior if we've somehow jumped >2 lines.
                ASSERT(adjacentRunIndex - runIndex == 2);
                // This scenario really should only happen with single "entity" runs (where an entity could be an ASCII
                // character, or a multi-byte emoji that occupies multiple indices but is one atomic entity).
                AX_BROKEN_ASSERT(!currentRuns->containsOnlyASCII || (currentRuns->runLength(runIndex) == 1 && currentRuns->runLength(adjacentRunIndex) == 1));
                // The next line end is simply the adjacent marker with an upstream affinity (with an ASSERT to verify this).
                ASSERT(currentRuns->indexForOffset(adjacentMarker.offset(), Affinity::Upstream) == runIndex + 1);
                adjacentMarker.setAffinity(Affinity::Upstream);
                return adjacentMarker;
            }
        }

        if (findOnNextLine)
            return adjacentMarker.findLine(direction, boundary, includeTrailingLineBreak, stopAtID);
    }

    auto computeOffset = [&] (size_t runEndOffset, size_t runLength) {
        // This works because `runEndOffset` is the offset pointing to the end of the given run, which includes the length of all runs preceding it. So subtracting that from the length of the current run gives us an offset to the start of the current run.
        return boundary == AXTextUnitBoundary::End ? runEndOffset : runEndOffset - runLength;
    };
    auto linePosition = AXTextMarker(treeID(), objectID(), computeOffset(currentRuns->runLengthSumTo(runIndex), currentRuns->runLength(runIndex)), origin);
    auto startLineID = currentRuns->lineID(runIndex);
    // We found the start run and associated line, now iterate until we find a line boundary.
    while (currentObject) {
        TEXT_MARKER_ASSERT_SINGLE(currentRuns->size(), (*this));
        if (!currentRuns->size())
            return { };

        unsigned cumulativeOffset = runIndex ? currentRuns->runLengthSumTo(runIndex - 1) : 0;
        // We should search in the right direction for a change in the line index.
        for (size_t i = runIndex; direction == AXDirection::Next ? i < currentRuns->size() : i >= 0; direction == AXDirection::Next ? i++ : i--) {
            cumulativeOffset += currentRuns->runLength(i);
            if (currentRuns->lineID(i) != startLineID) {
                if (boundary == AXTextUnitBoundary::End) {
                    // We are returning a line-end position, which is upstream in the case of soft linebreaks, e.g.:
                    // foo|
                    // bar
                    // rather than:
                    // foo
                    // |bar
                    linePosition.setAffinity(Affinity::Upstream);
                }
                return linePosition;
            }
            linePosition = AXTextMarker(*currentObject, computeOffset(cumulativeOffset, currentRuns->runLength(i)), origin);

            if (direction == AXDirection::Previous && !i) {
                // We want to execute the loop body when i == 0, but break now to avoid underflow.
                break;
            }
        }
        currentObject = findObjectWithRuns(*currentObject, direction, stopAtID);
        if (currentObject) {
            if (includeTrailingLineBreak == IncludeTrailingLineBreak::No && currentObject->role() == AccessibilityRole::LineBreak)
                break;
            currentRuns = currentObject->textRuns();
            // Reset the runIndex to 0 or the maximum, since we should start iterating from the very beginning/end of the next object's runs, depending on the direction.
            runIndex = direction == AXDirection::Next ? 0 : currentRuns->size() - 1;
        }
    }
    return linePosition;
}

AXTextMarker AXTextMarker::findParagraph(AXDirection direction, AXTextUnitBoundary boundary) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().findParagraph(direction, boundary);

    size_t runIndex = runs()->indexForOffset(offset(), affinity());
    TEXT_MARKER_ASSERT(runIndex != notFound);
    if (runIndex == notFound)
        return { };

    RefPtr currentObject = isolatedObject();
    const auto* currentRuns = currentObject->textRuns();
    auto origin = direction == AXDirection::Previous && boundary == AXTextUnitBoundary::Start ? TextMarkerOrigin::PreviousParagraphStart : TextMarkerOrigin::NextParagraphEnd;

    // Paragraphs must be handled differently from word + sentence boundaries, as there is no paragraph break iterator.
    // Rather, paragraph boundaries are based on rendered newlines and differences in node editability and block-grouping (through containing blocks).
    unsigned sumToRunIndex = runIndex ? currentRuns->runLengthSumTo(runIndex - 1) : 0;
    unsigned offsetInStartLine = offset() - sumToRunIndex;

    while (currentObject) {
        TEXT_MARKER_ASSERT_SINGLE(currentRuns->size(), (*this));
        if (!currentRuns->size())
            return { };

        for (size_t i = runIndex; i < currentRuns->size() && i >= 0; direction == AXDirection::Next ? i++ : i--) {
            // If a text run starts or ends with a newline character, that indicates a paragraph boundary. However, if the direction
            // is Next, and our starting offset points to the end of the line (past the newline character), we are past the boundary.
            if (currentRuns->runEndsWithLineBreak(i) && (i != runIndex || (direction == AXDirection::Next && currentRuns->runLength(i) != offsetInStartLine))) {
                unsigned sumIncludingCurrentLine = currentRuns->runLengthSumTo(i);
                unsigned newlineOffsetConsideringDirection = direction == AXDirection::Next ? sumIncludingCurrentLine - 1 : sumIncludingCurrentLine;
                return { *currentObject, newlineOffsetConsideringDirection, origin };
            }

            if (currentRuns->runStartsWithLineBreak(i) && (i != runIndex || (direction == AXDirection::Previous && offsetInStartLine))) {
                unsigned sumUpToCurrentLine = i ? currentRuns->runLengthSumTo(i - 1) : 0;
                unsigned newlineOffsetConsideringDirection = direction == AXDirection::Next ? 0 : 1;
                return { *currentObject, sumUpToCurrentLine + newlineOffsetConsideringDirection, origin };
            }
        }

        RefPtr previousObject = currentObject;
        const auto* previousRuns = previousObject->textRuns();
        currentObject = findObjectWithRuns(*currentObject, direction);
        currentRuns = currentObject ? currentObject->textRuns() : nullptr;

        // Paragraph boundaries also change based on editability, containing block, and whether we hit a line break.
        bool isContainingBlockBoundary = currentRuns && previousRuns && currentRuns->containingBlock != previousRuns->containingBlock;
        // Don't bother computing isEditBoundary if isContainingBlockBoundary since we only need one or the other below.
        bool isEditBoundary = !isContainingBlockBoundary && previousObject && currentObject && !!previousObject->editableAncestor() != !!currentObject->editableAncestor();
        if (!currentObject || !currentRuns || currentObject->role() == AccessibilityRole::LineBreak || isContainingBlockBoundary || isEditBoundary)
            return { *previousObject, direction == AXDirection::Next ? previousRuns->totalLength() : 0, origin };
    }
    return { };
}

AXTextMarker AXTextMarker::findWordOrSentence(AXDirection direction, bool findWord, AXTextUnitBoundary boundary) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().findWordOrSentence(direction, findWord, boundary);

    auto origin = TextMarkerOrigin::Unknown;
    if (findWord) {
        if (direction == AXDirection::Previous)
            origin = boundary == AXTextUnitBoundary::Start ? TextMarkerOrigin::PreviousWordStart : TextMarkerOrigin::PreviousWordEnd;
        else
            origin = boundary == AXTextUnitBoundary::Start ? TextMarkerOrigin::NextWordStart : TextMarkerOrigin::NextWordEnd;
    } else
        origin = direction == AXDirection::Previous && boundary == AXTextUnitBoundary::Start ? TextMarkerOrigin::PreviousSentenceStart : TextMarkerOrigin::NextSentenceEnd;

    RefPtr currentObject = isolatedObject();
    const auto* currentRuns = currentObject->textRuns();

    clampOffsetToLengthIfNeeded(currentRuns->totalLength());

    unsigned offset = this->offset();
    AXTextMarker resultMarker = *this;

    String flattenedRuns = currentRuns->toString();

    // objectBorder maintains the position in flattenedRuns between the current object's text and the previously scanned object(s)
    int objectBorder = direction == AXDirection::Next ? 0 : flattenedRuns.length();

    // Functions to update resultMarker for word and sentence text units.
    auto updateWordResultMarker = [&] () {
        if (direction == AXDirection::Previous && boundary == AXTextUnitBoundary::Start) {
            TEXT_MARKER_ASSERT_SINGLE(offset <= flattenedRuns.length(), (*this));
            int previousWordStart = findNextWordFromIndex(flattenedRuns, offset, false);
            if (previousWordStart <= objectBorder)
                resultMarker = AXTextMarker(*currentObject, previousWordStart, origin);
        } else if (direction == AXDirection::Next && boundary == AXTextUnitBoundary::End) {
            int nextWordEnd = 0;
            findEndWordBoundary(flattenedRuns, offset, &nextWordEnd);
            // If the next word end is at or beyond the object border, that means the word extends into the current object (and we should update the text marker).
            // Otherwise, the nextWordEnd is in the previous object and the text marker was already set in the previous loop.
            if (nextWordEnd >= objectBorder) {
                // We need to subtract the objectBorder from the word end since we need the offset relative to the
                // **current** object, and the nextWordEnd is relative to the flattenedRuns.
                resultMarker = AXTextMarker(*currentObject, nextWordEnd - objectBorder, origin);
                // Sometimes, the end word boundary will just return a whitespace word. For example: "Hello| world", with the text marker after hello, will return a text marker before world ("Hello |world").
                // If we detect this case, we want to continue searching for the next next-word-end.
                auto rangeString = AXTextMarkerRange(*this, resultMarker).toString();
                if (rangeString.containsOnly<isASCIIWhitespace>()) {
                    findEndWordBoundary(flattenedRuns, offset + rangeString.length(), &nextWordEnd);
                    if (nextWordEnd >= objectBorder)
                        resultMarker = AXTextMarker(*currentObject, nextWordEnd - objectBorder, origin);
                }
            }
        }
    };

    auto updateSentenceResultMarker = [&] () {
        if (boundary == AXTextUnitBoundary::Start) {
            int start = previousSentenceStartFromOffset(flattenedRuns, offset);
            if (direction == AXDirection::Previous && start < objectBorder && start != -1)
                resultMarker = AXTextMarker(*currentObject, start, origin);
            else if (direction == AXDirection::Next && start != -1 && start >= objectBorder)
                resultMarker = AXTextMarker(*currentObject, start - objectBorder, origin);
        } else {
            int end = nextSentenceEndFromOffset(flattenedRuns, offset);
            // If the current marker (this) is the same position from the end, start a new search from there.
            if (direction == AXDirection::Previous && end <= objectBorder && end != -1)
                resultMarker = AXTextMarker(*currentObject, end, origin);
            else if (direction == AXDirection::Next && end != -1 && end >= objectBorder && Checked<int>(offset) != end) {
                // Don't include the newline if it is returned at the end of the sentence.
                resultMarker = AXTextMarker(*currentObject, end - objectBorder, origin);
            }
        }
    };

    while (currentObject) {
        if (findWord)
            updateWordResultMarker();
        else
            updateSentenceResultMarker();

        bool lastObjectIsEditable = !!currentObject->editableAncestor();
        currentObject = findObjectWithRuns(*currentObject, direction);
        if (currentObject) {
            // We should return when the containing block is different (indicating a paragraph).
            if (currentRuns->containingBlock != currentObject->textRuns()->containingBlock)
                return resultMarker;

            // We only stop at line breaks when finding words, as for sentences, the text break iterator needs to find the next sentence boundary, which isn't necessarily at a break.
            bool shouldStopAtLineBreaks = findWord && currentObject->role() == AccessibilityRole::LineBreak && !currentObject->editableAncestor();

            // Also stop when we hit the border of an editable object.
            if (shouldStopAtLineBreaks || lastObjectIsEditable != !!currentObject->editableAncestor())
                return resultMarker;

            currentRuns = currentObject->textRuns();
            StringView newRunsFlattenedString = currentRuns->toStringView();
            if (direction == AXDirection::Previous) {
                flattenedRuns = makeString(newRunsFlattenedString, flattenedRuns);
                offset += newRunsFlattenedString.length();
                objectBorder = newRunsFlattenedString.length();
            } else {
                // We don't need to update the offset when moving fowards, since text is being appended to the end of flattenedRuns
                objectBorder = flattenedRuns.length();
                flattenedRuns = makeString(flattenedRuns, newRunsFlattenedString);
            }
        }
    }
    return resultMarker;
}

AXTextMarker AXTextMarker::previousParagraphStart() const
{
    // Mimic previousParagraphStartCharacterOffset and move off the current text marker.
    auto adjacentMarker = findMarker(AXDirection::Previous, CoalesceObjectBreaks::Yes, IgnoreBRs::No);
    // Like previousParagraphStartCharacterOffset, advance one if the object is a line break.
    RefPtr currentObject = isolatedObject();
    if (RefPtr adjacentObject = adjacentMarker.isolatedObject(); currentObject && adjacentObject) {
        if (currentObject->role() != AccessibilityRole::LineBreak && adjacentObject->role() == AccessibilityRole::LineBreak)
            adjacentMarker = adjacentMarker.findMarker(AXDirection::Previous, CoalesceObjectBreaks::No, IgnoreBRs::No);
    }

    return adjacentMarker.findParagraph(AXDirection::Previous, AXTextUnitBoundary::Start);
}

AXTextMarker AXTextMarker::nextParagraphEnd() const
{
    // Mimic nextParagraphEndCharacterOffset and move off the current text marker.
    auto adjacentMarker = findMarker(AXDirection::Next, CoalesceObjectBreaks::Yes, IgnoreBRs::No);
    // Like nextParagraphEndCharacterOffset, advance one if the object is a line break.
    RefPtr currentObject = isolatedObject();
    if (RefPtr adjacentObject = adjacentMarker.isolatedObject(); currentObject && adjacentObject) {
        if (currentObject->role() != AccessibilityRole::LineBreak && adjacentObject->role() == AccessibilityRole::LineBreak)
            adjacentMarker = adjacentMarker.findMarker(AXDirection::Next, CoalesceObjectBreaks::No, IgnoreBRs::No);
    }

    return adjacentMarker.findParagraph(AXDirection::Next, AXTextUnitBoundary::End);
}


AXTextMarker AXTextMarker::toTextRunMarker(std::optional<AXID> stopAtID) const
{
    RefPtr object = isolatedObject();
    if (!object) {
        // Equivalent to AXTextMarker::isValid, but "inlined" since this function is hot.
        return *this;
    }

    const auto* runs = object->textRuns();
    if (runs && runs->size()) {
        unsigned totalLength = runs->totalLength();
        // When a user types, we send out notifications with text markers whose offsets are relative
        // to the text at that time. By the time VoiceOver sends that text marker back to us, the text
        // may have further changed (e.g. when rapidly deleting multiple characters). This can also
        // happen when VoiceOver is holding on to a stale text marker. Gracefully handle this scenario
        // by setting this text marker back in-bounds.
        clampOffsetToLengthIfNeeded(totalLength);
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
    RefPtr current = runs ? WTFMove(object) : findObjectWithRuns(*object, AXDirection::Next, stopAtID);
    while (current) {
        unsigned totalLength = current->textRuns()->totalLength();
        if (precedingOffset + totalLength >= offset())
            break;
        precedingOffset += totalLength;
        current = findObjectWithRuns(*current, AXDirection::Next, stopAtID);
    }

    if (!current)
        return { };

    TEXT_MARKER_ASSERT(offset() >= precedingOffset);
    if (offset() < precedingOffset)
        return *this;

    return { current->treeID(), current->objectID(), static_cast<unsigned>(offset() - precedingOffset) };
}

bool AXTextMarker::isInTextRun() const
{
    const auto* runs = this->runs();
    return runs && runs->size();
}

AXTextMarkerRange AXTextMarker::lineRange(LineRangeType type, IncludeTrailingLineBreak includeTrailingLineBreak) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().lineRange(type, includeTrailingLineBreak);

    if (type == LineRangeType::Current) {
        auto startMarker = atLineStart() ? *this : previousLineStart();
        auto endMarker = atLineEnd() ? *this : nextLineEnd(includeTrailingLineBreak);
        return AXTextMarkerRange(startMarker, endMarker);
    }

    if (type == LineRangeType::Left) {
        // Move backwards off a line start (because this is a "left-line" request).
        auto startMarker = atLineStart() ? findMarker(AXDirection::Previous) : *this;
        if (!startMarker.atLineStart())
            startMarker = startMarker.previousLineStart();

        auto endMarker = startMarker.nextLineEnd(includeTrailingLineBreak);
        return { WTFMove(startMarker), WTFMove(endMarker) };
    }

    ASSERT(type == LineRangeType::Right);

    // Move forwards off a line end (because this a "right-line" request).
    auto startMarker = atLineEnd() ? findMarker(AXDirection::Next) : *this;
    if (!startMarker.atLineStart())
        startMarker = startMarker.previousLineStart();

    auto endMarker = startMarker.nextLineEnd(includeTrailingLineBreak);
    return { WTFMove(startMarker), WTFMove(endMarker) };

    return { };
}

AXTextMarkerRange AXTextMarker::wordRange(WordRangeType type) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().wordRange(type);

    AXTextMarker startMarker, endMarker;

    if (type == WordRangeType::Right) {
        endMarker = nextWordEnd();
        // To match the live tree, if we end up in the same spot, return a length 0 text marker.
        if (hasSameObjectAndOffset(endMarker))
            return { *this, *this };

        startMarker = endMarker.previousWordStart();
        // Don't return a right word if the word start is more than a position away from current text marker (e.g., there's a space between the word and current marker).
        auto order = startMarker <=> *this;
        if (order == std::partial_ordering::unordered)
            return { };
        if (is_gt(order))
            return { *this, *this };
    } else {
        startMarker = previousWordStart();
        // To match the live tree, if we end up in the same spot, return a length 0 text marker.
        if (hasSameObjectAndOffset(startMarker))
            return { *this, *this };

        endMarker = startMarker.nextWordEnd();
        // Don't return a left word if the word end is more than a position away from current text marker.
        auto order = endMarker <=> *this;
        if (order == std::partial_ordering::unordered)
            return { };
        if (is_lt(order))
            return { *this, *this };
    }

    return { WTFMove(startMarker), WTFMove(endMarker) };
}

AXTextMarkerRange AXTextMarker::sentenceRange(SentenceRangeType type) const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().sentenceRange(type);

    AXTextMarker startMarker, endMarker;

    if (type == SentenceRangeType::Current) {
        startMarker = previousSentenceStart();
        endMarker = startMarker.nextSentenceEnd();
        auto rangeString = AXTextMarkerRange { startMarker, endMarker }.toString();
        // If the sentence iterator returned a string of all whitespace characters, make the range out of the start marker (to match live tree behavior).
        if (rangeString.containsOnly<isASCIIWhitespace>())
            endMarker = startMarker;
    }

    return { WTFMove(startMarker), WTFMove(endMarker) };
}

AXTextMarkerRange AXTextMarker::paragraphRange() const
{
    if (!isValid())
        return { };
    if (!isInTextRun())
        return toTextRunMarker().paragraphRange();

    // paragraphForCharacterOffset on the main thread doesn't directly call nextParagraphEnd and previousParagraphStart.
    // When actually computing the range from the current position, directly call findParagraph.
    AXTextMarker startMarker = findParagraph(AXDirection::Previous, AXTextUnitBoundary::Start);
    AXTextMarker endMarker = findParagraph(AXDirection::Next, AXTextUnitBoundary::End);
    auto rangeString = AXTextMarkerRange { startMarker, endMarker }.toString();
    if (rangeString.containsOnly<isASCIIWhitespace>())
        endMarker = startMarker;

    return { WTFMove(startMarker), WTFMove(endMarker) };
}

bool AXTextMarker::equivalentTextPosition(const AXTextMarker& other) const
{
    return objectID() != other.objectID() && (findMarker(AXDirection::Next, CoalesceObjectBreaks::No, IgnoreBRs::Yes) == other || findMarker(AXDirection::Previous, CoalesceObjectBreaks::No, IgnoreBRs::Yes) == other);
}

namespace Accessibility {
// Finds the next object with text runs in the given direction, optionally stopping at the given ID and returning std::nullopt.
// You may optionally pass a lambda that runs each time an object is "exited" in the traversal, i.e. we processed its children
// (if present) and are moving beyond it. This can help mirror TextIterator::exitNode in the contexts where that's necessary.
AXIsolatedObject* findObjectWithRuns(AXIsolatedObject& start, AXDirection direction, std::optional<AXID> stopAtID, const std::function<void(AXIsolatedObject&)>& exitObject)
{
    auto shouldStop = [&stopAtID] (auto& object) {
        return stopAtID && *stopAtID == object.objectID();
    };

    if (direction == AXDirection::Next) {
        auto nextInPreOrder = [&] (AXIsolatedObject& object) -> AXIsolatedObject* {
            const auto& children = object.childrenIncludingIgnored();
            if (!children.isEmpty()) {
                auto role = object.role();
                if (role != AccessibilityRole::Column && role != AccessibilityRole::TableHeaderContainer && (object.isImage() || !object.isReplacedElement())) {
                    // Table columns and header containers add cells despite not being their "true" parent (which are the rows).
                    // Don't allow a pre-order traversal of these object types to return cells to avoid an infinite loop.
                    //
                    // We also don't want to descend into non-image replaced elements (e.g. <audio>), which can have user-agent shadow tree markup.
                    // This matches TextIterator behavior, and prevents us from emitting incorrect text.
                    return downcast<AXIsolatedObject>(children[0].ptr());
                }
            }

            RefPtr current = object;
            RefPtr next = object.nextSiblingIncludingIgnored(/* updateChildrenIfNeeded */ true);
            for (; !next; next = current->nextSiblingIncludingIgnored(/* updateChildrenIfNeeded */ true)) {
                if (shouldStop(*current))
                    return nullptr;
                RefPtr parent = current->parentObject();
                if (!parent || shouldStop(*parent))
                    return nullptr;
                // We immediately exit parent when evaluating next = current->... in the update step of the containing for-loop,
                // so run any exit lambda for it now.
                exitObject(*parent);
                current = parent;
            }
            return downcast<AXIsolatedObject>(next.get());
        };

        RefPtr current = nextInPreOrder(start);
        while (current) {
            if (shouldStop(*current))
                return nullptr;
            if (current->hasTextRuns())
                break;
            exitObject(*current);
            current = nextInPreOrder(*current);
        }
        return current.unsafeGet();
    }
    ASSERT(direction == AXDirection::Previous);

    auto previousInPreOrder = [&] (AXIsolatedObject& object) -> AXIsolatedObject* {
        if (RefPtr sibling = object.previousSiblingIncludingIgnored(/* updateChildrenIfNeeded */ true)) {
            if (shouldStop(*sibling))
                return nullptr;

            const auto& children = sibling->childrenIncludingIgnored(/* updateChildrenIfNeeded */ true);
            if (children.size())
                return downcast<AXIsolatedObject>(sibling->deepestLastChildIncludingIgnored(/* updateChildrenIfNeeded */ true));
            return downcast<AXIsolatedObject>(sibling.get());
        }
        return object.parentObject();
    };

    RefPtr current = previousInPreOrder(start);
    while (current) {
        if (shouldStop(*current))
            return nullptr;
        if (current->hasTextRuns())
            break;
        exitObject(*current);
        current = previousInPreOrder(*current);
    }
    return current.unsafeGet();
}

} // namespace Accessibility

#endif // ENABLE(AX_THREAD_TEXT_APIS)

} // namespace WebCore
