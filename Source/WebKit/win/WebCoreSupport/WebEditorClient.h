/*
 * Copyright (C) 2006-2007 Apple Inc.  All rights reserved.
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

#ifndef WebEditorClient_H
#define WebEditorClient_H

#include "WebKit.h"
#include <WebCore/EditorClient.h>
#include <WebCore/TextCheckerClient.h>

class WebView;
class WebNotification;
class WebEditorUndoTarget;

class WebEditorClient : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    WebEditorClient(WebView*);
    ~WebEditorClient();

    virtual void pageDestroyed();

    virtual bool isContinuousSpellCheckingEnabled();
    virtual void toggleGrammarChecking();
    virtual bool isGrammarCheckingEnabled();
    virtual void toggleContinuousSpellChecking();
    virtual int spellCheckerDocumentTag();

    virtual bool shouldBeginEditing(WebCore::Range*);
    virtual bool shouldEndEditing(WebCore::Range*);
    virtual bool shouldInsertText(const WTF::String&, WebCore::Range*, WebCore::EditorInsertAction);

    virtual void didBeginEditing();
    virtual void didEndEditing();
    virtual void willWriteSelectionToPasteboard(WebCore::Range*);
    virtual void didWriteSelectionToPasteboard();
    virtual void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer> >& pasteboardData);

    virtual void respondToChangedContents();
    virtual void respondToChangedSelection(WebCore::Frame*);
    void didChangeSelectionAndUpdateLayout() override { }
    void discardedComposition(WebCore::Frame*) override;

    bool shouldDeleteRange(WebCore::Range*);

    bool shouldInsertNode(WebCore::Node*, WebCore::Range* replacingRange, WebCore::EditorInsertAction);
    bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*);
    void didApplyStyle();
    bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*);
    bool shouldChangeTypingStyle(WebCore::StyleProperties* currentStyle, WebCore::StyleProperties* toProposedStyle);

    void webViewDidChangeTypingStyle(WebNotification*);
    void webViewDidChangeSelection(WebNotification*);

    bool smartInsertDeleteEnabled();
    bool isSelectTrailingWhitespaceEnabled();

    void registerUndoStep(PassRefPtr<WebCore::UndoStep>);
    void registerRedoStep(PassRefPtr<WebCore::UndoStep>);
    void clearUndoRedoOperations();

    bool canCopyCut(WebCore::Frame*, bool defaultValue) const;
    bool canPaste(WebCore::Frame*, bool defaultValue) const;
    bool canUndo() const;
    bool canRedo() const;
    
    void undo();
    void redo();    
    
    bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) override;
    void textFieldDidBeginEditing(WebCore::Element*) override;
    void textFieldDidEndEditing(WebCore::Element*) override;
    void textDidChangeInTextField(WebCore::Element*) override;
    bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) override;
    void textWillBeDeletedInTextField(WebCore::Element* input) override;
    void textDidChangeInTextArea(WebCore::Element*) override;
    void overflowScrollPositionChanged() override { }

    void handleKeyboardEvent(WebCore::KeyboardEvent*);
    void handleInputMethodKeydown(WebCore::KeyboardEvent*);

    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const override;
    void ignoreWordInSpellDocument(const WTF::String&) override;
    void learnWord(const WTF::String&) override;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) override;
    WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&) override;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) override;
    void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&) override;
    void updateSpellingUIWithMisspelledWord(const WTF::String&) override;
    void showSpellingUI(bool show) override;
    bool spellingUIIsShowing() override;
    void getGuessesForWord(const WTF::String& word, const WTF::String& context, const WebCore::VisibleSelection& currentSelection, WTF::Vector<WTF::String>& guesses) override;

    void willSetInputMethodState() override;
    void setInputMethodState(bool) override;
    void requestCheckingOfString(WTF::PassRefPtr<WebCore::TextCheckingRequest>, const WebCore::VisibleSelection&) override { }

    WebCore::TextCheckerClient* textChecker() override { return this; }

private:
    WebView* m_webView;
    WebEditorUndoTarget* m_undoTarget;
};

#endif // WebEditorClient_H
