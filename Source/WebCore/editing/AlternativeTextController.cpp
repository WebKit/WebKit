/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "AlternativeTextController.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "Range.h"
#include "RenderedDocumentMarker.h"
#include "SpellingCorrectionCommand.h"
#include "TextCheckerClient.h"
#include "TextCheckingHelper.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "markup.h"

namespace WebCore {


#if USE(DICTATION_ALTERNATIVES) || USE(AUTOCORRECTION_PANEL)

constexpr OptionSet<DocumentMarker::MarkerType> markerTypesForAppliedDictationAlternative()
{
    return DocumentMarker::SpellCheckingExemption;
}

#endif

#if USE(AUTOCORRECTION_PANEL)

static inline OptionSet<DocumentMarker::MarkerType> markerTypesForAutocorrection()
{
    return { DocumentMarker::Autocorrected, DocumentMarker::CorrectionIndicator, DocumentMarker::Replacement, DocumentMarker::SpellCheckingExemption };
}

static inline OptionSet<DocumentMarker::MarkerType> markerTypesForReplacement()
{
    return { DocumentMarker::Replacement, DocumentMarker::SpellCheckingExemption };
}

static bool markersHaveIdenticalDescription(const Vector<RenderedDocumentMarker*>& markers)
{
    if (markers.isEmpty())
        return true;

    const String& description = markers[0]->description();
    for (size_t i = 1; i < markers.size(); ++i) {
        if (description != markers[i]->description())
            return false;
    }
    return true;
}

AlternativeTextController::AlternativeTextController(Document& document)
    : m_timer(*this, &AlternativeTextController::timerFired)
    , m_document(document)
{
}

AlternativeTextController::~AlternativeTextController()
{
    dismiss(ReasonForDismissingAlternativeTextIgnored);
}

void AlternativeTextController::startAlternativeTextUITimer(AlternativeTextType type)
{
    const Seconds correctionPanelTimerInterval { 300_ms };
    if (!isAutomaticSpellingCorrectionEnabled())
        return;

    // If type is PanelTypeReversion, then the new range has been set. So we shouldn't clear it.
    if (type == AlternativeTextTypeCorrection)
        m_rangeWithAlternative = WTF::nullopt;
    m_type = type;
    m_timer.startOneShot(correctionPanelTimerInterval);
}

void AlternativeTextController::stopAlternativeTextUITimer()
{
    m_timer.stop();
    m_rangeWithAlternative = WTF::nullopt;
}

void AlternativeTextController::stopPendingCorrection(const VisibleSelection& oldSelection)
{
    // Make sure there's no pending autocorrection before we call markMisspellingsAndBadGrammar() below.
    VisibleSelection currentSelection(m_document.selection().selection());
    if (currentSelection == oldSelection)
        return;

    stopAlternativeTextUITimer();
    dismiss(ReasonForDismissingAlternativeTextIgnored);
}

void AlternativeTextController::applyPendingCorrection(const VisibleSelection& selectionAfterTyping)
{
    // Apply pending autocorrection before next round of spell checking.
    bool doApplyCorrection = true;
    VisiblePosition startOfSelection = selectionAfterTyping.visibleStart();
    VisibleSelection currentWord = VisibleSelection(startOfWord(startOfSelection, LeftWordIfOnBoundary), endOfWord(startOfSelection, RightWordIfOnBoundary));
    if (currentWord.visibleEnd() == startOfSelection) {
        if (auto wordRange = currentWord.firstRange()) {
            String wordText = plainText(*wordRange);
            if (!wordText.isEmpty() && isAmbiguousBoundaryCharacter(wordText[wordText.length() - 1]))
                doApplyCorrection = false;
        }
    }
    if (doApplyCorrection)
        handleAlternativeTextUIResult(dismissSoon(ReasonForDismissingAlternativeTextAccepted)); 
    else
        m_rangeWithAlternative = WTF::nullopt;
}

bool AlternativeTextController::hasPendingCorrection() const
{
    return m_rangeWithAlternative.hasValue();
}

bool AlternativeTextController::isSpellingMarkerAllowed(const SimpleRange& misspellingRange) const
{
    return !m_document.markers().hasMarkers(misspellingRange, DocumentMarker::SpellCheckingExemption);
}

void AlternativeTextController::show(const SimpleRange& rangeToReplace, const String& replacement)
{
    auto boundingBox = rootViewRectForRange(rangeToReplace);
    if (boundingBox.isEmpty())
        return;
    m_originalText = plainText(rangeToReplace);
    m_rangeWithAlternative = rangeToReplace;
    m_details = replacement;
    m_isActive = true;
    if (AlternativeTextClient* client = alternativeTextClient())
        client->showCorrectionAlternative(m_type, boundingBox, m_originalText, replacement, { });
}

void AlternativeTextController::handleCancelOperation()
{
    if (!m_isActive)
        return;
    m_isActive = false;
    dismiss(ReasonForDismissingAlternativeTextCancelled);
}

void AlternativeTextController::dismiss(ReasonForDismissingAlternativeText reasonForDismissing)
{
    if (!m_isActive)
        return;
    m_isActive = false;
    m_isDismissedByEditing = true;
    if (AlternativeTextClient* client = alternativeTextClient())
        client->dismissAlternative(reasonForDismissing);
}

String AlternativeTextController::dismissSoon(ReasonForDismissingAlternativeText reasonForDismissing)
{
    if (!m_isActive)
        return String();
    m_isActive = false;
    m_isDismissedByEditing = true;
    if (AlternativeTextClient* client = alternativeTextClient())
        return client->dismissAlternativeSoon(reasonForDismissing);
    return String();
}

bool AlternativeTextController::applyAutocorrectionBeforeTypingIfAppropriate()
{
    if (!m_rangeWithAlternative || !m_isActive)
        return false;

    if (m_type != AlternativeTextTypeCorrection)
        return false;

    Position caretPosition = m_document.selection().selection().start();

    if (makeDeprecatedLegacyPosition(m_rangeWithAlternative->end) == caretPosition) {
        handleAlternativeTextUIResult(dismissSoon(ReasonForDismissingAlternativeTextAccepted));
        return true;
    } 
    
    // Pending correction should always be where caret is. But in case this is not always true, we still want to dismiss the panel without accepting the correction.
    ASSERT(makeDeprecatedLegacyPosition(m_rangeWithAlternative->end) == caretPosition);
    dismiss(ReasonForDismissingAlternativeTextIgnored);
    return false;
}

void AlternativeTextController::respondToUnappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction)
{
    if (auto client = alternativeTextClient())
        client->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, corrected, correction);

    RefPtr<Frame> protector(m_document.frame());
    m_document.updateLayout();

    m_document.selection().setSelection(selectionOfCorrected, FrameSelection::defaultSetSelectionOptions() | FrameSelection::SpellCorrectionTriggered);
    auto range = m_document.selection().selection().firstRange();
    if (!range)
        return;
    removeMarkers(*range, OptionSet<DocumentMarker::MarkerType> { DocumentMarker::Spelling, DocumentMarker::Autocorrected }, RemovePartiallyOverlappingMarker::Yes);
    addMarker(*range, DocumentMarker::Replacement);
    addMarker(*range, DocumentMarker::SpellCheckingExemption);
}

