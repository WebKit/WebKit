/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#ifndef Editor_h
#define Editor_h

#include "Color.h"
#include "DataTransferAccessPolicy.h"
#include "DictationAlternative.h"
#include "DocumentMarker.h"
#include "EditAction.h"
#include "EditingBehavior.h"
#include "EditingStyle.h"
#include "EditorInsertAction.h"
#include "FindOptions.h"
#include "FrameSelection.h"
#include "TextChecking.h"
#include "TextIteratorBehavior.h"
#include "VisibleSelection.h"
#include "WritingDirection.h"
#include <memory>

#if PLATFORM(COCOA)
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSDictionary;
#endif

namespace WebCore {

class AlternativeTextController;
class ArchiveResource;
class DataTransfer;
class CompositeEditCommand;
class DeleteButtonController;
class EditCommand;
class EditCommandComposition;
class EditorClient;
class EditorInternalCommand;
class Frame;
class HTMLElement;
class HitTestResult;
class KillRing;
class Pasteboard;
class SharedBuffer;
class SimpleFontData;
class SpellCheckRequest;
class SpellChecker;
class StyleProperties;
class Text;
class TextCheckerClient;
class TextEvent;

struct PasteboardPlainText;
struct PasteboardURL;
struct TextCheckingResult;

struct CompositionUnderline {
    CompositionUnderline() 
        : startOffset(0), endOffset(0), thick(false) { }
    CompositionUnderline(unsigned s, unsigned e, const Color& c, bool t) 
        : startOffset(s), endOffset(e), color(c), thick(t) { }
    unsigned startOffset;
    unsigned endOffset;
    Color color;
    bool thick;
};

enum EditorCommandSource { CommandFromMenuOrKeyBinding, CommandFromDOM, CommandFromDOMWithUserInterface };
enum EditorParagraphSeparator { EditorParagraphSeparatorIsDiv, EditorParagraphSeparatorIsP };

class Editor {
public:
    explicit Editor(Frame&);
    ~Editor();

    EditorClient* client() const;
    TextCheckerClient* textChecker() const;

    CompositeEditCommand* lastEditCommand() { return m_lastEditCommand.get(); }

    void handleKeyboardEvent(KeyboardEvent*);
    void handleInputMethodKeydown(KeyboardEvent*);
    bool handleTextEvent(TextEvent*);

    bool canEdit() const;
    bool canEditRichly() const;

    bool canDHTMLCut();
    bool canDHTMLCopy();
    bool canDHTMLPaste();
    bool tryDHTMLCopy();
    bool tryDHTMLCut();
    bool tryDHTMLPaste();

    bool canCut() const;
    bool canCopy() const;
    bool canPaste() const;
    bool canDelete() const;
    bool canSmartCopyOrDelete();

    void cut();
    void copy();
    void paste();
    void paste(Pasteboard&);
    void pasteAsPlainText();
    void performDelete();

    void copyURL(const URL&, const String& title);
    void copyURL(const URL&, const String& title, Pasteboard&);
#if !PLATFORM(IOS)
    void copyImage(const HitTestResult&);
#endif

    String readPlainTextFromPasteboard(Pasteboard&);

    void indent();
    void outdent();
    void transpose();

    bool shouldInsertFragment(PassRefPtr<DocumentFragment>, PassRefPtr<Range>, EditorInsertAction);
    bool shouldInsertText(const String&, Range*, EditorInsertAction) const;
    bool shouldDeleteRange(Range*) const;
    bool shouldApplyStyle(StyleProperties*, Range*);

    void respondToChangedContents(const VisibleSelection& endingSelection);

    bool selectionStartHasStyle(CSSPropertyID, const String& value) const;
    TriState selectionHasStyle(CSSPropertyID, const String& value) const;
    String selectionStartCSSPropertyValue(CSSPropertyID);
    
    TriState selectionUnorderedListState() const;
    TriState selectionOrderedListState() const;
    PassRefPtr<Node> insertOrderedList();
    PassRefPtr<Node> insertUnorderedList();
    bool canIncreaseSelectionListLevel();
    bool canDecreaseSelectionListLevel();
    PassRefPtr<Node> increaseSelectionListLevel();
    PassRefPtr<Node> increaseSelectionListLevelOrdered();
    PassRefPtr<Node> increaseSelectionListLevelUnordered();
    void decreaseSelectionListLevel();
   
