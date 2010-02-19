/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EditorClientImpl_h
#define EditorClientImpl_h

#include "EditorClient.h"
#include "Timer.h"
#include <wtf/Deque.h>

namespace WebCore {
class HTMLInputElement;
}

namespace WebKit {
class WebViewImpl;

class EditorClientImpl : public WebCore::EditorClient {
public:
    EditorClientImpl(WebViewImpl* webView);

    virtual ~EditorClientImpl();
    virtual void pageDestroyed();

    virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*);
    virtual bool smartInsertDeleteEnabled();
    virtual bool isSelectTrailingWhitespaceEnabled();
    virtual bool isContinuousSpellCheckingEnabled();
    virtual void toggleContinuousSpellChecking();
    virtual bool isGrammarCheckingEnabled();
    virtual void toggleGrammarChecking();
    virtual int spellCheckerDocumentTag();
    virtual bool isEditable();
    virtual bool shouldBeginEditing(WebCore::Range*);
    virtual bool shouldEndEditing(WebCore::Range*);
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction);
    virtual bool shouldInsertText(const WebCore::String&, WebCore::Range*, WebCore::EditorInsertAction);
    virtual bool shouldDeleteRange(WebCore::Range*);
    virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange,
                                           WebCore::Range* toRange,
                                           WebCore::EAffinity,
                                           bool stillSelecting);
    virtual bool shouldApplyStyle(WebCore::CSSStyleDeclaration*, WebCore::Range*);
    virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*);
    virtual void didBeginEditing();
    virtual void respondToChangedContents();
    virtual void respondToChangedSelection();
    virtual void didEndEditing();
    virtual void didWriteSelectionToPasteboard();
    virtual void didSetSelectionTypesForPasteboard();
    virtual void registerCommandForUndo(PassRefPtr<WebCore::EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<WebCore::EditCommand>);
    virtual void clearUndoRedoOperations();
    virtual bool canUndo() const;
    virtual bool canRedo() const;
    virtual void undo();
    virtual void redo();
    virtual const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
    virtual bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
    virtual void handleKeyboardEvent(WebCore::KeyboardEvent*);
    virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*);
    virtual void textFieldDidBeginEditing(WebCore::Element*);
    virtual void textFieldDidEndEditing(WebCore::Element*);
    virtual void textDidChangeInTextField(WebCore::Element*);
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(WebCore::Element*);
    virtual void textDidChangeInTextArea(WebCore::Element*);
    virtual void ignoreWordInSpellDocument(const WebCore::String&);
    virtual void learnWord(const WebCore::String&);
    virtual void checkSpellingOfString(const UChar*, int length,
                                       int* misspellingLocation,
                                       int* misspellingLength);
    virtual void checkGrammarOfString(const UChar*, int length,
                                      WTF::Vector<WebCore::GrammarDetail>&,
                                      int* badGrammarLocation,
                                      int* badGrammarLength);
    virtual WebCore::String getAutoCorrectSuggestionForMisspelledWord(const WebCore::String&);
    virtual void updateSpellingUIWithGrammarString(const WebCore::String&, const WebCore::GrammarDetail&);
    virtual void updateSpellingUIWithMisspelledWord(const WebCore::String&);
    virtual void showSpellingUI(bool show);
    virtual bool spellingUIIsShowing();
    virtual void getGuessesForWord(const WebCore::String& word,
                                   WTF::Vector<WebCore::String>& guesses);
    virtual void setInputMethodState(bool enabled);

    // Shows the form autofill popup for |node| if it is an HTMLInputElement and
    // it is empty.  This is called when you press the up or down arrow in a
    // text-field or when clicking an already focused text-field.
    // Returns true if the autofill popup has been scheduled to be shown, false
    // otherwise.
    virtual bool showFormAutofillForNode(WebCore::Node*);

    // Notification that the text changed due to acceptance of a suggestion
    // provided by an Autocomplete popup.  Having a separate callback in this
    // case is a simple way to break the cycle that would otherwise occur if
    // textDidChangeInTextField was called.
    virtual void onAutocompleteSuggestionAccepted(WebCore::HTMLInputElement*);

private:
    void modifySelection(WebCore::Frame*, WebCore::KeyboardEvent*);

    // Triggers autofill for an input element if applicable.  This can be form
    // autofill (via a popup-menu) or password autofill depending on the
    // input element.  If |formAutofillOnly| is true, password autofill is not
    // triggered.
    // |autofillOnEmptyValue| indicates whether the autofill should be shown
    // when the text-field is empty.
    // If |requiresCaretAtEnd| is true, the autofill popup is only shown if the
    // caret is located at the end of the entered text.
    // Returns true if the autofill popup has been scheduled to be shown, false
    // otherwise.
    bool autofill(WebCore::HTMLInputElement*,
                  bool formAutofillOnly, bool autofillOnEmptyValue,
                  bool requiresCaretAtEnd);

    // Called to process the autofill described by m_autofillArgs.
    // This method is invoked asynchronously if the caret position is not
    // reflecting the last text change yet, and we need it to decide whether or
    // not to show the autofill popup.
    void doAutofill(WebCore::Timer<EditorClientImpl>*);

    void cancelPendingAutofill();

    // Returns whether or not the focused control needs spell-checking.
    // Currently, this function just retrieves the focused node and determines
    // whether or not it is a <textarea> element or an element whose
    // contenteditable attribute is true.
    // FIXME: Bug 740540: This code just implements the default behavior
    // proposed in this issue. We should also retrieve "spellcheck" attributes
    // for text fields and create a flag to over-write the default behavior.
    bool shouldSpellcheckByDefault();

    WebViewImpl* m_webView;
    bool m_inRedo;

    typedef Deque<RefPtr<WebCore::EditCommand> > EditCommandStack;
    EditCommandStack m_undoStack;
    EditCommandStack m_redoStack;

    // Whether the last entered key was a backspace.
    bool m_backspaceOrDeletePressed;

    // This flag is set to false if spell check for this editor is manually
    // turned off. The default setting is SpellCheckAutomatic.
    enum {
        SpellCheckAutomatic,
        SpellCheckForcedOn,
        SpellCheckForcedOff
    };
    int m_spellCheckThisFieldStatus;

    // Used to delay autofill processing.
    WebCore::Timer<EditorClientImpl> m_autofillTimer;

    struct AutofillArgs {
        RefPtr<WebCore::HTMLInputElement> inputElement;
        bool autofillFormOnly;
        bool autofillOnEmptyValue;
        bool requireCaretAtEnd;
        bool backspaceOrDeletePressed;
    };
    OwnPtr<AutofillArgs> m_autofillArgs;
};

} // namespace WebKit

#endif
