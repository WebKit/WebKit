/*
 * Copyright (C) 2006, 2007, 2008, 2013, 2014 Apple Inc. All rights reserved.
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
OBJC_CLASS NSMutableDictionary;
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
class KeyboardEvent;
class KillRing;
class Pasteboard;
class SharedBuffer;
class Font;
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

enum class MailBlockquoteHandling {
    RespectBlockquote,
    IgnoreBlockquote,
};

class Editor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit Editor(Frame&);
    ~Editor();

    WEBCORE_EXPORT EditorClient* client() const;
    WEBCORE_EXPORT TextCheckerClient* textChecker() const;

    CompositeEditCommand* lastEditCommand() { return m_lastEditCommand.get(); }

    void handleKeyboardEvent(KeyboardEvent*);
    void handleInputMethodKeydown(KeyboardEvent*);
    bool handleTextEvent(TextEvent*);

    WEBCORE_EXPORT bool canEdit() const;
    WEBCORE_EXPORT bool canEditRichly() const;

    bool canDHTMLCut();
    bool canDHTMLCopy();
    WEBCORE_EXPORT bool canDHTMLPaste();
    bool tryDHTMLCopy();
    bool tryDHTMLCut();
    WEBCORE_EXPORT bool tryDHTMLPaste();

    WEBCORE_EXPORT bool canCut() const;
    WEBCORE_EXPORT bool canCopy() const;
    WEBCORE_EXPORT bool canPaste() const;
    WEBCORE_EXPORT bool canDelete() const;
    bool canSmartCopyOrDelete();

    WEBCORE_EXPORT void cut();
    WEBCORE_EXPORT void copy();
    WEBCORE_EXPORT void paste();
    void paste(Pasteboard&);
    WEBCORE_EXPORT void pasteAsPlainText();
    WEBCORE_EXPORT void performDelete();

    WEBCORE_EXPORT void copyURL(const URL&, const String& title);
    void copyURL(const URL&, const String& title, Pasteboard&);
#if !PLATFORM(IOS)
    WEBCORE_EXPORT void copyImage(const HitTestResult&);
#endif

    String readPlainTextFromPasteboard(Pasteboard&);

    WEBCORE_EXPORT void indent();
    WEBCORE_EXPORT void outdent();
    void transpose();

    bool shouldInsertFragment(PassRefPtr<DocumentFragment>, PassRefPtr<Range>, EditorInsertAction);
    bool shouldInsertText(const String&, Range*, EditorInsertAction) const;
    WEBCORE_EXPORT bool shouldDeleteRange(Range*) const;
    bool shouldApplyStyle(StyleProperties*, Range*);

    void respondToChangedContents(const VisibleSelection& endingSelection);

    bool selectionStartHasStyle(CSSPropertyID, const String& value) const;
    WEBCORE_EXPORT TriState selectionHasStyle(CSSPropertyID, const String& value) const;
    String selectionStartCSSPropertyValue(CSSPropertyID);
    
    TriState selectionUnorderedListState() const;
    TriState selectionOrderedListState() const;
    WEBCORE_EXPORT PassRefPtr<Node> insertOrderedList();
    WEBCORE_EXPORT PassRefPtr<Node> insertUnorderedList();
    WEBCORE_EXPORT bool canIncreaseSelectionListLevel();
    WEBCORE_EXPORT bool canDecreaseSelectionListLevel();
    WEBCORE_EXPORT PassRefPtr<Node> increaseSelectionListLevel();
    WEBCORE_EXPORT PassRefPtr<Node> increaseSelectionListLevelOrdered();
    WEBCORE_EXPORT PassRefPtr<Node> increaseSelectionListLevelUnordered();
    WEBCORE_EXPORT void decreaseSelectionListLevel();
   
    void removeFormattingAndStyle();

    void clearLastEditCommand();
#if PLATFORM(IOS)
    WEBCORE_EXPORT void ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping();
#endif

    WEBCORE_EXPORT bool deleteWithDirection(SelectionDirection, TextGranularity, bool killRing, bool isTypingAction);
    WEBCORE_EXPORT void deleteSelectionWithSmartDelete(bool smartDelete, EditAction = EditActionDelete);
    void clearText();
#if PLATFORM(IOS)
    WEBCORE_EXPORT void removeUnchangeableStyles();
#endif
    
    bool dispatchCPPEvent(const AtomicString&, DataTransferAccessPolicy);
    
    WEBCORE_EXPORT void applyStyle(StyleProperties*, EditAction = EditActionUnspecified);
    void applyStyle(RefPtr<EditingStyle>&&, EditAction);
    void applyParagraphStyle(StyleProperties*, EditAction = EditActionUnspecified);
    WEBCORE_EXPORT void applyStyleToSelection(StyleProperties*, EditAction);
    WEBCORE_EXPORT void applyStyleToSelection(Ref<EditingStyle>&&, EditAction);
    void applyParagraphStyleToSelection(StyleProperties*, EditAction);

    void appliedEditing(PassRefPtr<CompositeEditCommand>);
    void unappliedEditing(PassRefPtr<EditCommandComposition>);
    void reappliedEditing(PassRefPtr<EditCommandComposition>);
    void unappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction);

    void setShouldStyleWithCSS(bool flag) { m_shouldStyleWithCSS = flag; }
    bool shouldStyleWithCSS() const { return m_shouldStyleWithCSS; }

    class Command {
    public:
        WEBCORE_EXPORT Command();
        Command(const EditorInternalCommand*, EditorCommandSource, PassRefPtr<Frame>);

        WEBCORE_EXPORT bool execute(const String& parameter = String(), Event* triggeringEvent = nullptr) const;
        WEBCORE_EXPORT bool execute(Event* triggeringEvent) const;

        WEBCORE_EXPORT bool isSupported() const;
        WEBCORE_EXPORT bool isEnabled(Event* triggeringEvent = nullptr) const;

        WEBCORE_EXPORT TriState state(Event* triggeringEvent = nullptr) const;
        String value(Event* triggeringEvent = nullptr) const;

        WEBCORE_EXPORT bool isTextInsertion() const;

    private:
        const EditorInternalCommand* m_command { nullptr };
        EditorCommandSource m_source;
        RefPtr<Frame> m_frame;
    };
    WEBCORE_EXPORT Command command(const String& commandName); // Command source is CommandFromMenuOrKeyBinding.
    Command command(const String& commandName, EditorCommandSource);
    WEBCORE_EXPORT static bool commandIsSupportedFromMenuOrKeyBinding(const String& commandName); // Works without a frame.

    WEBCORE_EXPORT bool insertText(const String&, Event* triggeringEvent);
    bool insertTextForConfirmedComposition(const String& text);
    WEBCORE_EXPORT bool insertDictatedText(const String&, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent);
    bool insertTextWithoutSendingTextEvent(const String&, bool selectInsertedText, TextEvent* triggeringEvent);
    bool insertLineBreak();
    bool insertParagraphSeparator();
    WEBCORE_EXPORT bool insertParagraphSeparatorInQuotedContent();

    WEBCORE_EXPORT bool isContinuousSpellCheckingEnabled() const;
    WEBCORE_EXPORT void toggleContinuousSpellChecking();
    bool isGrammarCheckingEnabled();
    void toggleGrammarChecking();
    void ignoreSpelling();
    void learnSpelling();
    int spellCheckerDocumentTag();
    WEBCORE_EXPORT bool isSelectionUngrammatical();
    String misspelledSelectionString() const;
    String misspelledWordAtCaretOrRange(Node* clickedNode) const;
    Vector<String> guessesForMisspelledWord(const String&) const;
    Vector<String> guessesForMisspelledOrUngrammatical(bool& misspelled, bool& ungrammatical);
    bool isSpellCheckingEnabledInFocusedNode() const;
    bool isSpellCheckingEnabledFor(Node*) const;
    WEBCORE_EXPORT void markMisspellingsAfterTypingToWord(const VisiblePosition &wordStart, const VisibleSelection& selectionAfterTyping, bool doReplacement);
    void markMisspellings(const VisibleSelection&, RefPtr<Range>& firstMisspellingRange);
    void markBadGrammar(const VisibleSelection&);
    void markMisspellingsAndBadGrammar(const VisibleSelection& spellingSelection, bool markGrammar, const VisibleSelection& grammarSelection);
    void markAndReplaceFor(PassRefPtr<SpellCheckRequest>, const Vector<TextCheckingResult>&);

    bool isOverwriteModeEnabled() const { return m_overwriteModeEnabled; }
    WEBCORE_EXPORT void toggleOverwriteModeEnabled();

    void markAllMisspellingsAndBadGrammarInRanges(TextCheckingTypeMask, Range* spellingRange, Range* grammarRange);
#if PLATFORM(IOS)
    NO_RETURN_DUE_TO_ASSERT
#endif
    void changeBackToReplacedString(const String& replacedString);

#if !PLATFORM(IOS)
    WEBCORE_EXPORT void advanceToNextMisspelling(bool startBeforeSelection = false);
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
    WEBCORE_EXPORT void setBaseWritingDirection(WritingDirection);

    // smartInsertDeleteEnabled and selectTrailingWhitespaceEnabled are 
    // mutually exclusive, meaning that enabling one will disable the other.
    bool smartInsertDeleteEnabled();
    bool isSelectTrailingWhitespaceEnabled();
    
    WEBCORE_EXPORT bool hasBidiSelection() const;

    // international text input composition
    bool hasComposition() const { return m_compositionNode; }
    WEBCORE_EXPORT void setComposition(const String&, const Vector<CompositionUnderline>&, unsigned selectionStart, unsigned selectionEnd);
    WEBCORE_EXPORT void confirmComposition();
    WEBCORE_EXPORT void confirmComposition(const String&); // if no existing composition, replaces selection
    WEBCORE_EXPORT void cancelComposition();
    bool cancelCompositionIfSelectionIsInvalid();
    WEBCORE_EXPORT PassRefPtr<Range> compositionRange() const;
    WEBCORE_EXPORT bool getCompositionSelection(unsigned& selectionStart, unsigned& selectionEnd) const;

    // getting international text input composition state (for use by InlineTextBox)
    Text* compositionNode() const { return m_compositionNode.get(); }
    unsigned compositionStart() const { return m_compositionStart; }
    unsigned compositionEnd() const { return m_compositionEnd; }
    bool compositionUsesCustomUnderlines() const { return !m_customCompositionUnderlines.isEmpty(); }
    const Vector<CompositionUnderline>& customCompositionUnderlines() const { return m_customCompositionUnderlines; }

    enum class RevealSelection { No, Yes };
    WEBCORE_EXPORT void setIgnoreCompositionSelectionChange(bool, RevealSelection shouldRevealExistingSelection = RevealSelection::Yes);
    bool ignoreCompositionSelectionChange() const { return m_ignoreCompositionSelectionChange; }

    WEBCORE_EXPORT PassRefPtr<Range> rangeForPoint(const IntPoint& windowPoint);

    void clear();

    VisibleSelection selectionForCommand(Event*);

    KillRing& killRing() const { return *m_killRing; }
    SpellChecker& spellChecker() const { return *m_spellChecker; }

    EditingBehavior behavior() const;

    RefPtr<Range> selectedRange();

#if PLATFORM(IOS)
    WEBCORE_EXPORT void confirmMarkedText();
    WEBCORE_EXPORT void setTextAsChildOfElement(const String&, Element*);
    WEBCORE_EXPORT void setTextAlignmentForChangedBaseWritingDirection(WritingDirection);
    WEBCORE_EXPORT void insertDictationPhrases(Vector<Vector<String>>&& dictationPhrases, RetainPtr<id> metadata);
    WEBCORE_EXPORT void setDictationPhrasesAsChildOfElement(const Vector<Vector<String>>& dictationPhrases, RetainPtr<id> metadata, Element&);
#endif
    
    enum class KillRingInsertionMode { PrependText, AppendText };
    void addRangeToKillRing(const Range&, KillRingInsertionMode);
    void addTextToKillRing(const String&, KillRingInsertionMode);
    void setStartNewKillRingSequence(bool);

    void startAlternativeTextUITimer();
    // If user confirmed a correction in the correction panel, correction has non-zero length, otherwise it means that user has dismissed the panel.
    WEBCORE_EXPORT void handleAlternativeTextUIResult(const String& correction);
    void dismissCorrectionPanelAsIgnored();

    WEBCORE_EXPORT void pasteAsFragment(Ref<DocumentFragment>&&, bool smartReplace, bool matchStyle, MailBlockquoteHandling = MailBlockquoteHandling::RespectBlockquote);
    WEBCORE_EXPORT void pasteAsPlainText(const String&, bool smartReplace);

    // This is only called on the mac where paste is implemented primarily at the WebKit level.
    WEBCORE_EXPORT void pasteAsPlainTextBypassingDHTML();
 
    void clearMisspellingsAndBadGrammar(const VisibleSelection&);
    void markMisspellingsAndBadGrammar(const VisibleSelection&);

    Node* findEventTargetFrom(const VisibleSelection& selection) const;

    WEBCORE_EXPORT String selectedText() const;
    String selectedTextForDataTransfer() const;
    WEBCORE_EXPORT bool findString(const String&, FindOptions);

    PassRefPtr<Range> rangeOfString(const String&, Range*, FindOptions);
    PassRefPtr<Range> findStringAndScrollToVisible(const String&, Range*, FindOptions);

    const VisibleSelection& mark() const; // Mark, to be used as emacs uses it.
    void setMark(const VisibleSelection&);

    void computeAndSetTypingStyle(EditingStyle& , EditAction = EditActionUnspecified);
    WEBCORE_EXPORT void computeAndSetTypingStyle(StyleProperties& , EditAction = EditActionUnspecified);
    WEBCORE_EXPORT void applyEditingStyleToBodyElement() const;
    void applyEditingStyleToElement(Element*) const;

    WEBCORE_EXPORT IntRect firstRectForRange(Range*) const;

    void respondToChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions);
    WEBCORE_EXPORT void updateEditorUINowIfScheduled();
    bool shouldChangeSelection(const VisibleSelection& oldSelection, const VisibleSelection& newSelection, EAffinity, bool stillSelecting) const;
    WEBCORE_EXPORT unsigned countMatchesForText(const String&, Range*, FindOptions, unsigned limit, bool markMatches, Vector<RefPtr<Range>>*);
    bool markedTextMatchesAreHighlighted() const;
    WEBCORE_EXPORT void setMarkedTextMatchesAreHighlighted(bool);

    void textFieldDidBeginEditing(Element*);
    void textFieldDidEndEditing(Element*);
    void textDidChangeInTextField(Element*);
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    void textWillBeDeletedInTextField(Element* input);
    void textDidChangeInTextArea(Element*);
    WEBCORE_EXPORT WritingDirection baseWritingDirectionForSelectionStart() const;

    WEBCORE_EXPORT void replaceSelectionWithFragment(PassRefPtr<DocumentFragment>, bool selectReplacement, bool smartReplace, bool matchStyle, EditAction = EditActionInsert, MailBlockquoteHandling = MailBlockquoteHandling::RespectBlockquote);
    WEBCORE_EXPORT void replaceSelectionWithText(const String&, bool selectReplacement, bool smartReplace, EditAction = EditActionInsert);
    WEBCORE_EXPORT bool selectionStartHasMarkerFor(DocumentMarker::MarkerType, int from, int length) const;
    void updateMarkersForWordsAffectedByEditing(bool onlyHandleWordsContainingSelection);
    void deletedAutocorrectionAtPosition(const Position&, const String& originalString);
    
    WEBCORE_EXPORT void simplifyMarkup(Node* startNode, Node* endNode);

    EditorParagraphSeparator defaultParagraphSeparator() const { return m_defaultParagraphSeparator; }
    void setDefaultParagraphSeparator(EditorParagraphSeparator separator) { m_defaultParagraphSeparator = separator; }
    Vector<String> dictationAlternativesForMarker(const DocumentMarker*);
    void applyDictationAlternativelternative(const String& alternativeString);

#if USE(APPKIT)
    WEBCORE_EXPORT void uppercaseWord();
    WEBCORE_EXPORT void lowercaseWord();
    WEBCORE_EXPORT void capitalizeWord();
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    WEBCORE_EXPORT void showSubstitutionsPanel();
    WEBCORE_EXPORT bool substitutionsPanelIsShowing();
    WEBCORE_EXPORT void toggleSmartInsertDelete();
    WEBCORE_EXPORT bool isAutomaticQuoteSubstitutionEnabled();
    WEBCORE_EXPORT void toggleAutomaticQuoteSubstitution();
    WEBCORE_EXPORT bool isAutomaticLinkDetectionEnabled();
    WEBCORE_EXPORT void toggleAutomaticLinkDetection();
    WEBCORE_EXPORT bool isAutomaticDashSubstitutionEnabled();
    WEBCORE_EXPORT void toggleAutomaticDashSubstitution();
    WEBCORE_EXPORT bool isAutomaticTextReplacementEnabled();
    WEBCORE_EXPORT void toggleAutomaticTextReplacement();
    WEBCORE_EXPORT bool isAutomaticSpellingCorrectionEnabled();
    WEBCORE_EXPORT void toggleAutomaticSpellingCorrection();
#endif

    RefPtr<DocumentFragment> webContentFromPasteboard(Pasteboard&, Range& context, bool allowPlainText, bool& chosePlainText);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static RenderStyle* styleForSelectionStart(Frame* , Node *&nodeToRemove);
    void getTextDecorationAttributesRespectingTypingStyle(RenderStyle&, NSMutableDictionary*) const;
    WEBCORE_EXPORT const Font* fontForSelection(bool&) const;
    WEBCORE_EXPORT NSDictionary *fontAttributesForSelectionStart() const;
    WEBCORE_EXPORT String stringSelectionForPasteboard();
    String stringSelectionForPasteboardWithImageAltText();
#if !PLATFORM(IOS)
    bool canCopyExcludingStandaloneImages();
    void takeFindStringFromSelection();
    WEBCORE_EXPORT void readSelectionFromPasteboard(const String& pasteboardName, MailBlockquoteHandling = MailBlockquoteHandling::RespectBlockquote);
    WEBCORE_EXPORT void replaceNodeFromPasteboard(Node*, const String& pasteboardName);
    WEBCORE_EXPORT RefPtr<SharedBuffer> dataSelectionForPasteboard(const String& pasteboardName);
    WEBCORE_EXPORT void applyFontStyles(const String& fontFamily, double fontSize, unsigned fontTraits);
#endif // !PLATFORM(IOS)
    WEBCORE_EXPORT void replaceSelectionWithAttributedString(NSAttributedString *, MailBlockquoteHandling = MailBlockquoteHandling::RespectBlockquote);
#endif

#if PLATFORM(COCOA) || PLATFORM(EFL) || PLATFORM(GTK)
    WEBCORE_EXPORT void writeSelectionToPasteboard(Pasteboard&);
    WEBCORE_EXPORT void writeImageToPasteboard(Pasteboard&, Element& imageElement, const URL&, const String& title);
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)
    void scanSelectionForTelephoneNumbers();
    const Vector<RefPtr<Range>>& detectedTelephoneNumberRanges() const { return m_detectedTelephoneNumberRanges; }
#endif

    WEBCORE_EXPORT String stringForCandidateRequest() const;
    WEBCORE_EXPORT void handleAcceptedCandidate(TextCheckingResult);
    bool isHandlingAcceptedCandidate() const { return m_isHandlingAcceptedCandidate; }

private:
    class WebContentReader;

    Document& document() const;

    bool canDeleteRange(Range*) const;
    bool canSmartReplaceWithPasteboard(Pasteboard&);
    void pasteAsPlainTextWithPasteboard(Pasteboard&);
    void pasteWithPasteboard(Pasteboard*, bool allowPlainText, MailBlockquoteHandling = MailBlockquoteHandling::RespectBlockquote);
    String plainTextFromPasteboard(const PasteboardPlainText&);

    void revealSelectionAfterEditingOperation(const ScrollAlignment& = ScrollAlignment::alignCenterIfNeeded, RevealExtentOption = DoNotRevealExtent);
    void markMisspellingsOrBadGrammar(const VisibleSelection&, bool checkSpelling, RefPtr<Range>& firstMisspellingRange);
    TextCheckingTypeMask resolveTextCheckingTypeMask(TextCheckingTypeMask);

    WEBCORE_EXPORT String selectedText(TextIteratorBehavior) const;

    void selectComposition();
    enum SetCompositionMode { ConfirmComposition, CancelComposition };
    void setComposition(const String&, SetCompositionMode);

    void changeSelectionAfterCommand(const VisibleSelection& newSelection, FrameSelection::SetSelectionOptions, AXTextStateChangeIntent = AXTextStateChangeIntent());

    enum EditorActionSpecifier { CutAction, CopyAction };
    void performCutOrCopy(EditorActionSpecifier);

    void editorUIUpdateTimerFired();

    Node* findEventTargetFromSelection() const;

    bool unifiedTextCheckerEnabled() const;

#if PLATFORM(COCOA)
    RefPtr<SharedBuffer> selectionInWebArchiveFormat();
    RefPtr<Range> adjustedSelectionRange();
    RefPtr<DocumentFragment> createFragmentForImageResourceAndAddResource(RefPtr<ArchiveResource>&&);
    RefPtr<DocumentFragment> createFragmentAndAddResources(NSAttributedString *);
    void fillInUserVisibleForm(PasteboardURL&);
#endif

    Frame& m_frame;
    RefPtr<CompositeEditCommand> m_lastEditCommand;
    RefPtr<Text> m_compositionNode;
    unsigned m_compositionStart;
    unsigned m_compositionEnd;
    Vector<CompositionUnderline> m_customCompositionUnderlines;
    bool m_ignoreCompositionSelectionChange;
    bool m_shouldStartNewKillRingSequence {false};
    bool m_shouldStyleWithCSS;
    const std::unique_ptr<KillRing> m_killRing;
    const std::unique_ptr<SpellChecker> m_spellChecker;
    const std::unique_ptr<AlternativeTextController> m_alternativeTextController;
    VisibleSelection m_mark;
    bool m_areMarkedTextMatchesHighlighted;
    EditorParagraphSeparator m_defaultParagraphSeparator;
    bool m_overwriteModeEnabled;

    VisibleSelection m_oldSelectionForEditorUIUpdate;
    Timer m_editorUIUpdateTimer;
    bool m_editorUIUpdateTimerShouldCheckSpellingAndGrammar;
    bool m_editorUIUpdateTimerWasTriggeredByDictation;
    bool m_isHandlingAcceptedCandidate { false };

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)
    bool shouldDetectTelephoneNumbers();
    void scanRangeForTelephoneNumbers(Range&, const StringView&, Vector<RefPtr<Range>>& markedRanges);

    Timer m_telephoneNumberDetectionUpdateTimer;
    Vector<RefPtr<Range>> m_detectedTelephoneNumberRanges;
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

} // namespace WebCore

#endif // Editor_h
