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
#include "Element.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "RenderedDocumentMarker.h"
#include "SpellingCorrectionCommand.h"
#include "TextCheckerClient.h"
#include "TextCheckingHelper.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "markup.h"

namespace WebCore {

#if USE(AUTOCORRECTION_PANEL)

static inline OptionSet<DocumentMarker::MarkerType> markerTypesForAutocorrection()
{
    return { DocumentMarker::Autocorrected, DocumentMarker::CorrectionIndicator, DocumentMarker::Replacement, DocumentMarker::SpellCheckingExemption };
}

static inline OptionSet<DocumentMarker::MarkerType> markerTypesForReplacement()
{
    return { DocumentMarker::Replacement, DocumentMarker::SpellCheckingExemption };
}

static inline OptionSet<DocumentMarker::MarkerType> markerTypesForAppliedDictationAlternative()
{
    return DocumentMarker::SpellCheckingExemption;
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

AlternativeTextController::AlternativeTextController(Frame& frame)
    : m_timer(*this, &AlternativeTextController::timerFired)
    , m_frame(frame)
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
        m_rangeWithAlternative = nullptr;
    m_type = type;
    m_timer.startOneShot(correctionPanelTimerInterval);
}

void AlternativeTextController::stopAlternativeTextUITimer()
{
    m_timer.stop();
    m_rangeWithAlternative = nullptr;
}

void AlternativeTextController::stopPendingCorrection(const VisibleSelection& oldSelection)
{
    // Make sure there's no pending autocorrection before we call markMisspellingsAndBadGrammar() below.
    VisibleSelection currentSelection(m_frame.selection().selection());
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
        String wordText = plainText(currentWord.toNormalizedRange().get());
        if (wordText.length() > 0 && isAmbiguousBoundaryCharacter(wordText[wordText.length() - 1]))
            doApplyCorrection = false;
    }
    if (doApplyCorrection)
        handleAlternativeTextUIResult(dismissSoon(ReasonForDismissingAlternativeTextAccepted)); 
    else
        m_rangeWithAlternative = nullptr;
}

bool AlternativeTextController::hasPendingCorrection() const
{
    return m_rangeWithAlternative;
}

bool AlternativeTextController::isSpellingMarkerAllowed(Range& misspellingRange) const
{
    return !m_frame.document()->markers().hasMarkers(misspellingRange, DocumentMarker::SpellCheckingExemption);
}

