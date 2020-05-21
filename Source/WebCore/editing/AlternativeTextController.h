/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AlternativeTextClient.h"
#include "DocumentMarker.h"
#include "Position.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/Variant.h>

namespace WebCore {

class CompositeEditCommand;
class Document;
class EditCommand;
class EditCommandComposition;
class EditorClient;
class Event;
class Range;
class TextCheckerClient;
class VisibleSelection;

struct DictationAlternative;
struct SimpleRange;
struct TextCheckingResult;

#if USE(AUTOCORRECTION_PANEL)
// These backslashes are for making style checker happy.
#define UNLESS_ENABLED(functionBody) \
;\

#else
#define UNLESS_ENABLED(functionBody) functionBody
#endif

class AlternativeTextController {
    WTF_MAKE_NONCOPYABLE(AlternativeTextController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AlternativeTextController(Document& document) UNLESS_ENABLED(: m_document(document) { })
    ~AlternativeTextController() UNLESS_ENABLED({ })

    void startAlternativeTextUITimer(AlternativeTextType) UNLESS_ENABLED({ })
    void stopAlternativeTextUITimer() UNLESS_ENABLED({ })

    void dismiss(ReasonForDismissingAlternativeText) UNLESS_ENABLED({ })
    void show(Range& rangeToReplace, const String& replacement) UNLESS_ENABLED({ UNUSED_PARAM(rangeToReplace); UNUSED_PARAM(replacement); })

    // Return true if correction was applied, false otherwise.
    bool applyAutocorrectionBeforeTypingIfAppropriate() UNLESS_ENABLED({ return false; })

    void respondToUnappliedSpellCorrection(const VisibleSelection&, const String& corrected, const String& correction) UNLESS_ENABLED({ UNUSED_PARAM(corrected); UNUSED_PARAM(correction); })
    void respondToAppliedEditing(CompositeEditCommand*) UNLESS_ENABLED({ })
    void respondToUnappliedEditing(EditCommandComposition*) UNLESS_ENABLED({ })
    void respondToChangedSelection(const VisibleSelection& oldSelection) UNLESS_ENABLED({ UNUSED_PARAM(oldSelection); })

    void stopPendingCorrection(const VisibleSelection& oldSelection) UNLESS_ENABLED({ UNUSED_PARAM(oldSelection); })
    void applyPendingCorrection(const VisibleSelection& selectionAfterTyping) UNLESS_ENABLED({ UNUSED_PARAM(selectionAfterTyping); })

    void handleAlternativeTextUIResult(const String& result) UNLESS_ENABLED({ UNUSED_PARAM(result); })
    void handleCancelOperation() UNLESS_ENABLED({ })

    bool hasPendingCorrection() const UNLESS_ENABLED({ return false; })
    bool isSpellingMarkerAllowed(Range& misspellingRange) const UNLESS_ENABLED({ UNUSED_PARAM(misspellingRange); return true; })
    bool isAutomaticSpellingCorrectionEnabled() UNLESS_ENABLED({ return false; })
    bool shouldRemoveMarkersUponEditing();

    void recordAutocorrectionResponse(AutocorrectionResponse, const String& replacedString, const SimpleRange& replacementRange) UNLESS_ENABLED({ UNUSED_PARAM(replacedString); UNUSED_PARAM(replacementRange); })
    void markReversed(Range& changedRange) UNLESS_ENABLED({ UNUSED_PARAM(changedRange); })
    void markCorrection(Range& replacedRange, const String& replacedString) UNLESS_ENABLED({ UNUSED_PARAM(replacedRange); UNUSED_PARAM(replacedString); })

    // This function returns false if the replacement should not be carried out.
    bool processMarkersOnTextToBeReplacedByResult(const TextCheckingResult&, Range& rangeToBeReplaced, const String& stringToBeReplaced) UNLESS_ENABLED({ UNUSED_PARAM(rangeToBeReplaced); UNUSED_PARAM(stringToBeReplaced); return true; });
    void deletedAutocorrectionAtPosition(const Position&, const String& originalString) UNLESS_ENABLED({ UNUSED_PARAM(originalString); })

    bool insertDictatedText(const String&, const Vector<DictationAlternative>&, Event*);
    void removeDictationAlternativesForMarker(const DocumentMarker&);
    Vector<String> dictationAlternativesForMarker(const DocumentMarker&);
    void applyDictationAlternative(const String& alternativeString);

private:
#if USE(AUTOCORRECTION_PANEL)
    using AutocorrectionReplacement = String;

    struct AlternativeTextInfo {
        RefPtr<Range> rangeWithAlternative;
        bool isActive;
        AlternativeTextType type;
        String originalText;
        Variant<AutocorrectionReplacement, DictationContext> details;
    };

    String dismissSoon(ReasonForDismissingAlternativeText);
    void timerFired();
    void recordSpellcheckerResponseForModifiedCorrection(Range& rangeOfCorrection, const String& corrected, const String& correction);

    bool shouldStartTimerFor(const DocumentMarker&, int endOffset) const;
    bool respondToMarkerAtEndOfWord(const DocumentMarker&, const Position& endOfWordPosition);

    EditorClient* editorClient();
    
    TextCheckerClient* textChecker();
    FloatRect rootViewRectForRange(const SimpleRange&) const;
    void markPrecedingWhitespaceForDeletedAutocorrectionAfterCommand(EditCommand*);

    Timer m_timer;
    RefPtr<Range> m_rangeWithAlternative;
    bool m_isActive;
    bool m_isDismissedByEditing;
    AlternativeTextType m_type;
    String m_originalText;
    Variant<AutocorrectionReplacement, DictationContext> m_details;

    String m_originalStringForLastDeletedAutocorrection;
    Position m_positionForLastDeletedAutocorrection;
#endif
#if USE(DICTATION_ALTERNATIVES) || USE(AUTOCORRECTION_PANEL)
    String markerDescriptionForAppliedAlternativeText(AlternativeTextType, DocumentMarker::MarkerType);
    void applyAlternativeTextToRange(const Range&, const String&, AlternativeTextType, OptionSet<DocumentMarker::MarkerType>);
    AlternativeTextClient* alternativeTextClient();
#endif

    Document& m_document;
};

#undef UNLESS_ENABLED

inline bool AlternativeTextController::shouldRemoveMarkersUponEditing()
{
#if USE(MARKER_REMOVAL_UPON_EDITING)
    return true;
#else
    return false;
#endif
}

} // namespace WebCore
