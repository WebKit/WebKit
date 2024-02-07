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

#pragma once

#include "AccessibilityObject.h"

namespace WebCore {

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
class AXIsolatedObject;
#endif
struct CharacterOffset;
struct AXTextRuns;

enum class AXTextUnit : uint8_t {
    Line,
    Paragraph,
    Sentence,
    Word,
};
enum class AXTextUnitBoundary : bool { Start, End, };

enum class LineRangeType : uint8_t {
    Current,
    Left,
    Right,
};

struct TextMarkerData {
    unsigned treeID;
    unsigned objectID;

    Node* node;
    unsigned offset;
    Position::AnchorType anchorType;
    Affinity affinity;

    unsigned characterStart;
    unsigned characterOffset;
    bool ignored;

    // Constructors of TextMarkerData must zero the struct's block of memory because platform client code may rely on a byte-comparison to determine instances equality.
    // Members initialization alone is not enough to guaranty that all bytes in the struct memeory are initialized, and may cause random inequalities when doing byte-comparisons.
    // For an exampel of such byte-comparison, see the TestRunner WTR::AccessibilityTextMarker::isEqual.
    TextMarkerData()
    {
        memset(static_cast<void*>(this), 0, sizeof(*this));
    }

    TextMarkerData(AXID axTreeID, AXID axObjectID,
        Node* nodeParam = nullptr, unsigned offsetParam = 0,
        Position::AnchorType anchorTypeParam = Position::PositionIsOffsetInAnchor,
        Affinity affinityParam = Affinity::Downstream,
        unsigned charStart = 0, unsigned charOffset = 0, bool ignoredParam = false)
    {
        memset(static_cast<void*>(this), 0, sizeof(*this));
        treeID = axTreeID.toUInt64();
        objectID = axObjectID.toUInt64();
        node = nodeParam;
        offset = offsetParam;
        anchorType = anchorTypeParam;
        affinity = affinityParam;
        characterStart = charStart;
        characterOffset = charOffset;
        ignored = ignoredParam;
    }

    TextMarkerData(AXObjectCache&, Node*, const VisiblePosition&, int charStart = 0, int charOffset = 0, bool ignoredParam = false);
    TextMarkerData(AXObjectCache&, const CharacterOffset&, bool ignoredParam = false);

    AXID axTreeID() const
    {
        return ObjectIdentifier<AXIDType>(treeID);
    }

    AXID axObjectID() const
    {
        return ObjectIdentifier<AXIDType>(objectID);
    }
private:
    void initializeAXIDs(AXObjectCache&, Node*);
};

#if PLATFORM(MAC)
using PlatformTextMarkerData = AXTextMarkerRef;
#elif PLATFORM(IOS_FAMILY)
using PlatformTextMarkerData = NSData *;;
#endif

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXTextMarker);
class AXTextMarker {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXTextMarker);
    friend class AXTextMarkerRange;
    friend std::partial_ordering partialOrder(const AXTextMarker&, const AXTextMarker&);
public:
    // Constructors
    AXTextMarker(const VisiblePosition&);
    AXTextMarker(const CharacterOffset&);
    AXTextMarker(const TextMarkerData& data)
        : m_data(data)
    { }
    AXTextMarker(TextMarkerData&& data)
        : m_data(data)
    { }
#if PLATFORM(COCOA)
    AXTextMarker(PlatformTextMarkerData);
#endif
    AXTextMarker(AXID treeID, AXID objectID, unsigned offset)
        : m_data({ treeID, objectID, nullptr, offset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, offset })
    { }
    AXTextMarker() = default;

    operator bool() const { return !isNull(); }
    operator VisiblePosition() const;
    operator CharacterOffset() const;
    std::optional<BoundaryPoint> boundaryPoint() const;
    bool hasSameObjectAndOffset(const AXTextMarker&) const;

#if PLATFORM(COCOA)
    RetainPtr<PlatformTextMarkerData> platformData() const;
    operator PlatformTextMarkerData() const { return platformData().autorelease(); }
