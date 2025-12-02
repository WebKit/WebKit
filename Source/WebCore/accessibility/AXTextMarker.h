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

#pragma once

#include <WebCore/AXCoreObject.h>
#include <WebCore/AccessibilityObject.h>
#include <WebCore/Position.h>
#include <WebCore/VisiblePosition.h>
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

#define TEXT_MARKER_ASSERT(assertion) do { \
    std::string debugString = "Text marker origin: " + originToString(origin()).utf8().toStdString(); \
    ASSERT_WITH_MESSAGE(assertion, "%s", debugString.c_str()); \
} while (0)
#define TEXT_MARKER_ASSERT_SINGLE(assertion, marker) do { \
    std::string debugString = "Text marker origin: " + originToString(marker.origin()).utf8().toStdString(); \
    ASSERT_WITH_MESSAGE(assertion, "%s", debugString.c_str()); \
} while (0)
#define TEXT_MARKER_ASSERT_DOUBLE(assertion, marker1, marker2) do { \
    std::string debugString = "Text marker origins: " + originToString(marker1.origin()).utf8().toStdString() + ", " + originToString(marker2.origin()).utf8().data(); \
    ASSERT_WITH_MESSAGE(assertion, "%s", debugString.c_str()); \
} while (0)

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
enum class AXTextUnitBoundary : bool { Start, End };

enum class LineRangeType : uint8_t {
    Current,
    Left,
    Right,
};

enum class WordRangeType : uint8_t {
    Left,
    Right,
};

enum class SentenceRangeType : uint8_t {
    Current,
    Left,
    Right,
};

enum class TextMarkerOrigin : uint16_t {
    Unknown, // 0
    PreviousLineStart,
    NextLineEnd,
    NextWordStart,
    NextWordEnd,
    PreviousWordStart,
    PreviousWordEnd,
    PreviousSentenceStart,
    NextSentenceEnd,
    PreviousParagraphStart,
    NextParagraphEnd,
    Position,
    StartTextMarkerForBounds,
    EndTextMarkerForBounds
};

static_assert(TextMarkerOrigin::Unknown == static_cast<TextMarkerOrigin>(0)); // Needed for AXObjectCache.h default parameters

inline String originToString(TextMarkerOrigin origin)
{
    String result;
    switch (origin) {
    case TextMarkerOrigin::PreviousLineStart:
        result = "PreviousLineStart"_s;
        break;
    case TextMarkerOrigin::NextLineEnd:
        result = "NextLineEnd"_s;
        break;
    case TextMarkerOrigin::NextWordStart:
        result = "NextWordStart"_s;
        break;
    case TextMarkerOrigin::NextWordEnd:
        result = "NextWordEnd"_s;
        break;
    case TextMarkerOrigin::PreviousWordStart:
        result = "PreviousWordStart"_s;
        break;
    case TextMarkerOrigin::PreviousWordEnd:
        result = "PreviousWordEnd"_s;
        break;
    case TextMarkerOrigin::PreviousSentenceStart:
        result = "PreviousSentenceStart"_s;
        break;
    case TextMarkerOrigin::NextSentenceEnd:
        result = "NextSentenceEnd"_s;
        break;
    case TextMarkerOrigin::PreviousParagraphStart:
        result = "PreviousParagraphStart"_s;
        break;
    case TextMarkerOrigin::NextParagraphEnd:
        result = "NextParagraphEnd"_s;
        break;
    case TextMarkerOrigin::Position:
        result = "TextMarkerForPosition"_s;
        break;
    case TextMarkerOrigin::StartTextMarkerForBounds:
        result = "StartTextMarkerForBounds"_s;
        break;
    case TextMarkerOrigin::EndTextMarkerForBounds:
        result = "EndTextMarkerForBounds"_s;
        break;
    default:
        result = "Unknown"_s;
        break;
    }

    return result;
}

// Options for findMarker
enum class CoalesceObjectBreaks : bool { No, Yes };
enum class IgnoreBRs : bool { No, Yes };
// This enum represents whether to force movement by singular offsets, vs. moving multiple offsets
// when encountering multi-byte glyphs like emojis.
enum class ForceSingleOffsetMovement : bool { No, Yes };
enum class IncludeTrailingLineBreak : bool { No, Yes };

