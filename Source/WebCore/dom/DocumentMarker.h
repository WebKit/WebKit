/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "Node.h"

#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <wtf/RetainPtr.h>
typedef struct objc_object *id;
#endif

namespace WebCore {

// A range of a node within a document that is "marked", such as the range of a misspelled word.
// It optionally includes a description that could be displayed in the user interface.
// It also optionally includes a flag specifying whether the match is active, which is ignored
// for all types other than type TextMatch.
class DocumentMarker {
public:
    enum MarkerType {
        Spelling = 1 << 0,
        Grammar = 1 << 1,
        TextMatch = 1 << 2,
        // Text has been modified by spell correction, reversion of spell correction or other type of substitution. 
        // On some platforms, this prevents the text from being autocorrected again. On post Snow Leopard Mac OS X, 
        // if a Replacement marker contains non-empty description, a reversion UI will be shown.
        Replacement = 1 << 3,
        // Renderer needs to add underline indicating that the text has been modified by spell
        // correction. Text with Replacement marker doesn't necessarily has CorrectionIndicator
        // marker. For instance, after some text has been corrected, it will have both Replacement
        // and CorrectionIndicator. However, if user further modifies such text, we would remove
        // CorrectionIndicator marker, but retain Replacement marker.
        CorrectionIndicator = 1 << 4,
        // Correction suggestion has been offered, but got rejected by user.
        RejectedCorrection = 1 << 5,
        // Text has been modified by autocorrection. The description of this marker is the original text before autocorrection.
        Autocorrected = 1 << 6,
        // On some platforms, this prevents the text from being spellchecked again.
        SpellCheckingExemption = 1 << 7,
        // This marker indicates user has deleted an autocorrection starting at the end of the
        // range that bears this marker. In some platforms, if the user later inserts the same original
        // word again at this position, it will not be autocorrected again. The description of this
        // marker is the original word before autocorrection was applied.
        DeletedAutocorrection = 1 << 8,
        // This marker indicates that the range of text spanned by the marker is entered by voice dictation,
        // and it has alternative text.
        DictationAlternatives = 1 << 9,
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        TelephoneNumber = 1 << 10,
#endif
#if PLATFORM(IOS_FAMILY)
        // FIXME: iOS should share the same dictation mark system with the other platforms <rdar://problem/9431249>.
        DictationPhraseWithAlternatives = 1 << 11,
        DictationResult = 1 << 12,
#endif
        // This marker indicates that the user has selected a text candidate.
        AcceptedCandidate = 1 << 13,
        // This marker indicates that the user has initiated a drag with this content.
        DraggedContent = 1 << 14,
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
        // This marker maintains state for the platform text checker.
        PlatformTextChecking = 1 << 15,
#endif
    };

    static constexpr OptionSet<MarkerType> allMarkers();

    using IsActiveMatchData = bool;
    using DescriptionData = String;
    struct DictationData {
        uint64_t context;
        String originalText;
    };
    struct DictationAlternativesData {
#if PLATFORM(IOS_FAMILY)
        Vector<String> alternatives;
        RetainPtr<id> metadata;
#endif
    };
    struct DraggedContentData {
        RefPtr<Node> targetNode;
    };
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    struct PlatformTextCheckingData {
        String key;
        String value;
    };
#endif
    using Data = Variant<IsActiveMatchData, DescriptionData, DictationData, DictationAlternativesData, DraggedContentData
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    , PlatformTextCheckingData
#endif
    >;