    void removeFormattingAndStyle();

    void clearLastEditCommand();
#if PLATFORM(IOS)
    void ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping();
#endif

    bool deleteWithDirection(SelectionDirection, TextGranularity, bool killRing, bool isTypingAction);
    void deleteSelectionWithSmartDelete(bool smartDelete);
#if PLATFORM(IOS)
    void clearText();
    void removeUnchangeableStyles();
#endif
    
    bool dispatchCPPEvent(const AtomicString&, DataTransferAccessPolicy);
    
    void applyStyle(StyleProperties*, EditAction = EditActionUnspecified);
    void applyParagraphStyle(StyleProperties*, EditAction = EditActionUnspecified);
    void applyStyleToSelection(StyleProperties*, EditAction);
    void applyParagraphStyleToSelection(StyleProperties*, EditAction);

    void appliedEditing(PassRefPtr<CompositeEditCommand>);
    void unappliedEditing(PassRefPtr<EditCommandComposition>);
    void reappliedEditing(PassRefPtr<EditCommandComposition>);
    void unappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction);

    void setShouldStyleWithCSS(bool flag) { m_shouldStyleWithCSS = flag; }
    bool shouldStyleWithCSS() const { return m_shouldStyleWithCSS; }

    class Command {
    public:
        Command();
        Command(const EditorInternalCommand*, EditorCommandSource, PassRefPtr<Frame>);

        bool execute(const String& parameter = String(), Event* triggeringEvent = 0) const;
        bool execute(Event* triggeringEvent) const;

        bool isSupported() const;
        bool isEnabled(Event* triggeringEvent = 0) const;

        TriState state(Event* triggeringEvent = 0) const;
        String value(Event* triggeringEvent = 0) const;

        bool isTextInsertion() const;

    private:
        const EditorInternalCommand* m_command;
        EditorCommandSource m_source;
        RefPtr<Frame> m_frame;
    };
    Command command(const String& commandName); // Command source is CommandFromMenuOrKeyBinding.
    Command command(const String& commandName, EditorCommandSource);
    static bool commandIsSupportedFromMenuOrKeyBinding(const String& commandName); // Works without a frame.

    bool insertText(const String&, Event* triggeringEvent);
    bool insertTextForConfirmedComposition(const String& text);
    bool insertDictatedText(const String&, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent);
    bool insertTextWithoutSendingTextEvent(const String&, bool selectInsertedText, TextEvent* triggeringEvent);
    bool insertLineBreak();
    bool insertParagraphSeparator();

    bool isContinuousSpellCheckingEnabled() const;
    void toggleContinuousSpellChecking();
    bool isGrammarCheckingEnabled();
    void toggleGrammarChecking();
    void ignoreSpelling();
    void learnSpelling();
    int spellCheckerDocumentTag();
    bool isSelectionUngrammatical();
    String misspelledSelectionString() const;
    String misspelledWordAtCaretOrRange(Node* clickedNode) const;
    Vector<String> guessesForMisspelledWord(const String&) const;
    Vector<String> guessesForMisspelledOrUngrammatical(bool& misspelled, bool& ungrammatical);
    bool isSpellCheckingEnabledInFocusedNode() const;
    bool isSpellCheckingEnabledFor(Node*) const;
    void markMisspellingsAfterTypingToWord(const VisiblePosition &wordStart, const VisibleSelection& selectionAfterTyping, bool doReplacement);
    void markMisspellings(const VisibleSelection&, RefPtr<Range>& firstMisspellingRange);
    void markBadGrammar(const VisibleSelection&);
    void markMisspellingsAndBadGrammar(const VisibleSelection& spellingSelection, bool markGrammar, const VisibleSelection& grammarSelection);
    void markAndReplaceFor(PassRefPtr<SpellCheckRequest>, const Vector<TextCheckingResult>&);

    bool isOverwriteModeEnabled() const { return m_overwriteModeEnabled; }
    void toggleOverwriteModeEnabled();

    void markAllMisspellingsAndBadGrammarInRanges(TextCheckingTypeMask, Range* spellingRange, Range* grammarRange);
#if PLATFORM(IOS)
    NO_RETURN_DUE_TO_ASSERT
#endif
    void changeBackToReplacedString(const String& replacedString);

#if !PLATFORM(IOS)
    void advanceToNextMisspelling(bool startBeforeSelection = false);
#endif // !PLATFORM(IOS)
    void showSpellingGuessPanel();
    bool spellingPanelIsShowing();