void AlternativeTextController::timerFired()
{
    m_isDismissedByEditing = false;
    switch (m_type) {
    case AlternativeTextTypeCorrection: {
        VisibleSelection selection(m_document.selection().selection());
        VisiblePosition start(selection.start(), selection.affinity());
        VisiblePosition p = startOfWord(start, LeftWordIfOnBoundary);
        VisibleSelection adjacentWords = VisibleSelection(p, start);
        auto adjacentWordRange = adjacentWords.toNormalizedRange();
        m_document.editor().markAllMisspellingsAndBadGrammarInRanges({ TextCheckingType::Spelling, TextCheckingType::Replacement, TextCheckingType::ShowCorrectionPanel }, adjacentWordRange, adjacentWordRange, WTF::nullopt);
    }
        break;
    case AlternativeTextTypeReversion: {
        if (!m_rangeWithAlternative)
            break;
        String replacementString = WTF::get<AutocorrectionReplacement>(m_details);
        if (replacementString.isEmpty())
            break;
        m_isActive = true;
        m_originalText = plainText(*m_rangeWithAlternative);
        auto boundingBox = rootViewRectForRange(*m_rangeWithAlternative);
        if (!boundingBox.isEmpty()) {
            if (AlternativeTextClient* client = alternativeTextClient())
                client->showCorrectionAlternative(m_type, boundingBox, m_originalText, replacementString, { });
        }
    }
        break;
    case AlternativeTextTypeSpellingSuggestions: {
        if (!m_rangeWithAlternative || plainText(*m_rangeWithAlternative) != m_originalText)
            break;
        String paragraphText = plainText(TextCheckingParagraph(*m_rangeWithAlternative).paragraphRange());
        Vector<String> suggestions;
        textChecker()->getGuessesForWord(m_originalText, paragraphText, m_document.selection().selection(), suggestions);
        if (suggestions.isEmpty()) {
            m_rangeWithAlternative = WTF::nullopt;
            break;
        }
        String topSuggestion = suggestions.first();
        suggestions.remove(0);
        m_isActive = true;
        auto boundingBox = rootViewRectForRange(*m_rangeWithAlternative);
        if (!boundingBox.isEmpty()) {
            if (AlternativeTextClient* client = alternativeTextClient())
                client->showCorrectionAlternative(m_type, boundingBox, m_originalText, topSuggestion, suggestions);
        }
    }
        break;
    case AlternativeTextTypeDictationAlternatives:
    {
#if USE(DICTATION_ALTERNATIVES)
        if (!m_rangeWithAlternative)
            return;
        auto dictationContext = WTF::get<DictationContext>(m_details);
        if (!dictationContext)
            return;
        auto boundingBox = rootViewRectForRange(*m_rangeWithAlternative);
        m_isActive = true;
        if (!boundingBox.isEmpty()) {
            if (auto client = alternativeTextClient())
                client->showDictationAlternativeUI(boundingBox, dictationContext);
        }
#endif
    }
        break;
    }
}