    DocumentMarker(unsigned startOffset, unsigned endOffset, bool isActiveMatch);
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset, const String& description = String());
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset, Data&&);
#if PLATFORM(IOS_FAMILY)
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset, const String& description, const Vector<String>& alternatives, RetainPtr<id> metadata);
#endif

    MarkerType type() const { return m_type; }
    unsigned startOffset() const { return m_startOffset; }
    unsigned endOffset() const { return m_endOffset; }

    const String& description() const;

    bool isActiveMatch() const;
    void setActiveMatch(bool);

    const Data& data() const { return m_data; }
    void clearData() { m_data = false; }

    // Offset modifications are done by DocumentMarkerController.
    // Other classes should not call following setters.
    void setStartOffset(unsigned offset) { m_startOffset = offset; }
    void setEndOffset(unsigned offset) { m_endOffset = offset; }
    void shiftOffsets(int delta);

#if PLATFORM(IOS_FAMILY)
    bool isDictation() const;
    const Vector<String>& alternatives() const;
    void setAlternative(const String&, size_t index);
    id metadata() const;
    void setMetadata(id);
#endif

private:
    MarkerType m_type;
    unsigned m_startOffset;
    unsigned m_endOffset;
    Data m_data;
};

constexpr auto DocumentMarker::allMarkers() -> OptionSet<MarkerType>
{
    return {
        AcceptedCandidate,
        Autocorrected,
        CorrectionIndicator,
        DeletedAutocorrection,
        DictationAlternatives,
        DraggedContent,
        Grammar,
        RejectedCorrection,
        Replacement,
        SpellCheckingExemption,
        Spelling,
        TextMatch,
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        TelephoneNumber,
#endif
#if PLATFORM(IOS_FAMILY)
        DictationPhraseWithAlternatives,
        DictationResult,
#endif
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
        PlatformTextChecking
#endif
    };
}

inline DocumentMarker::DocumentMarker(unsigned startOffset, unsigned endOffset, bool isActiveMatch)
    : m_type(TextMatch)
    , m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_data(isActiveMatch)
{
}

inline DocumentMarker::DocumentMarker(MarkerType type, unsigned startOffset, unsigned endOffset, const String& description)
    : m_type(type)
    , m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_data(description)
{
}

inline DocumentMarker::DocumentMarker(MarkerType type, unsigned startOffset, unsigned endOffset, Data&& data)
    : m_type(type)
    , m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_data(WTFMove(data))
{
}

inline void DocumentMarker::shiftOffsets(int delta)
{
    m_startOffset += delta;
    m_endOffset += delta;
}

inline const String& DocumentMarker::description() const
{
    return WTF::holds_alternative<String>(m_data) ? WTF::get<String>(m_data) : emptyString();
}

inline bool DocumentMarker::isActiveMatch() const
{
    return WTF::holds_alternative<bool>(m_data) && WTF::get<bool>(m_data);
}

inline void DocumentMarker::setActiveMatch(bool isActiveMatch)
{
    ASSERT(m_type == TextMatch);
    m_data = isActiveMatch;
}

#if PLATFORM(IOS_FAMILY)

// FIXME: iOS should share the same dictation mark system with the other platforms <rdar://problem/9431249>.

inline DocumentMarker::DocumentMarker(MarkerType type, unsigned startOffset, unsigned endOffset, const String&, const Vector<String>& alternatives, RetainPtr<id> metadata)
    : m_type(type)
    , m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_data(DictationAlternativesData { alternatives, metadata })
{
    ASSERT(isDictation());
}

inline bool DocumentMarker::isDictation() const
{
    return m_type == DictationPhraseWithAlternatives || m_type == DictationResult;
}

inline const Vector<String>& DocumentMarker::alternatives() const
{
    return WTF::get<DictationAlternativesData>(m_data).alternatives;
}

inline void DocumentMarker::setAlternative(const String& alternative, size_t index)
{
    WTF::get<DictationAlternativesData>(m_data).alternatives[index] = alternative;
}

inline id DocumentMarker::metadata() const
{
    return WTF::get<DictationAlternativesData>(m_data).metadata.get();
}

inline void DocumentMarker::setMetadata(id metadata)
{
    WTF::get<DictationAlternativesData>(m_data).metadata = metadata;
}

#endif

} // namespace WebCore
