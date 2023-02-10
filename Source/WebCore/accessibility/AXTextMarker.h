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

struct CharacterOffset;

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
        return makeObjectIdentifier<AXIDType>(treeID);
    }

    AXID axObjectID() const
    {
        return makeObjectIdentifier<AXIDType>(objectID);
    }
private:
    void initializeAXIDs(AXObjectCache&, Node*);
};

#if PLATFORM(MAC)
using PlatformTextMarkerData = AXTextMarkerRef;
#elif PLATFORM(IOS_FAMILY)
using PlatformTextMarkerData = NSData *;;
#endif

class AXTextMarker {
    WTF_MAKE_FAST_ALLOCATED;
    friend class AXTextMarkerRange;
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
    AXTextMarker() = default;

    operator bool() const { return !isNull(); }
    operator VisiblePosition() const;
    operator CharacterOffset() const;

#if PLATFORM(COCOA)
    RetainPtr<PlatformTextMarkerData> platformData() const;
    operator PlatformTextMarkerData() const { return platformData().autorelease(); }
#endif

    AXID treeID() const { return m_data.axTreeID(); }
    AXID objectID() const { return m_data.axObjectID(); }
    bool isNull() const { return !treeID().isValid() || !objectID().isValid(); }
    RefPtr<AXCoreObject> object() const;
    bool isValid() const { return object(); }

    Node* node() const
    {
        ASSERT(isMainThread());
        return m_data.node;
    }
    bool isIgnored() const { return m_data.ignored; }

#if ENABLE(TREE_DEBUGGING)
    String debugDescription() const;
#endif

private:
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
    AXTextMarkerRange() = default;

    operator bool() const { return !m_start.isNull() && !m_end.isNull(); }
    operator VisiblePositionRange() const;
    std::optional<SimpleRange> simpleRange() const;

#if PLATFORM(MAC)
    RetainPtr<AXTextMarkerRangeRef> platformData() const;
    operator AXTextMarkerRangeRef() const { return platformData().autorelease(); }
#endif

    AXTextMarker start() const { return m_start; }
    AXTextMarker end() const { return m_end; }
private:
    AXTextMarker m_start;
    AXTextMarker m_end;
};

} // namespace WebCore
