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
#include "DocumentFragment.h"
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "EventLoop.h"
#include "FloatQuad.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
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

constexpr OptionSet<DocumentMarker::Type> markerTypesForAppliedDictationAlternative()
{
    return DocumentMarker::Type::SpellCheckingExemption;
}

#endif

#if USE(AUTOCORRECTION_PANEL)

static inline OptionSet<DocumentMarker::Type> markerTypesForAutocorrection()
{
    return { DocumentMarker::Type::Autocorrected, DocumentMarker::Type::CorrectionIndicator, DocumentMarker::Type::Replacement, DocumentMarker::Type::SpellCheckingExemption };
}

static inline OptionSet<DocumentMarker::Type> markerTypesForReplacement()
{
    return { DocumentMarker::Type::Replacement, DocumentMarker::Type::SpellCheckingExemption };
}

static bool markersHaveIdenticalDescription(const Vector<WeakPtr<RenderedDocumentMarker>>& markers)
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
    : m_document(document)
{
}

AlternativeTextController::~AlternativeTextController()
{
    dismiss(ReasonForDismissingAlternativeText::Ignored);
}

void AlternativeTextController::startAlternativeTextUITimer(AlternativeTextType type)
{
    const Seconds correctionPanelTimerInterval { 300_ms };

    if (!isAutomaticSpellingCorrectionEnabled()) {
#if !ENABLE(ALTERNATIVE_TEXT_REQUIRES_AUTOMATIC_SPELLING_CORRECTION)
        // Exclude correction & reversion bubbles which have accept on dismiss behavior.
        if (type == AlternativeTextType::Correction || type == AlternativeTextType::Reversion)
            return;
#else
        return;
#endif
    }

    // If type is PanelTypeReversion, then the new range has been set. So we shouldn't clear it.
    if (type == AlternativeTextType::Correction)
        m_rangeWithAlternative = std::nullopt;
    m_type = type;
    m_timer = protectedDocument()->eventLoop().scheduleTask(correctionPanelTimerInterval, TaskSource::UserInteraction, [weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->timerFired();
    });
}

void AlternativeTextController::stopAlternativeTextUITimer()
{
    m_timer = nullptr;
    m_rangeWithAlternative = std::nullopt;
}

void AlternativeTextController::stopPendingCorrection(const VisibleSelection& oldSelection)
{
    // Make sure there's no pending autocorrection before we call markMisspellingsAndBadGrammar() below.
    VisibleSelection currentSelection(protectedDocument()->selection().selection());
    if (currentSelection == oldSelection)
        return;

    stopAlternativeTextUITimer();
    dismiss(ReasonForDismissingAlternativeText::Ignored);
}

void AlternativeTextController::applyPendingCorrection(const VisibleSelection& selectionAfterTyping)
{
    // Apply pending autocorrection before next round of spell checking.
    bool doApplyCorrection = true;
    VisiblePosition startOfSelection = selectionAfterTyping.visibleStart();
    VisibleSelection currentWord = VisibleSelection(startOfWord(startOfSelection, WordSide::LeftWordIfOnBoundary), endOfWord(startOfSelection, WordSide::RightWordIfOnBoundary));
    if (currentWord.visibleEnd() == startOfSelection) {
        if (auto wordRange = currentWord.firstRange()) {
            String wordText = plainText(*wordRange);
            if (!wordText.isEmpty() && isAmbiguousBoundaryCharacter(wordText[wordText.length() - 1]))
                doApplyCorrection = false;
        }
    }
    if (doApplyCorrection)
        handleAlternativeTextUIResult(dismissSoon(ReasonForDismissingAlternativeText::Accepted));
    else
        m_rangeWithAlternative = std::nullopt;
}

bool AlternativeTextController::hasPendingCorrection() const
{
    return m_rangeWithAlternative.has_value();
}

bool AlternativeTextController::isSpellingMarkerAllowed(const SimpleRange& misspellingRange) const
{
    CheckedPtr controller = protectedDocument()->markersIfExists();
    if (!controller)
        return true;
    return !controller->hasMarkers(misspellingRange, DocumentMarker::Type::SpellCheckingExemption);
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
    if (CheckedPtr client = alternativeTextClient())
        client->showCorrectionAlternative(m_type, boundingBox, m_originalText, replacement, { });
}