    bool shouldBeginEditing(Range*);
    bool shouldEndEditing(Range*);

    void clearUndoRedoOperations();
    bool canUndo();
    void undo();
    bool canRedo();
    void redo();

    void didBeginEditing();
    void didEndEditing();
    void willWriteSelectionToPasteboard(PassRefPtr<Range>);
    void didWriteSelectionToPasteboard();

    void showFontPanel();
    void showStylesPanel();
    void showColorPanel();
    void toggleBold();
    void toggleUnderline();
    void setBaseWritingDirection(WritingDirection);

    // smartInsertDeleteEnabled and selectTrailingWhitespaceEnabled are 
    // mutually exclusive, meaning that enabling one will disable the other.
    bool smartInsertDeleteEnabled();
    bool isSelectTrailingWhitespaceEnabled();
    
    bool hasBidiSelection() const;

    // international text input composition
    bool hasComposition() const { return m_compositionNode; }
    void setComposition(const String&, const Vector<CompositionUnderline>&, unsigned selectionStart, unsigned selectionEnd);
    void confirmComposition();
    void confirmComposition(const String&); // if no existing composition, replaces selection
    void cancelComposition();
    bool cancelCompositionIfSelectionIsInvalid();
    PassRefPtr<Range> compositionRange() const;
    bool getCompositionSelection(unsigned& selectionStart, unsigned& selectionEnd) const;

    // getting international text input composition state (for use by InlineTextBox)
    Text* compositionNode() const { return m_compositionNode.get(); }
    unsigned compositionStart() const { return m_compositionStart; }
    unsigned compositionEnd() const { return m_compositionEnd; }
    bool compositionUsesCustomUnderlines() const { return !m_customCompositionUnderlines.isEmpty(); }
    const Vector<CompositionUnderline>& customCompositionUnderlines() const { return m_customCompositionUnderlines; }

    void setIgnoreCompositionSelectionChange(bool);
    bool ignoreCompositionSelectionChange() const { return m_ignoreCompositionSelectionChange; }

    void setStartNewKillRingSequence(bool);

    PassRefPtr<Range> rangeForPoint(const IntPoint& windowPoint);

    void clear();

    VisibleSelection selectionForCommand(Event*);

    KillRing& killRing() const { return *m_killRing; }
    SpellChecker& spellChecker() const { return *m_spellChecker; }

    EditingBehavior behavior() const;

    PassRefPtr<Range> selectedRange();

#if PLATFORM(IOS)
    void confirmMarkedText();
    void setTextAsChildOfElement(const String&, Element*);
    void setTextAlignmentForChangedBaseWritingDirection(WritingDirection);
    void insertDictationPhrases(PassOwnPtr<Vector<Vector<String> > > dictationPhrases, RetainPtr<id> metadata);
    void setDictationPhrasesAsChildOfElement(PassOwnPtr<Vector<Vector<String> > > dictationPhrases, RetainPtr<id> metadata, Element* element);
#endif
    
    void addToKillRing(Range*, bool prepend);

    void startAlternativeTextUITimer();
    // If user confirmed a correction in the correction panel, correction has non-zero length, otherwise it means that user has dismissed the panel.
    void handleAlternativeTextUIResult(const String& correction);
    void dismissCorrectionPanelAsIgnored();

    void pasteAsFragment(PassRefPtr<DocumentFragment>, bool smartReplace, bool matchStyle);
    void pasteAsPlainText(const String&, bool smartReplace);

    // This is only called on the mac where paste is implemented primarily at the WebKit level.
    void pasteAsPlainTextBypassingDHTML();
 
    void clearMisspellingsAndBadGrammar(const VisibleSelection&);
    void markMisspellingsAndBadGrammar(const VisibleSelection&);

    Node* findEventTargetFrom(const VisibleSelection& selection) const;

    String selectedText() const;
    String selectedTextForDataTransfer() const;
    bool findString(const String&, FindOptions);

    PassRefPtr<Range> rangeOfString(const String&, Range*, FindOptions);
    PassRefPtr<Range> findStringAndScrollToVisible(const String&, Range*, FindOptions);

