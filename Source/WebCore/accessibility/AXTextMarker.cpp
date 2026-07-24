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
#include "AXLoggerBase.h"
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
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/CharacterNames.h>

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

TextMarkerData::TextMarkerData(AXObjectCache& cache, const VisiblePosition& visiblePosition, int charStart, int charOffset, bool isRedactedParam, TextMarkerOrigin originParam)
{
    AX_ASSERT(isMainThread());
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    AX_ASSERT(!AXObjectCache::shouldCreateAXThreadCompatibleMarkers());
#endif

    zeroBytes(*this);
    treeID = cache.treeID().toUInt64();
    auto position = visiblePosition.deepEquivalent();
    auto optionalObjectID = nodeID(cache, protect(position.anchorNode()).get());
    objectID = optionalObjectID ? optionalObjectID->toUInt64() : 0;
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = position.anchorType();
    affinity = visiblePosition.affinity();
    characterStart = std::max(charStart, 0);
    characterOffset = std::max(charOffset, 0);
    isRedacted = isRedactedParam;
    origin = originParam;
}

TextMarkerData::TextMarkerData(AXObjectCache& cache, const CharacterOffset& characterOffsetParam, bool isRedactedParam, TextMarkerOrigin originParam)
{
    AX_ASSERT(isMainThread());

    zeroBytes(*this);

    auto visiblePosition = cache.visiblePositionFromCharacterOffset(characterOffsetParam);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (AXObjectCache::shouldCreateAXThreadCompatibleMarkers()) {
        if (std::optional data = cache.textMarkerDataForVisiblePosition(WTF::move(visiblePosition), origin))
            *this = *data;
        return;
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    treeID = cache.treeID().toUInt64();
    auto optionalObjectID = nodeID(cache, characterOffsetParam.node.get());
    objectID = optionalObjectID ? optionalObjectID->toUInt64() : 0;
    auto position = visiblePosition.deepEquivalent();
    offset = !visiblePosition.isNull() ? std::max(position.deprecatedEditingOffset(), 0) : 0;
    anchorType = Position::PositionIsOffsetInAnchor;
    affinity = visiblePosition.affinity();
    characterStart = std::max(characterOffsetParam.startIndex, 0);
    characterOffset = std::max(characterOffsetParam.offset, 0);
    isRedacted = isRedactedParam;
    origin = originParam;
}

AXTextMarker::AXTextMarker(const VisiblePosition& visiblePosition, TextMarkerOrigin origin)
{
    AX_ASSERT(isMainThread());

    if (visiblePosition.isNull())
        return;

    RefPtr node = visiblePosition.deepEquivalent().anchorNode();
    AX_ASSERT(node);
    if (!node)
        return;

    CheckedPtr cache = protect(node->document())->axObjectCache();
    if (!cache)
        return;

    if (auto data = cache->textMarkerDataForVisiblePosition(visiblePosition, origin))
        m_data = WTF::move(*data);
}

AXTextMarker::AXTextMarker(const CharacterOffset& characterOffset, TextMarkerOrigin origin)
{
    AX_ASSERT(isMainThread());

    if (characterOffset.isNull())
        return;

    if (CheckedPtr cache = protect(characterOffset.node->document())->axObjectCache())
        m_data = cache->textMarkerDataForCharacterOffset(characterOffset, origin);
}

AXTextMarker::operator VisiblePosition() const
{
    AX_ASSERT(isMainThread());

    WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID());
    if (!cache)
        return { };

    return cache->visiblePositionForTextMarkerData(m_data);
}