#endif

    AXID treeID() const { return m_data.axTreeID(); }
    AXID objectID() const { return m_data.axObjectID(); }
    unsigned offset() const { return m_data.offset; }
    bool isNull() const { return !treeID().isValid() || !objectID().isValid(); }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    // FIXME: Currently, the logic for serving text APIs off the main-thread requires isolated objects, but should eventually be refactored to work with AXCoreObjects.
    RefPtr<AXIsolatedObject> isolatedObject() const;
#endif
    RefPtr<AXCoreObject> object() const;
    bool isValid() const { return object(); }

    Node* node() const;
    bool isIgnored() const { return m_data.ignored; }

    String debugDescription() const;

    // Sets m_data.node when the marker was created with a PlatformTextMarkerData that lacks the node pointer because it was created off the main thread.
    void setNodeIfNeeded() const;

#if ENABLE(AX_THREAD_TEXT_APIS)
    AXTextMarker toTextLeafMarker() const;
    // True if this marker points to a leaf node (no children) with non-empty text runs.
    bool isInTextLeaf() const;

    // Find the next or previous marker, optionally stopping at the given ID and returning an invalid marker.
    AXTextMarker findMarker(AXDirection, std::optional<AXID> = std::nullopt) const;
    // Starting from this text marker, creates a new position for the given direction and text unit type.
    AXTextMarker findMarker(AXDirection, AXTextUnit, AXTextUnitBoundary) const;
    // Creates a range for the line this marker points to.
    AXTextMarkerRange lineRange(LineRangeType) const;
    // Given a character offset relative to this marker, find the next marker the offset points to.
    AXTextMarker nextMarkerFromOffset(unsigned) const;
    // Returns the number of intermediate text markers between this and the root.
    unsigned offsetFromRoot() const;
    // Starting from this marker, navigate to the last marker before the given AXID. Assumes `this`
    // is before the AXID in the AX tree (anything else is a bug). std::nullopt means we will find
    // the last marker on the entire webpage.
    AXTextMarker findLastBefore(std::optional<AXID>) const;
    AXTextMarker findLast() const { return findLastBefore(std::nullopt); }
    // Determines partial order by traversing forward and backwards to try the other marker.
    std::partial_ordering partialOrderByTraversal(const AXTextMarker&) const;
#endif // ENABLE(AX_THREAD_TEXT_APIS)

private:
#if ENABLE(AX_THREAD_TEXT_APIS)
    const AXTextRuns* runs() const;
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    TextMarkerData m_data;
};

class AXTextMarkerRange {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Constructors.
    AXTextMarkerRange(const VisiblePositionRange&);
    AXTextMarkerRange(const std::optional<SimpleRange>&);
    AXTextMarkerRange(const AXTextMarker&, const AXTextMarker&);
#if PLATFORM(MAC)
    AXTextMarkerRange(AXTextMarkerRangeRef);
#endif
    AXTextMarkerRange(AXID treeID, AXID objectID, unsigned offset, unsigned length);
    AXTextMarkerRange() = default;

    operator bool() const { return m_start && m_end; }
    operator VisiblePositionRange() const;
    std::optional<SimpleRange> simpleRange() const;
    std::optional<CharacterRange> characterRange() const;

    std::optional<AXTextMarkerRange> intersectionWith(const AXTextMarkerRange&) const;

#if PLATFORM(MAC)
    RetainPtr<AXTextMarkerRangeRef> platformData() const;
    operator AXTextMarkerRangeRef() const { return platformData().autorelease(); }
#endif

#if PLATFORM(COCOA)
    std::optional<NSRange> nsRange() const;
#endif

    AXTextMarker start() const { return m_start; }
    AXTextMarker end() const { return m_end; }
    bool isConfinedTo(AXID) const;

#if ENABLE(AX_THREAD_TEXT_APIS)
    // Traverses from m_start to m_end, collecting all text along the way.
    String toString() const;
#endif

    String debugDescription() const;
private:
    AXTextMarker m_start;
    AXTextMarker m_end;
};

inline Node* AXTextMarker::node() const
{
    ASSERT(isMainThread());
    return m_data.node;
}

} // namespace WebCore