struct TextMarkerData {
    unsigned treeID;
    unsigned objectID;

    unsigned offset;
    Position::AnchorType anchorType;

    unsigned characterStart;
    unsigned characterOffset;
    bool ignored;
    Affinity affinity;

    TextMarkerOrigin origin;

    // Constructors of TextMarkerData must zero the struct's block of memory because platform client code may rely on a byte-comparison to determine instances equality.
    // Members initialization alone is not enough to guaranty that all bytes in the struct memeory are initialized, and may cause random inequalities when doing byte-comparisons.
    // For an example of such byte-comparison, see the TestRunner WTR::AccessibilityTextMarker::isEqual.
    TextMarkerData()
    {
        zeroBytes(*this);
    }

    TextMarkerData(std::optional<AXID> axTreeID, std::optional<AXID> axObjectID,
        unsigned offsetParam = 0,
        Position::AnchorType anchorTypeParam = Position::PositionIsOffsetInAnchor,
        Affinity affinityParam = Affinity::Downstream,
        unsigned charStart = 0, unsigned charOffset = 0, bool ignoredParam = false, TextMarkerOrigin originParam = TextMarkerOrigin::Unknown)
    {
        zeroBytes(*this);
        treeID = axTreeID ? axTreeID->toUInt64() : 0;
        objectID = axObjectID ? axObjectID->toUInt64() : 0;
        offset = offsetParam;
        anchorType = anchorTypeParam;
        affinity = affinityParam;
        characterStart = charStart;
        characterOffset = charOffset;
        ignored = ignoredParam;
        origin = originParam;
    }

    TextMarkerData(AXObjectCache&, const VisiblePosition&, int charStart = 0, int charOffset = 0, bool ignoredParam = false, TextMarkerOrigin originParam = TextMarkerOrigin::Unknown);
    TextMarkerData(AXObjectCache&, const CharacterOffset&, bool ignoredParam = false, TextMarkerOrigin originParam = TextMarkerOrigin::Unknown);

    friend bool operator==(const TextMarkerData&, const TextMarkerData&) = default;

    std::optional<AXID> axTreeID() const
    {
        return treeID ? std::optional { ObjectIdentifier<AXIDType>(treeID) } : std::nullopt;
    }

    std::optional<AXID> axObjectID() const
    {
        return objectID ? std::optional { ObjectIdentifier<AXIDType>(objectID) } : std::nullopt;
    }
};

#if PLATFORM(MAC)
using PlatformTextMarkerData = AXTextMarkerRef;
#elif PLATFORM(IOS_FAMILY)
using PlatformTextMarkerData = NSData *;
#endif

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXTextMarker);
class AXTextMarker {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXTextMarker, AXTextMarker);
    friend class AXTextMarkerRange;
    friend std::partial_ordering operator<=>(const AXTextMarker&, const AXTextMarker&);
public:
    // Constructors
    AXTextMarker(const VisiblePosition&, TextMarkerOrigin = TextMarkerOrigin::Unknown);
    AXTextMarker(const CharacterOffset&, TextMarkerOrigin = TextMarkerOrigin::Unknown);
    AXTextMarker(const TextMarkerData& data)
        : m_data(data)
    { }
    AXTextMarker(TextMarkerData&& data)
        : m_data(WTFMove(data))
    { }
#if PLATFORM(COCOA)
    AXTextMarker(PlatformTextMarkerData);
#endif
    AXTextMarker(std::optional<AXID> treeID, std::optional<AXID> objectID, unsigned offset, TextMarkerOrigin origin = TextMarkerOrigin::Unknown)
        : m_data({ treeID, objectID, offset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, offset, false, origin })
    { }
    AXTextMarker(const AXCoreObject& object, unsigned offset, TextMarkerOrigin origin = TextMarkerOrigin::Unknown)
        : m_data({ object.treeID(), object.objectID(), offset, Position::PositionIsOffsetInAnchor, Affinity::Downstream, 0, offset, false, origin })
    { }

    AXTextMarker() = default;

    operator bool() const { return !isNull(); }
    bool isEqual(const AXTextMarker& other) const { return m_data == other.m_data; }
    operator VisiblePosition() const;
    operator CharacterOffset() const;
    std::optional<BoundaryPoint> boundaryPoint() const;
    bool hasSameObjectAndOffset(const AXTextMarker&) const;