void AlternativeTextController::handleCancelOperation()
{
    if (!m_isActive)
        return;
    m_isActive = false;
    dismiss(ReasonForDismissingAlternativeText::Cancelled);
}

void AlternativeTextController::dismiss(ReasonForDismissingAlternativeText reasonForDismissing)
{
    if (!m_isActive)
        return;
    m_isActive = false;
    m_isDismissedByEditing = true;
    if (CheckedPtr client = alternativeTextClient())
        client->dismissAlternative(reasonForDismissing);
}

String AlternativeTextController::dismissSoon(ReasonForDismissingAlternativeText reasonForDismissing)
{
    if (!m_isActive)
        return String();
    m_isActive = false;
    m_isDismissedByEditing = true;
    if (CheckedPtr client = alternativeTextClient())
        return client->dismissAlternativeSoon(reasonForDismissing);
    return String();
}

bool AlternativeTextController::applyAutocorrectionBeforeTypingIfAppropriate()
{
    if (!m_rangeWithAlternative || !m_isActive)
        return false;

    if (m_type != AlternativeTextType::Correction)
        return false;

    Position caretPosition = protectedDocument()->selection().selection().start();

    if (makeDeprecatedLegacyPosition(m_rangeWithAlternative->end) == caretPosition) {
        handleAlternativeTextUIResult(dismissSoon(ReasonForDismissingAlternativeText::Accepted));
        return true;
    } 
    
    // Pending correction should always be where caret is. But in case this is not always true, we still want to dismiss the panel without accepting the correction.
    ASSERT(makeDeprecatedLegacyPosition(m_rangeWithAlternative->end) == caretPosition);
    dismiss(ReasonForDismissingAlternativeText::Ignored);
    return false;
}

void AlternativeTextController::respondToUnappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction)
{
    if (CheckedPtr client = alternativeTextClient())
        client->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, corrected, correction);

    auto document = protectedDocument();
    RefPtr protectedFrame { document->frame() };
    document->updateLayout();

    document->selection().setSelection(selectionOfCorrected, FrameSelection::defaultSetSelectionOptions() | FrameSelection::SetSelectionOption::SpellCorrectionTriggered);
    auto range = document->selection().selection().firstRange();
    if (!range)
        return;
    removeMarkers(*range, OptionSet<DocumentMarker::Type> { DocumentMarker::Type::Spelling, DocumentMarker::Type::Autocorrected }, RemovePartiallyOverlappingMarker::Yes);
    addMarker(*range, DocumentMarker::Type::Replacement);
    addMarker(*range, DocumentMarker::Type::SpellCheckingExemption);
}

void AlternativeTextController::timerFired()
{
    m_isDismissedByEditing = false;
    switch (m_type) {
    case AlternativeTextType::Correction: {
        auto document = protectedDocument();
        VisibleSelection selection(document->selection().selection());
        VisiblePosition start(selection.start(), selection.affinity());
        VisiblePosition p = startOfWord(start, WordSide::LeftWordIfOnBoundary);
        VisibleSelection adjacentWords = VisibleSelection(p, start);
        auto adjacentWordRange = adjacentWords.toNormalizedRange();
        document->editor().markAllMisspellingsAndBadGrammarInRanges({ TextCheckingType::Spelling, TextCheckingType::Replacement, TextCheckingType::ShowCorrectionPanel }, adjacentWordRange, adjacentWordRange, std::nullopt);
    }
        break;
    case AlternativeTextType::Reversion: {
        if (!m_rangeWithAlternative)
            break;
        String replacementString = std::get<AutocorrectionReplacement>(m_details);
        if (replacementString.isEmpty())
            break;
        m_isActive = true;
        m_originalText = plainText(*m_rangeWithAlternative);
        auto boundingBox = rootViewRectForRange(*m_rangeWithAlternative);
        if (!boundingBox.isEmpty()) {
            if (CheckedPtr client = alternativeTextClient()) {
                removeMarkers(*m_rangeWithAlternative, { DocumentMarker::Type::CorrectionIndicator });
                client->showCorrectionAlternative(m_type, boundingBox, m_originalText, replacementString, { });
            }
        }
    }
        break;
    case AlternativeTextType::SpellingSuggestions:
    case AlternativeTextType::GrammarSuggestions: {
        if (!m_rangeWithAlternative || plainText(*m_rangeWithAlternative) != m_originalText)
            break;

        Vector<String> suggestions;
        if (m_type == AlternativeTextType::GrammarSuggestions) {
            if (CheckedPtr editorClient = this->editorClient()) {
                TextCheckingHelper checker(*editorClient, *m_rangeWithAlternative);
                suggestions = checker.guessesForMisspelledWordOrUngrammaticalPhrase(true).guesses;
            }
        } else {
            auto paragraphText = plainText(TextCheckingParagraph(*m_rangeWithAlternative).paragraphRange());
            textChecker()->getGuessesForWord(m_originalText, paragraphText, protectedDocument()->selection().selection(), suggestions);
        }

        if (suggestions.isEmpty()) {
            m_rangeWithAlternative = std::nullopt;
            break;
        }
        String topSuggestion = suggestions.first();
        suggestions.remove(0);
        m_isActive = true;
        auto boundingBox = rootViewRectForRange(*m_rangeWithAlternative);
        if (!boundingBox.isEmpty()) {
            if (CheckedPtr client = alternativeTextClient())
                client->showCorrectionAlternative(m_type, boundingBox, m_originalText, topSuggestion, suggestions);
        }
    }
        break;
    case AlternativeTextType::DictationAlternatives:
    {
#if USE(DICTATION_ALTERNATIVES)
        if (!m_rangeWithAlternative)
            return;
        auto dictationContext = std::get<DictationContext>(m_details);
        if (!dictationContext)
            return;
        auto boundingBox = rootViewRectForRange(*m_rangeWithAlternative);
        m_isActive = true;
        if (!boundingBox.isEmpty()) {
            if (CheckedPtr client = alternativeTextClient())
                client->showDictationAlternativeUI(boundingBox, dictationContext);
        }
#endif
    }
        break;
    }
}

