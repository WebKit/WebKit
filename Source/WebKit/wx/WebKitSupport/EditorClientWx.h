/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#ifndef EditorClientWx_h
#define EditorClientWx_h

#include "EditorClient.h"
#include "Page.h"
#include "TextCheckerClient.h"

#include "WebView.h"
#include "WebFrame.h"

namespace WebCore {

class EditorClientWx : public EditorClient, public TextCheckerClient {
friend class WebKit::WebView;
friend class WebKit::WebFrame;

public:
    virtual ~EditorClientWx();
    void setPage(Page*);
    virtual void pageDestroyed();
    virtual void frameWillDetachPage(Frame*) { }

    virtual bool shouldDeleteRange(Range*);
    virtual bool shouldShowDeleteInterface(HTMLElement*);
    virtual bool smartInsertDeleteEnabled();
    virtual bool isSelectTrailingWhitespaceEnabled();
    virtual bool isContinuousSpellCheckingEnabled();
    virtual void toggleContinuousSpellChecking();
    virtual bool isGrammarCheckingEnabled();
    virtual void toggleGrammarChecking();
    virtual int spellCheckerDocumentTag();

    virtual bool selectWordBeforeMenuEvent();

    virtual bool shouldBeginEditing(Range*);
    virtual bool shouldEndEditing(Range*);
    virtual bool shouldInsertNode(Node*, Range*,
                                  EditorInsertAction);
    virtual bool shouldInsertText(const String&, Range*,
                                  EditorInsertAction);
    virtual bool shouldApplyStyle(StylePropertySet*,
                                  Range*);
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*);
    virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, 
                                EAffinity, bool stillSelecting);

    virtual void didBeginEditing();
    virtual void respondToChangedContents();
    virtual void respondToChangedSelection(Frame*);
    virtual void didEndEditing();
    virtual void didWriteSelectionToPasteboard();
    virtual void didSetSelectionTypesForPasteboard();

    virtual void registerUndoStep(PassRefPtr<UndoStep>);
    virtual void registerRedoStep(PassRefPtr<UndoStep>);
    virtual void clearUndoRedoOperations();

    virtual bool canCopyCut(Frame*, bool defaultValue) const;
    virtual bool canPaste(Frame*, bool defaultValue) const;
    virtual bool canUndo() const;
    virtual bool canRedo() const;

    virtual void undo();
    virtual void redo();
    
    virtual const char* interpretKeyEvent(const KeyboardEvent*);
    virtual bool handleEditingKeyboardEvent(KeyboardEvent*);
    virtual void handleKeyboardEvent(KeyboardEvent*);
    virtual void handleInputMethodKeydown(KeyboardEvent*);
    
    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);

    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const;
    virtual void ignoreWordInSpellDocument(const String&);
    virtual void learnWord(const String&);
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength);
    virtual void checkGrammarOfString(const UChar*, int length, Vector<GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength);
    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail& detail);
    virtual void updateSpellingUIWithMisspelledWord(const String&);
    virtual void showSpellingUI(bool show);
    virtual bool spellingUIIsShowing();
    virtual void getGuessesForWord(const String& word, const String& context, Vector<String>& guesses);
    virtual String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&);

    virtual void willSetInputMethodState();
    virtual void setInputMethodState(bool enabled);
    virtual void requestCheckingOfString(WebCore::SpellChecker*, const WebCore::TextCheckingRequest&) { }
    virtual TextCheckerClient* textChecker() { return this; }

private:
    Page* m_page;
};

}

#endif // EditorClientWx_h