    const VisibleSelection& mark() const; // Mark, to be used as emacs uses it.
    void setMark(const VisibleSelection&);

    void computeAndSetTypingStyle(StyleProperties* , EditAction = EditActionUnspecified);
    void applyEditingStyleToBodyElement() const;
    void applyEditingStyleToElement(Element*) const;

    IntRect firstRectForRange(Range*) const;

    void respondToChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions);
    void updateEditorUINowIfScheduled();
    bool shouldChangeSelection(const VisibleSelection& oldSelection, const VisibleSelection& newSelection, EAffinity, bool stillSelecting) const;
    unsigned countMatchesForText(const String&, Range*, FindOptions, unsigned limit, bool markMatches, Vector<RefPtr<Range>>*);
    bool markedTextMatchesAreHighlighted() const;
    void setMarkedTextMatchesAreHighlighted(bool);

    void textFieldDidBeginEditing(Element*);
    void textFieldDidEndEditing(Element*);
    void textDidChangeInTextField(Element*);
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    void textWillBeDeletedInTextField(Element* input);
    void textDidChangeInTextArea(Element*);
    WritingDirection baseWritingDirectionForSelectionStart() const;

    void replaceSelectionWithFragment(PassRefPtr<DocumentFragment>, bool selectReplacement, bool smartReplace, bool matchStyle);
    void replaceSelectionWithText(const String&, bool selectReplacement, bool smartReplace);
    bool selectionStartHasMarkerFor(DocumentMarker::MarkerType, int from, int length) const;
    void updateMarkersForWordsAffectedByEditing(bool onlyHandleWordsContainingSelection);
    void deletedAutocorrectionAtPosition(const Position&, const String& originalString);
    
    void simplifyMarkup(Node* startNode, Node* endNode);

    void deviceScaleFactorChanged();

    EditorParagraphSeparator defaultParagraphSeparator() const { return m_defaultParagraphSeparator; }
    void setDefaultParagraphSeparator(EditorParagraphSeparator separator) { m_defaultParagraphSeparator = separator; }
    Vector<String> dictationAlternativesForMarker(const DocumentMarker*);
    void applyDictationAlternativelternative(const String& alternativeString);

    PassRefPtr<Range> avoidIntersectionWithDeleteButtonController(const Range*) const;
    VisibleSelection avoidIntersectionWithDeleteButtonController(const VisibleSelection&) const;

#if USE(APPKIT)
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel();
    bool substitutionsPanelIsShowing();
    void toggleSmartInsertDelete();
    bool isAutomaticQuoteSubstitutionEnabled();
    void toggleAutomaticQuoteSubstitution();
    bool isAutomaticLinkDetectionEnabled();
    void toggleAutomaticLinkDetection();
    bool isAutomaticDashSubstitutionEnabled();
    void toggleAutomaticDashSubstitution();
    bool isAutomaticTextReplacementEnabled();
    void toggleAutomaticTextReplacement();
    bool isAutomaticSpellingCorrectionEnabled();
    void toggleAutomaticSpellingCorrection();
#endif

#if ENABLE(DELETION_UI)
    DeleteButtonController& deleteButtonController() const { return *m_deleteButtonController; }
#endif

#if PLATFORM(COCOA)
    bool insertParagraphSeparatorInQuotedContent();
    const SimpleFontData* fontForSelection(bool&) const;
    NSDictionary* fontAttributesForSelectionStart() const;
    String stringSelectionForPasteboard();
    String stringSelectionForPasteboardWithImageAltText();
    PassRefPtr<DocumentFragment> webContentFromPasteboard(Pasteboard&, Range& context, bool allowPlainText, bool& chosePlainText);
#if !PLATFORM(IOS)
    bool canCopyExcludingStandaloneImages();
    void takeFindStringFromSelection();
    void readSelectionFromPasteboard(const String& pasteboardName);
    PassRefPtr<SharedBuffer> dataSelectionForPasteboard(const String& pasteboardName);
#endif // !PLATFORM(IOS)
#endif

#if PLATFORM(COCOA) || PLATFORM(EFL)
    void writeSelectionToPasteboard(Pasteboard&);
    void writeImageToPasteboard(Pasteboard&, Element& imageElement, const URL&, const String& title);
#endif