void AlternativeTextController::handleAlternativeTextUIResult(const String& result)
{
    if (!m_rangeWithAlternative || m_document.ptr() != &m_rangeWithAlternative->start.document())
        return;

    String currentWord = plainText(*m_rangeWithAlternative);
    // Check to see if the word we are about to correct has been changed between timer firing and callback being triggered.
    if (currentWord != m_originalText)
        return;

    m_isActive = false;

    switch (m_type) {
    case AlternativeTextType::Correction:
        if (result.length())
            applyAlternativeTextToRange(*m_rangeWithAlternative, result, m_type, markerTypesForAutocorrection());
        else if (!m_isDismissedByEditing)
            addMarker(*m_rangeWithAlternative, DocumentMarker::Type::RejectedCorrection, m_originalText);
        break;
    case AlternativeTextType::Reversion:
    case AlternativeTextType::SpellingSuggestions:
    case AlternativeTextType::GrammarSuggestions:
        if (result.length())
            applyAlternativeTextToRange(*m_rangeWithAlternative, result, m_type, markerTypesForReplacement());
        break;
    case AlternativeTextType::DictationAlternatives:
        if (result.length())
            applyAlternativeTextToRange(*m_rangeWithAlternative, result, m_type, markerTypesForAppliedDictationAlternative());
        break;
    }

    m_rangeWithAlternative = std::nullopt;
}

bool AlternativeTextController::canEnableAutomaticSpellingCorrection() const
{
#if ENABLE(AUTOCORRECT)
    auto position = protectedDocument()->selection().selection().start();
    if (RefPtr control = enclosingTextFormControl(position)) {
        if (!control->shouldAutocorrect())
            return false;
    } else if (RefPtr editableRoot = dynamicDowncast<HTMLElement>(position.rootEditableElement()); editableRoot && !editableRoot->shouldAutocorrect())
        return false;
#endif

    return true;
}

bool AlternativeTextController::isAutomaticSpellingCorrectionEnabled()
{
    if (!editorClient() || !editorClient()->isAutomaticSpellingCorrectionEnabled())
        return false;

    return canEnableAutomaticSpellingCorrection();
}

FloatRect AlternativeTextController::rootViewRectForRange(const SimpleRange& range) const
{
    RefPtr view = m_document->view();
    if (!view)
        return { };
    return view->contentsToRootView(unitedBoundingBoxes(RenderObject::absoluteTextQuads(range)));
}        