void AlternativeTextController::handleAlternativeTextUIResult(const String& result)
{
    if (!m_rangeWithAlternative || &m_document != &m_rangeWithAlternative->start.document())
        return;

    String currentWord = plainText(*m_rangeWithAlternative);
    // Check to see if the word we are about to correct has been changed between timer firing and callback being triggered.
    if (currentWord != m_originalText)
        return;

    m_isActive = false;

    switch (m_type) {
    case AlternativeTextTypeCorrection:
        if (result.length())
            applyAlternativeTextToRange(*m_rangeWithAlternative, result, m_type, markerTypesForAutocorrection());
        else if (!m_isDismissedByEditing)
            addMarker(*m_rangeWithAlternative, DocumentMarker::RejectedCorrection, m_originalText);
        break;
    case AlternativeTextTypeReversion:
    case AlternativeTextTypeSpellingSuggestions:
        if (result.length())
            applyAlternativeTextToRange(*m_rangeWithAlternative, result, m_type, markerTypesForReplacement());
        break;
    case AlternativeTextTypeDictationAlternatives:
        if (result.length())
            applyAlternativeTextToRange(*m_rangeWithAlternative, result, m_type, markerTypesForAppliedDictationAlternative());
        break;
    }

    m_rangeWithAlternative = WTF::nullopt;
}

bool AlternativeTextController::isAutomaticSpellingCorrectionEnabled()
{
    if (!editorClient() || !editorClient()->isAutomaticSpellingCorrectionEnabled())
        return false;

#if ENABLE(AUTOCORRECT)
    auto position = m_document.selection().selection().start();
    if (auto editableRoot = position.rootEditableElement()) {
        if (is<HTMLElement>(editableRoot) && !downcast<HTMLElement>(*editableRoot).shouldAutocorrect())
            return false;
    }

    if (auto control = enclosingTextFormControl(position)) {
        if (!control->shouldAutocorrect())
            return false;
    }
#endif

    return true;
}

FloatRect AlternativeTextController::rootViewRectForRange(const SimpleRange& range) const
{
    auto* view = m_document.view();
    if (!view)
        return { };
    return view->contentsToRootView(unitedBoundingBoxes(RenderObject::absoluteTextQuads(range)));
}        

void AlternativeTextController::respondToChangedSelection(const VisibleSelection& oldSelection)
{
    VisibleSelection currentSelection(m_document.selection().selection());
    // When user moves caret to the end of autocorrected word and pauses, we show the panel
    // containing the original pre-correction word so that user can quickly revert the
    // undesired autocorrection. Here, we start correction panel timer once we confirm that
    // the new caret position is at the end of a word.
    if (!currentSelection.isCaret() || currentSelection == oldSelection || !currentSelection.isContentEditable())
        return;

    VisiblePosition selectionPosition = currentSelection.start();
    
    // Creating a Visible position triggers a layout and there is no
    // guarantee that the selection is still valid.
    if (selectionPosition.isNull())
        return;
    
    VisiblePosition endPositionOfWord = endOfWord(selectionPosition, LeftWordIfOnBoundary);
    if (selectionPosition != endPositionOfWord)
        return;

    Position position = endPositionOfWord.deepEquivalent();
    if (position.anchorType() != Position::PositionIsOffsetInAnchor)
        return;

    Node* node = position.containerNode();
    ASSERT(node);
    for (auto* marker : node->document().markers().markersFor(*node)) {
        ASSERT(marker);
        if (respondToMarkerAtEndOfWord(*marker, position))
            break;
    }
}

