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
class EditorClient;
class EventTargetNode;
class FontData;
class Frame;
class HTMLElement;
class Pasteboard;
class Range;
class SelectionController;
class Selection;

class Editor {
public:
    Editor(Frame*);
    ~Editor();

    EditorClient* client() const;
    Frame* frame() const { return m_frame; }
    DeleteButtonController* deleteButtonController() const { return m_deleteButtonController.get(); }
    EditCommand* lastEditCommand() { return m_lastEditCommand.get(); }

    void handleKeypress(KeyboardEvent*);
    void handleInputMethodKeypress(KeyboardEvent*);

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
    
    void cut();
    void copy();
    void paste();
    void pasteAsPlainText();
    void performDelete();

    void copyURL(const KURL&, const String&);
    void copyImage(const HitTestResult&);

    void indent();
    void outdent();

    bool shouldInsertFragment(PassRefPtr<DocumentFragment> fragment, PassRefPtr<Range> replacingDOMRange, EditorInsertAction givenAction);
    bool shouldInsertText(const String&, Range*, EditorInsertAction) const;
    bool shouldShowDeleteInterface(HTMLElement*) const;
    bool shouldDeleteRange(Range*) const;
    bool shouldApplyStyle(CSSStyleDeclaration*, Range*);
    
    void respondToChangedSelection(const Selection& oldSelection);
    void respondToChangedContents(const Selection& endingSelection);
    
    const FontData* fontForSelection(bool&) const;
    
    Frame::TriState selectionUnorderedListState() const;
    Frame::TriState selectionOrderedListState() const;
    
    void removeFormattingAndStyle();

    // FIXME: Once the Editor implements all editing commands, it should track 
    // the lastEditCommand on its own, and we should remove this function.
    void setLastEditCommand(PassRefPtr<EditCommand> lastEditCommand);

    bool deleteWithDirection(SelectionController::EDirection, TextGranularity, bool killRing, bool isTypingAction);
    void deleteRange(Range*, bool killRing, bool prepend, bool smartDeleteOK, EditorDeleteAction, TextGranularity);
    void deleteSelectionWithSmartDelete(bool smartDelete);
    void deleteSelectionWithSmartDelete();
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

    bool selectWordBeforeMenuEvent() const;
    bool clientIsEditable() const;
    
    bool execCommand(const AtomicString&);
    
    bool insertText(const String&, bool selectInsertedText, Event* triggeringEvent = 0);
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
    void setBaseWritingDirection(String);

    bool smartInsertDeleteEnabled();
    
    void selectMarkedText();
    void unmarkText();
    void discardMarkedText();

    bool ignoreMarkedTextSelectionChange() const { return m_ignoreMarkedTextSelectionChange; }
    void setIgnoreMarkedTextSelectionChange(bool ignore) { m_ignoreMarkedTextSelectionChange = ignore; }

#if PLATFORM(MAC)
    NSString* userVisibleString(NSURL*);
    void setStartNewKillRingSequence(bool flag) { m_startNewKillRingSequence = flag; }
#else 
    void setStartNewKillRingSequence(bool) { }
#endif

private:
    Frame* m_frame;
    OwnPtr<DeleteButtonController> m_deleteButtonController;
    RefPtr<EditCommand> m_lastEditCommand;
    RefPtr<Node> m_removedAnchor;
    bool m_ignoreMarkedTextSelectionChange;

    bool canDeleteRange(Range*) const;
    bool canSmartCopyOrDelete();
    bool canSmartReplaceWithPasteboard(Pasteboard*);
    PassRefPtr<Clipboard> newGeneralClipboard(ClipboardAccessPolicy);
    PassRefPtr<Range> selectedRange();
    void pasteAsPlainTextWithPasteboard(Pasteboard*);
    void pasteWithPasteboard(Pasteboard*, bool allowPlainText);
    void replaceSelectionWithFragment(PassRefPtr<DocumentFragment>, bool selectReplacement, bool smartReplace, bool matchStyle);
    void replaceSelectionWithText(const String&, bool selectReplacement, bool smartReplace);
    void writeSelectionToPasteboard(Pasteboard*);

#if PLATFORM(MAC)
    void addToKillRing(Range*, bool prepend);
    bool m_startNewKillRingSequence;
#else
    void addToKillRing(Range*, bool) { }
#endif

};

} // namespace WebCore

#endif // Editor_h
