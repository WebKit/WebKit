/*
 * Copyright (C) 2005-2019 Apple Inc.  All rights reserved.
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
#include "TypingCommand.h"

#include "AXObjectCache.h"
#include "BreakBlockquoteCommand.h"
#include "DataTransfer.h"
#include "DeleteSelectionCommand.h"
#include "DocumentInlines.h"
#include "EditCommand.h"
#include "Editing.h"
#include "Editor.h"
#include "Element.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MarkupAccumulator.h"
#include "MathMLElement.h"
#include "Range.h"
#include "RenderElement.h"
#include "StaticRange.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

class TypingCommandLineOperation
{
public:
    TypingCommandLineOperation(TypingCommand& typingCommand, bool selectInsertedText, const String& text)
    : m_typingCommand(typingCommand)
    , m_selectInsertedText(selectInsertedText)
    , m_text(text)
    { }
    
    void operator()(size_t lineOffset, size_t lineLength, bool isLastLine) const
    {
        if (isLastLine) {
            if (!lineOffset || lineLength > 0)
                m_typingCommand->insertTextRunWithoutNewlines(m_text.substring(lineOffset, lineLength), m_selectInsertedText);
        } else {
            if (lineLength > 0)
                m_typingCommand->insertTextRunWithoutNewlines(m_text.substring(lineOffset, lineLength), false);
            m_typingCommand->insertParagraphSeparator();
        }
    }
    
private:
    WeakRef<TypingCommand> m_typingCommand;
    bool m_selectInsertedText;
    const String& m_text;
};

static inline EditAction editActionForTypingCommand(TypingCommand::Type command, TextGranularity granularity, TypingCommand::TextCompositionType compositionType, bool isAutocompletion)
{
    if (compositionType == TypingCommand::TextCompositionType::Pending) {
        if (command == TypingCommand::Type::InsertText)
            return EditAction::TypingInsertPendingComposition;
        if (command == TypingCommand::Type::DeleteSelection)
            return EditAction::TypingDeletePendingComposition;
        ASSERT_NOT_REACHED();
    }

    if (compositionType == TypingCommand::TextCompositionType::Final) {
        if (command == TypingCommand::Type::InsertText)
            return EditAction::TypingInsertFinalComposition;
        if (command == TypingCommand::Type::DeleteSelection)
            return EditAction::TypingDeleteFinalComposition;
        ASSERT_NOT_REACHED();
    }

    switch (command) {
    case TypingCommand::Type::DeleteSelection:
        return EditAction::TypingDeleteSelection;
    case TypingCommand::Type::DeleteKey: {
        if (granularity == TextGranularity::WordGranularity)
            return EditAction::TypingDeleteWordBackward;
        if (granularity == TextGranularity::LineBoundary)
            return EditAction::TypingDeleteLineBackward;
        return EditAction::TypingDeleteBackward;
    }
    case TypingCommand::Type::ForwardDeleteKey:
        if (granularity == TextGranularity::WordGranularity)
            return EditAction::TypingDeleteWordForward;
        if (granularity == TextGranularity::LineBoundary)
            return EditAction::TypingDeleteLineForward;
        return EditAction::TypingDeleteForward;
    case TypingCommand::Type::InsertText:
        return isAutocompletion ? EditAction::InsertReplacement : EditAction::TypingInsertText;
    case TypingCommand::Type::InsertLineBreak:
        return EditAction::TypingInsertLineBreak;
    case TypingCommand::Type::InsertParagraphSeparator:
    case TypingCommand::Type::InsertParagraphSeparatorInQuotedContent:
        return EditAction::TypingInsertParagraph;
    default:
        return EditAction::Unspecified;
    }
}

static inline bool editActionIsDeleteByTyping(EditAction action)
{
    switch (action) {
    case EditAction::TypingDeleteSelection:
    case EditAction::TypingDeleteBackward:
    case EditAction::TypingDeleteWordBackward:
    case EditAction::TypingDeleteLineBackward:
    case EditAction::TypingDeleteForward:
    case EditAction::TypingDeleteWordForward:
    case EditAction::TypingDeleteLineForward:
        return true;
    default:
        return false;
    }
}

TypingCommand::TypingCommand(Ref<Document>&& document, Type commandType, const String &textToInsert, OptionSet<Option> options, TextGranularity granularity, TextCompositionType compositionType)
    : TextInsertionBaseCommand(WTFMove(document), editActionForTypingCommand(commandType, granularity, compositionType, options.contains(Option::IsAutocompletion)))
    , m_commandType(commandType)
    , m_textToInsert(textToInsert)
    , m_currentTextToInsert(textToInsert)
    , m_openForMoreTyping(true)
    , m_selectInsertedText(options & Option::SelectInsertedText)
    , m_smartDelete(options & Option::SmartDelete)
    , m_granularity(granularity)
    , m_compositionType(compositionType)
    , m_shouldAddToKillRing(options & Option::AddsToKillRing)
    , m_isAutocompletion(options & Option::IsAutocompletion)
    , m_openedByBackwardDelete(false)
    , m_shouldRetainAutocorrectionIndicator(options & Option::RetainAutocorrectionIndicator)
    , m_shouldPreventSpellChecking(options & Option::PreventSpellChecking)
{
    m_currentTypingEditAction = editingAction();
    updatePreservesTypingStyle(m_commandType);
}

void TypingCommand::deleteSelection(Ref<Document>&& document, OptionSet<Option> options, TextCompositionType compositionType)
{
    if (!document->selection().isRange())
        return;

    if (RefPtr<TypingCommand> lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
        lastTypingCommand->setIsAutocompletion(options.contains(Option::IsAutocompletion));
        lastTypingCommand->setCompositionType(compositionType);
        lastTypingCommand->setShouldPreventSpellChecking(options.contains(Option::PreventSpellChecking));
        lastTypingCommand->deleteSelection(options.contains(Option::SmartDelete));
        return;
    }

    TypingCommand::create(WTFMove(document), Type::DeleteSelection, emptyString(), options, compositionType)->apply();
}

void TypingCommand::deleteKeyPressed(Ref<Document>&& document, OptionSet<Option> options, TextGranularity granularity)
{
    if (granularity == TextGranularity::CharacterGranularity) {
        if (RefPtr<TypingCommand> lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
            updateSelectionIfDifferentFromCurrentSelection(lastTypingCommand.get(), document);
            lastTypingCommand->setIsAutocompletion(options.contains(Option::IsAutocompletion));
            lastTypingCommand->setCompositionType(TextCompositionType::None);
            lastTypingCommand->setShouldPreventSpellChecking(options.contains(Option::PreventSpellChecking));
            lastTypingCommand->deleteKeyPressed(granularity, options.contains(Option::AddsToKillRing));
            return;
        }
    }

    TypingCommand::create(WTFMove(document), Type::DeleteKey, emptyString(), options, granularity)->apply();
}

void TypingCommand::forwardDeleteKeyPressed(Ref<Document>&& document, OptionSet<Option> options, TextGranularity granularity)
{
    // FIXME: Forward delete in TextEdit appears to open and close a new typing command.
    if (granularity == TextGranularity::CharacterGranularity) {
        if (RefPtr<TypingCommand> lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
            updateSelectionIfDifferentFromCurrentSelection(lastTypingCommand.get(), document);
            lastTypingCommand->setIsAutocompletion(options.contains(Option::IsAutocompletion));
            lastTypingCommand->setCompositionType(TextCompositionType::None);
            lastTypingCommand->setShouldPreventSpellChecking(options.contains(Option::PreventSpellChecking));
            lastTypingCommand->forwardDeleteKeyPressed(granularity, options.contains(Option::AddsToKillRing));
            return;
        }
    }

    TypingCommand::create(WTFMove(document), Type::ForwardDeleteKey, emptyString(), options, granularity)->apply();
}

void TypingCommand::updateSelectionIfDifferentFromCurrentSelection(TypingCommand* typingCommand, Document& document)
{
    VisibleSelection currentSelection = document.selection().selection();
    if (currentSelection == typingCommand->endingSelection())
        return;

    typingCommand->setStartingSelection(currentSelection);
    typingCommand->setEndingSelection(currentSelection);
}

void TypingCommand::insertText(Ref<Document>&& document, const String& text, OptionSet<Option> options, TextCompositionType composition)
{
    if (!text.isEmpty())
        document->editor().updateMarkersForWordsAffectedByEditing(deprecatedIsSpaceOrNewline(text[0]));
    
    auto& selection = document->selection().selection();
    insertText(WTFMove(document), text, selection, options, composition);
}

// FIXME: We shouldn't need to take selectionForInsertion. It should be identical to FrameSelection's current selection.
void TypingCommand::insertText(Ref<Document>&& document, const String& text, const VisibleSelection& selectionForInsertion, OptionSet<Option> options, TextCompositionType compositionType)
{
    LOG(Editing, "TypingCommand::insertText (text %s)", text.utf8().data());

    VisibleSelection currentSelection = document->selection().selection();

    String newText = dispatchBeforeTextInsertedEvent(text, selectionForInsertion, compositionType == TextCompositionType::Pending);

    // Set the starting and ending selection appropriately if we are using a selection
    // that is different from the current selection.  In the future, we should change EditCommand
    // to deal with custom selections in a general way that can be used by all of the commands.
    if (RefPtr<TypingCommand> lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
        if (lastTypingCommand->endingSelection() != selectionForInsertion) {
            lastTypingCommand->setStartingSelection(selectionForInsertion);
            lastTypingCommand->setEndingSelection(selectionForInsertion);
        }

        lastTypingCommand->setIsAutocompletion(options.contains(Option::IsAutocompletion));
        lastTypingCommand->setCompositionType(compositionType);
        lastTypingCommand->setShouldRetainAutocorrectionIndicator(options.contains(Option::RetainAutocorrectionIndicator));
        lastTypingCommand->setShouldPreventSpellChecking(options.contains(Option::PreventSpellChecking));
#if HAVE(INLINE_PREDICTIONS)
        if (compositionType != TextCompositionType::None)
            lastTypingCommand->insertText(newText, options.contains(Option::SelectInsertedText));
        else
#endif
            lastTypingCommand->insertTextAndNotifyAccessibility(newText, options.contains(Option::SelectInsertedText));
        return;
    }

    RefPtr frame = document->frame();
    auto cmd = TypingCommand::create(WTFMove(document), Type::InsertText, newText, options, compositionType);
    applyTextInsertionCommand(frame.get(), cmd.get(), selectionForInsertion, currentSelection);
}

void TypingCommand::insertLineBreak(Ref<Document>&& document, OptionSet<Option> options)
{
    if (RefPtr lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
        lastTypingCommand->setIsAutocompletion(options.contains(Option::IsAutocompletion));
        lastTypingCommand->setCompositionType(TextCompositionType::None);
        lastTypingCommand->setShouldRetainAutocorrectionIndicator(options.contains(Option::RetainAutocorrectionIndicator));
        lastTypingCommand->insertLineBreakAndNotifyAccessibility();
        return;
    }

    TypingCommand::create(WTFMove(document), Type::InsertLineBreak, emptyString(), options)->apply();
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(Ref<Document>&& document)
{
    if (RefPtr lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
        lastTypingCommand->setIsAutocompletion(false);
        lastTypingCommand->setCompositionType(TextCompositionType::None);
        lastTypingCommand->insertParagraphSeparatorInQuotedContentAndNotifyAccessibility();
        return;
    }

    TypingCommand::create(WTFMove(document), Type::InsertParagraphSeparatorInQuotedContent)->apply();
}

void TypingCommand::insertParagraphSeparator(Ref<Document>&& document, OptionSet<Option> options)
{
    if (RefPtr lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
        lastTypingCommand->setIsAutocompletion(options.contains(Option::IsAutocompletion));
        lastTypingCommand->setCompositionType(TextCompositionType::None);
        lastTypingCommand->setShouldRetainAutocorrectionIndicator(options.contains(Option::RetainAutocorrectionIndicator));
        lastTypingCommand->insertParagraphSeparatorAndNotifyAccessibility();
        return;
    }

    TypingCommand::create(WTFMove(document), Type::InsertParagraphSeparator, emptyString(), options)->apply();
}

RefPtr<TypingCommand> TypingCommand::lastTypingCommandIfStillOpenForTyping(Document& document)
{
    RefPtr lastEditCommand = document.editor().lastEditCommand();
    if (!lastEditCommand || !lastEditCommand->isTypingCommand() || !static_cast<TypingCommand*>(lastEditCommand.get())->isOpenForMoreTyping())
        return nullptr;

    return static_pointer_cast<TypingCommand>(WTFMove(lastEditCommand));
}

bool TypingCommand::shouldDeferWillApplyCommandUntilAddingTypingCommand() const
{
    return !m_isHandlingInitialTypingCommand || editActionIsDeleteByTyping(editingAction());
}

void TypingCommand::closeTyping(Document& document)
{
    if (RefPtr lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document))
        lastTypingCommand->closeTyping();
}

#if PLATFORM(IOS_FAMILY)
void TypingCommand::ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping(Document& document, const VisibleSelection& newSelection)
{
    if (RefPtr lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document)) {
        lastTypingCommand->setEndingSelection(newSelection);
        lastTypingCommand->setEndingSelectionOnLastInsertCommand(newSelection);
    }
}
#endif

void TypingCommand::postTextStateChangeNotificationForDeletion(const VisibleSelection& selection)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    postTextStateChangeNotification(AXTextEditTypeDelete, AccessibilityObject::stringForVisiblePositionRange(selection), selection.start());
    VisiblePositionIndexRange range;
    range.startIndex.value = indexForVisiblePosition(selection.visibleStart(), range.startIndex.scope);
    range.endIndex.value = indexForVisiblePosition(selection.visibleEnd(), range.endIndex.scope);
    protectedComposition()->setRangeDeletedByUnapply(range);
}

bool TypingCommand::willApplyCommand()
{
    if (shouldDeferWillApplyCommandUntilAddingTypingCommand()) {
        // The TypingCommand will handle the willApplyCommand logic separately in TypingCommand::willAddTypingToOpenCommand.
        return true;
    }

    return CompositeEditCommand::willApplyCommand();
}

void TypingCommand::doApply()
{
    if (endingSelection().isNoneOrOrphaned())
        return;

    if (m_commandType == Type::DeleteKey) {
        if (m_commands.isEmpty())
            m_openedByBackwardDelete = true;
    }

    switch (m_commandType) {
    case Type::DeleteSelection:
        deleteSelection(m_smartDelete);
        return;
    case Type::DeleteKey:
        deleteKeyPressed(m_granularity, m_shouldAddToKillRing);
        return;
    case Type::ForwardDeleteKey:
        forwardDeleteKeyPressed(m_granularity, m_shouldAddToKillRing);
        return;
    case Type::InsertLineBreak:
        insertLineBreakAndNotifyAccessibility();
        return;
    case Type::InsertParagraphSeparator:
        insertParagraphSeparatorAndNotifyAccessibility();
        return;
    case Type::InsertParagraphSeparatorInQuotedContent:
        insertParagraphSeparatorInQuotedContentAndNotifyAccessibility();
        return;
    case Type::InsertText:
        insertTextAndNotifyAccessibility(m_textToInsert, m_selectInsertedText);
        return;
    }

    ASSERT_NOT_REACHED();
}

AtomString TypingCommand::inputEventTypeName() const
{
    return inputTypeNameForEditingAction(m_currentTypingEditAction);
}

bool TypingCommand::isInputMethodComposing() const
{
    return isInputMethodComposingForEditingAction(m_currentTypingEditAction);
}

bool TypingCommand::isBeforeInputEventCancelable() const
{
    return m_currentTypingEditAction != EditAction::TypingInsertPendingComposition && m_currentTypingEditAction != EditAction::TypingDeletePendingComposition;
}

String TypingCommand::inputEventData() const
{
    switch (m_currentTypingEditAction) {
    case EditAction::TypingInsertText:
    case EditAction::TypingInsertPendingComposition:
    case EditAction::TypingInsertFinalComposition:
        return m_currentTextToInsert;
    case EditAction::InsertReplacement:
        return isEditingTextAreaOrTextInput() ? m_currentTextToInsert : String();
    default:
        return CompositeEditCommand::inputEventData();
    }
}

RefPtr<DataTransfer> TypingCommand::inputEventDataTransfer() const
{
    if (m_currentTypingEditAction != EditAction::InsertReplacement || isEditingTextAreaOrTextInput())
        return nullptr;

    StringBuilder htmlText;
    MarkupAccumulator::appendCharactersReplacingEntities(htmlText, m_currentTextToInsert, 0, m_currentTextToInsert.length(), EntityMaskInHTMLPCDATA);
    return DataTransfer::createForInputEvent(m_currentTextToInsert, htmlText.toString());
}

void TypingCommand::didApplyCommand()
{
    // TypingCommands handle applied editing separately (see TypingCommand::typingAddedToOpenCommand).
    m_isHandlingInitialTypingCommand = false;
}

void TypingCommand::markMisspellingsAfterTyping(Type commandType)
{
    auto document = protectedDocument();
#if PLATFORM(MAC)
    if (!document->editor().isContinuousSpellCheckingEnabled()
        && !document->editor().isAutomaticQuoteSubstitutionEnabled()
        && !document->editor().isAutomaticLinkDetectionEnabled()
        && !document->editor().isAutomaticDashSubstitutionEnabled()
        && !document->editor().isAutomaticTextReplacementEnabled())
            return;
    if (document->editor().isHandlingAcceptedCandidate())
        return;
#else
    if (!document->editor().isContinuousSpellCheckingEnabled())
        return;
#endif
    // Take a look at the selection that results after typing and determine whether we need to spellcheck. 
    // Since the word containing the current selection is never marked, this does a check to
    // see if typing made a new word that is not in the current selection. Basically, you
    // get this by being at the end of a word and typing a space.    
    VisiblePosition start(endingSelection().start(), endingSelection().affinity());
    VisiblePosition previous = start.previous();
    if (previous.isNotNull()) {
#if !PLATFORM(IOS_FAMILY)
        VisiblePosition p1 = startOfWord(previous, WordSide::LeftWordIfOnBoundary);
        VisiblePosition p2 = startOfWord(start, WordSide::LeftWordIfOnBoundary);
        if (p1 != p2) {
            auto range = makeSimpleRange(p1, p2);
            String trimmedPreviousWord;
            if (range && (commandType == TypingCommand::Type::InsertText || commandType == TypingCommand::Type::InsertLineBreak || commandType == TypingCommand::Type::InsertParagraphSeparator || commandType == TypingCommand::Type::InsertParagraphSeparatorInQuotedContent))
                trimmedPreviousWord = plainText(*range).trim(deprecatedIsSpaceOrNewline);
            document->editor().markMisspellingsAfterTypingToWord(p1, endingSelection(), !trimmedPreviousWord.isEmpty());
        } else if (commandType == TypingCommand::Type::InsertText)
            document->editor().startAlternativeTextUITimer();
#else
        UNUSED_PARAM(commandType);
        // If this bug gets fixed, this PLATFORM(IOS_FAMILY) code could be removed:
        // <rdar://problem/7259611> Word boundary code on iPhone gives different results than desktop
        WordSide startWordSide = WordSide::LeftWordIfOnBoundary;
        char32_t c = previous.characterAfter();
        // FIXME: VisiblePosition::characterAfter() and characterBefore() do not emit newlines the same
        // way as TextIterator, so we do an isEndOfParagraph check here.
        if (deprecatedIsSpaceOrNewline(c) || c == noBreakSpace || isEndOfParagraph(previous)) {
            startWordSide = WordSide::RightWordIfOnBoundary;
        }
        VisiblePosition p1 = startOfWord(previous, startWordSide);
        VisiblePosition p2 = startOfWord(start, startWordSide);
        if (p1 != p2)
            document->editor().markMisspellingsAfterTypingToWord(p1, endingSelection(), false);
#endif // !PLATFORM(IOS_FAMILY)
    }
}

bool TypingCommand::willAddTypingToOpenCommand(Type commandType, TextGranularity granularity, const String& text, const std::optional<SimpleRange>& range)
{
    m_currentTextToInsert = text;
    m_currentTypingEditAction = editActionForTypingCommand(commandType, granularity, m_compositionType, m_isAutocompletion);

    if (!shouldDeferWillApplyCommandUntilAddingTypingCommand())
        return true;

    if (!range || isEditingTextAreaOrTextInput())
        return protectedDocument()->editor().willApplyEditing(*this, CompositeEditCommand::targetRangesForBindings());

    return protectedDocument()->editor().willApplyEditing(*this, { 1, StaticRange::create(*range) });
}

void TypingCommand::typingAddedToOpenCommand(Type commandTypeForAddedTyping)
{
    Ref document = this->document();
    RefPtr protectedFrame { document->frame() };

    updatePreservesTypingStyle(commandTypeForAddedTyping);

#if PLATFORM(COCOA)
    document->editor().appliedEditing(*this);
    // Since the spellchecking code may also perform corrections and other replacements, it should happen after the typing changes.
    if (!m_shouldPreventSpellChecking)
        markMisspellingsAfterTyping(commandTypeForAddedTyping);
#else
    // The old spellchecking code requires that checking be done first, to prevent issues like that in 6864072, where <doesn't> is marked as misspelled.
    markMisspellingsAfterTyping(commandTypeForAddedTyping);
    document->editor().appliedEditing(*this);
#endif
}

void TypingCommand::insertText(const String &text, bool selectInsertedText)
{
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    TypingCommandLineOperation operation(*this, selectInsertedText, text);
    forEachLineInString(text, operation);
}

void TypingCommand::insertTextAndNotifyAccessibility(const String& text, bool selectInsertedText)
{
    LOG(Editing, "TypingCommand %p insertTextAndNotifyAccessibility (text %s, selectInsertedText %d)", this, text.utf8().data(), selectInsertedText);

    Ref document = this->document();
    AccessibilityReplacedText replacedText(document->selection().selection());
    insertText(text, selectInsertedText);
    replacedText.postTextStateChangeNotification(document->existingAXObjectCache(), AXTextEditTypeTyping, text, document->selection().selection());
    protectedComposition()->setRangeDeletedByUnapply(replacedText.replacedRange());
}

void TypingCommand::insertTextRunWithoutNewlines(const String& text, bool selectInsertedText)
{
    if (!willAddTypingToOpenCommand(Type::InsertText, TextGranularity::CharacterGranularity, text))
        return;

    auto command = InsertTextCommand::create(protectedDocument(), text, selectInsertedText,
        m_compositionType == TextCompositionType::None ? InsertTextCommand::RebalanceLeadingAndTrailingWhitespaces : InsertTextCommand::RebalanceAllWhitespaces, EditAction::TypingInsertText);

    applyCommandToComposite(WTFMove(command), endingSelection());
    typingAddedToOpenCommand(Type::InsertText);
}

void TypingCommand::insertLineBreak()
{
    if (!canAppendNewLineFeedToSelection(endingSelection()))
        return;

    if (!willAddTypingToOpenCommand(Type::InsertLineBreak, TextGranularity::LineGranularity))
        return;

    applyCommandToComposite(InsertLineBreakCommand::create(protectedDocument()));
    typingAddedToOpenCommand(Type::InsertLineBreak);
}

void TypingCommand::insertLineBreakAndNotifyAccessibility()
{
    Ref document = this->document();
    AccessibilityReplacedText replacedText(document->selection().selection());
    insertLineBreak();
    replacedText.postTextStateChangeNotification(document->existingAXObjectCache(), AXTextEditTypeTyping, "\n"_s, document->selection().selection());
    protectedComposition()->setRangeDeletedByUnapply(replacedText.replacedRange());
}

void TypingCommand::insertParagraphSeparator()
{
    if (!canAppendNewLineFeedToSelection(endingSelection()))
        return;

    if (!willAddTypingToOpenCommand(Type::InsertParagraphSeparator, TextGranularity::ParagraphGranularity))
        return;

    applyCommandToComposite(InsertParagraphSeparatorCommand::create(protectedDocument(), false, false, EditAction::TypingInsertParagraph));
    typingAddedToOpenCommand(Type::InsertParagraphSeparator);
}

void TypingCommand::insertParagraphSeparatorAndNotifyAccessibility()
{
    Ref document = this->document();
    AccessibilityReplacedText replacedText(document->selection().selection());
    insertParagraphSeparator();
    replacedText.postTextStateChangeNotification(document->existingAXObjectCache(), AXTextEditTypeTyping, "\n"_s, document->selection().selection());
    protectedComposition()->setRangeDeletedByUnapply(replacedText.replacedRange());
}

void TypingCommand::insertParagraphSeparatorInQuotedContent()
{
    if (!willAddTypingToOpenCommand(Type::InsertParagraphSeparatorInQuotedContent, TextGranularity::ParagraphGranularity))
        return;

    // If the selection starts inside a table, just insert the paragraph separator normally
    // Breaking the blockquote would also break apart the table, which is unecessary when inserting a newline
    if (enclosingNodeOfType(endingSelection().start(), &isTableStructureNode)) {
        insertParagraphSeparator();
        return;
    }

    applyCommandToComposite(BreakBlockquoteCommand::create(protectedDocument()));
    typingAddedToOpenCommand(Type::InsertParagraphSeparatorInQuotedContent);
}

void TypingCommand::insertParagraphSeparatorInQuotedContentAndNotifyAccessibility()
{
    Ref document = this->document();
    AccessibilityReplacedText replacedText(document->selection().selection());
    insertParagraphSeparatorInQuotedContent();
    replacedText.postTextStateChangeNotification(document->existingAXObjectCache(), AXTextEditTypeTyping, "\n"_s, document->selection().selection());
    protectedComposition()->setRangeDeletedByUnapply(replacedText.replacedRange());
}

bool TypingCommand::makeEditableRootEmpty()
{
    RefPtr root = endingSelection().rootEditableElement();
    if (!root || !root->firstChild())
        return false;

    if (root->firstChild() == root->lastChild() && root->firstChild()->hasTagName(brTag)) {
        // If there is a single child and it could be a placeholder, leave it alone.
        if (root->renderer() && root->renderer()->isRenderBlockFlow())
            return false;
    }

    while (RefPtr child = root->firstChild())
        removeNode(*child);

    addBlockPlaceholderIfNeeded(root.get());
    setEndingSelection(VisibleSelection(firstPositionInNode(root.get()), Affinity::Downstream, endingSelection().isDirectional()));

    return true;
}

void TypingCommand::deleteKeyPressed(TextGranularity granularity, bool shouldAddToKillRing)
{
    Ref document = this->document();
    RefPtr protectedFrame { document->frame() };

    document->editor().updateMarkersForWordsAffectedByEditing(false);

    VisibleSelection selectionToDelete;
    VisibleSelection selectionAfterUndo;
    bool expandForSpecialElements = false;

    ASSERT(endingSelection().isCaretOrRange());

    if (endingSelection().isRange()) {
        selectionToDelete = endingSelection();
        selectionAfterUndo = selectionToDelete;
        expandForSpecialElements = true;
    } else {
        // After breaking out of an empty mail blockquote, we still want continue with the deletion
        // so actual content will get deleted, and not just the quote style.
        if (breakOutOfEmptyMailBlockquotedParagraph())
            typingAddedToOpenCommand(Type::DeleteKey);

        m_smartDelete = false;

        FrameSelection selection;
        selection.setSelection(endingSelection());
        selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, granularity);
        if (shouldAddToKillRing && selection.isCaret() && granularity != TextGranularity::CharacterGranularity)
            selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::CharacterGranularity);

        const VisiblePosition& visibleStart = endingSelection().visibleStart();
        const VisiblePosition& previousPosition = visibleStart.previous(CannotCrossEditingBoundary);
        RefPtr enclosingTableCell = enclosingNodeOfType(visibleStart.deepEquivalent(), &isTableCell);
        RefPtr enclosingTableCellForPreviousPosition = enclosingNodeOfType(previousPosition.deepEquivalent(), &isTableCell);
        if (previousPosition.isNull() || enclosingTableCell != enclosingTableCellForPreviousPosition) {
            // When the caret is at the start of the editable area in an empty list item, break out of the list item.
            if (auto deleteListSelection = shouldBreakOutOfEmptyListItem(); !deleteListSelection.isNone()) {
                if (willAddTypingToOpenCommand(Type::DeleteKey, granularity, { }, deleteListSelection.firstRange())) {
                    breakOutOfEmptyListItem();
                    typingAddedToOpenCommand(Type::DeleteKey);
                }
                return;
            }
        }
        if (previousPosition.isNull()) {
            // When there are no visible positions in the editing root, delete its entire contents.
            // FIXME: Dispatch a `beforeinput` event here and bail if preventDefault() was invoked.
            if (visibleStart.next(CannotCrossEditingBoundary).isNull() && makeEditableRootEmpty()) {
                typingAddedToOpenCommand(Type::DeleteKey);
                return;
            }
        }

        // If we have a caret selection at the beginning of a cell, we have nothing to do.
        if (enclosingTableCell && visibleStart == firstPositionInNode(enclosingTableCell.get()))
            return;

        // If the caret is at the start of a paragraph after a table, move content into the last table cell.
        if (isStartOfParagraph(visibleStart) && isFirstPositionAfterTable(visibleStart.previous(CannotCrossEditingBoundary))) {
            // Unless the caret is just before a table.  We don't want to move a table into the last table cell.
            if (isLastPositionBeforeTable(visibleStart))
                return;
            // Extend the selection backward into the last cell, then deletion will handle the move.
            selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, granularity);
        // If the caret is just after a table, select the table and don't delete anything.
        } else if (RefPtr table = isFirstPositionAfterTable(visibleStart)) {
            setEndingSelection(VisibleSelection(positionBeforeNode(table.get()), endingSelection().start(), Affinity::Downstream, endingSelection().isDirectional()));
            typingAddedToOpenCommand(Type::DeleteKey);
            return;
        }

        selectionToDelete = selection.selection();

        if (granularity == TextGranularity::CharacterGranularity && selectionToDelete.end().containerNode() == selectionToDelete.start().containerNode()
            && selectionToDelete.end().computeOffsetInContainerNode() - selectionToDelete.start().computeOffsetInContainerNode() > 1) {
            // If there are multiple Unicode code points to be deleted, adjust the range to match platform conventions.
            selectionToDelete.setWithoutValidation(selectionToDelete.end(), selectionToDelete.end().previous(BackwardDeletion));
        }

        if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start())
            selectionAfterUndo = selectionToDelete;
        else
            // It's a little tricky to compute what the starting selection would have been in the original document.
            // We can't let the VisibleSelection class's validation kick in or it'll adjust for us based on
            // the current state of the document and we'll get the wrong result.
            selectionAfterUndo.setWithoutValidation(startingSelection().end(), selectionToDelete.extent());
    }
    
    ASSERT(!selectionToDelete.isNone());
    if (selectionToDelete.isNone()) {
#if PLATFORM(IOS_FAMILY)
        // Workaround for this bug:
        // <rdar://problem/4653755> UIKit text widgets should use WebKit editing API to manipulate text
        setEndingSelection(document->selection().selection());
        closeTyping(document);
#endif
        return;
    }

    if (selectionToDelete.isCaret() || !document->selection().shouldDeleteSelection(selectionToDelete))
        return;

    if (!willAddTypingToOpenCommand(Type::DeleteKey, granularity, { }, selectionToDelete.firstRange()))
        return;

    if (shouldAddToKillRing)
        document->editor().addRangeToKillRing(*selectionToDelete.toNormalizedRange(), Editor::KillRingInsertionMode::PrependText);

    // Post the accessibility notification before actually deleting the content while selectionToDelete is still valid
    postTextStateChangeNotificationForDeletion(selectionToDelete);

    // Make undo select everything that has been deleted, unless an undo will undo more than just this deletion.
    // FIXME: This behaves like TextEdit except for the case where you open with text insertion and then delete
    // more text than you insert.  In that case all of the text that was around originally should be selected.
    if (m_openedByBackwardDelete)
        setStartingSelection(selectionAfterUndo);
    CompositeEditCommand::deleteSelection(selectionToDelete, m_smartDelete, /* mergeBlocksAfterDelete*/ true, /* replace*/ false, expandForSpecialElements, /*sanitizeMarkup*/ true);
    setSmartDelete(false);
    typingAddedToOpenCommand(Type::DeleteKey);
}

