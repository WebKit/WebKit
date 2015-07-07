/*
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EditorClientWinCE_h
#define EditorClientWinCE_h

#include "EditorClient.h"
#include "TextCheckerClient.h"

class WebView;

namespace WebKit {

class EditorClientWinCE : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    EditorClientWinCE(WebView*);
    ~EditorClientWinCE();

    virtual void pageDestroyed() override;

    virtual bool shouldDeleteRange(WebCore::Range*) override;
    virtual bool smartInsertDeleteEnabled() override;
    virtual bool isSelectTrailingWhitespaceEnabled() override;
    virtual bool isContinuousSpellCheckingEnabled() override;
    virtual void toggleContinuousSpellChecking() override;
    virtual bool isGrammarCheckingEnabled() override;
    virtual void toggleGrammarChecking() override;
    virtual int spellCheckerDocumentTag() override;

    virtual bool shouldBeginEditing(WebCore::Range*) override;
    virtual bool shouldEndEditing(WebCore::Range*) override;
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction) override;
    virtual bool shouldInsertText(const WTF::String&, WebCore::Range*, WebCore::EditorInsertAction) override;
    virtual bool shouldChangeSelectedRange(WebCore::Range*, WebCore::Range*, WebCore::EAffinity, bool) override;

    virtual bool shouldApplyStyle(WebCore::StylePropertySet*, WebCore::Range*) override;
    virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*) override;

    virtual void didBeginEditing() override;
    virtual void respondToChangedContents() override;
    virtual void respondToChangedSelection(WebCore::Frame*) override;
    virtual void didEndEditing() override;
    virtual void willWriteSelectionToPasteboard(WebCore::Range*) override;
    virtual void didWriteSelectionToPasteboard() override;
    virtual void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer> >& pasteboardData) override;

    virtual void registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep>) override;
    virtual void registerRedoStep(WTF::PassRefPtr<WebCore::UndoStep>) override;
    virtual void clearUndoRedoOperations() override;

    virtual bool canCopyCut(WebCore::Frame*, bool defaultValue) const override;
    virtual bool canPaste(WebCore::Frame*, bool defaultValue) const override;
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;

    virtual void undo() override;
    virtual void redo() override;

    virtual void handleKeyboardEvent(WebCore::KeyboardEvent*) override;
    virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*) override;

    virtual void textFieldDidBeginEditing(WebCore::Element*) override;
    virtual void textFieldDidEndEditing(WebCore::Element*) override;
    virtual void textDidChangeInTextField(WebCore::Element*) override;
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) override;
    virtual void textWillBeDeletedInTextField(WebCore::Element*) override;
    virtual void textDidChangeInTextArea(WebCore::Element*) override;

    virtual bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const override;
    virtual void ignoreWordInSpellDocument(const WTF::String&) override;
    virtual void learnWord(const WTF::String&) override;
    virtual void checkSpellingOfString(StringView, int*, int*) override;
    virtual WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&) override;
    virtual void checkGrammarOfString(StringView, WTF::Vector<WebCore::GrammarDetail>&, int*, int*) override;
    virtual void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&) override;
    virtual void updateSpellingUIWithMisspelledWord(const WTF::String&) override;
    virtual void showSpellingUI(bool) override;
    virtual bool spellingUIIsShowing() override;
    virtual void getGuessesForWord(const WTF::String& word, const WTF::String& context, WTF::Vector<WTF::String>& guesses) override;
    virtual void willSetInputMethodState() override;
    virtual void setInputMethodState(bool) override;
    virtual void requestCheckingOfString(WTF::PassRefPtr<WebCore::TextCheckingRequest>) override { }
    virtual WebCore::TextCheckerClient* textChecker() override { return this; }

private:
    const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);

    WebView* m_webView;
};

} // namespace WebKit

#endif // EditorClientWinCE_h