private:
    class WebContentReader;

    Document& document() const;

    bool canDeleteRange(Range*) const;
    bool canSmartReplaceWithPasteboard(Pasteboard&);
    void pasteAsPlainTextWithPasteboard(Pasteboard&);
    void pasteWithPasteboard(Pasteboard*, bool allowPlainText);
    String plainTextFromPasteboard(const PasteboardPlainText&);

    void revealSelectionAfterEditingOperation(const ScrollAlignment& = ScrollAlignment::alignCenterIfNeeded, RevealExtentOption = DoNotRevealExtent);
    void markMisspellingsOrBadGrammar(const VisibleSelection&, bool checkSpelling, RefPtr<Range>& firstMisspellingRange);
    TextCheckingTypeMask resolveTextCheckingTypeMask(TextCheckingTypeMask);

    String selectedText(TextIteratorBehavior) const;

    void selectComposition();
    enum SetCompositionMode { ConfirmComposition, CancelComposition };
    void setComposition(const String&, SetCompositionMode);

    void changeSelectionAfterCommand(const VisibleSelection& newSelection, FrameSelection::SetSelectionOptions);

    enum EditorActionSpecifier { CutAction, CopyAction };
    void performCutOrCopy(EditorActionSpecifier);

    void editorUIUpdateTimerFired(Timer<Editor>&);

    Node* findEventTargetFromSelection() const;

    bool unifiedTextCheckerEnabled() const;

#if PLATFORM(COCOA)
    PassRefPtr<SharedBuffer> selectionInWebArchiveFormat();
    PassRefPtr<Range> adjustedSelectionRange();
    PassRefPtr<DocumentFragment> createFragmentForImageResourceAndAddResource(PassRefPtr<ArchiveResource>);
    PassRefPtr<DocumentFragment> createFragmentAndAddResources(NSAttributedString *);
    void fillInUserVisibleForm(PasteboardURL&);
#endif

    Frame& m_frame;
#if ENABLE(DELETION_UI)
    std::unique_ptr<DeleteButtonController> m_deleteButtonController;
#endif
    RefPtr<CompositeEditCommand> m_lastEditCommand;
    RefPtr<Text> m_compositionNode;
    unsigned m_compositionStart;
    unsigned m_compositionEnd;
    Vector<CompositionUnderline> m_customCompositionUnderlines;
    bool m_ignoreCompositionSelectionChange;
    bool m_shouldStartNewKillRingSequence;
    bool m_shouldStyleWithCSS;
    const std::unique_ptr<KillRing> m_killRing;
    const std::unique_ptr<SpellChecker> m_spellChecker;
    const std::unique_ptr<AlternativeTextController> m_alternativeTextController;
    VisibleSelection m_mark;
    bool m_areMarkedTextMatchesHighlighted;
    EditorParagraphSeparator m_defaultParagraphSeparator;
    bool m_overwriteModeEnabled;

    VisibleSelection m_oldSelectionForEditorUIUpdate;
    Timer<Editor> m_editorUIUpdateTimer;
    bool m_editorUIUpdateTimerShouldCheckSpellingAndGrammar;
    bool m_editorUIUpdateTimerWasTriggeredByDictation;

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)
    bool shouldDetectTelephoneNumbers();
    void scanSelectionForTelephoneNumbers(Timer<Editor>&);
    void scanRangeForTelephoneNumbers(Range&, const StringView&, Vector<RefPtr<Range>>& markedRanges);
    void clearDataDetectedTelephoneNumbers();

    Timer<Editor> m_telephoneNumberDetectionUpdateTimer;
#endif
};

inline void Editor::setStartNewKillRingSequence(bool flag)
{
    m_shouldStartNewKillRingSequence = flag;
}

inline const VisibleSelection& Editor::mark() const
{
    return m_mark;
}

inline void Editor::setMark(const VisibleSelection& selection)
{
    m_mark = selection;
}

inline bool Editor::markedTextMatchesAreHighlighted() const
{
    return m_areMarkedTextMatchesHighlighted;
}

#if !ENABLE(DELETION_UI)

inline PassRefPtr<Range> Editor::avoidIntersectionWithDeleteButtonController(const Range* range) const
{
    return const_cast<Range*>(range);
}

inline VisibleSelection Editor::avoidIntersectionWithDeleteButtonController(const VisibleSelection& selection) const
{
    return selection;
}

#endif

} // namespace WebCore

#endif // Editor_h
