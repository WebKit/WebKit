/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebEditorClient_h
#define WebEditorClient_h

#include <WebCore/EditorClient.h>
#include <WebCore/TextCheckerClient.h>

namespace WebKit {

class WebPage;

class WebEditorClient : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    WebEditorClient(WebPage* page)
        : m_page(page)
    {
    }

private:
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
    virtual bool shouldInsertText(const String&, WebCore::Range*, WebCore::EditorInsertAction) override;
    virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) override;
    
    virtual bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*) override;
    virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*) override;

    virtual void didBeginEditing() override;
    virtual void respondToChangedContents() override;
    virtual void respondToChangedSelection(WebCore::Frame*) override;
    virtual void didEndEditing() override;
    virtual void willWriteSelectionToPasteboard(WebCore::Range*) override;
    virtual void didWriteSelectionToPasteboard() override;
    virtual void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) override;
    
    virtual void registerUndoStep(PassRefPtr<WebCore::UndoStep>) override;
    virtual void registerRedoStep(PassRefPtr<WebCore::UndoStep>) override;
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

#if PLATFORM(COCOA)
    virtual NSString *userVisibleString(NSURL *) override;
    virtual WebCore::DocumentFragment* documentFragmentFromAttributedString(NSAttributedString *, Vector< RefPtr<WebCore::ArchiveResource>>&) override;
    virtual void setInsertionPasteboard(const String& pasteboardName) override;
    virtual NSURL* canonicalizeURL(NSURL*) override;
    virtual NSURL* canonicalizeURLString(NSString*) override;
#endif

#if USE(APPKIT)
    virtual void uppercaseWord() override;
    virtual void lowercaseWord() override;
    virtual void capitalizeWord() override;
#endif
#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    virtual void showSubstitutionsPanel(bool show) override;
    virtual bool substitutionsPanelIsShowing() override;
    virtual void toggleSmartInsertDelete() override;
    virtual bool isAutomaticQuoteSubstitutionEnabled() override;
    virtual void toggleAutomaticQuoteSubstitution() override;
    virtual bool isAutomaticLinkDetectionEnabled() override;
    virtual void toggleAutomaticLinkDetection() override;
    virtual bool isAutomaticDashSubstitutionEnabled() override;
    virtual void toggleAutomaticDashSubstitution() override;
    virtual bool isAutomaticTextReplacementEnabled() override;
    virtual void toggleAutomaticTextReplacement() override;
    virtual bool isAutomaticSpellingCorrectionEnabled() override;
    virtual void toggleAutomaticSpellingCorrection() override;
#endif

#if ENABLE(DELETION_UI)
    virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*) override;
#endif

#if PLATFORM(GTK)
    bool executePendingEditorCommands(WebCore::Frame*, const Vector<WTF::String>&, bool);
    void getEditorCommandsForKeyEvent(const WebCore::KeyboardEvent*, Vector<WTF::String>&);
    void updateGlobalSelection(WebCore::Frame*);
#endif

    TextCheckerClient* textChecker()  override { return this; }

    virtual bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const override;
    virtual void ignoreWordInSpellDocument(const String&) override;
    virtual void learnWord(const String&) override;
    virtual void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) override;
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord) override;
    virtual void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) override;
#if USE(UNIFIED_TEXT_CHECKING)
    virtual Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, WebCore::TextCheckingTypeMask checkingTypes) override;
#endif
    virtual void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) override;
    virtual void updateSpellingUIWithMisspelledWord(const String&) override;
    virtual void showSpellingUI(bool show) override;
    virtual bool spellingUIIsShowing() override;
    virtual void getGuessesForWord(const String& word, const String& context, Vector<String>& guesses) override;
    virtual void willSetInputMethodState() override;
    virtual void setInputMethodState(bool enabled) override;
    virtual void requestCheckingOfString(WTF::PassRefPtr<WebCore::TextCheckingRequest>) override;
#if PLATFORM(GTK)
    virtual bool shouldShowUnicodeMenu() override;
#endif
#if PLATFORM(IOS)
    virtual void startDelayingAndCoalescingContentChangeNotifications() override;
    virtual void stopDelayingAndCoalescingContentChangeNotifications() override;
    virtual void writeDataToPasteboard(NSDictionary*) override;
    virtual NSArray *supportedPasteboardTypesForCurrentSelection() override;
    virtual NSArray *readDataFromPasteboard(NSString* type, int index) override;
    virtual bool hasRichlyEditableSelection() override;
    virtual int getPasteboardItemsCount() override;
    virtual WebCore::DocumentFragment* documentFragmentFromDelegate(int index) override;
    virtual bool performsTwoStepPaste(WebCore::DocumentFragment*) override;
    virtual int pasteboardChangeCount() override;
#endif

    virtual bool supportsGlobalSelection() override;

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    virtual void selectedTelephoneNumberRangesChanged(const Vector<RefPtr<WebCore::Range>>&) override;
#endif

    WebPage* m_page;
};

} // namespace WebKit

#endif // WebEditorClient_h