void AlternativeTextController::respondToAppliedEditing(CompositeEditCommand* command)
{
    if (command->isTopLevelCommand() && !command->shouldRetainAutocorrectionIndicator())
        m_document.markers().removeMarkers(DocumentMarker::CorrectionIndicator);

    markPrecedingWhitespaceForDeletedAutocorrectionAfterCommand(command);
    m_originalStringForLastDeletedAutocorrection = String();

    dismiss(ReasonForDismissingAlternativeTextIgnored);
}

void AlternativeTextController::respondToUnappliedEditing(EditCommandComposition* command)
{
    if (!command->wasCreateLinkCommand())
        return;
    auto range = command->startingSelection().firstRange();
    if (!range)
        return;
    addMarker(*range, DocumentMarker::Replacement);
    addMarker(*range, DocumentMarker::SpellCheckingExemption);
}

EditorClient* AlternativeTextController::editorClient()
{
    return m_document.page() ? &m_document.page()->editorClient() : nullptr;
}

TextCheckerClient* AlternativeTextController::textChecker()
{
    if (EditorClient* owner = editorClient())
        return owner->textChecker();
    return nullptr;
}

void AlternativeTextController::recordAutocorrectionResponse(AutocorrectionResponse response, const String& replacedString, const SimpleRange& replacementRange)
{
    if (auto client = alternativeTextClient())
        client->recordAutocorrectionResponse(response, replacedString, plainText(replacementRange));
}

void AlternativeTextController::markReversed(const SimpleRange& changedRange)
{
    removeMarkers(changedRange, DocumentMarker::Autocorrected, RemovePartiallyOverlappingMarker::Yes);
    addMarker(changedRange, DocumentMarker::SpellCheckingExemption);
}

void AlternativeTextController::markCorrection(const SimpleRange& replacedRange, const String& replacedString)
{
    for (auto markerType : markerTypesForAutocorrection()) {
        if (markerType == DocumentMarker::Replacement || markerType == DocumentMarker::Autocorrected)
            addMarker(replacedRange, markerType, replacedString);
        else
            addMarker(replacedRange, markerType);
    }
}

void AlternativeTextController::recordSpellcheckerResponseForModifiedCorrection(const SimpleRange& rangeOfCorrection, const String& corrected, const String& correction)
{
    DocumentMarkerController& markers = rangeOfCorrection.startContainer().document().markers();
    Vector<RenderedDocumentMarker*> correctedOnceMarkers = markers.markersInRange(rangeOfCorrection, DocumentMarker::Autocorrected);
    if (correctedOnceMarkers.isEmpty())
        return;

    if (AlternativeTextClient* client = alternativeTextClient()) {
        // Spelling corrected text has been edited. We need to determine whether user has reverted it to original text or
        // edited it to something else, and notify spellchecker accordingly.
        if (markersHaveIdenticalDescription(correctedOnceMarkers) && correctedOnceMarkers[0]->description() == corrected)
            client->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, corrected, correction);
        else
            client->recordAutocorrectionResponse(AutocorrectionResponse::Edited, corrected, correction);
    }

    removeMarkers(rangeOfCorrection, DocumentMarker::Autocorrected, RemovePartiallyOverlappingMarker::Yes);
}

void AlternativeTextController::deletedAutocorrectionAtPosition(const Position& position, const String& originalString)
{
    m_originalStringForLastDeletedAutocorrection = originalString;
    m_positionForLastDeletedAutocorrection = position;
}