void TypingCommand::forwardDeleteKeyPressed(TextGranularity granularity, bool shouldAddToKillRing)
{
    Ref document = this->document();
    RefPtr protectedFrame { document->frame() };

    document->editor().updateMarkersForWordsAffectedByEditing(false);

    VisibleSelection selectionToDelete;
    VisibleSelection selectionAfterUndo;
    bool expandForSpecialElements = false;

    ASSERT(endingSelection().isCaretOrRange());

    if (endingSelection().isRange()) {
        selectionToDelete = endingSelection();
        selectionAfterUndo = selectionToDelete;
        expandForSpecialElements = true;
    } else {
        m_smartDelete = false;

        // Handle delete at beginning-of-block case.
        // Do nothing in the case that the caret is at the start of a
        // root editable element or at the start of a document.
        FrameSelection selection;
        selection.setSelection(endingSelection());
        selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, granularity);
        if (selection.isNone())
            return;
        if (shouldAddToKillRing && selection.isCaret() && granularity != TextGranularity::CharacterGranularity)
            selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::CharacterGranularity);

        Position downstreamEnd = endingSelection().end().downstream();
        VisiblePosition visibleEnd = endingSelection().visibleEnd();
        auto enclosingTableCell = enclosingNodeOfType(visibleEnd.deepEquivalent(), &isTableCell);
        if (enclosingTableCell && visibleEnd == lastPositionInNode(enclosingTableCell.get()))
            return;
        if (visibleEnd == endOfParagraph(visibleEnd))
            downstreamEnd = visibleEnd.next(CannotCrossEditingBoundary).deepEquivalent().downstream();
        // When deleting tables: Select the table first, then perform the deletion
        if (downstreamEnd.containerNode() && downstreamEnd.containerNode()->renderer() && downstreamEnd.containerNode()->renderer()->isRenderTable()
            && downstreamEnd.computeOffsetInContainerNode() <= caretMinOffset(*downstreamEnd.containerNode())) {
            setEndingSelection(VisibleSelection(endingSelection().end(), positionAfterNode(downstreamEnd.containerNode()), Affinity::Downstream, endingSelection().isDirectional()));
            typingAddedToOpenCommand(Type::ForwardDeleteKey);
            return;
        }

        // deleting to end of paragraph when at end of paragraph needs to merge the next paragraph (if any)
        if (granularity == TextGranularity::ParagraphBoundary && selection.selection().isCaret() && isEndOfParagraph(selection.selection().visibleEnd()))
            selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::CharacterGranularity);

        selectionToDelete = selection.selection();
        if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start())
            selectionAfterUndo = selectionToDelete;
        else {
            // It's a little tricky to compute what the starting selection would have been in the original document.
            // We can't let the VisibleSelection class's validation kick in or it'll adjust for us based on
            // the current state of the document and we'll get the wrong result.
            Position extent = startingSelection().end();
            if (extent.containerNode() != selectionToDelete.end().containerNode())
                extent = selectionToDelete.extent();
            else {
                int extraCharacters;
                if (selectionToDelete.start().containerNode() == selectionToDelete.end().containerNode())
                    extraCharacters = selectionToDelete.end().computeOffsetInContainerNode() - selectionToDelete.start().computeOffsetInContainerNode();
                else
                    extraCharacters = selectionToDelete.end().computeOffsetInContainerNode();
                extent = Position(extent.containerNode(), extent.computeOffsetInContainerNode() + extraCharacters, Position::PositionIsOffsetInAnchor);
            }
            selectionAfterUndo.setWithoutValidation(startingSelection().start(), extent);
        }
    }
    
    ASSERT(!selectionToDelete.isNone());
    if (selectionToDelete.isNone()) {
#if PLATFORM(IOS_FAMILY)
        // Workaround for this bug:
        // <rdar://problem/4653755> UIKit text widgets should use WebKit editing API to manipulate text
        setEndingSelection(document->selection().selection());
        closeTyping(document);
#endif
        return;
    }
    
    if (selectionToDelete.isCaret() || !document->selection().shouldDeleteSelection(selectionToDelete))
        return;

    if (!willAddTypingToOpenCommand(Type::ForwardDeleteKey, granularity, { }, selectionToDelete.firstRange()))
        return;

    // Post the accessibility notification before actually deleting the content while selectionToDelete is still valid
    postTextStateChangeNotificationForDeletion(selectionToDelete);

    if (shouldAddToKillRing)
        document->editor().addRangeToKillRing(*selectionToDelete.toNormalizedRange(), Editor::KillRingInsertionMode::AppendText);
    // make undo select what was deleted
    setStartingSelection(selectionAfterUndo);
    CompositeEditCommand::deleteSelection(selectionToDelete, m_smartDelete, /* mergeBlocksAfterDelete*/ true, /* replace*/ false, expandForSpecialElements, /*sanitizeMarkup*/ true);
    setSmartDelete(false);
    typingAddedToOpenCommand(Type::ForwardDeleteKey);
}