void AlternativeTextController::show(Range& rangeToReplace, const String& replacement)
{
    FloatRect boundingBox = rootViewRectForRange(&rangeToReplace);
    if (boundingBox.isEmpty())
        return;
    m_originalText = plainText(&rangeToReplace);
    m_rangeWithAlternative = &rangeToReplace;
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

void AlternativeTextController::applyAlternativeTextToRange(const Range& range, const String& alternative, AlternativeTextType alternativeType, OptionSet<DocumentMarker::MarkerType> markerTypesToAdd)
{
    auto paragraphRangeContainingCorrection = range.cloneRange();

    setStart(paragraphRangeContainingCorrection.ptr(), startOfParagraph(range.startPosition()));
    setEnd(paragraphRangeContainingCorrection.ptr(), endOfParagraph(range.endPosition()));

    // After we replace the word at range rangeWithAlternative, we need to add markers to that range.
    // However, once the replacement took place, the value of rangeWithAlternative is not valid anymore.
    // So before we carry out the replacement, we need to store the start position of rangeWithAlternative
    // relative to the start position of the containing paragraph. We use correctionStartOffsetInParagraph
    // to store this value. In order to obtain this offset, we need to first create a range
    // which spans from the start of paragraph to the start position of rangeWithAlternative.
    auto correctionStartOffsetInParagraphAsRange = Range::create(paragraphRangeContainingCorrection->startContainer().document(), paragraphRangeContainingCorrection->startPosition(), paragraphRangeContainingCorrection->startPosition());

    Position startPositionOfRangeWithAlternative = range.startPosition();
    if (!startPositionOfRangeWithAlternative.containerNode())
        return;
    auto setEndResult = correctionStartOffsetInParagraphAsRange->setEnd(*startPositionOfRangeWithAlternative.containerNode(), startPositionOfRangeWithAlternative.computeOffsetInContainerNode());
    if (setEndResult.hasException())
        return;

    // Take note of the location of autocorrection so that we can add marker after the replacement took place.
    int correctionStartOffsetInParagraph = TextIterator::rangeLength(correctionStartOffsetInParagraphAsRange.ptr());

    // Clone the range, since the caller of this method may want to keep the original range around.
    auto rangeWithAlternative = range.cloneRange();

    ContainerNode& rootNode = paragraphRangeContainingCorrection->startContainer().treeScope().rootNode();
    int paragraphStartIndex = TextIterator::rangeLength(Range::create(rootNode.document(), &rootNode, 0, &paragraphRangeContainingCorrection->startContainer(), paragraphRangeContainingCorrection->startOffset()).ptr());
    SpellingCorrectionCommand::create(rangeWithAlternative, alternative)->apply();
    // Recalculate pragraphRangeContainingCorrection, since SpellingCorrectionCommand modified the DOM, such that the original paragraphRangeContainingCorrection is no longer valid. Radar: 10305315 Bugzilla: 89526
    auto updatedParagraphRangeContainingCorrection = TextIterator::rangeFromLocationAndLength(&rootNode, paragraphStartIndex, correctionStartOffsetInParagraph + alternative.length());
    if (!updatedParagraphRangeContainingCorrection)
        return;
    
    setEnd(updatedParagraphRangeContainingCorrection.get(), m_frame.selection().selection().start());
    RefPtr<Range> replacementRange = TextIterator::subrange(*updatedParagraphRangeContainingCorrection, correctionStartOffsetInParagraph, alternative.length());
    String newText = plainText(replacementRange.get());

    // Check to see if replacement succeeded.
    if (newText != alternative)
        return;

    DocumentMarkerController& markers = replacementRange->startContainer().document().markers();

    for (auto markerType : markerTypesToAdd)
        markers.addMarker(replacementRange.get(), markerType, markerDescriptionForAppliedAlternativeText(alternativeType, markerType));
}

bool AlternativeTextController::applyAutocorrectionBeforeTypingIfAppropriate()
{
    if (!m_rangeWithAlternative || !m_isActive)
        return false;

    if (m_type != AlternativeTextTypeCorrection)
        return false;

    Position caretPosition = m_frame.selection().selection().start();

    if (m_rangeWithAlternative->endPosition() == caretPosition) {
        handleAlternativeTextUIResult(dismissSoon(ReasonForDismissingAlternativeTextAccepted));
        return true;
    } 
    
    // Pending correction should always be where caret is. But in case this is not always true, we still want to dismiss the panel without accepting the correction.
    ASSERT(m_rangeWithAlternative->endPosition() == caretPosition);
    dismiss(ReasonForDismissingAlternativeTextIgnored);
    return false;
}

void AlternativeTextController::respondToUnappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction)
{
    if (AlternativeTextClient* client = alternativeTextClient())
        client->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, corrected, correction);

    Ref<Frame> protector(m_frame);
    m_frame.document()->updateLayout();
    m_frame.selection().setSelection(selectionOfCorrected, FrameSelection::defaultSetSelectionOptions() | FrameSelection::SpellCorrectionTriggered);
    auto range = Range::create(*m_frame.document(), m_frame.selection().selection().start(), m_frame.selection().selection().end());

    DocumentMarkerController& markers = m_frame.document()->markers();
    markers.removeMarkers(range.ptr(), OptionSet<DocumentMarker::MarkerType> { DocumentMarker::Spelling, DocumentMarker::Autocorrected }, DocumentMarkerController::RemovePartiallyOverlappingMarker);
    markers.addMarker(range.ptr(), DocumentMarker::Replacement);
    markers.addMarker(range.ptr(), DocumentMarker::SpellCheckingExemption);
}