void AlternativeTextController::markPrecedingWhitespaceForDeletedAutocorrectionAfterCommand(EditCommand* command)
{
    Position endOfSelection = command->endingSelection().end();
    if (endOfSelection != m_positionForLastDeletedAutocorrection)
        return;

    Position precedingCharacterPosition = endOfSelection.previous();
    if (endOfSelection == precedingCharacterPosition)
        return;

    auto precedingCharacterRange = makeSimpleRange(precedingCharacterPosition, endOfSelection);
    if (!precedingCharacterRange)
        return;
    String string = plainText(*precedingCharacterRange);
    if (string.isEmpty() || !deprecatedIsEditingWhitespace(string[string.length() - 1]))
        return;

    // Mark this whitespace to indicate we have deleted an autocorrection following this
    // whitespace. So if the user types the same original word again at this position, we
    // won't autocorrect it again.
    addMarker(*precedingCharacterRange, DocumentMarker::DeletedAutocorrection, m_originalStringForLastDeletedAutocorrection);
}

bool AlternativeTextController::processMarkersOnTextToBeReplacedByResult(const TextCheckingResult& result, const SimpleRange& rangeWithAlternative, const String& stringToBeReplaced)
{
    auto& markers = m_document.markers();
    if (markers.hasMarkers(rangeWithAlternative, DocumentMarker::Replacement)) {
        if (result.type == TextCheckingType::Correction)
            recordSpellcheckerResponseForModifiedCorrection(rangeWithAlternative, stringToBeReplaced, result.replacement);
        return false;
    }

    if (markers.hasMarkers(rangeWithAlternative, DocumentMarker::RejectedCorrection))
        return false;

    if (markers.hasMarkers(rangeWithAlternative, DocumentMarker::AcceptedCandidate))
        return false;

    auto precedingCharacterRange = makeSimpleRange(makeDeprecatedLegacyPosition(rangeWithAlternative.start).previous(), rangeWithAlternative.start);
    if (!precedingCharacterRange)
        return false;

    for (auto& marker : markers.markersInRange(*precedingCharacterRange, DocumentMarker::DeletedAutocorrection)) {
        if (marker->description() == stringToBeReplaced)
            return false;
    }

    return true;
}

bool AlternativeTextController::shouldStartTimerFor(const WebCore::DocumentMarker &marker, int endOffset) const
{
    return (((marker.type() == DocumentMarker::Replacement && !marker.description().isNull()) || marker.type() == DocumentMarker::Spelling || marker.type() == DocumentMarker::DictationAlternatives) && static_cast<int>(marker.endOffset()) == endOffset);
}