void TypingCommand::deleteSelection(bool smartDelete)
{
    if (!willAddTypingToOpenCommand(Type::DeleteSelection, TextGranularity::CharacterGranularity))
        return;

    CompositeEditCommand::deleteSelection(smartDelete);
    typingAddedToOpenCommand(Type::DeleteSelection);
}

#if PLATFORM(IOS_FAMILY)
class FriendlyEditCommand : public EditCommand {
public:
    void setEndingSelection(const VisibleSelection& selection)
    {
        EditCommand::setEndingSelection(selection);
    }
};

void TypingCommand::setEndingSelectionOnLastInsertCommand(const VisibleSelection& selection)
{
    if (!m_commands.isEmpty()) {
        RefPtr lastCommand = m_commands.last().get();
        if (lastCommand->isInsertTextCommand())
            static_cast<FriendlyEditCommand*>(lastCommand.get())->setEndingSelection(selection);
    }
}
#endif

void TypingCommand::updatePreservesTypingStyle(Type commandType)
{
    switch (commandType) {
    case Type::DeleteSelection:
    case Type::DeleteKey:
    case Type::ForwardDeleteKey:
    case Type::InsertParagraphSeparator:
    case Type::InsertLineBreak:
        m_preservesTypingStyle = true;
        return;
    case Type::InsertParagraphSeparatorInQuotedContent:
    case Type::InsertText:
        m_preservesTypingStyle = false;
        return;
    }
    ASSERT_NOT_REACHED();
    m_preservesTypingStyle = false;
}

bool TypingCommand::isTypingCommand() const
{
    return true;
}

} // namespace WebCore
