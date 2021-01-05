/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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

#ifndef EditorClientHaiku_h
#define EditorClientHaiku_h

#include "EditorClient.h"
#include "Page.h"
#include "TextCheckerClient.h"
#include <wtf/RefCounted.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include "wtf/text/StringView.h"
#include <String.h>

class BMessage;
class BWebPage;

namespace WebCore {
class PlatformKeyboardEvent;

class EditorClientHaiku : public EditorClient, public TextCheckerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    EditorClientHaiku(BWebPage* page);

    bool shouldDeleteRange(const Optional<SimpleRange>&) override;
    bool smartInsertDeleteEnabled() override;
    bool isContinuousSpellCheckingEnabled() override;
    void toggleContinuousSpellChecking() override;
    bool isGrammarCheckingEnabled() override;
    void toggleGrammarChecking() override;
    int spellCheckerDocumentTag() override;

    bool shouldBeginEditing(const SimpleRange&) override;
    bool shouldEndEditing(const SimpleRange&) override;
    bool shouldInsertNode(Node&, const Optional<SimpleRange>&, EditorInsertAction) override;
    bool shouldInsertText(const String&, const Optional <SimpleRange>&, EditorInsertAction) override;
    bool shouldChangeSelectedRange(const Optional<SimpleRange>& fromRange, const Optional<SimpleRange>& toRange,
                                           Affinity, bool stillSelecting) override;

    bool shouldApplyStyle(const StyleProperties&, const Optional<SimpleRange>&) override;
    bool shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&) override;

    void didBeginEditing() override;
    void didEndEditing() override;
    void didApplyStyle() override {};

    bool isSelectTrailingWhitespaceEnabled() const override;
    void willWriteSelectionToPasteboard(const Optional<SimpleRange>&) override;
    void didWriteSelectionToPasteboard() override;
    void getClientPasteboardData(const Optional<SimpleRange>&, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer> >& pasteboardData) override;

    void respondToChangedContents() override;
    void respondToChangedSelection(Frame*) override;
    void discardedComposition(Frame*) override;
    void canceledComposition() override;

    void registerUndoStep(UndoStep&) override;
    void registerRedoStep(UndoStep&) override;
    void clearUndoRedoOperations() override;

    bool canCopyCut(Frame*, bool defaultValue) const override;
    bool canPaste(Frame*, bool defaultValue) const override;
    bool canUndo() const override;
    bool canRedo() const override;

    void undo() override;
    void redo() override;

    void handleKeyboardEvent(KeyboardEvent&) override;
    void handleInputMethodKeydown(KeyboardEvent&) override;

    void textFieldDidBeginEditing(Element*) override;
    void textFieldDidEndEditing(Element*) override;
    void textDidChangeInTextField(Element*) override;
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) override;
    void textWillBeDeletedInTextField(Element*) override;
    void textDidChangeInTextArea(Element*) override;
    void overflowScrollPositionChanged() override;
    void updateEditorStateAfterLayoutIfEditabilityChanged() override {};
	void didEndUserTriggeredSelectionChanges() override {};
	void didUpdateComposition() override {};

    TextCheckerClient* textChecker() override { return this; }

    void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) override;
    void updateSpellingUIWithMisspelledWord(const String&) override;
    void showSpellingUI(bool show) override;
    bool spellingUIIsShowing() override;
    void willSetInputMethodState() override;
    void setInputMethodState(Element*) override;

    bool performTwoStepDrop(DocumentFragment&, const SimpleRange& destination, bool isMove) override;
    // TextCheckerClient

    bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const override;
    void ignoreWordInSpellDocument(const String&) override;
    void learnWord(const String&) override;
    void checkSpellingOfString(StringView, int* misspellingLocation,
                                       int* misspellingLength) override;
    String getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord) override;
    void checkGrammarOfString(StringView, Vector<GrammarDetail>&,
                                      int* badGrammarLocation, int* badGrammarLength) override;

    void getGuessesForWord(const String& word, const String& context, const WebCore::VisibleSelection&, Vector<String>& guesses) override;
    void requestCheckingOfString(TextCheckingRequest&, const VisibleSelection& currentSelection) override;

	WebCore::DOMPasteAccessResponse requestDOMPasteAccess(const String&) final { return WebCore::DOMPasteAccessResponse::DeniedForGesture;}

	bool canShowFontPanel() const final { return false; }
	void subFrameScrollPositionChanged() final {}

private:
    bool handleEditingKeyboardEvent(KeyboardEvent* event,
        const PlatformKeyboardEvent* platformEvent);
    void setPendingComposition(const char* newComposition);
    void setPendingPreedit(const char* newPreedit);
    void clearPendingIMData();
    void imContextCommitted(const char* str, EditorClient* client);
    void imContextPreeditChanged(EditorClient* client);

    void dispatchMessage(BMessage& message);

    BWebPage* m_page;

    typedef Deque<RefPtr<WebCore::UndoStep>> UndoManagerStack;
    UndoManagerStack m_undoStack;
    UndoManagerStack m_redoStack;

    bool m_isInRedo;

    BString m_pendingComposition;
    BString m_pendingPreedit;
};

} // namespace WebCore

#endif // EditorClientHaiku_h