#if PLATFORM(COCOA)
    RetainPtr<PlatformTextMarkerData> platformData() const;
    operator PlatformTextMarkerData() const { return platformData().autorelease(); }
#endif

    std::optional<AXID> treeID() const { return m_data.axTreeID(); }
    std::optional<AXID> objectID() const { return m_data.axObjectID(); }
    unsigned offset() const { return m_data.offset; }
    bool isNull() const { return !treeID() || !objectID(); }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    // FIXME: Currently, the logic for serving text APIs off the main-thread requires isolated objects, but should eventually be refactored to work with AXCoreObjects.
    RefPtr<AXIsolatedObject> isolatedObject() const;
#endif
    RefPtr<AXCoreObject> object() const;
    bool isValid() const { return object(); }
    bool isIgnored() const { return m_data.ignored; }
    Affinity affinity() const { return m_data.affinity; }
    bool isDownstream() const { return affinity() == Affinity::Downstream; }
    void setAffinity(Affinity affinity) { m_data.affinity = affinity; }

    String debugDescription() const;
    TextMarkerOrigin origin() const { return m_data.origin; }

#if ENABLE(AX_THREAD_TEXT_APIS)
    AXTextMarker toTextRunMarker(std::optional<AXID> stopAtID = std::nullopt) const;
    // True if this marker points to an object with non-empty text runs.
    bool isInTextRun() const;
    AXTextMarker convertToDomOffset() const;
    void clampOffsetToLengthIfNeeded(unsigned) const;

    // Find the next or previous marker, optionally stopping at the given ID and returning an invalid marker.
    AXTextMarker findMarker(AXDirection, CoalesceObjectBreaks = CoalesceObjectBreaks::Yes, IgnoreBRs = IgnoreBRs::No, std::optional<AXID> = std::nullopt, ForceSingleOffsetMovement = ForceSingleOffsetMovement::No) const;

    // Starting from this text marker, these functions find a position representing the given boundary (start / end) and text unit type (e.g. line, word, paragraph).
    AXTextMarker findWord(AXDirection direction, AXTextUnitBoundary boundary) const
    {
        return findWordOrSentence(direction, /* findWord */ true, boundary);
    }
    AXTextMarker findSentence(AXDirection direction, AXTextUnitBoundary boundary) const
    {
        return findWordOrSentence(direction, /* findWord */ false, boundary);
    }
    AXTextMarker findWordOrSentence(AXDirection, bool findWord, AXTextUnitBoundary) const;
    AXTextMarker findLine(AXDirection, AXTextUnitBoundary, IncludeTrailingLineBreak = IncludeTrailingLineBreak::No, std::optional<AXID> stopAtID = std::nullopt) const;
    AXTextMarker findLine(AXDirection direction, AXTextUnitBoundary boundary, std::optional<AXID> stopAtID = std::nullopt) const
    {
        return findLine(direction, boundary, IncludeTrailingLineBreak::No, stopAtID);
    }
    AXTextMarker findParagraph(AXDirection, AXTextUnitBoundary) const;

    AXTextMarker previousLineStart(std::optional<AXID> stopAtID = std::nullopt) const { return findLine(AXDirection::Previous, AXTextUnitBoundary::Start, stopAtID); }
    AXTextMarker nextLineEnd(std::optional<AXID> stopAtID = std::nullopt) const { return findLine(AXDirection::Next, AXTextUnitBoundary::End, stopAtID); }
    AXTextMarker nextLineEnd(IncludeTrailingLineBreak includeTrailingLineBreak, std::optional<AXID> stopAtID = std::nullopt) const { return findLine(AXDirection::Next, AXTextUnitBoundary::End, includeTrailingLineBreak, stopAtID); }
    AXTextMarker nextWordStart() const { return findWord(AXDirection::Next, AXTextUnitBoundary::Start); }
    // The next end word boundary, not including the current position
    // Exception: unless the current text marker is at the end of a containing block, which
    // would return the current position.
    AXTextMarker nextWordEnd() const { return findWord(AXDirection::Next, AXTextUnitBoundary::End); }
    // The previous start word boundary, not including the current position
    // Exception: unless the current text marker is at the start of a containing block, which
    // would return the current position.
    AXTextMarker previousWordStart() const { return findWord(AXDirection::Previous, AXTextUnitBoundary::Start); }
    AXTextMarker previousWordEnd() const { return findWord(AXDirection::Previous, AXTextUnitBoundary::End); }
    AXTextMarker previousSentenceStart() const { return findSentence(AXDirection::Previous, AXTextUnitBoundary::Start); }
    AXTextMarker nextSentenceEnd() const { return findSentence(AXDirection::Next, AXTextUnitBoundary::End); }
    AXTextMarker previousParagraphStart() const;
    AXTextMarker nextParagraphEnd() const;

    // Creates a range for the line this marker points to.
    AXTextMarkerRange lineRange(LineRangeType, IncludeTrailingLineBreak = IncludeTrailingLineBreak::No) const;
    // This returns the full word range *immediately* to the right/left of a text marker. If the
    // text marker is in a word, this is that word range.
    AXTextMarkerRange wordRange(WordRangeType) const;
    // Creates a range for the sentence specified by the sentence range type;
    AXTextMarkerRange sentenceRange(SentenceRangeType) const;
    // Creates a range for the paragraph at the current marker.
    AXTextMarkerRange paragraphRange() const;
    // Returns a range pointing to the start and end positions that have the same text styles as `this`.
    AXTextMarkerRange rangeWithSameStyle() const;
    // Starting from this marker, return a text marker that is `offset` characters away.
    AXTextMarker nextMarkerFromOffset(unsigned, ForceSingleOffsetMovement = ForceSingleOffsetMovement::No, std::optional<AXID> stopAtID = std::nullopt) const;
    // Returns the number of intermediate text markers between this and the root.
    unsigned offsetFromRoot() const;
    // Starting from this marker, navigate to the last marker before the given AXID. Assumes `this`
    // is before the AXID in the AX tree (anything else is a bug). std::nullopt means we will find
    // the last marker on the entire webpage.
    AXTextMarker findLastBefore(std::optional<AXID>) const;
    AXTextMarker findLast() const { return findLastBefore(std::nullopt); }
    // The index of the line this text marker is on relative to the nearest editable ancestor (or start of the page if there are no editable ancestors).
    // Returns -1 if the line couldn't be computed (i.e. because `this` is invalid).
    int lineIndex() const;
    // After resolving this marker to a text-run marker, what line does the offset point to?
    AXTextRunLineID lineID() const;
    // Returns the line number for the character index within the descendants of this marker's object.
    // Returns -1 if the index is out of bounds, or this marker isn't valid.
    int lineNumberForIndex(unsigned) const;
    // The location and length of the line that is `lineIndex` lines away from the start of this marker.
    CharacterRange characterRangeForLine(unsigned lineIndex) const;
    // The AXTextMarkerRange of the line that is `lineIndex` lines away from the start of this marker.
    AXTextMarkerRange markerRangeForLineIndex(unsigned lineIndex) const;
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    friend bool operator==(const AXTextMarker& a, const AXTextMarker& b) { return a.isEqual(b); }

