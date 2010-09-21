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

#include "config.h"
#include "EditorClientWinCE.h"

#include "EditCommand.h"
#include "NotImplemented.h"

using namespace WebCore;

namespace WebKit {

EditorClientWinCE::EditorClientWinCE(WebView* webView)
    : m_webView(webView)
{
}

EditorClientWinCE::~EditorClientWinCE()
{
}

void EditorClientWinCE::setInputMethodState(bool active)
{
    notImplemented();
}

bool EditorClientWinCE::shouldDeleteRange(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWinCE::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientWinCE::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientWinCE::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClientWinCE::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientWinCE::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWinCE::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWinCE::shouldInsertText(const String&, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

bool EditorClientWinCE::shouldChangeSelectedRange(Range*, Range*, EAffinity, bool)
{
    notImplemented();
    return true;
}

bool EditorClientWinCE::shouldApplyStyle(WebCore::CSSStyleDeclaration*, WebCore::Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWinCE::shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*)
{
    notImplemented();
    return true;
}

void EditorClientWinCE::didBeginEditing()
{
    notImplemented();
}

void EditorClientWinCE::respondToChangedContents()
{
    notImplemented();
}

void EditorClientWinCE::respondToChangedSelection()
{
    notImplemented();
}

void EditorClientWinCE::didEndEditing()
{
    notImplemented();
}

void EditorClientWinCE::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientWinCE::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

bool EditorClientWinCE::isEditable()
{
    notImplemented();
    return false;
}

void EditorClientWinCE::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand> command)
{
    notImplemented();
}

void EditorClientWinCE::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand> command)
{
    notImplemented();
}

void EditorClientWinCE::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientWinCE::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientWinCE::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientWinCE::undo()
{
    notImplemented();
}

void EditorClientWinCE::redo()
{
    notImplemented();
}

bool EditorClientWinCE::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
    return true;
}

void EditorClientWinCE::pageDestroyed()
{
    delete this;
}

bool EditorClientWinCE::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientWinCE::isSelectTrailingWhitespaceEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWinCE::toggleContinuousSpellChecking()
{
    notImplemented();
}

void EditorClientWinCE::toggleGrammarChecking()
{
    notImplemented();
}

void EditorClientWinCE::handleKeyboardEvent(KeyboardEvent* event)
{
    notImplemented();
}

void EditorClientWinCE::handleInputMethodKeydown(KeyboardEvent* event)
{
    notImplemented();
}

void EditorClientWinCE::textFieldDidBeginEditing(Element*)
{
}

void EditorClientWinCE::textFieldDidEndEditing(Element*)
{
}

void EditorClientWinCE::textDidChangeInTextField(Element*)
{
}

bool EditorClientWinCE::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    return false;
}

void EditorClientWinCE::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientWinCE::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientWinCE::ignoreWordInSpellDocument(const String& text)
{
    notImplemented();
}

void EditorClientWinCE::learnWord(const String& text)
{
    notImplemented();
}

void EditorClientWinCE::checkSpellingOfString(const UChar* text, int length, int* misspellingLocation, int* misspellingLength)
{
    notImplemented();
}

String EditorClientWinCE::getAutoCorrectSuggestionForMisspelledWord(const String& inputWord)
{
    // This method can be implemented using customized algorithms for the particular browser.
    // Currently, it computes an empty string.
    return String();
}

void EditorClientWinCE::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientWinCE::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientWinCE::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientWinCE::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientWinCE::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientWinCE::getGuessesForWord(const String& word, WTF::Vector<String>& guesses)
{
    notImplemented();
}

void EditorClientWinCE::willSetInputMethodState()
{
    notImplemented();
}

} // namespace WebKit