bool AlternativeTextController::respondToMarkerAtEndOfWord(const DocumentMarker& marker, const Position& endOfWordPosition)
{
    if (!shouldStartTimerFor(marker, endOfWordPosition.offsetInContainerNode()))
        return false;
    Node* node = endOfWordPosition.containerNode();
    auto wordRange = makeSimpleRange(*node, marker);
    String currentWord = plainText(wordRange);
    if (!currentWord.length())
        return false;
    m_originalText = currentWord;
    switch (marker.type()) {
    case DocumentMarker::Spelling:
        m_rangeWithAlternative = WTFMove(wordRange);
        m_details = emptyString();
        startAlternativeTextUITimer(AlternativeTextTypeSpellingSuggestions);
        break;
    case DocumentMarker::Replacement:
        m_rangeWithAlternative = WTFMove(wordRange);
        m_details = marker.description();
        startAlternativeTextUITimer(AlternativeTextTypeReversion);
        break;
    case DocumentMarker::DictationAlternatives: {
        auto& markerData = WTF::get<DocumentMarker::DictationData>(marker.data());
        if (currentWord != markerData.originalText)
            return false;
        m_rangeWithAlternative = WTFMove(wordRange);
        m_details = markerData.context;
        startAlternativeTextUITimer(AlternativeTextTypeDictationAlternatives);
    }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return true;
}

#endif // USE(AUTOCORRECTION_PANEL)

#if USE(DICTATION_ALTERNATIVES) || USE(AUTOCORRECTION_PANEL)

AlternativeTextClient* AlternativeTextController::alternativeTextClient()
{
    return m_document.frame() && m_document.page() ? m_document.page()->alternativeTextClient() : nullptr;
}

String AlternativeTextController::markerDescriptionForAppliedAlternativeText(AlternativeTextType alternativeTextType, DocumentMarker::MarkerType markerType)
{
#if USE(AUTOCORRECTION_PANEL)
    if (alternativeTextType != AlternativeTextTypeReversion && alternativeTextType != AlternativeTextTypeDictationAlternatives && (markerType == DocumentMarker::Replacement || markerType == DocumentMarker::Autocorrected))
        return m_originalText;
#else
    UNUSED_PARAM(alternativeTextType);
    UNUSED_PARAM(markerType);
#endif
    return emptyString();
}

void AlternativeTextController::applyAlternativeTextToRange(const SimpleRange& range, const String& alternative, AlternativeTextType alternativeType, OptionSet<DocumentMarker::MarkerType> markerTypesToAdd)
{
    // After we replace the word at range rangeWithAlternative, we need to add markers to that range.
    // So before we carry out the replacement,store the start position relative to the start position
    // of the containing paragraph.

    // Take note of the location of autocorrection so that we can add marker after the replacement took place.
    auto paragraphStart = makeBoundaryPoint(startOfParagraph(makeDeprecatedLegacyPosition(range.start)));
    if (!paragraphStart)
        return;
    auto treeScopeRoot = makeRef(range.start.container->treeScope().rootNode());
    auto treeScopeStart = BoundaryPoint { treeScopeRoot.get(), 0 };
    auto correctionOffsetInParagraph = characterCount({ *paragraphStart, range.start });
    auto paragraphOffsetInTreeScope = characterCount({ treeScopeStart, *paragraphStart });

    SpellingCorrectionCommand::create(range, alternative)->apply();

    // Recalculate pragraphRangeContainingCorrection, since SpellingCorrectionCommand modified the DOM, such that the original paragraphRangeContainingCorrection is no longer valid. Radar: 10305315 Bugzilla: 89526
    auto updatedParagraphStartContainingCorrection = resolveCharacterLocation(makeRangeSelectingNodeContents(treeScopeRoot), paragraphOffsetInTreeScope);
    auto updatedParagraphEndContainingCorrection = makeBoundaryPoint(m_document.selection().selection().start());
    if (!updatedParagraphEndContainingCorrection)
        return;
    auto replacementRange = resolveCharacterRange({ updatedParagraphStartContainingCorrection, *updatedParagraphEndContainingCorrection }, CharacterRange(correctionOffsetInParagraph, alternative.length()));

    // Check to see if replacement succeeded.
    if (plainText(replacementRange) != alternative)
        return;

    for (auto markerType : markerTypesToAdd)
        addMarker(replacementRange, markerType, markerDescriptionForAppliedAlternativeText(alternativeType, markerType));
}

#endif

bool AlternativeTextController::insertDictatedText(const String& text, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent)
{
    EventTarget* target;
    if (triggeringEvent)
        target = triggeringEvent->target();
    else
        target = eventTargetElementForDocument(&m_document);
    if (!target)
        return false;

    auto event = TextEvent::createForDictation(&m_document.frame()->windowProxy(), text, dictationAlternatives);
    event->setUnderlyingEvent(triggeringEvent);

    target->dispatchEvent(event);
    return event->defaultHandled();
}

void AlternativeTextController::removeDictationAlternativesForMarker(const DocumentMarker& marker)
{
#if USE(DICTATION_ALTERNATIVES)
    if (auto* client = alternativeTextClient())
        client->removeDictationAlternatives(WTF::get<DocumentMarker::DictationData>(marker.data()).context);
#else
    UNUSED_PARAM(marker);
#endif
}

Vector<String> AlternativeTextController::dictationAlternativesForMarker(const DocumentMarker& marker)
{
#if USE(DICTATION_ALTERNATIVES)
    if (auto* client = alternativeTextClient())
        return client->dictationAlternatives(WTF::get<DocumentMarker::DictationData>(marker.data()).context);
    return Vector<String>();
#else
    UNUSED_PARAM(marker);
    return Vector<String>();
#endif
}

void AlternativeTextController::applyDictationAlternative(const String& alternativeString)
{
#if USE(DICTATION_ALTERNATIVES)
    auto& editor = m_document.editor();
    auto selection = editor.selectedRange();
    if (!selection || !editor.shouldInsertText(alternativeString, *selection, EditorInsertAction::Pasted))
        return;
    for (auto* marker : selection->startContainer().document().markers().markersInRange(*selection, DocumentMarker::DictationAlternatives))
        removeDictationAlternativesForMarker(*marker);
    applyAlternativeTextToRange(*selection, alternativeString, AlternativeTextTypeDictationAlternatives, markerTypesForAppliedDictationAlternative());
#else
    UNUSED_PARAM(alternativeString);
#endif
}

} // namespace WebCore