private:
#if ENABLE(AX_THREAD_TEXT_APIS)
    const AXTextRuns* runs() const;
    // Are we at the start or end of a line?
    bool atLineBoundaryForDirection(AXDirection) const;
    // Fast path to calcuate line boundary when a callsite already has the runs and runIndex available.
    bool atLineBoundaryForDirection(AXDirection, const AXTextRuns*, size_t) const;
    bool atLineStart() const { return atLineBoundaryForDirection(AXDirection::Previous); }
    bool atLineEnd() const { return atLineBoundaryForDirection(AXDirection::Next); }
    // True when two nodes are visually the same (i.e. on the boundary of an object)
    bool equivalentTextPosition(const AXTextMarker&) const;
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    TextMarkerData m_data;
};

class AXTextMarkerRange {
    WTF_MAKE_TZONE_ALLOCATED(AXTextMarkerRange);
    friend bool operator==(const AXTextMarkerRange&, const AXTextMarkerRange&) = default;
    friend bool operator<(const AXTextMarkerRange&, const AXTextMarkerRange&);
    friend bool operator>(const AXTextMarkerRange&, const AXTextMarkerRange&);
public:
    // Constructors.
    AXTextMarkerRange(const VisiblePositionRange&);
    AXTextMarkerRange(const VisibleSelection&);
    AXTextMarkerRange(const std::optional<SimpleRange>&);
    AXTextMarkerRange(const AXTextMarker&, const AXTextMarker&);
    AXTextMarkerRange(AXTextMarker&&, AXTextMarker&&);
#if PLATFORM(MAC)
    AXTextMarkerRange(AXTextMarkerRangeRef);
#endif
    AXTextMarkerRange(std::optional<AXID> treeID, std::optional<AXID> objectID, const CharacterRange&);
    AXTextMarkerRange(std::optional<AXID> treeID, std::optional<AXID> objectID, unsigned offset, unsigned length);
    AXTextMarkerRange() = default;