void AlternativeTextController::timerFired()
{
    m_isDismissedByEditing = false;
    switch (m_type) {
    case AlternativeTextTypeCorrection: {
        VisibleSelection selection(m_frame.selection().selection());
        VisiblePosition start(selection.start(), selection.affinity());
        VisiblePosition p = startOfWord(start, LeftWordIfOnBoundary);
        VisibleSelection adjacentWords = VisibleSelection(p, start);
        auto adjacentWordRange = adjacentWords.toNormalizedRange();
        m_frame.editor().markAllMisspellingsAndBadGrammarInRanges({ TextCheckingType::Spelling, TextCheckingType::Replacement, TextCheckingType::ShowCorrectionPanel }, adjacentWordRange.copyRef(), adjacentWordRange.copyRef(), nullptr);
    }
        break;
    case AlternativeTextTypeReversion: {
        if (!m_rangeWithAlternative)
            break;
        String replacementString = WTF::get<AutocorrectionReplacement>(m_details);
        if (replacementString.isEmpty())
            break;
        m_isActive = true;
        m_originalText = plainText(m_rangeWithAlternative.get());
        FloatRect boundingBox = rootViewRectForRange(m_rangeWithAlternative.get());
        if (!boundingBox.isEmpty()) {
            if (AlternativeTextClient* client = alternativeTextClient())
                client->showCorrectionAlternative(m_type, boundingBox, m_originalText, replacementString, { });
        }
    }
        break;
    case AlternativeTextTypeSpellingSuggestions: {
        if (!m_rangeWithAlternative || plainText(m_rangeWithAlternative.get()) != m_originalText)
            break;
        String paragraphText = plainText(&TextCheckingParagraph(*m_rangeWithAlternative).paragraphRange());
        Vector<String> suggestions;
        textChecker()->getGuessesForWord(m_originalText, paragraphText, m_frame.selection().selection(), suggestions);
        if (suggestions.isEmpty()) {
            m_rangeWithAlternative = nullptr;
            break;
        }
        String topSuggestion = suggestions.first();
        suggestions.remove(0);
        m_isActive = true;
        FloatRect boundingBox = rootViewRectForRange(m_rangeWithAlternative.get());
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
        uint64_t dictationContext = WTF::get<AlternativeDictationContext>(m_details);
        if (!dictationContext)
            return;
        FloatRect boundingBox = rootViewRectForRange(m_rangeWithAlternative.get());
        m_isActive = true;
        if (!boundingBox.isEmpty()) {
            if (AlternativeTextClient* client = alternativeTextClient())
                client->showDictationAlternativeUI(boundingBox, dictationContext);
        }
#endif
    }
        break;
    }
}

void AlternativeTextController::handleAlternativeTextUIResult(const String& result)
{
    Range* rangeWithAlternative = m_rangeWithAlternative.get();
    if (!rangeWithAlternative || m_frame.document() != &rangeWithAlternative->ownerDocument())
        return;

    String currentWord = plainText(rangeWithAlternative);
    // Check to see if the word we are about to correct has been changed between timer firing and callback being triggered.
    if (currentWord != m_originalText)
        return;

    m_isActive = false;

    switch (m_type) {
    case AlternativeTextTypeCorrection:
        if (result.length())
            applyAlternativeTextToRange(*rangeWithAlternative, result, m_type, markerTypesForAutocorrection());
        else if (!m_isDismissedByEditing)
            rangeWithAlternative->startContainer().document().markers().addMarker(rangeWithAlternative, DocumentMarker::RejectedCorrection, m_originalText);
        break;
    case AlternativeTextTypeReversion:
    case AlternativeTextTypeSpellingSuggestions:
        if (result.length())
            applyAlternativeTextToRange(*rangeWithAlternative, result, m_type, markerTypesForReplacement());
        break;
    case AlternativeTextTypeDictationAlternatives:
        if (result.length())
            applyAlternativeTextToRange(*rangeWithAlternative, result, m_type, markerTypesForAppliedDictationAlternative());
        break;
    }

    m_rangeWithAlternative = nullptr;
}

bool AlternativeTextController::isAutomaticSpellingCorrectionEnabled()
{
    return editorClient() && editorClient()->isAutomaticSpellingCorrectionEnabled();
}

