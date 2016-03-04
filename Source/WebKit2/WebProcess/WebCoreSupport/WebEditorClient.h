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
    void pageDestroyed() override;

    bool shouldDeleteRange(WebCore::Range*) override;
    bool smartInsertDeleteEnabled() override;
    bool isSelectTrailingWhitespaceEnabled() override;
    bool isContinuousSpellCheckingEnabled() override;
    void toggleContinuousSpellChecking() override;
    bool isGrammarCheckingEnabled() override;
    void toggleGrammarChecking() override;
    int spellCheckerDocumentTag() override;
    
    bool shouldBeginEditing(WebCore::Range*) override;
    bool shouldEndEditing(WebCore::Range*) override;
    bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction) override;
    bool shouldInsertText(const String&, WebCore::Range*, WebCore::EditorInsertAction) override;
    bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) override;
    
    bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*) override;
    void didApplyStyle() override;
    bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*) override;

    void didBeginEditing() override;
    void respondToChangedContents() override;
    void respondToChangedSelection(WebCore::Frame*) override;
    void didChangeSelectionAndUpdateLayout() override;
    void discardedComposition(WebCore::Frame*) override;
    void didEndEditing() override;
    void willWriteSelectionToPasteboard(WebCore::Range*) override;
    void didWriteSelectionToPasteboard() override;
    void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) override;
    
    void registerUndoStep(PassRefPtr<WebCore::UndoStep>) override;
    void registerRedoStep(PassRefPtr<WebCore::UndoStep>) override;
    void clearUndoRedoOperations() override;

    bool canCopyCut(WebCore::Frame*, bool defaultValue) const override;
    bool canPaste(WebCore::Frame*, bool defaultValue) const override;
    bool canUndo() const override;
    bool canRedo() const override;
    
    void undo() override;
    void redo() override;

    void handleKeyboardEvent(WebCore::KeyboardEvent*) override;
    void handleInputMethodKeydown(WebCore::KeyboardEvent*) override;
    
    void textFieldDidBeginEditing(WebCore::Element*) override;
    void textFieldDidEndEditing(WebCore::Element*) override;
    void textDidChangeInTextField(WebCore::Element*) override;
    bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) override;
    void textWillBeDeletedInTextField(WebCore::Element*) override;
    void textDidChangeInTextArea(WebCore::Element*) override;
    void overflowScrollPositionChanged() override;

#if PLATFORM(COCOA)
    NSString *userVisibleString(NSURL *) override;
    WebCore::DocumentFragment* documentFragmentFromAttributedString(NSAttributedString *, Vector< RefPtr<WebCore::ArchiveResource>>&) override;
    void setInsertionPasteboard(const String& pasteboardName) override;
    NSURL* canonicalizeURL(NSURL*) override;
    NSURL* canonicalizeURLString(NSString*) override;
#endif

#if USE(APPKIT)
    void uppercaseWord() override;
    void lowercaseWord() override;
    void capitalizeWord() override;
#endif
#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel(bool show) override;
    bool substitutionsPanelIsShowing() override;
    void toggleSmartInsertDelete() override;
    bool isAutomaticQuoteSubstitutionEnabled() override;
    void toggleAutomaticQuoteSubstitution() override;
    bool isAutomaticLinkDetectionEnabled() override;
    void toggleAutomaticLinkDetection() override;
    bool isAutomaticDashSubstitutionEnabled() override;
    void toggleAutomaticDashSubstitution() override;
    bool isAutomaticTextReplacementEnabled() override;
    void toggleAutomaticTextReplacement() override;
    bool isAutomaticSpellingCorrectionEnabled() override;
    void toggleAutomaticSpellingCorrection() override;
#endif

#if PLATFORM(GTK)
    bool executePendingEditorCommands(WebCore::Frame*, const Vector<WTF::String>&, bool);
    void getEditorCommandsForKeyEvent(const WebCore::KeyboardEvent*, Vector<WTF::String>&);
    void updateGlobalSelection(WebCore::Frame*);
#endif

    TextCheckerClient* textChecker()  override { return this; }

    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const override;
    void ignoreWordInSpellDocument(const String&) override;
    void learnWord(const String&) override;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) override;
    String getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord) override;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) override;
#if USE(UNIFIED_TEXT_CHECKING)
    Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, WebCore::TextCheckingTypeMask checkingTypes) override;
#endif
    void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) override;
    void updateSpellingUIWithMisspelledWord(const String&) override;
    void showSpellingUI(bool show) override;
    bool spellingUIIsShowing() override;
    void getGuessesForWord(const String& word, const String& context, Vector<String>& guesses) override;
    void willSetInputMethodState() override;
    void setInputMethodState(bool enabled) override;
    void requestCheckingOfString(WTF::PassRefPtr<WebCore::TextCheckingRequest>) override;

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() override;
#endif
#if PLATFORM(IOS)
    void startDelayingAndCoalescingContentChangeNotifications() override;
    void stopDelayingAndCoalescingContentChangeNotifications() override;
    void writeDataToPasteboard(NSDictionary*) override;
    NSArray *supportedPasteboardTypesForCurrentSelection() override;
    NSArray *readDataFromPasteboard(NSString* type, int index) override;
    bool hasRichlyEditableSelection() override;
    int getPasteboardItemsCount() override;
    WebCore::DocumentFragment* documentFragmentFromDelegate(int index) override;
    bool performsTwoStepPaste(WebCore::DocumentFragment*) override;
    int pasteboardChangeCount() override;
#endif

    bool supportsGlobalSelection() override;

    WebPage* m_page;
};

} // namespace WebKit

#endif // WebEditorClient_h