    operator bool() const { return m_start && m_end; }
    operator VisiblePositionRange() const;
    std::optional<SimpleRange> simpleRange() const;
    std::optional<CharacterRange> characterRange() const;

    std::optional<AXTextMarkerRange> intersectionWith(const AXTextMarkerRange&) const;

#if PLATFORM(MAC)
    RetainPtr<AXTextMarkerRangeRef> platformData() const;
    operator AXTextMarkerRangeRef() const { return platformData().autorelease(); }
#elif PLATFORM(IOS_FAMILY)
    // There is no iOS native type for a TextMarkerRange analogous to AXTextMarkerRangeRef on Mac.
    // Instead, an NSArray of 2 elements is used.
    AXTextMarkerRange(NSArray *);
    RetainPtr<NSArray> platformData() const;
#endif

#if PLATFORM(COCOA)
    std::optional<NSRange> nsRange() const;
#endif

    AXTextMarker start() const { return m_start; }
    AXTextMarker end() const { return m_end; }
    bool isCollapsed() const { return m_start.isEqual(m_end); }
    bool isConfinedTo(std::optional<AXID>) const;
    bool isConfined() const;
    String toString(IncludeListMarkerText = IncludeListMarkerText::Yes, IncludeImageAltText = IncludeImageAltText::No) const;

#if ENABLE(AX_THREAD_TEXT_APIS)
    // Returns the bounds (frame) of the text in this range relative to the viewport.
    // Analagous to AXCoreObject::relativeFrame().
    FloatRect viewportRelativeFrame() const;
    AXTextMarkerRange convertToDomOffsetRange() const;
#if PLATFORM(COCOA)
    RetainPtr<NSAttributedString> toAttributedString(AXCoreObject::SpellCheck) const;
#endif // PLATFORM(COCOA)
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    String debugDescription() const;
private:
    AXTextMarker m_start;
    AXTextMarker m_end;
};

inline AXTextMarkerRange::AXTextMarkerRange(std::optional<AXID> treeID, std::optional<AXID> objectID, const CharacterRange& range)
    : AXTextMarkerRange(treeID, objectID, range.location, range.location + range.length)
{ }

inline bool operator<(const AXTextMarkerRange& range1, const AXTextMarkerRange& range2)
{
    return range1.m_start < range2.m_start || range1.m_end < range2.m_end;
}

inline bool operator>(const AXTextMarkerRange& range1, const AXTextMarkerRange& range2)
{
    return range1.m_start > range2.m_start || range1.m_end > range2.m_end;
}

inline bool operator<=(const AXTextMarkerRange& range1, const AXTextMarkerRange& range2)
{
    return range1 == range2 || range1 < range2;
}

inline bool operator>=(const AXTextMarkerRange& range1, const AXTextMarkerRange& range2)
{
    return range1 == range2 || range1 > range2;
}

#if ENABLE(AX_THREAD_TEXT_APIS)
String listMarkerTextOnSameLine(const AXTextMarker&);
#endif

namespace Accessibility {

#if ENABLE(AX_THREAD_TEXT_APIS)
AXIsolatedObject* findObjectWithRuns(AXIsolatedObject& start, AXDirection direction, std::optional<AXID> stopAtID = std::nullopt, const std::function<void(AXIsolatedObject&)>& exitObject = [] (AXIsolatedObject&) { });
#endif // ENABLE(AX_THREAD_TEXT_APIS)

} // namespace Accessibility

} // namespace WebCore