FloatRect AlternativeTextController::rootViewRectForRange(const Range* range) const
{
    FrameView* view = m_frame.view();
    if (!view)
        return FloatRect();
    Vector<FloatQuad> textQuads;
    range->absoluteTextQuads(textQuads);
    FloatRect boundingRect;
    for (auto& textQuad : textQuads)
        boundingRect.unite(textQuad.boundingBox());
    return view->contentsToRootView(IntRect(boundingRect));
}        

void AlternativeTextController::respondToChangedSelection(const VisibleSelection& oldSelection)
{
    VisibleSelection currentSelection(m_frame.selection().selection());
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
        m_frame.document()->markers().removeMarkers(DocumentMarker::CorrectionIndicator);

    markPrecedingWhitespaceForDeletedAutocorrectionAfterCommand(command);
    m_originalStringForLastDeletedAutocorrection = String();

    dismiss(ReasonForDismissingAlternativeTextIgnored);
}

void AlternativeTextController::respondToUnappliedEditing(EditCommandComposition* command)
{
    if (!command->wasCreateLinkCommand())
        return;
    auto range = Range::create(*m_frame.document(), command->startingSelection().start(), command->startingSelection().end());
    DocumentMarkerController& markers = m_frame.document()->markers();
    markers.addMarker(range.ptr(), DocumentMarker::Replacement);
    markers.addMarker(range.ptr(), DocumentMarker::SpellCheckingExemption);
}

AlternativeTextClient* AlternativeTextController::alternativeTextClient()
{
    return m_frame.page() ? m_frame.page()->alternativeTextClient() : nullptr;
}

EditorClient* AlternativeTextController::editorClient()
{
    return m_frame.page() ? &m_frame.page()->editorClient() : nullptr;
}

TextCheckerClient* AlternativeTextController::textChecker()
{
    if (EditorClient* owner = editorClient())
        return owner->textChecker();
    return nullptr;
}

void AlternativeTextController::recordAutocorrectionResponse(AutocorrectionResponse response, const String& replacedString, Range* replacementRange)
{
    if (auto client = alternativeTextClient())
        client->recordAutocorrectionResponse(response, replacedString, plainText(replacementRange));
}

void AlternativeTextController::markReversed(Range& changedRange)
{
    changedRange.startContainer().document().markers().removeMarkers(&changedRange, DocumentMarker::Autocorrected, DocumentMarkerController::RemovePartiallyOverlappingMarker);
    changedRange.startContainer().document().markers().addMarker(&changedRange, DocumentMarker::SpellCheckingExemption);
}

void AlternativeTextController::markCorrection(Range& replacedRange, const String& replacedString)
{
    DocumentMarkerController& markers = replacedRange.startContainer().document().markers();
    for (auto markerType : markerTypesForAutocorrection()) {
        if (markerType == DocumentMarker::Replacement || markerType == DocumentMarker::Autocorrected)
            markers.addMarker(&replacedRange, markerType, replacedString);
        else
            markers.addMarker(&replacedRange, markerType);
    }
}

void AlternativeTextController::recordSpellcheckerResponseForModifiedCorrection(Range& rangeOfCorrection, const String& corrected, const String& correction)
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

    markers.removeMarkers(&rangeOfCorrection, DocumentMarker::Autocorrected, DocumentMarkerController::RemovePartiallyOverlappingMarker);
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

    auto precedingCharacterRange = Range::create(*m_frame.document(), precedingCharacterPosition, endOfSelection);
    String string = plainText(precedingCharacterRange.ptr());
    if (string.isEmpty() || !deprecatedIsEditingWhitespace(string[string.length() - 1]))
        return;

    // Mark this whitespace to indicate we have deleted an autocorrection following this
    // whitespace. So if the user types the same original word again at this position, we
    // won't autocorrect it again.
    m_frame.document()->markers().addMarker(precedingCharacterRange.ptr(), DocumentMarker::DeletedAutocorrection, m_originalStringForLastDeletedAutocorrection);
}