AXTextMarker::operator CharacterOffset() const
{
    AX_ASSERT(isMainThread());

    if (isRedacted() || isNull())
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
    AX_ASSERT(isMainThread());

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

String AXTextMarker::description() const
{
    auto separator = ", "_s;
    RefPtr object = this->object();

    // Most text markers have the default affinity of downstream — avoid noisy output by only logging anything if
    // the value is the non-default one: upstream.
    String affinity = m_data.affinity == Affinity::Downstream ? ""_s : makeString(separator, "upstream"_s);
    String origin = m_data.origin == TextMarkerOrigin::Unknown ? ""_s : makeString(separator, originToString(m_data.origin));

    return makeString("{"_s
        , object ? makeString("role "_s, roleToString(object->role())) : "no object"_s
        , isRedacted() ? makeString(separator, "redacted"_s) : ""_s
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

String AXTextMarker::debugDescription() const
{
    String description = this->description();
    RefPtr object = this->object();
    String id = object ? makeString("ID "_s, object->objectID().loggingString()) : String("no object"_s);

    // Insert the object ID after the opening brace.
    return makeString("{"_s, id, ", "_s, StringView(description).substring(1));
}

AXTextMarkerRange::AXTextMarkerRange(const VisibleSelection& selection)
    : m_start(selection.visibleStart())
    , m_end(selection.visibleEnd())
{
    AX_ASSERT(isMainThread());
}

AXTextMarkerRange::AXTextMarkerRange(const VisiblePositionRange& range)
    : m_start(range.start)
    , m_end(range.end)
{
    AX_ASSERT(isMainThread());
}

AXTextMarkerRange::AXTextMarkerRange(const std::optional<SimpleRange>& range)
{
    AX_ASSERT(isMainThread());

    if (!range)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (AXObjectCache::shouldCreateAXThreadCompatibleMarkers()) {
        auto visiblePositionRange = makeVisiblePositionRange(range);
        m_start = AXTextMarker { visiblePositionRange.start };
        m_end = AXTextMarker { visiblePositionRange.end };
        return;
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    if (CheckedPtr cache = protect(range->start.document())->axObjectCache()) {
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
    m_start = reverse ? WTF::move(end) : WTF::move(start);
    m_end = reverse ? WTF::move(start) : WTF::move(end);
}

AXTextMarkerRange::AXTextMarkerRange(std::optional<AXTreeID> treeID, std::optional<AXID> objectID, unsigned start, unsigned end)
{
    if (start > end)
        std::swap(start, end);
    m_start = AXTextMarker({ treeID, objectID, start, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, start });
    m_end = AXTextMarker({ treeID, objectID, end, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, end });
}

AXTextMarkerRange::operator VisiblePositionRange() const
{
    AX_ASSERT(isMainThread());
    if (!m_start || !m_end)
        return { };
    return { m_start, m_end };
}

std::optional<SimpleRange> AXTextMarkerRange::simpleRange() const
{
    AX_ASSERT(isMainThread());

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
        AX_ASSERT_NOT_REACHED();
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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainThread()) {
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
        return { { WTF::move(intersectionStart), WTF::move(intersectionEnd) } };
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    // We handle the !isMainThread() case above.
    AX_ASSERT(isMainThread());

    auto intersection = WebCore::intersection(*this, other);
    if (intersection.isNull())
        return std::nullopt;
    return { AXTextMarkerRange(intersection) };
}

String AXTextMarkerRange::description() const
{
    return makeString("text: '"_s, toString(), "'"_s,
        ", start: {"_s, m_start.description(), '}',
        ", end: {"_s, m_end.description(), '}');
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

    if (!isMainThread())
        return object && otherObject ? object->partialOrder(*otherObject) : std::partial_ordering::unordered;

    std::optional startBoundaryPoint = marker1.boundaryPoint();
    if (!startBoundaryPoint)
        return std::partial_ordering::unordered;
    std::optional endBoundaryPoint = marker2.boundaryPoint();
    if (!endBoundaryPoint)
        return std::partial_ordering::unordered;
    return treeOrder<ComposedTree>(*startBoundaryPoint, *endBoundaryPoint);
}

bool AXTextMarkerRange::isConfinedTo(std::optional<AXID> objectID) const
{
    return m_start.objectID() == objectID
        && m_end.objectID() == objectID
        && m_start.treeID() == m_end.treeID();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
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
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

String AXTextMarkerRange::toString(IncludeListMarkerText includeListMarkerText, IncludeImageAltText includeImageAltText) const
{
#if !ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    UNUSED_PARAM(includeListMarkerText);
#endif // !ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainThread()) {
        // Traverses from m_start to m_end, collecting all text along the way.
        auto markers = toValidTextRunMarkers();
        if (!markers)
            return emptyString();
        auto& [start, end] = *markers;

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
            RefPtr next = findObjectWithRuns(*current, AXDirection::Next, std::nullopt, emitAuxiliaryText);
            if (next == current) [[unlikely]] {
                // findObjectWithRuns returned its input. Would loop forever.
                AX_ASSERT_NOT_REACHED();
                break;
            }
            current = WTF::move(next);
        }
        result.append(end.runs()->substring(0, end.offset()));
        return result.toString();
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    std::optional range = simpleRange();
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
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
AXTextMarker AXTextMarker::convertToDomOffset() const
{
    AX_ASSERT(!isMainThread());

    if (!isValid())
        return { };
    if (!isInTextRun()) {
        // This marker is anchored to a non-text object, so to compute a DOM offset we normalize it to
        // the text run its offset points into. A non-text *container* (e.g. a group) legitimately
        // resolves to a descendant text run, but an empty non-text *leaf* (e.g. a button with no text
        // of its own) has none, and the forward walk can then escape into unrelated downstream text. If
        // that text is inside an editable, applying the result as the selection steals DOM focus into
        // the editable, which can cause adverse side effects like navigation loops.
        //
        // So, when the resolved text run lies outside this object's own subtree, anchor at the object
        // itself. Return a normalized element-anchored offset (0) so we maintain the invariant of this
        // function always returning a DOM-offset marker.
        auto textRunMarker = toTextRunMarker();
        RefPtr object = isolatedObject();
        RefPtr textRunObject = textRunMarker.isolatedObject();
        if (!textRunMarker.isValid() || !object || !textRunObject || !object->isAncestorOfObject(*textRunObject))
            return { treeID(), objectID(), 0 };

        return textRunMarker.convertToDomOffset();
    }

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

    // Fast path: when the start marker and this marker share a containing block, both
    // line IDs are drawn from that block's own monotonic line numbering (the line-box
    // index within the RenderBlock), so the number of lines between them is simply the
    // difference of their line indices. This avoids the line-by-line walk below, which
    // starts at the beginning of the document (or editable/text-control root) and is
    // therefore O(lines-from-start) on every call. That is pathological on a page that
    // is a single large block wrapping onto thousands of lines (e.g. a long flat list
    // of links), where an assistive technology requests the line index on every caret
    // movement, making a full traversal O(lines^2).
    //
    // Only out-of-flow (float / position:absolute) replaced elements and boxless line breaks
    // store their own renderer in the lineID's containing-block slot (see
    // AccessibilityRenderObject::textRuns); their synthetic lineIDs don't compare equal here and
    // fall through to the walk. Everything else — including a normal in-flow inline <img>, which
    // uses box->lineIndex() against the real containing block — compares equal and takes this
    // fast path, which is correct because its line index is real.
    if (currentLineID.containingBlock && currentLineID.containingBlock == targetLineID.containingBlock
        && targetLineID.lineIndex >= currentLineID.lineIndex)
        return static_cast<int>(targetLineID.lineIndex - currentLineID.lineIndex);

    auto currentMarker = WTF::move(startMarker);
    if (!currentMarker.atLineEnd()) {
        // Start from a line end, so that subsequent calls to nextLineEnd() yield a new line.
        // Otherwise if we started from the middle of a line, we would count the the first line twice.
        auto nextLineEndMarker = currentMarker.nextLineEnd();
        TEXT_MARKER_ASSERT_DOUBLE(nextLineEndMarker.lineID() == currentMarker.lineID(), nextLineEndMarker, currentMarker);
        currentMarker = WTF::move(nextLineEndMarker);
    }

    unsigned index = 0;
    while (currentLineID && currentLineID != targetLineID) {
        auto newMarker = currentMarker.nextLineEnd();
        auto newLineID = newMarker.lineID();
        ++index;

        if (currentLineID == newLineID && currentMarker == newMarker) {
            // nextLineEnd() returned its input, so break. The line walk would loop
            // forever otherwise, causing a hang. This indicates a bug elsewhere
            // (e.g. a sibling lineID collision the caller couldn't disambiguate).
            AX_ASSERT_NOT_REACHED();
            break;
        }

        currentMarker = WTF::move(newMarker);
        currentLineID = newLineID;
    }
    return index;
}

// A text control whose value ends in a line break renders an empty final line. Depending on
// whether the control is being edited, that line's sole content, a single newline, is exposed
// either as a placeholder <br> (role LineBreak, appended by HTMLTextFormControlElement::
// setInnerTextValue) or as a text node (role StaticText). Detect it by content and position, a
// lone newline with no following text runs within the text control (bounded by |stopAtID|),
// rather than by object role, which differs between those two representations. |lineLength| is
// the caller's already-computed length of |lineRange|; a lone newline is length 1, so checking it
// first avoids building the range's string (and walking for following runs) on every other line.
static bool isOnTrailingPlaceholderBlankLine(const AXTextMarkerRange& lineRange, unsigned lineLength, std::optional<AXID> stopAtID)
{
    if (lineLength != 1 || lineRange.toString() != "\n"_s)
        return false;
    RefPtr object = lineRange.start().isolatedObject();
    return object && !findObjectWithRuns(*object, AXDirection::Next, stopAtID);
}

// Advances |lineRange| to the following line: the range from the start of the next line through
// its end. Returns an invalid range when there is no next line (nextLineEnd does not advance),
// which ends the line walks in characterRangeForLine, markerRangeForLineIndex, and
// lineNumberForIndex. |includeTrailingLineBreak| and |stopAtID| select the caller's line semantics.
static AXTextMarkerRange nextLineRange(const AXTextMarkerRange& lineRange, IncludeTrailingLineBreak includeTrailingLineBreak, std::optional<AXID> stopAtID)
{
    auto lineEnd = lineRange.end();
    auto nextLineEndMarker = lineEnd.nextLineEnd(includeTrailingLineBreak, stopAtID);
    if (nextLineEndMarker == lineEnd)
        return { };
    return { nextLineEndMarker.previousLineStart(stopAtID), WTF::move(nextLineEndMarker) };
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
        currentLineRange = nextLineRange(currentLineRange, IncludeTrailingLineBreak::Yes, stopAtID);
        --lineIndex;
    }
    if (!currentLineRange)
        return { };
    // Report the trailing blank line of a value ending in a line break as an empty range at the
    // document end, so it reads as an empty line rather than a line whose content is a newline.
    unsigned lineLength = currentLineRange.toString().length();
    if (isOnTrailingPlaceholderBlankLine(currentLineRange, lineLength, stopAtID))
        return CharacterRange(precedingLength, 0);
    return CharacterRange(precedingLength, lineLength);
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
        currentLineRange = nextLineRange(currentLineRange, IncludeTrailingLineBreak::No, std::nullopt);
        --lineIndex;
    }
    return currentLineRange;
}

int AXTextMarker::lineNumberForIndex(unsigned index) const
{
    RefPtr object = isolatedObject();
    if (!object)
        return -1;

    std::optional stopAtID = object->idOfNextSiblingIncludingIgnoredOrParent();
    auto textRunMarker = toTextRunMarker(stopAtID);
    if (!textRunMarker.isValid())
        return !index ? 0 : -1;

    // Walk lines with nextLineRange, the same helper characterRangeForLine uses, so the two APIs
    // stay consistent: return the line whose [start, end) character range contains |index|, or -1
    // if |index| is past the end.
    unsigned lineStart = 0;
    unsigned lineNumber = 0;
    auto currentLineRange = textRunMarker.lineRange(LineRangeType::Current, IncludeTrailingLineBreak::Yes);
    while (currentLineRange) {
        unsigned lineLength = currentLineRange.toString().length();
        if (index < lineStart + lineLength) {
            // Report an index on the trailing blank line of a value ending in a line break as
            // out of range, matching the non-isolated AccessibilityRenderObject path: that line
            // sits at the document end and is surfaced by characterRangeForLine (an empty range)
            // instead. A value with no trailing line break has no such line and is unaffected.
            if (isOnTrailingPlaceholderBlankLine(currentLineRange, lineLength, stopAtID))
                return -1;
            return static_cast<int>(lineNumber);
        }
        lineStart += lineLength;
        currentLineRange = nextLineRange(currentLineRange, IncludeTrailingLineBreak::Yes, stopAtID);
        ++lineNumber;
    }
    return -1;
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

// --- Shared text-length walk backing AXIndexForTextMarker (offsetFromRoot) and
// --- AXTextMarkerForIndex (nextMarkerFromOffset).
//
// These two attributes must be inverses: VoiceOver converts a text marker to a
// document-relative index and back, and any drift (especially drift that accumulates
// down the page) is a serious bug. To guarantee they invert each other, both are driven
// by ONE traversal — forEachRunObjectForward — that visits text-run objects in document
// order via findObjectWithRuns and counts characters the same way AXTextMarkerRange::toString
// emits them: each run contributes its length, and crossing a block boundary contributes a
// newline (TextEmissionBehavior::Newline => 1, DoubleNewline => 2) unless the previously
// emitted character was already a newline. Image alt text is intentionally NOT counted, so
// the index space is independent of it (this is the one place we deliberately diverge from
// toString).

static bool runEndsWithNewline(const AXIsolatedObject& object)
{
    const auto* runs = object.textRuns();
    return runs && runs->toStringView().endsWith('\n');
}

// Mirrors the newline emission in AXTextMarkerRange::toString's emitAuxiliaryText (minus alt text).
static unsigned emittedNewlineLength(const AXCoreObject& object, bool lastEmittedWasNewline)
{
    if (lastEmittedWasNewline)
        return 0;
    switch (object.textEmissionBehavior()) {
    case TextEmissionBehavior::Newline:
        return 1;
    case TextEmissionBehavior::DoubleNewline:
        return 2;
    case TextEmissionBehavior::None:
    case TextEmissionBehavior::Tab:
        return 0;
    }
    return 0;
}

// Visits each text-run object reachable forward from `start`'s object (inclusive), in document
// order, invoking visit(object, precedingNewlines) where precedingNewlines is the number of
// newline characters emitted immediately before that object's text. Return false from `visit`
// to stop. The newline accounting matches AXTextMarkerRange::toString (excluding alt text).
template<typename Visitor>
static void forEachRunObjectForward(const AXTextMarker& start, std::optional<AXID> stopAtID, Visitor&& visit)
{
    RefPtr current = start.isolatedObject();
    const auto* runs = current ? current->textRuns() : nullptr;
    if (!runs || !runs->size())
        return;

    if (!visit(*current, 0u))
        return;
    bool lastEmittedWasNewline = runEndsWithNewline(*current);

    for (unsigned iterations = 0; ; ++iterations) {
        if (iterations >= maxDescendantTraversalIterations) [[unlikely]] {
            // Failsafe: never spin forever on a malformed (e.g. cyclic) tree.
            AX_ASSERT_NOT_REACHED();
            break;
        }

        unsigned precedingNewlines = 0;
        auto exitObject = [&] (AXIsolatedObject& exited) {
            if (unsigned emitted = emittedNewlineLength(exited, lastEmittedWasNewline)) {
                precedingNewlines += emitted;
                lastEmittedWasNewline = true;
            }
        };
        RefPtr next = findObjectWithRuns(*current, AXDirection::Next, stopAtID, exitObject);
        if (!next || next == current)
            break;
        if (!visit(*next, precedingNewlines))
            return;
        lastEmittedWasNewline = runEndsWithNewline(*next);
        current = WTF::move(next);
    }
}

unsigned AXTextMarker::offsetFromRoot() const
{
    AX_ASSERT(!isMainThread());

    if (!isValid())
        return 0;
    auto target = toTextRunMarker();
    if (!target.isValid())
        return 0;

    RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID()));
    RefPtr root = tree ? tree->rootNode() : nullptr;
    if (!root)
        return 0;
    auto start = AXTextMarker { root->treeID(), root->objectID(), 0 }.toTextRunMarker();
    if (!start.isValid())
        return 0;

    // `start` is the document's first text position, so its offset within its run is 0. The walk
    // therefore counts each object's full run length, with no per-object base to subtract (which
    // also means there is no unsigned subtraction here to underflow).
    TEXT_MARKER_ASSERT(!start.offset());
    auto targetID = target.objectID();
    unsigned offset = 0;
    bool found = false;
    forEachRunObjectForward(start, std::nullopt, [&] (AXIsolatedObject& object, unsigned precedingNewlines) {
        offset += precedingNewlines;
        if (object.objectID() == targetID) {
            offset += target.offset();
            found = true;
            return false;
        }
        offset += object.textRuns()->totalLength();
        return true;
    });

    // If this assert fails, it means we couldn't navigate from root to `this`, which should never happen.
    TEXT_MARKER_ASSERT_DOUBLE(found, (*this), target);
    return offset;
}

// The ForceSingleOffsetMovement argument is intentionally ignored: this walk always advances by
// UTF-16 code units (offsets into the run text), which is what the old ForceSingleOffsetMovement::Yes
// path did. That keeps the index space consistent with offsetFromRoot (AXIndexForTextMarker) and with
// stringForTextMarkerRange, all of which count UTF-16 code units, so the round-trip and index/length
// invariants hold for non-ASCII text too. The trade-off is that an index landing inside a grapheme
// cluster (e.g. between the halves of a surrogate pair) yields a marker inside that cluster.
// FIXME: Consider snapping the returned marker to a grapheme-cluster boundary (a bounded,
// non-accumulating snap, like the newline-gap case below).
AXTextMarker AXTextMarker::nextMarkerFromOffset(unsigned offset, ForceSingleOffsetMovement, std::optional<AXID> stopAtID) const
{
    AX_ASSERT(!isMainThread());

    if (!isValid())
        return { };
    auto start = toTextRunMarker(stopAtID);
    if (!start.isValid())
        return { };

    // Walk the same traversal offsetFromRoot counts, consuming `offset` characters. Because both
    // functions share forEachRunObjectForward with identical newline accounting, the marker this
    // returns satisfies offsetFromRoot(result) == offset for every position that has a distinct
    // text-run marker — i.e. the round-trip is exact for real markers. Positions that fall inside an
    // emitted (virtual) newline gap have no marker of their own, so they snap back to the end of the
    // preceding run; this is a bounded, non-accumulating snap (acceptable per VoiceOver's needs).
    unsigned remaining = offset;
    auto result = start;
    forEachRunObjectForward(start, stopAtID, [&] (AXIsolatedObject& object, unsigned precedingNewlines) {
        if (remaining < precedingNewlines) {
            // Target lands within an emitted newline gap; keep `result` at the previous run's end.
            return false;
        }
        remaining -= precedingNewlines; // Safe: guarded by the check above.

        unsigned totalLength = object.textRuns()->totalLength();
        // Only the first object can start partway through its run (when `this` was mid-run). Clamp so
        // `totalLength - base` can never underflow even if a stale marker's offset is out of bounds.
        unsigned base = object.objectID() == start.objectID() ? std::min(start.offset(), totalLength) : 0;
        unsigned available = totalLength - base;
        if (remaining <= available) {
            result = AXTextMarker { object, base + remaining };
            return false;
        }
        remaining -= available; // Safe: guarded by the check above.
        result = AXTextMarker { object, totalLength };
        return true;
    });
    return result;
}

AXTextMarker AXTextMarker::findLastBefore(std::optional<AXID> stopAtID) const
{
    AX_ASSERT(!isMainThread());

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
        if (newObject == lastObjectWithRuns) [[unlikely]] {
            // findObjectWithRuns returned its input. Would loop forever.
            AX_ASSERT_NOT_REACHED();
            break;
        }
        lastObjectWithRuns = WTF::move(newObject);
    }

    return AXTextMarker { *lastObjectWithRuns, lastObjectWithRuns->textRuns()->totalLength() };
}

AXTextMarkerRange AXTextMarker::rangeWithSameStyle() const
{
    AX_ASSERT(!isMainThread());

    if (!isValid())
        return { };

    auto originalStyle = object()->stylesForAttributedString();
    auto findMarkerWithDifferentStyle = [&] (AXDirection direction) -> AXTextMarker {
        RefPtr current = isolatedObject();
        while (current) {
            RefPtr next = findObjectWithRuns(*current, direction);
            if (next && originalStyle != next->stylesForAttributedString())
                break;
            current = WTF::move(next);
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

std::optional<std::pair<AXTextMarker, AXTextMarker>> AXTextMarkerRange::toValidTextRunMarkers() const
{
    auto start = m_start.toTextRunMarker();
    if (!start.isValid())
        return std::nullopt;
    auto end = m_end.toTextRunMarker();
    if (!end.isValid())
        return std::nullopt;
    return { { WTF::move(start), WTF::move(end) } };
}

FloatRect AXTextMarkerRange::viewportRelativeFrame() const
{
    AX_ASSERT(!isMainThread());

    auto markers = toValidTextRunMarkers();
    if (!markers)
        return { };
    auto& [start, end] = *markers;

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
        RefPtr next = findObjectWithRuns(*current, AXDirection::Next, /* stopAtID */ *end.objectID());
        if (next == current) [[unlikely]] {
            // findObjectWithRuns returned its input. Would loop forever.
            AX_ASSERT_NOT_REACHED();
            break;
        }
        current = WTF::move(next);
    }
    result.unite(viewportRelativeFrameFromRuns(*end.isolatedObject(), /* start */ 0, /* end */ end.offset()));

    return result;
}

AXTextMarkerRange AXTextMarkerRange::convertToDomOffsetRange() const
{
    AX_ASSERT(!isMainThread());

    return {
        m_start.convertToDomOffset(),
        m_end.convertToDomOffset()
    };
}

const AXTextRuns* AXTextMarker::runs() const
{
    AX_ASSERT(!isMainThread());

    RefPtr object = isolatedObject();
    return object ? object->textRuns() : nullptr;
}

// Custom text unit iterator wrappers

static int previousSentenceStartFromOffset(StringView text, unsigned offset)
{
    return ubrk_preceding(WTF::NonSharedSentenceBreakIterator(text), offset);
}

static int nextSentenceEndFromOffset(StringView text, unsigned offset)
{
    int endIndex = ubrk_following(WTF::NonSharedSentenceBreakIterator(text), offset);

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
                AX_ASSERT(adjacentRunIndex - runIndex == 2);
                // This scenario really should only happen with single "entity" runs (where an entity could be an ASCII
                // character, or a multi-byte emoji that occupies multiple indices but is one atomic entity).
                AX_BROKEN_ASSERT(!currentRuns->containsOnlyASCII || (currentRuns->runLength(runIndex) == 1 && currentRuns->runLength(adjacentRunIndex) == 1));
                // The next line end is simply the adjacent marker with an upstream affinity (with an ASSERT to verify this).
                AX_ASSERT(currentRuns->indexForOffset(adjacentMarker.offset(), Affinity::Upstream) == runIndex + 1);
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
    RefPtr current = runs ? WTF::move(object) : RefPtr { findObjectWithRuns(*object, AXDirection::Next, stopAtID) };
    while (current) {
        unsigned totalLength = current->textRuns()->totalLength();
        if (precedingOffset + totalLength >= offset())
            break;
        precedingOffset += totalLength;
        RefPtr next = findObjectWithRuns(*current, AXDirection::Next, stopAtID);
        if (next == current) [[unlikely]] {
            // findObjectWithRuns returned its input. Would loop forever.
            AX_ASSERT_NOT_REACHED();
            break;
        }
        current = WTF::move(next);
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
        return { WTF::move(startMarker), WTF::move(endMarker) };
    }

    AX_ASSERT(type == LineRangeType::Right);

    // Move forwards off a line end (because this a "right-line" request).
    auto startMarker = atLineEnd() ? findMarker(AXDirection::Next) : *this;
    if (!startMarker.atLineStart())
        startMarker = startMarker.previousLineStart();

    auto endMarker = startMarker.nextLineEnd(includeTrailingLineBreak);
    return { WTF::move(startMarker), WTF::move(endMarker) };

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

    return { WTF::move(startMarker), WTF::move(endMarker) };
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

    return { WTF::move(startMarker), WTF::move(endMarker) };
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

    return { WTF::move(startMarker), WTF::move(endMarker) };
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
            return downcast<AXIsolatedObject>(next.unsafeGet());
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
    AX_ASSERT(direction == AXDirection::Previous);

    auto previousInPreOrder = [&] (AXIsolatedObject& object) -> AXIsolatedObject* {
        if (RefPtr sibling = object.previousSiblingIncludingIgnored(/* updateChildrenIfNeeded */ true)) {
            if (shouldStop(*sibling))
                return nullptr;

            const auto& children = sibling->childrenIncludingIgnored(/* updateChildrenIfNeeded */ true);
            if (children.size())
                return downcast<AXIsolatedObject>(sibling->deepestLastChildIncludingIgnored(/* updateChildrenIfNeeded */ true));
            return downcast<AXIsolatedObject>(sibling.unsafeGet());
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

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

} // namespace WebCore
