/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "ClipboardAccessPolicy.h"
#include "EditorDeleteAction.h"
#include "EditorInsertAction.h"
#include "Frame.h"
#include "SelectionController.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
class NSString;
class NSURL;
#endif

namespace WebCore {

class Clipboard;
class DeleteButtonController;
class DocumentFragment;
class EditCommand;
class EditorInternalCommand;
class EditorClient;
class EventTargetNode;
class Frame;
class HTMLElement;
class Pasteboard;
class Range;
class SelectionController;
class Selection;
class SimpleFontData;

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

enum TriState { FalseTriState, TrueTriState, MixedTriState };
enum EditorCommandSource { CommandFromMenuOrKeyBinding, CommandFromDOM, CommandFromDOMWithUserInterface };

class Editor {
public:
    Editor(Frame*);
    ~Editor();

    EditorClient* client() const;
    Frame* frame() const { return m_frame; }
    DeleteButtonController* deleteButtonController() const { return m_deleteButtonController.get(); }
    EditCommand* lastEditCommand() { return m_lastEditCommand.get(); }

    void handleKeyboardEvent(KeyboardEvent*);
    void handleInputMethodKeydown(KeyboardEvent*);

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
    void pasteAsPlainText();
    void performDelete();

    void copyURL(const KURL&, const String&);
    void copyImage(const HitTestResult&);

    void indent();
    void outdent();
    void transpose();

    bool shouldInsertFragment(PassRefPtr<DocumentFragment> fragment, PassRefPtr<Range> replacingDOMRange, EditorInsertAction givenAction);
    bool shouldInsertText(const String&, Range*, EditorInsertAction) const;
    bool shouldShowDeleteInterface(HTMLElement*) const;
    bool shouldDeleteRange(Range*) const;
    bool shouldApplyStyle(CSSStyleDeclaration*, Range*);
    
    void respondToChangedSelection(const Selection& oldSelection);
    void respondToChangedContents(const Selection& endingSelection);

    TriState selectionHasStyle(CSSStyleDeclaration*) const;
    const SimpleFontData* fontForSelection(bool&) const;
    
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

    // FIXME: Once the Editor implements all editing commands, it should track 
    // the lastEditCommand on its own, and we should remove this function.
    void setLastEditCommand(PassRefPtr<EditCommand> lastEditCommand);

    bool deleteWithDirection(SelectionController::EDirection, TextGranularity, bool killRing, bool isTypingAction);
    void deleteSelectionWithSmartDelete(bool smartDelete);
    bool dispatchCPPEvent(const AtomicString&, ClipboardAccessPolicy);
    
    Node* removedAnchor() const { return m_removedAnchor.get(); }
    void setRemovedAnchor(PassRefPtr<Node> n) { m_removedAnchor = n; }

    void applyStyle(CSSStyleDeclaration*, EditAction = EditActionUnspecified);
    void applyParagraphStyle(CSSStyleDeclaration*, EditAction = EditActionUnspecified);
    void applyStyleToSelection(CSSStyleDeclaration*, EditAction);
    void applyParagraphStyleToSelection(CSSStyleDeclaration*, EditAction);

    void appliedEditing(PassRefPtr<EditCommand>);
    void unappliedEditing(PassRefPtr<EditCommand>);
    void reappliedEditing(PassRefPtr<EditCommand>);
    
    bool selectionStartHasStyle(CSSStyleDeclaration*) const;

    bool clientIsEditable() const;

    class Command {
    public:
        Command();
        Command(PassRefPtr<Frame>, const EditorInternalCommand*, EditorCommandSource);

        bool execute(const String& parameter = String(), Event* triggeringEvent = 0) const;
        bool execute(Event* triggeringEvent) const;

        bool isSupported() const;
        bool isEnabled(Event* triggeringEvent = 0) const;

        TriState state(Event* triggeringEvent = 0) const;
        String value(Event* triggeringEvent = 0) const;

        bool isTextInsertion() const;

    private:
        RefPtr<Frame> m_frame;
        const EditorInternalCommand* m_command;
        EditorCommandSource m_source;
    };
    Command command(const String& commandName); // Default is CommandFromMenuOrKeyBinding.
    Command command(const String& commandName, EditorCommandSource);

