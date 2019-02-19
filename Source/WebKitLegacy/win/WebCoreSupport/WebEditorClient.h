/*
 * Copyright (C) 2006-2017 Apple Inc.  All rights reserved.
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

#include "WebKit.h"
#include <WebCore/EditorClient.h>
#include <WebCore/TextCheckerClient.h>

class WebView;
class WebNotification;
class WebEditorUndoTarget;

class WebEditorClient final : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    WebEditorClient(WebView*);
    ~WebEditorClient();

private:
    bool isContinuousSpellCheckingEnabled() final;
    void toggleGrammarChecking() final;
    bool isGrammarCheckingEnabled() final;
    void toggleContinuousSpellChecking() final;
    int spellCheckerDocumentTag() final;

    bool shouldBeginEditing(WebCore::Range*) final;
    bool shouldEndEditing(WebCore::Range*) final;
    bool shouldInsertText(const WTF::String&, WebCore::Range*, WebCore::EditorInsertAction) final;

    void didBeginEditing() final;
    void didEndEditing() final;
    void willWriteSelectionToPasteboard(WebCore::Range*) final;
    void didWriteSelectionToPasteboard() final;
    void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) final;

    void didEndUserTriggeredSelectionChanges() final { }
    void respondToChangedContents() final;
    void respondToChangedSelection(WebCore::Frame*) final;
    void updateEditorStateAfterLayoutIfEditabilityChanged() final { } 
    void canceledComposition() final;
    void discardedComposition(WebCore::Frame*) final;
    void didUpdateComposition() final { }

    bool shouldDeleteRange(WebCore::Range*) final;

    bool shouldInsertNode(WebCore::Node*, WebCore::Range* replacingRange, WebCore::EditorInsertAction) final;
    bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*) final;
    void didApplyStyle() final;
    bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*) final;

    bool smartInsertDeleteEnabled() final;
    bool isSelectTrailingWhitespaceEnabled() const final;

    void registerUndoStep(WebCore::UndoStep&) final;
    void registerRedoStep(WebCore::UndoStep&) final;
    void clearUndoRedoOperations();

    bool canCopyCut(WebCore::Frame*, bool defaultValue) const final;
    bool canPaste(WebCore::Frame*, bool defaultValue) const final;
    bool canUndo() const final;
    bool canRedo() const final;
    
    void undo() final;
    void redo() final;
    
    bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) final;
    void textFieldDidBeginEditing(WebCore::Element*) final;
    void textFieldDidEndEditing(WebCore::Element*) final;
    void textDidChangeInTextField(WebCore::Element*) final;
    bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) final;
    void textWillBeDeletedInTextField(WebCore::Element* input) final;
    void textDidChangeInTextArea(WebCore::Element*) final;
    void overflowScrollPositionChanged() final { }

    void handleKeyboardEvent(WebCore::KeyboardEvent*) final;
    void handleInputMethodKeydown(WebCore::KeyboardEvent*) final;

    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const final;
    void ignoreWordInSpellDocument(const WTF::String&) final;
    void learnWord(const WTF::String&) final;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) final;
    WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&) final;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) final;
    void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&) final;
    void updateSpellingUIWithMisspelledWord(const WTF::String&) final;
    void showSpellingUI(bool show) final;
    bool spellingUIIsShowing() final;
    void getGuessesForWord(const WTF::String& word, const WTF::String& context, const WebCore::VisibleSelection& currentSelection, WTF::Vector<WTF::String>& guesses) final;

    void willSetInputMethodState() final;
    void setInputMethodState(bool) final;
    void requestCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection&) final { }
    bool performTwoStepDrop(WebCore::DocumentFragment&, WebCore::Range&, bool) final { return false; }

    bool requestDOMPasteAccess() final { return false; }

    WebCore::TextCheckerClient* textChecker() final { return this; }

    WebView* m_webView;
    WebEditorUndoTarget* m_undoTarget;
};