void AlternativeTextController::respondToChangedSelection(const VisibleSelection& oldSelection)
{
    VisibleSelection currentSelection(protectedDocument()->selection().selection());
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
    
    VisiblePosition endPositionOfWord = endOfWord(selectionPosition, WordSide::LeftWordIfOnBoundary);
    if (selectionPosition != endPositionOfWord)
        return;

    Position position = endPositionOfWord.deepEquivalent();
    if (position.anchorType() != Position::PositionIsOffsetInAnchor)
        return;

    RefPtr node = position.containerNode();
    CheckedPtr markers = node->document().markersIfExists();
    if (!markers)
        return;

    ASSERT(node);
    for (auto& marker : markers->markersFor(*node)) {
        ASSERT(marker);
        if (respondToMarkerAtEndOfWord(*marker, position))
            break;
    }
}

void AlternativeTextController::respondToUnappliedEditing(EditCommandComposition* command)
{
    if (!command->wasCreateLinkCommand())
        return;
    auto range = command->startingSelection().firstRange();
    if (!range)
        return;
    addMarker(*range, DocumentMarker::Type::Replacement);
    addMarker(*range, DocumentMarker::Type::SpellCheckingExemption);
}

EditorClient* AlternativeTextController::editorClient()
{
    return m_document->page() ? &m_document->page()->editorClient() : nullptr;
}

TextCheckerClient* AlternativeTextController::textChecker()
{
    if (CheckedPtr owner = editorClient())
        return owner->textChecker();
    return nullptr;
}

void AlternativeTextController::recordAutocorrectionResponse(AutocorrectionResponse response, const String& replacedString, const SimpleRange& replacementRange)
{
    if (CheckedPtr client = alternativeTextClient())
        client->recordAutocorrectionResponse(response, replacedString, plainText(replacementRange));
}

void AlternativeTextController::markReversed(const SimpleRange& changedRange)
{
    removeMarkers(changedRange, DocumentMarker::Type::Autocorrected, RemovePartiallyOverlappingMarker::Yes);
    addMarker(changedRange, DocumentMarker::Type::SpellCheckingExemption);
}

void AlternativeTextController::markCorrection(const SimpleRange& replacedRange, const String& replacedString)
{
    for (auto markerType : markerTypesForAutocorrection()) {
        if (markerType == DocumentMarker::Type::Replacement || markerType == DocumentMarker::Type::Autocorrected)
            addMarker(replacedRange, markerType, replacedString);
        else
            addMarker(replacedRange, markerType);
    }
}

void AlternativeTextController::recordSpellcheckerResponseForModifiedCorrection(const SimpleRange& rangeOfCorrection, const String& corrected, const String& correction)
{
    CheckedPtr markers = rangeOfCorrection.startContainer().document().markersIfExists();
    if (!markers)
        return;

    auto correctedOnceMarkers = markers->markersInRange(rangeOfCorrection, DocumentMarker::Type::Autocorrected);
    if (correctedOnceMarkers.isEmpty())
        return;

    if (CheckedPtr client = alternativeTextClient()) {
        // Spelling corrected text has been edited. We need to determine whether user has reverted it to original text or
        // edited it to something else, and notify spellchecker accordingly.
        if (markersHaveIdenticalDescription(correctedOnceMarkers) && correctedOnceMarkers[0]->description() == corrected)
            client->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, corrected, correction);
        else
            client->recordAutocorrectionResponse(AutocorrectionResponse::Edited, corrected, correction);
    }

    removeMarkers(rangeOfCorrection, DocumentMarker::Type::Autocorrected, RemovePartiallyOverlappingMarker::Yes);
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
    addMarker(*precedingCharacterRange, DocumentMarker::Type::DeletedAutocorrection, m_originalStringForLastDeletedAutocorrection);
}

bool AlternativeTextController::processMarkersOnTextToBeReplacedByResult(const TextCheckingResult& result, const SimpleRange& rangeWithAlternative, const String& stringToBeReplaced)
{
    auto document = protectedDocument();
    auto& markers = document->markers();
    if (markers.hasMarkers(rangeWithAlternative, DocumentMarker::Type::Replacement)) {
        if (result.type == TextCheckingType::Correction)
            recordSpellcheckerResponseForModifiedCorrection(rangeWithAlternative, stringToBeReplaced, result.replacement);
        return false;
    }

    if (markers.hasMarkers(rangeWithAlternative, DocumentMarker::Type::RejectedCorrection))
        return false;

    if (markers.hasMarkers(rangeWithAlternative, DocumentMarker::Type::AcceptedCandidate))
        return false;

    auto precedingCharacterRange = makeSimpleRange(makeDeprecatedLegacyPosition(rangeWithAlternative.start).previous(), rangeWithAlternative.start);
    if (!precedingCharacterRange)
        return false;

    for (auto& marker : markers.markersInRange(*precedingCharacterRange, DocumentMarker::Type::DeletedAutocorrection)) {
        if (marker->description() == stringToBeReplaced)
            return false;
    }

    return true;
}