bool AlternativeTextController::processMarkersOnTextToBeReplacedByResult(const TextCheckingResult& result, Range& rangeWithAlternative, const String& stringToBeReplaced)
{
    DocumentMarkerController& markerController = m_frame.document()->markers();
    if (markerController.hasMarkers(rangeWithAlternative, DocumentMarker::Replacement)) {
        if (result.type == TextCheckingType::Correction)
            recordSpellcheckerResponseForModifiedCorrection(rangeWithAlternative, stringToBeReplaced, result.replacement);
        return false;
    }

    if (markerController.hasMarkers(rangeWithAlternative, DocumentMarker::RejectedCorrection))
        return false;

    if (markerController.hasMarkers(rangeWithAlternative, DocumentMarker::AcceptedCandidate))
        return false;

    Position beginningOfRange = rangeWithAlternative.startPosition();
    Position precedingCharacterPosition = beginningOfRange.previous();
    auto precedingCharacterRange = Range::create(*m_frame.document(), precedingCharacterPosition, beginningOfRange);

    Vector<RenderedDocumentMarker*> markers = markerController.markersInRange(precedingCharacterRange, DocumentMarker::DeletedAutocorrection);
    for (const auto* marker : markers) {
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
    auto wordRange = Range::create(*m_frame.document(), node, marker.startOffset(), node, marker.endOffset());
    String currentWord = plainText(wordRange.ptr());
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
        if (!WTF::holds_alternative<DocumentMarker::DictationData>(marker.data()))
            return false;
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

String AlternativeTextController::markerDescriptionForAppliedAlternativeText(AlternativeTextType alternativeTextType, DocumentMarker::MarkerType markerType)
{

    if (alternativeTextType != AlternativeTextTypeReversion && alternativeTextType != AlternativeTextTypeDictationAlternatives && (markerType == DocumentMarker::Replacement || markerType == DocumentMarker::Autocorrected))
        return m_originalText;
    return emptyString();
}

#endif

bool AlternativeTextController::insertDictatedText(const String& text, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent)
{
    EventTarget* target;
    if (triggeringEvent)
        target = triggeringEvent->target();
    else
        target = eventTargetElementForDocument(m_frame.document());
    if (!target)
        return false;

    if (FrameView* view = m_frame.view())
        view->disableLayerFlushThrottlingTemporarilyForInteraction();

    auto event = TextEvent::createForDictation(&m_frame.windowProxy(), text, dictationAlternatives);
    event->setUnderlyingEvent(triggeringEvent);

    target->dispatchEvent(event);
    return event->defaultHandled();
}

void AlternativeTextController::removeDictationAlternativesForMarker(const DocumentMarker& marker)
{
#if USE(DICTATION_ALTERNATIVES)
    ASSERT(WTF::holds_alternative<DocumentMarker::DictationData>(marker.data()));
    if (auto* client = alternativeTextClient())
        client->removeDictationAlternatives(WTF::get<DocumentMarker::DictationData>(marker.data()).context);
#else
    UNUSED_PARAM(marker);
#endif
}

Vector<String> AlternativeTextController::dictationAlternativesForMarker(const DocumentMarker& marker)
{
#if USE(DICTATION_ALTERNATIVES)
    ASSERT(marker.type() == DocumentMarker::DictationAlternatives);
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
    Editor& editor = m_frame.editor();
    RefPtr<Range> selection = editor.selectedRange();
    if (!selection || !editor.shouldInsertText(alternativeString, selection.get(), EditorInsertAction::Pasted))
        return;
    DocumentMarkerController& markers = selection->startContainer().document().markers();
    Vector<RenderedDocumentMarker*> dictationAlternativesMarkers = markers.markersInRange(*selection, DocumentMarker::DictationAlternatives);
    for (auto* marker : dictationAlternativesMarkers)
        removeDictationAlternativesForMarker(*marker);

    applyAlternativeTextToRange(*selection, alternativeString, AlternativeTextTypeDictationAlternatives, markerTypesForAppliedDictationAlternative());
#else
    UNUSED_PARAM(alternativeString);
#endif
}

} // namespace WebCore