    bool insertText(const String&, Event* triggeringEvent);
    bool insertTextWithoutSendingTextEvent(const String&, bool selectInsertedText, Event* triggeringEvent = 0);
    bool insertLineBreak();
    bool insertParagraphSeparator();
    
    bool isContinuousSpellCheckingEnabled();
    void toggleContinuousSpellChecking();
    bool isGrammarCheckingEnabled();
    void toggleGrammarChecking();
    void ignoreSpelling();
    void learnSpelling();
    int spellCheckerDocumentTag();
    bool isSelectionUngrammatical();
    bool isSelectionMisspelled();
    Vector<String> guessesForMisspelledSelection();
    Vector<String> guessesForUngrammaticalSelection();
    void markMisspellingsAfterTypingToPosition(const VisiblePosition&);
    void markMisspellings(const Selection&);
    void markBadGrammar(const Selection&);
    void advanceToNextMisspelling(bool startBeforeSelection = false);
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
    void didWriteSelectionToPasteboard();
    
    void showFontPanel();
    void showStylesPanel();
    void showColorPanel();
    void toggleBold();
    void toggleUnderline();
    void setBaseWritingDirection(const String&);

    bool smartInsertDeleteEnabled();
    
    // international text input composition
    bool hasComposition() const { return m_compositionNode; }
    void setComposition(const String&, const Vector<CompositionUnderline>&, unsigned selectionStart, unsigned selectionEnd);
    void confirmComposition();
    void confirmComposition(const String&); // if no existing composition, replaces selection
    void confirmCompositionWithoutDisturbingSelection();
    PassRefPtr<Range> compositionRange() const;
    bool getCompositionSelection(unsigned& selectionStart, unsigned& selectionEnd) const;

    // getting international text input composition state (for use by InlineTextBox)
    Text* compositionNode() const { return m_compositionNode.get(); }
    unsigned compositionStart() const { return m_compositionStart; }
    unsigned compositionEnd() const { return m_compositionEnd; }
    bool compositionUsesCustomUnderlines() const { return !m_customCompositionUnderlines.isEmpty(); }
    const Vector<CompositionUnderline>& customCompositionUnderlines() const { return m_customCompositionUnderlines; }

    bool ignoreCompositionSelectionChange() const { return m_ignoreCompositionSelectionChange; }

    void setStartNewKillRingSequence(bool);

    PassRefPtr<Range> rangeForPoint(const IntPoint& windowPoint);

    void clear();

    Selection selectionForCommand(Event*);

#if PLATFORM(MAC)
    NSString* userVisibleString(NSURL*);
#endif

    void appendToKillRing(const String&);
    void prependToKillRing(const String&);
    String yankFromKillRing();
    void startNewKillRingSequence();
    void setKillRingToYankedState();

    PassRefPtr<Range> selectedRange();

private:
    Frame* m_frame;
    OwnPtr<DeleteButtonController> m_deleteButtonController;
    RefPtr<EditCommand> m_lastEditCommand;
    RefPtr<Node> m_removedAnchor;

    RefPtr<Text> m_compositionNode;
    unsigned m_compositionStart;
    unsigned m_compositionEnd;
    Vector<CompositionUnderline> m_customCompositionUnderlines;
    bool m_ignoreCompositionSelectionChange;
    bool m_shouldStartNewKillRingSequence;

    bool canDeleteRange(Range*) const;
    bool canSmartReplaceWithPasteboard(Pasteboard*);
    PassRefPtr<Clipboard> newGeneralClipboard(ClipboardAccessPolicy);
    void pasteAsPlainTextWithPasteboard(Pasteboard*);
    void pasteWithPasteboard(Pasteboard*, bool allowPlainText);
    void replaceSelectionWithFragment(PassRefPtr<DocumentFragment>, bool selectReplacement, bool smartReplace, bool matchStyle);
    void replaceSelectionWithText(const String&, bool selectReplacement, bool smartReplace);
    void writeSelectionToPasteboard(Pasteboard*);
    void revealSelectionAfterEditingOperation();

    void selectComposition();
    void confirmComposition(const String&, bool preserveSelection);
    void setIgnoreCompositionSelectionChange(bool ignore);

    void addToKillRing(Range*, bool prepend);
};

inline void Editor::setStartNewKillRingSequence(bool flag)
{
    m_shouldStartNewKillRingSequence = flag;
}

} // namespace WebCore

#endif // Editor_h
