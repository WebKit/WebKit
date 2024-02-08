/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

#include "DictationContext.h"
#include "SimpleRange.h"
#include <variant>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <wtf/RetainPtr.h>
#endif

namespace WebCore {

// A range of a node within a document that is "marked", such as the range of a misspelled word.
// It optionally includes a description that could be displayed in the user interface.
class DocumentMarker : public CanMakeWeakPtr<DocumentMarker> {
public:
    enum class Type : uint32_t {
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
        // FIXME: iOS should share the same dictation mark system with the other platforms.
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
#if ENABLE(UNIFIED_TEXT_REPLACEMENT)
        UnifiedTextReplacement = 1 << 16,
#endif
    };

    static constexpr OptionSet<Type> allMarkers();

    struct DictationData {
        DictationContext context;
        String originalText;
    };
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    struct PlatformTextCheckingData {
        String key;
        String value;
    };
#endif

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)
    struct UnifiedTextReplacementData {
        enum class State: uint8_t {
            Pending,
            Committed,
            Reverted
        };

        String originalText;
        WTF::UUID uuid;
        State state { State::Pending };
    };
#endif

    using Data = std::variant<
        String
        , DictationData // DictationAlternatives
#if PLATFORM(IOS_FAMILY)
        , Vector<String> // DictationPhraseWithAlternatives
        , RetainPtr<id> // DictationResult
#endif
        , RefPtr<Node> // DraggedContent
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
        , PlatformTextCheckingData // PlatformTextChecking
#endif
#if ENABLE(UNIFIED_TEXT_REPLACEMENT)
        , UnifiedTextReplacementData // UnifiedTextReplacement
#endif
    >;

    DocumentMarker(Type, OffsetRange, Data&& = { });

    Type type() const { return m_type; }
    unsigned startOffset() const { return m_range.start; }
    unsigned endOffset() const { return m_range.end; }

    String description() const;

    const Data& data() const { return m_data; }
    void clearData() { m_data = String { }; }

    // Offset modifications are done by DocumentMarkerController.
    // Other classes should not call following setters.
    void setStartOffset(unsigned offset) { m_range.start = offset; }
    void setEndOffset(unsigned offset) { m_range.end = offset; }
    void shiftOffsets(int delta);

private:
    Type m_type;
    OffsetRange m_range;
    Data m_data;
};

constexpr auto DocumentMarker::allMarkers() -> OptionSet<Type>
{
    return {
        Type::AcceptedCandidate,
        Type::Autocorrected,
        Type::CorrectionIndicator,
        Type::DeletedAutocorrection,
        Type::DictationAlternatives,
        Type::DraggedContent,
        Type::Grammar,
        Type::RejectedCorrection,
        Type::Replacement,
        Type::SpellCheckingExemption,
        Type::Spelling,
        Type::TextMatch,
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        Type::TelephoneNumber,
#endif
#if PLATFORM(IOS_FAMILY)
        Type::DictationPhraseWithAlternatives,
        Type::DictationResult,
#endif
#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
        Type::PlatformTextChecking,
#endif
#if ENABLE(UNIFIED_TEXT_REPLACEMENT)
        Type::UnifiedTextReplacement,
#endif
    };
}

inline DocumentMarker::DocumentMarker(Type type, OffsetRange range, Data&& data)
    : m_type(type)
    , m_range(range)
    , m_data(WTFMove(data))
{
}

inline void DocumentMarker::shiftOffsets(int delta)
{
    m_range.start += delta;
    m_range.end += delta;
}

inline String DocumentMarker::description() const
{
    if (auto* description = std::get_if<String>(&m_data))
        return *description;

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)
    if (auto* data = std::get_if<DocumentMarker::UnifiedTextReplacementData>(&m_data))
        return makeString("('", data->originalText, "', state: ", enumToUnderlyingType(data->state), ")");
#endif

    return emptyString();
}

} // namespace WebCore