bool AlternativeTextController::shouldStartTimerFor(const WebCore::DocumentMarker &marker, int endOffset) const
{
    if (static_cast<int>(marker.endOffset()) != endOffset)
        return false;

    switch (marker.type()) {
    case DocumentMarker::Type::Spelling:
    case DocumentMarker::Type::Grammar:
    case DocumentMarker::Type::DictationAlternatives:
        return true;
    case DocumentMarker::Type::Replacement:
        return !marker.description().isNull();
    default:
        return false;
    }
}

bool AlternativeTextController::respondToMarkerAtEndOfWord(const DocumentMarker& marker, const Position& endOfWordPosition)
{
    if (!shouldStartTimerFor(marker, endOfWordPosition.offsetInContainerNode()))
        return false;
    RefPtr node = endOfWordPosition.containerNode();
    auto wordRange = makeSimpleRange(*node, marker);
    String currentWord = plainText(wordRange);
    if (!currentWord.length())
        return false;
    m_originalText = currentWord;
    switch (marker.type()) {
    case DocumentMarker::Type::Spelling:
    case DocumentMarker::Type::Grammar:
        m_rangeWithAlternative = WTFMove(wordRange);
        m_details = emptyString();
        startAlternativeTextUITimer((marker.type() == DocumentMarker::Type::Spelling) ? AlternativeTextType::SpellingSuggestions : AlternativeTextType::GrammarSuggestions);
        break;
    case DocumentMarker::Type::Replacement:
        m_rangeWithAlternative = WTFMove(wordRange);
        m_details = marker.description();
        startAlternativeTextUITimer(AlternativeTextType::Reversion);
        break;
    case DocumentMarker::Type::DictationAlternatives: {
        auto& markerData = std::get<DocumentMarker::DictationData>(marker.data());
        if (currentWord != markerData.originalText)
            return false;
        m_rangeWithAlternative = WTFMove(wordRange);
        m_details = markerData.context;
        startAlternativeTextUITimer(AlternativeTextType::DictationAlternatives);
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
    return m_document->frame() && m_document->page() ? m_document->page()->alternativeTextClient() : nullptr;
}

String AlternativeTextController::markerDescriptionForAppliedAlternativeText(AlternativeTextType alternativeTextType, DocumentMarker::Type markerType)
{
#if USE(AUTOCORRECTION_PANEL)
    if (alternativeTextType != AlternativeTextType::Reversion && alternativeTextType != AlternativeTextType::DictationAlternatives && (markerType == DocumentMarker::Type::Replacement || markerType == DocumentMarker::Type::Autocorrected))
        return m_originalText;
#else
    UNUSED_PARAM(alternativeTextType);
    UNUSED_PARAM(markerType);
#endif
    return emptyString();
}

void AlternativeTextController::applyAlternativeTextToRange(const SimpleRange& range, const String& alternative, AlternativeTextType alternativeType, OptionSet<DocumentMarker::Type> markerTypesToAdd)
{
    // After we replace the word at range rangeWithAlternative, we need to add markers to that range.
    // So before we carry out the replacement,store the start position relative to the start position
    // of the containing paragraph.

    // Take note of the location of autocorrection so that we can add marker after the replacement took place.
    auto paragraphStart = makeBoundaryPoint(startOfParagraph(makeDeprecatedLegacyPosition(range.start)));
    if (!paragraphStart)
        return;
    Ref treeScopeRoot = range.start.container->treeScope().rootNode();
    auto treeScopeStart = BoundaryPoint { treeScopeRoot.get(), 0 };
    auto correctionOffsetInParagraph = characterCount({ *paragraphStart, range.start });
    auto paragraphOffsetInTreeScope = characterCount({ treeScopeStart, *paragraphStart });

    SpellingCorrectionCommand::create(range, alternative)->apply();

    // Recalculate pragraphRangeContainingCorrection, since SpellingCorrectionCommand modified the DOM, such that the original paragraphRangeContainingCorrection is no longer valid. Radar: 10305315 Bugzilla: 89526
    auto updatedParagraphStartContainingCorrection = resolveCharacterLocation(makeRangeSelectingNodeContents(treeScopeRoot), paragraphOffsetInTreeScope);
    auto updatedParagraphEndContainingCorrection = makeBoundaryPoint(protectedDocument()->selection().selection().start());
    if (!updatedParagraphEndContainingCorrection)
        return;
    auto replacementRange = resolveCharacterRange({ updatedParagraphStartContainingCorrection, *updatedParagraphEndContainingCorrection }, CharacterRange(correctionOffsetInParagraph, alternative.length()));

    // Check to see if replacement succeeded.
    if (plainText(replacementRange) != alternative)
        return;

    for (auto markerType : markerTypesToAdd)
        addMarker(replacementRange, markerType, markerDescriptionForAppliedAlternativeText(alternativeType, markerType));
}

#endif // USE(AUTOCORRECTION_PANEL)

void AlternativeTextController::removeCorrectionIndicatorMarkers()
{
    CheckedPtr markers = protectedDocument()->markersIfExists();
    if (!markers)
        return;
#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
    markers->dismissMarkers(DocumentMarker::Type::CorrectionIndicator);
#else
    markers->removeMarkers(DocumentMarker::Type::CorrectionIndicator);
#endif
}

void AlternativeTextController::respondToAppliedEditing(CompositeEditCommand* command)
{
#if USE(AUTOCORRECTION_PANEL)
    if (command->isTopLevelCommand() && !command->shouldRetainAutocorrectionIndicator())
        removeCorrectionIndicatorMarkers();

    markPrecedingWhitespaceForDeletedAutocorrectionAfterCommand(command);
    m_originalStringForLastDeletedAutocorrection = String();

    dismiss(ReasonForDismissingAlternativeText::Ignored);
#elif HAVE(AUTOCORRECTION_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)
    if (!command->shouldRetainAutocorrectionIndicator())
        removeCorrectionIndicatorMarkers();
#else
    UNUSED_PARAM(command);
#endif
}

bool AlternativeTextController::insertDictatedText(const String& text, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent)
{
    RefPtr<EventTarget> target;
    auto document = protectedDocument();
    if (triggeringEvent)
        target = triggeringEvent->target();
    else
        target = eventTargetElementForDocument(document.ptr());
    if (!target)
        return false;

    Ref windowProxy = document->frame()->windowProxy();
    auto event = TextEvent::createForDictation(windowProxy.ptr(), text, dictationAlternatives);
    event->setUnderlyingEvent(triggeringEvent);

    target->dispatchEvent(event);
    return event->defaultHandled();
}

void AlternativeTextController::removeDictationAlternativesForMarker(const DocumentMarker& marker)
{
#if USE(DICTATION_ALTERNATIVES)
    if (CheckedPtr client = alternativeTextClient())
        client->removeDictationAlternatives(std::get<DocumentMarker::DictationData>(marker.data()).context);
#else
    UNUSED_PARAM(marker);
#endif
}

Vector<String> AlternativeTextController::dictationAlternativesForMarker(const DocumentMarker& marker)
{
#if USE(DICTATION_ALTERNATIVES)
    if (CheckedPtr client = alternativeTextClient())
        return client->dictationAlternatives(std::get<DocumentMarker::DictationData>(marker.data()).context);
    return Vector<String>();
#else
    UNUSED_PARAM(marker);
    return Vector<String>();
#endif
}

void AlternativeTextController::applyDictationAlternative(const String& alternativeString)
{
#if USE(DICTATION_ALTERNATIVES)
    auto document = protectedDocument();
    auto& editor = document->editor();
    auto selection = editor.selectedRange();
    if (!selection || !editor.shouldInsertText(alternativeString, *selection, EditorInsertAction::Pasted))
        return;
    for (auto& marker : selection->startContainer().document().markers().markersInRange(*selection, DocumentMarker::Type::DictationAlternatives))
        removeDictationAlternativesForMarker(*marker);
    applyAlternativeTextToRange(*selection, alternativeString, AlternativeTextType::DictationAlternatives, markerTypesForAppliedDictationAlternative());
#else
    UNUSED_PARAM(alternativeString);
#endif
}

} // namespace WebCore
