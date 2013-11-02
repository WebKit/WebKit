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

    virtual void pageDestroyed() OVERRIDE;

    virtual bool shouldDeleteRange(WebCore::Range*) OVERRIDE;
    virtual bool smartInsertDeleteEnabled() OVERRIDE;
    virtual bool isSelectTrailingWhitespaceEnabled() OVERRIDE;
    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE;
    virtual void toggleContinuousSpellChecking() OVERRIDE;
    virtual bool isGrammarCheckingEnabled() OVERRIDE;
    virtual void toggleGrammarChecking() OVERRIDE;
    virtual int spellCheckerDocumentTag() OVERRIDE;

    virtual bool shouldBeginEditing(WebCore::Range*) OVERRIDE;
    virtual bool shouldEndEditing(WebCore::Range*) OVERRIDE;
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction) OVERRIDE;
    virtual bool shouldInsertText(const WTF::String&, WebCore::Range*, WebCore::EditorInsertAction) OVERRIDE;
    virtual bool shouldChangeSelectedRange(WebCore::Range*, WebCore::Range*, WebCore::EAffinity, bool) OVERRIDE;

    virtual bool shouldApplyStyle(WebCore::StylePropertySet*, WebCore::Range*) OVERRIDE;
    virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*) OVERRIDE;

    virtual void didBeginEditing() OVERRIDE;
    virtual void respondToChangedContents() OVERRIDE;
    virtual void respondToChangedSelection(WebCore::Frame*) OVERRIDE;
    virtual void didEndEditing() OVERRIDE;
    virtual void willWriteSelectionToPasteboard(WebCore::Range*) OVERRIDE;
    virtual void didWriteSelectionToPasteboard() OVERRIDE;
    virtual void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer> >& pasteboardData) OVERRIDE;

    virtual void registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep>) OVERRIDE;
    virtual void registerRedoStep(WTF::PassRefPtr<WebCore::UndoStep>) OVERRIDE;
    virtual void clearUndoRedoOperations() OVERRIDE;

    virtual bool canCopyCut(WebCore::Frame*, bool defaultValue) const OVERRIDE;
    virtual bool canPaste(WebCore::Frame*, bool defaultValue) const OVERRIDE;
    virtual bool canUndo() const OVERRIDE;
    virtual bool canRedo() const OVERRIDE;

    virtual void undo() OVERRIDE;
    virtual void redo() OVERRIDE;

    virtual void handleKeyboardEvent(WebCore::KeyboardEvent*) OVERRIDE;
    virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*) OVERRIDE;

    virtual void textFieldDidBeginEditing(WebCore::Element*) OVERRIDE;
    virtual void textFieldDidEndEditing(WebCore::Element*) OVERRIDE;
    virtual void textDidChangeInTextField(WebCore::Element*) OVERRIDE;
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) OVERRIDE;
    virtual void textWillBeDeletedInTextField(WebCore::Element*) OVERRIDE;
    virtual void textDidChangeInTextArea(WebCore::Element*) OVERRIDE;

    virtual bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const OVERRIDE;
    virtual void ignoreWordInSpellDocument(const WTF::String&) OVERRIDE;
    virtual void learnWord(const WTF::String&) OVERRIDE;
    virtual void checkSpellingOfString(const UChar*, int, int*, int*) OVERRIDE;
    virtual WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&) OVERRIDE;
    virtual void checkGrammarOfString(const UChar*, int, WTF::Vector<WebCore::GrammarDetail>&, int*, int*) OVERRIDE;
    virtual void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&) OVERRIDE;
    virtual void updateSpellingUIWithMisspelledWord(const WTF::String&) OVERRIDE;
    virtual void showSpellingUI(bool) OVERRIDE;
    virtual bool spellingUIIsShowing() OVERRIDE;
    virtual void getGuessesForWord(const WTF::String& word, const WTF::String& context, WTF::Vector<WTF::String>& guesses) OVERRIDE;
    virtual void willSetInputMethodState() OVERRIDE;
    virtual void setInputMethodState(bool) OVERRIDE;
    virtual void requestCheckingOfString(WTF::PassRefPtr<WebCore::TextCheckingRequest>) OVERRIDE { }
    virtual WebCore::TextCheckerClient* textChecker() OVERRIDE { return this; }

private:
    const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);

    WebView* m_webView;
};

} // namespace WebKit

#endif // EditorClientWinCE_h
